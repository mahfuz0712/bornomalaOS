#include "uistate.h"
static ui_state_t current = UI_LOCK_SCREEN, pending = UI_LOCK_SCREEN;
static bool have_pending;
ui_state_t ui_state(void) { return current; }
void ui_request(ui_state_t next) { pending = next; have_pending = true; }
bool ui_take_request(ui_state_t *next) {
    if (!have_pending) return false;
    have_pending = false;
    *next = pending;
    return true;
}
void ui_commit(ui_state_t now) { current = now; }
bool ui_is_desktop_scene(ui_state_t s) { return s == UI_DESKTOP || s == UI_LAUNCHPAD; }
