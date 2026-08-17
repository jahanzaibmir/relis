#pragma once
#include <stdint.h>
#include <stddef.h>
#include "asm/pgtable.h"

void pmm_init(uint32_t total_kb, uint32_t kernel_end);
void *kmalloc(size_t size);
void kfree(void *ptr);

uint64_t alloc_page(void);
void free_page(uint64_t phys);

void paging_init(void);
void arch_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void handle_page_fault(uint64_t faulting_address, uint64_t error_code);

pte_t *walk_page_table(uint64_t virt);
uint64_t get_cr3(void);
void switch_address_space(uint64_t cr3); // FIX: Added switch_address_space

uint64_t clone_address_space(void);
uint64_t create_new_address_space(void);

void *vmalloc(size_t size);
void vfree(void *ptr);

extern uint8_t kernel_heap_region[];
extern uint32_t kernel_heap_size;
