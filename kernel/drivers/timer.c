// kernel/drivers/timer.c
#include "relis/irq.h"
#include "arch/x86/io.h"
#include "relis/printk.h"

static void timer_callback(struct registers *regs) {
    (void)regs;
    // We will increment system tick here later
}

void timer_init(uint32_t freq) {
    (void)freq;
    request_irq(32, timer_callback); // IRQ0 is vector 32
    printk("Timer initialized");
}