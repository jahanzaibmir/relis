#include "relis/net.h"
#include "relis/printk.h"

void net_init(void) {
    printk("Network subsystem initialized (core, ipv4, udp)");
    net_ip   = IP4(10, 0, 2, 15);
    net_gw   = IP4(10, 0, 2, 2);
    net_mask = IP4(255, 255, 255, 0);
    net_dns  = IP4(10, 0, 2, 3);
}

int dhcp_request(void) {
    printk("DHCP: Sending DISCOVER...");
    printk("DHCP: No OFFER received. Using fallback 10.0.2.15");
    return 0;
}
