#include "icons.h"

/* sin(k * 15 deg) * 1024, k = 0..23. cos(k) = sin(k + 6). Angles run clockwise on screen (y down). */
static const int16_t SIN15[24] = { 0, 265, 512, 724, 887, 989, 1024, 989, 887, 724, 512, 265,
                                   0, -265, -512, -724, -887, -989, -1024, -989, -887, -724, -512, -265 };
static inline int sin15(int k) { return SIN15[((k % 24) + 24) % 24]; }
static inline int cos15(int k) { return sin15(k + 6); }

/* 8x-precision point on a circle */
static inline int ptx(int cx, int r, int k) { return cx * 8 + (r * 8 * cos15(k)) / 1024; }
static inline int pty(int cy, int r, int k) { return cy * 8 + (r * 8 * sin15(k)) / 1024; }

/* Arc from step k0 to k1 (15 degrees per step), drawn as short thick segments. Radius/width in 1/8 px. */
static void arc8(gfx_t *g, int cx8, int cy8, int r8, int k0, int k1, int w8, uint32_t c) {
    for (int k = k0; k < k1; k++) {
        int x0 = cx8 + (r8 * cos15(k)) / 1024, y0 = cy8 + (r8 * sin15(k)) / 1024;
        int x1 = cx8 + (r8 * cos15(k + 1)) / 1024, y1 = cy8 + (r8 * sin15(k + 1)) / 1024;
        gfx_line_fx(g, x0, y0, x1, y1, w8, c);
    }
}

static void ring(gfx_t *g, int cx, int cy, int r, int thick, uint32_t c) {
    for (int t = 0; t < thick; t++) gfx_stroke_round_rect(g, cx - r + t, cy - r + t, 2 * (r - t), 2 * (r - t), r - t, c);
}

/* ════════════════════════════════════════════════════════════════════════════
   application icons  (designed on a 64-unit grid, scaled to `size`)
   ════════════════════════════════════════════════════════════════════════════ */
