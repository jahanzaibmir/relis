/* =============================================================================
 B lizzardOS — kernel/net/net.h                           *
 Network stack — ARP, IPv4, ICMP, UDP, DHCP client
 Works on: QEMU user-mode, QEMU TAP/bridge, VirtualBox NAT, VirtualBox bridged
 ============================================================================= */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "drivers/net/e1000.h"

typedef uint32_t ip4_addr_t;

#define IP4(a,b,c,d) \
((ip4_addr_t)(((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(d)))

#define IP4_BCAST   IP4(255,255,255,255)
#define IP4_ZERO    IP4(0,0,0,0)

/* ── Live network config — updated by DHCP ───────────────────────────────── */
extern ip4_addr_t net_ip;
extern ip4_addr_t net_gw;
extern ip4_addr_t net_mask;
extern ip4_addr_t net_dns;

/* ── Packet headers ──────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)){
    uint16_t htype,ptype;
    uint8_t  hlen,plen;
    uint16_t oper;
    uint8_t  sha[6],spa[4],tha[6],tpa[4];
} arp_packet_t;

typedef struct __attribute__((packed)){
    uint8_t  ver_ihl,dscp;
    uint16_t total_len,id,flags_frag;
    uint8_t  ttl,proto;
    uint16_t checksum;
    uint8_t  src[4],dst[4];
} ip4_header_t;

#define IP_PROTO_ICMP  1
#define IP_PROTO_UDP   17
#define IP_PROTO_TCP   6

typedef struct __attribute__((packed)){
    uint8_t  type,code;
    uint16_t checksum,id,seq;
} icmp_header_t;

#define ICMP_ECHO_REQUEST  8
#define ICMP_ECHO_REPLY    0

typedef struct __attribute__((packed)){
    uint16_t src_port,dst_port,length,checksum;
} udp_header_t;

/* ── DHCP ────────────────────────────────────────────────────────────────── */
#define DHCP_MAGIC          0x63825363u
#define DHCP_OP_REQUEST     1
#define DHCP_OP_REPLY       2
#define DHCP_PORT_SERVER    67
#define DHCP_PORT_CLIENT    68
#define DHCP_DISCOVER       1
#define DHCP_OFFER          2
#define DHCP_REQUEST        3
#define DHCP_ACK            5
#define DHCP_NAK            6

/* Fixed-size DHCP packet (options field sized for typical use) */
typedef struct __attribute__((packed)){
    uint8_t  op,htype,hlen,hops;
    uint32_t xid;
    uint16_t secs,flags;
    uint32_t ciaddr,yiaddr,siaddr,giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[312];
} dhcp_pkt_t;

/* ── UDP callback ────────────────────────────────────────────────────────── */
typedef void (*udp_rx_cb_t)(ip4_addr_t src_ip,uint16_t src_port,
                            const uint8_t *data,uint16_t len);

/* ── API ─────────────────────────────────────────────────────────────────── */
void     net_init(void);
int      dhcp_request(void);      /* returns 1=got IP, 0=timeout/failed */
void     net_print_info(void);

int      arp_lookup(ip4_addr_t ip,mac_addr_t *out);
void     arp_send_request(ip4_addr_t ip);

/* raw_ip4_send: send with explicit source IP (used by DHCP which needs 0.0.0.0) */
void     raw_ip4_send(ip4_addr_t src,ip4_addr_t dst,uint8_t proto,
                      const uint8_t *payload,uint16_t len);
void     ip4_send(ip4_addr_t dst,uint8_t proto,
                  const uint8_t *payload,uint16_t len);
void     icmp_send_ping(ip4_addr_t dst,uint16_t id,uint16_t seq);
void     udp_send(ip4_addr_t dst,uint16_t dst_port,uint16_t src_port,
                  const uint8_t *data,uint16_t len);
/* raw_udp_send: send with explicit source IP (used by DHCP) */
void     raw_udp_send(ip4_addr_t src_ip,ip4_addr_t dst_ip,
                      uint16_t dst_port,uint16_t src_port,
                      const uint8_t *data,uint16_t len);
void     udp_register_port(uint16_t port,udp_rx_cb_t cb);

/* Byte order */
static inline uint16_t htons(uint16_t x){return(uint16_t)((x>>8)|(x<<8));}
static inline uint16_t ntohs(uint16_t x){return htons(x);}
static inline uint32_t htonl(uint32_t x){
    return((x&0xFF000000u)>>24)|((x&0x00FF0000u)>>8)
    |((x&0x0000FF00u)<<8) |((x&0x000000FFu)<<24);
}
static inline uint32_t ntohl(uint32_t x){return htonl(x);}

uint16_t net_checksum(const void *data,uint16_t len);
uint16_t udp_checksum(ip4_addr_t src,ip4_addr_t dst,
                      const uint8_t *udp_seg,uint16_t len);
