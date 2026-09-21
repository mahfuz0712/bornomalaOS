/* kernel/console.h - VGA text console + COM1 serial log + kprintf */
#ifndef BORNOMALA_CONSOLE_H
#define BORNOMALA_CONSOLE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
} vga_color_t;

void console_init(void);
void console_setcolor(vga_color_t fg, vga_color_t bg);
void console_putc(char c);
void console_puts(const char *s);

/* Text goes to the serial port (always) and the VGA text buffer (when the
   machine is still in text mode). Supported: %c %s %d %i %u %x %X %p %%
   with flags '0', width, and l / ll / z length modifiers. */
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);
int  ksnprintf(char *buf, size_t size, const char *fmt, ...);

/* Stop the world: print a message and halt forever. */
void kpanic(const char *fmt, ...) __attribute__((noreturn));

#endif
