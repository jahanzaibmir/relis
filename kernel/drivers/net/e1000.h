/* kernel/drivers/net/e1000.h
 * Intel E1000 NIC driver — send/receive raw Ethernet frames
 * QEMU emulates this as PCI vendor 0x8086, device 0x100E
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* Ethernet frame size limits */
#define ETH_FRAME_MAX  1518
#define ETH_FRAME_MIN  64
#define ETH_ADDR_LEN   6

typedef struct {
    uint8_t addr[ETH_ADDR_LEN];
} mac_addr_t;

typedef struct __attribute__((packed)) {
    uint8_t  dst[ETH_ADDR_LEN];
    uint8_t  src[ETH_ADDR_LEN];
    uint16_t ethertype;
} eth_header_t;

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IP   0x0800

/* called by the driver when a frame arrives */
typedef void (*eth_rx_callback_t)(const uint8_t *frame, uint16_t len);

int  e1000_init(void);
int  e1000_send(const uint8_t *data, uint16_t len);
void e1000_register_rx(eth_rx_callback_t cb);
void e1000_get_mac(mac_addr_t *out);
void e1000_handle_irq(void);
void e1000_status(void);
