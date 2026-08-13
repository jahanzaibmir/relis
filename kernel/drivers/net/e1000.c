// kernel/drivers/net/e1000.c
#include "relis/printk.h"
#include "relis/irq.h"
#include <stdint.h>

// Stub to satisfy the linker. We will implement the real E1000 driver
// after paging and virtual memory are fully completed.
int e1000_init(void) {
    printk("E1000: Network disabled (stubbed for now)");
    return -1; 
}