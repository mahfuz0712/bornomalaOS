#include "power.h"
#include "cpu.h"
#include "console.h"

void power_restart(void) {
    cpu_cli();
    /* 1) pulse the CPU reset line through the keyboard controller */
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++) {}
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 1000000; i++) {}
    /* 2) PCI reset control register (0xCF9) */
    outb(0xCF9, 0x06);
    for (volatile int i = 0; i < 1000000; i++) {}
    /* 3) last resort: load an empty IDT and raise an interrupt -> triple fault -> reset */
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) empty = { 0, 0 };
    __asm__ volatile("lidt %0; int3" : : "m"(empty));
    for (;;) cpu_hlt();
}

void power_shutdown(void) {
    cpu_cli();
    outw(0x604, 0x2000);       /* QEMU (q35 / modern i440fx ACPI PM) */
    outw(0xB004, 0x2000);      /* older QEMU / Bochs */
    outw(0x4004, 0x3400);      /* VirtualBox */
    outw(0x600, 0x34);         /* Cloud Hypervisor */
    console_puts("\nIt is now safe to turn off your computer.\n");
    for (;;) cpu_hlt();
}
