#include "relis/net.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

int udp_send_packet(uint32_t dest_ip, uint16_t dest_port, const uint8_t *data, uint32_t len) {
    uint8_t buf[1024];
    struct udphdr *udp = (struct udphdr *)buf;
    udp->source = 1234;
    udp->dest = dest_port;
    udp->len = sizeof(struct udphdr) + len;
    udp->check = 0;

    kmemcpy(buf + sizeof(struct udphdr), data, len);
    
    return ip_send_packet(dest_ip, 17, buf, udp->len);
}
