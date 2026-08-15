/*
 * RELIS — arch/io.h
 * Comprehensive hardware communication layer for x86_64.
 *
 * Includes:
 * 1. Port I/O (inb/outb)
 * 2. Block Port I/O (insw/outsw/insl/outsl) for fast disk/ATA transfers
 * 3. Memory Mapped I/O (readb/readl/readq) for PCIe & Network devices
 * 4. Memory Barriers (mb/rmb/wmb) to enforce CPU instruction ordering
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

// compiler attributes
#define __always_inline inline __attribute__((always_inline))

/*8-bit, 16-bit, 32-bit Port I/O */
static __always_inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static __always_inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static __always_inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
static __always_inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static __always_inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
static __always_inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*Block Port I/O */
static __always_inline void insw(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile("rep insw"
        : "+D"(addr), "+c"(count)
        : "d"(port)
        : "memory");
}
static __always_inline void outsw(uint16_t port, const void *addr, uint32_t count) {
    __asm__ volatile("rep outsw"
        : "+S"(addr), "+c"(count)
        : "d"(port)
        : "memory");
}

static __always_inline void insl(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile("rep insl"
        : "+D"(addr), "+c"(count)
        : "d"(port)
        : "memory");
}
static __always_inline void outsl(uint16_t port, const void *addr, uint32_t count) {
    __asm__ volatile("rep outsl"
        : "+S"(addr), "+c"(count)
        : "d"(port)
        : "memory");
}

/*Memory Mapped I/O  */
static __always_inline uint8_t readb(const volatile void *addr) {
    return *(volatile uint8_t *)addr;
}
static __always_inline uint16_t readw(const volatile void *addr) {
    return *(volatile uint16_t *)addr;
}
static __always_inline uint32_t readl(const volatile void *addr) {
    return *(volatile uint32_t *)addr;
}
static __always_inline uint64_t readq(const volatile void *addr) {
    return *(volatile uint64_t *)addr;
}

static __always_inline void writeb(volatile void *addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
}
static __always_inline void writew(volatile void *addr, uint16_t val) {
    *(volatile uint16_t *)addr = val;
}
static __always_inline void writel(volatile void *addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}
static __always_inline void writeq(volatile void *addr, uint64_t val) {
    *(volatile uint64_t *)addr = val;
}

/* memory barriers */
static __always_inline void mb(void)  { __asm__ volatile("mfence" ::: "memory"); }
static __always_inline void rmb(void) { __asm__ volatile("lfence" ::: "memory"); }
static __always_inline void wmb(void) { __asm__ volatile("sfence" ::: "memory"); }

/* i/o wait */
static __always_inline void io_wait(void) {
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}
