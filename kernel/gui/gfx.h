/* kernel/gui/gfx.h - 2D software rasteriser: rects, gradients, rounded shapes,
   anti-aliased triangles/lines, text. Pixels are 0x00RRGGBB; colours passed in
   are 0xAARRGGBB (straight alpha). */
#ifndef BORNOMALA_GFX_H
#define BORNOMALA_GFX_H

#include <stdint.h>
#include <stdbool.h>
#include "font.h"

/* ── geometry ─────────────────────────────────────────────────────────────── */
typedef struct { int x, y, w, h; } rect_t;

static inline rect_t R(int x, int y, int w, int h) { rect_t r = { x, y, w, h }; return r; }
static inline bool rect_empty(rect_t r) { return r.w <= 0 || r.h <= 0; }
static inline bool rect_contains(rect_t r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
bool   rect_intersect(rect_t a, rect_t b, rect_t *out);
rect_t rect_union(rect_t a, rect_t b);           /* an empty rect is the identity */
rect_t rect_inflate(rect_t r, int d);

/* ── colours ──────────────────────────────────────────────────────────────── */
#define ARGB(a, r, g, b) ((uint32_t)(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))
#define RGB(r, g, b)     ARGB(255, r, g, b)
/* Same, with the alpha given as a percentage (CSS rgba alpha .72 -> 72). */
#define RGBA(r, g, b, pct) ARGB(((pct) * 255 + 50) / 100, r, g, b)
#define WHITE_A(pct)     RGBA(255, 255, 255, pct)
#define BLACK_A(pct)     RGBA(0, 0, 0, pct)

static inline uint32_t gfx_blend(uint32_t d, uint32_t s) {
    uint32_t a = s >> 24;
    if (a == 255) return s | 0xFF000000u;
    if (a == 0) return d;
    uint32_t ia = 255 - a;
    uint32_t r = (((s >> 16) & 255) * a + ((d >> 16) & 255) * ia + 127) / 255;
    uint32_t g = (((s >> 8) & 255) * a + ((d >> 8) & 255) * ia + 127) / 255;
    uint32_t b = ((s & 255) * a + (d & 255) * ia + 127) / 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
uint32_t color_lerp(uint32_t c0, uint32_t c1, int t256);      /* t256: 0..256 */
uint32_t color_with_alpha_scaled(uint32_t c, int scale255);   /* multiply alpha */

/* ── canvas ───────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t *px;
    int w, h;
    int stride;                 /* in PIXELS */
    rect_t clip;
} gfx_t;

void   gfx_init(gfx_t *g, uint32_t *px, int w, int h, int stride);
void   gfx_set_clip(gfx_t *g, rect_t r);                      /* clamped to the canvas */
rect_t gfx_clip(const gfx_t *g);
/* Intersects the current clip with r and returns the previous clip to restore. */
rect_t gfx_push_clip(gfx_t *g, rect_t r);
void   gfx_pop_clip(gfx_t *g, rect_t saved);
bool   gfx_visible(const gfx_t *g, rect_t r);                 /* does r touch the clip? */

/* ── paints ───────────────────────────────────────────────────────────────── */
typedef enum { PAINT_FLAT = 0, PAINT_VGRAD = 1, PAINT_DIAG = 2 } paint_kind_t;
typedef struct {
    paint_kind_t kind;
    int          n;             /* number of stops (1..4) */
    uint16_t     pos[4];        /* per-mille positions, ascending */
    uint32_t     col[4];
} paint_t;

paint_t paint_flat(uint32_t c);
paint_t paint_v2(uint32_t top, uint32_t bottom);
paint_t paint_v3(uint32_t top, uint32_t mid, int mid_permille, uint32_t bottom);
paint_t paint_v4(uint32_t c0, int p1, uint32_t c1, int p2, uint32_t c2, uint32_t c3);
paint_t paint_diag(uint32_t top_left, uint32_t bottom_right);  /* ~135deg CSS gradient */
paint_t paint_diag3(uint32_t c0, uint32_t mid, int mid_permille, uint32_t c1);

/* ── shapes ───────────────────────────────────────────────────────────────── */
void gfx_fill_rect(gfx_t *g, int x, int y, int w, int h, uint32_t argb);
void gfx_fill_paint(gfx_t *g, int x, int y, int w, int h, const paint_t *p);
void gfx_fill_round_rect(gfx_t *g, int x, int y, int w, int h, int r, uint32_t argb);
void gfx_fill_round_paint(gfx_t *g, int x, int y, int w, int h, int r, const paint_t *p);
/* Independent corner radii: top-left, top-right, bottom-right, bottom-left. */
void gfx_fill_shape(gfx_t *g, int x, int y, int w, int h,
                    int r_tl, int r_tr, int r_br, int r_bl, const paint_t *p);
void gfx_fill_circle(gfx_t *g, int cx, int cy, int r, uint32_t argb);
/* 1-pixel outline just inside the given rounded rectangle. */
void gfx_stroke_round_rect(gfx_t *g, int x, int y, int w, int h, int r, uint32_t argb);
/* Soft drop shadow drawn OUTSIDE the box (nothing is painted under it). */
void gfx_shadow(gfx_t *g, int x, int y, int w, int h, int r, int blur, int alpha_pct, int dx, int dy);
/* Elliptical radial glow: alpha fades linearly from `color`'s alpha at the centre to 0 at the edge. */
void gfx_radial_glow(gfx_t *g, int cx, int cy, int rx, int ry, uint32_t color);
/* Elliptical ring: alpha 0 below t_start, full at t_peak, 0 again at t_end (all in per-mille of rx/ry). */
void gfx_radial_band(gfx_t *g, int cx, int cy, int rx, int ry, uint32_t color, int t_start, int t_peak, int t_end);
/* A glow clipped to a circle: circle (ocx, ocy, orad) is the visible orb, the glow is centred at (gcx, gcy) with radius grad. */
void gfx_orb(gfx_t *g, int ocx, int ocy, int orad, int gcx, int gcy, int grad, uint32_t color);

void gfx_hline(gfx_t *g, int x, int y, int w, uint32_t argb);
void gfx_vline(gfx_t *g, int x, int y, int h, uint32_t argb);

/* Anti-aliased triangle / thick line. Coordinates are in 1/8 pixel (use FX()). */
#define FX(v) ((int)((v) * 8))
void gfx_fill_triangle_fx(gfx_t *g, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t argb);
void gfx_line_fx(gfx_t *g, int x0, int y0, int x1, int y1, int width_fx, uint32_t argb);

/* Copy a rectangle from another canvas (no blending). Both rects are clipped. */
void gfx_copy_rect(gfx_t *dst, const gfx_t *src, rect_t r);

/* ── text ─────────────────────────────────────────────────────────────────── */
int  font_text_width(const font_t *f, const char *s);                     /* pixels */
int  font_text_width_n(const font_t *f, const char *s, int n);
/* (x, baseline_y) is the pen start on the baseline. spacing_px is extra advance between glyphs. */
void gfx_text(gfx_t *g, const font_t *f, int x, int baseline_y, const char *s, uint32_t argb);
void gfx_text_ex(gfx_t *g, const font_t *f, int x, int baseline_y, const char *s, uint32_t argb, int spacing_px);
void gfx_text_shadow(gfx_t *g, const font_t *f, int x, int baseline_y, const char *s, uint32_t argb,
                     uint32_t shadow, int sx, int sy);
void gfx_text_center(gfx_t *g, const font_t *f, int cx, int baseline_y, const char *s, uint32_t argb);
/* Baseline that vertically centres capital letters in a box y..y+h. */
static inline int text_baseline_in(const font_t *f, int y, int h) { return y + (h + f->cap) / 2; }

/* Fixed 8x16 bitmap font (Notepad, panic screen). (x, y) is the top-left. */
void gfx_text_mono(gfx_t *g, int x, int y, const char *s, uint32_t argb);

#endif
