/* kernel/ext4.c  —  ext4 file system driver implementation */

#include "ext4.h"
#include "ata.h"
#include "alloc.h"

#define MAX_PATH_LEN 256
#define MAX_FILENAME_LEN 255

ext4_ctx_t ext4_context = {0};

static inline uint32_t get_block_group(uint32_t inode_num) {
    return (inode_num - 1) / ext4_context.sb.s_inodes_per_group;
}

static inline uint32_t get_inode_offset(uint32_t inode_num) {
    return ((inode_num - 1) % ext4_context.sb.s_inodes_per_group) * ext4_context.inode_size;
}

static inline uint64_t get_block_address(uint32_t block_num) {
    return (uint64_t)block_num * ext4_context.block_size;
}

static inline uint32_t get_inode_block(uint32_t inode_num) {
    uint32_t group = get_block_group(inode_num);
    uint32_t offset = get_inode_offset(inode_num);
    return ext4_context.group_descs[group].bg_inode_table_lo + (offset / ext4_context.block_size);
}

bool ext4_init(void) {
    uint8_t sector_buf[512];

    /* Read superblock from sector 2 (byte 1024) */
    if (!ata_read_sector(2, 1, sector_buf)) return false;

    ext4_superblock_t *sb = (ext4_superblock_t *)sector_buf;
    if (sb->s_magic != 0xEF53) return false;

    ext4_context.sb = *sb;
    ext4_context.block_size = 1024 << sb->s_log_block_size;
    ext4_context.inode_size = sb->s_inode_size;

    if (ext4_context.block_size < 1024 || ext4_context.block_size > 4096) return false;
    if (ext4_context.inode_size < 128) return false;

    ext4_context.num_block_groups = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;

    /* Allocate space for group descriptors */
    uint32_t gd_size = ext4_context.num_block_groups * sizeof(ext4_group_desc_t);
    ext4_context.group_descs = (ext4_group_desc_t *)kmalloc(gd_size);
    if (!ext4_context.group_descs) return false;

    /* Read group descriptor table (located right after superblock, starting at block 1) */
    uint32_t gdt_blocks = (gd_size + ext4_context.block_size - 1) / ext4_context.block_size;
    uint8_t *gdt_buf = (uint8_t *)kmalloc(gdt_blocks * ext4_context.block_size);
    if (!gdt_buf) return false;

    for (uint32_t i = 0; i < gdt_blocks; i++) {
        if (!ext4_read_block(1 + i, gdt_buf + i * ext4_context.block_size)) {
            kfree(gdt_buf);
            return false;
        }
    }

    /* Copy group descriptors */
    for (uint32_t i = 0; i < ext4_context.num_block_groups; i++) {
        ext4_context.group_descs[i] = *(ext4_group_desc_t *)(gdt_buf + i * sizeof(ext4_group_desc_t));
    }
    kfree(gdt_buf);

    ext4_context.current_inode = 2; /* root inode */
    return true;
}

bool ext4_read_block(uint32_t block_num, uint8_t *buffer) {
    uint64_t byte_offset = (uint64_t)block_num * ext4_context.block_size;
    uint32_t sector_offset = byte_offset / 512;
    uint16_t num_sectors = (ext4_context.block_size + 511) / 512;

    return ata_read_sector(sector_offset, num_sectors, buffer);
}

bool ext4_write_block(uint32_t block_num, uint8_t *buffer) {
    uint64_t byte_offset = (uint64_t)block_num * ext4_context.block_size;
    uint32_t sector_offset = byte_offset / 512;
    uint16_t num_sectors = (ext4_context.block_size + 511) / 512;

    return ata_write_sector(sector_offset, num_sectors, buffer);
}

