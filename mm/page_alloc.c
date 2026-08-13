// mm/page_alloc.c
#include "relis/mm.h"
#include "relis/printk.h"
#include "relis/string.h"

#define PMM_BITMAP_SIZE 32768
static uint32_t pmm_bitmap[PMM_BITMAP_SIZE];

void pmm_init(uint32_t total_kb, uint32_t kernel_end) {
    (void)total_kb;
    (void)kernel_end;
    kmemset(pmm_bitmap, 0, sizeof(pmm_bitmap));
    printk("PMM initialized");
}