/* kernel/ata.c  —  ATA PIO mode disk driver implementation */

#include "ata.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outw(uint16_t port, uint16_t v) {
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}

/* Poll ATA status until not busy, return true if ready, false on timeout/error */
static bool ata_poll(void) {
    uint32_t timeout = 30000;
    while (timeout--) {
        uint8_t status = inb(ATA_STATUS_CMD);
        if (!(status & ATA_SR_BSY)) return true;
    }
    return false;
}

/* Poll until data ready or error */
static bool ata_wait_data(void) {
    uint32_t timeout = 30000;
    while (timeout--) {
        uint8_t status = inb(ATA_STATUS_CMD);
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DRQ) return true;
    }
    return false;
}

void ata_init(void) {
    /* Soft reset: write 0x04 to control port, then 0x00 */
    outb(0x3F6, 0x04);
    for (volatile int i = 0; i < 1000; i++);
    outb(0x3F6, 0x00);

    ata_poll();
}

bool ata_read_sector(uint32_t lba, uint16_t count, uint8_t *buffer) {
    if (count == 0 || count > 255) return false;

    if (!ata_poll()) return false;

    /* Set sector count */
    outb(ATA_SECTOR_COUNT, count & 0xFF);

    /* Set LBA */
    outb(ATA_LBA_LOW,  (lba & 0xFF));
    outb(ATA_LBA_MID,  ((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, ((lba >> 16) & 0xFF));

    /* Set device (LBA mode, master) */
    outb(ATA_DEVICE_HEAD, ATA_DEV_LBA | ATA_DEV_MASTER | ((lba >> 24) & 0x0F));

    /* Issue read command */
    outb(ATA_STATUS_CMD, ATA_CMD_READ_PIO);

    /* Read sectors */
    for (uint16_t i = 0; i < count; i++) {
        if (!ata_wait_data()) return false;

        /* Read 256 words (512 bytes) per sector */
        for (int j = 0; j < 256; j++) {
            uint16_t word = inw(ATA_DATA_PORT);
            buffer[i * 512 + j * 2 + 0] = word & 0xFF;
            buffer[i * 512 + j * 2 + 1] = (word >> 8) & 0xFF;
        }

        if (!ata_poll()) return false;
    }

    return true;
}

bool ata_write_sector(uint32_t lba, uint16_t count, uint8_t *buffer) {
    if (count == 0 || count > 255) return false;

    if (!ata_poll()) return false;

    /* Set sector count */
    outb(ATA_SECTOR_COUNT, count & 0xFF);

    /* Set LBA */
    outb(ATA_LBA_LOW,  (lba & 0xFF));
    outb(ATA_LBA_MID,  ((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, ((lba >> 16) & 0xFF));

    /* Set device (LBA mode, master) */
    outb(ATA_DEVICE_HEAD, ATA_DEV_LBA | ATA_DEV_MASTER | ((lba >> 24) & 0x0F));

    /* Issue write command */
    outb(ATA_STATUS_CMD, ATA_CMD_WRITE_PIO);

    /* Write sectors */
    for (uint16_t i = 0; i < count; i++) {
        if (!ata_wait_data()) return false;

        /* Write 256 words (512 bytes) per sector */
        for (int j = 0; j < 256; j++) {
            uint16_t word = buffer[i * 512 + j * 2] | (buffer[i * 512 + j * 2 + 1] << 8);
            outw(ATA_DATA_PORT, word);
        }

        if (!ata_poll()) return false;
    }

    return true;
}