bool ext4_read_inode(uint32_t inode_num, ext4_inode_t *inode) {
    if (inode_num < 1 || inode_num > ext4_context.sb.s_inodes_count) return false;

    uint32_t group = get_block_group(inode_num);
    uint32_t offset = get_inode_offset(inode_num);
    uint32_t inode_table_block = ext4_context.group_descs[group].bg_inode_table_lo;
    uint32_t block_in_table = offset / ext4_context.block_size;
    uint32_t offset_in_block = offset % ext4_context.block_size;

    uint8_t *buf = (uint8_t *)kmalloc(ext4_context.block_size);
    if (!buf) return false;

    if (!ext4_read_block(inode_table_block + block_in_table, buf)) {
        kfree(buf);
        return false;
    }

    *inode = *(ext4_inode_t *)(buf + offset_in_block);
    kfree(buf);
    return true;
}

static uint32_t ext4_get_file_block(ext4_inode_t *inode, uint32_t logical_block) {
    if (logical_block < 12) {
        return inode->i_block[logical_block];
    }

    /* Indirect blocks not fully implemented yet - use direct blocks only */
    return 0;
}

bool ext4_read_file(uint32_t inode_num, uint64_t offset, size_t size, uint8_t *buffer) {
    ext4_inode_t inode;
    if (!ext4_read_inode(inode_num, &inode)) return false;

    uint64_t file_size = inode.i_size_lo | ((uint64_t)inode.i_size_hi << 32);
    if (offset >= file_size) return false;

    if (offset + size > file_size) size = file_size - offset;

    uint8_t *temp_buf = (uint8_t *)kmalloc(ext4_context.block_size);
    if (!temp_buf) return false;

    uint32_t start_block = offset / ext4_context.block_size;
    uint32_t start_offset = offset % ext4_context.block_size;
    size_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t phys_block = ext4_get_file_block(&inode, start_block);
        if (phys_block == 0) {
            kfree(temp_buf);
            return false;
        }

        if (!ext4_read_block(phys_block, temp_buf)) {
            kfree(temp_buf);
            return false;
        }

        size_t to_copy = ext4_context.block_size - start_offset;
        if (bytes_read + to_copy > size) to_copy = size - bytes_read;

        for (size_t i = 0; i < to_copy; i++) {
            buffer[bytes_read + i] = temp_buf[start_offset + i];
        }

        bytes_read += to_copy;
        start_block++;
        start_offset = 0;
    }

    kfree(temp_buf);
    return true;
}

bool ext4_write_file(uint32_t inode_num, uint64_t offset, size_t size, uint8_t *buffer) {
    ext4_inode_t inode;
    if (!ext4_read_inode(inode_num, &inode)) return false;

    uint64_t file_size = inode.i_size_lo | ((uint64_t)inode.i_size_hi << 32);
    if (offset > file_size) offset = file_size;

    uint8_t *temp_buf = (uint8_t *)kmalloc(ext4_context.block_size);
    if (!temp_buf) return false;

    uint32_t start_block = offset / ext4_context.block_size;
    uint32_t start_offset = offset % ext4_context.block_size;
    size_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t phys_block = ext4_get_file_block(&inode, start_block);
        if (phys_block == 0) {
            kfree(temp_buf);
            return false;
        }

        /* Read block if partial write */
        if (start_offset > 0 || (size - bytes_written) < ext4_context.block_size) {
            if (!ext4_read_block(phys_block, temp_buf)) {
                kfree(temp_buf);
                return false;
            }
        }

        size_t to_write = ext4_context.block_size - start_offset;
        if (bytes_written + to_write > size) to_write = size - bytes_written;

        for (size_t i = 0; i < to_write; i++) {
            temp_buf[start_offset + i] = buffer[bytes_written + i];
        }

        if (!ext4_write_block(phys_block, temp_buf)) {
            kfree(temp_buf);
            return false;
        }

        bytes_written += to_write;
        start_block++;
        start_offset = 0;
    }

    kfree(temp_buf);
    return true;
}

/* Helper callback for finding inode by name in directory */
typedef struct {
    const char *target_name;
    uint32_t found_inode;
} find_inode_ctx_t;

static bool find_inode_callback(const char *name, uint32_t inode_num, uint8_t file_type, void *ctx) {
    (void)file_type;
    find_inode_ctx_t *fctx = (find_inode_ctx_t *)ctx;
    int i = 0;
    while (name[i] && fctx->target_name[i]) {
        if (name[i] != fctx->target_name[i]) return true;
        i++;
    }
    if (name[i] == '\0' && fctx->target_name[i] == '\0') {
        fctx->found_inode = inode_num;
        return false; /* stop iteration */
    }
    return true; /* continue */
}

