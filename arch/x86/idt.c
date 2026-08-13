// arch/x86/idt.c
#include "idt.h"
#include "io.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_descriptor_t;

#define IDT_ENTRIES 256
static idt_entry_t idt[IDT_ENTRIES];
static idt_descriptor_t idtr;

#define IDT_INTERRUPT_GATE 0x8E

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void isr128(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static void idt_set_gate(uint8_t idx, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[idx].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[idx].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[idx].selector    = sel;
    idt[idx].zero        = 0;
    idt[idx].type_attr   = flags;
}

static void pic_remap(void) {
    uint8_t m1 = inb(0x21);
    uint8_t m2 = inb(0xA1);

    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    outb(0x21, m1);
    outb(0xA1, m2);
}

void idt_init(void) {
    idtr.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_ENTRIES - 1);
    idtr.base  = (uint32_t)(uintptr_t)&idt;

    for (int i = 0; i < 32; i++) {
        switch(i) {
            case 8: case 10: case 11: case 12: case 13: case 14: case 17:
                idt_set_gate(i, (uint32_t)isr0 + i * 0, 0x08, IDT_INTERRUPT_GATE); // Fallback
                break;
            default:
                idt_set_gate(i, (uint32_t)isr0 + i * 0, 0x08, IDT_INTERRUPT_GATE); // Fallback
                break;
        }
    }
    
    // Correctly map ISRs
    idt_set_gate(0,  (uint32_t)isr0,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_INTERRUPT_GATE);

    pic_remap();
    idt_set_gate(32, (uint32_t)irq0,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(33, (uint32_t)irq1,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(34, (uint32_t)irq2,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(35, (uint32_t)irq3,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(36, (uint32_t)irq4,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(37, (uint32_t)irq5,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(38, (uint32_t)irq6,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(39, (uint32_t)irq7,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(40, (uint32_t)irq8,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(41, (uint32_t)irq9,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(42, (uint32_t)irq10, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(43, (uint32_t)irq11, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(44, (uint32_t)irq12, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(45, (uint32_t)irq13, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(46, (uint32_t)irq14, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(47, (uint32_t)irq15, 0x08, IDT_INTERRUPT_GATE);

    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    idt_load((uint32_t)(uintptr_t)&idtr);
}