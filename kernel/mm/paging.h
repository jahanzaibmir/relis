/* =============================================================================
   RELIS — kernel/mm/paging.h
   x86 32-bit paging — page directories, page tables, virtual memory mapping.

   Memory layout after paging is enabled:
     0x00000000 – 0xBFFFFFFF  →  user space  (3 GiB)
     0xC0000000 – 0xFFFFFFFF  →  kernel space (1 GiB, identity-mapped)

   The kernel lives at its physical load address (1 MiB), but we also map it
   at 0xC0000000 so that kernel virtual addresses are always stable regardless
   of where user processes are mapped.
   ============================================================================= */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* ── Page / table sizes ──────────────────────────────────────────────────── */
#define PAGE_SIZE        4096
#define PAGES_PER_TABLE  1024
#define TABLES_PER_DIR   1024

/* ── Page entry flag bits ───────────────────────────────────────────────── */
#define PAGE_PRESENT   (1 << 0)   /* page is present in RAM              */
#define PAGE_WRITE     (1 << 1)   /* page is writable                    */
#define PAGE_USER      (1 << 2)   /* user-mode code can access           */
#define PAGE_ACCESSED  (1 << 5)   /* CPU sets this on read               */
#define PAGE_DIRTY     (1 << 6)   /* CPU sets this on write              */

/* ── Kernel virtual base ────────────────────────────────────────────────── */
#define KERNEL_VIRT_BASE 0xC0000000u

/* ── A single page-directory / page-table entry ─────────────────────────── */
typedef uint32_t page_entry_t;

/* ── Page directory: 1024 entries, each pointing to a page table ─────────── */
typedef struct {
    page_entry_t entries[TABLES_PER_DIR];
} __attribute__((aligned(4096))) page_directory_t;

/* ── Page table: 1024 entries, each pointing to a 4 KiB page frame ───────── */
typedef struct {
    page_entry_t entries[PAGES_PER_TABLE];
} __attribute__((aligned(4096))) page_table_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Initialise paging and switch the CPU to use our page directory */
void paging_init(void);

/* Create a fresh page directory (used for new processes) */
page_directory_t *paging_create_directory(void);

/* Free a page directory and all user-space page tables it owns */
void paging_destroy_directory(page_directory_t *dir);

/* Map one virtual page → one physical frame in the given directory.
   flags: combination of PAGE_PRESENT | PAGE_WRITE | PAGE_USER          */
void paging_map_page(page_directory_t *dir,
                     uint32_t virt, uint32_t phys, uint32_t flags);

/* Unmap a virtual page (marks entry not-present, does NOT free the frame) */
void paging_unmap_page(page_directory_t *dir, uint32_t virt);

/* Translate virt → phys in the given directory. Returns 0 if not mapped. */
uint32_t paging_virt_to_phys(page_directory_t *dir, uint32_t virt);

/* Switch the CPU's active page directory (writes CR3) */
void paging_switch_directory(page_directory_t *dir);

/* Return the kernel's own page directory */
page_directory_t *paging_kernel_directory(void);

/* Invalidate a single TLB entry */
static inline void paging_invalidate(uint32_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}
