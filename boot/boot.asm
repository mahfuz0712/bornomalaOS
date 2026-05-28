; boot/boot.asm — multiboot2 + 64-bit long mode entry
; Uses flat labels (no dot-locals) to avoid NASM local-label issues

bits 32

; ── Multiboot2 header ──────────────────────────────────────────────
MAGIC       equ 0xE85250D6
ARCH        equ 0
HDR_LEN     equ (header_end - header_start)
CHECKSUM    equ -(MAGIC + ARCH + HDR_LEN)

section .multiboot2
align 8
header_start:
    dd MAGIC
    dd ARCH
    dd HDR_LEN
    dd CHECKSUM
    dw 0
    dw 0
    dd 8
header_end:

; ── BSS: stack + page tables ───────────────────────────────────────
section .bss
align 4096
pml4_table: resb 4096
pdp_table:  resb 4096
pd_table:   resb 4096
pt_table:   resb 4096 * 4

align 16
stack_bottom:
    resb 65536
stack_top:

; ── GDT for long mode ──────────────────────────────────────────────
section .rodata
align 8
gdt64:
    dq 0
gdt64_code_off: equ $ - gdt64
    dq (1<<44)|(1<<47)|(1<<41)|(1<<43)|(1<<53)
gdt64_data_off: equ $ - gdt64
    dq (1<<44)|(1<<47)|(1<<41)
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

; ── 32-bit entry (GRUB lands here) ─────────────────────────────────
section .text
global _start

_start:
    mov esp, stack_top
    mov edi, ebx                ; save multiboot2 info pointer

    ; verify CPUID is available
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je  cpu_error

    ; verify long mode is available
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb  cpu_error
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz  cpu_error

    call setup_paging
    call enable_long_mode

    lgdt [gdt64_ptr]
    jmp  gdt64_code_off:long_mode_entry   ; far jump → flushes pipeline

cpu_error:
    cli
    hlt
    jmp cpu_error

; ── Identity-map first 8 MB (4 KB pages) ───────────────────────────
setup_paging:
    ; PML4[0] → PDP
    mov eax, pdp_table
    or  eax, 0x3
    mov [pml4_table], eax

    ; PDP[0] → PD
    mov eax, pd_table
    or  eax, 0x3
    mov [pdp_table], eax

    ; PD[0..3] → four page tables
    mov ecx, 0
fill_pd:
    mov eax, 4096
    imul eax, ecx
    add eax, pt_table
    or  eax, 0x3
    mov [pd_table + ecx*8], eax
    inc ecx
    cmp ecx, 4
    jl  fill_pd

    ; fill all PT entries: frame address | present+writable
    mov ecx, 0
fill_pt:
    mov eax, 0x1000
    mul ecx
    or  eax, 0x3
    mov [pt_table + ecx*8], eax
    inc ecx
    cmp ecx, 2048           ; 4 tables × 512 entries
    jl  fill_pt

    ret

; ── Enable PAE + EFER.LME + paging ────────────────────────────────
enable_long_mode:
    ; PAE in CR4
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; PML4 base into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; set EFER.LME via MSR 0xC0000080
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    ; enable paging in CR0
    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    ret

; ── 64-bit code (entered via far jump above) ───────────────────────
bits 64
extern kernel_main

long_mode_entry:
    ; reload segment registers with 64-bit data descriptor
    mov ax, gdt64_data_off
    mov ss, ax
    mov ds, ax
    mov es, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    ; rdi = multiboot2 info ptr (already set from edi, zero-extended by CPU)
    call kernel_main

    cli
hang:
    hlt
    jmp hang