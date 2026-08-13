/* =============================================================================
   RELIS — kernel/mm/paging.c
   x86 two-level paging.
   Maps first 8 MiB identity (covers kernel + BSS + heap at 0x100000–0x5AEEXX)
   and also at KERNEL_VIRT_BASE (0xC0000000) for future higher-half move.
   ============================================================================= */
#include "paging.h"
#include "pmm.h"
#include "drivers/vga.h"
#include "kprintf.h"
#include <stddef.h>

/* Two page tables cover 0–8 MiB (each table maps 4 MiB) */
static page_directory_t kernel_dir  __attribute__((aligned(4096)));
static page_table_t     table0      __attribute__((aligned(4096))); /* 0–4 MiB   */
static page_table_t     table1      __attribute__((aligned(4096))); /* 4–8 MiB   */
static page_table_t     high_table0 __attribute__((aligned(4096))); /* 0xC0000000 */
static page_table_t     high_table1 __attribute__((aligned(4096))); /* 0xC0400000 */

static inline uint32_t phys(void *ptr) {
    return (uint32_t)(uintptr_t)ptr;
}
static inline void write_cr3(uint32_t val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val) : "memory");
}
static inline void enable_paging(void) {
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

uint32_t paging_read_cr2(void) {
    uint32_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

void paging_init(void) {
    int i;

    /* Zero all tables */
    for (i = 0; i < PAGES_PER_TABLE; i++) {
        table0.entries[i]      = 0;
        table1.entries[i]      = 0;
        high_table0.entries[i] = 0;
        high_table1.entries[i] = 0;
    }
    for (i = 0; i < TABLES_PER_DIR; i++)
        kernel_dir.entries[i] = 0;

    /* table0: identity-map 0x000000 – 0x3FFFFF (0–4 MiB) */
    for (i = 0; i < PAGES_PER_TABLE; i++)
        table0.entries[i] = (uint32_t)(i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;

    /* table1: identity-map 0x400000 – 0x7FFFFF (4–8 MiB) */
    for (i = 0; i < PAGES_PER_TABLE; i++)
        table1.entries[i] = (uint32_t)(0x400000 + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;

    /* Mirror both tables at KERNEL_VIRT_BASE (0xC0000000) */
    for (i = 0; i < PAGES_PER_TABLE; i++) {
        high_table0.entries[i] = (uint32_t)(i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
        high_table1.entries[i] = (uint32_t)(0x400000 + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Install into directory */
    kernel_dir.entries[0] = phys(&table0) | PAGE_PRESENT | PAGE_WRITE;
    kernel_dir.entries[1] = phys(&table1) | PAGE_PRESENT | PAGE_WRITE;

    uint32_t kb = KERNEL_VIRT_BASE >> 22;   /* = 768 */
    kernel_dir.entries[kb]     = phys(&high_table0) | PAGE_PRESENT | PAGE_WRITE;
    kernel_dir.entries[kb + 1] = phys(&high_table1) | PAGE_PRESENT | PAGE_WRITE;

    write_cr3(phys(&kernel_dir));
    enable_paging();

    kprintf("[paging] Enabled — 8 MiB identity mapped\n");
}

page_directory_t *paging_kernel_directory(void) { return &kernel_dir; }

void paging_switch_directory(page_directory_t *dir) {
    write_cr3(phys(dir));
}

static page_table_t *alloc_table(void) {
    page_table_t *t = (page_table_t *)pmm_alloc();
    if (!t) return NULL;
    for (int i = 0; i < PAGES_PER_TABLE; i++) t->entries[i] = 0;
    return t;
}

page_directory_t *paging_create_directory(void) {
    page_directory_t *dir = (page_directory_t *)pmm_alloc();
    if (!dir) return NULL;
    for (int i = 0; i < (int)(KERNEL_VIRT_BASE >> 22); i++)
        dir->entries[i] = 0;
    for (int i = (int)(KERNEL_VIRT_BASE >> 22); i < TABLES_PER_DIR; i++)
        dir->entries[i] = kernel_dir.entries[i];
    return dir;
}

void paging_destroy_directory(page_directory_t *dir) {
    if (!dir || dir == &kernel_dir) return;
    for (int i = 0; i < (int)(KERNEL_VIRT_BASE >> 22); i++) {
        if (!(dir->entries[i] & PAGE_PRESENT)) continue;
        page_table_t *tbl = (page_table_t *)(uintptr_t)(dir->entries[i] & ~0xFFF);
        for (int j = 0; j < PAGES_PER_TABLE; j++)
            if (tbl->entries[j] & PAGE_PRESENT)
                pmm_free((void *)(uintptr_t)(tbl->entries[j] & ~0xFFF));
        pmm_free(tbl);
    }
    pmm_free(dir);
}

void paging_map_page(page_directory_t *dir,
                     uint32_t virt, uint32_t phys_addr, uint32_t flags) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;
    page_table_t *tbl;
    if (dir->entries[dir_idx] & PAGE_PRESENT) {
        tbl = (page_table_t *)(uintptr_t)(dir->entries[dir_idx] & ~0xFFFu);
    } else {
        tbl = alloc_table();
        if (!tbl) return;
        dir->entries[dir_idx] = (uint32_t)(uintptr_t)tbl | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }
    tbl->entries[table_idx] = (phys_addr & ~0xFFFu) | (flags | PAGE_PRESENT);
    paging_invalidate(virt);
}

void paging_unmap_page(page_directory_t *dir, uint32_t virt) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;
    if (!(dir->entries[dir_idx] & PAGE_PRESENT)) return;
    page_table_t *tbl = (page_table_t *)(uintptr_t)(dir->entries[dir_idx] & ~0xFFFu);
    tbl->entries[table_idx] = 0;
    paging_invalidate(virt);
}

uint32_t paging_virt_to_phys(page_directory_t *dir, uint32_t virt) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;
    if (!(dir->entries[dir_idx] & PAGE_PRESENT)) return 0;
    page_table_t *tbl = (page_table_t *)(uintptr_t)(dir->entries[dir_idx] & ~0xFFFu);
    if (!(tbl->entries[table_idx] & PAGE_PRESENT)) return 0;
    return (tbl->entries[table_idx] & ~0xFFFu) | (virt & 0xFFF);
}
