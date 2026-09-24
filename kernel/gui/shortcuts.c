#include "shortcuts.h"
#include "uistate.h"
#include "wm.h"
#include "../input.h"

typedef enum { A_LAUNCHPAD, A_LOCK, A_ALT_TAB, A_CLOSE, A_COPY, A_CUT, A_PASTE, A_UNDO, A_SELECT_ALL } action_t;
typedef struct { uint16_t key; uint8_t mods; uint8_t mods_mask; action_t action; } binding_t;

#define M_ALL (MOD_CTRL | MOD_ALT)
static const binding_t table[] = {
    { KEY_SUPER,  0,        0,     A_LAUNCHPAD },
    { KEY_F1 + 3, 0,        M_ALL, A_LAUNCHPAD },       /* F4 */
    { '\t',       MOD_ALT,  M_ALL, A_ALT_TAB },
    { KEY_F1 + 3, MOD_ALT,  M_ALL, A_CLOSE },           /* Alt+F4 */
    { 'l',        MOD_CTRL, M_ALL, A_LOCK },
    { 'c',        MOD_CTRL, M_ALL, A_COPY },
    { 'x',        MOD_CTRL, M_ALL, A_CUT },
    { 'v',        MOD_CTRL, M_ALL, A_PASTE },
    { 'z',        MOD_CTRL, M_ALL, A_UNDO },
    { 'a',        MOD_CTRL, M_ALL, A_SELECT_ALL },
};

bool shortcuts_dispatch(uint16_t key, uint8_t mods) {
    if (key >= 'A' && key <= 'Z') key = (uint16_t)(key + 32);
    for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        const binding_t *b = &table[i];
        if (b->key != key || (mods & b->mods_mask) != b->mods) continue;
        switch (b->action) {
        case A_LAUNCHPAD: ui_request(ui_state() == UI_LAUNCHPAD ? UI_DESKTOP : UI_LAUNCHPAD); return true;
        case A_LOCK:      ui_request(UI_LOCK_SCREEN); return true;
        case A_ALT_TAB:   wm_cycle_focus(); return true;
        case A_CLOSE:     wm_close_active(); return true;
        case A_COPY:      return wm_command(WM_CMD_COPY);
        case A_CUT:       return wm_command(WM_CMD_CUT);
        case A_PASTE:     return wm_command(WM_CMD_PASTE);
        case A_UNDO:      return wm_command(WM_CMD_UNDO);
        case A_SELECT_ALL:return wm_command(WM_CMD_SELECT_ALL);
        }
    }
    return false;
}
