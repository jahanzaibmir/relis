
#pragma once
#include <stdint.h>

// This struct MUST exactly match the push order in isr_stubs.asm.
// Because the stack grows downwards, the last register pushed (r15)
// ends up at the lowest memory address (offset 0).
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct registers *);

void irq_init(void);
void request_irq(uint32_t irq, irq_handler_t handler);
void ack_irq(uint32_t irq);
void dispatch_irq(struct registers *regs);
void dispatch_isr(struct registers *regs);
