#include "gfx.h"
#include "../klib.h"

/* ════════════════════════════════════════════════════════════════════════════
   geometry
   ════════════════════════════════════════════════════════════════════════════ */
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }

bool rect_intersect(rect_t a, rect_t b, rect_t *out) {
    int x0 = imax(a.x, b.x), y0 = imax(a.y, b.y);
    int x1 = imin(a.x + a.w, b.x + b.w), y1 = imin(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        if (out) { out->x = out->y = out->w = out->h = 0; }
        return false;
    }
    if (out) { out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0; }
    return true;
}

rect_t rect_union(rect_t a, rect_t b) {
    if (rect_empty(a)) return b;
    if (rect_empty(b)) return a;
    int x0 = imin(a.x, b.x), y0 = imin(a.y, b.y);
    int x1 = imax(a.x + a.w, b.x + b.w), y1 = imax(a.y + a.h, b.y + b.h);
    return R(x0, y0, x1 - x0, y1 - y0);
}

rect_t rect_inflate(rect_t r, int d) { return R(r.x - d, r.y - d, r.w + 2 * d, r.h + 2 * d); }

/* ════════════════════════════════════════════════════════════════════════════
   colour helpers
   ════════════════════════════════════════════════════════════════════════════ */
uint32_t color_lerp(uint32_t c0, uint32_t c1, int t) {
    if (t <= 0) return c0;
    if (t >= 256) return c1;
    int a0 = c0 >> 24, r0 = (c0 >> 16) & 255, g0 = (c0 >> 8) & 255, b0 = c0 & 255;
    int a1 = c1 >> 24, r1 = (c1 >> 16) & 255, g1 = (c1 >> 8) & 255, b1 = c1 & 255;
    int a = a0 + (((a1 - a0) * t) >> 8);
    int r = r0 + (((r1 - r0) * t) >> 8);
    int g = g0 + (((g1 - g0) * t) >> 8);
    int b = b0 + (((b1 - b0) * t) >> 8);
    return ARGB(a, r, g, b);
}

uint32_t color_with_alpha_scaled(uint32_t c, int scale) {
    if (scale >= 255) return c;
    if (scale <= 0) return c & 0x00FFFFFFu;
    uint32_t a = ((c >> 24) * (uint32_t)scale + 127) / 255;
    return (c & 0x00FFFFFFu) | (a << 24);
}

/* ════════════════════════════════════════════════════════════════════════════
   canvas + clip
   ════════════════════════════════════════════════════════════════════════════ */
void gfx_init(gfx_t *g, uint32_t *px, int w, int h, int stride) {
    g->px = px; g->w = w; g->h = h; g->stride = stride;
    g->clip = R(0, 0, w, h);
}

void gfx_set_clip(gfx_t *g, rect_t r) {
    rect_t out;
    if (!rect_intersect(r, R(0, 0, g->w, g->h), &out)) out = R(0, 0, 0, 0);
    g->clip = out;
}

rect_t gfx_clip(const gfx_t *g) { return g->clip; }

rect_t gfx_push_clip(gfx_t *g, rect_t r) {
    rect_t saved = g->clip, out;
    if (!rect_intersect(saved, r, &out)) out = R(0, 0, 0, 0);
    g->clip = out;
    return saved;
}

void gfx_pop_clip(gfx_t *g, rect_t saved) { g->clip = saved; }

bool gfx_visible(const gfx_t *g, rect_t r) { return rect_intersect(g->clip, r, 0); }

/* ════════════════════════════════════════════════════════════════════════════
   paints
   ════════════════════════════════════════════════════════════════════════════ */
