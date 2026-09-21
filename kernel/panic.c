/* Exception handler + "blue screen". Deliberately self-contained: it must work even
   when the heap, the compositor or the rest of the GUI is in a broken state, so it only
   uses the raw framebuffer and the 8x16 bitmap font. */
#include "panic.h"
#include "interrupts.h"
#include "console.h"
#include "cpu.h"
#include "klib.h"
#include "gui/font.h"

static mb2_framebuffer_t fb;
static bool have_fb;

void panic_register_framebuffer(const mb2_framebuffer_t *f) {
    fb = *f;
    have_fb = f->available;
}

static const char *const exc_names[32] = {
    "Divide error (#DE)", "Debug (#DB)", "Non-maskable interrupt", "Breakpoint (#BP)",
    "Overflow (#OF)", "Bound range exceeded (#BR)", "Invalid opcode (#UD)", "Device not available (#NM)",
    "Double fault (#DF)", "Coprocessor segment overrun", "Invalid TSS (#TS)", "Segment not present (#NP)",
    "Stack-segment fault (#SS)", "General protection fault (#GP)", "Page fault (#PF)", "Reserved",
    "x87 floating-point (#MF)", "Alignment check (#AC)", "Machine check (#MC)", "SIMD floating-point (#XM)",
    "Virtualization (#VE)", "Control protection (#CP)", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor injection", "VMM communication", "Security (#SX)", "Reserved"
};

static void fb_rect(int x, int y, int w, int h, uint32_t color) {
    for (int yy = y; yy < y + h && yy < (int)fb.height; yy++) {
        if (yy < 0) continue;
        uint32_t *row = (uint32_t *)(uintptr_t)(fb.address + (uint64_t)yy * fb.pitch);
        for (int xx = x; xx < x + w && xx < (int)fb.width; xx++)
            if (xx >= 0) row[xx] = color;
    }
}

static void fb_text(int x, int y, const char *s, uint32_t color) {
    for (; *s; s++, x += 8) {
        unsigned char ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) continue;
        const uint8_t *glyph = font_mono8x16[ch - 32];
        for (int gy = 0; gy < 16; gy++) {
            uint32_t *row = (uint32_t *)(uintptr_t)(fb.address + (uint64_t)(y + gy) * fb.pitch);
            for (int gx = 0; gx < 8; gx++)
                if ((glyph[gy] & (0x80u >> gx)) && x + gx < (int)fb.width) row[x + gx] = color;
        }
    }
}

void panic_exception(const interrupt_frame_t *f) {
    cpu_cli();
    const char *name = f->vector < 32 ? exc_names[f->vector] : "Unknown";

    /* Serial / VGA text first: always available. */
    console_setcolor(VGA_WHITE, VGA_RED);
    kprintf("\n*** BornomalaOS kernel exception %u: %s\n", (unsigned)f->vector, name);
    kprintf("    error=%llx rip=%p cs=%llx rflags=%llx rsp=%p cr2=%p\n",
            (unsigned long long)f->error, (void *)f->rip, (unsigned long long)f->cs,
            (unsigned long long)f->rflags, (void *)f->rsp, (void *)read_cr2());

    if (have_fb) {
        const uint32_t bg = 0x000B3D6B, fg = 0x00FFFFFF, dim = 0x00A9CDEA;
        fb_rect(0, 0, (int)fb.width, (int)fb.height, bg);
        int x = 40, y = 40;
        fb_text(x, y, "BornomalaOS - a fatal kernel error occurred", fg); y += 32;
        char line[128];
        ksnprintf(line, sizeof line, "Exception %u: %s", (unsigned)f->vector, name);
        fb_text(x, y, line, fg); y += 24;
        ksnprintf(line, sizeof line, "error code %016llx   cr2 %016llx",
                  (unsigned long long)f->error, (unsigned long long)read_cr2());
        fb_text(x, y, line, dim); y += 32;
        ksnprintf(line, sizeof line, "RIP %016llx   RSP %016llx   RFLAGS %016llx",
                  (unsigned long long)f->rip, (unsigned long long)f->rsp, (unsigned long long)f->rflags);
        fb_text(x, y, line, fg); y += 24;
        ksnprintf(line, sizeof line, "RAX %016llx   RBX %016llx   RCX %016llx",
                  (unsigned long long)f->rax, (unsigned long long)f->rbx, (unsigned long long)f->rcx);
        fb_text(x, y, line, dim); y += 20;
        ksnprintf(line, sizeof line, "RDX %016llx   RSI %016llx   RDI %016llx",
                  (unsigned long long)f->rdx, (unsigned long long)f->rsi, (unsigned long long)f->rdi);
        fb_text(x, y, line, dim); y += 20;
        ksnprintf(line, sizeof line, "RBP %016llx   R8  %016llx   R9  %016llx",
                  (unsigned long long)f->rbp, (unsigned long long)f->r8, (unsigned long long)f->r9);
        fb_text(x, y, line, dim); y += 32;
        fb_text(x, y, "The system has been halted. Restart the machine to continue.", fg);
    }
    for (;;) cpu_hlt();
}
