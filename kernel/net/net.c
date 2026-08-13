/* =============================================================================
 *  RELIS — kernel/net/net.c
 *  DIAGNOSTIC VERSION — logs every step of DHCP to serial
 *  ============================================================================= */
#include "net.h"
#include "drivers/net/e1000.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include <stddef.h>
#include <stdint.h>

ip4_addr_t net_ip   = 0;
ip4_addr_t net_gw   = IP4(10,0,2,2);
ip4_addr_t net_mask = IP4(255,255,255,0);
ip4_addr_t net_dns  = IP4(10,0,2,3);

static mac_addr_t our_mac;

/* ── Serial helpers ──────────────────────────────────────────────────────── */
static void sn_u8(uint8_t v){
    char s[4];int i=0;
    if(v>=100)s[i++]=(char)('0'+v/100);
    if(v>=10) s[i++]=(char)('0'+(v/10)%10);
    s[i++]=(char)('0'+v%10);s[i]='\0';serial_write(s);
}
static void sn_ip(ip4_addr_t ip){
    sn_u8((uint8_t)(ip>>24));serial_write(".");
    sn_u8((uint8_t)(ip>>16));serial_write(".");
    sn_u8((uint8_t)(ip>>8)); serial_write(".");
    sn_u8((uint8_t)(ip));
}
static void sn_hex8(uint8_t v){
    const char *h="0123456789abcdef";
    char s[3];s[0]=h[v>>4];s[1]=h[v&0xF];s[2]='\0';serial_write(s);
}
static void sn_hex16(uint16_t v){sn_hex8(v>>8);sn_hex8(v&0xFF);}
static void sn_hex32(uint32_t v){sn_hex8(v>>24);sn_hex8(v>>16);sn_hex8(v>>8);sn_hex8(v);}
static void sn_dec32(uint32_t v){
    char s[12];int i=11;s[i]='\0';
    if(!v){serial_write("0");return;}
    while(v){s[--i]=(char)('0'+v%10);v/=10;}
    serial_write(s+i);
}

/* ── ARP cache ───────────────────────────────────────────────────────────── */
#define ARP_CACHE 16
typedef struct{ip4_addr_t ip;mac_addr_t mac;int valid;}arp_entry_t;
static arp_entry_t arp_cache[ARP_CACHE];

static void arp_add(ip4_addr_t ip,const uint8_t *mac){
    for(int i=0;i<ARP_CACHE;i++){
        if(arp_cache[i].valid&&arp_cache[i].ip==ip){
            for(int j=0;j<6;j++)arp_cache[i].mac.addr[j]=mac[j];return;}
    }
    for(int i=0;i<ARP_CACHE;i++){
        if(!arp_cache[i].valid){
            arp_cache[i].ip=ip;arp_cache[i].valid=1;
            for(int j=0;j<6;j++)arp_cache[i].mac.addr[j]=mac[j];return;}
    }
    arp_cache[0].ip=ip;arp_cache[0].valid=1;
    for(int j=0;j<6;j++)arp_cache[0].mac.addr[j]=mac[j];
}

int arp_lookup(ip4_addr_t ip,mac_addr_t *out){
    for(int i=0;i<ARP_CACHE;i++){
        if(arp_cache[i].valid&&arp_cache[i].ip==ip){*out=arp_cache[i].mac;return 1;}
    }
    return 0;
}

/* ── Checksum ────────────────────────────────────────────────────────────── */
uint16_t net_checksum(const void *data,uint16_t len){
    const uint16_t *p=(const uint16_t*)data;uint32_t sum=0;
    while(len>1){sum+=*p++;len-=2;}
    if(len)sum+=*(const uint8_t*)p;
    while(sum>>16)sum=(sum&0xFFFF)+(sum>>16);
    return(uint16_t)(~sum);
}

