<<<<<<< HEAD
# BornomalaOS - Phase 5 build
#
#   make          build mykernel.bin
#   make iso      build bornomalaOS.iso (needs grub-mkrescue + xorriso)
#   make run      build the ISO and boot it in QEMU
#   make clean
#
# Toolchain: gcc (x86_64), nasm, ld, and for the ISO grub-mkrescue/xorriso.
# On Debian/Ubuntu:  sudo apt install build-essential nasm grub-pc-bin xorriso mtools qemu-system-x86
# A plain "gcc"/"ld" is used automatically when x86_64-linux-gnu-* is not installed.

CROSS ?= $(shell command -v x86_64-linux-gnu-gcc >/dev/null 2>&1 && echo x86_64-linux-gnu- || echo)
CC    = $(CROSS)gcc
LD    = $(CROSS)ld
AS    = nasm
GRUB  = grub-mkrescue
QEMU ?= qemu-system-x86_64

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-stack-protector -fno-pic -fno-pie -fno-common -fno-asynchronous-unwind-tables \
         -mno-red-zone -mgeneral-regs-only -Ikernel -Ikernel/gui

LDFLAGS = -nostdlib -static -T boot/linker.ld -z max-page-size=0x1000 -z noexecstack -z noseparate-code

ASM_OBJS = boot/boot.o boot/isr.o

C_OBJS = kernel/kernel.o kernel/klib.o kernel/console.o kernel/mb2.o kernel/memory.o kernel/alloc.o \
         kernel/interrupts.o kernel/input.o kernel/ps2.o kernel/keyboard.o kernel/mouse.o \
         kernel/rtc.o kernel/users.o kernel/lockscreen.o kernel/power.o kernel/panic.o kernel/disk.o \
         kernel/gui/gfx.o kernel/gui/font_data.o kernel/gui/font_mono.o kernel/gui/icons.o \
         kernel/gui/compositor.o kernel/gui/menu.o kernel/gui/wm.o kernel/gui/apps.o \
         kernel/gui/lock.o kernel/gui/desktop.o kernel/gui/session.o \
         kernel/gui/uistate.o kernel/gui/notify.o kernel/gui/shortcuts.o kernel/gui/sysscreen.o kernel/clipboard.o kernel/bootmode.o

OBJS = $(ASM_OBJS) $(C_OBJS)
DEPS = $(C_OBJS:.o=.d)

.PHONY: all iso run clean check
all: mykernel.bin

boot/%.o: boot/%.asm
	$(AS) -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

mykernel.bin: $(OBJS) boot/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Optional: iso/boot/grub/themes is only populated when a grub-theme/ directory exists.
iso: mykernel.bin
	cp mykernel.bin iso/boot/mykernel.bin
	@if [ -d grub-theme ]; then \
	    mkdir -p iso/boot/grub/themes/bornomala/select_bkg; \
	    cp grub-theme/theme.txt grub-theme/logo.png grub-theme/splash.png iso/boot/grub/themes/bornomala/; \
	    cp grub-theme/select_bkg/*.png iso/boot/grub/themes/bornomala/select_bkg/; \
	fi
	$(GRUB) -o bornomalaOS.iso iso/

run: iso
	./run-qemu.sh

# Verifies the image is a valid Multiboot2 kernel (needs grub-file from grub-common).
check: mykernel.bin
	grub-file --is-x86-multiboot2 mykernel.bin && echo "multiboot2 header OK"

clean:
	rm -f $(OBJS) $(DEPS) mykernel.bin bornomalaOS.iso iso/boot/mykernel.bin
	rm -rf iso/boot/grub/themes

-include $(DEPS)
=======
# BornomalaOS - Phase 5 build
#
#   make          build mykernel.bin
#   make iso      build bornomalaOS.iso (needs grub-mkrescue + xorriso)
#   make run      build the ISO and boot it in QEMU
#   make clean
#
# Toolchain: gcc (x86_64), nasm, ld, and for the ISO grub-mkrescue/xorriso.
# On Debian/Ubuntu:  sudo apt install build-essential nasm grub-pc-bin xorriso mtools qemu-system-x86
# A plain "gcc"/"ld" is used automatically when x86_64-linux-gnu-* is not installed.

CROSS ?= $(shell command -v x86_64-linux-gnu-gcc >/dev/null 2>&1 && echo x86_64-linux-gnu- || echo)
CC    = $(CROSS)gcc
LD    = $(CROSS)ld
AS    = nasm
GRUB  = grub-mkrescue
QEMU ?= qemu-system-x86_64

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-stack-protector -fno-pic -fno-pie -fno-common -fno-asynchronous-unwind-tables \
         -mno-red-zone -mgeneral-regs-only -Ikernel -Ikernel/gui

LDFLAGS = -nostdlib -static -T boot/linker.ld -z max-page-size=0x1000 -z noexecstack -z noseparate-code

ASM_OBJS = boot/boot.o boot/isr.o

C_OBJS = kernel/kernel.o kernel/klib.o kernel/console.o kernel/mb2.o kernel/memory.o kernel/alloc.o \
         kernel/interrupts.o kernel/input.o kernel/ps2.o kernel/keyboard.o kernel/mouse.o \
         kernel/rtc.o kernel/users.o kernel/lockscreen.o kernel/power.o kernel/panic.o kernel/disk.o \
         kernel/gui/gfx.o kernel/gui/font_data.o kernel/gui/font_mono.o kernel/gui/icons.o \
         kernel/gui/compositor.o kernel/gui/menu.o kernel/gui/wm.o kernel/gui/apps.o \
         kernel/gui/lock.o kernel/gui/desktop.o kernel/gui/session.o

OBJS = $(ASM_OBJS) $(C_OBJS)
DEPS = $(C_OBJS:.o=.d)

.PHONY: all iso run clean check
all: mykernel.bin

boot/%.o: boot/%.asm
	$(AS) -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

mykernel.bin: $(OBJS) boot/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Optional: iso/boot/grub/themes is only populated when a grub-theme/ directory exists.
iso: mykernel.bin
	cp mykernel.bin iso/boot/mykernel.bin
	@if [ -d grub-theme ]; then \
	    mkdir -p iso/boot/grub/themes/bornomala/select_bkg; \
	    cp grub-theme/theme.txt grub-theme/logo.png grub-theme/splash.png iso/boot/grub/themes/bornomala/; \
	    cp grub-theme/select_bkg/*.png iso/boot/grub/themes/bornomala/select_bkg/; \
	fi
	$(GRUB) -o bornomalaOS.iso iso/

run: iso
	./run-qemu.sh

# Verifies the image is a valid Multiboot2 kernel (needs grub-file from grub-common).
check: mykernel.bin
	grub-file --is-x86-multiboot2 mykernel.bin && echo "multiboot2 header OK"

clean:
	rm -f $(OBJS) $(DEPS) mykernel.bin bornomalaOS.iso iso/boot/mykernel.bin
	rm -rf iso/boot/grub/themes

-include $(DEPS)
>>>>>>> 23b11cf3087acc2108f276bdfb25d3a6f909e2f7
