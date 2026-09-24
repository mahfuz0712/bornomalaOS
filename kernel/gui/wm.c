<<<<<<< HEAD
#include "wm.h"
#include "compositor.h"
#include "../klib.h"
#include "../memory.h"

#define CTRL_W 27
#define CTRL_H 20

enum { EDGE_L = 1, EDGE_R = 2, EDGE_T = 4, EDGE_B = 8 };

static window_t  wins[WM_MAX_WINDOWS];
static window_t *zorder[WM_MAX_WINDOWS];          /* back .. front */
static int       zcount;
static window_t *active;
static rect_t    work;
static uint32_t  next_id = 1;

static struct { bool on; window_t *w; int off_x, off_y; } drag;
static struct { bool on; window_t *w; int edges; rect_t start; int sx, sy; } rsz;
static struct { window_t *w; int which; } press;          /* which: 0 min, 1 max, 2 close */
static struct { window_t *w; uint64_t tick; } last_title_click;

/* ═══════════════════════════ geometry ═══════════════════════════ */
rect_t window_client_size(const window_t *w) { return R(0, 0, w->rect.w - 2, w->rect.h - 2 - WM_TITLE_H); }
rect_t window_client_screen(const window_t *w) {
    return R(w->rect.x + 1, w->rect.y + 1 + WM_TITLE_H, w->rect.w - 2, w->rect.h - 2 - WM_TITLE_H);
}
static rect_t title_rect(const window_t *w) { return R(w->rect.x + 1, w->rect.y + 1, w->rect.w - 2, WM_TITLE_H); }

static int first_ctrl(const window_t *w) { return w->no_minmax ? 2 : 0; }

/* index 0 = minimize, 1 = maximize, 2 = close (only 2 exists for dialogs) */
static rect_t ctrl_rect(const window_t *w, int which) {
    int slot = 2 - which;                                  /* slots counted from the right edge */
    int x = w->rect.x + w->rect.w - 1 - 6 - CTRL_W - slot * (CTRL_W + 2);
    return R(x, w->rect.y + 1 + (WM_TITLE_H - CTRL_H) / 2, CTRL_W, CTRL_H);
}

void window_damage(const window_t *w) { compositor_add_damage(rect_inflate(w->rect, 26)); }

/* ═══════════════════════════ surface ═══════════════════════════ */
static bool surface_alloc(window_t *w) {
    int cw = w->rect.w - 2, ch = w->rect.h - 2 - WM_TITLE_H;
    if (cw < 1 || ch < 1) return false;
    size_t pages = ((size_t)cw * (size_t)ch * 4u + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t *px = (uint32_t *)pmm_alloc_contiguous(pages);
    if (!px) return false;
    if (w->surface_px) pmm_free_contiguous(w->surface_px, w->surface_pages);
    w->surface_px = px;
    w->surface_pages = pages;
    gfx_init(&w->surface, px, cw, ch, cw);
    w->dirty = true;
    return true;
}

void window_invalidate(window_t *w) {
    if (!w) return;
    w->dirty = true;
    compositor_add_damage(window_client_screen(w));
}

/* ═══════════════════════════ lifecycle ═══════════════════════════ */
void wm_init(rect_t work_area) {
    memset(wins, 0, sizeof(wins));
    memset(zorder, 0, sizeof(zorder));
    memset(&drag, 0, sizeof drag); memset(&rsz, 0, sizeof rsz);
    press.w = 0; press.which = -1; last_title_click.w = 0;
    zcount = 0; active = 0; work = work_area;
}

static void z_remove(window_t *w) {
    int idx = -1;
    for (int i = 0; i < zcount; i++) if (zorder[i] == w) { idx = i; break; }
    if (idx < 0) return;
    for (int i = idx; i < zcount - 1; i++) zorder[i] = zorder[i + 1];
    zcount--;
}

static void z_push_top(window_t *w) { z_remove(w); zorder[zcount++] = w; }

static window_t *top_modal(void) {
    for (int i = zcount - 1; i >= 0; i--)
        if (zorder[i]->modal && zorder[i]->visible && !zorder[i]->minimized) return zorder[i];
    return 0;
}
bool wm_has_modal(void) { return top_modal() != 0; }

window_t *window_create(int owner, const char *title, icon_id_t icon, rect_t rect,
                        int min_w, int min_h, unsigned flags, window_t *parent) {
    window_t *w = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) if (!wins[i].used) { w = &wins[i]; break; }
    if (!w) return 0;
    memset(w, 0, sizeof(*w));
    w->used = true;
    w->id = next_id++;
    w->owner = owner;
    k_strncpy(w->title, title, sizeof(w->title) - 1);
    w->icon = icon;
    w->rect = w->restore = rect;
    w->min_w = min_w; w->min_h = min_h; w->max_w = 0; w->max_h = 0;
    w->visible = true;
    w->movable   = (flags & WINDOW_MOVABLE) != 0;
    w->resizable = (flags & WINDOW_RESIZABLE) != 0;
    w->modal     = (flags & WINDOW_MODAL) != 0;
    w->no_minmax = (flags & WINDOW_NO_MINMAX) != 0;
    w->parent    = parent;
    if (!surface_alloc(w)) { w->used = false; return 0; }
    zorder[zcount++] = w;
    window_focus(w);
    return w;
}

