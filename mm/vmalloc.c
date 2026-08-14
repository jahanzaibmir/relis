#include "relis/mm.h"
#include "asm/pgtable.h"
#include "relis/printk.h"
#include "relis/list.h"
#include <stdint.h>

#define VMALLOC_START 0xFFFFC90000000000ULL
#define VMALLOC_END   0xFFFFC90040000000ULL

static uint64_t vmalloc_ptr = VMALLOC_START;
static LIST_HEAD(vm_struct_list);

struct vm_struct {
    void *addr;
    size_t size;
    struct list_head list;
};

void *vmalloc(size_t size) {
    if (size == 0) return 0;
    
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t virt_start = vmalloc_ptr;
    
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint64_t phys = alloc_page();
        arch_map_page(vmalloc_ptr, phys, PTE_WRITABLE);
        vmalloc_ptr += PAGE_SIZE;
    }
    
    struct vm_struct *vm = kmalloc(sizeof(struct vm_struct));
    if (vm) {
        vm->addr = (void*)virt_start;
        vm->size = pages_needed * PAGE_SIZE;
        list_add(&vm->list, &vm_struct_list);
    }
    
    return (void*)virt_start;
}

void vfree(void *ptr) {
    if (!ptr) return;
    
    struct vm_struct *entry, *temp;
    list_for_each_entry_safe(entry, temp, &vm_struct_list, list) {
        if (entry->addr == ptr) {
            uint32_t pages = entry->size / PAGE_SIZE;
            for (uint32_t i = 0; i < pages; i++) {
                pte_t *pte = walk_page_table((uint64_t)ptr + i * PAGE_SIZE);
                if (pte && (*pte & PTE_PRESENT)) {
                    uint64_t phys = *pte & PTE_ADDR_MASK;
                    free_page(phys);
                    *pte = 0;
                    __asm__ volatile("invlpg (%0)" :: "r"((uint64_t)ptr + i * PAGE_SIZE) : "memory");
                }
            }
            list_del(&entry->list);
            kfree(entry);
            return;
        }
    }
}