#include "desktop.h"
#include "apps.h"
#include "compositor.h"
#include "icons.h"
#include "menu.h"
#include "session.h"
#include "wm.h"
#include "../console.h"
#include "../input.h"
#include "../interrupts.h"
#include "../klib.h"
#include "../lockscreen.h"
#include "../power.h"
#include "../rtc.h"
#include "../users.h"

#define TOPBAR_H     30
#define DOCK_H       67
#define DOCK_MARGIN  12
#define DOCK_TILE    52
#define DOCK_GAP     7

/* ════════════════════════════════════════════════════════════════════════════
   state
   ════════════════════════════════════════════════════════════════════════════ */
enum { ACT_LAUNCHPAD = 100, ACT_TRASH = 101, ACT_SEP = 102, ACT_NONE = 0 };
enum { POP_CTX_VIEW = 1, POP_CTX_SORT, POP_CTX_REFRESH, POP_CTX_PASTE, POP_CTX_PERSONALIZE,
       POP_SYS_ABOUT = 20, POP_SYS_LOCK, POP_SYS_RESTART, POP_SYS_SHUTDOWN };

typedef struct { icon_id_t icon; int action; const char *label; } dock_item_t;
static const dock_item_t dock_items[6] = {
    { ICON_LAUNCHPAD, ACT_LAUNCHPAD, "Launchpad" },
    { ICON_FILES,     APP_FILES,     "Files" },
    { ICON_NOTEPAD,   APP_NOTEPAD,   "Notepad" },
    { ICON_CALC,      APP_CALC,      "Calculator" },
    { ICON_COUNT,     ACT_SEP,       "" },
    { ICON_TRASH,     ACT_TRASH,     "Trash" },
};

typedef struct { const char *name; icon_id_t icon; int app; } app_entry_t;
static const app_entry_t launch_apps[14] = {
    { "Files", ICON_FILES, APP_FILES }, { "Notepad", ICON_NOTEPAD, APP_NOTEPAD },
    { "Calculator", ICON_CALC, APP_CALC }, { "Computer", ICON_COMPUTER, APP_COMPUTER },
    { "Browser", ICON_BROWSER, APP_NONE }, { "Paint", ICON_PAINT, APP_NONE },
    { "Media Player", ICON_MEDIA, APP_NONE }, { "Settings", ICON_SETTINGS, APP_NONE },
    { "Camera", ICON_CAMERA, APP_NONE }, { "Calendar", ICON_CALENDAR, APP_NONE },
    { "Code", ICON_CODE, APP_NONE }, { "Dashboard", ICON_DASHBOARD, APP_NONE },
    { "Mail", ICON_MAIL, APP_NONE }, { "Notes", ICON_NOTES, APP_NONE },
};

typedef struct { const char *label; icon_id_t icon; int app; } desk_icon_t;
static desk_icon_t desk_icons[5] = {
    { "Computer",  ICON_COMPUTER, APP_COMPUTER },
    { "Files",     ICON_FILES,    APP_FILES },
    { "Notepad",   ICON_NOTEPAD,  APP_NOTEPAD },
    { "Calculator", ICON_CALC,    APP_CALC },
    { "Internet",  ICON_BROWSER,  APP_NONE },
};

static bool      wm_ready;
static popup_t   popup;
static bool      launchpad;
static char      search[32];
static int       search_len;
static int       lp_hover = -1;
static int       dock_hover = -1;
static int       icon_hover = -1, icon_selected = -1;
static int       topbar_hover = -1;
static uint64_t  last_icon_click; static int last_icon_click_idx = -1;
static char      toast_text[80];
static uint64_t  toast_until;
static char      clock_hm[8], date_short[16];
static uint64_t  last_sec = (uint64_t)-1;
static char      files_label[48];

static int SW(void) { return compositor_width(); }
static int SH(void) { return compositor_height(); }

/* ════════════════════════════════════════════════════════════════════════════
   toast
   ════════════════════════════════════════════════════════════════════════════ */
