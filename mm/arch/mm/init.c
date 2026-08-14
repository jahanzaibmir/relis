// arch/mm/init.c
#include "asm/pgtable.h"
#include "relis/mm.h"
#include "relis/printk.h"
#include <stdint.h>

static pgd_t *current_pgd = 0;

void arch_paging_init(void) {
    // CR3 currently points to the PML4 table set up in entry.asm
    // Because physical RAM is direct-mapped, we can read it directly.
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    current_pgd = (pgd_t*)phys_to_virt(cr3 & PTE_ADDR_MASK);
}

// Walks the 4-level page tables to map a 4KB virtual page to a 4KB physical page
void arch_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!current_pgd) return;
    
    // Level 4: PML4 (PGD)
    pgd_t *pgd = &current_pgd[pgd_index(virt)];
    if (!(*pgd & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        *pgd = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    
    // Level 3: PDPT (PUD)
    pud_t *pud = (pud_t*)phys_to_virt(*pgd & PTE_ADDR_MASK);
    if (!(pud[pud_index(virt)] & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        pud[pud_index(virt)] = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    
    // Level 2: PD (PMD)
    pmd_t *pmd = (pmd_t*)phys_to_virt(pud[pud_index(virt)] & PTE_ADDR_MASK);
    if (!(pmd[pmd_index(virt)] & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        pmd[pmd_index(virt)] = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE;
    }
    
    // Level 1: PT (PTE)
    pte_t *pte = (pte_t*)phys_to_virt(pmd[pmd_index(virt)] & PTE_ADDR_MASK);
    pte[pte_index(virt)] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    
    // Invalidate the TLB entry for this virtual address so the CPU sees the change
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
