// kernel/drivers/timer.c
#include "relis/irq.h"
#include "arch/x86/io.h"
#include "relis/printk.h"

volatile uint32_t timer_ticks = 0; // Renamed and made global

static void timer_callback(struct registers *regs) {
    (void)regs;
    timer_ticks++;
}

void timer_init(uint32_t freq) {
    request_irq(32, timer_callback); 
    
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    
    printk("Timer initialized (%d Hz)", freq);
}