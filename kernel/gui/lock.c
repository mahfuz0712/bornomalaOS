#include "lock.h"
#include "compositor.h"
#include "icons.h"
#include "menu.h"
#include "session.h"
#include "uistate.h"
#include "../klib.h"
#include "../console.h"
#include "../input.h"
#include "../interrupts.h"
#include "../lockscreen.h"
#include "../rtc.h"
#include "../users.h"

/* ── state ────────────────────────────────────────────────────────────────── */
enum { MENU_RESTART = 1, MENU_SHUTDOWN = 2, MENU_SLEEP = 3 };
enum { HOVER_NONE = 0, HOVER_UNLOCK, HOVER_POWER, HOVER_ACCESS };

#define PW_MAX 63

static char     password[PW_MAX + 1];
static int      pw_len;
static bool     show_error;
static popup_t  menu;
static int      hover;
static char     hm[8], date_long[32];
static uint64_t last_sec = (uint64_t)-1;
static char     display_name[BM_DISPLAY_NAME_MAX + 1];

typedef struct {
    const font_t *clock;
    int   clock_spacing;
    int   time_by, date_by;
    int   av_cx, av_top;
    int   name_by, hint_by;
    rect_t field, btn;
    int   err_by;
    rect_t access, power;
    rect_t login_area;      /* damage rect for the whole form */
    rect_t clock_area;
} layout_t;

static layout_t L;

static int clock_text_width(const char *s) {
    int n = (int)k_strlen(s);
    return font_text_width(L.clock, s) + (n > 1 ? L.clock_spacing * (n - 1) : 0);
}

static void compute_layout(void) {
    int W = compositor_width(), H = compositor_height();
    L.clock = W >= 1200 ? &font_clock144 : (W >= 900 ? &font_clock104 : &font_clock76);
    L.clock_spacing = W >= 1200 ? -5 : (W >= 900 ? -4 : -3);

    int stack = L.clock->cap + 17 + 30 + 46 + 92 + 14 + 26 + 5 + 16 + 18 + 43 + 9 + 18;
    int avail = H - 78;                        /* final design: no top navigation bar */
    int top = (avail - stack) / 2 - 10;
    if (top < 30) top = 30;

    int cx = W / 2, y = top;
    L.time_by = y + L.clock->cap;   y += L.clock->cap + 17;
    L.date_by = y + 20;             y += 30 + 46;
    L.av_cx = cx; L.av_top = y;     y += 92 + 14;
    L.name_by = y + 19;             y += 26 + 5;
    L.hint_by = y + 11;             y += 16 + 18;
    L.field = R(cx - 150, y, 249, 43);
    L.btn   = R(cx + 150 - 43, y, 43, 43);
    y += 43 + 9;
    L.err_by = y + 12;              y += 18;

    L.access = R(28, H - 61, 44, 44);
    L.power  = R(W - 28 - 44, H - 61, 44, 44);
    L.login_area = R(cx - 190, L.av_top - 8, 380, y - L.av_top + 16);
    L.clock_area = R(cx - 330, top - 6, 660, L.clock->cap + 17 + 34 + 12);
}

/* ── clock ────────────────────────────────────────────────────────────────── */
static void update_time(bool force_damage) {
    rtc_time_t t;
    rtc_read(&t);
    char nhm[8], nd[32];
    rtc_format_hm(&t, nhm);
    rtc_format_date_long(&t, nd);
    if (force_damage || k_strcmp(nhm, hm) != 0 || k_strcmp(nd, date_long) != 0) {
        k_strcpy(hm, nhm);
        k_strcpy(date_long, nd);
        compositor_add_damage(L.clock_area);
    }
}

void lock_tick(uint64_t ticks) {
    uint64_t sec = ticks / 100;
    if (sec == last_sec) return;
    last_sec = sec;
    update_time(false);
}

