// mm/paging.c
#include <stdint.h>
#include <stddef.h>

uint32_t *paging_kernel_directory = 0;

void paging_map_page(uint32_t virtual, uint32_t physical, uint32_t flags) {
    (void)virtual; 
    (void)physical; 
    (void)flags;
    // Stub for now so E1000 links. We will implement real paging later.
}

void paging_init(void) {
    // Stub
}