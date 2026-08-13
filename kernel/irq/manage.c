// kernel/irq/manage.c
#include "relis/irq.h"
#include "relis/printk.h"
#include "arch/x86/io.h"

#define IDT_ENTRIES 256
static irq_handler_t irq_handlers[IDT_ENTRIES];

void request_irq(uint32_t irq, irq_handler_t handler) {
    if (irq < IDT_ENTRIES) {
        irq_handlers[irq] = handler;
    }
}

void ack_irq(uint32_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void dispatch_irq(struct registers *regs) {
    uint32_t irq = regs->int_no;
    if (irq < IDT_ENTRIES && irq_handlers[irq]) {
        irq_handlers[irq](regs);
    }
    ack_irq(irq);
}

void dispatch_isr(struct registers *regs) {
    // If it's a syscall (128), handle it here later
    if (regs->int_no == 128) {
        return;
    }
    
    // Otherwise, it's an exception. Halt the system.
    printk("KERNEL PANIC: Exception %d", regs->int_no);
    for (;;) __asm__ volatile("cli; hlt");
}

void irq_init(void) {
    idt_init();
    printk("IRQ subsystem initialized");
}