#include "compositor.h"
#include "../memory.h"
#include "../klib.h"
#include "../console.h"
#include "../panic.h"

static mb2_framebuffer_t hw;                 /* real framebuffer description */
static uint32_t *hw_px;
static int hw_stride;                        /* pixels */
static bool fast_path;                       /* framebuffer is plain 0x00RRGGBB */

static gfx_t back, bg;
static rect_t damage;                        /* single bounding rectangle */
static rect_t frame_rect;                    /* what begin_frame() is working on */

static uint32_t *alloc_layer(int w, int h) {
    size_t bytes = (size_t)w * (size_t)h * 4u;
    return (uint32_t *)pmm_alloc_contiguous((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

bool compositor_init(const mb2_framebuffer_t *fb) {
    if (!fb || !fb->available) return false;
    hw = *fb;
    hw_px = (uint32_t *)(uintptr_t)fb->address;
    hw_stride = (int)(fb->pitch / 4);
    fast_path = (fb->red_pos == 16 && fb->red_size == 8 &&
                 fb->green_pos == 8 && fb->green_size == 8 &&
                 fb->blue_pos == 0 && fb->blue_size == 8);
    panic_register_framebuffer(fb);

    int w = (int)fb->width, h = (int)fb->height;
    uint32_t *a = alloc_layer(w, h);
    uint32_t *b = alloc_layer(w, h);
    if (!a || !b) {
        if (a) pmm_free_contiguous(a, ((size_t)w * h * 4 + PAGE_SIZE - 1) / PAGE_SIZE);
        if (b) pmm_free_contiguous(b, ((size_t)w * h * 4 + PAGE_SIZE - 1) / PAGE_SIZE);
        kprintf("compositor: not enough memory for %dx%d layers\n", w, h);
        return false;
    }
    gfx_init(&back, a, w, h, w);
    gfx_init(&bg, b, w, h, w);
    damage = R(0, 0, 0, 0);
    compositor_damage_all();
    return true;
}

int compositor_width(void)  { return back.w; }
int compositor_height(void) { return back.h; }
gfx_t *compositor_background(void) { return &bg; }
gfx_t *compositor_canvas(void)     { return &back; }

void compositor_add_damage(rect_t r) {
    rect_t clipped;
    if (!rect_intersect(r, R(0, 0, back.w, back.h), &clipped)) return;
    damage = rect_union(damage, clipped);
}

void compositor_damage_all(void) { damage = R(0, 0, back.w, back.h); }
bool compositor_has_damage(void) { return !rect_empty(damage); }

gfx_t *compositor_begin_frame(void) {
    frame_rect = damage;
    damage = R(0, 0, 0, 0);
    gfx_set_clip(&back, R(0, 0, back.w, back.h));
    gfx_copy_rect(&back, &bg, frame_rect);          /* fresh background under the damaged area */
    gfx_set_clip(&back, frame_rect);
    return &back;
}

static inline uint32_t pack(uint32_t p) {
    uint32_t r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
    if (hw.red_size < 8)   r >>= (8 - hw.red_size);
    if (hw.green_size < 8) g >>= (8 - hw.green_size);
    if (hw.blue_size < 8)  b >>= (8 - hw.blue_size);
    return (r << hw.red_pos) | (g << hw.green_pos) | (b << hw.blue_pos);
}

void compositor_end_frame(void) {
    rect_t r = frame_rect;
    if (rect_empty(r)) return;
    for (int y = r.y; y < r.y + r.h; y++) {
        const uint32_t *src = back.px + y * back.stride + r.x;
        uint32_t *dst = hw_px + y * hw_stride + r.x;
        if (fast_path) {
            mem_copy32(dst, src, (size_t)r.w);
        } else {
            for (int x = 0; x < r.w; x++) dst[x] = pack(src[x]);
        }
    }
    gfx_set_clip(&back, R(0, 0, back.w, back.h));
}

void compositor_blank(void) {
    for (int y = 0; y < back.h; y++) mem_fill32(hw_px + y * hw_stride, 0, (size_t)back.w);
}
