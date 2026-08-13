/*
 * RELIS — kernel/arch/x86/idt.h
 * Interrupt Descriptor Table — exception and IRQ dispatch.
 *
 * registers_t matches the stack layout built by isr_stubs.asm.
 * The C handlers isr_handler() and irq_handler() receive a pointer to this.
 */
#pragma once
#include <stdint.h>

/* Saved CPU state — built on the kernel stack by isr_stubs.asm */
typedef struct __attribute__((packed)) {
    uint32_t ds;                        /* data segment saved before switch   */
    uint32_t edi, esi, ebp, esp_dummy;  /* pusha output (esp_dummy discarded) */
    uint32_t ebx, edx, ecx, eax;        /* pusha output (continued)           */
    uint32_t int_no, err_code;           /* pushed by our stub / CPU           */
    uint32_t eip, cs, eflags;           /* pushed by CPU on interrupt entry   */
    uint32_t useresp, ss;               /* pushed by CPU only on ring-3 entry */
} registers_t;

void idt_init(void);
void irq_register_handler(int irq, void (*handler)(registers_t *));
