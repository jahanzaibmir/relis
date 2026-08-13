/*
 * RELIS — kernel/syscall/syscall.c
 * System call dispatcher — called from isr_stubs.asm via int 0x80.
 *
 * eax holds the syscall number on entry.
 * Return value is written back into regs->eax.
 */
#include "syscall.h"
#include "proc/process.h"
#include "drivers/vga.h"
#include "kprintf.h"
#include <stdint.h>

void syscall_init(void) {
    kprintf("[syscall] interface ready (int 0x80)\n");
}

void syscall_dispatch(registers_t *regs) {
    uint32_t num  = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;

    switch (num) {
        case SYS_EXIT:
            proc_exit((int)arg1);
            break;

        case SYS_WRITE: {
            /* arg1 = fd (ignored for now), arg2 = buf, arg3 = len */
            const char *buf = (const char *)(uintptr_t)arg2;
            uint32_t    len = arg3;
            for (uint32_t i = 0; i < len; i++)
                terminal_putchar(buf[i]);
            regs->eax = len;
            break;
        }

        case SYS_GETPID:
            regs->eax = proc_current()->pid;
            break;

        case SYS_SLEEP:
            proc_sleep(arg1);
            regs->eax = 0;
            break;

        case SYS_YIELD:
            proc_yield();
            regs->eax = 0;
            break;

        default:
            kprintf("[syscall] unknown syscall %u\n", num);
            regs->eax = (uint32_t)-1;
            break;
    }

    (void)arg2; (void)arg3;
}
