/* kernel/gui/wm.h - window manager: z-order, focus, dragging, min/max/close, Aero chrome */
#ifndef BORNOMALA_WM_H
#define BORNOMALA_WM_H

#include "gfx.h"
#include "icons.h"

#define WM_MAX_WINDOWS 12
#define WM_TITLE_H     31

typedef struct window window_t;

struct window {
    bool       used;
    int        app;                 /* application id (apps.h), 0 = none */
    char       title[48];
    icon_id_t  icon;
    rect_t     rect;                /* outer rectangle, screen coordinates */
    rect_t     restore;             /* size before maximizing */
    int        min_w, min_h;
    bool       maximized;
    bool       minimized;
    void      *data;                /* application state */

    /* client-area callbacks; coordinates handed to on_mouse are client-local */
    void (*paint)(gfx_t *g, window_t *w, rect_t client);
    void (*on_mouse)(window_t *w, int lx, int ly, int buttons, int prev_buttons);
    void (*on_key)(window_t *w, uint16_t key, uint8_t mods);
    void (*on_close)(window_t *w);
};

void       wm_init(rect_t work_area);
void       wm_reset(void);                                   /* closes everything (used on lock/logout) */
window_t  *wm_create(int app, const char *title, icon_id_t icon, rect_t rect, int min_w, int min_h);
window_t  *wm_find_app(int app);
bool       wm_app_running(int app);
void       wm_close(window_t *w);
void       wm_minimize(window_t *w);
void       wm_toggle_maximize(window_t *w);
void       wm_activate(window_t *w);                         /* raise + focus, restores if minimized */
window_t  *wm_active(void);

/* Draw every visible window back-to-front. */
void       wm_draw(gfx_t *g);

/* Mouse: returns true if a window consumed the event (so the desktop underneath must ignore it). */
bool       wm_mouse(int x, int y, int buttons, int prev_buttons, uint64_t now_ticks);
bool       wm_key(uint16_t key, uint8_t mods);
bool       wm_is_dragging(void);
bool       wm_captured(void);                                /* dragging, or a title-bar button is held down */
bool       wm_hit(int x, int y);                             /* is a visible window under this point? */

rect_t     wm_client_rect(const window_t *w);
void       wm_damage_window(const window_t *w);              /* damage incl. shadow */

#endif
