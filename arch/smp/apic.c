#include "asm/apic.h"
#include "arch/io.h"
#include "relis/printk.h"
#include <stdint.h>

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

uint32_t lapic_get_id(void) {
    return lapic_read(LAPIC_ID) >> 24;
}

//  Use a simple delay instead of polling Delivery Status bit
static void wait_for_ipi_delivery(void) {
    for (volatile int i = 0; i < 1000000; i++);
}

void send_apic_init(uint8_t apic_id) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LO, 0x4500); // INIT IPI
    wait_for_ipi_delivery();
}

void send_apic_startup(uint8_t apic_id, uint32_t vector) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LO, 0x4600 | (vector & 0xFF)); // SIPI
    wait_for_ipi_delivery();
}

void remap_pic(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void ioapic_init(void) {
    for (int i = 0; i < 16; i++) {
        uint32_t low = 32 + i;
        low |= (0 << 8);
        low |= (0 << 11);
        low |= (0 << 16);
        uint32_t high = 0;
        ioapic_write(0x10 + i * 2, low);
        ioapic_write(0x11 + i * 2, high);
    }
    printk("APIC: IOAPIC configured for ISA interrupts");
}

void apic_init(void) {
    remap_pic();

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
    lo = (lo & ~(0xFFFFF << 12)) | 0xFEE00000;
    lo |= (1 << 11);   // Set APIC Enable bit
    lo &= ~(1 << 10);  // Ensure x2APIC is disabled
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(0x1B));

    lapic_write(0xE0, 0xFFFFFFFF); // DFR Flat mode
    lapic_write(LAPIC_SVR, LAPIC_ENABLE | 0xFF);
    lapic_write(LAPIC_ERROR, 0);

    ioapic_init();
    printk("APIC: Local APIC initialized (ID: %d)", lapic_get_id());
}
