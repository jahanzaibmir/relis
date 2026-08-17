#include "sched.h"
#include "relis/printk.h"

// Basic yield syscall equivalent
void sys_sched_yield(void) {
    schedule();
}
