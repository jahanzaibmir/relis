#include "relis/syscall.h"
#include "relis/printk.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include <stdint.h>

void syscall_init(void) {
    // For int 0x80, we don't need to set up MSRs.
    // The IDT entry for vector 128 is already set up in arch/idt.c.
    printk("Syscall interface initialized (int 0x80 active)");
}

void syscall_dispatch(struct registers *regs) {
    switch (regs->rax) {
        case 0: { // SYS_WRITE
            if (regs->rdi == 1 || regs->rdi == 2) {
                const char *buf = (const char*)regs->rsi;
                uint64_t len = regs->rdx;
                for (uint64_t i = 0; i < len; i++) {
                    terminal_putchar(buf[i]);
                    serial_putchar(buf[i]); // Write to serial so it shows in terminal!
                }
                regs->rax = len;
                return;
            }
            regs->rax = -1;
            return;
        }
        default:
            printk("Unknown syscall: %d", regs->rax);
            regs->rax = -1;
            return;
    }
}
