/* kernel/gui/session.h - the graphical session: scenes (lock / desktop / sleep) and the main event loop */
#ifndef BORNOMALA_SESSION_H
#define BORNOMALA_SESSION_H

#include <stdbool.h>
#include "../mb2.h"

typedef enum { SCENE_LOCK = 0, SCENE_DESKTOP = 1, SCENE_SLEEP = 2 } scene_id_t;

/* Runs forever. Returns false only if the GUI could not be started (caller falls back to text mode). */
bool session_run(const mb2_framebuffer_t *fb);

void session_switch(scene_id_t scene);
int  session_mouse_x(void);
int  session_mouse_y(void);

#endif
