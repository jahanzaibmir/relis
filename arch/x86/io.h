/*
 * RELIS — kernel/arch/x86/io.h
 * x86 port I/O inline functions — inb/outb/inw/outw/inl/outl + io_wait.
 *
 * All functions are static inline — no .c file needed.
 * The "memory" clobber prevents the compiler reordering I/O around other
 * memory accesses, which is essential for correct hardware communication.
 */
#pragma once
#include <stdint.h>

/* ── 8-bit port I/O ──────────────────────────────────────────────────────── */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/* ── 16-bit port I/O ─────────────────────────────────────────────────────── */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/* ── 32-bit port I/O ─────────────────────────────────────────────────────── */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/*
 * io_wait — burn ~1 µs by writing to an unused diagnostic port.
 * Required between consecutive writes to slow hardware (8259 PIC, 8042 KBC).
 */
static inline void io_wait(void) { outb(0x80, 0); }