void window_destroy(window_t *w) {
    if (!w || !w->used) return;
    window_damage(w);
    z_remove(w);
    if (drag.w == w) drag.on = false;
    if (rsz.w == w) rsz.on = false;
    if (press.w == w) { press.w = 0; press.which = -1; }
    if (last_title_click.w == w) last_title_click.w = 0;
    if (w->surface_px) pmm_free_contiguous(w->surface_px, w->surface_pages);
    bool was_active = (active == w);
    memset(w, 0, sizeof(*w));
    if (was_active) {
        active = 0;
        for (int i = zcount - 1; i >= 0; i--)
            if (zorder[i]->visible && !zorder[i]->minimized) { active = zorder[i]; active->focused = true; window_damage(active); break; }
    }
}

void window_close(window_t *w) {
    if (!w || !w->used) return;
    for (int i = zcount - 1; i >= 0; i--)                   /* children first */
        if (i < zcount && zorder[i]->parent == w) window_close(zorder[i]);
    if (w->on_close) w->on_close(w);
    window_destroy(w);
}

void wm_reset(void) {
    while (zcount > 0) window_close(zorder[zcount - 1]);
    compositor_damage_all();
}

/* ═══════════════════════════ state changes ═══════════════════════════ */
static void raise_children(window_t *w) {
    for (int i = 0; i < zcount; i++) {
        window_t *c = zorder[i];
        if (c->parent == w) { z_push_top(c); raise_children(c); i = -1; }
    }
}

void window_focus(window_t *w) {
    if (!w || !w->used) return;
    window_t *m = top_modal();
    if (m && m != w && w->parent != m) w = m;               /* a modal dialog keeps the focus */
    window_t *prev = active;
    w->minimized = false;
    w->visible = true;
    z_push_top(w);
    raise_children(w);
    if (prev && prev != w) { prev->focused = false; window_damage(prev); }
    active = w; w->focused = true;
    window_damage(w);
    for (int i = 0; i < zcount; i++)
        if (zorder[i]->parent == w) { zorder[i]->minimized = false; window_damage(zorder[i]); }
}

void window_show(window_t *w) { if (!w) return; w->visible = true; window_focus(w); }

static void deactivate_if_active(window_t *w) {
    if (active != w) return;
    w->focused = false;
    active = 0;
    for (int i = zcount - 1; i >= 0; i--)
        if (zorder[i] != w && zorder[i]->visible && !zorder[i]->minimized) { active = zorder[i]; active->focused = true; window_damage(active); break; }
}

void window_hide(window_t *w) {
    if (!w) return;
    window_damage(w);
    w->visible = false;
    deactivate_if_active(w);
}

void window_minimize(window_t *w) {
    if (!w || w->no_minmax) return;
    window_damage(w);
    w->minimized = true;
    for (int i = 0; i < zcount; i++) if (zorder[i]->parent == w) { zorder[i]->minimized = true; window_damage(zorder[i]); }
    deactivate_if_active(w);
}

static void apply_rect(window_t *w, rect_t r) {
    window_damage(w);
    w->rect = r;
    if (surface_alloc(w) && w->on_resize) w->on_resize(w);
    w->dirty = true;
    window_damage(w);
}

void window_move(window_t *w, int x, int y) {
    if (!w || (w->rect.x == x && w->rect.y == y)) return;
    window_damage(w);
    w->rect.x = x; w->rect.y = y;
    window_damage(w);
}

