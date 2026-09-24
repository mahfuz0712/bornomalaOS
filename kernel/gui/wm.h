<<<<<<< HEAD
/* kernel/gui/wm.h - window manager (Phase 6)
 *
 * Every window owns an off-screen client SURFACE. Applications paint into their surface (in
 * surface-local coordinates, origin 0,0) only when the window is invalidated; the window manager
 * composes  shadow + Aero chrome + surface  onto the compositor canvas. Moving a window therefore
 * never re-runs application paint code. */
#ifndef BORNOMALA_WM_H
#define BORNOMALA_WM_H

#include <stdint.h>
#include <stddef.h>
#include "gfx.h"
#include "icons.h"

#define WM_MAX_WINDOWS 16
#define WM_TITLE_H     31
#define WM_RESIZE_GRIP 5

typedef struct window window_t;

/* Edit commands routed by the shortcut manager to the active window. */
typedef enum { WM_CMD_COPY = 1, WM_CMD_CUT, WM_CMD_PASTE, WM_CMD_UNDO, WM_CMD_SELECT_ALL } wm_command_t;

typedef enum {
    WINDOW_MOVABLE   = 1u << 0,
    WINDOW_RESIZABLE = 1u << 1,
    WINDOW_MODAL     = 1u << 2,
    WINDOW_NO_MINMAX = 1u << 3,       /* dialogs: only a close button */
} window_flags_t;

struct window {
    bool       used;
    uint32_t   id;                    /* unique, never reused */
    int        owner;                 /* owning application id */
    char       title[48];
    icon_id_t  icon;

    rect_t     rect;                  /* outer rectangle in screen coordinates */
    rect_t     restore;               /* geometry before maximize */
    int        min_w, min_h, max_w, max_h;

    bool       visible, focused, minimized, maximized;
    bool       movable, resizable, modal, no_minmax;
    window_t  *parent;                /* child windows stay above, and die with, their parent */

    gfx_t      surface;               /* client area buffer */
    uint32_t  *surface_px;
    size_t     surface_pages;
    bool       dirty;

    void      *data;                  /* application state */
    /* All callbacks use CLIENT-local coordinates (0,0 = top-left of the client area). */
    void (*paint)(gfx_t *g, window_t *w, rect_t client);
    void (*on_mouse)(window_t *w, int lx, int ly, int buttons, int prev_buttons);
    void (*on_key)(window_t *w, uint16_t key, uint8_t mods);
    void (*on_command)(window_t *w, int cmd);
    void (*on_resize)(window_t *w);
    void (*on_close)(window_t *w);
};

void       wm_init(rect_t work_area);
void       wm_reset(void);

/* ── window API ── */
window_t  *window_create(int owner, const char *title, icon_id_t icon, rect_t rect,
                         int min_w, int min_h, unsigned flags, window_t *parent);
void       window_destroy(window_t *w);          /* free without calling on_close */
void       window_close(window_t *w);            /* on_close + children + destroy */
void       window_show(window_t *w);
void       window_hide(window_t *w);
void       window_focus(window_t *w);            /* raise + activate (restores if minimized) */
void       window_move(window_t *w, int x, int y);
void       window_resize(window_t *w, int outer_w, int outer_h);
void       window_minimize(window_t *w);
void       window_maximize(window_t *w);
void       window_restore(window_t *w);
void       window_toggle_maximize(window_t *w);
void       window_invalidate(window_t *w);       /* repaint the surface + damage its screen area */

rect_t     window_client_size(const window_t *w);        /* R(0,0,cw,ch) */
rect_t     window_client_screen(const window_t *w);      /* client area in screen coordinates */
void       window_damage(const window_t *w);             /* whole window incl. shadow */

/* ── manager ── */
window_t  *wm_active(void);
window_t  *wm_find_owner(int owner);
bool       wm_owner_running(int owner);
void       wm_cycle_focus(void);                          /* Alt+Tab */
void       wm_close_active(void);                         /* Alt+F4 */
void       wm_draw(gfx_t *g);
bool       wm_mouse(int x, int y, int buttons, int prev_buttons, uint64_t now_ticks);
bool       wm_key(uint16_t key, uint8_t mods);            /* to the active window */
bool       wm_command(int cmd);                           /* WM_CMD_* to the active window */
bool       wm_captured(void);                             /* drag / resize / pressed button in progress */
bool       wm_has_modal(void);
int        wm_window_count(void);

#endif
=======
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
>>>>>>> 23b11cf3087acc2108f276bdfb25d3a6f909e2f7
