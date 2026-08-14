#include "relis/mm.h"
#include "relis/printk.h"
#include "relis/string.h"
#include "asm/pgtable.h"
#include <stdint.h>

#define MAX_PHYS_PAGES 65536
#define BITS_PER_WORD 64

static uint64_t pmm_bitmap[MAX_PHYS_PAGES / BITS_PER_WORD];
static uint32_t pmm_pages_total = 0;
static uint32_t pmm_pages_free = 0;
static uint32_t next_free_page = 0;

extern char kernel_end[];

void pmm_init(uint32_t total_kb, uint32_t kernel_end_addr) {
    (void)kernel_end_addr;
    pmm_pages_total = total_kb / 4;
    if (pmm_pages_total > MAX_PHYS_PAGES) pmm_pages_total = MAX_PHYS_PAGES;
    
    kmemset(pmm_bitmap, 0, sizeof(pmm_bitmap));
    
    uint64_t kernel_phys_end = (uint64_t)kernel_end - 0xFFFFFFFF80000000ULL;
    uint32_t kernel_pages = (kernel_phys_end + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint32_t i = 0; i < kernel_pages; i++) {
        pmm_bitmap[i / BITS_PER_WORD] |= (1ULL << (i % BITS_PER_WORD));
    }
    
    pmm_pages_free = pmm_pages_total - kernel_pages;
    printk("PMM initialized: %d total pages, %d free (%d KB)", 
           pmm_pages_total, pmm_pages_free, pmm_pages_free * 4);
}

uint64_t alloc_page(void) {
    for (uint32_t i = next_free_page; i < pmm_pages_total; i++) {
        uint64_t bit = 1ULL << (i % BITS_PER_WORD);
        if (!(pmm_bitmap[i / BITS_PER_WORD] & bit)) {
            pmm_bitmap[i / BITS_PER_WORD] |= bit;
            pmm_pages_free--;
            next_free_page = i + 1;
            uint64_t phys = (uint64_t)i * PAGE_SIZE;
            kmemset(phys_to_virt(phys), 0, PAGE_SIZE);
            return phys;
        }
    }
    printk("PANIC: Out of physical memory!");
    for(;;) __asm__ volatile("cli; hlt");
    return 0;
}

void free_page(uint64_t phys) {
    uint32_t i = phys / PAGE_SIZE;
    if (i >= pmm_pages_total) return;
    uint64_t bit = 1ULL << (i % BITS_PER_WORD);
    if (pmm_bitmap[i / BITS_PER_WORD] & bit) {
        pmm_bitmap[i / BITS_PER_WORD] &= ~bit;
        pmm_pages_free++;
        if (i < next_free_page) next_free_page = i;
    }
}