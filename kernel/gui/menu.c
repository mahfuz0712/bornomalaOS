#include "menu.h"
#include "../klib.h"

static int item_h(const popup_t *p, const menu_item_t *it) {
    if (it->separator) return 9;
    return p->style == POPUP_DARK ? 34 : 27;
}

void popup_open(popup_t *p, int x, int y, bool anchor_bottom, const menu_item_t *items, int n,
                popup_style_t style, int sw, int sh) {
    memset(p, 0, sizeof(*p));
    if (n > POPUP_MAX_ITEMS) n = POPUP_MAX_ITEMS;
    for (int i = 0; i < n; i++) p->items[i] = items[i];
    p->count = n;
    p->style = style;
    p->hover = -1;
    p->open = true;

    int w = style == POPUP_DARK ? 170 : 200;
    int h = (style == POPUP_DARK) ? 16 : 6;                /* padding */
    for (int i = 0; i < n; i++) h += item_h(p, &p->items[i]);
    if (anchor_bottom) y -= h;
    if (x + w > sw - 4) x = sw - 4 - w;
    if (y + h > sh - 4) y = sh - 4 - h;
    if (x < 4) x = 4;
    if (y < 4) y = 4;
    p->rect = R(x, y, w, h);
}

void popup_close(popup_t *p) { p->open = false; p->hover = -1; }

rect_t popup_damage_rect(const popup_t *p) { return rect_inflate(p->rect, 22); }

static int row_at(const popup_t *p, int x, int y) {
    if (!rect_contains(p->rect, x, y)) return -1;
    int pad = p->style == POPUP_DARK ? 8 : 3;
    int cy = p->rect.y + pad;
    for (int i = 0; i < p->count; i++) {
        int h = item_h(p, &p->items[i]);
        if (y >= cy && y < cy + h) return p->items[i].separator ? -1 : i;
        cy += h;
    }
    return -1;
}

bool popup_move(popup_t *p, int x, int y) {
    if (!p->open) return false;
    int r = row_at(p, x, y);
    if (r == p->hover) return false;
    p->hover = r;
    return true;
}

int popup_click(popup_t *p, int x, int y) {
    if (!p->open) return POPUP_CLICK_OUTSIDE;
    if (!rect_contains(p->rect, x, y)) return POPUP_CLICK_OUTSIDE;
    int r = row_at(p, x, y);
    if (r < 0) return POPUP_CLICK_NONE;
    return p->items[r].id;
}

void popup_draw(gfx_t *g, const popup_t *p) {
    if (!p->open || !gfx_visible(g, popup_damage_rect(p))) return;
    extern const font_t font_ui13;
    rect_t rc = p->rect;
    bool dark = p->style == POPUP_DARK;

    if (dark) {
        gfx_shadow(g, rc.x, rc.y, rc.w, rc.h, 14, 14, 40, 0, 10);
        gfx_fill_round_rect(g, rc.x, rc.y, rc.w, rc.h, 14, RGBA(12, 23, 35, 92));
        gfx_stroke_round_rect(g, rc.x, rc.y, rc.w, rc.h, 14, WHITE_A(15));
    } else {
        gfx_shadow(g, rc.x, rc.y, rc.w, rc.h, 2, 8, 35, 3, 4);
        gfx_fill_rect(g, rc.x, rc.y, rc.w, rc.h, RGB(0xF4, 0xF8, 0xFB));
        gfx_stroke_round_rect(g, rc.x, rc.y, rc.w, rc.h, 0, RGB(0x78, 0x94, 0xAA));
    }

    int pad = dark ? 8 : 3;
    int cy = rc.y + pad;
    for (int i = 0; i < p->count; i++) {
        const menu_item_t *it = &p->items[i];
        int h = item_h(p, it);
        if (it->separator) {
            gfx_hline(g, rc.x + 8, cy + 4, rc.w - 16, dark ? WHITE_A(18) : RGB(0xB9, 0xCA, 0xD6));
        } else {
            if (i == p->hover) {
                if (dark) gfx_fill_round_rect(g, rc.x + 8, cy, rc.w - 16, h, 9, WHITE_A(12));
                else      gfx_fill_rect(g, rc.x + 3, cy, rc.w - 6, h, RGB(0xCC, 0xE7, 0xFA));
            }
            uint32_t tc = dark ? RGB(255, 255, 255) : RGB(0x1C, 0x2A, 0x36);
            gfx_text(g, &font_ui13, rc.x + (dark ? 20 : 12), text_baseline_in(&font_ui13, cy, h), it->label, tc);
        }
        cy += h;
    }
}