/* Bitmap helpers */
static int bitmap_find_free_bit(uint8_t *bitmap, uint32_t max_bits) {
    for (uint32_t i = 0; i < (max_bits + 7) / 8; i++) {
        if (bitmap[i] != 0xFF) {
            for (int j = 0; j < 8; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    return i * 8 + j;
                }
            }
        }
    }
    return -1;
}

static void bitmap_set_bit(uint8_t *bitmap, int bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static void bitmap_clear_bit(uint8_t *bitmap, int bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static bool bitmap_is_set(uint8_t *bitmap, int bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

uint32_t ext4_find_inode(const char *path) {
    if (!path || *path != '/') return 0;

    uint32_t current_inode = 2; /* root */
    const char *p = path + 1;

    while (*p) {
        char name[MAX_FILENAME_LEN + 1];
        int len = 0;

        while (*p && *p != '/' && len < MAX_FILENAME_LEN) {
            name[len++] = *p++;
        }
        name[len] = '\0';

        if (len == 0) {
            if (*p == '/') p++;
            continue;
        }

        find_inode_ctx_t fctx = {name, 0};
        ext4_list_dir(current_inode, find_inode_callback, &fctx);

        if (!fctx.found_inode) return 0;
        current_inode = fctx.found_inode;

        if (*p == '/') p++;
    }

    return current_inode;
}

bool ext4_list_dir(uint32_t inode_num, ext4_dir_callback_t callback, void *ctx) {
    ext4_inode_t inode;
    if (!ext4_read_inode(inode_num, &inode)) return false;

    if ((inode.i_mode & EXT4_S_ITYPE) != EXT4_S_IFDIR) return false;

    uint64_t dir_size = inode.i_size_lo | ((uint64_t)inode.i_size_hi << 32);
    uint8_t *dir_buf = (uint8_t *)kmalloc(dir_size);
    if (!dir_buf) return false;

    if (!ext4_read_file(inode_num, 0, dir_size, dir_buf)) {
        kfree(dir_buf);
        return false;
    }

    uint64_t offset = 0;
    while (offset < dir_size) {
        ext4_dir_entry_2_t *entry = (ext4_dir_entry_2_t *)(dir_buf + offset);

        if (entry->inode != 0 && entry->name_len > 0) {
            if (callback) {
                char name[MAX_FILENAME_LEN + 1];
                for (int i = 0; i < entry->name_len; i++) {
                    name[i] = entry->name[i];
                }
                name[entry->name_len] = '\0';
                callback(name, entry->inode, entry->file_type, ctx);
            }
        }

        offset += entry->rec_len;
        if (entry->rec_len == 0) break;
    }

    kfree(dir_buf);
    return true;
}

bool ext4_create_file(uint32_t parent_inode, const char *name, uint16_t mode) {
    /* Placeholder - full implementation requires free block/inode allocation */
    (void)parent_inode;
    (void)name;
    (void)mode;
    return false;
}

bool ext4_create_dir(uint32_t parent_inode, const char *name) {
    return ext4_create_file(parent_inode, name, EXT4_S_IFDIR | 0755);
}

bool ext4_delete_file(uint32_t parent_inode, const char *name) {
    /* Placeholder - full implementation requires block deallocation */
    (void)parent_inode;
    (void)name;
    return false;
}

bool ext4_add_dir_entry(uint32_t parent_inode, const char *name, uint32_t inode_num, uint8_t file_type) {
    (void)parent_inode;
    (void)name;
    (void)inode_num;
    (void)file_type;
    return false;
}

bool ext4_remove_dir_entry(uint32_t parent_inode, const char *name) {
    (void)parent_inode;
    (void)name;
    return false;
}

uint32_t ext4_find_inode_in_dir(uint32_t parent_inode, const char *name) {
    find_inode_ctx_t fctx = {name, 0};
    ext4_list_dir(parent_inode, find_inode_callback, &fctx);
    return fctx.found_inode;
}
