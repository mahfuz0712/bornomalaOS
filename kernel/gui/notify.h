/* kernel/gui/notify.h - notification foundation (stacked, timed banners) */
#ifndef BORNOMALA_NOTIFY_H
#define BORNOMALA_NOTIFY_H
#include <stdint.h>
#include "gfx.h"
void   notify_post(const char *title, const char *body);
void   notify_tick(uint64_t ticks);
void   notify_draw(gfx_t *g);
void   notify_reset(void);
#endif