/* ── background (baked once into the background layer) ───────────────────── */
static void bake_background(void) {
    gfx_t *bg = compositor_background();
    int W = bg->w, H = bg->h;
    gfx_set_clip(bg, R(0, 0, W, H));

    paint_t base = paint_diag3(RGB(0x07, 0x11, 0x1D), RGB(0x0C, 0x20, 0x34), 480, RGB(0x06, 0x10, 0x1B));
    gfx_fill_paint(bg, 0, 0, W, H, &base);

    /* two large soft light pools:  radial-gradient(circle at 20% 25% / 80% 75%, ..., transparent 30%) */
    int dx = W * 80 / 100, dy = H * 75 / 100;
    int pool = (int)(k_isqrt((uint32_t)(dx * dx + dy * dy)) * 3 / 10);
    gfx_radial_glow(bg, W * 20 / 100, H * 25 / 100, pool, pool, ARGB(56, 45, 120, 184));
    gfx_radial_glow(bg, W * 80 / 100, H * 75 / 100, pool, pool, ARGB(32, 0, 200, 255));

    /* the two orbs (opacity .6 already folded into the alpha) */
    gfx_orb(bg, 110, 300, 230, 41, 231, 296, ARGB(84, 96, 185, 255));
    gfx_orb(bg, W - 110, H - 110, 260, W - 136, H - 188, 310, ARGB(58, 60, 210, 255));

    /* frosted-glass tint over everything */
    gfx_fill_rect(bg, 0, 0, W, H, RGBA(3, 13, 23, 12));

    /* bottom hint text */
    const char *secure = "BornomalaOS  |  Secure Session";
    int sw = font_text_width(&font_ui12, secure) + 30 /* letter spacing */;
    gfx_text_ex(bg, &font_ui12, W / 2 - sw / 2, H - 39 + 4, secure, WHITE_A(50), 1);
}

/* ── avatar ───────────────────────────────────────────────────────────────── */
static int circle_cov(int px, int py, int cx, int cy, int r) {
    int dx2 = 2 * px + 1 - 2 * cx, dy2 = 2 * py + 1 - 2 * cy;
    int dist16 = (int)k_isqrt((uint32_t)(dx2 * dx2 + dy2 * dy2) * 64u);
    int c = r * 16 + 8 - dist16;
    if (c <= 0) return 0;
    if (c >= 16) return 255;
    return (c * 255) >> 4;
}

static void draw_avatar(gfx_t *g, int cx, int top) {
    int r = 46, cy = top + r;
    if (!gfx_visible(g, R(cx - r - 40, top - 10, 2 * r + 80, 2 * r + 60))) return;
    gfx_shadow(g, cx - r, top, 2 * r, 2 * r, r, 22, 34, 0, 12);
    paint_t p = paint_diag(RGB(0x8F, 0xC8, 0xFF), RGB(0x2D, 0x6F, 0xA7));
    gfx_fill_round_paint(g, cx - r, top, 2 * r, 2 * r, r, &p);

    /* silhouette: head + shoulders, both clipped to the avatar disc */
    rect_t a;
    if (rect_intersect(g->clip, R(cx - r, top, 2 * r, 2 * r), &a)) {
        for (int y = a.y; y < a.y + a.h; y++) {
            uint32_t *row = g->px + y * g->stride;
            for (int x = a.x; x < a.x + a.w; x++) {
                int head = circle_cov(x, y, cx, top + 32, 11);
                int body = circle_cov(x, y, cx, top + 75, 27);
                int cov = head > body ? head : body;
                if (!cov) continue;
                cov = cov * circle_cov(x, y, cx, cy, r) / 255;
                if (cov) row[x] = gfx_blend(row[x], color_with_alpha_scaled(RGB(0xDC, 0xEF, 0xFF), cov));
            }
        }
    }
    for (int t = 0; t < 3; t++)
        gfx_stroke_round_rect(g, cx - r + t, top + t, 2 * (r - t), 2 * (r - t), r - t, WHITE_A(72));
}

