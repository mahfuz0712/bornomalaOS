/* kernel/interrupts.h - IDT, PIC, PIT timer and exception handling */
#ifndef BORNOMALA_INTERRUPTS_H
#define BORNOMALA_INTERRUPTS_H

#include <stdint.h>
#include <stdbool.h>

/* Layout MUST match the push order in boot/isr.asm (r15 is pushed last). */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

void     interrupts_init(void);
void     interrupts_enable(void);
void     interrupts_disable(void);

uint64_t timer_ticks(void);        /* 100 Hz */
uint32_t timer_hz(void);

/* Implemented in panic.c */
void     panic_exception(const interrupt_frame_t *f);
void     interrupt_dispatch(interrupt_frame_t *f);

#endif
