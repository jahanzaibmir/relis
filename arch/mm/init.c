#include "asm/pgtable.h"
#include "relis/mm.h"
#include "relis/printk.h"
#include <stdint.h>

static pgd_t *current_pgd = 0;

void arch_paging_init(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    current_pgd = (pgd_t*)phys_to_virt(cr3 & PTE_ADDR_MASK);
}

void arch_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!current_pgd) return;
    
    // If mapping a user page, all higher-level tables MUST also have the User bit
    uint64_t user_flag = (flags & PTE_USER) ? PTE_USER : 0;
    
    pgd_t *pgd = &current_pgd[pgd_index(virt)];
    if (!(*pgd & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        *pgd = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | user_flag;
    } else {
        *pgd |= user_flag; // FIX: MUST ADD USER BIT TO EXISTING TABLE!
    }
    
    pud_t *pud = (pud_t*)phys_to_virt(*pgd & PTE_ADDR_MASK);
    if (!(pud[pud_index(virt)] & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        pud[pud_index(virt)] = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | user_flag;
    } else {
        pud[pud_index(virt)] |= user_flag; // FIX: MUST ADD USER BIT!
    }
    
    pmd_t *pmd = (pmd_t*)phys_to_virt(pud[pud_index(virt)] & PTE_ADDR_MASK);
    if (!(pmd[pmd_index(virt)] & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        pmd[pmd_index(virt)] = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | user_flag;
    } else {
        pmd[pmd_index(virt)] |= user_flag; // FIX: MUST ADD USER BIT!
    }
    
    pte_t *pte = (pte_t*)phys_to_virt(pmd[pmd_index(virt)] & PTE_ADDR_MASK);
    pte[pte_index(virt)] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
