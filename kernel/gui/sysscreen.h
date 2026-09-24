/* kernel/gui/sysscreen.h - shared full-screen system scenes: Booting, Shutting down,
   Restarting, Signing out. Same dark-blue Aero language as the lock screen (glass panel,
   bottom-center wordmark, no top navbar, no icons/cancel on power transitions). */
#ifndef BORNOMALA_SYSSCREEN_H
#define BORNOMALA_SYSSCREEN_H

#include <stdint.h>
#include "gfx.h"

typedef enum { SYS_BOOT, SYS_SHUTDOWN, SYS_RESTART, SYS_SIGNOUT, SYS_INSTALLER_INFO } sysscreen_kind_t;

void sysscreen_enter(sysscreen_kind_t kind, const char *user_initial /* SYS_SIGNOUT only, may be NULL */);
void sysscreen_draw(gfx_t *g, uint64_t ticks);   /* call every frame; animates logo/halo/progress */
bool sysscreen_progress_done(uint64_t ticks);    /* true once the fill animation has finished a cycle */

#endif
