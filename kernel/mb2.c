<<<<<<< HEAD
#include "mb2.h"
#include "klib.h"

typedef struct { uint32_t type, size; } __attribute__((packed)) mb2_tag_t;

typedef struct {
    uint32_t type, size;
    uint32_t entry_size, entry_version;
} __attribute__((packed)) mb2_mmap_tag_t;

typedef struct {
    uint64_t addr, len;
    uint32_t type, zero;
} __attribute__((packed)) mb2_mmap_raw_t;

typedef struct {
    uint32_t type, size;
    uint64_t addr;
    uint32_t pitch, width, height;
    uint8_t  bpp, fb_type;
    uint16_t reserved;
    uint8_t  red_pos, red_size, green_pos, green_size, blue_pos, blue_size;
} __attribute__((packed)) mb2_fb_tag_t;

bool mb2_parse(uint64_t info_addr, uint32_t magic, boot_info_t *out) {
    memset(out, 0, sizeof(*out));
    if (magic != MB2_BOOTLOADER_MAGIC || info_addr == 0 || (info_addr & 7)) return false;

    const uint8_t *base = (const uint8_t *)(uintptr_t)info_addr;
    uint32_t total = *(const uint32_t *)base;
    if (total < 16 || total > 0x100000) return false;   /* sanity: 16 B .. 1 MiB */

    out->info_start = info_addr;
    out->info_end   = info_addr + total;

    const uint8_t *p   = base + 8;
    const uint8_t *end = base + total;
    while (p + sizeof(mb2_tag_t) <= end) {
        const mb2_tag_t *tag = (const mb2_tag_t *)p;
        if (tag->type == 0) break;                     /* end tag */
        if (tag->size < 8 || p + tag->size > end) break;

        if (tag->type == 6 && tag->size >= sizeof(mb2_mmap_tag_t)) {
            const mb2_mmap_tag_t *mt = (const mb2_mmap_tag_t *)p;
            if (mt->entry_size >= sizeof(mb2_mmap_raw_t)) {
                const uint8_t *q = p + sizeof(mb2_mmap_tag_t);
                while (q + mt->entry_size <= p + mt->size && out->mmap_count < MB2_MMAP_MAX) {
                    const mb2_mmap_raw_t *e = (const mb2_mmap_raw_t *)q;
                    out->mmap[out->mmap_count].addr = e->addr;
                    out->mmap[out->mmap_count].len  = e->len;
                    out->mmap[out->mmap_count].type = e->type;
                    out->mmap_count++;
                    q += mt->entry_size;
                }
            }
        } else if (tag->type == 1 && tag->size > 8) {
            uint32_t n = tag->size - 8;
            if (n >= sizeof(out->cmdline)) n = sizeof(out->cmdline) - 1;
            memcpy(out->cmdline, p + 8, n);
            out->cmdline[n] = '\0';
        } else if (tag->type == 8 && tag->size >= sizeof(mb2_fb_tag_t)) {
            const mb2_fb_tag_t *f = (const mb2_fb_tag_t *)p;
            mb2_framebuffer_t *fb = &out->fb;
            fb->address = f->addr;
            fb->pitch   = f->pitch;
            fb->width   = f->width;
            fb->height  = f->height;
            fb->bpp     = f->bpp;
            fb->type    = f->fb_type;
            fb->red_pos = f->red_pos;     fb->red_size = f->red_size;
            fb->green_pos = f->green_pos; fb->green_size = f->green_size;
            fb->blue_pos = f->blue_pos;   fb->blue_size = f->blue_size;
            /* We need direct RGB, 32 bits per pixel, below 4 GiB (our identity map),
               and a pitch that is a whole number of pixels. */
            fb->available = (f->fb_type == 1 && f->bpp == 32 &&
                             f->width >= 640 && f->height >= 480 &&
                             (f->pitch & 3) == 0 && f->pitch >= f->width * 4 &&
                             f->addr + (uint64_t)f->pitch * f->height <= 0x100000000ULL);
        }
        p += (tag->size + 7u) & ~7u;
    }
    out->valid = true;
    return true;
}
=======
#include "mb2.h"
#include "klib.h"