paint_t paint_flat(uint32_t c) {
    paint_t p; memset(&p, 0, sizeof(p));
    p.kind = PAINT_FLAT; p.n = 1; p.col[0] = c;
    return p;
}
paint_t paint_v2(uint32_t top, uint32_t bottom) {
    paint_t p; memset(&p, 0, sizeof(p));
    p.kind = PAINT_VGRAD; p.n = 2;
    p.pos[0] = 0; p.pos[1] = 1000; p.col[0] = top; p.col[1] = bottom;
    return p;
}
paint_t paint_v3(uint32_t top, uint32_t mid, int mid_permille, uint32_t bottom) {
    paint_t p; memset(&p, 0, sizeof(p));
    p.kind = PAINT_VGRAD; p.n = 3;
    p.pos[0] = 0; p.pos[1] = (uint16_t)mid_permille; p.pos[2] = 1000;
    p.col[0] = top; p.col[1] = mid; p.col[2] = bottom;
    return p;
}
paint_t paint_v4(uint32_t c0, int p1, uint32_t c1, int p2, uint32_t c2, uint32_t c3) {
    paint_t p; memset(&p, 0, sizeof(p));
    p.kind = PAINT_VGRAD; p.n = 4;
    p.pos[0] = 0; p.pos[1] = (uint16_t)p1; p.pos[2] = (uint16_t)p2; p.pos[3] = 1000;
    p.col[0] = c0; p.col[1] = c1; p.col[2] = c2; p.col[3] = c3;
    return p;
}
paint_t paint_diag(uint32_t a, uint32_t b) {
    paint_t p = paint_v2(a, b);
    p.kind = PAINT_DIAG;
    return p;
}
paint_t paint_diag3(uint32_t c0, uint32_t mid, int mid_permille, uint32_t c1) {
    paint_t p = paint_v3(c0, mid, mid_permille, c1);
    p.kind = PAINT_DIAG;
    return p;
}

static uint32_t paint_at(const paint_t *p, int permille) {
    if (p->n <= 1 || permille <= p->pos[0]) return p->col[0];
    for (int i = 1; i < p->n; i++) {
        if (permille <= p->pos[i]) {
            int span = p->pos[i] - p->pos[i - 1];
            int u = span ? ((permille - p->pos[i - 1]) * 256) / span : 256;
            return color_lerp(p->col[i - 1], p->col[i], u);
        }
    }
    return p->col[p->n - 1];
}

/* ════════════════════════════════════════════════════════════════════════════
   rectangles
   ════════════════════════════════════════════════════════════════════════════ */
void gfx_fill_rect(gfx_t *g, int x, int y, int w, int h, uint32_t argb) {
    rect_t a;
    if (w <= 0 || h <= 0 || !rect_intersect(g->clip, R(x, y, w, h), &a)) return;
    uint32_t alpha = argb >> 24;
    if (alpha == 0) return;
    if (alpha == 255) {
        for (int yy = a.y; yy < a.y + a.h; yy++) mem_fill32(g->px + yy * g->stride + a.x, argb & 0x00FFFFFFu, (size_t)a.w);
        return;
    }
    for (int yy = a.y; yy < a.y + a.h; yy++) {
        uint32_t *row = g->px + yy * g->stride;
        for (int xx = a.x; xx < a.x + a.w; xx++) row[xx] = gfx_blend(row[xx], argb);
    }
}

void gfx_hline(gfx_t *g, int x, int y, int w, uint32_t argb) { gfx_fill_rect(g, x, y, w, 1, argb); }
void gfx_vline(gfx_t *g, int x, int y, int h, uint32_t argb) { gfx_fill_rect(g, x, y, 1, h, argb); }

/* coverage (0..255) of a pixel inside a rounded corner of radius r.
   cx, cy = distance in pixels from the corner's two outer edges (0-based). */
static inline int corner_cov(int cx, int cy, int r) {
    int dx2 = 2 * r - (2 * cx + 1);
    int dy2 = 2 * r - (2 * cy + 1);
    uint32_t d2 = (uint32_t)(dx2 * dx2 + dy2 * dy2);     /* (2*distance)^2 */
    int dist16 = (int)k_isqrt(d2 * 64u);                 /* = 16 * distance */
    int c = r * 16 + 8 - dist16;
    if (c <= 0) return 0;
    if (c >= 16) return 255;
    return (c * 255) >> 4;
}

