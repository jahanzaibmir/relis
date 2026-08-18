#include "asm/smp_boot.h"
#include "asm/apic.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"
#include "relis/sched.h"
#include "relis/smp.h"
#include <stdint.h>

extern uint8_t trampoline_bin[];
extern uint8_t trampoline_end[];

#define TRAMP_CR3_OFF    0x08
#define TRAMP_STACK_OFF  0x10
#define TRAMP_ENTRY_OFF  0x18
#define TRAMP_GDT_OFF   0x20

// Static stacks eliminate race conditions and kmalloc issues
static uint8_t ap_stacks[4][16384] __attribute__((aligned(16)));

void smp_boot_apus(void) {
    uint64_t tramp_size = trampoline_end - trampoline_bin;
    
    //  Use identity-mapped 0x8000 to GUARANTEE we write to physical 0x8000!
    uint8_t *tramp_dest = (uint8_t*)0x8000;
    
    // Identity map 0x8000 so the BSP can write to it, and the AP can execute it
    arch_map_page(0x8000, 0x8000, 0x03);
    
    for (uint64_t i = 0; i < tramp_size; i++) {
        tramp_dest[i] = trampoline_bin[i];
    }

    extern uint64_t gdtr_base;
    extern uint16_t gdtr_limit;
    uint64_t gdt_phys = (uint64_t)gdtr_base - 0xFFFFFFFF80000000ULL;

    *((uint64_t*)(tramp_dest + TRAMP_CR3_OFF)) = get_cr3();
    *((uint64_t*)(tramp_dest + TRAMP_ENTRY_OFF)) = (uint64_t)ap_main;
    *((uint16_t*)(tramp_dest + TRAMP_GDT_OFF)) = gdtr_limit;
    *((uint32_t*)(tramp_dest + TRAMP_GDT_OFF + 2)) = (uint32_t)gdt_phys;

    //  Disable interrupts during AP boot sequence!
    __asm__ volatile("cli");

    for (int i = 1; i < 4; i++) {
        *((uint64_t*)(tramp_dest + TRAMP_STACK_OFF)) = (uint64_t)&ap_stacks[i][16384];

        printk("SMP: Waking up AP %d...", i);

        send_apic_init(i);
        // Greatly increase delay to ensure AP processes INIT before SIPI
        for (volatile int d = 0; d < 100000000; d++); 

        send_apic_startup(i, 0x08);
        for (volatile int d = 0; d < 1000000; d++); 

        send_apic_startup(i, 0x08);
        for (volatile int d = 0; d < 1000000; d++); 
    }

    __asm__ volatile("sti");
}
