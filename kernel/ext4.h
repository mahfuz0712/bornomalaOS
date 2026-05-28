/* kernel/ext4.h  —  ext4 file system driver */

#ifndef EXT4_H
#define EXT4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ext4 Superblock (offset 1024, size 256 bytes minimum) */
typedef struct {
    uint32_t s_inodes_count;           /* 0x00 */
    uint32_t s_blocks_count_lo;        /* 0x04 */
    uint32_t s_r_blocks_count_lo;      /* 0x08 */
    uint32_t s_free_blocks_count_lo;   /* 0x0C */
    uint32_t s_free_inodes_count;      /* 0x10 */
    uint32_t s_first_data_block;       /* 0x14 */
    uint32_t s_log_block_size;         /* 0x18 */
    uint32_t s_log_frag_size;          /* 0x1C */
    uint32_t s_blocks_per_group;       /* 0x20 */
    uint32_t s_frags_per_group;        /* 0x24 */
    uint32_t s_inodes_per_group;       /* 0x28 */
    uint32_t s_mtime;                  /* 0x2C */
    uint32_t s_wtime;                  /* 0x30 */
    uint16_t s_mnt_count;              /* 0x34 */
    int16_t  s_max_mnt_count;          /* 0x36 */
    uint16_t s_magic;                  /* 0x38 = 0xEF53 */
    uint16_t s_state;                  /* 0x3A */
    uint16_t s_errors;                 /* 0x3C */
    uint16_t s_minor_rev_level;        /* 0x3E */
    uint32_t s_lastcheck;              /* 0x40 */
    uint32_t s_checkinterval;          /* 0x44 */
    uint32_t s_creator_os;             /* 0x48 */
    uint32_t s_rev_level;              /* 0x4C */
    uint16_t s_def_resuid;             /* 0x50 */
    uint16_t s_def_resgid;             /* 0x52 */
    uint32_t s_first_ino;              /* 0x54 */
    uint16_t s_inode_size;             /* 0x58 */
    uint16_t s_block_group_nr;         /* 0x5A */
    uint32_t s_feature_compat;         /* 0x5C */
    uint32_t s_feature_incompat;       /* 0x60 */
    uint32_t s_feature_ro_compat;      /* 0x64 */
    uint8_t  s_uuid[16];               /* 0x68 */
    char     s_volume_name[16];        /* 0x78 */
    char     s_last_mounted[64];       /* 0x88 */
    uint32_t s_algorithm_usage_bitmap; /* 0xC8 */
    uint8_t  s_prealloc_blocks;        /* 0xCC */
    uint8_t  s_prealloc_dir_blocks;    /* 0xCD */
    uint16_t s_reserved_gdt_blocks;    /* 0xCE */
    uint8_t  s_journal_uuid[16];       /* 0xD0 */
    uint32_t s_journal_inum;           /* 0xE0 */
    uint32_t s_journal_dev;            /* 0xE4 */
    uint32_t s_last_orphan;            /* 0xE8 */
    uint32_t s_hash_seed[4];           /* 0xEC */
    uint8_t  s_def_hash_version;       /* 0xFC */
    uint8_t  s_padding[3];             /* 0xFD */
    uint32_t s_default_mount_opts;     /* 0x100 */
    uint32_t s_first_meta_bg;          /* 0x104 */
    uint32_t s_mkfs_time;              /* 0x108 */
    uint32_t s_jnl_blocks[17];         /* 0x10C */
    uint32_t s_blocks_count_hi;        /* 0x150 */
    uint32_t s_r_blocks_count_hi;      /* 0x154 */
    uint32_t s_free_blocks_count_hi;   /* 0x158 */
    uint16_t s_min_extra_isize;        /* 0x15C */
    uint16_t s_want_extra_isize;       /* 0x15E */
    uint32_t s_flags;                  /* 0x160 */
    uint16_t s_raid_stride;            /* 0x164 */
    uint16_t s_raid_stripe_width;      /* 0x166 */
    uint32_t s_inode_size_high;        /* 0x168 */
} ext4_superblock_t;

/* ext4 Group Descriptor (can be 32 or 64 bytes depending on s_desc_size) */
typedef struct {
    uint32_t bg_block_bitmap_lo;       /* 0x00 */
    uint32_t bg_inode_bitmap_lo;       /* 0x04 */
    uint32_t bg_inode_table_lo;        /* 0x08 */
    uint16_t bg_free_blocks_count_lo;  /* 0x0C */
    uint16_t bg_free_inodes_count_lo;  /* 0x0E */
    uint16_t bg_used_dirs_count_lo;    /* 0x10 */
    uint16_t bg_flags;                 /* 0x12 */
    uint32_t bg_exclude_bitmap_lo;     /* 0x14 */
    uint16_t bg_block_bitmap_csum_lo;  /* 0x18 */
    uint16_t bg_inode_bitmap_csum_lo;  /* 0x1A */
    uint16_t bg_itable_unused_lo;      /* 0x1C */
    uint16_t bg_checksum;              /* 0x1E */
    uint32_t bg_block_bitmap_hi;       /* 0x20 */
    uint32_t bg_inode_bitmap_hi;       /* 0x24 */
    uint32_t bg_inode_table_hi;        /* 0x28 */
    uint16_t bg_free_blocks_count_hi;  /* 0x2C */
    uint16_t bg_free_inodes_count_hi;  /* 0x2E */
    uint16_t bg_used_dirs_count_hi;    /* 0x30 */
    uint16_t bg_itable_unused_hi;      /* 0x32 */
    uint32_t bg_exclude_bitmap_hi;     /* 0x34 */
    uint16_t bg_block_bitmap_csum_hi;  /* 0x38 */
    uint16_t bg_inode_bitmap_csum_hi;  /* 0x3A */
    uint32_t bg_reserved;              /* 0x3C */
} ext4_group_desc_t;