void window_resize(window_t *w, int ow, int oh) {
    if (!w) return;
    if (ow < w->min_w) ow = w->min_w;
    if (oh < w->min_h) oh = w->min_h;
    if (w->max_w && ow > w->max_w) ow = w->max_w;
    if (w->max_h && oh > w->max_h) oh = w->max_h;
    if (ow == w->rect.w && oh == w->rect.h) return;
    apply_rect(w, R(w->rect.x, w->rect.y, ow, oh));
    if (!w->maximized) w->restore = w->rect;
}

void window_maximize(window_t *w) {
    if (!w || w->maximized || w->no_minmax || !w->resizable) return;
    w->restore = w->rect;
    w->maximized = true;
    apply_rect(w, work);
}

void window_restore(window_t *w) {
    if (!w) return;
    if (w->minimized) { window_focus(w); return; }
    if (w->maximized) { w->maximized = false; apply_rect(w, w->restore); }
}

void window_toggle_maximize(window_t *w) {
    if (!w) return;
    if (w->maximized) window_restore(w); else window_maximize(w);
}

/* ═══════════════════════════ queries ═══════════════════════════ */
window_t *wm_active(void) { return active; }
int wm_window_count(void) { return zcount; }

window_t *wm_find_owner(int owner) {
    for (int i = 0; i < zcount; i++) if (zorder[i]->owner == owner && !zorder[i]->parent) return zorder[i];
    return 0;
}
bool wm_owner_running(int owner) { return wm_find_owner(owner) != 0; }

void wm_cycle_focus(void) {
    /* Bring the window that has been in the background longest to the front (Alt+Tab). */
    if (top_modal()) return;
    for (int i = 0; i < zcount; i++) {
        window_t *w = zorder[i];
        if (w->visible && !w->parent && w != active) { window_focus(w); return; }
    }
}

void wm_close_active(void) { if (active) window_close(active); }

/* ═══════════════════════════ painting ═══════════════════════════ */
static void draw_control(gfx_t *g, rect_t r, int which, bool pressed, bool focused, bool maximized) {
    paint_t p;
    uint32_t border = RGB(0x6E, 0x87, 0x98), glyph = RGB(0x1B, 0x33, 0x44);
    if (which == 2) {
        p = pressed ? paint_v2(RGB(0xD9, 0x7A, 0x6A), RGB(0x8F, 0x2C, 0x1E)) : paint_v2(RGB(0xF7, 0xD0, 0xC9), RGB(0xBB, 0x49, 0x35));
        glyph = RGB(255, 255, 255);
    } else {
        p = pressed ? paint_v2(RGB(0xB9, 0xD5, 0xE8), RGB(0x6F, 0x97, 0xB4)) : paint_v2(RGB(0xEF, 0xF9, 0xFF), RGB(0x9E, 0xBB, 0xD0));
    }
    if (!focused) {
        p.col[0] = color_lerp(p.col[0], RGB(0xE4, 0xEE, 0xF5), 110);
        p.col[1] = color_lerp(p.col[1], RGB(0xC9, 0xD9, 0xE6), 110);
    }
    gfx_fill_round_paint(g, r.x, r.y, r.w, r.h, 3, &p);
    gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 3, border);
    gfx_hline(g, r.x + 2, r.y + 1, r.w - 4, WHITE_A(60));
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    sym_id_t s = which == 0 ? SYM_MINIMIZE : (which == 1 ? (maximized ? SYM_RESTORE : SYM_MAXIMIZE) : SYM_CLOSE);
    sym_draw(g, s, cx, cy, which == 2 ? 8 : 9, glyph);
}

static void blit_surface(gfx_t *g, window_t *w) {
    rect_t cr = window_client_screen(w), a;
    if (!rect_intersect(g->clip, cr, &a)) return;
    for (int y = a.y; y < a.y + a.h; y++)
        mem_copy32(g->px + y * g->stride + a.x,
                   w->surface.px + (y - cr.y) * w->surface.stride + (a.x - cr.x), (size_t)a.w);
}

