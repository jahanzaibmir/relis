#pragma once
#include <stdint.h>

struct registers {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct registers *);

void irq_init(void);
void request_irq(uint32_t irq, irq_handler_t handler);
void ack_irq(uint32_t irq);
void dispatch_irq(struct registers *regs);
void dispatch_isr(struct registers *regs);
