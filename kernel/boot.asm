BITS 32

SECTION .multiboot
align 8
dd 0xe85250d6
dd 0
dd header_end - header_start
dd -(0xe85250d6 + 0 + (header_end - header_start))

header_start:
dw 0
dw 0
dd 8
header_end:

SECTION .data
align 8
gdt:
    dq 0x0000000000000000        ; NULL descriptor
    dq 0x00cf9a000000ffff        ; Code segment (0x08)
    dq 0x00cf92000000ffff        ; Data segment (0x10)

gdt_ptr:
    dw gdt_ptr - gdt - 1
    dd gdt

SECTION .bss
align 16
stack_bottom:
resb 16384        ; 16 KB stack
stack_top:

SECTION .text
global _start
extern kernel_main

_start:
    cli
    
    ; Load GDT
    lgdt [gdt_ptr]
    
    ; Reload segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov esp, stack_top
    
    ; Far jump to reload CS
    jmp 0x08:reload_cs
    
reload_cs:
    call kernel_main
    hlt
