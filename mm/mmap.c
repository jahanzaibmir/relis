// mm/mmap.c
#include "relis/printk.h"
#include "relis/list.h"
#include "relis/mm.h"
#include <stdint.h>

// Virtual Memory Area structure
struct vm_area_struct {
    uint64_t vm_start;
    uint64_t vm_end;
    uint32_t vm_flags; // Permissions (Read, Write, Execute)
    struct list_head vm_list;
};

static LIST_HEAD(kernel_vma_list);

void sys_mmap(uint64_t addr, size_t length, uint32_t flags) {
    // Allocate a VMA struct to track this memory region
    struct vm_area_struct *vma = kmalloc(sizeof(struct vm_area_struct));
    if (!vma) {
        printk("sys_mmap: Failed to allocate VMA");
        return;
    }
    
    vma->vm_start = (uint64_t)vmalloc(length);
    vma->vm_end = vma->vm_start + length;
    vma->vm_flags = flags;
    
    list_add(&vma->vm_list, &kernel_vma_list);
    printk("sys_mmap: Allocated VMA from 0x%x to 0x%x", vma->vm_start, vma->vm_end);
}