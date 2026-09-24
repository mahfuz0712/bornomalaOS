/* kernel/storage/block.h - generic block device layer.
   Every storage driver (ATA now; NVMe / USB mass storage later) registers a block_dev_t.
   Partitions are block devices too (a window onto a parent device). */
#ifndef BORNOMALA_BLOCK_H
#define BORNOMALA_BLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BLOCK_MAX_DEVICES 16
#define BLOCK_ERR_IO      (-5)
#define BLOCK_ERR_RANGE   (-22)
#define BLOCK_ERR_RO      (-30)

typedef struct block_dev block_dev_t;
struct block_dev {
    char      name[16];              /* "hd0", "hd0p1" */
    uint32_t  sector_size;
    uint64_t  sector_count;
    bool      read_only;
    bool      is_partition;
    block_dev_t *parent;             /* for partitions */
    uint64_t  parent_start;          /* first LBA inside the parent */
    void     *ctx;
    int (*read)(block_dev_t *d, uint64_t lba, uint32_t count, void *buf);
    int (*write)(block_dev_t *d, uint64_t lba, uint32_t count, const void *buf);
    int (*flush)(block_dev_t *d);
};

block_dev_t *block_register(const block_dev_t *proto);            /* copies the descriptor */
block_dev_t *block_create_partition(block_dev_t *parent, const char *name, uint64_t first_lba, uint64_t sectors);
void         block_remove_partitions(block_dev_t *parent);
int          block_count(void);
block_dev_t *block_get(int index);
block_dev_t *block_find(const char *name);

/* Bounds-checked front end used by everything above the drivers. Return 0 or a negative BLOCK_ERR_*. */
int      block_read(block_dev_t *d, uint64_t lba, uint32_t count, void *buf);
int      block_write(block_dev_t *d, uint64_t lba, uint32_t count, const void *buf);
int      block_flush(block_dev_t *d);
uint32_t block_get_sector_size(const block_dev_t *d);
uint64_t block_get_sector_count(const block_dev_t *d);

#endif
