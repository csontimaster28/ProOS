BITS 32

GLOBAL isr_stub
GLOBAL isr_keyboard
GLOBAL isr_timer
GLOBAL isr_syscall
EXTERN keyboard_handler
EXTERN scheduler_schedule
EXTERN handle_syscall

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

; Syscall interrupt (int 0x80) entry
isr_syscall:
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

    ; Push syscall arguments for C caller: id (eax), a (ebx), b (ecx), c (edx)
    push eax
    push ebx
    push ecx
    push edx
    call handle_syscall
    add esp, 16

    ; Save return value and restore registers
    push eax

    pop gs
    pop fs
    pop es
    pop ds

    popa

    pop eax
    iretd
