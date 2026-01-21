BITS 32

GLOBAL isr_stub
GLOBAL isr_keyboard
GLOBAL isr_timer
EXTERN keyboard_handler
EXTERN scheduler_schedule

isr_stub:
    pusha
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    popa
    iretd

isr_keyboard:
    cli

    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10        ; kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call keyboard_handler

    pop gs
    pop fs
    pop es
    pop ds

    ; Send EOI to PIC (master controller)
    mov al, 0x20
    out 0x20, al

    popa
    
    ; Don't use sti here - let iretd restore IF flag from stack
    iretd

isr_timer:
    cli
    pusha
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    popa
    
    sti
    iretd
