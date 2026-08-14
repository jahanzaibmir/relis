// mm/vmalloc.c
#include "relis/mm.h"
#include "asm/pgtable.h"
#include "relis/printk.h"
#include <stdint.h>

// VMalloc area starts safely above the direct physical mapping
#define VMALLOC_START 0xFFFFC90000000000ULL
#define VMALLOC_END   0xFFFFC90040000000ULL
static uint64_t vmalloc_ptr = VMALLOC_START;

// Allocates `size` bytes of virtually contiguous memory.
// Note: The physical pages backing this memory may be scattered.
void *vmalloc(size_t size) {
    if (size == 0) return 0;
    
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t virt_start = vmalloc_ptr;
    
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint64_t phys = alloc_page();
        arch_map_page(vmalloc_ptr, phys, PTE_WRITABLE);
        vmalloc_ptr += PAGE_SIZE;
    }
    
    return (void*)virt_start;
}

void vfree(void *ptr) {
    // Stub: In a full kernel, this would walk the page tables, 
    // free the physical pages via free_page(), and unmap the PTEs.
    (void)ptr;
}
