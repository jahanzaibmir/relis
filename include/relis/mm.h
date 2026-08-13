#pragma once
#include <stdint.h>
#include <stddef.h>

void pmm_init(uint32_t total_kb, uint32_t kernel_end);
void *kmalloc(size_t size);
void kfree(void *ptr);

extern uint8_t kernel_heap_region[];
extern uint32_t kernel_heap_size;