static void draw_window(gfx_t *g, window_t *w) {
    rect_t rc = w->rect;
    bool focused = (w == active);
    if (!gfx_visible(g, rect_inflate(rc, 26))) return;

    int blur = focused ? 16 : 10, pct = focused ? 42 : 26;
    gfx_shadow(g, rc.x, rc.y, rc.w, rc.h, 6, blur, pct, 0, 4);
    int sa = pct * 255 / 100 * blur * blur / ((blur + 1) * (blur + 1));
    gfx_fill_rect(g, rc.x + 3, rc.y + rc.h, rc.w - 6, 4, ARGB(sa, 0, 0, 0));

    paint_t body = paint_flat(RGB(0xEA, 0xF3, 0xF9));
    gfx_fill_round_paint(g, rc.x, rc.y, rc.w, rc.h, 6, &body);

    paint_t tb = focused
        ? paint_v3(RGBA(231, 247, 255, 92), RGBA(121, 179, 215, 78), 480, RGBA(57, 119, 163, 82))
        : paint_v3(RGBA(240, 247, 252, 92), RGBA(190, 213, 230, 72), 480, RGBA(150, 182, 205, 72));
    gfx_fill_shape(g, rc.x + 1, rc.y + 1, rc.w - 2, WM_TITLE_H, 5, 5, 0, 0, &tb);
    gfx_hline(g, rc.x + 1, rc.y + WM_TITLE_H, rc.w - 2, RGB(0x31, 0x5C, 0x78));
    gfx_hline(g, rc.x + 6, rc.y + 2, rc.w - 12, WHITE_A(55));

    icon_draw(g, w->icon, rc.x + 10, rc.y + 8, 16);
    int by = text_baseline_in(&font_ui13b, rc.y + 1, WM_TITLE_H);
    gfx_text(g, &font_ui13b, rc.x + 33, by + 1, w->title, WHITE_A(70));
    gfx_text(g, &font_ui13b, rc.x + 32, by, w->title, focused ? RGB(0x10, 0x2D, 0x43) : RGB(0x4A, 0x62, 0x75));

    for (int which = first_ctrl(w); which < 3; which++)
        draw_control(g, ctrl_rect(w, which), which, press.w == w && press.which == which, focused, w->maximized);

    if (w->dirty) {                                        /* repaint the client surface lazily */
        gfx_set_clip(&w->surface, R(0, 0, w->surface.w, w->surface.h));
        if (w->paint) w->paint(&w->surface, w, window_client_size(w));
        w->dirty = false;
    }
    blit_surface(g, w);

    gfx_stroke_round_rect(g, rc.x, rc.y, rc.w, rc.h, 6, RGB(0x0A, 0x35, 0x52));
    gfx_stroke_round_rect(g, rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2, 5, WHITE_A(45));
}

void wm_draw(gfx_t *g) {
    for (int i = 0; i < zcount; i++)
        if (zorder[i]->visible && !zorder[i]->minimized) draw_window(g, zorder[i]);
}

/* ═══════════════════════════ input ═══════════════════════════ */
static window_t *window_at(int x, int y) {
    for (int i = zcount - 1; i >= 0; i--) {
        window_t *w = zorder[i];
        if (w->visible && !w->minimized && rect_contains(w->rect, x, y)) return w;
    }
    return 0;
}