void icon_draw(gfx_t *g, icon_id_t id, int x, int y, int size) {
    #define S(v) ((v) * size / 64)
    #define PX(v) (x + S(v))
    #define PY(v) (y + S(v))
    #define RR(a, b, c, d, r, col) gfx_fill_round_rect(g, PX(a), PY(b), S(c), S(d), S(r), (col))
    #define FXP(v) ((x * 8) + (v) * size / 8)      /* 1/8 px point from grid unit */
    #define FYP(v) ((y * 8) + (v) * size / 8)
    if (!gfx_visible(g, R(x - 2, y - 2, size + 4, size + 4))) return;

    switch (id) {
    case ICON_LAUNCHPAD: {
        static const uint32_t cols[9] = {
            RGB(0xE5,0x5B,0x4D), RGB(0xF0,0xA0,0x30), RGB(0xF2,0xD2,0x3C),
            RGB(0x58,0xC4,0x7A), RGB(0x3E,0xA5,0xE6), RGB(0x6E,0x7B,0xE8),
            RGB(0xB0,0x62,0xD8), RGB(0x7A,0x8A,0x9A), RGB(0x2F,0x8F,0x9D) };
        for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++)
            RR(9 + i * 16, 9 + j * 16, 13, 13, 4, cols[j * 3 + i]);
        break; }
    case ICON_FOLDER:
    case ICON_FILES: {
        RR(6, 12, 24, 14, 4, RGB(0xD9,0x9A,0x1E));
        paint_t p = paint_v2(RGB(0xFF,0xDC,0x7A), RGB(0xF0,0xB1,0x2C));
        gfx_fill_round_paint(g, PX(5), PY(20), S(54), S(36), S(5), &p);
        gfx_fill_rect(g, PX(6), PY(21), S(52), 1, WHITE_A(60));
        break; }
    case ICON_NOTEPAD: {
        RR(14, 6, 36, 52, 4, RGB(0xFA,0xFC,0xFE));
        gfx_stroke_round_rect(g, PX(14), PY(6), S(36), S(52), S(4), RGB(0x8F,0xA8,0xBC));
        { paint_t hp = paint_flat(RGB(0x3D,0x8F,0xD1)); gfx_fill_shape(g, PX(14), PY(6), S(36), S(11), S(4), S(4), 0, 0, &hp); }
        for (int i = 0; i < 4; i++) gfx_fill_rect(g, PX(20), PY(25 + i * 8), S(i == 3 ? 16 : 24), S(2) > 0 ? S(2) : 1, RGB(0x9A,0xAB,0xBA));
        break; }
    case ICON_DOC: {
        RR(14, 6, 36, 52, 3, RGB(0xFA,0xFC,0xFE));
        gfx_stroke_round_rect(g, PX(14), PY(6), S(36), S(52), S(3), RGB(0x8F,0xA8,0xBC));
        for (int i = 0; i < 5; i++) gfx_fill_rect(g, PX(20), PY(16 + i * 8), S(24), S(2) > 0 ? S(2) : 1, RGB(0xA4,0xB4,0xC2));
        break; }
    case ICON_CALC: {
        paint_t body = paint_v2(RGB(0x55,0x66,0x76), RGB(0x2B,0x37,0x42));
        gfx_fill_round_paint(g, PX(12), PY(5), S(40), S(54), S(6), &body);
        RR(17, 11, 30, 12, 2, RGB(0xA8,0xE6,0xC8));
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++)
            RR(17 + i * 8, 28 + j * 8, 6, 5, 1, i == 3 ? RGB(0xF0,0x9A,0x30) : RGB(0xD6,0xDE,0xE6));
        break; }
    case ICON_COMPUTER: {
        RR(7, 10, 50, 34, 3, RGB(0x2B,0x3A,0x48));
        paint_t scr = paint_v2(RGB(0x7C,0xC6,0xF6), RGB(0x1F,0x6F,0xB5));
        gfx_fill_paint(g, PX(10), PY(13), S(44), S(28), &scr);
        gfx_fill_rect(g, PX(10), PY(13), S(44), S(3), WHITE_A(25));
        RR(27, 44, 10, 6, 1, RGB(0x5C,0x6B,0x78));
        RR(19, 50, 26, 5, 2, RGB(0x6E,0x7D,0x8A));
        break; }
    case ICON_BROWSER: {
        paint_t p = paint_v2(RGB(0x6C,0xBC,0xFF), RGB(0x1B,0x6F,0xC0));
        gfx_fill_round_paint(g, PX(8), PY(8), S(48), S(48), S(24), &p);
        uint32_t w = WHITE_A(85);
        gfx_stroke_round_rect(g, PX(8), PY(8), S(48), S(48), S(24), w);
        gfx_stroke_round_rect(g, PX(22), PY(8), S(20), S(48), S(10), w);
        gfx_hline(g, PX(9), PY(32), S(46), w);
        gfx_hline(g, PX(13), PY(20), S(38), WHITE_A(55));
        gfx_hline(g, PX(13), PY(44), S(38), WHITE_A(55));
        break; }
    case ICON_PAINT: {
        paint_t p = paint_v2(RGB(0xF7,0xE0,0xB0), RGB(0xE2,0xB8,0x74));
        gfx_fill_round_paint(g, PX(6), PY(12), S(52), S(42), S(20), &p);
        int cx[4] = { 20, 32, 44, 40 }, cy[4] = { 28, 22, 28, 42 };
        uint32_t cc[4] = { RGB(0xE5,0x4B,0x3D), RGB(0xF2,0xC8,0x2C), RGB(0x3C,0xB0,0x5A), RGB(0x2E,0x8F,0xE0) };
        for (int i = 0; i < 4; i++) gfx_fill_circle(g, PX(cx[i]), PY(cy[i]), S(5), cc[i]);
        gfx_fill_circle(g, PX(22), PY(44), S(6), RGB(0xEA,0xF1,0xF7));
        break; }
    case ICON_MEDIA: {
        paint_t p = paint_v2(RGB(0x46,0x58,0x6A), RGB(0x1F,0x2A,0x35));
        gfx_fill_round_paint(g, PX(8), PY(8), S(48), S(48), S(24), &p);
        gfx_fill_triangle_fx(g, FXP(26), FYP(19), FXP(26), FYP(45), FXP(46), FYP(32), RGB(0xF4,0xF8,0xFC));
        break; }
    case ICON_SETTINGS: {
        uint32_t c = RGB(0x6B,0x7A,0x88);
        for (int k = 0; k < 8; k++) {
            int a = k * 3;
            gfx_fill_circle(g, PX(32) + (S(20) * cos15(a)) / 1024, PY(32) + (S(20) * sin15(a)) / 1024, S(5), c);
        }
        gfx_fill_circle(g, PX(32), PY(32), S(17), c);
        gfx_fill_circle(g, PX(32), PY(32), S(7), RGB(0xE9,0xF0,0xF6));
        break; }
    case ICON_CAMERA: {
        RR(22, 10, 20, 10, 3, RGB(0x2F,0x3A,0x45));
        paint_t p = paint_v2(RGB(0x54,0x63,0x72), RGB(0x2B,0x36,0x41));
        gfx_fill_round_paint(g, PX(6), PY(16), S(52), S(38), S(6), &p);
        gfx_fill_circle(g, PX(32), PY(35), S(13), RGB(0x1B,0x23,0x2B));
        gfx_fill_circle(g, PX(32), PY(35), S(9), RGB(0x4C,0x8B,0xD0));
        gfx_fill_circle(g, PX(29), PY(32), S(3), WHITE_A(70));
        RR(48, 21, 5, 4, 1, RGB(0xF0,0x9A,0x30));
        break; }
    case ICON_CALENDAR: {
        RR(9, 8, 46, 48, 5, RGB(0xFA,0xFC,0xFE));
        gfx_stroke_round_rect(g, PX(9), PY(8), S(46), S(48), S(5), RGB(0x9A,0xAE,0xBF));
        { paint_t hp = paint_flat(RGB(0xE0,0x48,0x3E)); gfx_fill_shape(g, PX(9), PY(8), S(46), S(14), S(5), S(5), 0, 0, &hp); }
        for (int j = 0; j < 3; j++) for (int i = 0; i < 4; i++)
            RR(14 + i * 10, 28 + j * 9, 7, 5, 1, RGB(0xA9,0xB7,0xC4));
        break; }
    case ICON_CODE: {
        RR(7, 10, 50, 44, 6, RGB(0x24,0x31,0x3D));
        uint32_t c = RGB(0x56,0xD4,0xF0);
        gfx_line_fx(g, FXP(27), FYP(23), FXP(17), FYP(32), 24, c);
        gfx_line_fx(g, FXP(17), FYP(32), FXP(27), FYP(41), 24, c);
        gfx_line_fx(g, FXP(37), FYP(23), FXP(47), FYP(32), 24, c);
        gfx_line_fx(g, FXP(47), FYP(32), FXP(37), FYP(41), 24, c);
        gfx_line_fx(g, FXP(35), FYP(21), FXP(29), FYP(43), 18, RGB(0xF0,0xC8,0x50));
        break; }
    case ICON_DASHBOARD: {
        RR(8, 8, 48, 48, 5, RGB(0xF4,0xF8,0xFB));
        gfx_stroke_round_rect(g, PX(8), PY(8), S(48), S(48), S(5), RGB(0xA9,0xBA,0xC8));
        RR(14, 34, 10, 16, 2, RGB(0x4E,0xA1,0xE8));
        RR(27, 24, 10, 26, 2, RGB(0x58,0xC4,0x7A));
        RR(40, 16, 10, 34, 2, RGB(0xF0,0xA0,0x30));
        break; }
    case ICON_MAIL: {
        paint_t p = paint_v2(RGB(0xFF,0xFF,0xFF), RGB(0xDD,0xE7,0xEF));
        gfx_fill_round_paint(g, PX(7), PY(14), S(50), S(36), S(4), &p);
        gfx_stroke_round_rect(g, PX(7), PY(14), S(50), S(36), S(4), RGB(0x8F,0xA6,0xB8));
        gfx_line_fx(g, FXP(9), FYP(17), FXP(32), FYP(35), 14, RGB(0x8F,0xA6,0xB8));
        gfx_line_fx(g, FXP(55), FYP(17), FXP(32), FYP(35), 14, RGB(0x8F,0xA6,0xB8));
        break; }
    case ICON_NOTES: {
        paint_t p = paint_v2(RGB(0xFF,0xEA,0x8C), RGB(0xF5,0xCF,0x55));
        gfx_fill_round_paint(g, PX(10), PY(8), S(44), S(48), S(4), &p);
        for (int i = 0; i < 4; i++) gfx_fill_rect(g, PX(16), PY(20 + i * 8), S(i == 3 ? 18 : 32), S(2) > 0 ? S(2) : 1, RGB(0xC9,0xA5,0x2E));
        break; }
    case ICON_TRASH: {
        RR(13, 15, 38, 5, 2, RGB(0x86,0x96,0xA4));
        RR(25, 10, 14, 6, 2, RGB(0x86,0x96,0xA4));
        uint32_t c = RGB(0xA9,0xB9,0xC6);
        gfx_fill_triangle_fx(g, FXP(16), FYP(22), FXP(48), FYP(22), FXP(44), FYP(55), c);
        gfx_fill_triangle_fx(g, FXP(16), FYP(22), FXP(44), FYP(55), FXP(20), FYP(55), c);
        for (int i = 0; i < 3; i++) gfx_fill_rect(g, PX(25 + i * 7), PY(27), S(2) > 0 ? S(2) : 1, S(22), WHITE_A(55));
        break; }
    case ICON_DRIVE: {
        paint_t p = paint_v2(RGB(0xC8,0xD3,0xDC), RGB(0x8C,0x9B,0xA8));
        gfx_fill_round_paint(g, PX(6), PY(20), S(52), S(26), S(5), &p);
        gfx_stroke_round_rect(g, PX(6), PY(20), S(52), S(26), S(5), RGB(0x6E,0x7D,0x8A));
        RR(11, 38, 26, 3, 1, RGB(0x6E,0x7D,0x8A));
        gfx_fill_circle(g, PX(50), PY(39), S(3), RGB(0x4C,0xD0,0x70));
        break; }
    default: break;
    }
    #undef S
    #undef PX
    #undef PY
    #undef RR
    #undef FXP
    #undef FYP
}

