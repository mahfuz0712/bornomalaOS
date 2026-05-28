CC      = x86_64-linux-gnu-gcc
AS      = nasm
LD      = x86_64-linux-gnu-ld
GRUB    = grub-mkrescue

CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -fno-stack-protector -fno-pic -mno-red-zone \
          -fno-omit-frame-pointer \
          -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel
LDFLAGS = -nostdlib -T boot/linker.ld -z max-page-size=0x1000

.PHONY: all iso clean run

all: mykernel.bin

boot/boot.o: boot/boot.asm
	$(AS) -f elf64 $< -o $@

kernel/kernel.o: kernel/kernel.c kernel/kernel.h kernel/ata.h kernel/alloc.h kernel/ext4.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/ata.o: kernel/ata.c kernel/ata.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/alloc.o: kernel/alloc.c kernel/alloc.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/ext4.o: kernel/ext4.c kernel/ext4.h kernel/ata.h kernel/alloc.h
	$(CC) $(CFLAGS) -c $< -o $@

mykernel.bin: boot/boot.o kernel/kernel.o kernel/ata.o kernel/alloc.o kernel/ext4.o
	$(LD) $(LDFLAGS) -o $@ $^

iso: mykernel.bin
	cp mykernel.bin iso/boot/mykernel.bin
	$(GRUB) -o myos.iso iso/

clean:
	rm -f boot/boot.o kernel/kernel.o kernel/ata.o kernel/alloc.o kernel/ext4.o \
	      mykernel.bin iso/boot/mykernel.bin myos.iso