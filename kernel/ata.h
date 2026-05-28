/* kernel/ata.h  —  ATA PIO mode disk driver for IDE */

#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ATA I/O Ports (Primary Channel, Master) */
#define ATA_DATA_PORT       0x1F0
#define ATA_SECTOR_COUNT    0x1F2
#define ATA_LBA_LOW         0x1F3
#define ATA_LBA_MID         0x1F4
#define ATA_LBA_HIGH        0x1F5
#define ATA_DEVICE_HEAD     0x1F6
#define ATA_STATUS_CMD      0x1F7

/* ATA Commands */
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30

/* Status register bits */
#define ATA_SR_ERR          0x01  /* Error */
#define ATA_SR_DRQ          0x08  /* Data Request */
#define ATA_SR_BSY          0x80  /* Busy */

/* Device/Head register bits */
#define ATA_DEV_LBA         0x40  /* LBA mode */
#define ATA_DEV_MASTER      0x00  /* Master drive */
#define ATA_DEV_SLAVE       0x10  /* Slave drive */

void ata_init(void);
bool ata_read_sector(uint32_t lba, uint16_t count, uint8_t *buffer);
bool ata_write_sector(uint32_t lba, uint16_t count, uint8_t *buffer);

#endif
