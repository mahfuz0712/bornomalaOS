/* kernel/panic.h - fatal-error screen (works with or without a framebuffer) */
#ifndef BORNOMALA_PANIC_H
#define BORNOMALA_PANIC_H

#include "mb2.h"

void panic_register_framebuffer(const mb2_framebuffer_t *fb);

#endif
