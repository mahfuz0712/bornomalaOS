/* kernel/gui/compositor.h - back buffer, background layer, damage tracking, presentation
 *
 * Pipeline per frame:
 *   1. anything that changed calls compositor_add_damage(rect)
 *   2. compositor_begin_frame()  -> restores the background layer into the damaged area of the
 *                                   back buffer and returns the canvas (clip = damaged area)
 *   3. the shell draws windows / dock / cursor on top, clipped automatically
 *   4. compositor_end_frame()    -> copies ONLY the damaged area to the real framebuffer
 */
#ifndef BORNOMALA_COMPOSITOR_H
#define BORNOMALA_COMPOSITOR_H

#include <stdbool.h>
#include "gfx.h"
#include "../mb2.h"

bool     compositor_init(const mb2_framebuffer_t *fb);
int      compositor_width(void);
int      compositor_height(void);

gfx_t   *compositor_background(void);    /* static layer, redrawn only when the scene "theme" changes */
gfx_t   *compositor_canvas(void);        /* back buffer */

void     compositor_add_damage(rect_t r);
void     compositor_damage_all(void);
bool     compositor_has_damage(void);

gfx_t   *compositor_begin_frame(void);
void     compositor_end_frame(void);

void     compositor_blank(void);         /* paint the real framebuffer black (sleep) */

#endif
