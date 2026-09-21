/* kernel/gui/menu.h - popup menu (desktop context menu, system menu, lock-screen power menu) */
#ifndef BORNOMALA_MENU_H
#define BORNOMALA_MENU_H

#include "gfx.h"

#define POPUP_MAX_ITEMS 8

typedef struct {
    const char *label;      /* NULL label + separator=true draws a divider */
    int         id;
    bool        separator;
} menu_item_t;

typedef enum { POPUP_LIGHT = 0, POPUP_DARK = 1 } popup_style_t;

typedef struct {
    bool          open;
    popup_style_t style;
    rect_t        rect;
    menu_item_t   items[POPUP_MAX_ITEMS];
    int           count;
    int           hover;    /* index or -1 */
} popup_t;

#define POPUP_CLICK_OUTSIDE (-2)
#define POPUP_CLICK_NONE    (-1)

/* Opens the menu with its top-left at (x, y) (or its bottom-left when anchor_bottom) and keeps it on screen. */
void   popup_open(popup_t *p, int x, int y, bool anchor_bottom, const menu_item_t *items, int n,
                  popup_style_t style, int screen_w, int screen_h);
void   popup_close(popup_t *p);
rect_t popup_damage_rect(const popup_t *p);            /* includes the shadow */
/* Returns true if the highlighted row changed (caller should add damage). */
bool   popup_move(popup_t *p, int x, int y);
/* Returns the activated item id, POPUP_CLICK_OUTSIDE if the click missed, POPUP_CLICK_NONE for separators. */
int    popup_click(popup_t *p, int x, int y);
void   popup_draw(gfx_t *g, const popup_t *p);

#endif
