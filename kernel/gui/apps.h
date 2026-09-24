/* kernel/gui/apps.h - built-in demo applications that live inside window-manager windows */
#ifndef BORNOMALA_APPS_H
#define BORNOMALA_APPS_H

#include "wm.h"

typedef enum {
    APP_NONE = 0,
    APP_COMPUTER,
    APP_FILES,
    APP_NOTEPAD,
    APP_CALC,
    APP_ABOUT,
    APP_COUNT
} app_id_t;

/* Opens the app, or raises/restores it when it is already running. */
window_t *apps_launch(app_id_t id, int screen_w, int screen_h, const char *username);

/* Called every main-loop iteration; adds damage when something animates (caret blink). */
void apps_tick(uint64_t ticks);

#endif
