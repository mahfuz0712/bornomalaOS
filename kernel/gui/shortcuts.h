/* kernel/gui/shortcuts.h - central keyboard shortcut manager. Applications never hard-code shortcuts. */
#ifndef BORNOMALA_SHORTCUTS_H
#define BORNOMALA_SHORTCUTS_H
#include <stdint.h>
#include <stdbool.h>
/* Returns true when the key combination was a system shortcut and has been handled. */
bool shortcuts_dispatch(uint16_t key, uint8_t mods);
#endif
