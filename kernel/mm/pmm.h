/* =============================================================================
   RELIS — kernel/mm/pmm.h
   Physical Memory Manager — bitmap-based 4 KiB page frame allocator
   ============================================================================= */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

void  pmm_init(uint32_t mem_kb, uint32_t kernel_end);
void *pmm_alloc(void);          /* allocate one 4 KiB frame, returns phys addr */
void  pmm_free(void *addr);     /* free a previously allocated frame            */
size_t pmm_free_frames(void);   /* number of free frames available              */
