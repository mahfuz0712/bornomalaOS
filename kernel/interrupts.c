#include "interrupts.h"
#include "cpu.h"
#include "klib.h"
#include "ps2.h"

#define IDT_COUNT 256
#define PIC1      0x20
#define PIC2      0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1
#define PIT_CMD   0x43
#define PIT_CH0   0x40
#define TIMER_HZ  100u

struct idt_entry {
    uint16_t off_lo, sel;
    uint8_t  ist, flags;
    uint16_t off_mid;
    uint32_t off_hi, zero;
} __attribute__((packed));

struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));

extern void *isr_stub_table[48];        /* boot/isr.asm: vectors 0..47 */

static struct idt_entry idt[IDT_COUNT] __attribute__((aligned(16)));
static volatile uint64_t ticks;

static void idt_set_gate(int n, void *handler) {
    uint64_t a = (uint64_t)(uintptr_t)handler;
    idt[n].off_lo  = (uint16_t)a;
    idt[n].sel     = 0x08;              /* 64-bit code segment from boot.asm */
    idt[n].ist     = 0;
    idt[n].flags   = 0x8E;              /* present, DPL0, 64-bit interrupt gate (IF cleared on entry) */
    idt[n].off_mid = (uint16_t)(a >> 16);
    idt[n].off_hi  = (uint32_t)(a >> 32);
    idt[n].zero    = 0;
}

static void pic_remap(void) {
    outb(PIC1, 0x11); io_wait(); outb(PIC2, 0x11); io_wait();     /* ICW1: init + ICW4 */
    outb(PIC1_DATA, 0x20); io_wait(); outb(PIC2_DATA, 0x28); io_wait(); /* ICW2: vector offsets 32 / 40 */
    outb(PIC1_DATA, 0x04); io_wait(); outb(PIC2_DATA, 0x02); io_wait(); /* ICW3: cascade on IRQ2 */
    outb(PIC1_DATA, 0x01); io_wait(); outb(PIC2_DATA, 0x01); io_wait(); /* ICW4: 8086 mode */
    /* Unmask: IRQ0 timer, IRQ1 keyboard, IRQ2 cascade, IRQ12 mouse. Everything else masked. */
    outb(PIC1_DATA, (uint8_t)~((1u << 0) | (1u << 1) | (1u << 2)));
    outb(PIC2_DATA, (uint8_t)~(1u << 4));
}

static void pit_init(uint32_t hz) {
    uint32_t divisor = 1193182u / hz;
    if (divisor < 1) divisor = 1;
    if (divisor > 65535) divisor = 65535;
    outb(PIT_CMD, 0x36);                /* channel 0, lo/hi, mode 3 (square wave) */
    outb(PIT_CH0, (uint8_t)divisor);
    outb(PIT_CH0, (uint8_t)(divisor >> 8));
}

static void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2, 0x20);
    outb(PIC1, 0x20);
}

/* IRQ7 / IRQ15 can be spurious: only acknowledge them when the PIC really has them in service. */
static bool pic_is_spurious(uint8_t irq) {
    if (irq == 7)  { outb(PIC1, 0x0B); return !(inb(PIC1) & 0x80); }
    if (irq == 15) {
        outb(PIC2, 0x0B);
        if (!(inb(PIC2) & 0x80)) { outb(PIC1, 0x20); return true; }   /* master still needs its EOI */
    }
    return false;
}

void interrupt_dispatch(interrupt_frame_t *f) {
    uint64_t v = f->vector;

    if (v < 32) { panic_exception(f); return; }      /* CPU exception: never returns */

    uint8_t irq = (uint8_t)(v - 32);
    if (pic_is_spurious(irq)) return;

    switch (irq) {
    case 0:  ticks++; break;
    case 1:
    case 12: ps2_irq_service(); break;
    default: break;
    }
    pic_eoi(irq);
}

void interrupts_init(void) {
    memset(idt, 0, sizeof(idt));
    for (int i = 0; i < 48; i++) idt_set_gate(i, isr_stub_table[i]);
    pic_remap();
    pit_init(TIMER_HZ);
    ticks = 0;
    struct idt_ptr p = { (uint16_t)(sizeof(idt) - 1), (uint64_t)(uintptr_t)idt };
    __asm__ volatile("lidt %0" : : "m"(p));
}

void interrupts_enable(void)  { cpu_sti(); }
void interrupts_disable(void) { cpu_cli(); }
uint64_t timer_ticks(void)    { return ticks; }
uint32_t timer_hz(void)       { return TIMER_HZ; }