static int resize_edges(const window_t *w, int x, int y) {
    if (!w->resizable || w->maximized) return 0;
    if (!rect_contains(w->rect, x, y)) return 0;
    int e = 0;
    if (x < w->rect.x + WM_RESIZE_GRIP) e |= EDGE_L;
    if (x >= w->rect.x + w->rect.w - WM_RESIZE_GRIP) e |= EDGE_R;
    if (y < w->rect.y + WM_RESIZE_GRIP) e |= EDGE_T;
    if (y >= w->rect.y + w->rect.h - WM_RESIZE_GRIP) e |= EDGE_B;
    return e;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void do_resize(int x, int y) {
    window_t *w = rsz.w;
    int dx = x - rsz.sx, dy = y - rsz.sy;
    rect_t s = rsz.start, n = s;
    int maxw = w->max_w ? w->max_w : 100000, maxh = w->max_h ? w->max_h : 100000;
    if (rsz.edges & EDGE_R) n.w = clampi(s.w + dx, w->min_w, maxw);
    if (rsz.edges & EDGE_L) { n.w = clampi(s.w - dx, w->min_w, maxw); n.x = s.x + s.w - n.w; }
    if (rsz.edges & EDGE_B) n.h = clampi(s.h + dy, w->min_h, maxh);
    if (rsz.edges & EDGE_T) {
        n.h = clampi(s.h - dy, w->min_h, maxh);
        n.y = s.y + s.h - n.h;
        if (n.y < work.y) { n.y = work.y; n.h = s.y + s.h - work.y; }
    }
    if (n.x != w->rect.x || n.y != w->rect.y || n.w != w->rect.w || n.h != w->rect.h) {
        apply_rect(w, n);
        w->restore = w->rect;
    }
}

bool wm_captured(void) { return drag.on || rsz.on || press.w != 0; }

bool wm_mouse(int x, int y, int buttons, int prev, uint64_t now) {
    bool left_down = (buttons & 1) && !(prev & 1);
    bool left_up   = !(buttons & 1) && (prev & 1);

    if (rsz.on) {
        if (!(buttons & 1)) { rsz.on = false; return true; }
        do_resize(x, y);
        return true;
    }
    if (drag.on) {
        if (!(buttons & 1)) { drag.on = false; return true; }
        window_t *w = drag.w;
        int sw = compositor_width(), sh = compositor_height();
        int nx = clampi(x - drag.off_x, 80 - w->rect.w, sw - 80);
        int ny = clampi(y - drag.off_y, work.y, sh - 48);
        window_move(w, nx, ny);
        return true;
    }
    if (press.w) {
        window_t *w = press.w;
        int which = press.which;
        if (left_up) {
            press.w = 0; press.which = -1;
            if (rect_contains(ctrl_rect(w, which), x, y)) {
                if (which == 0) window_minimize(w);
                else if (which == 1) window_toggle_maximize(w);
                else window_close(w);
            } else window_damage(w);
        }
        return true;
    }

    window_t *w = window_at(x, y);
    window_t *modal = top_modal();
    if (modal && w && w != modal && w->parent != modal) return true;    /* blocked by the modal dialog */

    if (left_down) {
        if (!w) return false;
        if (w != active) window_focus(w);
        w = window_at(x, y);                                     /* focusing may have changed the stack */
        if (!w) return true;

        for (int which = first_ctrl(w); which < 3; which++) {
            if (rect_contains(ctrl_rect(w, which), x, y)) {
                press.w = w; press.which = which; window_damage(w);
                return true;
            }
        }
        int edges = resize_edges(w, x, y);
        if (edges) {
            rsz.on = true; rsz.w = w; rsz.edges = edges; rsz.start = w->rect; rsz.sx = x; rsz.sy = y;
            return true;
        }
        if (rect_contains(title_rect(w), x, y)) {
            if (w->resizable && !w->no_minmax && last_title_click.w == w && now - last_title_click.tick < 40) {
                last_title_click.w = 0;
                window_toggle_maximize(w);
                return true;
            }
            last_title_click.w = w; last_title_click.tick = now;
            if (w->movable && !w->maximized) {
                drag.on = true; drag.w = w; drag.off_x = x - w->rect.x; drag.off_y = y - w->rect.y;
            }
            return true;
        }
    }

    if (w) {
        rect_t cl = window_client_screen(w);
        if ((rect_contains(cl, x, y) || (buttons & 1) || left_up) && w == active && w->on_mouse)
            w->on_mouse(w, x - cl.x, y - cl.y, buttons, prev);
        return true;
    }
    return false;
}

bool wm_key(uint16_t key, uint8_t mods) {
    if (!active || !active->on_key) return false;
    active->on_key(active, key, mods);
    return true;
}

bool wm_command(int cmd) {
    if (!active || !active->on_command) return false;
    active->on_command(active, cmd);
    return true;
}
=======
#include "wm.h"
#include "compositor.h"
#include "../klib.h"
#include "../alloc.h"

#define CTRL_W 27
#define CTRL_H 20

static window_t  wins[WM_MAX_WINDOWS];
static window_t *zorder[WM_MAX_WINDOWS];      /* back .. front */
static int       zcount;
static window_t *active;
static rect_t    work;

static struct {
    window_t *w;
    int off_x, off_y;
    bool active;
} drag;

static struct {
    window_t *w;
    int which;                                  /* 0 min, 1 max, 2 close, -1 none */
} press;

static struct { window_t *w; uint64_t tick; } last_title_click;

/* ── geometry ─────────────────────────────────────────────────────────────── */
rect_t wm_client_rect(const window_t *w) {
    return R(w->rect.x + 1, w->rect.y + 1 + WM_TITLE_H, w->rect.w - 2, w->rect.h - 2 - WM_TITLE_H);
}

static rect_t title_rect(const window_t *w) { return R(w->rect.x + 1, w->rect.y + 1, w->rect.w - 2, WM_TITLE_H); }

static rect_t ctrl_rect(const window_t *w, int i) {
    int close_x = w->rect.x + w->rect.w - 1 - 6 - CTRL_W;
    int x = close_x - (2 - i) * (CTRL_W + 2);
    return R(x, w->rect.y + 1 + (WM_TITLE_H - CTRL_H) / 2, CTRL_W, CTRL_H);
}

void wm_damage_window(const window_t *w) { compositor_add_damage(rect_inflate(w->rect, 26)); }

/* ── lifecycle ────────────────────────────────────────────────────────────── */
void wm_init(rect_t work_area) {
    memset(wins, 0, sizeof(wins));
    memset(zorder, 0, sizeof(zorder));
    zcount = 0; active = 0; work = work_area;
    memset(&drag, 0, sizeof(drag));
    press.w = 0; press.which = -1;
    last_title_click.w = 0;
}

void wm_reset(void) {
    for (int i = zcount - 1; i >= 0; i--) {
        window_t *w = zorder[i];
        if (w->on_close) w->on_close(w);
    }
    wm_init(work);
    compositor_damage_all();
}

window_t *wm_create(int app, const char *title, icon_id_t icon, rect_t rect, int min_w, int min_h) {
    window_t *w = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) if (!wins[i].used) { w = &wins[i]; break; }
    if (!w) return 0;
    memset(w, 0, sizeof(*w));
    w->used = true;
    w->app = app;
    k_strncpy(w->title, title, sizeof(w->title) - 1);
    w->icon = icon;
    w->rect = rect;
    w->restore = rect;
    w->min_w = min_w; w->min_h = min_h;
    zorder[zcount++] = w;
    wm_activate(w);
    return w;
}

