#include "asm/pgtable.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"
#include "asm/apic.h" //  Include APIC definitions
#include <stdint.h>

static pgd_t *current_pgd = 0;

void arch_paging_init(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    current_pgd = (pgd_t*)phys_to_virt(cr3 & PTE_ADDR_MASK);

    //  Map Local APIC and IOAPIC MMIO space into the VMALLOC area
    // We use PCD (Cache Disable) because MMIO must not be cached by the CPU!
    arch_map_page(LAPIC_VADDR, LAPIC_BASE_PHYS, PTE_WRITABLE | PTE_PCD | PTE_GLOBAL);
    arch_map_page(IOAPIC_VADDR, IOAPIC_BASE_PHYS, PTE_WRITABLE | PTE_PCD | PTE_GLOBAL);
}

void switch_address_space(uint64_t cr3) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3));
    current_pgd = (pgd_t*)phys_to_virt(cr3 & PTE_ADDR_MASK);
}

void arch_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!current_pgd) return;
    uint64_t user_flag = (flags & PTE_USER) ? PTE_USER : 0;

    pgd_t *pgd = &current_pgd[pgd_index(virt)];
    if (!(*pgd & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        *pgd = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | user_flag;
    } else {
        *pgd |= user_flag;
    }

    pud_t *pud = (pud_t*)phys_to_virt(*pgd & PTE_ADDR_MASK);
    if (!(pud[pud_index(virt)] & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        pud[pud_index(virt)] = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | user_flag;
    } else {
        pud[pud_index(virt)] |= user_flag;
    }

    pmd_t *pmd = (pmd_t*)phys_to_virt(pud[pud_index(virt)] & PTE_ADDR_MASK);
    if (!(pmd[pmd_index(virt)] & PTE_PRESENT)) {
        uint64_t new_table = alloc_page();
        pmd[pmd_index(virt)] = (new_table & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | user_flag;
    } else {
        pmd[pmd_index(virt)] |= user_flag;
    }

    pte_t *pte = (pte_t*)phys_to_virt(pmd[pmd_index(virt)] & PTE_ADDR_MASK);
    pte[pte_index(virt)] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

pte_t *walk_page_table(uint64_t virt) {
    if (!current_pgd) return 0;
    
    pgd_t *pgd = &current_pgd[pgd_index(virt)];
    if (!(*pgd & PTE_PRESENT)) return 0;
    
    pud_t *pud = (pud_t*)phys_to_virt(*pgd & PTE_ADDR_MASK);
    if (!(pud[pud_index(virt)] & PTE_PRESENT)) return 0;
    
    pmd_t *pmd = (pmd_t*)phys_to_virt(pud[pud_index(virt)] & PTE_ADDR_MASK);
    if (!(pmd[pmd_index(virt)] & PTE_PRESENT)) return 0;
    
    pte_t *pte = (pte_t*)phys_to_virt(pmd[pmd_index(virt)] & PTE_ADDR_MASK);
    return &pte[pte_index(virt)];
}

uint64_t get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

uint64_t clone_address_space(void) {
    uint64_t new_pml4_phys = alloc_page();
    uint64_t *new_pml4 = phys_to_virt(new_pml4_phys);
    uint64_t *old_pml4 = phys_to_virt(get_cr3() & PTE_ADDR_MASK);

    for (int i = 256; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }

    for (int i = 0; i < 256; i++) {
        if (!(old_pml4[i] & PTE_PRESENT)) continue;
        uint64_t *old_pdpt = phys_to_virt(old_pml4[i] & PTE_ADDR_MASK);
        uint64_t new_pdpt_phys = alloc_page();
        uint64_t *new_pdpt = phys_to_virt(new_pdpt_phys);
        new_pml4[i] = new_pdpt_phys | (old_pml4[i] & PTE_FLAGS_MASK);

        for (int j = 0; j < 512; j++) {
            if (!(old_pdpt[j] & PTE_PRESENT)) continue;
            uint64_t *old_pd = phys_to_virt(old_pdpt[j] & PTE_ADDR_MASK);
            uint64_t new_pd_phys = alloc_page();
            uint64_t *new_pd = phys_to_virt(new_pd_phys);
            new_pdpt[j] = new_pd_phys | (old_pdpt[j] & PTE_FLAGS_MASK);

            for (int k = 0; k < 512; k++) {
                if (!(old_pd[k] & PTE_PRESENT)) continue;
                if (old_pd[k] & PTE_HUGE) {
                    new_pd[k] = old_pd[k]; 
                } else {
                    uint64_t *old_pt = phys_to_virt(old_pd[k] & PTE_ADDR_MASK);
                    uint64_t new_pt_phys = alloc_page();
                    uint64_t *new_pt = phys_to_virt(new_pt_phys);
                    new_pd[k] = new_pt_phys | (old_pd[k] & PTE_FLAGS_MASK);

                    for (int l = 0; l < 512; l++) {
                        if (!(old_pt[l] & PTE_PRESENT)) continue;
                        uint64_t old_page_phys = old_pt[l] & PTE_ADDR_MASK;
                        uint64_t new_page_phys = alloc_page();
                        kmemcpy(phys_to_virt(new_page_phys), phys_to_virt(old_page_phys), PAGE_SIZE);
                        new_pt[l] = new_page_phys | (old_pt[l] & PTE_FLAGS_MASK);
                    }
                }
            }
        }
    }
    return new_pml4_phys;
}

uint64_t create_new_address_space(void) {
    uint64_t new_pml4_phys = alloc_page();
    uint64_t *new_pml4 = phys_to_virt(new_pml4_phys);
    uint64_t *old_pml4 = phys_to_virt(get_cr3() & PTE_ADDR_MASK);

    for (int i = 256; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }
    return new_pml4_phys;
}
