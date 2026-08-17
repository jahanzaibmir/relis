#include "relis_nic.h"
#include "relis/mm.h"
#include "relis/printk.h"

extern void nic_write_reg(uint32_t offset, uint32_t value);
extern uint32_t nic_read_reg(uint32_t offset);

static struct e1000_rx_desc *rx_descs;
static void *rx_buffers[E1000_NUM_RX_DESC];

void nic_rx_init(void) {
    uint64_t phys_descs = alloc_page();
    rx_descs = (struct e1000_rx_desc*)phys_to_virt(phys_descs);
    
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        uint64_t buf_phys = alloc_page();
        rx_buffers[i] = phys_to_virt(buf_phys);
        rx_descs[i].buffer_addr = buf_phys;
        rx_descs[i].status = 0;
    }
    
    nic_write_reg(E1000_RDBAL, (uint32_t)phys_descs);
    nic_write_reg(E1000_RDBAH, (uint32_t)(phys_descs >> 32));
    nic_write_reg(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    nic_write_reg(E1000_RDH, 0);
    nic_write_reg(E1000_RDT, E1000_NUM_RX_DESC - 1);
    nic_write_reg(E1000_RCTL, (1 << 1) | (1 << 4));
    
    printk("NIC RX Ring initialized (%d descriptors)", E1000_NUM_RX_DESC);
}

void nic_rx_poll(void) {
    uint32_t tail = nic_read_reg(E1000_RDT);
    uint32_t next_tail = (tail + 1) % E1000_NUM_RX_DESC;
    
    if (rx_descs[next_tail].status & 0x01) {
        uint16_t len = rx_descs[next_tail].length;
        printk("NIC: Packet received! Length: %d bytes", len);
        
        rx_descs[next_tail].status = 0;
        nic_write_reg(E1000_RDT, next_tail);
    }
}
