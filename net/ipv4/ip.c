#include "relis/net.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

uint32_t net_ip = 0;
uint32_t net_gw = 0;
uint32_t net_mask = 0;
uint32_t net_dns = 0;

static uint16_t ip_checksum(struct iphdr *hdr) {
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)hdr;
    for (int i = 0; i < 10; i++) {
        sum += *p++;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

int ip_send_packet(uint32_t dest_ip, uint8_t protocol, const uint8_t *data, uint32_t len) {
    struct sk_buff *skb = kmalloc(sizeof(struct sk_buff));
    if (!skb) return -1;

    struct iphdr *hdr = (struct iphdr *)skb->data;
    hdr->version = 4;
    hdr->ihl = 5;
    hdr->tos = 0;
    hdr->tot_len = sizeof(struct iphdr) + len;
    hdr->id = 1;
    hdr->frag_off = 0;
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->saddr = net_ip;
    hdr->daddr = dest_ip;
    hdr->check = 0;
    hdr->check = ip_checksum(hdr);

    kmemcpy(skb->data + sizeof(struct iphdr), data, len);
    skb->len = hdr->tot_len;

    printk("IP Packet TX: proto=%d len=%d", protocol, skb->len);
    kfree(skb);
    return 0;
}