void icon_draw_tile(gfx_t *g, icon_id_t id, int x, int y, int size, int style) {
    if (!gfx_visible(g, R(x - 4, y - 2, size + 8, size + 14))) return;
    int r = size / 4;
    paint_t p;
    uint32_t border;
    switch (style) {
    case 1:  p = paint_diag(RGBA(255,255,255,92), RGBA(185,215,237,78)); border = WHITE_A(67); break;
    case 2:  p = paint_diag(RGB(0xEA,0xF7,0xFF), RGB(0x8E,0xBB,0xE0));   border = WHITE_A(67); break;
    case 3:  p = paint_diag(RGB(0xFF,0xFF,0xFF), RGB(0xC4,0xD0,0xDA));   border = WHITE_A(67); break;
    default: p = paint_diag(WHITE_A(53), RGBA(185,205,224,33));          border = WHITE_A(70); break;
    }
    /* soft shadow under the tile */
    gfx_shadow(g, x, y, size, size, r, style ? 8 : 4, style ? 45 : 35, 0, style ? 6 : 3);
    gfx_fill_round_paint(g, x, y, size, size, r, &p);
    gfx_stroke_round_rect(g, x, y, size, size, r, border);
    gfx_hline(g, x + r, y + 1, size - 2 * r, WHITE_A(70));         /* inset top highlight */
    int gs = size * 3 / 4;
    icon_draw(g, id, x + (size - gs) / 2, y + (size - gs) / 2, gs);
}

