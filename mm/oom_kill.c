// mm/oom_kill.c
#include "relis/printk.h"

void out_of_memory(void) {
    printk("OOM Killer: Out of physical memory! Selecting victim process...");
    // Since we have no userspace processes yet, we just panic.
    for(;;) __asm__ volatile("cli; hlt");
}