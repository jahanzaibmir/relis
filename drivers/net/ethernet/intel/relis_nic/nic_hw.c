#include "relis_nic.h"
#include "arch/io.h"
#include "asm/pgtable.h"
#include "relis/mm.h"
#include "relis/printk.h"

static uint8_t *nic_mmio_base = 0;

// Use a safe virtual address in the vmalloc range for MMIO
#define MMIO_VIRT_BASE 0xFFFFC90000000000ULL

void nic_write_reg(uint32_t offset, uint32_t value) {
    writel(nic_mmio_base + offset, value);
}

uint32_t nic_read_reg(uint32_t offset) {
    return readl(nic_mmio_base + offset);
}

int nic_hw_init(uint64_t phys_bar0) {
    // Dynamically map the physical MMIO address to a virtual address.
    // This is exactly what Linux's ioremap() does!
    // We map 3 pages because the E1000 registers span ~128KB.
    for (int i = 0; i < 3; i++) {
        arch_map_page(MMIO_VIRT_BASE + i * PAGE_SIZE, phys_bar0 + i * PAGE_SIZE, PTE_WRITABLE | PTE_PCD | PTE_PWT);
    }
    
    nic_mmio_base = (uint8_t*)MMIO_VIRT_BASE;
    
    // Reset the device
    nic_write_reg(E1000_CTRL, (1 << 26)); // RST bit
    for (volatile int i = 0; i < 1000000; i++); // wait for reset
    
    // Disable interrupts temporarily
    nic_write_reg(E1000_IMS, 0);
    
    printk("NIC HW Reset complete. MMIO Mapped Virt: 0x%x", nic_mmio_base);
    return 0;
}