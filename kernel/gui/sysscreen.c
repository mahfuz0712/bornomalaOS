#include "sysscreen.h"
#include "compositor.h"
#include "icons.h"
#include "../interrupts.h"
#include "../klib.h"

/* progress fill easing, matches the reference's fill keyframes (4% -> 73% -> 96%) but on one pass */
#define PROGRESS_TICKS 220      /* ~2.2 s at 100 Hz */

static sysscreen_kind_t kind;
static char     initial[2];
static uint64_t t0;

static const char *title_of(sysscreen_kind_t k) {
    switch (k) {
    case SYS_BOOT:           return "Starting up";
    case SYS_SHUTDOWN:       return "Shutting down";
    case SYS_RESTART:        return "Restarting";
    case SYS_SIGNOUT:        return "Signing out";
    case SYS_INSTALLER_INFO: return "Bornomala Installer";
    }
    return "";
}
static const char *sub_of(sysscreen_kind_t k) {
    switch (k) {
    case SYS_BOOT:           return "Loading BornomalaOS";
    case SYS_SHUTDOWN:       return "Saving your session and powering off";
    case SYS_RESTART:        return "Closing services and restarting BornomalaOS";
    case SYS_SIGNOUT:        return "Signing out of the current user session";
    case SYS_INSTALLER_INFO: return "The graphical installer is not available in this build yet";
    }
    return "";
}
static const char *note_of(sysscreen_kind_t k) {
    if (k == SYS_SIGNOUT) return "Returning to the sign-in screen...";
    if (k == SYS_INSTALLER_INFO) return "Press any key, or click, to continue to the Live Desktop";
    return "Please wait...";
}

void sysscreen_enter(sysscreen_kind_t k, const char *user_initial) {
    kind = k;
    initial[0] = (user_initial && *user_initial) ? *user_initial : 'U';
    initial[1] = '\0';
    t0 = timer_ticks();
    compositor_damage_all();
}

/* Same dark navy gradient + soft light pools used by the lock screen, so all system
   scenes read as one family. Baked once per entry into the background layer. */
static void bake_background(void) {
    gfx_t *bg = compositor_background();
    int W = bg->w, H = bg->h;
    gfx_set_clip(bg, R(0, 0, W, H));
    paint_t base = paint_diag3(RGB(0x07, 0x11, 0x1D), RGB(0x0C, 0x20, 0x34), 480, RGB(0x06, 0x10, 0x1B));
    gfx_fill_paint(bg, 0, 0, W, H, &base);
    int dx = W * 80 / 100, dy = H * 75 / 100;
    int pool = (int)(k_isqrt((uint32_t)(dx * dx + dy * dy)) * 3 / 10);
    gfx_radial_glow(bg, W * 50 / 100, H * 12 / 100, pool, pool, ARGB(46, 77, 151, 190));
    gfx_radial_glow(bg, W * 15 / 100, H * 85 / 100, pool, pool, ARGB(38, 12, 78, 105));
    gfx_fill_rect(bg, 0, 0, W, H, RGBA(3, 13, 23, 10));
}

/* sine table reused from icons.c's approach, kept local so this file has no odd dependency */
static const int16_t SIN15[24] = { 0, 265, 512, 724, 887, 989, 1024, 989, 887, 724, 512, 265,
                                   0, -265, -512, -724, -887, -989, -1024, -989, -887, -724, -512, -265 };
static int sin15(int k) { return SIN15[((k % 24) + 24) % 24]; }

/* triangular wave 0..1000..0 over `period` ticks, for the float/halo breathing animation */
static int tri1000(uint64_t t, int period) {
    int phase = (int)(t % (uint64_t)period);
    int half = period / 2;
    return phase < half ? (phase * 1000) / half : 1000 - ((phase - half) * 1000) / half;
}

static void draw_logo(gfx_t *g, int cx, int cy, uint64_t t) {
    int breathe = tri1000(t, 280);                          /* ~2.8 s cycle, matches the reference */
    int r = 56 + breathe * 8 / 1000;
    int float_y = -4 + breathe * 4 / 1000;
    int halo_r = 44 + breathe * 10 / 1000;
    gfx_radial_glow(g, cx, cy + float_y, halo_r, halo_r, ARGB(56, 105, 204, 255));
    paint_t p = paint_diag3(RGB(0xF4, 0xFD, 0xFF), RGB(0x58, 0xB9, 0xE5), 500, RGB(0x20, 0x74, 0x9E));
    gfx_fill_round_paint(g, cx - r, cy + float_y - r, 2 * r, 2 * r, r * 28 / 56, &p);
    gfx_stroke_round_rect(g, cx - r, cy + float_y - r, 2 * r, 2 * r, r * 28 / 56, WHITE_A(45));
    const glyph_t *gl = &font_logo.glyphs[0];
    int scale_h = font_logo.cap * 2;
    (void)scale_h;
    gfx_text(g, &font_ui24, cx - font_text_width(&font_ui24, "B") / 2, cy + float_y + font_ui24.cap / 2, "B", RGB(0x08, 0x22, 0x35));
    (void)gl; (void)sin15;
}

