/* kernel/gui/lock.h - the lock screen scene (design: bornomalaos_lock_screen.html) */
#ifndef BORNOMALA_LOCK_H
#define BORNOMALA_LOCK_H

#include "gfx.h"
#include <stdint.h>

void lock_enter(void);                 /* bake background, reset the login form */
void lock_draw(gfx_t *g);
void lock_mouse(int x, int y, int buttons, int prev_buttons);
void lock_key(uint16_t key, uint8_t mods);
void lock_tick(uint64_t ticks);        /* clock + error timeout */

#endif