/* ── drawing ──────────────────────────────────────────────────────────────── */
void lock_draw(gfx_t *g) {
    int W = compositor_width(), H = compositor_height();
    int cx = W / 2;

    /* clock + date */
    if (gfx_visible(g, L.clock_area)) {
        int tw = clock_text_width(hm);
        gfx_text_ex(g, L.clock, cx - tw / 2, L.time_by, hm, RGB(255, 255, 255), L.clock_spacing);
        gfx_text_center(g, &font_ui24, cx, L.date_by, date_long, WHITE_A(87));
    }

    if (gfx_visible(g, L.login_area)) {
        draw_avatar(g, L.av_cx, L.av_top);
        gfx_text_center(g, &font_ui24, cx, L.name_by, display_name, RGB(255, 255, 255));
        gfx_text_center(g, &font_ui13, cx, L.hint_by, "Enter your password to unlock", WHITE_A(60));

        /* password field, always focused */
        rect_t f = L.field;
        for (int t = 3; t >= 1; t--) gfx_stroke_round_rect(g, f.x - t, f.y - t, f.w + 2 * t, f.h + 2 * t, 12 + t, RGBA(80, 180, 255, 14));
        gfx_fill_round_rect(g, f.x, f.y, f.w, f.h, 12, BLACK_A(24));
        gfx_stroke_round_rect(g, f.x, f.y, f.w, f.h, 12, RGBA(130, 205, 255, 79));
        if (pw_len == 0) {
            gfx_text(g, &font_ui13, f.x + 15, text_baseline_in(&font_ui13, f.y, f.h), "Password", WHITE_A(50));
        } else {
            int visible = (f.w - 30) / 14;
            int first = pw_len > visible ? pw_len - visible : 0;
            for (int i = first; i < pw_len; i++)
                gfx_fill_circle(g, f.x + 21 + (i - first) * 14, f.y + f.h / 2, 4, RGB(255, 255, 255));
        }

        /* unlock button */
        rect_t b = L.btn;
        paint_t bp = hover == HOVER_UNLOCK
            ? paint_diag(RGBA(120, 200, 255, 92), RGBA(50, 130, 190, 92))
            : paint_diag(RGBA(91, 180, 255, 82), RGBA(36, 106, 165, 82));
        gfx_fill_round_paint(g, b.x, b.y, b.w, b.h, 12, &bp);
        gfx_stroke_round_rect(g, b.x, b.y, b.w, b.h, 12, WHITE_A(20));
        sym_draw(g, SYM_ARROW_RIGHT, b.x + b.w / 2, b.y + b.h / 2, 16, RGB(255, 255, 255));

        if (show_error) gfx_text_center(g, &font_ui12, cx, L.err_by, "Incorrect password", RGB(0xFF, 0xD0, 0xD0));
    }

    /* bottom round buttons */
    rect_t bots[2] = { L.access, L.power };
    for (int i = 0; i < 2; i++) {
        rect_t r = bots[i];
        if (!gfx_visible(g, rect_inflate(r, 3))) continue;
        bool hv = (i == 0 && hover == HOVER_ACCESS) || (i == 1 && hover == HOVER_POWER) || (i == 1 && menu.open);
        gfx_fill_round_rect(g, r.x, r.y, r.w, r.h, 22, hv ? WHITE_A(18) : BLACK_A(20));
        gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 22, WHITE_A(18));
        sym_draw(g, i == 0 ? SYM_ACCESS : SYM_POWER, r.x + 22, r.y + 22, 18, RGB(255, 255, 255));
    }
    (void)H;

    popup_draw(g, &menu);
}

