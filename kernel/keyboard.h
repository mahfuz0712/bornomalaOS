/* kernel/keyboard.h - PS/2 keyboard (scancode set 1 via i8042 translation) */
#ifndef BORNOMALA_KEYBOARD_H
#define BORNOMALA_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

void    keyboard_init(void);
void    keyboard_feed_scancode(uint8_t sc);   /* IRQ context */
uint8_t keyboard_mods(void);

#endif
