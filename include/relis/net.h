#pragma once
#include <stdint.h>
#include <stddef.h>
#include "relis/types.h"

#define IP4(a,b,c,d) ((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))

struct sk_buff {
    uint8_t  data[1536];
    uint32_t len;
    uint32_t protocol;
    struct net_device *dev;
};

struct net_device {
    char name[16];
    uint32_t ip_addr;
    uint32_t gateway;
    uint32_t netmask;
    uint8_t  mac_addr[6];
    int (*hard_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void (*rx_handler)(struct sk_buff *skb);
};

struct iphdr {
    uint8_t  ihl:4, version:4;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__((packed));

struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
} __attribute__((packed));

void net_init(void);
int dhcp_request(void);
void netif_rx(struct sk_buff *skb);
struct net_device *alloc_netdev(const char *name);
int register_netdev(struct net_device *dev);
int ip_send_packet(uint32_t dest_ip, uint8_t protocol, const uint8_t *data, uint32_t len);
int udp_send_packet(uint32_t dest_ip, uint16_t dest_port, const uint8_t *data, uint32_t len);

extern uint32_t net_ip;
extern uint32_t net_gw;
extern uint32_t net_mask;
extern uint32_t net_dns;
