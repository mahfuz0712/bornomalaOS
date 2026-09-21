/* kernel/gui/desktop.h - the desktop shell (design: windows7_aero_desktop-1.html):
   Aero wallpaper, top bar, desktop icons, floating Dock, Launchpad, windows, context menus, toasts */
#ifndef BORNOMALA_DESKTOP_H
#define BORNOMALA_DESKTOP_H

#include "gfx.h"
#include <stdint.h>

void desktop_enter(void);                 /* bake wallpaper (called every time the desktop becomes active) */
void desktop_draw(gfx_t *g);
void desktop_mouse(int x, int y, int buttons, int prev_buttons);
void desktop_key(uint16_t key, uint8_t mods);
void desktop_tick(uint64_t ticks);

#endif
