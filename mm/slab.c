#include "relis/mm.h"
#include "relis/spinlock.h"

uint8_t kernel_heap_region[4 * 1024 * 1024] __attribute__((aligned(16)));
uint32_t kernel_heap_size = 4 * 1024 * 1024;

static spinlock_t heap_lock = SPIN_LOCK_UNLOCKED;
static uint32_t heap_ptr = 0;

void *kmalloc(size_t size) {
    spin_lock(&heap_lock);
    if (heap_ptr + size > kernel_heap_size) {
        spin_unlock(&heap_lock);
        return 0; 
    }
    void *ptr = &kernel_heap_region[heap_ptr];
    heap_ptr += (size + 15) & ~15; // 16-byte alignment
    spin_unlock(&heap_lock);
    return ptr;
}

void kfree(void *ptr) {
    (void)ptr; 
}