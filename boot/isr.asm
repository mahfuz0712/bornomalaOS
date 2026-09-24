; boot/isr.asm - interrupt / exception entry stubs
;
; Every stub leaves the stack as:   [vector][error code][rip][cs][rflags][rsp][ss]
; (CPU exceptions without an error code get a dummy 0), then jumps to isr_common,
; which saves the general registers and calls  interrupt_dispatch(interrupt_frame_t *).

bits 64
default rel
extern interrupt_dispatch

section .text

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push qword %1
    jmp isr_common
%endmacro

; Vectors 0..47: 0-31 CPU exceptions, 32-47 the remapped PIC IRQs.
%assign v 0
%rep 48
  %if (v == 8) || (v == 10) || (v == 11) || (v == 12) || (v == 13) || (v == 14) || (v == 17) || (v == 21) || (v == 29) || (v == 30)
    ISR_ERR v
  %else
    ISR_NOERR v
  %endif
  %assign v v+1
%endrep

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    cld
    mov rdi, rsp                    ; -> interrupt_frame_t
    call interrupt_dispatch         ; rsp is 16-byte aligned here (CPU aligns + 7 + 15 qword pushes)

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16                     ; drop vector + error code
    iretq

section .rodata
align 8
global isr_stub_table
isr_stub_table:
%assign v 0
%rep 48
    dq isr_stub_ %+ v
%assign v v+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
