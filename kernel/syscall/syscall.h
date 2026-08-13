/*
 * RELIS — kernel/syscall/syscall.h
 * System call interface — user programs invoke via 'int 0x80'.
 *
 * Call convention (mirrors Linux i386 ABI):
 *   eax = syscall number
 *   ebx = arg1, ecx = arg2, edx = arg3
 *   return value in eax
 */
#pragma once
#include <stdint.h>
#include "idt.h"

/* Syscall numbers */
#define SYS_EXIT    1
#define SYS_WRITE   2
#define SYS_READ    3
#define SYS_OPEN    4
#define SYS_CLOSE   5
#define SYS_GETPID  6
#define SYS_SLEEP   7
#define SYS_YIELD   8

void syscall_init(void);

/* Called by isr128 trampoline in isr_stubs.asm */
void syscall_dispatch(registers_t *regs);
