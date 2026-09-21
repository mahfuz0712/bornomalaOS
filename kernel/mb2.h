/* kernel/mb2.h - Multiboot2 boot-information parser */
#ifndef BORNOMALA_MB2_H
#define BORNOMALA_MB2_H

#include <stdint.h>
#include <stdbool.h>

#define MB2_BOOTLOADER_MAGIC 0x36D76289u

#define MB2_MMAP_MAX 64

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;      /* 1 = usable RAM, everything else reserved */
} mb2_mmap_entry_t;

typedef struct {
    bool     available;     /* linear 32-bpp direct-colour framebuffer that we can use */
    uint64_t address;
    uint32_t pitch;         /* BYTES per scanline */
    uint32_t width, height;
    uint8_t  bpp;
    uint8_t  type;          /* 1 = direct RGB */
    uint8_t  red_pos, red_size;
    uint8_t  green_pos, green_size;
    uint8_t  blue_pos, blue_size;
} mb2_framebuffer_t;

typedef struct {
    bool               valid;
    uint64_t           info_start, info_end;   /* the boot-info block itself (must be preserved) */
    uint32_t           mmap_count;
    mb2_mmap_entry_t   mmap[MB2_MMAP_MAX];
    mb2_framebuffer_t  fb;
} boot_info_t;

/* Parses and COPIES what the kernel needs, so later code never has to touch
   the bootloader-owned block again. Returns false when the magic/structure is bad. */
bool mb2_parse(uint64_t info_addr, uint32_t magic, boot_info_t *out);

#endif
