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
	$(CC) $(CFLAGS) -c kernel/memory.c -o memory.o
	$(CC) $(CFLAGS) -c kernel/process.c -o process.o
	$(CC) $(CFLAGS) -c kernel/filesystem.c -o filesystem.o
	$(CC) $(CFLAGS) -c kernel/ipc.c -o ipc.o
	$(CC) $(CFLAGS) -c kernel/logging.c -o logging.o
	$(CC) $(CFLAGS) -c kernel/event.c -o event.o
	$(CC) $(CFLAGS) -c kernel/framebuffer.c -o framebuffer.o
	$(CC) $(CFLAGS) -c kernel/capability.c -o capability.o
	$(CC) $(CFLAGS) -c kernel/module.c -o module.o
	$(CC) $(CFLAGS) -c kernel/debug_module.c -o debug_module.o
	$(CC) $(CFLAGS) -c kernel/daemons.c -o daemons.o
	$(CC) $(CFLAGS) -c kernel/syscalls.c -o syscalls.o
	$(CC) $(CFLAGS) -c kernel/profs.c -o profs.o
	$(CC) $(CFLAGS) -c kernel/embedded_displayd.c -o embedded_displayd.o
	$(LD) $(LDFLAGS) boot.o isr.o kernel.o idt.o keyboard.o pic.o scheduler.o memory.o process.o filesystem.o ipc.o logging.o event.o framebuffer.o capability.o module.o debug_module.o daemons.o syscalls.o profs.o -o kernel.bin


iso/myos.iso: kernel.bin
	cp kernel.bin iso/boot/kernel.bin
	# Build and copy userland binaries into iso tree
	$(MAKE) -C userland all || true
	mkdir -p iso/bin
	cp userland/gfxd.elf iso/bin/ || true
	cp userland/displayd.elf iso/bin/ || true
	cp userland/hello_gui.elf iso/bin/ || true
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
