/* =============================================================================
   RELIS — kernel/mm/heap.h / heap.c
   Simple kernel heap — slab-like block allocator with free list
   ============================================================================= */
#pragma once
#include <stddef.h>

void  heap_init(void *start, size_t size);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void  kfree(void *ptr);
