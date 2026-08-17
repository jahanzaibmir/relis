#include "relis/smp.h"
#include "relis/printk.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "asm/apic.h"

struct cpu_info cpus[MAX_CPUS];
int num_cpus = 1;

struct cpu_info* get_cpu(uint8_t cpu_id) {
    if (cpu_id >= MAX_CPUS) return 0;
    return &cpus[cpu_id];
}

struct cpu_info* get_current_cpu(void) {
    // In a full SMP kernel, this reads from the GS register.
    // For now, we only have CPU 0 running.
    return &cpus[0];
}

void smp_init(void) {
    kmemset(cpus, 0, sizeof(cpus));
    
    // Initialize CPU 0 (BSP)
    cpus[0].cpu_id = 0;
    cpus[0].apic_id = lapic_get_id();
    cpus[0].flags = 1; // Online
    cpus[0].current_task = current_task;
    
    printk("SMP: BSP initialized (APIC ID: %d)", cpus[0].apic_id);
}
