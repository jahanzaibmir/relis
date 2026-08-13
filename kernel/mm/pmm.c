/* =============================================================================
   RELIS — kernel/mm/pmm.c
   Bitmap-based physical page frame allocator
   One bit per 4 KiB page: 0 = free, 1 = used
   ============================================================================= */
#include "pmm.h"

#define FRAMES_MAX  (1024 * 1024)          /* support up to 4 GiB physical RAM */
#define BITMAP_SIZE (FRAMES_MAX / 32)      /* 32 frames per uint32 word         */

static uint32_t bitmap[BITMAP_SIZE];
static size_t   total_frames;
static size_t   used_frames;

static inline void bitmap_set(size_t frame) {
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static inline void bitmap_clear(size_t frame) {
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static inline int bitmap_test(size_t frame) {
    return (int)(bitmap[frame / 32] >> (frame % 32)) & 1;
}

/* Find the first free frame using a two-level search */
static size_t bitmap_first_free(void) {
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i] == 0xFFFFFFFF)
            continue;                       /* all 32 frames in this word used */
        for (int bit = 0; bit < 32; bit++) {
            if (!(bitmap[i] & (1u << bit)))
                return i * 32 + (size_t)bit;
        }
    }
    return (size_t)-1;                      /* out of memory */
}

void pmm_init(uint32_t mem_kb, uint32_t kernel_end) {
    total_frames = (mem_kb * 1024) / PAGE_SIZE;
    used_frames  = 0;

    /* Mark every frame as used to start — we'll free usable ones below */
    for (size_t i = 0; i < BITMAP_SIZE; i++)
        bitmap[i] = 0xFFFFFFFF;

    /* Free all RAM above the kernel up to total_frames */
    size_t first_free = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = first_free; i < total_frames; i++) {
        bitmap_clear(i);
    }

    /* Keep first 1 MiB reserved (BIOS, VGA, bootloader artefacts) */
    for (size_t i = 0; i < 256; i++)
        bitmap_set(i);

    /* Count used frames */
    used_frames = 0;
    for (size_t i = 0; i < total_frames; i++)
        if (bitmap_test(i))
            used_frames++;
}

void *pmm_alloc(void) {
    size_t frame = bitmap_first_free();
    if (frame == (size_t)-1)
        return NULL;                        /* out of memory */
    bitmap_set(frame);
    used_frames++;
    return (void *)(frame * PAGE_SIZE);
}

void pmm_free(void *addr) {
    size_t frame = (size_t)addr / PAGE_SIZE;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

size_t pmm_free_frames(void) {
    return total_frames - used_frames;
}
