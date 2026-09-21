#include "mouse.h"
#include "input.h"
#include "interrupts.h"
#include "ps2.h"

static bool     present;
static uint8_t  packet[3];
static uint8_t  idx;
static uint64_t last_byte_tick;

bool mouse_present(void) { return present; }

bool mouse_init(void) {
    idx = 0;
    present = false;
    if (!ps2_write_aux(0xF6)) return false;      /* set defaults (100 Hz, 1:1, 4 counts/mm) */
    if (!ps2_write_aux(0xF4)) return false;      /* enable data reporting */
    present = true;
    return true;
}

void mouse_feed_byte(uint8_t b) {
    if (!present) return;

    /* Resynchronise: a half-received packet older than 100 ms is garbage. */
    uint64_t now = timer_ticks();
    if (idx != 0 && now - last_byte_tick > 10) idx = 0;
    last_byte_tick = now;

    if (idx == 0 && !(b & 0x08)) return;         /* bit 3 is always 1 in byte 0 */
    packet[idx++] = b;
    if (idx < 3) return;
    idx = 0;

    uint8_t st = packet[0];
    if (st & 0xC0) return;                       /* X/Y overflow: discard packet */

    int dx = (int)packet[1] - (int)((st << 4) & 0x100);   /* 9-bit two's complement */
    int dy = (int)packet[2] - (int)((st << 3) & 0x100);

    input_event_t ev = { 0 };
    ev.type    = INPUT_MOUSE;
    ev.dx      = (int16_t)dx;
    ev.dy      = (int16_t)(-dy);                 /* PS/2 +y is up; screen +y is down */
    ev.buttons = (uint8_t)(st & 0x07);
    input_push(&ev);
}
