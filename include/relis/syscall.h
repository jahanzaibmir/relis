#pragma once
#include "relis/irq.h"
void syscall_init(void);
void syscall_dispatch(struct registers *regs);