static rect_t toast_rect(void) {
    int w = font_text_width(&font_ui12, toast_text) + 24;
    return R(SW() - 15 - w, SH() - 50 - 32, w, 32);
}

static void toast(const char *msg) {
    if (toast_until) compositor_add_damage(rect_inflate(toast_rect(), 4));
    k_strncpy(toast_text, msg, sizeof toast_text - 1);
    toast_text[sizeof toast_text - 1] = '\0';
    toast_until = timer_ticks() + 180;
    compositor_add_damage(rect_inflate(toast_rect(), 4));
}

/* ════════════════════════════════════════════════════════════════════════════
   geometry
   ════════════════════════════════════════════════════════════════════════════ */
static rect_t dock_rect(void) {
    int w = 2 + 20 + 5 * DOCK_TILE + 7 + 5 * DOCK_GAP;
    return R((SW() - w) / 2, SH() - DOCK_MARGIN - DOCK_H, w, DOCK_H);
}

/* untransformed tile rectangle of dock item i */
static rect_t dock_tile_rect(int i) {
    rect_t d = dock_rect();
    int x = d.x + 1 + 10;
    for (int k = 0; k < i; k++) {
        x += (dock_items[k].action == ACT_SEP ? 7 : DOCK_TILE) + DOCK_GAP;
    }
    if (dock_items[i].action == ACT_SEP) return R(x + 3, d.y + 12, 1, 43);
    return R(x, d.y + (DOCK_H - DOCK_TILE) / 2, DOCK_TILE, DOCK_TILE);
}

static rect_t dock_damage_rect(void) {
    rect_t d = dock_rect();
    return R(d.x - 20, d.y - 46, d.w + 40, d.h + 60);
}

static int dock_index_at(int x, int y) {
    rect_t d = dock_rect();
    if (!rect_contains(rect_inflate(d, 2), x, y) && !(dock_hover >= 0 && rect_contains(rect_inflate(dock_tile_rect(dock_hover), 8), x, y)))
        return -1;
    for (int i = 0; i < 6; i++) {
        if (dock_items[i].action == ACT_SEP) continue;
        rect_t t = dock_tile_rect(i);
        if (x >= t.x - 3 && x < t.x + t.w + 3 && y >= d.y && y < d.y + d.h) return i;
    }
    return -1;
}

static rect_t desk_icon_rect(int i) { return R(12, 42 + i * 98, 78, 88); }

/* top bar item rectangles (left side) */
static const char *const topbar_labels[6] = { "BornomalaOS", "File", "Edit", "View", "Window", "Help" };
static rect_t topbar_item_rect(int i) {
    int x = 12;
    for (int k = 0; k < i; k++) x += font_text_width(k == 0 ? &font_ui13b : &font_ui13, topbar_labels[k]) + 17;
    int w = font_text_width(i == 0 ? &font_ui13b : &font_ui13, topbar_labels[i]);
    return R(x - 8, 0, w + 16, TOPBAR_H);
}

static rect_t clock_area(void) { return R(SW() - 320, 0, 320, TOPBAR_H); }

/* ── launchpad geometry ───────────────────────────────────────────────────── */
typedef struct { int cols, pitch_x, pitch_y, x0, y0; } lp_layout_t;

static lp_layout_t lp_layout(void) {
    lp_layout_t l;
    int W = SW(), H = SH();
    int grid_w = W - 80 < 900 ? W - 80 : 900;
    l.cols = W >= 760 ? 6 : 4;
    l.pitch_x = grid_w / l.cols;
    l.x0 = (W - grid_w) / 2;
    l.pitch_y = H >= 700 ? 132 : 118;
    l.y0 = 187;
    return l;
}

static bool lp_matches(const app_entry_t *a) {
    if (!search_len) return true;
    int n = (int)k_strlen(a->name);
    for (int i = 0; i + search_len <= n; i++) {
        bool ok = true;
        for (int j = 0; j < search_len && ok; j++)
            if (k_tolower(a->name[i + j]) != k_tolower(search[j])) ok = false;
        if (ok) return true;
    }
    return false;
}

