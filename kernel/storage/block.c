#include "block.h"
#include "../klib.h"

static block_dev_t devs[BLOCK_MAX_DEVICES];
static int         ndevs;

block_dev_t *block_register(const block_dev_t *proto) {
    if (ndevs >= BLOCK_MAX_DEVICES) return 0;
    devs[ndevs] = *proto;
    return &devs[ndevs++];
}

int block_count(void) { return ndevs; }
block_dev_t *block_get(int i) { return (i >= 0 && i < ndevs) ? &devs[i] : 0; }

block_dev_t *block_find(const char *name) {
    for (int i = 0; i < ndevs; i++) if (k_strcmp(devs[i].name, name) == 0) return &devs[i];
    return 0;
}

uint32_t block_get_sector_size(const block_dev_t *d)  { return d->sector_size; }
uint64_t block_get_sector_count(const block_dev_t *d) { return d->sector_count; }

int block_read(block_dev_t *d, uint64_t lba, uint32_t count, void *buf) {
    if (!d || !d->read || count == 0) return BLOCK_ERR_RANGE;
    if (lba >= d->sector_count || count > d->sector_count - lba) return BLOCK_ERR_RANGE;
    return d->read(d, lba, count, buf);
}

int block_write(block_dev_t *d, uint64_t lba, uint32_t count, const void *buf) {
    if (!d || !d->write || count == 0) return BLOCK_ERR_RANGE;
    if (d->read_only) return BLOCK_ERR_RO;
    if (lba >= d->sector_count || count > d->sector_count - lba) return BLOCK_ERR_RANGE;
    return d->write(d, lba, count, buf);
}

int block_flush(block_dev_t *d) { return (d && d->flush) ? d->flush(d) : 0; }

/* ── partitions: forward to the parent with an offset ── */
static int part_read(block_dev_t *d, uint64_t lba, uint32_t n, void *buf) {
    return block_read(d->parent, d->parent_start + lba, n, buf);
}
static int part_write(block_dev_t *d, uint64_t lba, uint32_t n, const void *buf) {
    return block_write(d->parent, d->parent_start + lba, n, buf);
}
static int part_flush(block_dev_t *d) { return block_flush(d->parent); }

block_dev_t *block_create_partition(block_dev_t *parent, const char *name, uint64_t first, uint64_t sectors) {
    if (!parent || first >= parent->sector_count || sectors == 0 || sectors > parent->sector_count - first) return 0;
    block_dev_t p;
    memset(&p, 0, sizeof p);
    k_strncpy(p.name, name, sizeof p.name - 1);
    p.sector_size = parent->sector_size;
    p.sector_count = sectors;
    p.read_only = parent->read_only;
    p.is_partition = true;
    p.parent = parent;
    p.parent_start = first;
    p.read = part_read; p.write = part_write; p.flush = part_flush;
    return block_register(&p);
}

void block_remove_partitions(block_dev_t *parent) {
    int w = 0;
    for (int i = 0; i < ndevs; i++) {
        if (devs[i].is_partition && devs[i].parent == parent) continue;
        if (w != i) devs[w] = devs[i];
        w++;
    }
    ndevs = w;
}
