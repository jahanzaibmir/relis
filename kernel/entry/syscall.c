#include "relis/syscall.h"
#include "relis/printk.h"
#include "relis/irq.h"

void syscall_init(void) {
    printk("Syscall interface initialized");
}

void syscall_dispatch(struct registers *regs) {
    (void)regs;
    // Syscalls will be dispatched here
}