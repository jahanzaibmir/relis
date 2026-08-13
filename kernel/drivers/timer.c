#include "relis/irq.h"
#include "arch/io.h"
#include "relis/printk.h"
#include "relis/sched.h"
#include <stdint.h>

volatile uint64_t jiffies = 0;
static uint64_t tick_ns = 10000000;

static void timer_callback(struct registers *regs) {
    (void)regs;
    jiffies++;
    schedule();
}

void timer_init(uint32_t freq) {
    request_irq(32, timer_callback);
    tick_ns = 1000000000 / freq;

    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    
    printk("Timer initialized (%d Hz) - Preemption enabled", freq);
}
