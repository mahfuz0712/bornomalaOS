/* kernel/storage/gpt.h - GUID Partition Table: parse, validate, edit, write */
#ifndef BORNOMALA_GPT_H
#define BORNOMALA_GPT_H

#include <stdint.h>
#include <stdbool.h>
#include "block.h"

#define GPT_MAX_ENTRIES 128

typedef struct { uint8_t b[16]; } guid_t;

typedef struct {
    bool     used;
    guid_t   type, unique;
    uint64_t first_lba, last_lba, attrs;
    char     name[37];             /* UTF-16 -> ASCII ('?' for non-ASCII) */
} gpt_entry_t;

typedef struct {
    block_dev_t *dev;
    bool         valid;
    bool         used_backup;      /* the primary header was bad; this came from the backup copy */
    guid_t       disk_guid;
    uint64_t     first_usable, last_usable, alt_lba;
    uint32_t     num_entries;
    gpt_entry_t  entries[GPT_MAX_ENTRIES];
    char         error[80];
} gpt_disk_t;

extern const guid_t GUID_EFI_SYSTEM, GUID_BASIC_DATA, GUID_BORNOMALA_SYSTEM;

int         gpt_read(block_dev_t *dev, gpt_disk_t *out);           /* 0 = ok, else out->error explains */
int         gpt_write(gpt_disk_t *g);                               /* primary + backup, CRCs recomputed */
int         gpt_init_blank(block_dev_t *dev, gpt_disk_t *out);      /* protective MBR + empty table (destroys!) */
int         gpt_create_partition(gpt_disk_t *g, guid_t type, uint64_t first, uint64_t last, const char *name);
int         gpt_delete_partition(gpt_disk_t *g, int index);
int         gpt_rename_partition(gpt_disk_t *g, int index, const char *name);
bool        gpt_find_free(const gpt_disk_t *g, uint64_t *first, uint64_t *last);   /* largest free extent */
int         gpt_register_partitions(gpt_disk_t *g);                 /* creates hdNpM block devices */
bool        guid_equal(guid_t a, guid_t b);
const char *gpt_type_name(guid_t t);
void        guid_to_string(guid_t g, char *out);                    /* >= 37 bytes */

#endif