void gfx_fill_shape(gfx_t *g, int x, int y, int w, int h,
                    int r_tl, int r_tr, int r_br, int r_bl, const paint_t *p) {
    rect_t a;
    if (w <= 0 || h <= 0 || !rect_intersect(g->clip, R(x, y, w, h), &a)) return;

    int maxr = imin(w, h) / 2;
    r_tl = imax(0, imin(r_tl, maxr)); r_tr = imax(0, imin(r_tr, maxr));
    r_br = imax(0, imin(r_br, maxr)); r_bl = imax(0, imin(r_bl, maxr));

    int diag_inv = 0;
    if (p->kind == PAINT_DIAG) diag_inv = (int)((1000u << 16) / (uint32_t)imax(1, w + h - 2));

    for (int yy = a.y; yy < a.y + a.h; yy++) {
        int ry = yy - y;
        uint32_t rowcol = 0;
        if (p->kind == PAINT_FLAT) rowcol = p->col[0];
        else if (p->kind == PAINT_VGRAD) rowcol = paint_at(p, ((ry * 2 + 1) * 1000) / (h * 2));

        int rl = 0, rr = 0, cyl = 0, cyr = 0;
        if (ry < r_tl)            { rl = r_tl; cyl = ry; }
        else if (ry >= h - r_bl)  { rl = r_bl; cyl = h - 1 - ry; }
        if (ry < r_tr)            { rr = r_tr; cyr = ry; }
        else if (ry >= h - r_br)  { rr = r_br; cyr = h - 1 - ry; }

        uint32_t *row = g->px + yy * g->stride;
        int xa = a.x, xb = a.x + a.w;
        int ix0 = imax(xa, x + rl), ix1 = imin(xb, x + w - rr);
        if (ix1 < ix0) ix1 = ix0;

        /* left edge pixels (rounded corner, if any) */
        for (int xx = xa; xx < ix0 && xx < xb; xx++) {
            int cov = corner_cov(xx - x, cyl, rl);
            if (!cov) continue;
            uint32_t c = (p->kind == PAINT_DIAG)
                ? paint_at(p, (((xx - x) + ry) * diag_inv) >> 16) : rowcol;
            row[xx] = gfx_blend(row[xx], color_with_alpha_scaled(c, cov));
        }
        /* interior */
        if (p->kind == PAINT_DIAG) {
            for (int xx = ix0; xx < ix1; xx++) {
                uint32_t c = paint_at(p, (((xx - x) + ry) * diag_inv) >> 16);
                row[xx] = gfx_blend(row[xx], c);
            }
        } else if ((rowcol >> 24) == 255) {
            if (ix1 > ix0) mem_fill32(row + ix0, rowcol & 0x00FFFFFFu, (size_t)(ix1 - ix0));
        } else if ((rowcol >> 24) != 0) {
            for (int xx = ix0; xx < ix1; xx++) row[xx] = gfx_blend(row[xx], rowcol);
        }
        /* right edge pixels */
        for (int xx = imax(ix1, xa); xx < xb; xx++) {
            int cov = corner_cov(w - 1 - (xx - x), cyr, rr);
            if (!cov) continue;
            uint32_t c = (p->kind == PAINT_DIAG)
                ? paint_at(p, (((xx - x) + ry) * diag_inv) >> 16) : rowcol;
            row[xx] = gfx_blend(row[xx], color_with_alpha_scaled(c, cov));
        }
    }
}

void gfx_fill_paint(gfx_t *g, int x, int y, int w, int h, const paint_t *p) {
    gfx_fill_shape(g, x, y, w, h, 0, 0, 0, 0, p);
}
void gfx_fill_round_paint(gfx_t *g, int x, int y, int w, int h, int r, const paint_t *p) {
    gfx_fill_shape(g, x, y, w, h, r, r, r, r, p);
}
void gfx_fill_round_rect(gfx_t *g, int x, int y, int w, int h, int r, uint32_t argb) {
    paint_t p = paint_flat(argb);
    gfx_fill_shape(g, x, y, w, h, r, r, r, r, &p);
}
void gfx_fill_circle(gfx_t *g, int cx, int cy, int r, uint32_t argb) {
    gfx_fill_round_rect(g, cx - r, cy - r, 2 * r, 2 * r, r, argb);
}

void gfx_stroke_round_rect(gfx_t *g, int x, int y, int w, int h, int r, uint32_t argb) {
    if (w < 2 || h < 2) return;
    r = imax(0, imin(r, imin(w, h) / 2));
    gfx_fill_rect(g, x + r, y, w - 2 * r, 1, argb);
    gfx_fill_rect(g, x + r, y + h - 1, w - 2 * r, 1, argb);
    gfx_fill_rect(g, x, y + r, 1, h - 2 * r, argb);
    gfx_fill_rect(g, x + w - 1, y + r, 1, h - 2 * r, argb);
    if (r == 0) return;

    rect_t bb = R(x, y, w, h);
    if (!rect_intersect(g->clip, bb, 0)) return;
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            int outer = corner_cov(cx, cy, r);
            int inner = (cx >= 1 && cy >= 1 && r > 1) ? corner_cov(cx - 1, cy - 1, r - 1) : 0;
            int cov = outer - inner;
            if (cov <= 0) continue;
            uint32_t c = color_with_alpha_scaled(argb, cov);
            int px[4] = { x + cx, x + w - 1 - cx, x + w - 1 - cx, x + cx };
            int py[4] = { y + cy, y + cy, y + h - 1 - cy, y + h - 1 - cy };
            for (int k = 0; k < 4; k++) {
                if (!rect_contains(g->clip, px[k], py[k])) continue;
                uint32_t *d = g->px + py[k] * g->stride + px[k];
                *d = gfx_blend(*d, c);
            }
        }
    }
}

