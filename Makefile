CC=i686-linux-gnu-gcc
AS=nasm
LD=i686-linux-gnu-ld

CFLAGS=-m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -O2 -Wall -Wextra
LDFLAGS=-m elf_i386 -T linker.ld -nostdlib

all: iso/myos.iso

iso/boot/grub:
	mkdir -p iso/boot/grub

kernel.bin: iso/boot/grub
	$(AS) -f elf32 kernel/boot.asm -o boot.o
	$(AS) -f elf32 kernel/isr.asm -o isr.o
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c kernel/idt.c -o idt.o
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o keyboard.o
	$(CC) $(CFLAGS) -c kernel/pic.c -o pic.o
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o scheduler.o
	$(LD) $(LDFLAGS) boot.o isr.o kernel.o idt.o keyboard.o pic.o scheduler.o -o kernel.bin


iso/myos.iso: kernel.bin
	cp kernel.bin iso/boot/kernel.bin
	cp boot/grub/grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o iso/myos.iso iso

run: iso/myos.iso
	qemu-system-i386 -cdrom iso/myos.iso -display curses

run-gui: iso/myos.iso
	qemu-system-i386 -cdrom iso/myos.iso

run-debug: iso/myos.iso
	qemu-system-i386 -cdrom iso/myos.iso -s -S

clean:
	rm -rf *.o kernel.bin iso