/* ext4 Inode (128+ bytes) */
typedef struct {
    uint16_t i_mode;                   /* 0x00 */
    uint16_t i_uid;                    /* 0x02 */
    uint32_t i_size_lo;                /* 0x04 */
    uint32_t i_atime;                  /* 0x08 */
    uint32_t i_ctime;                  /* 0x0C */
    uint32_t i_mtime;                  /* 0x10 */
    uint32_t i_dtime;                  /* 0x14 */
    uint16_t i_gid;                    /* 0x18 */
    uint16_t i_links_count;            /* 0x1A */
    uint32_t i_blocks_lo;              /* 0x1C */
    uint32_t i_flags;                  /* 0x20 */
    uint32_t i_osd1;                   /* 0x24 */
    uint32_t i_block[15];              /* 0x28 direct/indirect blocks */
    uint32_t i_generation;             /* 0x64 */
    uint32_t i_file_acl_lo;            /* 0x68 */
    uint32_t i_size_hi;                /* 0x6C */
    uint32_t i_obso_faddr;             /* 0x70 */
    uint32_t i_osd2_1;                 /* 0x74 */
    uint16_t i_osd2_2;                 /* 0x78 */
    uint16_t i_pad1;                   /* 0x7A */
    uint16_t i_uid_high;               /* 0x7C */
    uint16_t i_gid_high;               /* 0x7E */
    uint32_t i_author;                 /* 0x80 */
} ext4_inode_t;

/* ext4 Directory Entry */
typedef struct {
    uint32_t inode;                    /* Inode number */
    uint16_t rec_len;                  /* Entry length */
    uint8_t  name_len;                 /* Filename length */
    uint8_t  file_type;                /* File type (1=regular, 2=dir, etc) */
    char     name[0];                  /* Filename (variable) */
} ext4_dir_entry_2_t;

/* ext4 Extent Header */
typedef struct {
    uint16_t eh_magic;                 /* 0xF30A */
    uint16_t eh_entries;               /* Number of extents */
    uint16_t eh_max;                   /* Maximum entries */
    uint16_t eh_depth;                 /* Tree depth */
    uint32_t eh_generation;            /* Generation */
} ext4_extent_header_t;

/* ext4 Extent */
typedef struct {
    uint32_t ee_block;                 /* First block in extent */
    uint16_t ee_len;                   /* Number of blocks */
    uint16_t ee_start_hi;              /* High bits of start block */
    uint32_t ee_start_lo;              /* Low bits of start block */
} ext4_extent_t;

/* File mode bits */
#define EXT4_S_IFREG    0x8000
#define EXT4_S_IFDIR    0x4000
#define EXT4_S_IFLNK    0xA000
#define EXT4_S_ITYPE    0xF000

/* Directory entry types */
#define EXT4_FT_UNKNOWN  0
#define EXT4_FT_REG_FILE 1
#define EXT4_FT_DIR      2
#define EXT4_FT_CHRDEV   3
#define EXT4_FT_BLKDEV   4
#define EXT4_FT_FIFO     5
#define EXT4_FT_SOCK     6
#define EXT4_FT_SYMLINK  7

/* ext4 context */
typedef struct {
    ext4_superblock_t sb;
    ext4_group_desc_t *group_descs;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t num_block_groups;
    uint32_t current_inode;
} ext4_ctx_t;

extern ext4_ctx_t ext4_context;

bool ext4_init(void);
bool ext4_read_inode(uint32_t inode_num, ext4_inode_t *inode);
bool ext4_read_block(uint32_t block_num, uint8_t *buffer);
bool ext4_write_block(uint32_t block_num, uint8_t *buffer);
bool ext4_read_file(uint32_t inode_num, uint64_t offset, size_t size, uint8_t *buffer);
bool ext4_write_file(uint32_t inode_num, uint64_t offset, size_t size, uint8_t *buffer);
uint32_t ext4_find_inode(const char *path);
typedef bool (*ext4_dir_callback_t)(const char *name, uint32_t inode_num, uint8_t file_type, void *ctx);
bool ext4_list_dir(uint32_t inode_num, ext4_dir_callback_t callback, void *ctx);
uint32_t ext4_find_inode_in_dir(uint32_t parent_inode, const char *name);
bool ext4_create_file(uint32_t parent_inode, const char *name, uint16_t mode);
bool ext4_create_dir(uint32_t parent_inode, const char *name);
bool ext4_delete_file(uint32_t parent_inode, const char *name);
bool ext4_add_dir_entry(uint32_t parent_inode, const char *name, uint32_t inode_num, uint8_t file_type);
bool ext4_remove_dir_entry(uint32_t parent_inode, const char *name);

#endif