void gfx_shadow(gfx_t *g, int x, int y, int w, int h, int r, int blur, int alpha_pct, int dx, int dy) {
    if (blur < 1) return;
    rect_t whole = rect_inflate(R(x + dx, y + dy, w, h), blur + 1);
    if (!rect_intersect(g->clip, whole, 0)) return;
    for (int i = 1; i <= blur; i++) {
        int t = blur + 1 - i;                          /* falls off towards the outside */
        int a = (alpha_pct * t * t * 255) / (100 * (blur + 1) * (blur + 1));
        if (a <= 0) continue;
        gfx_stroke_round_rect(g, x + dx - i, y + dy - i, w + 2 * i, h + 2 * i, r + i, ARGB(a, 0, 0, 0));
    }
}

void gfx_radial_glow(gfx_t *g, int cx, int cy, int rx, int ry, uint32_t color) {
    if (rx <= 0 || ry <= 0) return;
    rect_t a;
    if (!rect_intersect(g->clip, R(cx - rx, cy - ry, 2 * rx + 1, 2 * ry + 1), &a)) return;
    int inv_x = (int)((1024u << 16) / (uint32_t)rx);
    int inv_y = (int)((1024u << 16) / (uint32_t)ry);
    uint32_t a0 = color >> 24;
    for (int yy = a.y; yy < a.y + a.h; yy++) {
        int ny = (int)(((int64_t)(yy - cy) * inv_y) >> 16);
        uint32_t *row = g->px + yy * g->stride;
        for (int xx = a.x; xx < a.x + a.w; xx++) {
            int nx = (int)(((int64_t)(xx - cx) * inv_x) >> 16);
            uint32_t d2 = (uint32_t)(nx * nx + ny * ny);
            if (d2 >= (1u << 20)) continue;
            uint32_t d = k_isqrt(d2);                         /* 0..1023 */
            uint32_t al = (a0 * (1024u - d)) >> 10;
            if (!al) continue;
            row[xx] = gfx_blend(row[xx], (color & 0x00FFFFFFu) | (al << 24));
        }
    }
}

void gfx_radial_band(gfx_t *g, int cx, int cy, int rx, int ry, uint32_t color, int t0, int t1, int t2) {
    if (rx <= 0 || ry <= 0 || t2 <= t0) return;
    int ex = rx * t2 / 1000 + 1, ey = ry * t2 / 1000 + 1;
    rect_t a;
    if (!rect_intersect(g->clip, R(cx - ex, cy - ey, 2 * ex + 1, 2 * ey + 1), &a)) return;
    int inv_x = (int)((1000u << 16) / (uint32_t)rx);
    int inv_y = (int)((1000u << 16) / (uint32_t)ry);
    uint32_t a0 = color >> 24;
    for (int yy = a.y; yy < a.y + a.h; yy++) {
        int ny = (int)(((int64_t)(yy - cy) * inv_y) >> 16);
        uint32_t *row = g->px + yy * g->stride;
        for (int xx = a.x; xx < a.x + a.w; xx++) {
            int nx = (int)(((int64_t)(xx - cx) * inv_x) >> 16);
            uint32_t d2 = (uint32_t)(nx * nx + ny * ny);
            if (d2 >= (uint32_t)t2 * (uint32_t)t2 || d2 <= (uint32_t)t0 * (uint32_t)t0) continue;
            int d = (int)k_isqrt(d2);
            int al;
            if (d < t1) al = (int)(a0 * (uint32_t)(d - t0)) / imax(1, t1 - t0);
            else        al = (int)(a0 * (uint32_t)(t2 - d)) / imax(1, t2 - t1);
            if (al <= 0) continue;
            row[xx] = gfx_blend(row[xx], (color & 0x00FFFFFFu) | ((uint32_t)al << 24));
        }
    }
}

