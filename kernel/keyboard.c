#include "keyboard.h"
#include "input.h"
#include "ps2.h"

static bool shift_l, shift_r, ctrl_l, ctrl_r, alt_l, alt_r, caps, e0_prefix;

/* Scancode set 1 make-code -> ASCII for codes 0x00..0x39. Adjacent string
   literals keep the \x escapes from swallowing the digits that follow. */
static const char sc_lower[0x3A + 1] =
    "\0\x1b" "1234567890-=\b\t"
    "qwertyuiop[]\n\0"
    "asdfghjkl;'`\0\\"
    "zxcvbnm,./\0*\0 ";
static const char sc_upper[0x3A + 1] =
    "\0\x1b" "!@#$%^&*()_+\b\t"
    "QWERTYUIOP{}\n\0"
    "ASDFGHJKL:\"~\0|"
    "ZXCVBNM<>?\0*\0 ";
static const char keypad[] = "789-456+1230.";      /* scancodes 0x47..0x53 */

uint8_t keyboard_mods(void) {
    return (uint8_t)(((shift_l || shift_r) ? MOD_SHIFT : 0) |
                     ((ctrl_l  || ctrl_r)  ? MOD_CTRL  : 0) |
                     ((alt_l   || alt_r)   ? MOD_ALT   : 0));
}

static void emit(uint16_t key) {
    input_event_t ev = { 0 };
    ev.type = INPUT_KEY;
    ev.key  = key;
    ev.mods = keyboard_mods();
    input_push(&ev);
}

void keyboard_init(void) {
    shift_l = shift_r = ctrl_l = ctrl_r = alt_l = alt_r = caps = e0_prefix = false;
    (void)ps2_write_kbd(0xF4);       /* enable scanning (harmless if already on) */
}

void keyboard_feed_scancode(uint8_t raw) {
    if (raw == 0xE0) { e0_prefix = true; return; }
    /* 0xE1 = Pause prefix, 0xFA = ACK, 0xFE = resend.  NOT 0xAA: with translation on that byte is the
       break code of Left Shift (0x2A | 0x80) and must reach the modifier tracking below. */
    if (raw == 0xE1 || raw == 0xFA || raw == 0xFE) return;

    bool release = (raw & 0x80) != 0;
    uint8_t sc = raw & 0x7F;
    bool ext = e0_prefix;
    e0_prefix = false;

    /* modifiers */
    if (!ext && sc == 0x2A) { shift_l = !release; return; }
    if (!ext && sc == 0x36) { shift_r = !release; return; }
    if (sc == 0x1D)         { if (ext) ctrl_r = !release; else ctrl_l = !release; return; }
    if (sc == 0x38)         { if (ext) alt_r  = !release; else alt_l  = !release; return; }
    if (!ext && sc == 0x3A) { if (!release) caps = !caps; return; }
    if (release) return;

    if (ext) {
        switch (sc) {
        case 0x48: emit(KEY_UP);       return;
        case 0x50: emit(KEY_DOWN);     return;
        case 0x4B: emit(KEY_LEFT);     return;
        case 0x4D: emit(KEY_RIGHT);    return;
        case 0x47: emit(KEY_HOME);     return;
        case 0x4F: emit(KEY_END);      return;
        case 0x53: emit(KEY_DELETE);   return;
        case 0x49: emit(KEY_PAGEUP);   return;
        case 0x51: emit(KEY_PAGEDOWN); return;
        case 0x52: emit(KEY_INSERT);   return;
        case 0x1C: emit('\n');         return;     /* keypad enter */
        case 0x35: emit('/');          return;     /* keypad slash */
        case 0x5B: case 0x5C: emit(KEY_SUPER); return;
        default: return;
        }
    }

    if (sc >= 0x3B && sc <= 0x44) { emit((uint16_t)(KEY_F1 + (sc - 0x3B))); return; }
    if (sc == 0x57) { emit(KEY_F1 + 10); return; }
    if (sc == 0x58) { emit(KEY_F1 + 11); return; }
    if (sc >= 0x47 && sc <= 0x53) { emit((uint16_t)keypad[sc - 0x47]); return; }

    if (sc <= 0x39) {
        bool shifted = shift_l || shift_r;
        char base = sc_lower[sc];
        if (base >= 'a' && base <= 'z') shifted ^= caps;   /* Caps Lock only affects letters */
        char c = shifted ? sc_upper[sc] : sc_lower[sc];
        if (c) emit((uint16_t)(uint8_t)c);
    }
}
