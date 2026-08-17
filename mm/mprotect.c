// mm/mprotect.c
#include "relis/printk.h"
#include "relis/mm.h"
#include "asm/pgtable.h"
#include <stdint.h>

void sys_mprotect(uint64_t addr, size_t len, uint64_t new_flags) {
    uint64_t end = addr + len;
    
    for (uint64_t vaddr = addr; vaddr < end; vaddr += PAGE_SIZE) {
        pte_t *pte = walk_page_table(vaddr);
        if (pte && (*pte & PTE_PRESENT)) {
            // Clear old flags and set new ones
            *pte = (*pte & ~PTE_FLAGS_MASK) | (new_flags & PTE_FLAGS_MASK) | PTE_PRESENT;
            __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
        }
    }
    printk("sys_mprotect: Updated permissions for 0x%x to 0x%x", addr, end);
}