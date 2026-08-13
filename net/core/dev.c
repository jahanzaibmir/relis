#include "relis/net.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

#define MAX_NETDEVS 8
static struct net_device *netdevs[MAX_NETDEVS];
static int netdev_count = 0;

struct net_device *alloc_netdev(const char *name) {
    struct net_device *dev = kmalloc(sizeof(struct net_device));
    if (!dev) return NULL;
    kmemset(dev, 0, sizeof(struct net_device));
    kstrncpy(dev->name, name, 15);
    return dev;
}

int register_netdev(struct net_device *dev) {
    if (netdev_count >= MAX_NETDEVS) return -1;
    netdevs[netdev_count++] = dev;
    printk("Registered netdev: %s", dev->name);
    return 0;
}

void netif_rx(struct sk_buff *skb) {
    if (!skb) return;
    printk("RX Packet: len=%d", skb->len);
    kfree(skb);
}
