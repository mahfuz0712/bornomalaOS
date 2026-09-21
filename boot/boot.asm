; boot/boot.asm - Multiboot2 entry, 32-bit -> 64-bit long mode transition
;
; GRUB enters _start in 32-bit protected mode with
;     eax = 0x36D76289 (multiboot2 magic), ebx = physical address of boot info.
; We build identity-mapped page tables for the first 4 GiB (2 MiB pages), switch to
; long mode and call  kernel_main(rdi = boot info, rsi = magic).

bits 32

; Preferred video mode requested from GRUB (a hint - GRUB may pick another one).
FB_WIDTH    equ 1280
FB_HEIGHT   equ 720
FB_DEPTH    equ 32

MB2_MAGIC   equ 0xE85250D6
MB2_ARCH    equ 0                       ; i386 protected mode
MB2_LEN     equ (mb2_header_end - mb2_header_start)

; ── Multiboot2 header (must lie in the first 32 KiB of the image) ───────────
section .multiboot2 align=8
mb2_header_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LEN
    dd 0x100000000 - (MB2_MAGIC + MB2_ARCH + MB2_LEN)   ; checksum: sum of all four == 0 (mod 2^32)

    align 8
    dw 5                                ; framebuffer request tag
    dw 1                                ; flags: 1 = optional (kernel falls back to text mode)
    dd 20                               ; tag size
    dd FB_WIDTH
    dd FB_HEIGHT
    dd FB_DEPTH

    align 8
    dw 0                                ; end tag
    dw 0
    dd 8
mb2_header_end:

; ── BSS: page tables + stack ────────────────────────────────────────────────
section .bss align=4096
pml4_table:  resb 4096
pdp_table:   resb 4096
pd_table:    resb 4096 * 4              ; four page directories = 4 GiB of 2 MiB pages
align 16
stack_bottom:
    resb 65536
stack_top:

; ── Read-only data: 64-bit GDT ──────────────────────────────────────────────
section .rodata
align 8
gdt64:
    dq 0                                                        ; null
gdt64_code: equ $ - gdt64
    dq (1<<44) | (1<<47) | (1<<41) | (1<<43) | (1<<53)         ; code: S P RW X L
gdt64_data: equ $ - gdt64
    dq (1<<44) | (1<<47) | (1<<41)                              ; data: S P RW
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

; ── 32-bit entry ────────────────────────────────────────────────────────────
section .text
global _start

_start:
    cli
    cld
    mov esp, stack_top
    mov edi, ebx                        ; -> rdi: multiboot2 info pointer
    mov esi, eax                        ; -> rsi: multiboot2 magic

    cmp eax, 0x36D76289
    mov al, 'M'
    jne boot_error                      ; not loaded by a multiboot2 bootloader

    ; CPUID available? (flip EFLAGS.ID, bit 21)
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
    xor eax, ecx
    mov al, 'C'
    jz boot_error

    ; Long mode available?
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    mov al, 'L'
    jb boot_error
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    mov al, 'L'
    jz boot_error

    call setup_paging
    call enable_long_mode

    lgdt [gdt64_ptr]
    jmp gdt64_code:long_mode_entry

; Show "ERR x" on the VGA text screen and stop. al = error letter.
boot_error:
    mov dword [0xB8000], 0x4F524F45     ; "ER" white on red
    mov dword [0xB8004], 0x4F204F52     ; "R "
    mov ah, 0x4F
    mov word [0xB8008], ax
.halt:
    hlt
    jmp .halt

; ── Identity-map 4 GiB with 2 MiB pages ─────────────────────────────────────
setup_paging:
    mov eax, pdp_table
    or  eax, 0x3                        ; present | writable
    mov [pml4_table], eax

    xor ecx, ecx
.fill_pdp:                              ; PDP[i] -> PD i (1 GiB each)
    mov eax, ecx
    shl eax, 12                         ; i * 4096
    add eax, pd_table
    or  eax, 0x3
    mov [pdp_table + ecx*8], eax
    inc ecx
    cmp ecx, 4
    jb  .fill_pdp

    xor ecx, ecx
.fill_pd:                               ; 2048 entries, 2 MiB each; index i lives at pd_table + i*8
    mov eax, ecx
    shl eax, 21                         ; physical address = i * 2 MiB
    or  eax, 0x83                       ; present | writable | page-size(2 MiB)
    cmp ecx, 1536                       ; >= 3 GiB: PCI / framebuffer window
    jb  .store
    or  eax, 0x18                       ; PWT | PCD -> uncached, so pixels reach the display promptly
.store:
    mov [pd_table + ecx*8], eax
    inc ecx
    cmp ecx, 2048
    jb  .fill_pd
    ret

enable_long_mode:
    mov eax, cr4
    or  eax, 1 << 5                     ; PAE
    mov cr4, eax

    mov eax, pml4_table
    mov cr3, eax

    mov ecx, 0xC0000080                 ; EFER
    rdmsr
    or  eax, 1 << 8                     ; LME
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31                    ; PG
    mov cr0, eax
    ret

; ── 64-bit ──────────────────────────────────────────────────────────────────
bits 64
extern kernel_main

long_mode_entry:
    mov ax, gdt64_data
    mov ss, ax
    mov ds, ax
    mov es, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax

    mov esp, stack_top                  ; writing esp zero-extends: rsp = stack_top
    mov edi, edi                        ; make sure the upper halves are clean
    mov esi, esi
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