window_t *wm_find_app(int app) {
    for (int i = 0; i < zcount; i++) if (zorder[i]->app == app) return zorder[i];
    return 0;
}

bool wm_app_running(int app) { return wm_find_app(app) != 0; }
window_t *wm_active(void) { return active; }

static void z_remove(window_t *w) {
    int idx = -1;
    for (int i = 0; i < zcount; i++) if (zorder[i] == w) { idx = i; break; }
    if (idx < 0) return;
    for (int i = idx; i < zcount - 1; i++) zorder[i] = zorder[i + 1];
    zcount--;
}

static void activate_topmost_visible(void) {
    active = 0;
    for (int i = zcount - 1; i >= 0; i--)
        if (!zorder[i]->minimized) { active = zorder[i]; break; }
    if (active) wm_damage_window(active);
}

void wm_activate(window_t *w) {
    if (!w) return;
    window_t *prev = active;
    if (w->minimized) w->minimized = false;
    z_remove(w);
    zorder[zcount++] = w;
    active = w;
    if (prev && prev != w) wm_damage_window(prev);       /* title bar look changes */
    wm_damage_window(w);
}

void wm_close(window_t *w) {
    if (!w || !w->used) return;
    wm_damage_window(w);
    if (w->on_close) w->on_close(w);
    z_remove(w);
    if (drag.w == w) drag.active = false;
    if (press.w == w) { press.w = 0; press.which = -1; }
    if (last_title_click.w == w) last_title_click.w = 0;
    bool was_active = (active == w);
    w->used = false;
    if (was_active) activate_topmost_visible();
}

void wm_minimize(window_t *w) {
    if (!w) return;
    w->minimized = true;
    wm_damage_window(w);
    if (active == w) activate_topmost_visible();
}

void wm_toggle_maximize(window_t *w) {
    if (!w) return;
    wm_damage_window(w);
    if (w->maximized) {
        w->rect = w->restore;
        w->maximized = false;
    } else {
        w->restore = w->rect;
        w->rect = work;
        w->maximized = true;
    }
    wm_damage_window(w);
}