/* i-th VISIBLE cell -> rect */
static rect_t lp_cell_rect(int visible_index) {
    lp_layout_t l = lp_layout();
    int c = visible_index % l.cols, r = visible_index / l.cols;
    return R(l.x0 + c * l.pitch_x + (l.pitch_x - 110) / 2, l.y0 + r * l.pitch_y, 110, 118);
}

static int lp_visible_to_app(int visible_index) {
    int seen = 0;
    for (int i = 0; i < 14; i++) if (lp_matches(&launch_apps[i])) { if (seen == visible_index) return i; seen++; }
    return -1;
}

static int lp_visible_count(void) {
    int n = 0;
    for (int i = 0; i < 14; i++) if (lp_matches(&launch_apps[i])) n++;
    return n;
}

static rect_t lp_close_rect(void) { return R(SW() - SW() * 7 / 100 - 32, 62, 32, 32); }
static rect_t lp_search_rect(void) { return R((SW() - 340) / 2, 123, 340, 34); }
static rect_t lp_area(void) { return R(0, TOPBAR_H, SW(), SH() - TOPBAR_H); }

/* ════════════════════════════════════════════════════════════════════════════
   wallpaper (baked once per desktop_enter)
   ════════════════════════════════════════════════════════════════════════════ */
static void bake_wallpaper(void) {
    gfx_t *bg = compositor_background();
    int W = bg->w, H = bg->h;
    gfx_set_clip(bg, R(0, 0, W, H));

    paint_t base = paint_v3(RGB(0x0A, 0x4F, 0x78), RGB(0x08, 0x71, 0x9B), 480, RGB(0x06, 0x3E, 0x65));
    gfx_fill_paint(bg, 0, 0, W, H, &base);

    /* .desktop: radial-gradient(ellipse at 50% 38%, rgba(255,255,255,.22), transparent 30%) */
    gfx_radial_glow(bg, W / 2, H * 38 / 100, W * 212 / 1000, H * 263 / 1000, WHITE_A(22));

    /* .desktop:before, two soft rings */
    gfx_radial_band(bg, W / 2, H * 60 / 100, W * 707 / 1000, H * 849 / 1000, WHITE_A(12), 190, 200, 330);
    gfx_radial_band(bg, W / 2, H * 76 / 100, W * 707 / 1000, H * 1075 / 1000, WHITE_A(10), 310, 320, 460);

    /* .glow: 600x260 white ellipse behind the upper window area */
    gfx_radial_glow(bg, W / 2, H * 33 / 100, 288, 125, WHITE_A(25));
}

/* ════════════════════════════════════════════════════════════════════════════
   drawing: desktop icons
   ════════════════════════════════════════════════════════════════════════════ */
