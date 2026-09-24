/* kernel/gui/uistate.h - the ONE place that knows which top-level UI state the system is in */
#ifndef BORNOMALA_UISTATE_H
#define BORNOMALA_UISTATE_H
#include <stdbool.h>

typedef enum {
    UI_LOCK_SCREEN = 0,
    UI_DESKTOP,
    UI_LAUNCHPAD,       /* desktop + Launchpad overlay */
    UI_SLEEP,
    UI_SHUTDOWN,
    UI_RESTART,
    UI_SIGNOUT
} ui_state_t;

ui_state_t ui_state(void);
void       ui_request(ui_state_t next);            /* deferred: applied by the session loop */
bool       ui_take_request(ui_state_t *next);      /* session loop only */
void       ui_commit(ui_state_t now);              /* session loop only */
bool       ui_is_desktop_scene(ui_state_t s);      /* DESKTOP or LAUNCHPAD */
#endif