/* ── painting ─────────────────────────────────────────────────────────────── */
static void draw_control(gfx_t *g, rect_t r, int which, bool pressed, bool focused) {
    paint_t p;
    uint32_t border = RGB(0x6E, 0x87, 0x98), glyph = RGB(0x1B, 0x33, 0x44);
    if (which == 2) {
        p = paint_v2(RGB(0xF7, 0xD0, 0xC9), RGB(0xBB, 0x49, 0x35));
        glyph = RGB(255, 255, 255);
        if (pressed) p = paint_v2(RGB(0xD9, 0x7A, 0x6A), RGB(0x8F, 0x2C, 0x1E));
    } else {
        p = paint_v2(RGB(0xEF, 0xF9, 0xFF), RGB(0x9E, 0xBB, 0xD0));
        if (pressed) p = paint_v2(RGB(0xB9, 0xD5, 0xE8), RGB(0x6F, 0x97, 0xB4));
    }
    if (!focused) { p.col[0] = color_lerp(p.col[0], RGB(0xE4, 0xEE, 0xF5), 110); p.col[1] = color_lerp(p.col[1], RGB(0xC9, 0xD9, 0xE6), 110); }
    gfx_fill_round_paint(g, r.x, r.y, r.w, r.h, 3, &p);
    gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 3, border);
    gfx_hline(g, r.x + 2, r.y + 1, r.w - 4, WHITE_A(60));
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    sym_id_t s = which == 0 ? SYM_MINIMIZE : (which == 1 ? SYM_MAXIMIZE : SYM_CLOSE);
    if (which == 1) {
        /* the maximize glyph is shown as "restore" while maximized; the caller passes it via the window */
    }
    sym_draw(g, s, cx, cy, which == 2 ? 8 : 9, glyph);
}

static void draw_window(gfx_t *g, window_t *w, bool focused) {
    rect_t rc = w->rect;
    if (!gfx_visible(g, rect_inflate(rc, 26))) return;

    {
        int blur = focused ? 16 : 10, pct = focused ? 42 : 26;
        gfx_shadow(g, rc.x, rc.y, rc.w, rc.h, 6, blur, pct, 0, 4);
        /* the 4px strip between the window's bottom edge and the offset shadow box must be dark too */
        int a = pct * 255 / 100 * blur * blur / ((blur + 1) * (blur + 1));
        gfx_fill_rect(g, rc.x + 3, rc.y + rc.h, rc.w - 6, 4, ARGB(a, 0, 0, 0));
    }

    /* frame + body */
    paint_t body = paint_flat(RGB(0xEA, 0xF3, 0xF9));
    gfx_fill_round_paint(g, rc.x, rc.y, rc.w, rc.h, 6, &body);

    /* Aero glass title bar */
    paint_t tb = focused
        ? paint_v3(RGBA(231, 247, 255, 92), RGBA(121, 179, 215, 78), 480, RGBA(57, 119, 163, 82))
        : paint_v3(RGBA(240, 247, 252, 92), RGBA(190, 213, 230, 72), 480, RGBA(150, 182, 205, 72));
    gfx_fill_shape(g, rc.x + 1, rc.y + 1, rc.w - 2, WM_TITLE_H, 5, 5, 0, 0, &tb);
    gfx_hline(g, rc.x + 1, rc.y + WM_TITLE_H, rc.w - 2, RGB(0x31, 0x5C, 0x78));
    gfx_hline(g, rc.x + 6, rc.y + 2, rc.w - 12, WHITE_A(55));

    extern const font_t font_ui13b;
    icon_draw(g, w->icon, rc.x + 10, rc.y + 8, 16);
    int by = text_baseline_in(&font_ui13b, rc.y + 1, WM_TITLE_H);
    uint32_t tc = focused ? RGB(0x10, 0x2D, 0x43) : RGB(0x4A, 0x62, 0x75);
    gfx_text(g, &font_ui13b, rc.x + 32 + 1, by + 1, w->title, WHITE_A(70));      /* text-shadow: 0 1px white */
    gfx_text(g, &font_ui13b, rc.x + 32, by, w->title, tc);

    for (int i = 0; i < 3; i++) {
        rect_t cr = ctrl_rect(w, i);
        if (i == 1 && w->maximized) {
            draw_control(g, cr, 1, press.w == w && press.which == 1, focused);
            /* overdraw the glyph with the restore symbol */
            gfx_fill_round_rect(g, cr.x + 3, cr.y + 3, cr.w - 6, cr.h - 6, 1, RGB(0xC2, 0xD6, 0xE4));
            sym_draw(g, SYM_RESTORE, cr.x + cr.w / 2, cr.y + cr.h / 2, 9, RGB(0x1B, 0x33, 0x44));
        } else {
            draw_control(g, cr, i, press.w == w && press.which == i, focused);
        }
    }

    /* client area */
    rect_t client = wm_client_rect(w);
    if (w->paint) {
        rect_t saved = gfx_push_clip(g, client);
        w->paint(g, w, client);
        gfx_pop_clip(g, saved);
    }

    /* outer border on top of everything */
    gfx_stroke_round_rect(g, rc.x, rc.y, rc.w, rc.h, 6, RGB(0x0A, 0x35, 0x52));
    gfx_stroke_round_rect(g, rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2, 5, WHITE_A(45));
}

