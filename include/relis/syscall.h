#pragma once
#include "relis/irq.h"

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_READ   2
#define SYS_OPEN   3
#define SYS_CLOSE  4
#define SYS_FORK   57
#define SYS_EXECVE 59

// Phase 7 IPC
#define SYS_KILL     62
#define SYS_SIGACTION 134
#define SYS_PIPE     22
#define SYS_SHMGET   29
#define SYS_SHMAT    30

void syscall_init(void);
void syscall_dispatch(struct registers *regs);
