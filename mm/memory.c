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

void paging_unmap_page(uint64_t virt) {
    pte_t *pte = walk_page_table(virt);
    if (pte && (*pte & PTE_PRESENT)) {
        *pte = 0;
        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    }
}

pte_t *walk_page_table(uint64_t virt) {
    uint64_t cr3 = get_cr3();
    pgd_t *pgd = (pgd_t*)phys_to_virt(cr3 & PTE_ADDR_MASK);
    
    if (!(*pgd & PTE_PRESENT)) return NULL;
    pud_t *pud = (pud_t*)phys_to_virt(*pgd & PTE_ADDR_MASK);
    
    if (!(pud[pud_index(virt)] & PTE_PRESENT)) return NULL;
    pmd_t *pmd = (pmd_t*)phys_to_virt(pud[pud_index(virt)] & PTE_ADDR_MASK);
    
    if (!(pmd[pmd_index(virt)] & PTE_PRESENT)) return NULL;
    pte_t *pte = (pte_t*)phys_to_virt(pmd[pmd_index(virt)] & PTE_ADDR_MASK);
    
    return &pte[pte_index(virt)];
}

void handle_page_fault(uint64_t faulting_address, uint64_t error_code) {
    printk("\n KERNEL PANIC: PAGE FAULT ");
    printk("Faulting Address: 0x%x", faulting_address);
    printk("Error Code: 0x%x", error_code);
    if (error_code & 0x1) printk("Cause: Protection violation");
    else printk("Cause: Non-present page");
    if (error_code & 0x2) printk("Operation: Write");
    else printk("Operation: Read");
    if (error_code & 0x4) printk("Mode: User-space (Ring 3)");
    else printk("Mode: Kernel-space (Ring 0)");
    for(;;) __asm__ volatile("cli; hlt");
}