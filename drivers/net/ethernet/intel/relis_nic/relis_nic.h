#pragma once
#include <stdint.h>

#define E1000_VENDOR_ID  0x8086
#define E1000_DEVICE_ID  0x100E

#define E1000_NUM_RX_DESC 32
#define E1000_RX_BUFFER_SIZE 2048

// E1000 Register Offsets
#define E1000_CTRL   0x0000
#define E1000_STATUS 0x0008
#define E1000_ICR    0x00C0
#define E1000_IMS    0x00D0
#define E1000_RCTL   0x0100
#define E1000_TCTL   0x0400
#define E1000_RDBAL  0x2800
#define E1000_RDBAH  0x2804
#define E1000_RDLEN  0x2808
#define E1000_RDH    0x2810
#define E1000_RDT    0x2818

// Receive Descriptor
struct e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t csum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

int relis_nic_init(void);