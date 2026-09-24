/* kernel/gui/session.h - the graphical session: state-driven main event loop */
#ifndef BORNOMALA_SESSION_H
#define BORNOMALA_SESSION_H

#include <stdbool.h>
#include "../mb2.h"
#include "../bootmode.h"

/* Runs forever. Returns false only if the GUI could not be started (caller falls back to text mode). */
bool session_run(const mb2_framebuffer_t *fb, boot_mode_t mode);

int  session_mouse_x(void);
int  session_mouse_y(void);

#endif
