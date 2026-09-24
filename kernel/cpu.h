/* kernel/cpu.h - port I/O and CPU helpers (one copy for the whole kernel) */
#ifndef BORNOMALA_CPU_H
#define BORNOMALA_CPU_H

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outw(uint16_t port, uint16_t v) {
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}
static inline void insw(uint16_t port, void *dst, uint32_t count) {
    __asm__ volatile("rep insw" : "+D"(dst), "+c"(count) : "d"(port) : "memory");
}
static inline void outsw(uint16_t port, const void *src, uint32_t count) {
    __asm__ volatile("rep outsw" : "+S"(src), "+c"(count) : "d"(port) : "memory");
}
static inline void io_wait(void) { outb(0x80, 0); }

static inline void cpu_cli(void) { __asm__ volatile("cli" ::: "memory"); }
static inline void cpu_sti(void) { __asm__ volatile("sti" ::: "memory"); }
static inline void cpu_hlt(void) { __asm__ volatile("hlt" ::: "memory"); }

/* Save RFLAGS and disable interrupts; restore with irq_restore(). */
static inline uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
static inline void irq_restore(uint64_t flags) {
    __asm__ volatile("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
}

static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

/* Atomically enable interrupts and halt: no lost-wakeup window between the
   "nothing to do" check and the hlt. */
static inline void cpu_idle(void) { __asm__ volatile("sti; hlt" ::: "memory"); }

#endif