void wm_draw(gfx_t *g) {
    for (int i = 0; i < zcount; i++) {
        window_t *w = zorder[i];
        if (w->minimized) continue;
        draw_window(g, w, w == active);
    }
}

/* ── input ────────────────────────────────────────────────────────────────── */
static window_t *window_at(int x, int y) {
    for (int i = zcount - 1; i >= 0; i--) {
        window_t *w = zorder[i];
        if (!w->minimized && rect_contains(w->rect, x, y)) return w;
    }
    return 0;
}

bool wm_hit(int x, int y) { return window_at(x, y) != 0; }
bool wm_is_dragging(void) { return drag.active; }
bool wm_captured(void) { return drag.active || press.w != 0; }

bool wm_mouse(int x, int y, int buttons, int prev, uint64_t now) {
    bool left_down = (buttons & 1) && !(prev & 1);
    bool left_up   = !(buttons & 1) && (prev & 1);

    /* an active drag owns the mouse until the button is released */
    if (drag.active) {
        if (!(buttons & 1)) { drag.active = false; return true; }
        window_t *w = drag.w;
        int sw = compositor_width(), sh = compositor_height();
        int nx = x - drag.off_x, ny = y - drag.off_y;
        if (ny < work.y) ny = work.y;
        if (ny > sh - 48) ny = sh - 48;
        if (nx < 80 - w->rect.w) nx = 80 - w->rect.w;
        if (nx > sw - 80) nx = sw - 80;
        if (nx != w->rect.x || ny != w->rect.y) {
            wm_damage_window(w);
            w->rect.x = nx; w->rect.y = ny;
            wm_damage_window(w);
        }
        return true;
    }

    /* pressed window-control buttons complete on release */
    if (press.w) {
        window_t *w = press.w;
        int which = press.which;
        if (left_up) {
            press.w = 0; press.which = -1;
            if (rect_contains(ctrl_rect(w, which), x, y)) {
                if (which == 0) wm_minimize(w);
                else if (which == 1) wm_toggle_maximize(w);
                else wm_close(w);
            } else {
                wm_damage_window(w);
            }
        }
        return true;
    }

    window_t *w = window_at(x, y);

    if (left_down) {
        if (!w) return false;
        if (w != active) wm_activate(w);

        for (int i = 0; i < 3; i++) {
            if (rect_contains(ctrl_rect(w, i), x, y)) {
                press.w = w; press.which = i;
                wm_damage_window(w);
                return true;
            }
        }
        if (rect_contains(title_rect(w), x, y)) {
            if (last_title_click.w == w && now - last_title_click.tick < 40) {     /* double click */
                last_title_click.w = 0;
                wm_toggle_maximize(w);
                return true;
            }
            last_title_click.w = w; last_title_click.tick = now;
            if (!w->maximized) {
                drag.active = true; drag.w = w;
                drag.off_x = x - w->rect.x; drag.off_y = y - w->rect.y;
            }
            return true;
        }
    }

    if (w) {
        rect_t cl = wm_client_rect(w);
        if (rect_contains(cl, x, y) || (buttons & 1) || left_up) {
            if (w->on_mouse && w == active) w->on_mouse(w, x - cl.x, y - cl.y, buttons, prev);
        }
        return true;
    }
    return false;
}

bool wm_key(uint16_t key, uint8_t mods) {
    if (!active) return false;
    if (active->on_key) { active->on_key(active, key, mods); return true; }
    return false;
}
>>>>>>> 23b11cf3087acc2108f276bdfb25d3a6f909e2f7
