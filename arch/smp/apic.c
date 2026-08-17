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

void send_apic_init(uint8_t apic_id) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LO, APIC_INIT | 0x4000); // Assert INIT
}

void send_apic_startup(uint8_t apic_id, uint32_t vector) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LO, APIC_STARTUP | (vector & 0xFF));
}

static void remap_pic(void) {
    // Mask all 8259 PIC interrupts to disable it completely
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void apic_init(void) {
    remap_pic();
    
    // Enable Local APIC by setting Spurious Vector Register
    lapic_write(LAPIC_SVR, LAPIC_ENABLE | 0xFF); // Vector 0xFF for spurious
    
    // Set Error Handling Vector
    lapic_write(LAPIC_ERROR, 0xFE); // Vector 0xFE for errors
    
    printk("APIC: Local APIC initialized (ID: %d)", lapic_get_id());
}
