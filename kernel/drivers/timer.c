/* =============================================================================
   RELIS — kernel/drivers/timer.c
   Sets up PIT channel 0 at the requested frequency and counts ticks via IRQ0.
   ============================================================================= */
#include "timer.h"
#include "idt.h"
#include "io.h"

#define PIT_CHANNEL0  0x40
#define PIT_CMD       0x43
#define PIT_BASE_HZ   1193182   /* PIT oscillator frequency in Hz */

static volatile uint64_t tick_count = 0;
static uint32_t ticks_per_ms = 0;

static void timer_irq_handler(registers_t *regs) {
    (void)regs;
    tick_count++;
}

void timer_init(uint32_t hz) {
    extern void irq_register_handler(int irq, void (*handler)(registers_t *));
    irq_register_handler(0, timer_irq_handler);

    ticks_per_ms = hz / 1000;
    if (!ticks_per_ms) ticks_per_ms = 1;

    uint32_t divisor = PIT_BASE_HZ / hz;

    /* Channel 0, lobyte/hibyte, mode 3 (square wave generator) */
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t timer_ticks(void) {
    return tick_count;
}

void timer_sleep(uint32_t ms) {
    uint64_t target = tick_count + (uint64_t)(ms * ticks_per_ms);
    while (tick_count < target)
        __asm__ volatile ("hlt");
}
