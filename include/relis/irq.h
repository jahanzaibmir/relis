#pragma once
#include <stdint.h>

struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

typedef void (*irq_handler_t)(struct registers *);

void irq_init(void);
void request_irq(uint32_t irq, irq_handler_t handler);
void ack_irq(uint32_t irq);
void dispatch_irq(struct registers *regs);