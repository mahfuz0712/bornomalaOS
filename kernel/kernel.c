/* kernel/kernel.c - BornomalaOS 64-bit kernel entry (Phase 5: visual GUI foundation)
 *
 * Boot order:  console -> multiboot2 -> memory -> heap -> accounts -> interrupts ->
 *              PS/2 keyboard + mouse -> graphical session (lock screen -> desktop).
 */
#include <stdint.h>
#include <stdbool.h>
#include "alloc.h"
#include "console.h"
#include "cpu.h"
#include "input.h"
#include "interrupts.h"
#include "keyboard.h"
#include "klib.h"
#include "lockscreen.h"
#include "mb2.h"
#include "memory.h"
#include "mouse.h"
#include "ps2.h"
#include "users.h"
#include "gui/session.h"

#define HEAP_PAGES 1024                     /* 4 MiB kernel heap */

/* Text-mode login used only when GRUB could not give us a framebuffer. */
static void text_mode_session(void) {
    console_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    console_puts("\nBornomalaOS\n");
    console_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    console_puts("No framebuffer was provided by the bootloader: running the text-mode login.\n");
    const bm_user_t *u = users_current();
    kprintf("User: %s\n", u ? u->display_name : "unknown");

    char pw[64];
    for (;;) {
        kprintf("Password: ");
        size_t n = 0;
        for (;;) {
            input_event_t ev;
            cpu_cli();
            if (!input_pop(&ev)) { cpu_idle(); continue; }
            cpu_sti();
            if (ev.type != INPUT_KEY) continue;
            if (ev.key == '\n') break;
            if (ev.key == KEY_BACKSPACE) { if (n) { n--; console_putc('\b'); } continue; }
            if (ev.key >= 32 && ev.key < 127 && n < sizeof(pw) - 1) { pw[n++] = (char)ev.key; console_putc('*'); }
        }
        pw[n] = '\0';
        console_putc('\n');
        bool ok = lockscreen_try_unlock(pw);
        memset(pw, 0, sizeof pw);
        if (ok) break;
        kprintf("Incorrect password.\n");
    }
    console_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Session authenticated. The desktop needs a framebuffer, so the system idles here.\n");
    for (;;) cpu_idle();
}

void kernel_main(uint64_t mbi_addr, uint32_t magic) {
    console_init();
    kprintf("BornomalaOS Phase 5 starting...\n");

    boot_info_t bi;
    if (!mb2_parse(mbi_addr, magic, &bi))
        kpanic("not started by a Multiboot2 bootloader (magic=%x, info=%p)", magic, (void *)(uintptr_t)mbi_addr);

    memory_init(&bi);
    kprintf("memory: %llu KiB usable, %llu KiB free\n",
            (unsigned long long)(memory_usable_bytes() / 1024), (unsigned long long)(memory_free_bytes() / 1024));

    void *heap = pmm_alloc_contiguous(HEAP_PAGES);
    if (!heap) kpanic("cannot allocate the kernel heap");
    alloc_init(heap, (size_t)HEAP_PAGES * PAGE_SIZE);

    users_init();
    lockscreen_init();

    interrupts_init();
    ps2_init();
    keyboard_init();
    bool have_mouse = mouse_init();
    kprintf("input: keyboard ok, mouse %s\n", have_mouse ? "ok" : "not found");
    interrupts_enable();

    if (bi.fb.available) {
        if (!session_run(&bi.fb)) kprintf("gui: could not start, falling back to text mode\n");
    } else {
        kprintf("gui: no usable framebuffer (need 32-bpp direct colour)\n");
    }
    text_mode_session();
    for (;;) cpu_hlt();
}