uint16_t udp_checksum(ip4_addr_t src,ip4_addr_t dst,
                      const uint8_t *seg,uint16_t len){
    uint8_t pseudo[12];
    pseudo[0]=(uint8_t)(src>>24);pseudo[1]=(uint8_t)(src>>16);
    pseudo[2]=(uint8_t)(src>>8); pseudo[3]=(uint8_t)(src);
    pseudo[4]=(uint8_t)(dst>>24);pseudo[5]=(uint8_t)(dst>>16);
    pseudo[6]=(uint8_t)(dst>>8); pseudo[7]=(uint8_t)(dst);
    pseudo[8]=0;pseudo[9]=IP_PROTO_UDP;
    pseudo[10]=(uint8_t)(len>>8);pseudo[11]=(uint8_t)(len);
    uint32_t sum=0;
    const uint16_t *ph=(const uint16_t*)pseudo;
    for(int i=0;i<6;i++)sum+=*ph++;
    const uint16_t *ps=(const uint16_t*)seg;uint16_t l2=len;
    while(l2>1){sum+=*ps++;l2-=2;}
    if(l2)sum+=*(const uint8_t*)ps;
    while(sum>>16)sum=(sum&0xFFFF)+(sum>>16);
    return(uint16_t)(~sum);
                      }

                      /* ── Ethernet send ───────────────────────────────────────────────────────── */
                      static uint8_t eth_frame[ETH_FRAME_MAX];
                      static const uint8_t ETH_BCAST[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

                      static void eth_send_raw(const uint8_t *dst_mac,uint16_t et,
                                               const uint8_t *payload,uint16_t plen){
                          if(sizeof(eth_header_t)+plen>ETH_FRAME_MAX)return;
                          eth_header_t *h=(eth_header_t*)eth_frame;
                          for(int i=0;i<6;i++){h->dst[i]=dst_mac[i];h->src[i]=our_mac.addr[i];}
                          h->ethertype=htons(et);
                          for(uint16_t i=0;i<plen;i++)eth_frame[sizeof(eth_header_t)+i]=payload[i];
                          int r=e1000_send(eth_frame,(uint16_t)(sizeof(eth_header_t)+plen));
                          if(r!=0){serial_write("[eth] SEND FAILED ret=");sn_hex8((uint8_t)r);serial_write("\n");}
                                               }

                                               /* ── ARP ─────────────────────────────────────────────────────────────────── */
                                               void arp_send_request(ip4_addr_t target){
                                                   arp_packet_t p;
                                                   p.htype=htons(1);p.ptype=htons(ETHERTYPE_IP);p.hlen=6;p.plen=4;p.oper=htons(1);
                                                   for(int i=0;i<6;i++){p.sha[i]=our_mac.addr[i];p.tha[i]=0;}
                                                   uint32_t s=htonl(net_ip),t=htonl(target);
                                                   for(int i=0;i<4;i++){p.spa[i]=(uint8_t)(s>>((3-i)*8));p.tpa[i]=(uint8_t)(t>>((3-i)*8));}
                                                   eth_send_raw(ETH_BCAST,ETHERTYPE_ARP,(const uint8_t*)&p,sizeof(p));
                                               }

                                               static void arp_handle(const uint8_t *payload,uint16_t len){
                                                   if(len<sizeof(arp_packet_t))return;
                                                   const arp_packet_t *p=(const arp_packet_t*)payload;
                                                   if(ntohs(p->htype)!=1||ntohs(p->ptype)!=ETHERTYPE_IP)return;
                                                   ip4_addr_t sender=((uint32_t)p->spa[0]<<24)|((uint32_t)p->spa[1]<<16)
                                                   |((uint32_t)p->spa[2]<<8)|(uint32_t)p->spa[3];
                                                   arp_add(sender,p->sha);
                                                   if(ntohs(p->oper)!=1||net_ip==0)return;
                                                   ip4_addr_t tgt=((uint32_t)p->tpa[0]<<24)|((uint32_t)p->tpa[1]<<16)
                                                   |((uint32_t)p->tpa[2]<<8)|(uint32_t)p->tpa[3];
                                                   if(tgt!=net_ip)return;
                                                   arp_packet_t r;
                                                   r.htype=htons(1);r.ptype=htons(ETHERTYPE_IP);r.hlen=6;r.plen=4;r.oper=htons(2);
                                                   for(int i=0;i<6;i++){r.sha[i]=our_mac.addr[i];r.tha[i]=p->sha[i];}
                                                   uint32_t s=htonl(net_ip);
                                                   for(int i=0;i<4;i++){r.spa[i]=(uint8_t)(s>>((3-i)*8));r.tpa[i]=p->spa[i];}
                                                   eth_send_raw(p->sha,ETHERTYPE_ARP,(const uint8_t*)&r,sizeof(r));
                                               }

                                               /* ── IPv4 ────────────────────────────────────────────────────────────────── */
                                               static uint16_t ip_id=1;
                                               static uint8_t  ip_pkt[ETH_FRAME_MAX];

                                               void raw_ip4_send(ip4_addr_t src,ip4_addr_t dst,uint8_t proto,
                                                                 const uint8_t *payload,uint16_t plen){
                                                   uint16_t total=(uint16_t)(sizeof(ip4_header_t)+plen);
                                                   if(total>ETH_FRAME_MAX)return;
                                                   ip4_header_t *h=(ip4_header_t*)ip_pkt;
                                                   h->ver_ihl=0x45;h->dscp=0;h->total_len=htons(total);
                                                   h->id=htons(ip_id++);h->flags_frag=0;h->ttl=64;h->proto=proto;h->checksum=0;
                                                   uint32_t s=htonl(src),d=htonl(dst);
                                                   for(int i=0;i<4;i++){
                                                       h->src[i]=(uint8_t)(s>>((3-i)*8));
                                                       h->dst[i]=(uint8_t)(d>>((3-i)*8));
                                                   }
                                                   h->checksum=net_checksum(h,sizeof(ip4_header_t));
                                                   for(uint16_t i=0;i<plen;i++) ip_pkt[sizeof(ip4_header_t)+i]=payload[i];

                                                   /* Broadcast → Ethernet broadcast, no ARP */
                                                   if(dst==IP4_BCAST){
                                                       eth_send_raw(ETH_BCAST,ETHERTYPE_IP,ip_pkt,total);
                                                       return;
                                                   }
                                                   /* Subnet broadcast */
                                                   if(net_ip!=0&&net_mask!=0&&dst==(net_ip|(~net_mask))){
                                                       eth_send_raw(ETH_BCAST,ETHERTYPE_IP,ip_pkt,total);
                                                       return;
                                                   }
                                                   /* Unicast — ARP lookup */
                                                   ip4_addr_t next=dst;
                                                   if(net_ip!=0&&(dst&net_mask)!=(net_ip&net_mask)) next=net_gw;
                                                   mac_addr_t dm;
                                                   if(!arp_lookup(next,&dm)){
                                                       arp_send_request(next);
                                                       serial_write("[net] ARP miss ");sn_ip(next);serial_write("\n");
                                                       return;
                                                   }
                                                   eth_send_raw(dm.addr,ETHERTYPE_IP,ip_pkt,total);
                                                                 }

                                                                 void ip4_send(ip4_addr_t dst,uint8_t proto,const uint8_t *payload,uint16_t plen){
                                                                     raw_ip4_send(net_ip,dst,proto,payload,plen);
                                                                 }

                                                                 /* ── ICMP ────────────────────────────────────────────────────────────────── */
                                                                 void icmp_send_ping(ip4_addr_t dst,uint16_t id,uint16_t seq){
                                                                     uint8_t buf[sizeof(icmp_header_t)+32];
                                                                     icmp_header_t *h=(icmp_header_t*)buf;
                                                                     h->type=ICMP_ECHO_REQUEST;h->code=0;h->checksum=0;
                                                                     h->id=htons(id);h->seq=htons(seq);
                                                                     for(int i=0;i<32;i++)buf[sizeof(icmp_header_t)+i]=(uint8_t)i;
                                                                     h->checksum=net_checksum(buf,sizeof(buf));
                                                                     ip4_send(dst,IP_PROTO_ICMP,buf,sizeof(buf));
                                                                     serial_write("[icmp] ping → ");sn_ip(dst);serial_write("\n");
                                                                 }

                                                                 static void icmp_handle(ip4_addr_t src,const uint8_t *p,uint16_t len){
                                                                     if(len<sizeof(icmp_header_t))return;
                                                                     const icmp_header_t *h=(const icmp_header_t*)p;
                                                                     if(h->type==ICMP_ECHO_REQUEST){
                                                                         uint8_t r[512];if(len>512)return;
                                                                         icmp_header_t *rh=(icmp_header_t*)r;
                                                                         for(uint16_t i=0;i<len;i++)r[i]=p[i];
                                                                         rh->type=ICMP_ECHO_REPLY;rh->checksum=0;rh->checksum=net_checksum(r,len);
                                                                         ip4_send(src,IP_PROTO_ICMP,r,len);
                                                                     }else if(h->type==ICMP_ECHO_REPLY){
                                                                         serial_write("[icmp] pong ← ");sn_ip(src);
                                                                         serial_write(" seq=");sn_u8((uint8_t)ntohs(h->seq));serial_write("\n");
                                                                     }
                                                                 }

                                                                 /* ── UDP ─────────────────────────────────────────────────────────────────── */
                                                                 #define UDP_MAX_BINDS 16
                                                                 typedef struct{uint16_t port;udp_rx_cb_t cb;}udp_bind_t;
                                                                 static udp_bind_t udp_binds[UDP_MAX_BINDS];
                                                                 static int        udp_nbinds=0;

                                                                 void udp_register_port(uint16_t port,udp_rx_cb_t cb){
                                                                     for(int i=0;i<udp_nbinds;i++){
                                                                         if(udp_binds[i].port==port){udp_binds[i].cb=cb;return;}
                                                                     }
                                                                     if(udp_nbinds>=UDP_MAX_BINDS)return;
                                                                     udp_binds[udp_nbinds].port=port;
                                                                     udp_binds[udp_nbinds].cb=cb;
                                                                     udp_nbinds++;
                                                                 }

                                                                 void raw_udp_send(ip4_addr_t src_ip,ip4_addr_t dst_ip,
                                                                                   uint16_t dst_port,uint16_t src_port,
                                                                                   const uint8_t *data,uint16_t len){
                                                                     static uint8_t buf[ETH_FRAME_MAX];
                                                                     uint16_t ulen=(uint16_t)(sizeof(udp_header_t)+len);
                                                                     udp_header_t *h=(udp_header_t*)buf;
                                                                     h->src_port=htons(src_port);h->dst_port=htons(dst_port);
                                                                     h->length=htons(ulen);h->checksum=0;
                                                                     for(uint16_t i=0;i<len;i++)buf[sizeof(udp_header_t)+i]=data[i];
                                                                     h->checksum=udp_checksum(src_ip,dst_ip,buf,ulen);
                                                                     raw_ip4_send(src_ip,dst_ip,IP_PROTO_UDP,buf,ulen);
                                                                                   }

                                                                                   void udp_send(ip4_addr_t dst,uint16_t dp,uint16_t sp,
                                                                                                 const uint8_t *data,uint16_t len){
                                                                                       raw_udp_send(net_ip,dst,dp,sp,data,len);
                                                                                                 }

                                                                                                 static void udp_handle(ip4_addr_t src,const uint8_t *p,uint16_t len){
                                                                                                     if(len<sizeof(udp_header_t))return;
                                                                                                     const udp_header_t *h=(const udp_header_t*)p;
                                                                                                     uint16_t dp=ntohs(h->dst_port),sp=ntohs(h->src_port);
                                                                                                     uint16_t dlen=(uint16_t)(ntohs(h->length)-sizeof(udp_header_t));
                                                                                                     const uint8_t *data=p+sizeof(udp_header_t);

                                                                                                     /* Log ALL incoming UDP so we can see what's arriving */
                                                                                                     serial_write("[udp] rx dst=");sn_hex16(dp);
                                                                                                     serial_write(" src=");sn_hex16(sp);
                                                                                                     serial_write(" from=");sn_ip(src);
                                                                                                     serial_write(" len=");sn_dec32(dlen);
                                                                                                     serial_write("\n");

                                                                                                     for(int i=0;i<udp_nbinds;i++){
                                                                                                         if(udp_binds[i].port==dp&&udp_binds[i].cb)
                                                                                                             udp_binds[i].cb(src,sp,data,dlen);
                                                                                                     }
                                                                                                 }

                                                                                                 /* ── IPv4 receive ────────────────────────────────────────────────────────── */
                                                                                                 static void ip4_handle(const uint8_t *p,uint16_t len){
                                                                                                     if(len<sizeof(ip4_header_t))return;
                                                                                                     const ip4_header_t *h=(const ip4_header_t*)p;
                                                                                                     if((h->ver_ihl>>4)!=4)return;
                                                                                                     ip4_addr_t src=((uint32_t)h->src[0]<<24)|((uint32_t)h->src[1]<<16)
                                                                                                     |((uint32_t)h->src[2]<<8)|(uint32_t)h->src[3];
                                                                                                     uint8_t  ihl=(h->ver_ihl&0xF)*4;
                                                                                                     uint16_t tot=ntohs(h->total_len);
                                                                                                     if(tot<ihl||ihl<20)return;
                                                                                                     if(h->proto==IP_PROTO_ICMP) icmp_handle(src,p+ihl,(uint16_t)(tot-ihl));
                                                                                                     else if(h->proto==IP_PROTO_UDP) udp_handle(src,p+ihl,(uint16_t)(tot-ihl));
                                                                                                 }

                                                                                                 /* ── Ethernet RX — log everything during DHCP ───────────────────────────── */
                                                                                                 static int log_rx=0;  /* set to 1 during DHCP to log all frames */

                                                                                                 static void eth_rx(const uint8_t *frame,uint16_t len){
                                                                                                     if(len<sizeof(eth_header_t))return;
                                                                                                     const eth_header_t *h=(const eth_header_t*)frame;
                                                                                                     uint16_t et=ntohs(h->ethertype);

                                                                                                     if(log_rx){
                                                                                                         serial_write("[eth] rx et=");sn_hex16(et);
                                                                                                         serial_write(" len=");sn_dec32(len);
                                                                                                         serial_write(" src=");
                                                                                                         for(int i=0;i<6;i++){sn_hex8(h->src[i]);if(i<5)serial_write(":");}
                                                                                                         serial_write("\n");
                                                                                                     }

                                                                                                     const uint8_t *p=frame+sizeof(eth_header_t);
                                                                                                     uint16_t pl=(uint16_t)(len-sizeof(eth_header_t));
                                                                                                     if(et==ETHERTYPE_ARP) arp_handle(p,pl);
                                                                                                     else if(et==ETHERTYPE_IP) ip4_handle(p,pl);
                                                                                                 }

                                                                                                 /* ═══════════════════════════════════════════════════════════════════════════
                                                                                                  *  DHCP CLIENT — RFC 2131
                                                                                                  *  ═══════════════════════════════════════════════════════════════════════════ */

                                                                                                 static volatile int        dhcp_got_offer=0;
                                                                                                 static volatile int        dhcp_got_ack  =0;
                                                                                                 static volatile ip4_addr_t dhcp_offered  =0;
                                                                                                 static volatile ip4_addr_t dhcp_server   =0;
                                                                                                 static volatile ip4_addr_t dhcp_gw       =0;
                                                                                                 static volatile ip4_addr_t dhcp_mask     =0;
                                                                                                 static volatile ip4_addr_t dhcp_dns      =0;
                                                                                                 static uint32_t            dhcp_xid      =0xB1177777u;

                                                                                                 static int dhcp_parse_opts(const uint8_t *opts,uint16_t len){
                                                                                                     uint16_t i=0;int msg=0;
                                                                                                     while(i<len){
                                                                                                         uint8_t tag=opts[i++];
                                                                                                         if(tag==255)break;
                                                                                                         if(tag==0)continue;
                                                                                                         if(i>=len)break;
                                                                                                         uint8_t l=opts[i++];
                                                                                                         if(i+l>len)break;
                                                                                                         switch(tag){
                                                                                                             case 53:msg=opts[i];
                                                                                                             serial_write("[dhcp] opt53 msgtype=");sn_hex8(opts[i]);serial_write("\n");
                                                                                                             break;
                                                                                                             case 1: if(l==4)dhcp_mask=((uint32_t)opts[i]<<24)|((uint32_t)opts[i+1]<<16)|((uint32_t)opts[i+2]<<8)|opts[i+3];break;
                                                                                                             case 3: if(l>=4)dhcp_gw  =((uint32_t)opts[i]<<24)|((uint32_t)opts[i+1]<<16)|((uint32_t)opts[i+2]<<8)|opts[i+3];break;
                                                                                                             case 6: if(l>=4)dhcp_dns =((uint32_t)opts[i]<<24)|((uint32_t)opts[i+1]<<16)|((uint32_t)opts[i+2]<<8)|opts[i+3];break;
                                                                                                             case 54:if(l==4)dhcp_server=((uint32_t)opts[i]<<24)|((uint32_t)opts[i+1]<<16)|((uint32_t)opts[i+2]<<8)|opts[i+3];break;
                                                                                                         }
                                                                                                         i+=l;
                                                                                                     }
                                                                                                     return msg;
                                                                                                 }

                                                                                                 static void dhcp_rx(ip4_addr_t src,uint16_t sport,
                                                                                                                     const uint8_t *data,uint16_t len){
                                                                                                     (void)src;(void)sport;
                                                                                                     serial_write("[dhcp] rx len=");sn_dec32(len);serial_write("\n");
                                                                                                     if(len<sizeof(dhcp_pkt_t)){
                                                                                                         serial_write("[dhcp] rx too short, need=");sn_dec32(sizeof(dhcp_pkt_t));serial_write("\n");
                                                                                                         return;
                                                                                                     }
                                                                                                     const dhcp_pkt_t *p=(const dhcp_pkt_t*)data;
                                                                                                     serial_write("[dhcp] op=");sn_hex8(p->op);
                                                                                                     serial_write(" xid=");sn_hex32(ntohl(p->xid));
                                                                                                     serial_write(" our=");sn_hex32(dhcp_xid);
                                                                                                     serial_write(" magic=");sn_hex32(ntohl(p->magic));
                                                                                                     serial_write(" yiaddr=");sn_ip(ntohl(p->yiaddr));
                                                                                                     serial_write("\n");
                                                                                                     if(p->op!=DHCP_OP_REPLY){serial_write("[dhcp] not a reply\n");return;}
                                                                                                     if(ntohl(p->xid)!=dhcp_xid){serial_write("[dhcp] xid mismatch\n");return;}
                                                                                                     if(ntohl(p->magic)!=DHCP_MAGIC){serial_write("[dhcp] bad magic\n");return;}

                                                                                                     /* options field offset inside dhcp_pkt_t */
                                                                                                     uint16_t opts_off=(uint16_t)offsetof(dhcp_pkt_t,options);
                                                                                                     uint16_t opts_len=(len>opts_off)?(uint16_t)(len-opts_off):0;
                                                                                                     int msg=dhcp_parse_opts(p->options,opts_len);

                                                                                                     if(msg==DHCP_OFFER&&!dhcp_got_offer){
                                                                                                         dhcp_offered=ntohl(p->yiaddr);
                                                                                                         dhcp_got_offer=1;
                                                                                                         serial_write("[dhcp] OFFER → ");sn_ip(dhcp_offered);serial_write("\n");
                                                                                                     }else if(msg==DHCP_ACK&&dhcp_got_offer){
                                                                                                         dhcp_offered=ntohl(p->yiaddr);
                                                                                                         dhcp_got_ack=1;
                                                                                                         serial_write("[dhcp] ACK → ");sn_ip(dhcp_offered);serial_write("\n");
                                                                                                     }else if(msg==DHCP_NAK){
                                                                                                         serial_write("[dhcp] NAK\n");
                                                                                                         dhcp_got_offer=0;dhcp_offered=0;dhcp_server=0;
                                                                                                     }
                                                                                                                     }

                                                                                                                     static void dhcp_send_pkt(uint8_t msg_type,ip4_addr_t req_ip,ip4_addr_t srv_ip){
                                                                                                                         static dhcp_pkt_t pkt;
                                                                                                                         uint8_t *pb=(uint8_t*)&pkt;
                                                                                                                         for(uint32_t i=0;i<sizeof(pkt);i++)pb[i]=0;
                                                                                                                         pkt.op=DHCP_OP_REQUEST;pkt.htype=1;pkt.hlen=6;pkt.hops=0;
                                                                                                                         pkt.xid=htonl(dhcp_xid);
                                                                                                                         pkt.secs=0;
                                                                                                                         pkt.flags=htons(0x8000);  /* broadcast flag */
                                                                                                                         pkt.ciaddr=0;pkt.yiaddr=0;pkt.siaddr=0;pkt.giaddr=0;
                                                                                                                         for(int i=0;i<6;i++)pkt.chaddr[i]=our_mac.addr[i];
                                                                                                                         pkt.magic=htonl(DHCP_MAGIC);

                                                                                                                         uint8_t *o=pkt.options;uint16_t n=0;
                                                                                                                         o[n++]=53;o[n++]=1;o[n++]=msg_type;
                                                                                                                         o[n++]=61;o[n++]=7;o[n++]=1;for(int i=0;i<6;i++)o[n++]=our_mac.addr[i];
                                                                                                                         if(msg_type==DHCP_REQUEST){
                                                                                                                             o[n++]=50;o[n++]=4;
                                                                                                                             o[n++]=(uint8_t)(req_ip>>24);o[n++]=(uint8_t)(req_ip>>16);
                                                                                                                             o[n++]=(uint8_t)(req_ip>>8);o[n++]=(uint8_t)(req_ip);
                                                                                                                             o[n++]=54;o[n++]=4;
                                                                                                                             o[n++]=(uint8_t)(srv_ip>>24);o[n++]=(uint8_t)(srv_ip>>16);
                                                                                                                             o[n++]=(uint8_t)(srv_ip>>8);o[n++]=(uint8_t)(srv_ip);
                                                                                                                         }
                                                                                                                         o[n++]=55;o[n++]=4;o[n++]=1;o[n++]=3;o[n++]=6;o[n++]=15;
                                                                                                                         o[n++]=255;

                                                                                                                         serial_write("[dhcp] sending type=");sn_hex8(msg_type);
                                                                                                                         serial_write(" size=");sn_dec32(sizeof(pkt));serial_write("\n");

                                                                                                                         /* src=0.0.0.0 dst=255.255.255.255 as required by RFC 2131 */
                                                                                                                         raw_udp_send(IP4_ZERO,IP4_BCAST,
                                                                                                                                      DHCP_PORT_SERVER,DHCP_PORT_CLIENT,
                                                                                                                                      (const uint8_t*)&pkt,sizeof(pkt));
                                                                                                                     }

                                                                                                                     int dhcp_request(void){
                                                                                                                         serial_write("[dhcp] === starting DHCP ===\n");
                                                                                                                         serial_write("[dhcp] our MAC: ");
                                                                                                                         for(int i=0;i<6;i++){sn_hex8(our_mac.addr[i]);if(i<5)serial_write(":");}
                                                                                                                         serial_write("\n");
                                                                                                                         serial_write("[dhcp] xid=");sn_hex32(dhcp_xid);serial_write("\n");
                                                                                                                         serial_write("[dhcp] dhcp_pkt_t size=");sn_dec32(sizeof(dhcp_pkt_t));serial_write("\n");

                                                                                                                         dhcp_got_offer=0;dhcp_got_ack=0;
                                                                                                                         dhcp_offered=0;dhcp_server=0;dhcp_gw=0;dhcp_mask=0;dhcp_dns=0;

                                                                                                                         udp_register_port(DHCP_PORT_CLIENT,dhcp_rx);
                                                                                                                         serial_write("[dhcp] registered port 68\n");

                                                                                                                         log_rx=1;  /* enable ethernet frame logging */

                                                                                                                         for(int attempt=0;attempt<3&&!dhcp_got_offer;attempt++){
                                                                                                                             serial_write("[dhcp] DISCOVER attempt ");sn_u8((uint8_t)(attempt+1));serial_write("\n");
                                                                                                                             dhcp_send_pkt(DHCP_DISCOVER,0,0);

                                                                                                                             /* Wait 5s for OFFER — poll NIC every 10ms */
                                                                                                                             for(int i=0;i<500&&!dhcp_got_offer;i++){
                                                                                                                                 e1000_handle_irq();
                                                                                                                                 uint64_t t=timer_ticks();while(timer_ticks()-t<1);
                                                                                                                             }
                                                                                                                             if(!dhcp_got_offer){serial_write("[dhcp] no offer after 5s\n");}
                                                                                                                         }

                                                                                                                         log_rx=0;

                                                                                                                         if(!dhcp_got_offer){
                                                                                                                             serial_write("[dhcp] FAILED — no OFFER received\n");
                                                                                                                             serial_write("[dhcp] check: VirtualBox promiscuous=AllowAll? bridged adapter selected?\n");
                                                                                                                             return 0;
                                                                                                                         }

                                                                                                                         dhcp_send_pkt(DHCP_REQUEST,dhcp_offered,dhcp_server);

                                                                                                                         log_rx=1;
                                                                                                                         for(int i=0;i<500&&!dhcp_got_ack;i++){
                                                                                                                             e1000_handle_irq();
                                                                                                                             uint64_t t=timer_ticks();while(timer_ticks()-t<1);
                                                                                                                         }
                                                                                                                         log_rx=0;

                                                                                                                         if(!dhcp_got_ack){serial_write("[dhcp] no ACK\n");return 0;}

                                                                                                                         net_ip  =dhcp_offered;
                                                                                                                         if(dhcp_mask)net_mask=dhcp_mask;
                                                                                                                         if(dhcp_gw)  net_gw  =dhcp_gw;
                                                                                                                         if(dhcp_dns) net_dns =dhcp_dns;

                                                                                                                         arp_send_request(net_ip); /* gratuitous ARP */

                                                                                                                         serial_write("[dhcp] SUCCESS\n");
                                                                                                                         serial_write("  IP  : ");sn_ip(net_ip);  serial_write("\n");
                                                                                                                         serial_write("  GW  : ");sn_ip(net_gw);  serial_write("\n");
                                                                                                                         serial_write("  Mask: ");sn_ip(net_mask);serial_write("\n");
                                                                                                                         serial_write("  DNS : ");sn_ip(net_dns); serial_write("\n");
                                                                                                                         return 1;
                                                                                                                     }

                                                                                                                     void net_print_info(void){
                                                                                                                         mac_addr_t mac;e1000_get_mac(&mac);
                                                                                                                         serial_write("[net] MAC : ");
                                                                                                                         for(int i=0;i<6;i++){sn_hex8(mac.addr[i]);if(i<5)serial_write(":");}
                                                                                                                         serial_write("\n[net] IP  : ");sn_ip(net_ip);
                                                                                                                         serial_write("\n[net] GW  : ");sn_ip(net_gw);
                                                                                                                         serial_write("\n[net] Mask: ");sn_ip(net_mask);
                                                                                                                         serial_write("\n[net] DNS : ");sn_ip(net_dns);serial_write("\n");
                                                                                                                     }

                                                                                                                     void net_init(void){
                                                                                                                         for(int i=0;i<ARP_CACHE;i++)arp_cache[i].valid=0;
                                                                                                                         udp_nbinds=0;net_ip=0;log_rx=0;
                                                                                                                         e1000_get_mac(&our_mac);
                                                                                                                         e1000_register_rx(eth_rx);
                                                                                                                         serial_write("[net] stack ready\n");
                                                                                                                     }
