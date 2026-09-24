#include "lockscreen.h"
#include "users.h"

static lockscreen_state_t g_state = LOCKSCREEN_LOCKED;
static uint32_t g_user_id;

void lockscreen_init(void) {
    const bm_user_t *u = users_current();
    g_user_id = u ? u->id : 0;
    g_state = LOCKSCREEN_LOCKED;
}

lockscreen_state_t lockscreen_state(void) { return g_state; }
uint32_t lockscreen_user_id(void) { return g_user_id; }

bool lockscreen_try_unlock(const char *password) {
    if (!g_user_id || !users_verify_password(g_user_id, password)) return false;
    g_state = LOCKSCREEN_AUTHENTICATED;
    return true;
}

void lockscreen_lock(void) { g_state = LOCKSCREEN_LOCKED; }
