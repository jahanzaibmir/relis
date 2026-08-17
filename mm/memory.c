#include "relis/mm.h"
#include "relis/printk.h"
#include "relis/sched.h"
#include "asm/pgtable.h"
#include <stdint.h>

void paging_init(void) {
    arch_paging_init();
    printk("Virtual Memory subsystem initialized (4-level paging active)");
}

void paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    arch_map_page(virt, phys, flags);
}

void paging_unmap_page(uint64_t virt) {
    pte_t *pte = walk_page_table(virt);
    if (pte && (*pte & PTE_PRESENT)) {
        *pte = 0;
        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    }
}

// FIX: The Page Fault Handler is now a beast!
void handle_page_fault(uint64_t faulting_address, uint64_t error_code) {
    struct task_struct *current = current_task;

    // 1. Demand Paging: Page Not Present
    if (!(error_code & 1)) {
        // Is it within the user stack VMA?
        if (current && faulting_address >= current->stack_start && faulting_address < current->stack_end) {
            uint64_t phys = alloc_page();
            arch_map_page(faulting_address & PAGE_MASK, phys, PTE_WRITABLE | PTE_USER);
            return; // Demand paging success!
        }
        // Is it an anonymous mmap region? (We'd check VMA tree here in a full OS)
    }

    // 2. Copy-on-Write: Write Fault on Present Page
    if (error_code & 2) {
        pte_t *pte = walk_page_table(faulting_address);
        if (pte && (*pte & PTE_PRESENT) && (*pte & PTE_COW)) {
            uint64_t old_phys = *pte & PTE_ADDR_MASK;
            uint64_t new_phys = alloc_page();
            
            // Copy the old data to the new page
            kmemcpy(phys_to_virt(new_phys), phys_to_virt(old_phys), PAGE_SIZE);
            
            // Map the new page as writable and clear CoW
            *pte = new_phys | PTE_WRITABLE | PTE_USER | PTE_PRESENT;
            __asm__ volatile("invlpg (%0)" :: "r"(faulting_address & PAGE_MASK) : "memory");
            return; // CoW success!
        }
    }

    // 3. Real fault
    printk("\n--- KERNEL PANIC: PAGE FAULT ---");
    printk("Faulting Address: 0x%x", faulting_address);
    printk("Error Code: 0x%x", error_code);
    if (error_code & 1) printk("Cause: Protection violation");
    else printk("Cause: Non-present page");
    if (error_code & 2) printk("Operation: Write");
    else printk("Operation: Read");
    if (error_code & 4) printk("Mode: User-space (Ring 3)");
    else printk("Mode: Kernel-space (Ring 0)");
    for(;;) __asm__ volatile("cli; hlt");
}
