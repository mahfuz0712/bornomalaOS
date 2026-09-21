#include "console.h"
#include "cpu.h"
#include "klib.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define COM1       0x3F8

static volatile uint16_t *const vga = (volatile uint16_t *)VGA_MEMORY;
static size_t  t_row, t_col;
static uint8_t t_color = 0x07;
static int     serial_ok;

static uint16_t vga_entry(char c, uint8_t col) { return (uint16_t)((uint8_t)c | ((uint16_t)col << 8)); }

static void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* no interrupts */
    outb(COM1 + 3, 0x80);   /* DLAB on */
    outb(COM1 + 0, 0x01);   /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO */
    outb(COM1 + 4, 0x0B);
    /* loopback self-test so we do not spin on a machine without a UART */
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);
    if (inb(COM1 + 0) != 0xAE) { serial_ok = 0; return; }
    outb(COM1 + 4, 0x0F);
    serial_ok = 1;
}

static void serial_putc(char c) {
    if (!serial_ok) return;
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) {}
    outb(COM1, (uint8_t)c);
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', t_color);
}

static void vga_cursor(void) {
    uint16_t pos = (uint16_t)(t_row * VGA_WIDTH + t_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

void console_setcolor(vga_color_t fg, vga_color_t bg) { t_color = (uint8_t)(fg | (bg << 4)); }

void console_init(void) {
    serial_init();
    t_row = t_col = 0;
    t_color = 0x07;
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga[y * VGA_WIDTH + x] = vga_entry(' ', t_color);
    vga_cursor();
}

void console_putc(char c) {
    serial_putc(c);
    if (c == '\n') serial_putc('\r');
    if (c == '\r') { t_col = 0; return; }
    if (c == '\b') {
        if (t_col > 0) t_col--;
        vga[t_row * VGA_WIDTH + t_col] = vga_entry(' ', t_color);
        vga_cursor();
        return;
    }
    if (c == '\n') {
        t_col = 0;
        if (++t_row == VGA_HEIGHT) { vga_scroll(); t_row = VGA_HEIGHT - 1; }
        vga_cursor();
        return;
    }
    vga[t_row * VGA_WIDTH + t_col] = vga_entry(c, t_color);
    if (++t_col == VGA_WIDTH) {
        t_col = 0;
        if (++t_row == VGA_HEIGHT) { vga_scroll(); t_row = VGA_HEIGHT - 1; }
    }
    vga_cursor();
}

void console_puts(const char *s) { while (*s) console_putc(*s++); }

/* ── printf core: writes through a tiny sink so kprintf and ksnprintf share it ── */
typedef struct { char *buf; size_t size, len; int to_console; } sink_t;

static void sink_put(sink_t *s, char c) {
    if (s->to_console) { console_putc(c); return; }
    if (s->len + 1 < s->size) s->buf[s->len] = c;
    s->len++;
}

static void put_number(sink_t *s, uint64_t v, unsigned base, bool neg, int width, bool zero, bool upper) {
    char tmp[32];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = digits[v % base]; v /= base; }
    int total = n + (neg ? 1 : 0);
    if (neg && zero) sink_put(s, '-');
    for (int i = total; i < width; i++) sink_put(s, zero ? '0' : ' ');
    if (neg && !zero) sink_put(s, '-');
    while (n) sink_put(s, tmp[--n]);
}

static void vformat(sink_t *s, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { sink_put(s, *fmt); continue; }
        fmt++;
        bool zero = false;
        int width = 0, lng = 0;
        if (*fmt == '0') { zero = true; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        while (*fmt == 'l' || *fmt == 'z') { lng++; fmt++; }
        switch (*fmt) {
        case 'c': sink_put(s, (char)va_arg(ap, int)); break;
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            int len = (int)k_strlen(str);
            for (int i = len; i < width; i++) sink_put(s, ' ');
            while (*str) sink_put(s, *str++);
            break;
        }
        case 'd': case 'i': {
            int64_t v = lng ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int);
            bool neg = v < 0;
            put_number(s, neg ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v, 10, neg, width, zero, false);
            break;
        }
        case 'u': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int);
            put_number(s, v, 10, false, width, zero, false);
            break;
        }
        case 'x': case 'X': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int);
            put_number(s, v, 16, false, width, zero, *fmt == 'X');
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            sink_put(s, '0'); sink_put(s, 'x');
            put_number(s, v, 16, false, 16, true, false);
            break;
        }
        case '%': sink_put(s, '%'); break;
        case '\0': return;
        default: sink_put(s, '?'); break;
        }
    }
}

void kvprintf(const char *fmt, va_list ap) {
    sink_t s = { 0, 0, 0, 1 };
    vformat(&s, fmt, ap);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    sink_t s = { buf, size, 0, 0 };
    va_start(ap, fmt);
    vformat(&s, fmt, ap);
    va_end(ap);
    if (size) buf[s.len < size ? s.len : size - 1] = '\0';
    return (int)s.len;
}

void kpanic(const char *fmt, ...) {
    va_list ap;
    cpu_cli();
    console_setcolor(VGA_WHITE, VGA_RED);
    console_puts("\nKERNEL PANIC: ");
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    console_puts("\n");
    for (;;) cpu_hlt();
}
