/* kernel/mouse.h - PS/2 mouse (3-byte standard protocol) */
#ifndef BORNOMALA_MOUSE_H
#define BORNOMALA_MOUSE_H

#include <stdint.h>
#include <stdbool.h>

bool mouse_init(void);                  /* returns false if no mouse answered */
void mouse_feed_byte(uint8_t b);        /* IRQ context */
bool mouse_present(void);

#endif
