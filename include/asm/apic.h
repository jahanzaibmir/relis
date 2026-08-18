#pragma once
#include <stdint.h>

#define LAPIC_BASE_PHYS 0xFEE00000
#define IOAPIC_BASE_PHYS 0xFEC00000

#define LAPIC_VADDR  0xFFFFC90000000000ULL
#define IOAPIC_VADDR 0xFFFFC90000001000ULL

#define LAPIC_ID          0x020
#define LAPIC_EOI         0x0B0
#define LAPIC_SVR         0x0F0
#define LAPIC_ERROR       0x280
#define LAPIC_ICR_HI      0x310
#define LAPIC_ICR_LO      0x300

#define IOAPIC_REG  0x00
#define IOAPIC_DATA 0x10

#define LAPIC_ENABLE 0x100

static inline void lapic_write(uint32_t offset, uint32_t val) {
    volatile uint32_t *base = (volatile uint32_t*)LAPIC_VADDR;
    base[offset / 4] = val;
}

static inline uint32_t lapic_read(uint32_t offset) {
    volatile uint32_t *base = (volatile uint32_t*)LAPIC_VADDR;
    return base[offset / 4];
}

static inline void ioapic_write(uint8_t offset, uint32_t val) {
    volatile uint32_t *base = (volatile uint32_t*)IOAPIC_VADDR;
    base[0] = offset;
    base[4] = val;
}

static inline uint32_t ioapic_read(uint8_t offset) {
    volatile uint32_t *base = (volatile uint32_t*)IOAPIC_VADDR;
    base[0] = offset;
    return base[4];
}

void apic_init(void);
void ioapic_init(void);
void lapic_eoi(void);
uint32_t lapic_get_id(void);

void send_apic_init(uint8_t apic_id);
void send_apic_startup(uint8_t apic_id, uint32_t vector);