/* ── behaviour ────────────────────────────────────────────────────────────── */
void lock_enter(void) {
    const bm_user_t *u = users_current();
    k_strncpy(display_name, u ? u->display_name : "User", sizeof display_name - 1);
    display_name[sizeof display_name - 1] = '\0';

    compute_layout();
    memset(password, 0, sizeof password);
    pw_len = 0; show_error = false; hover = HOVER_NONE;
    popup_close(&menu);
    lockscreen_lock();

    bake_background();
    update_time(true);
    last_sec = timer_ticks() / 100;
    compositor_damage_all();
}

static void damage_login(void) { compositor_add_damage(L.login_area); }

static void try_unlock(void) {
    password[pw_len] = '\0';
    bool ok = lockscreen_try_unlock(password);
    memset(password, 0, sizeof password);        /* never keep the password around */
    pw_len = 0;
    if (ok) {
        show_error = false;
        ui_request(UI_DESKTOP);
        return;
    }
    show_error = true;
    damage_login();
}

static void menu_action(int id) {
    switch (id) {
    case MENU_RESTART:  ui_request(UI_RESTART); break;
    case MENU_SHUTDOWN: ui_request(UI_SHUTDOWN); break;
    case MENU_SLEEP:    ui_request(UI_SLEEP); break;
    default: break;
    }
}

void lock_key(uint16_t key, uint8_t mods) {
    if (mods & (MOD_CTRL | MOD_ALT)) return;
    if (menu.open) {
        if (key == KEY_ESC) { compositor_add_damage(popup_damage_rect(&menu)); popup_close(&menu); }
        return;
    }
    if (key == '\n') { try_unlock(); return; }
    if (key == KEY_ESC) {
        if (pw_len || show_error) { memset(password, 0, sizeof password); pw_len = 0; show_error = false; damage_login(); }
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (pw_len) password[--pw_len] = '\0';
        show_error = false;
        damage_login();
        return;
    }
    if (key >= 32 && key <= 126 && pw_len < PW_MAX) {
        password[pw_len++] = (char)key;
        password[pw_len] = '\0';
        show_error = false;
        damage_login();
    }
}

void lock_mouse(int x, int y, int buttons, int prev) {
    bool clicked = (buttons & MOUSE_LEFT) && !(prev & MOUSE_LEFT);

    if (menu.open) {
        if (popup_move(&menu, x, y)) compositor_add_damage(popup_damage_rect(&menu));
        if (clicked) {
            int id = popup_click(&menu, x, y);
            if (id == POPUP_CLICK_NONE) return;
            compositor_add_damage(popup_damage_rect(&menu));
            popup_close(&menu);
            if (id != POPUP_CLICK_OUTSIDE) menu_action(id);
            else if (rect_contains(L.power, x, y)) { /* clicking the power button again just closes the menu */ }
            compositor_add_damage(rect_inflate(L.power, 4));
        }
        return;
    }

    int nh = HOVER_NONE;
    if (rect_contains(L.btn, x, y)) nh = HOVER_UNLOCK;
    else if (rect_contains(L.power, x, y)) nh = HOVER_POWER;
    else if (rect_contains(L.access, x, y)) nh = HOVER_ACCESS;
    if (nh != hover) {
        hover = nh;
        compositor_add_damage(rect_inflate(L.btn, 2));
        compositor_add_damage(rect_inflate(L.power, 2));
        compositor_add_damage(rect_inflate(L.access, 2));
    }
    if (!clicked) return;

    if (nh == HOVER_UNLOCK) { try_unlock(); return; }
    if (nh == HOVER_POWER) {
        static const menu_item_t items[3] = {
            { "Restart", MENU_RESTART, false },
            { "Shut Down", MENU_SHUTDOWN, false },
            { "Sleep", MENU_SLEEP, false } };
        popup_open(&menu, L.power.x + L.power.w - 170, L.power.y - 8, true, items, 3, POPUP_DARK,
                   compositor_width(), compositor_height());
        compositor_add_damage(popup_damage_rect(&menu));
        compositor_add_damage(rect_inflate(L.power, 4));
    }
}