/* bottom-center bold wordmark, present on every system scene (matches the design reference) */
static void draw_brand(gfx_t *g) {
    int W = compositor_width(), H = compositor_height();
    const char *brand = "bornomalaOS";
    int w = font_text_width(&font_ui24, brand);
    int x = W / 2 - w / 2, y = H - 34;
    gfx_text(g, &font_ui24, x + 1, y, brand, BLACK_A(60));      /* poor-man's bold via offset + shadow */
    gfx_text(g, &font_ui24, x, y, brand, WHITE_A(92));
}

static void draw_avatar(gfx_t *g, int cx, int cy) {
    int r = 33;
    paint_t p = paint_v3(RGB(0xEE, 0xF8, 0xFC), RGB(0x79, 0xB8, 0xD2), 450, RGB(0x35, 0x6D, 0x86));
    gfx_fill_round_paint(g, cx - r, cy - r, 2 * r, 2 * r, r, &p);
    gfx_stroke_round_rect(g, cx - r, cy - r, 2 * r, 2 * r, r, RGBA(220, 245, 255, 65));
    gfx_text_center(g, &font_ui24, cx, cy + font_ui24.cap / 2, initial, RGB(0x17, 0x38, 0x4B));
}

void sysscreen_draw(gfx_t *g, uint64_t ticks) {
    static sysscreen_kind_t baked_kind = (sysscreen_kind_t)-1;
    if (baked_kind != kind) { bake_background(); baked_kind = kind; }

    int W = compositor_width(), H = compositor_height();
    int cx = W / 2, cy = H / 2 - (kind == SYS_BOOT ? 30 : 0);
    uint64_t t = ticks - t0;

    if (kind == SYS_BOOT) {
        draw_logo(g, cx, cy - 40, t);
        gfx_text_center(g, &font_ui24, cx, cy + 76, title_of(kind), RGB(255, 255, 255));
        gfx_text_center(g, &font_ui13, cx, cy + 100, sub_of(kind), WHITE_A(78));
        int pw = 430 < W - 80 ? 430 : W - 80;
        int px = cx - pw / 2, py = cy + 120;
        gfx_fill_round_rect(g, px, py, pw, 11, 5, BLACK_A(55));
        gfx_stroke_round_rect(g, px, py, pw, 11, 5, BLACK_A(72));
        int fillp = t >= PROGRESS_TICKS ? 960 : (int)((t * 960) / PROGRESS_TICKS);
        if (fillp < 40) fillp = 40;
        paint_t fp = paint_v3(RGB(0xBD, 0xEE, 0xFF), RGB(0x62, 0xB8, 0xE2), 430, RGB(0x27, 0x7D, 0xA8));
        gfx_fill_round_paint(g, px + 2, py + 2, (pw - 4) * fillp / 1000, 7, 3, &fp);
        draw_brand(g);
        return;
    }

    /* Shutdown / Restart / Sign out: a glass panel, matching lock-screen chrome */
    int pw = 500 < W - 60 ? 500 : W - 60;
    int ph = kind == SYS_SIGNOUT ? 216 : (kind == SYS_INSTALLER_INFO ? 168 : 176);
    int px = cx - pw / 2, py = cy - ph / 2;
    for (int t3 = 3; t3 >= 1; t3--) gfx_stroke_round_rect(g, px - t3, py - t3, pw + 2 * t3, ph + 2 * t3, 9 + t3, WHITE_A(6));
    paint_t panel = paint_v3(RGBA(180, 220, 238, 24), RGBA(40, 92, 118, 30), 460, RGBA(3, 18, 30, 72));
    gfx_fill_round_paint(g, px, py, pw, ph, 9, &panel);
    gfx_stroke_round_rect(g, px, py, pw, ph, 9, RGBA(190, 235, 255, 38));
    gfx_hline(g, px + 9, py + 1, pw - 18, WHITE_A(25));

    int y = py + 24;
    if (kind == SYS_SIGNOUT) { draw_avatar(g, cx, y + 33); y += 80; }
    gfx_text_center(g, &font_ui24, cx, y + 20, title_of(kind), RGB(255, 255, 255));
    y += 20 + 24;
    gfx_text_center(g, &font_ui13, cx, y, sub_of(kind), RGB(0xA9, 0xC7, 0xD6));
    y += 30;
    if (kind != SYS_INSTALLER_INFO) {
        int barw = pw - 2 * (kind == SYS_SIGNOUT ? 34 : 32);
        int bx = cx - barw / 2;
        gfx_fill_round_rect(g, bx, y, barw, 11, 5, BLACK_A(55));
        gfx_stroke_round_rect(g, bx, y, barw, 11, 5, BLACK_A(72));
        int fillp = t >= PROGRESS_TICKS ? 960 : (int)((t * 960) / PROGRESS_TICKS);
        if (fillp < 40) fillp = 40;
        paint_t fp = paint_v3(RGB(0xBD, 0xEE, 0xFF), RGB(0x62, 0xB8, 0xE2), 430, RGB(0x27, 0x7D, 0xA8));
        gfx_fill_round_paint(g, bx + 2, y + 2, (barw - 4) * fillp / 1000, 7, 3, &fp);
        y += 22;
    } else {
        y += 4;
    }
    gfx_text_center(g, &font_ui12, cx, y, note_of(kind), RGB(0x9B, 0xB8, 0xC7));

    draw_brand(g);
}

bool sysscreen_progress_done(uint64_t ticks) { return (ticks - t0) >= PROGRESS_TICKS; }