void gfx_orb(gfx_t *g, int ocx, int ocy, int orad, int gcx, int gcy, int grad, uint32_t color) {
    rect_t a;
    if (orad <= 0 || grad <= 0 || !rect_intersect(g->clip, R(ocx - orad, ocy - orad, 2 * orad + 1, 2 * orad + 1), &a)) return;
    uint32_t a0 = color >> 24;
    int inv = (int)((1024u << 16) / (uint32_t)grad);
    for (int yy = a.y; yy < a.y + a.h; yy++) {
        uint32_t *row = g->px + yy * g->stride;
        int ny = (int)(((int64_t)(yy - gcy) * inv) >> 16);
        for (int xx = a.x; xx < a.x + a.w; xx++) {
            int nx = (int)(((int64_t)(xx - gcx) * inv) >> 16);
            uint32_t d2 = (uint32_t)(nx * nx + ny * ny);
            if (d2 >= (1u << 20)) continue;
            uint32_t d = k_isqrt(d2);
            uint32_t al = (a0 * (1024u - d)) >> 10;
            /* soft circular edge (1.5 px) */
            int ex = xx - ocx, ey = yy - ocy;
            int dist16 = (int)k_isqrt((uint32_t)(ex * ex + ey * ey) * 256u);
            int edge = orad * 16 + 16 - dist16;
            if (edge <= 0) continue;
            if (edge < 24) al = al * (uint32_t)edge / 24u;
            if (!al) continue;
            row[xx] = gfx_blend(row[xx], (color & 0x00FFFFFFu) | (al << 24));
        }
    }
}

void gfx_copy_rect(gfx_t *dst, const gfx_t *src, rect_t r) {
    rect_t a;
    if (!rect_intersect(r, R(0, 0, src->w, src->h), &a)) return;
    if (!rect_intersect(a, dst->clip, &a)) return;
    for (int yy = a.y; yy < a.y + a.h; yy++)
        mem_copy32(dst->px + yy * dst->stride + a.x, src->px + yy * src->stride + a.x, (size_t)a.w);
}

/* ════════════════════════════════════════════════════════════════════════════
   anti-aliased convex polygons (4x4 sub-sampling), coordinates in 1/8 pixel
   ════════════════════════════════════════════════════════════════════════════ */
static void fill_convex_fx(gfx_t *g, const int *xs, const int *ys, int n, uint32_t argb) {
    int minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int i = 1; i < n; i++) {
        minx = imin(minx, xs[i]); maxx = imax(maxx, xs[i]);
        miny = imin(miny, ys[i]); maxy = imax(maxy, ys[i]);
    }
    rect_t bb = R(minx >> 3, miny >> 3, ((maxx + 7) >> 3) - (minx >> 3) + 1, ((maxy + 7) >> 3) - (miny >> 3) + 1);
    rect_t a;
    if (!rect_intersect(g->clip, bb, &a)) return;

    /* orientation: make "inside" the positive side of every edge */
    int64_t area2 = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area2 += (int64_t)xs[i] * ys[j] - (int64_t)xs[j] * ys[i];
    }
    int sign = area2 >= 0 ? 1 : -1;

    for (int py = a.y; py < a.y + a.h; py++) {
        uint32_t *row = g->px + py * g->stride;
        for (int px = a.x; px < a.x + a.w; px++) {
            int count = 0;
            for (int sy = 0; sy < 4; sy++) {
                int sample_y = py * 8 + 2 * sy + 1;
                for (int sx = 0; sx < 4; sx++) {
                    int sample_x = px * 8 + 2 * sx + 1;
                    bool inside = true;
                    for (int i = 0; i < n && inside; i++) {
                        int j = (i + 1) % n;
                        int64_t e = (int64_t)(xs[j] - xs[i]) * (sample_y - ys[i]) -
                                    (int64_t)(ys[j] - ys[i]) * (sample_x - xs[i]);
                        if (e * sign < 0) inside = false;
                    }
                    if (inside) count++;
                }
            }
            if (!count) continue;
            uint32_t c = count == 16 ? argb : color_with_alpha_scaled(argb, count * 255 / 16);
            row[px] = gfx_blend(row[px], c);
        }
    }
}

void gfx_fill_triangle_fx(gfx_t *g, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t argb) {
    int xs[3] = { x0, x1, x2 }, ys[3] = { y0, y1, y2 };
    fill_convex_fx(g, xs, ys, 3, argb);
}

