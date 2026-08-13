// init/main.c
#include "relis/printk.h"
#include "relis/sched.h"
#include "relis/irq.h"
#include "relis/syscall.h"
#include "relis/mm.h"
#include "arch/x86/gdt.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
} multiboot_info_t;

static void heartbeat_task(void) {
    while (1) {
        __asm__ volatile("hlt"); // Just idle for now
    }
}

void start_kernel(uint32_t mb_magic, multiboot_info_t *mb_info) {
    (void)mb_magic;
    
    console_init();
    printk(" RELIS KERNEL BOOTING ");

    gdt_init();
    
    pmm_init(mb_info->mem_upper, 0); 
    
    irq_init();  // Loads IDT and PIC
    
    sched_init(); 
    
    syscall_init();

    timer_init(100);
    keyboard_init();

    __asm__ volatile("sti");
    printk("Interrupts enabled");

    kernel_thread("heartbeat", heartbeat_task, 0);

    printk("RELIS Kernel initializing done. Idling...");

    while (1) {
        __asm__ volatile("hlt");
    }
}