CC=i686-linux-gnu-gcc
AS=nasm
LD=i686-linux-gnu-ld

USER_CFLAGS=-m32 -ffreestanding -fno-stack-protector -nostdlib -O2 -Wall -Wextra -I./include -I../kernel
LDFLAGS=-m elf_i386

SRCS=gfxd.c displayd.c hello_gui.c syscall.c gui_toolkit.c
OBJS=$(SRCS:.c=.o)

all: gfxd.elf displayd.elf hello_gui.elf

%.o: %.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

gfxd.elf: gfxd.o syscall.o gui_toolkit.o
	$(LD) $(LDFLAGS) -Ttext 0x1000 gfxd.o syscall.o gui_toolkit.o -o gfxd.elf

displayd.elf: displayd.o syscall.o gui_toolkit.o
	$(LD) $(LDFLAGS) -Ttext 0x1000 displayd.o syscall.o gui_toolkit.o -o displayd.elf

hello_gui.elf: hello_gui.o syscall.o gui_toolkit.o
	$(LD) $(LDFLAGS) -Ttext 0x1000 hello_gui.o syscall.o gui_toolkit.o -o hello_gui.elf

clean:
	rm -f *.o *.elf
