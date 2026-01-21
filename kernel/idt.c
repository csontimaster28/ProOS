#include "idt.h"

extern void isr_keyboard(void);
extern void isr_timer(void);
extern void isr_stub(void);  // Default handler for all interrupts

static struct idt_entry idt[256];
static struct idt_ptr idtp;

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].base_low  = handler & 0xFFFF;
    idt[n].selector  = 0x08;   // kernel code segment
    idt[n].zero      = 0;
    idt[n].flags     = 0x8E;   // present, ring 0, interrupt gate
    idt[n].base_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    // ⚠️ MINDEN interrupt kap stubot
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, (uint32_t)isr_stub);

    // Timer IRQ0 → 0x20 (PIC remap után)
    idt_set_gate(0x20, (uint32_t)isr_timer);
    
    // Keyboard IRQ1 → 0x21 (PIC remap után)
    idt_set_gate(0x21, (uint32_t)isr_keyboard);

    asm volatile("lidt %0" : : "m"(idtp));
}
