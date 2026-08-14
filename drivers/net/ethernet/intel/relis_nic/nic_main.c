#include "relis_nic.h"
#include "drivers/pci/pci.h"
#include "relis/irq.h"
#include "relis/printk.h"

extern int nic_hw_init(uint64_t phys_bar0);
extern void nic_rx_init(void);
extern void nic_rx_poll(void);
extern uint32_t nic_read_reg(uint32_t offset);
extern void nic_write_reg(uint32_t offset, uint32_t value);

static void nic_irq_handler(struct registers *regs) {
    (void)regs;
    uint32_t icr = nic_read_reg(E1000_ICR);
    if (icr) {
        nic_rx_poll();
    }
}

int relis_nic_init(void) {
    pci_device_t dev;
    
    // 1. Find the E1000 device on the PCI bus using your superior function!
    if (!pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID, &dev)) {
        printk("RELIS NIC: No E1000 device found on PCI bus");
        return -1;
    }
    
    // 2. Enable Bus Mastering and MMIO using your superior function!
    pci_enable_device(&dev);
    
    // 3. Read the physical MMIO base from the decoded BAR0
    uint64_t phys_mmio = dev.bar[0];
    
    printk("RELIS NIC: Found E1000 at PCI %d:%d.%d, BAR0 Phys: 0x%x", 
           dev.bus, dev.slot, dev.func, phys_mmio);
    
    // 4. Initialize hardware and RX ring
    nic_hw_init(phys_mmio);
    nic_rx_init();
    
    // 5. Register interrupt handler (QEMU E1000 usually maps to IRQ 11 -> Vector 43)
    // Your pci_find_device reads the IRQ line into dev.irq!
    uint32_t irq_vector = 43; // Default fallback
    if (dev.irq != 0) {
        irq_vector = dev.irq + 32;
    }
    request_irq(irq_vector, nic_irq_handler);
    
    // 6. Enable Receive Timer Interrupt
    nic_write_reg(E1000_IMS, (1 << 0)); // RXT0
    
    printk("RELIS NIC: Driver initialized successfully (Using IRQ %d)", dev.irq);
    return 0;
}