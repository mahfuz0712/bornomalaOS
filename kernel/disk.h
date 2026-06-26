/* kernel/disk.h — unified disk abstraction (NVMe + SATA/ATA) */
#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    DISK_TYPE_NONE = 0,
    DISK_TYPE_NVME,
    DISK_TYPE_ATA,      /* covers SATA/HDD/SSD via ATA PIO */
} disk_type_t;

typedef struct {
    disk_type_t type;
    uint64_t    num_sectors;    /* total 512-byte sectors */
    uint32_t    sector_size;    /* always 512 for ATA; NVMe native size */
    bool        present;
    char        model[41];      /* human-readable model string */
} disk_t;

/* Initialise: probes NVMe first, then ATA primary master.
   Returns true if any disk found.                          */
bool disk_init(void);

/* Read/write arbitrary LBA ranges (512-byte sectors)       */
bool disk_read (uint64_t lba, uint32_t count, void *buf);
bool disk_write(uint64_t lba, uint32_t count, const void *buf);

/* The single active disk — read by ext4 / installer        */
extern disk_t active_disk;

#endif