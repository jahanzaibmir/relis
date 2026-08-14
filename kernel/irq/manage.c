#include "relis/irq.h"
#include "relis/printk.h"
#include "arch/io.h"

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
    ack_irq(irq);
    
    if (irq < IDT_ENTRIES && irq_handlers[irq]) {
        irq_handlers[irq](regs);
    }
}

void dispatch_isr(struct registers *regs) {
    // Page Fault (ISR 14)
    if (regs->int_no == 14) {
        uint64_t faulting_address;
        __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
        handle_page_fault(faulting_address, regs->err_code);
        return;
    }
    
    // Syscall (ISR 128)
    if (regs->int_no == 128) {
        return; 
    }
    
    printk("KERNEL PANIC: Exception %d", regs->int_no);
    for (;;) __asm__ volatile("cli; hlt");
}

extern void idt_init(void);
void irq_init(void) {
    idt_init();
    printk("IRQ subsystem initialized");
}