CC      = x86_64-linux-gnu-gcc
AS      = nasm
LD      = x86_64-linux-gnu-ld
GRUB    = grub-mkrescue

CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -fno-stack-protector -fno-pic -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel \
          -Ikernel

LDFLAGS = -nostdlib -T boot/linker.ld -z max-page-size=0x1000

OBJS = boot/boot.o \
       kernel/kernel.o \
       kernel/klib.o   \
       kernel/alloc.o  \
       kernel/disk.o   \
       kernel/ext4.o   \
       kernel/installer.o

.PHONY: all iso clean

all: mykernel.bin

boot/boot.o: boot/boot.asm
	$(AS) -f elf64 $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

mykernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

iso: mykernel.bin
	cp mykernel.bin iso/boot/mykernel.bin
	mkdir -p iso/boot/grub/themes/bornomala/select_bkg
	cp grub-theme/theme.txt        iso/boot/grub/themes/bornomala/
	cp grub-theme/logo.png         iso/boot/grub/themes/bornomala/
	cp grub-theme/splash.png       iso/boot/grub/themes/bornomala/
	cp grub-theme/select_bkg/*.png iso/boot/grub/themes/bornomala/select_bkg/
	$(GRUB) -o bornomalaOS.iso iso/

clean:
	rm -f $(OBJS) mykernel.bin bornomalaOS.iso
	rm -f iso/boot/mykernel.bin
	rm -rf iso/boot/grub/themes