// kernel/drivers/keyboard.c
#include "relis/irq.h"
#include "arch/x86/io.h"
#include "relis/printk.h"

static void keyboard_callback(struct registers *regs) {
    (void)regs;
    uint8_t sc = inb(0x60);
    (void)sc; 
}

void keyboard_init(void) {
    request_irq(33, keyboard_callback); // IRQ1 is vector 33
    printk("Keyboard initialized");
}