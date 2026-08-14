#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE  (1UL << PAGE_SHIFT)
#define PAGE_MASK  (~(PAGE_SIZE - 1))

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_PWT      (1ULL << 3)
#define PTE_PCD      (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY    (1ULL << 6)
#define PTE_HUGE     (1ULL << 7)
#define PTE_GLOBAL   (1ULL << 8)
#define PTE_NX       (1ULL << 63)

#define PTE_FLAGS_MASK 0xFFF0000000000FFFULL
#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL

typedef uint64_t pte_t;
typedef uint64_t pmd_t;
typedef uint64_t pud_t;
typedef uint64_t pgd_t;

static inline uint64_t pgd_index(uint64_t va) { return (va >> 39) & 0x1FF; }
static inline uint64_t pud_index(uint64_t va) { return (va >> 30) & 0x1FF; }
static inline uint64_t pmd_index(uint64_t va) { return (va >> 21) & 0x1FF; }
static inline uint64_t pte_index(uint64_t va) { return (va >> 12) & 0x1FF; }

// Direct map physical memory to 0xFFFF800000000000
#define __PHYS_OFFSET 0xFFFF800000000000ULL

static inline void* phys_to_virt(uint64_t pa) {
    return (void*)(pa + __PHYS_OFFSET);
}

static inline uint64_t virt_to_phys(void *va) {
    return (uint64_t)va - __PHYS_OFFSET;
}