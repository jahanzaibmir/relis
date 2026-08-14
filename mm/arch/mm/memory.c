// mm/memory.c
#include "relis/mm.h"
#include "relis/printk.h"
#include "asm/pgtable.h"
#include <stdint.h>

void paging_init(void) {
    arch_paging_init();
    printk("Virtual Memory subsystem initialized (4-level paging active)");
}

void paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    arch_map_page(virt, phys, flags);
}

// Page Fault Handler (ISR 14)
// When the CPU tries to access an unmapped page, it calls this.
void handle_page_fault(uint64_t faulting_address, uint64_t error_code) {
    printk("\n--- KERNEL PANIC: PAGE FAULT ---");
    printk("Faulting Address: 0x%x", faulting_address);
    printk("Error Code: %d", error_code);
    
    if (error_code & 1) printk("Cause: Protection violation (Page present, but no permission)");
    else printk("Cause: Non-present page");
    
    if (error_code & 2) printk("Operation: Write");
    else printk("Operation: Read");
    
    if (error_code & 4) printk("Mode: User-space (Ring 3)");
    else printk("Mode: Kernel-space (Ring 0)");
    
    if (error_code & 8) printk("Flag: Reserved bit set in page table");
    if (error_code & 16) printk("Flag: Instruction fetch");
    
    for(;;) __asm__ volatile("cli; hlt");
}