static void draw_desktop_icons(gfx_t *g) {
    for (int i = 0; i < 5; i++) {
        rect_t r = desk_icon_rect(i);
        if (!gfx_visible(g, r)) continue;
        if (i == icon_hover || i == icon_selected) {
            gfx_fill_round_rect(g, r.x, r.y, r.w, r.h, 3, RGBA(120, 190, 240, 25));
            gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 3, WHITE_A(35));
        }
        icon_draw(g, desk_icons[i].icon, r.x + (r.w - 42) / 2, r.y + 4, 42);

        const char *label = desk_icons[i].label;
        /* wrap onto two lines when wider than the tile */
        char l1[48], l2[48] = "";
        k_strncpy(l1, label, sizeof l1 - 1);
        l1[sizeof l1 - 1] = '\0';
        int maxw = r.w - 8;
        if (font_text_width(&font_ui12, l1) > maxw) {
            int sp = -1;
            for (int k = 0; l1[k]; k++) if (l1[k] == ' ' || l1[k] == '\'') { if (font_text_width_n(&font_ui12, l1, k + 1) <= maxw) sp = k; }
            if (sp >= 0) {
                int cut = (l1[sp] == ' ') ? sp : sp + 1;
                k_strcpy(l2, l1 + cut + (l1[cut] == ' ' ? 1 : 0));
                l1[cut] = '\0';
            }
        }
        uint32_t sh = BLACK_A(85), fg = RGB(255, 255, 255);
        gfx_text_shadow(g, &font_ui12, r.x + (r.w - font_text_width(&font_ui12, l1)) / 2, r.y + 64, l1, fg, sh, 1, 1);
        if (l2[0]) gfx_text_shadow(g, &font_ui12, r.x + (r.w - font_text_width(&font_ui12, l2)) / 2, r.y + 78, l2, fg, sh, 1, 1);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   drawing: top bar
   ════════════════════════════════════════════════════════════════════════════ */
static void draw_topbar(gfx_t *g) {
    rect_t bar = R(0, 0, SW(), TOPBAR_H);
    if (!gfx_visible(g, R(0, 0, SW(), TOPBAR_H + 4))) return;

    /* soft drop shadow first, then the glass strip */
    for (int i = 0; i < 4; i++) gfx_hline(g, 0, TOPBAR_H + i, SW(), BLACK_A(14 - i * 4));
    paint_t p = paint_v2(WHITE_A(72), RGBA(220, 230, 238, 48));
    gfx_fill_paint(g, bar.x, bar.y, bar.w, bar.h, &p);
    gfx_hline(g, 0, TOPBAR_H - 1, SW(), RGBA(40, 70, 90, 45));

    uint32_t ink = RGB(0x15, 0x25, 0x32);
    for (int i = 0; i < 6; i++) {
        rect_t r = topbar_item_rect(i);
        if (i == topbar_hover || (i == 0 && popup.open && popup.rect.y <= TOPBAR_H + 2 && popup.rect.x < 200))
            gfx_fill_round_rect(g, r.x, 3, r.w, TOPBAR_H - 6, 4, RGBA(255, 255, 255, 45));
        const font_t *f = i == 0 ? &font_ui13b : &font_ui13;
        gfx_text(g, f, r.x + 8, text_baseline_in(f, 0, TOPBAR_H), topbar_labels[i], ink);
    }

    /* right side: volume, wifi, battery, date, time */
    int x = SW() - 12;
    int tw = font_text_width(&font_ui12, clock_hm);
    gfx_text(g, &font_ui12, x - tw, text_baseline_in(&font_ui12, 0, TOPBAR_H), clock_hm, ink);
    x -= tw + 12;
    tw = font_text_width(&font_ui12, date_short);
    gfx_text(g, &font_ui12, x - tw, text_baseline_in(&font_ui12, 0, TOPBAR_H), date_short, ink);
    x -= tw + 14;
    sym_battery_level(g, x - 22, 10, 20, 10, 82, ink);
    x -= 22 + 14;
    sym_draw(g, SYM_WIFI, x - 6, 15, 12, ink);
    x -= 12 + 12;
    sym_draw(g, SYM_VOLUME, x - 7, 15, 12, ink);
}

/* ════════════════════════════════════════════════════════════════════════════
   drawing: dock
   ════════════════════════════════════════════════════════════════════════════ */
static void draw_dock(gfx_t *g) {
    rect_t d = dock_rect();
    if (!gfx_visible(g, dock_damage_rect())) return;

    gfx_shadow(g, d.x, d.y, d.w, d.h, 18, 16, 45, 0, 5);
    paint_t p = paint_v2(WHITE_A(58), RGBA(215, 225, 235, 34));
    gfx_fill_round_paint(g, d.x, d.y, d.w, d.h, 18, &p);
    gfx_stroke_round_rect(g, d.x, d.y, d.w, d.h, 18, WHITE_A(80));
    gfx_hline(g, d.x + 18, d.y + 1, d.w - 36, WHITE_A(70));

    for (int i = 0; i < 6; i++) {
        rect_t t = dock_tile_rect(i);
        if (dock_items[i].action == ACT_SEP) {
            gfx_vline(g, t.x, t.y, t.h, WHITE_A(60));
            gfx_vline(g, t.x + 1, t.y, t.h, BLACK_A(20));
            continue;
        }
        bool hv = (i == dock_hover);
        int size = hv ? 59 : DOCK_TILE;
        int cx = t.x + t.w / 2, cy = t.y + t.h / 2 - (hv ? 8 : 0);
        icon_draw_tile(g, dock_items[i].icon, cx - size / 2, cy - size / 2, size, 0);
        if (dock_items[i].action < APP_COUNT && dock_items[i].action != 0 && wm_app_running(dock_items[i].action))
            gfx_fill_circle(g, cx, d.y + d.h - 4, 2, RGBA(20, 40, 60, 80));
    }

    if (dock_hover >= 0) {                                   /* tooltip */
        rect_t t = dock_tile_rect(dock_hover);
        const char *label = dock_items[dock_hover].label;
        int w = font_text_width(&font_ui12, label) + 20;
        int x = t.x + t.w / 2 - w / 2, y = d.y - 44;
        gfx_shadow(g, x, y, w, 24, 6, 6, 30, 0, 2);
        gfx_fill_round_rect(g, x, y, w, 24, 6, RGBA(20, 40, 55, 92));
        gfx_stroke_round_rect(g, x, y, w, 24, 6, RGBA(145, 200, 232, 60));
        gfx_text(g, &font_ui12, x + 10, text_baseline_in(&font_ui12, y, 24), label, RGB(255, 255, 255));
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   drawing: launchpad
   ════════════════════════════════════════════════════════════════════════════ */
static void draw_launchpad(gfx_t *g) {
    rect_t area = lp_area();
    if (!gfx_visible(g, area)) return;
    int W = SW();

    paint_t bgp = paint_diag(RGBA(18, 48, 73, 91), RGBA(4, 20, 35, 96));
    gfx_fill_paint(g, area.x, area.y, area.w, area.h, &bgp);
    int gr = (int)(k_isqrt((uint32_t)((W / 2) * (W / 2) + (area.h * 88 / 100) * (area.h * 88 / 100))) * 28 / 100);
    gfx_radial_glow(g, W / 2, area.y + area.h * 12 / 100, gr, gr, ARGB(51, 110, 190, 255));

    gfx_text_shadow(g, &font_ui24, W / 2 - font_text_width(&font_ui24, "Applications") / 2, 98, "Applications",
                    RGB(255, 255, 255), BLACK_A(70), 0, 2);

    rect_t sr = lp_search_rect();
    gfx_fill_round_rect(g, sr.x, sr.y, sr.w, sr.h, 17, WHITE_A(9));
    gfx_stroke_round_rect(g, sr.x, sr.y, sr.w, sr.h, 17, WHITE_A(22));
    sym_draw(g, SYM_SEARCH, sr.x + 20, sr.y + 17, 14, WHITE_A(75));
    if (search_len) gfx_text(g, &font_ui13, sr.x + 36, text_baseline_in(&font_ui13, sr.y, sr.h), search, RGB(255, 255, 255));
    else            gfx_text(g, &font_ui13, sr.x + 36, text_baseline_in(&font_ui13, sr.y, sr.h), "Search", WHITE_A(67));

    rect_t cr = lp_close_rect();
    gfx_fill_round_rect(g, cr.x, cr.y, cr.w, cr.h, 16, WHITE_A(9));
    gfx_stroke_round_rect(g, cr.x, cr.y, cr.w, cr.h, 16, WHITE_A(21));
    sym_draw(g, SYM_CLOSE, cr.x + 16, cr.y + 16, 7, RGB(255, 255, 255));

    int vis = 0;
    for (int i = 0; i < 14; i++) {
        if (!lp_matches(&launch_apps[i])) continue;
        rect_t c = lp_cell_rect(vis);
        bool hv = (vis == lp_hover);
        vis++;
        if (!gfx_visible(g, rect_inflate(c, 4))) continue;
        int lift = hv ? 3 : 0;
        if (hv) gfx_fill_round_rect(g, c.x, c.y - lift, c.w, c.h, 15, WHITE_A(9));
        int style = ((i + 1) % 3 == 0) ? 3 : (((i + 1) % 2 == 0) ? 2 : 1);
        icon_draw_tile(g, launch_apps[i].icon, c.x + (c.w - 68) / 2, c.y + 10 - lift, 68, style);
        const char *n = launch_apps[i].name;
        int nw = font_text_width(&font_ui12, n);
        gfx_text_shadow(g, &font_ui12, c.x + (c.w - nw) / 2, c.y + 100 - lift, n, RGB(255, 255, 255), BLACK_A(80), 0, 1);
    }
    if (vis == 0) gfx_text_center(g, &font_ui13, W / 2, 230, "No applications match your search", WHITE_A(70));
}

/* ════════════════════════════════════════════════════════════════════════════
   drawing: toast
   ════════════════════════════════════════════════════════════════════════════ */
static void draw_toast(gfx_t *g) {
    if (!toast_until) return;
    rect_t r = toast_rect();
    if (!gfx_visible(g, rect_inflate(r, 2))) return;
    gfx_fill_round_rect(g, r.x, r.y, r.w, r.h, 4, RGBA(20, 40, 55, 90));
    gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 4, RGB(0x91, 0xC8, 0xE8));
    gfx_text(g, &font_ui12, r.x + 12, text_baseline_in(&font_ui12, r.y, r.h), toast_text, RGB(255, 255, 255));
}

void desktop_draw(gfx_t *g) {
    draw_desktop_icons(g);
    wm_draw(g);
    if (launchpad) draw_launchpad(g);
    draw_topbar(g);
    draw_dock(g);
    popup_draw(g, &popup);
    draw_toast(g);
}

/* ════════════════════════════════════════════════════════════════════════════
   behaviour
   ════════════════════════════════════════════════════════════════════════════ */
static void update_clock(bool force) {
    rtc_time_t t;
    rtc_read(&t);
    char nhm[8], nd[16];
    rtc_format_hm(&t, nhm);
    rtc_format_date_short(&t, nd);
    if (force || k_strcmp(nhm, clock_hm) || k_strcmp(nd, date_short)) {
        k_strcpy(clock_hm, nhm);
        k_strcpy(date_short, nd);
        compositor_add_damage(clock_area());
    }
}

void desktop_enter(void) {
    int W = SW(), H = SH();
    if (!wm_ready) { wm_init(R(0, TOPBAR_H, W, H - TOPBAR_H - 90)); wm_ready = true; }
    const bm_user_t *u = users_current();
    ksnprintf(files_label, sizeof files_label, "%s's Files", u ? u->display_name : "User");
    desk_icons[1].label = files_label;
    launchpad = false; search_len = 0; search[0] = '\0';
    popup_close(&popup);
    dock_hover = icon_hover = topbar_hover = lp_hover = -1;
    toast_until = 0;
    bake_wallpaper();
    update_clock(true);
    last_sec = timer_ticks() / 100;
    compositor_damage_all();
}

static void close_popup(void) {
    if (!popup.open) return;
    compositor_add_damage(popup_damage_rect(&popup));
    popup_close(&popup);
    compositor_add_damage(R(0, 0, SW(), TOPBAR_H));
}

static void set_launchpad(bool open) {
    if (launchpad == open) return;
    launchpad = open;
    search_len = 0; search[0] = '\0'; lp_hover = -1;
    compositor_damage_all();
}

static void launch(int app, const char *name) {
    const bm_user_t *u = users_current();
    if (app != APP_NONE) apps_launch((app_id_t)app, SW(), SH(), u ? u->display_name : "User");
    else {
        char msg[80];
        ksnprintf(msg, sizeof msg, "%s is not installed yet", name);
        toast(msg);
    }
}

static void open_system_menu(void) {
    static const menu_item_t items[5] = {
        { "About BornomalaOS", POP_SYS_ABOUT, false },
        { 0, 0, true },
        { "Lock Screen", POP_SYS_LOCK, false },
        { "Restart", POP_SYS_RESTART, false },
        { "Shut Down", POP_SYS_SHUTDOWN, false } };
    rect_t r = topbar_item_rect(0);
    popup_open(&popup, r.x, TOPBAR_H - 1, false, items, 5, POPUP_LIGHT, SW(), SH());
    compositor_add_damage(popup_damage_rect(&popup));
    compositor_add_damage(R(0, 0, SW(), TOPBAR_H));
}

static void open_context_menu(int x, int y) {
    static const menu_item_t items[6] = {
        { "View", POP_CTX_VIEW, false }, { "Sort by", POP_CTX_SORT, false },
        { "Refresh", POP_CTX_REFRESH, false }, { 0, 0, true },
        { "Paste", POP_CTX_PASTE, false }, { "Personalize", POP_CTX_PERSONALIZE, false } };
    popup_open(&popup, x, y, false, items, 6, POPUP_LIGHT, SW(), SH());
    compositor_add_damage(popup_damage_rect(&popup));
}

static void popup_action(int id) {
    switch (id) {
    case POP_CTX_VIEW:         toast("View options opened"); break;
    case POP_CTX_SORT:         toast("Sort By options opened"); break;
    case POP_CTX_REFRESH:      compositor_damage_all(); break;
    case POP_CTX_PASTE:        toast("Paste is unavailable right now"); break;
    case POP_CTX_PERSONALIZE:  toast("Desktop personalization"); break;
    case POP_SYS_ABOUT:        toast("BornomalaOS - Phase 5 (Visual GUI foundation)"); break;
    case POP_SYS_LOCK:         session_switch(SCENE_LOCK); break;
    case POP_SYS_RESTART:      power_restart();
    case POP_SYS_SHUTDOWN:     power_shutdown();
    default: break;
    }
}

void desktop_tick(uint64_t ticks) {
    uint64_t sec = ticks / 100;
    if (sec != last_sec) { last_sec = sec; update_clock(false); }
    if (toast_until && ticks >= toast_until) {
        compositor_add_damage(rect_inflate(toast_rect(), 4));
        toast_until = 0;
    }
    apps_tick(ticks);
}

void desktop_key(uint16_t key, uint8_t mods) {
    if (popup.open) { if (key == KEY_ESC) close_popup(); return; }

    if (key == KEY_SUPER || key == KEY_F1 + 3) { set_launchpad(!launchpad); return; }   /* Win key or F4 */
    if ((mods & MOD_CTRL) && (key == 'l' || key == 'L')) { session_switch(SCENE_LOCK); return; }

    if (launchpad) {
        if (key == KEY_ESC) { if (search_len) { search_len = 0; search[0] = 0; compositor_damage_all(); } else set_launchpad(false); return; }
        if (key == KEY_BACKSPACE) { if (search_len) { search[--search_len] = 0; lp_hover = -1; compositor_add_damage(lp_area()); } return; }
        if (key == '\n') {
            int a = lp_visible_to_app(0);
            if (a >= 0) { set_launchpad(false); launch(launch_apps[a].app, launch_apps[a].name); }
            return;
        }
        if (key >= 32 && key <= 126 && search_len < (int)sizeof(search) - 1) {
            search[search_len++] = (char)key; search[search_len] = 0;
            lp_hover = -1;
            compositor_add_damage(lp_area());
        }
        return;
    }
    wm_key(key, mods);
}

static void dock_activate(int i) {
    int a = dock_items[i].action;
    if (a == ACT_LAUNCHPAD) { set_launchpad(!launchpad); return; }
    if (a == ACT_TRASH)     { toast("Trash is empty"); return; }
    if (launchpad) set_launchpad(false);
    launch(a, dock_items[i].label);
}

static int lp_cell_at(int x, int y) {
    int n = lp_visible_count();
    for (int v = 0; v < n; v++) if (rect_contains(lp_cell_rect(v), x, y)) return v;
    return -1;
}

static int desk_icon_at(int x, int y) {
    for (int i = 0; i < 5; i++) if (rect_contains(desk_icon_rect(i), x, y)) return i;
    return -1;
}

void desktop_mouse(int x, int y, int buttons, int prev) {
    bool left_down  = (buttons & MOUSE_LEFT)  && !(prev & MOUSE_LEFT);
    bool right_down = (buttons & MOUSE_RIGHT) && !(prev & MOUSE_RIGHT);
    uint64_t now = timer_ticks();

    /* A window drag / pressed title-bar button must see every event until it finishes. */
    if (wm_captured()) { wm_mouse(x, y, buttons, prev, now); return; }

    /* 1) an open popup menu owns the mouse */
    if (popup.open) {
        if (popup_move(&popup, x, y)) compositor_add_damage(popup_damage_rect(&popup));
        if (left_down || right_down) {
            int id = popup_click(&popup, x, y);
            if (id == POPUP_CLICK_NONE) return;
            close_popup();
            if (id != POPUP_CLICK_OUTSIDE) popup_action(id);
        }
        return;
    }

    /* 2) dock (floats above everything else) */
    int di = dock_index_at(x, y);
    if (di != dock_hover) {
        compositor_add_damage(dock_damage_rect());
        dock_hover = di;
    }
    if (di >= 0) {
        if (left_down) dock_activate(di);
        return;
    }

    /* 3) top bar */
    int th = -1;
    if (y < TOPBAR_H) for (int i = 0; i < 6; i++) if (rect_contains(topbar_item_rect(i), x, y)) th = i;
    if (th != topbar_hover) { topbar_hover = th; compositor_add_damage(R(0, 0, SW(), TOPBAR_H)); }
    if (y < TOPBAR_H) {
        if (left_down && th == 0) open_system_menu();
        return;
    }

    /* 4) launchpad overlay */
    if (launchpad) {
        int hv = lp_cell_at(x, y);
        if (hv != lp_hover) {
            if (lp_hover >= 0) compositor_add_damage(rect_inflate(lp_cell_rect(lp_hover), 6));
            if (hv >= 0) compositor_add_damage(rect_inflate(lp_cell_rect(hv), 6));
            lp_hover = hv;
        }
        if (left_down) {
            if (rect_contains(lp_close_rect(), x, y)) { set_launchpad(false); return; }
            if (hv >= 0) {
                int a = lp_visible_to_app(hv);
                if (a >= 0) { set_launchpad(false); launch(launch_apps[a].app, launch_apps[a].name); }
                return;
            }
            if (!rect_contains(lp_search_rect(), x, y)) set_launchpad(false);   /* click on empty space */
        }
        return;
    }

    /* 5) windows */
    if (wm_mouse(x, y, buttons, prev, now)) {
        if (icon_hover >= 0) { icon_hover = -1; compositor_add_damage(R(0, TOPBAR_H, 100, SH())); }
        return;
    }

    /* 6) desktop icons and the bare desktop */
    int ih = desk_icon_at(x, y);
    if (ih != icon_hover) { icon_hover = ih; compositor_add_damage(R(0, TOPBAR_H, 100, SH())); }
    if (left_down) {
        if (ih != icon_selected) { icon_selected = ih; compositor_add_damage(R(0, TOPBAR_H, 100, SH())); }
        if (ih >= 0) {
            if (ih == last_icon_click_idx && now - last_icon_click < 40) {
                last_icon_click_idx = -1;
                launch(desk_icons[ih].app, desk_icons[ih].label);
            } else {
                last_icon_click_idx = ih; last_icon_click = now;
            }
        } else {
            last_icon_click_idx = -1;
        }
    }
    if (right_down) open_context_menu(x, y);
}
