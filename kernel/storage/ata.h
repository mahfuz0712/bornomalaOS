/* kernel/storage/ata.h - ATA PIO driver (IDE / legacy SATA mode), LBA28 + LBA48 */
#ifndef BORNOMALA_ATA_H
#define BORNOMALA_ATA_H
int ata_init(void);        /* probes both channels, registers "hd0".. block devices; returns the count */
#endif
