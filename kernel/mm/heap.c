/* =============================================================================
   RELIS — kernel/mm/heap.c
   Kernel heap allocator — linked-list of blocks with first-fit strategy
   Each allocation is prefixed with a block_header_t describing its size/state.
   ============================================================================= */
#include "heap.h"
#include <stdint.h>

#define HEAP_MAGIC 0x57AC4B05   /* "STACKOS" magic */
#define ALIGN8(x)  (((x) + 7) & ~7u)

typedef struct block_header {
    uint32_t            magic;
    size_t              size;       /* usable bytes after header */
    int                 free;       /* 1 = free, 0 = allocated   */
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

static block_header_t *heap_head = NULL;

void heap_init(void *start, size_t size) {
    heap_head        = (block_header_t *)start;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size  = size - sizeof(block_header_t);
    heap_head->free  = 1;
    heap_head->next  = NULL;
    heap_head->prev  = NULL;
}

/* Split block b so that b->size == needed; leftover becomes a new free block */
static void split_block(block_header_t *b, size_t needed) {
    size_t leftover = b->size - needed - sizeof(block_header_t);
    if (leftover <= sizeof(block_header_t) + 8)
        return;   /* not worth splitting */

    block_header_t *nb = (block_header_t *)((uint8_t *)b + sizeof(block_header_t) + needed);
    nb->magic = HEAP_MAGIC;
    nb->size  = leftover;
    nb->free  = 1;
    nb->next  = b->next;
    nb->prev  = b;

    if (b->next) b->next->prev = nb;
    b->next = nb;
    b->size = needed;
}

/* Coalesce adjacent free blocks to reduce fragmentation */
static void merge_free(block_header_t *b) {
    /* Merge with next */
    while (b->next && b->next->free) {
        b->size += sizeof(block_header_t) + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    /* Merge with previous */
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(block_header_t) + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void *kmalloc(size_t size) {
    if (!size) return NULL;
    size = ALIGN8(size);

    block_header_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            split_block(b, size);
            b->free  = 0;
            b->magic = HEAP_MAGIC;
            return (void *)((uint8_t *)b + sizeof(block_header_t));
        }
        b = b->next;
    }
    return NULL;   /* out of heap */
}

void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void  *ptr   = kmalloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *b = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (b->magic != HEAP_MAGIC || b->free)
        return;   /* double-free or corruption — silently ignore */
    b->free = 1;
    merge_free(b);
}
