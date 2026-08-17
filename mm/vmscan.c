// mm/vmscan.c
#include "relis/printk.h"
#include "relis/list.h"

struct page_lru {
    uint64_t phys_addr;
    struct list_head lru_list;
};

static LIST_HEAD(active_list);
static LIST_HEAD(inactive_list);

void lru_add_page(uint64_t phys) {
    // In a full kernel, this adds the page to the inactive LRU list
    (void)phys;
}

void shrink_slab(void) {
    // In a full kernel, this reclaims cached slab objects (dentries, inodes)
    printk("vmscan: Shrinking slab caches (LRU stub)");
}