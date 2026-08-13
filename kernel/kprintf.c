// kernel/kprintf.c
#include "relis/printk.h"
#include <stdarg.h>

void kprintf(const char *fmt, ...) {
    // For now, just forward to printk without parsing varargs
    // to keep the old FS/Net code compiling.
    printk(fmt);
}