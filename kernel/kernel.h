/* kernel/kernel.h */
#ifndef KERNEL_H
#define KERNEL_H
#include "disk.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── VGA ─────────────────────────────────────────────────────────── */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

typedef enum {
    VGA_BLACK=0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
} vga_color_t;

void terminal_init(void);
void terminal_putchar(char c);
void terminal_writestring(const char *s);
void terminal_setcolor(vga_color_t fg, vga_color_t bg);
void terminal_writehex(uint64_t n);
void terminal_writedec(int64_t n);

/* ── PS/2 Keyboard ───────────────────────────────────────────────── */
void keyboard_init(void);
char keyboard_getchar(void);      /* blocking — waits for a keypress  */

/* ── Our scanf / printf replacements ────────────────────────────── */
void  kprintf(const char *fmt, ...);
int   kscanf(const char *fmt, ...);  /* supports %s %d %u %c %x     */
char *kgets(char *buf, int max);     /* read a line (echo + backspace) */

/* ── Memory Allocator ─────────────────────────────────────────────── */
void alloc_init(void *heap_start, size_t heap_size);
void *kmalloc(size_t size);
void kfree(void *ptr);

/* ── ATA Disk Driver ──────────────────────────────────────────────── */
void ata_init(void);
bool ata_read_sector(uint32_t lba, uint16_t count, uint8_t *buffer);
bool ata_write_sector(uint32_t lba, uint16_t count, uint8_t *buffer);

/* ── ext4 File System ─────────────────────────────────────────────── */
bool ext4_init(void);

#endif