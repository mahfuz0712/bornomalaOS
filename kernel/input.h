/* kernel/input.h - unified keyboard + mouse event queue (IRQ producer, main-loop consumer) */
#ifndef BORNOMALA_INPUT_H
#define BORNOMALA_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Key codes: printable keys are plain ASCII; everything else is >= 0x100. */
#define KEY_ESC        27
#define KEY_BACKSPACE  '\b'
#define KEY_TAB        '\t'
#define KEY_ENTER      '\n'
#define KEY_UP         0x101
#define KEY_DOWN       0x102
#define KEY_LEFT       0x103
#define KEY_RIGHT      0x104
#define KEY_HOME       0x105
#define KEY_END        0x106
#define KEY_DELETE     0x107
#define KEY_PAGEUP     0x108
#define KEY_PAGEDOWN   0x109
#define KEY_INSERT     0x10A
#define KEY_SUPER      0x120      /* Windows / Command key */
#define KEY_F1         0x130      /* F1..F12 = KEY_F1 + 0..11 */

#define MOD_SHIFT 0x01
#define MOD_CTRL  0x02
#define MOD_ALT   0x04

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

typedef enum { INPUT_NONE = 0, INPUT_KEY = 1, INPUT_MOUSE = 2 } input_type_t;

typedef struct {
    uint8_t  type;      /* input_type_t */
    uint8_t  mods;      /* MOD_* (keys) */
    uint16_t key;       /* KEY_* or ASCII (keys) */
    int16_t  dx, dy;    /* relative motion, +y = DOWN (mouse) */
    uint8_t  buttons;   /* MOUSE_* bit mask after this packet (mouse) */
} input_event_t;

void input_push(const input_event_t *ev);      /* IRQ context */
bool input_pop(input_event_t *out);            /* main context; non-blocking */
bool input_pending(void);
void input_flush(void);

#endif
