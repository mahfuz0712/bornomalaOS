#include "ps2.h"
#include "cpu.h"
#include "keyboard.h"
#include "mouse.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define ST_OUT_FULL 0x01
#define ST_IN_FULL  0x02
#define ST_AUX      0x20

static bool wait_write(void) {
    for (int i = 0; i < 100000; i++) if (!(inb(PS2_STATUS) & ST_IN_FULL)) return true;
    return false;
}
static bool wait_read(void) {
    for (int i = 0; i < 100000; i++) if (inb(PS2_STATUS) & ST_OUT_FULL) return true;
    return false;
}
static void flush_output(void) {
    for (int i = 0; i < 64 && (inb(PS2_STATUS) & ST_OUT_FULL); i++) { (void)inb(PS2_DATA); io_wait(); }
}

static bool send_and_ack(bool aux, uint8_t byte) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (aux) { if (!wait_write()) return false; outb(PS2_CMD, 0xD4); }
        if (!wait_write()) return false;
        outb(PS2_DATA, byte);
        for (int i = 0; i < 4; i++) {                 /* the device may send junk before the ACK */
            if (!wait_read()) break;
            uint8_t r = inb(PS2_DATA);
            if (r == 0xFA) return true;               /* ACK */
            if (r != 0xFE) continue;                  /* 0xFE = resend, anything else: keep reading */
            break;
        }
    }
    return false;
}

bool ps2_write_kbd(uint8_t byte) { return send_and_ack(false, byte); }
bool ps2_write_aux(uint8_t byte) { return send_and_ack(true, byte); }

void ps2_init(void) {
    /* Quiesce both ports while we reconfigure. */
    if (wait_write()) outb(PS2_CMD, 0xAD);
    if (wait_write()) outb(PS2_CMD, 0xA7);
    flush_output();

    /* Config byte: bit0 kbd IRQ, bit1 aux IRQ, bit4/5 port clocks (0 = enabled),
       bit6 scancode translation to set 1 (what keyboard.c decodes). */
    uint8_t cfg = 0x47;
    if (wait_write()) {
        outb(PS2_CMD, 0x20);
        if (wait_read()) cfg = inb(PS2_DATA);
    }
    cfg |= (1u << 0) | (1u << 1) | (1u << 6);
    cfg &= (uint8_t)~((1u << 4) | (1u << 5));
    if (wait_write()) { outb(PS2_CMD, 0x60); if (wait_write()) outb(PS2_DATA, cfg); }

    if (wait_write()) outb(PS2_CMD, 0xAE);            /* enable keyboard port */
    if (wait_write()) outb(PS2_CMD, 0xA8);            /* enable aux (mouse) port */
    flush_output();
}

/* Both IRQ1 and IRQ12 land here. The status register tells us which device
   produced the byte, so a mouse byte can never be mistaken for a key. */
void ps2_irq_service(void) {
    for (int guard = 0; guard < 16; guard++) {
        uint8_t st = inb(PS2_STATUS);
        if (!(st & ST_OUT_FULL)) return;
        uint8_t data = inb(PS2_DATA);
        if (st & ST_AUX) mouse_feed_byte(data);
        else             keyboard_feed_scancode(data);
    }
}