typedef struct { uint32_t type, size; } __attribute__((packed)) mb2_tag_t;

typedef struct {
    uint32_t type, size;
    uint32_t entry_size, entry_version;
} __attribute__((packed)) mb2_mmap_tag_t;

typedef struct {
    uint64_t addr, len;
    uint32_t type, zero;
} __attribute__((packed)) mb2_mmap_raw_t;

typedef struct {
    uint32_t type, size;
    uint64_t addr;
    uint32_t pitch, width, height;
    uint8_t  bpp, fb_type;
    uint16_t reserved;
    uint8_t  red_pos, red_size, green_pos, green_size, blue_pos, blue_size;
} __attribute__((packed)) mb2_fb_tag_t;

bool mb2_parse(uint64_t info_addr, uint32_t magic, boot_info_t *out) {
    memset(out, 0, sizeof(*out));
    if (magic != MB2_BOOTLOADER_MAGIC || info_addr == 0 || (info_addr & 7)) return false;

    const uint8_t *base = (const uint8_t *)(uintptr_t)info_addr;
    uint32_t total = *(const uint32_t *)base;
    if (total < 16 || total > 0x100000) return false;   /* sanity: 16 B .. 1 MiB */

    out->info_start = info_addr;
    out->info_end   = info_addr + total;

    const uint8_t *p   = base + 8;
    const uint8_t *end = base + total;
    while (p + sizeof(mb2_tag_t) <= end) {
        const mb2_tag_t *tag = (const mb2_tag_t *)p;
        if (tag->type == 0) break;                     /* end tag */
        if (tag->size < 8 || p + tag->size > end) break;

        if (tag->type == 6 && tag->size >= sizeof(mb2_mmap_tag_t)) {
            const mb2_mmap_tag_t *mt = (const mb2_mmap_tag_t *)p;
            if (mt->entry_size >= sizeof(mb2_mmap_raw_t)) {
                const uint8_t *q = p + sizeof(mb2_mmap_tag_t);
                while (q + mt->entry_size <= p + mt->size && out->mmap_count < MB2_MMAP_MAX) {
                    const mb2_mmap_raw_t *e = (const mb2_mmap_raw_t *)q;
                    out->mmap[out->mmap_count].addr = e->addr;
                    out->mmap[out->mmap_count].len  = e->len;
                    out->mmap[out->mmap_count].type = e->type;
                    out->mmap_count++;
                    q += mt->entry_size;
                }
            }
        } else if (tag->type == 8 && tag->size >= sizeof(mb2_fb_tag_t)) {
            const mb2_fb_tag_t *f = (const mb2_fb_tag_t *)p;
            mb2_framebuffer_t *fb = &out->fb;
            fb->address = f->addr;
            fb->pitch   = f->pitch;
            fb->width   = f->width;
            fb->height  = f->height;
            fb->bpp     = f->bpp;
            fb->type    = f->fb_type;
            fb->red_pos = f->red_pos;     fb->red_size = f->red_size;
            fb->green_pos = f->green_pos; fb->green_size = f->green_size;
            fb->blue_pos = f->blue_pos;   fb->blue_size = f->blue_size;
            /* We need direct RGB, 32 bits per pixel, below 4 GiB (our identity map),
               and a pitch that is a whole number of pixels. */
            fb->available = (f->fb_type == 1 && f->bpp == 32 &&
                             f->width >= 640 && f->height >= 480 &&
                             (f->pitch & 3) == 0 && f->pitch >= f->width * 4 &&
                             f->addr + (uint64_t)f->pitch * f->height <= 0x100000000ULL);
        }
        p += (tag->size + 7u) & ~7u;
    }
    out->valid = true;
    return true;
}
>>>>>>> 23b11cf3087acc2108f276bdfb25d3a6f909e2f7