void gfx_line_fx(gfx_t *g, int x0, int y0, int x1, int y1, int width_fx, uint32_t argb) {
    int dx = x1 - x0, dy = y1 - y0;
    uint32_t len = k_isqrt((uint32_t)(dx * dx + dy * dy));
    if (len == 0) return;
    /* half-width normal, in 1/8 px */
    int nx = (int)(((int64_t)(-dy) * width_fx) / (2 * (int64_t)len));
    int ny = (int)(((int64_t)dx * width_fx) / (2 * (int64_t)len));
    int xs[4] = { x0 + nx, x1 + nx, x1 - nx, x0 - nx };
    int ys[4] = { y0 + ny, y1 + ny, y1 - ny, y0 - ny };
    fill_convex_fx(g, xs, ys, 4, argb);
}

/* ════════════════════════════════════════════════════════════════════════════
   text
   ════════════════════════════════════════════════════════════════════════════ */
static const glyph_t *glyph_for(const font_t *f, unsigned char ch) {
    if (ch < f->first || ch >= f->first + f->count) return 0;
    return &f->glyphs[ch - f->first];
}

int font_text_width_n(const font_t *f, const char *s, int n) {
    int sum16 = 0;
    for (int i = 0; s[i] && (n < 0 || i < n); i++) {
        const glyph_t *gl = glyph_for(f, (unsigned char)s[i]);
        if (gl) sum16 += gl->adv16;
    }
    return (sum16 + 8) >> 4;
}

int font_text_width(const font_t *f, const char *s) { return font_text_width_n(f, s, -1); }

static void draw_mask(gfx_t *g, const glyph_t *gl, const uint8_t *bitmap, int gx, int gy, uint32_t argb) {
    rect_t a;
    if (!gl->w || !gl->h || !rect_intersect(g->clip, R(gx, gy, gl->w, gl->h), &a)) return;
    const uint8_t *mask = bitmap + gl->off;
    uint32_t ca = argb >> 24;
    for (int yy = a.y; yy < a.y + a.h; yy++) {
        const uint8_t *m = mask + (yy - gy) * gl->w + (a.x - gx);
        uint32_t *row = g->px + yy * g->stride;
        for (int xx = a.x; xx < a.x + a.w; xx++, m++) {
            uint32_t cov = *m;
            if (!cov) continue;
            uint32_t al = (ca == 255) ? cov : (cov * ca + 127) / 255;
            row[xx] = gfx_blend(row[xx], (argb & 0x00FFFFFFu) | (al << 24));
        }
    }
}

void gfx_text_ex(gfx_t *g, const font_t *f, int x, int by, const char *s, uint32_t argb, int spacing_px) {
    int pen16 = x * 16;
    for (; *s; s++) {
        const glyph_t *gl = glyph_for(f, (unsigned char)*s);
        if (!gl) continue;
        draw_mask(g, gl, f->bitmap, ((pen16 + 8) >> 4) + gl->xoff, by + gl->yoff, argb);
        pen16 += gl->adv16 + spacing_px * 16;
    }
}

void gfx_text(gfx_t *g, const font_t *f, int x, int by, const char *s, uint32_t argb) {
    gfx_text_ex(g, f, x, by, s, argb, 0);
}

void gfx_text_shadow(gfx_t *g, const font_t *f, int x, int by, const char *s, uint32_t argb,
                     uint32_t shadow, int sx, int sy) {
    gfx_text(g, f, x + sx, by + sy, s, shadow);
    gfx_text(g, f, x, by, s, argb);
}

void gfx_text_center(gfx_t *g, const font_t *f, int cx, int by, const char *s, uint32_t argb) {
    gfx_text(g, f, cx - font_text_width(f, s) / 2, by, s, argb);
}

void gfx_text_mono(gfx_t *g, int x, int y, const char *s, uint32_t argb) {
    for (; *s; s++, x += 8) {
        unsigned char ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) continue;
        rect_t a;
        if (!rect_intersect(g->clip, R(x, y, 8, 16), &a)) continue;
        const uint8_t *glyph = font_mono8x16[ch - 32];
        for (int yy = a.y; yy < a.y + a.h; yy++) {
            uint8_t bits = glyph[yy - y];
            uint32_t *row = g->px + yy * g->stride;
            for (int xx = a.x; xx < a.x + a.w; xx++)
                if (bits & (0x80u >> (xx - x))) row[xx] = gfx_blend(row[xx], argb);
        }
    }
}
