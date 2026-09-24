#include "ata.h"
#include "block.h"
#include "../cpu.h"
#include "../console.h"
#include "../klib.h"

#define ST_ERR 0x01
#define ST_DRQ 0x08
#define ST_DF  0x20
#define ST_BSY 0x80

typedef struct { uint16_t io, ctl; bool slave; } ata_drive_t;
static ata_drive_t drives[4];
static int         drive_count;

static uint8_t status(const ata_drive_t *d) { return inb((uint16_t)(d->io + 7)); }

static void delay400(const ata_drive_t *d) { for (int i = 0; i < 4; i++) (void)inb(d->ctl); }

static bool wait_not_busy(const ata_drive_t *d) {
    for (int i = 0; i < 4000000; i++) if (!(status(d) & ST_BSY)) return true;
    return false;
}

static int wait_drq(const ata_drive_t *d) {
    delay400(d);
    for (int i = 0; i < 4000000; i++) {
        uint8_t s = status(d);
        if (s & (ST_ERR | ST_DF)) return BLOCK_ERR_IO;
        if (!(s & ST_BSY) && (s & ST_DRQ)) return 0;
    }
    return BLOCK_ERR_IO;
}

static void select_drive(const ata_drive_t *d, uint64_t lba, bool lba48) {
    outb((uint16_t)(d->io + 6), (uint8_t)((lba48 ? 0x40 : (0xE0 | ((lba >> 24) & 0x0F))) | (d->slave ? 0x10 : 0)));
    if (!lba48) return;
}

static int rw(block_dev_t *bd, uint64_t lba, uint32_t count, void *buf, bool write) {
    const ata_drive_t *d = (const ata_drive_t *)bd->ctx;
    uint8_t *p = (uint8_t *)buf;
    while (count) {
        uint32_t n = count > 128 ? 128 : count;              /* <= 128 sectors per command */
        bool lba48 = (lba + n) > 0x0FFFFFFFULL;
        if (!wait_not_busy(d)) return BLOCK_ERR_IO;
        select_drive(d, lba, lba48);
        if (lba48) {
            outb((uint16_t)(d->io + 2), (uint8_t)(n >> 8));
            outb((uint16_t)(d->io + 3), (uint8_t)(lba >> 24));
            outb((uint16_t)(d->io + 4), (uint8_t)(lba >> 32));
            outb((uint16_t)(d->io + 5), (uint8_t)(lba >> 40));
        }
        outb((uint16_t)(d->io + 2), (uint8_t)n);
        outb((uint16_t)(d->io + 3), (uint8_t)lba);
        outb((uint16_t)(d->io + 4), (uint8_t)(lba >> 8));
        outb((uint16_t)(d->io + 5), (uint8_t)(lba >> 16));
        outb((uint16_t)(d->io + 7), write ? (lba48 ? 0x34 : 0x30) : (lba48 ? 0x24 : 0x20));

        for (uint32_t s = 0; s < n; s++) {
            int r = wait_drq(d);
            if (r) return r;
            if (write) outsw(d->io, p, 256); else insw(d->io, p, 256);
            p += 512;
        }
        if (write && !wait_not_busy(d)) return BLOCK_ERR_IO;
        lba += n; count -= n;
    }
    return 0;
}

static int ata_read(block_dev_t *d, uint64_t lba, uint32_t n, void *buf) { return rw(d, lba, n, buf, false); }
static int ata_write(block_dev_t *d, uint64_t lba, uint32_t n, const void *buf) { return rw(d, lba, n, (void *)buf, true); }
static int ata_flush(block_dev_t *bd) {
    const ata_drive_t *d = (const ata_drive_t *)bd->ctx;
    if (!wait_not_busy(d)) return BLOCK_ERR_IO;
    outb((uint16_t)(d->io + 6), (uint8_t)(0xE0 | (d->slave ? 0x10 : 0)));
    outb((uint16_t)(d->io + 7), 0xE7);                        /* FLUSH CACHE */
    return wait_not_busy(d) ? 0 : BLOCK_ERR_IO;
}

static bool identify(ata_drive_t *d, uint64_t *sectors, char *model) {
    outb((uint16_t)(d->io + 6), (uint8_t)(0xA0 | (d->slave ? 0x10 : 0)));
    delay400(d);
    outb((uint16_t)(d->io + 2), 0); outb((uint16_t)(d->io + 3), 0);
    outb((uint16_t)(d->io + 4), 0); outb((uint16_t)(d->io + 5), 0);
    outb((uint16_t)(d->io + 7), 0xEC);                         /* IDENTIFY DEVICE */
    if (status(d) == 0 || status(d) == 0xFF) return false;     /* nothing there */
    if (!wait_not_busy(d)) return false;
    if (inb((uint16_t)(d->io + 4)) || inb((uint16_t)(d->io + 5))) return false;   /* ATAPI / SATA signature: not a disk */
    for (int i = 0; i < 100000; i++) {
        uint8_t s = status(d);
        if (s & ST_ERR) return false;
        if (s & ST_DRQ) break;
    }
    uint16_t id[256];
    insw(d->io, id, 256);
    uint64_t n = (uint64_t)id[60] | ((uint64_t)id[61] << 16);
    if (id[83] & (1u << 10))
        n = (uint64_t)id[100] | ((uint64_t)id[101] << 16) | ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
    if (n == 0) return false;
    *sectors = n;
    for (int i = 0; i < 20; i++) { model[i * 2] = (char)(id[27 + i] >> 8); model[i * 2 + 1] = (char)(id[27 + i] & 0xFF); }
    model[40] = '\0';
    for (int i = 39; i >= 0 && (model[i] == ' ' || model[i] == 0); i--) model[i] = '\0';
    return true;
}

int ata_init(void) {
    static const struct { uint16_t io, ctl; } ch[2] = { { 0x1F0, 0x3F6 }, { 0x170, 0x376 } };
    int found = 0;
    for (int c = 0; c < 2; c++) {
        outb(ch[c].ctl, 0x02);                                  /* nIEN: we poll, never use IRQ14/15 */
        for (int s = 0; s < 2; s++) {
            ata_drive_t *d = &drives[drive_count];
            d->io = ch[c].io; d->ctl = ch[c].ctl; d->slave = (s == 1);
            uint64_t sectors; char model[48];
            if (!identify(d, &sectors, model)) continue;
            block_dev_t bd;
            memset(&bd, 0, sizeof bd);
            ksnprintf(bd.name, sizeof bd.name, "hd%d", found);
            bd.sector_size = 512; bd.sector_count = sectors; bd.ctx = d;
            bd.read = ata_read; bd.write = ata_write; bd.flush = ata_flush;
            if (block_register(&bd)) {
                kprintf("ata: %s  %s  %llu MiB\n", bd.name, model, (unsigned long long)(sectors / 2048));
                drive_count++; found++;
            }
        }
    }
    return found;
}
