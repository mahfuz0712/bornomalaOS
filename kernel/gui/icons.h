/* kernel/gui/icons.h - vector icons and UI symbols (no bitmaps, everything is drawn) */
#ifndef BORNOMALA_ICONS_H
#define BORNOMALA_ICONS_H

#include "gfx.h"

typedef enum {
    ICON_LAUNCHPAD = 0, ICON_FILES, ICON_NOTEPAD, ICON_CALC, ICON_COMPUTER, ICON_BROWSER,
    ICON_PAINT, ICON_MEDIA, ICON_SETTINGS, ICON_CAMERA, ICON_CALENDAR, ICON_CODE,
    ICON_DASHBOARD, ICON_MAIL, ICON_NOTES, ICON_TRASH, ICON_DRIVE, ICON_FOLDER, ICON_DOC,
    ICON_COUNT
} icon_id_t;

/* Glyph only, filling roughly the size x size box at (x, y). */
void icon_draw(gfx_t *g, icon_id_t id, int x, int y, int size);

/* Rounded glass tile + glyph. style 0 = Dock (translucent), 1..3 = Launchpad variants. */
void icon_draw_tile(gfx_t *g, icon_id_t id, int x, int y, int size, int style);

typedef enum {
    SYM_POWER = 0, SYM_WIFI, SYM_BATTERY, SYM_VOLUME, SYM_SEARCH, SYM_ARROW_RIGHT,
    SYM_ACCESS, SYM_CLOSE, SYM_MINIMIZE, SYM_MAXIMIZE, SYM_RESTORE, SYM_GLOBE,
    SYM_PERSON
} sym_id_t;

/* Centred on (cx, cy); `size` is the nominal box. */
void sym_draw(gfx_t *g, sym_id_t id, int cx, int cy, int size, uint32_t color);

/* Battery with a fill level (0..100). */
void sym_battery_level(gfx_t *g, int x, int y, int w, int h, int percent, uint32_t color);

/* Mouse pointer arrow with hot spot at (x, y). */
void cursor_draw(gfx_t *g, int x, int y);
#define CURSOR_W 14
#define CURSOR_H 22

#endif