/* ════════════════════════════════════════════════════════════════════════════
   UI symbols
   ════════════════════════════════════════════════════════════════════════════ */
void sym_battery_level(gfx_t *g, int x, int y, int w, int h, int percent, uint32_t c) {
    gfx_stroke_round_rect(g, x, y, w, h, 3, c);
    gfx_fill_rect(g, x + w, y + h / 2 - 2, 2, 5, c);
    int fw = (w - 4) * percent / 100;
    if (fw > 0) gfx_fill_round_rect(g, x + 2, y + 2, fw, h - 4, 1, c);
}

void sym_draw(gfx_t *g, sym_id_t id, int cx, int cy, int size, uint32_t c) {
    int r = size / 2;
    if (!gfx_visible(g, R(cx - r - 2, cy - r - 2, size + 4, size + 4))) return;
    switch (id) {
    case SYM_POWER:
        /* ring open at the top: 300 deg -> 240 deg clockwise (steps 20..40), gap centred on 270 deg */
        arc8(g, cx * 8, cy * 8 + 4, (r - 3) * 8, 20, 40, 14, c);
        gfx_line_fx(g, cx * 8, (cy - r + 1) * 8, cx * 8, (cy) * 8, 18, c);
        break;
    case SYM_WIFI:
        arc8(g, cx * 8, (cy + r / 2 + 1) * 8, r * 8,        15, 22, 12, c);
        arc8(g, cx * 8, (cy + r / 2 + 1) * 8, r * 5,        15, 22, 12, c);
        gfx_fill_circle(g, cx, cy + r / 2, 1, c);
        break;
    case SYM_BATTERY: sym_battery_level(g, cx - r, cy - r / 2, size - 2, r, 82, c); break;
    case SYM_VOLUME:
        gfx_fill_rect(g, cx - r, cy - r / 3, r / 2, 2 * r / 3, c);
        gfx_fill_triangle_fx(g, FX(cx - r / 2), FX(cy - r / 3), FX(cx - r / 2), FX(cy + r / 3), FX(cx + 1), FX(cy + r * 2 / 3), c);
        gfx_fill_triangle_fx(g, FX(cx - r / 2), FX(cy - r / 3), FX(cx + 1), FX(cy - r * 2 / 3), FX(cx + 1), FX(cy + r * 2 / 3), c);
        arc8(g, (cx + 1) * 8, cy * 8, (r - 1) * 8, 21, 27, 12, c);
        break;
    case SYM_SEARCH: {
        int rr = r * 2 / 3;
        ring(g, cx - 1, cy - 1, rr, 2, c);
        gfx_line_fx(g, FX(cx - 1 + rr * 72 / 100), FX(cy - 1 + rr * 72 / 100), FX(cx + r - 1), FX(cy + r - 1), 20, c);
        break; }
    case SYM_ARROW_RIGHT:
        gfx_line_fx(g, FX(cx - r + 1), FX(cy), FX(cx + r - 1), FX(cy), 18, c);
        gfx_line_fx(g, FX(cx + r - 1), FX(cy), FX(cx + r / 2), FX(cy - r / 2), 18, c);
        gfx_line_fx(g, FX(cx + r - 1), FX(cy), FX(cx + r / 2), FX(cy + r / 2), 18, c);
        break;
    case SYM_ACCESS:
        ring(g, cx, cy, r, 2, c);
        gfx_fill_circle(g, cx, cy, r / 2 - 1, c);
        break;
    case SYM_CLOSE:
        gfx_line_fx(g, FX(cx - r), FX(cy - r), FX(cx + r), FX(cy + r), 20, c);
        gfx_line_fx(g, FX(cx - r), FX(cy + r), FX(cx + r), FX(cy - r), 20, c);
        break;
    case SYM_MINIMIZE: gfx_fill_rect(g, cx - r, cy + r / 2 - 1, 2 * r, 2, c); break;
    case SYM_MAXIMIZE: gfx_stroke_round_rect(g, cx - r, cy - r, 2 * r, 2 * r, 0, c); gfx_hline(g, cx - r, cy - r + 1, 2 * r, c); break;
    case SYM_RESTORE:
        gfx_stroke_round_rect(g, cx - r, cy - r + 3, 2 * r - 3, 2 * r - 3, 0, c);
        gfx_hline(g, cx - r + 3, cy - r, 2 * r - 3, c);
        gfx_vline(g, cx + r - 1, cy - r, 2 * r - 3, c);
        break;
    case SYM_GLOBE:
        ring(g, cx, cy, r, 1, c);
        gfx_stroke_round_rect(g, cx - r / 2, cy - r, r, 2 * r, r / 2, c);
        gfx_hline(g, cx - r, cy, 2 * r, c);
        break;
    case SYM_PERSON:
        gfx_fill_circle(g, cx, cy - r / 3, r / 3 + 1, c);
        { paint_t bp = paint_flat(c); gfx_fill_shape(g, cx - r / 2 - 1, cy + r / 6, r + 2, r * 3 / 4, r / 2 + 2, r / 2 + 2, 0, 0, &bp); }
        break;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   mouse pointer (classic arrow, black outline, white fill)
   ════════════════════════════════════════════════════════════════════════════ */
void cursor_draw(gfx_t *g, int x, int y) {
    if (!gfx_visible(g, R(x - 1, y - 1, CURSOR_W + 2, CURSOR_H + 2))) return;
    const int X = x * 8, Y = y * 8;                 /* offsets below are in 1/8 pixel */
    uint32_t edge = BLACK_A(85), fill = RGB(255, 255, 255);
    /* dark outline (slightly larger) ... */
    gfx_fill_triangle_fx(g, X - 8, Y - 12, X - 8, Y + 140, X + 36, Y + 106, edge);
    gfx_fill_triangle_fx(g, X - 8, Y - 12, X + 100, Y + 100, X + 36, Y + 106, edge);
    gfx_line_fx(g, X + 32, Y + 100, X + 64, Y + 164, 34, edge);
    /* ... and the white body */
    gfx_fill_triangle_fx(g, X + 2, Y + 6, X + 2, Y + 124, X + 32, Y + 96, fill);
    gfx_fill_triangle_fx(g, X + 2, Y + 6, X + 84, Y + 92, X + 32, Y + 96, fill);
    gfx_line_fx(g, X + 34, Y + 96, X + 61, Y + 152, 20, fill);
}
