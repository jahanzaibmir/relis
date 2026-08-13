/*
 * kernel/drivers/net/e1000.c
 * Intel E1000 NIC driver — QEMU 82540EM (PCI 8086:100E)
 *
 * BAR0 note: QEMU's E1000 uses a 64-bit prefetchable MMIO BAR.
 * The raw BAR0 value has type bits in the low nibble that must be masked off:
 *   bits[1:0] = 00 → 32-bit MMIO
 *   bits[1:0] = 10 → 64-bit MMIO  (BAR0 = low 32 bits, BAR1 = high 32 bits)
 * On QEMU the high 32 bits are always 0, but we read them correctly anyway.
 */
#include "e1000.h"
#include "drivers/pci/pci.h"
#include "idt.h"
#include "io.h"
#include "mm/heap.h"
#include "drivers/serial.h"
#include <stddef.h>
#include <stdint.h>
#define E1000_VENDOR  0x8086
#define E1000_DEVICE  0x100E

/* MMIO register offsets */
#define E1000_CTRL   0x0000
#define E1000_STATUS 0x0008
#define E1000_EECD   0x0010
#define E1000_EERD   0x0014
#define E1000_ICR    0x00C0
#define E1000_IMS    0x00D0
#define E1000_IMC    0x00D8
#define E1000_RCTL   0x0100
#define E1000_TCTL   0x0400
#define E1000_TIPG   0x0410
#define E1000_RDBAL  0x2800
#define E1000_RDBAH  0x2804
#define E1000_RDLEN  0x2808
#define E1000_RDH    0x2810
#define E1000_RDT    0x2818
#define E1000_TDBAL  0x3800
#define E1000_TDBAH  0x3804
#define E1000_TDLEN  0x3808
#define E1000_TDH    0x3810
#define E1000_TDT    0x3818
#define E1000_MTA    0x5200
#define E1000_RAL0   0x5400
#define E1000_RAH0   0x5404

/* CTRL bits */
#define CTRL_RST   (1u << 26)   /* software reset */
#define CTRL_SLU   (1u <<  6)   /* set link up */
#define CTRL_ASDE  (1u <<  5)   /* auto-speed detection */

/* RCTL bits */
#define RCTL_EN         (1u <<  1)  /* receiver enable */
#define RCTL_BAM        (1u << 15)  /* broadcast accept */
#define RCTL_BSIZE_2048 (0u << 16)  /* 2KB receive buffers */
#define RCTL_SECRC      (1u << 26)  /* strip CRC */

/* TCTL bits */
#define TCTL_EN  (1u << 1)  /* transmitter enable */
#define TCTL_PSP (1u << 3)  /* pad short packets */

/* TX descriptor command/status bits */
#define TDESC_CMD_EOP  (1u << 0)  /* end of packet */
#define TDESC_CMD_IFCS (1u << 1)  /* insert FCS */
#define TDESC_CMD_RS   (1u << 3)  /* report status */
#define TDESC_STA_DD   (1u << 0)  /* descriptor done */

/* RX descriptor status bits */
#define RDESC_STA_DD   (1u << 0)  /* descriptor done */

/* interrupt cause bits */
#define ICR_LSC    (1u << 2)  /* link status change */
#define ICR_RXDMT0 (1u << 4)  /* RX descriptor min threshold */
#define ICR_RXT0   (1u << 7)  /* RX timer */

#define RX_DESC_COUNT  16
#define TX_DESC_COUNT  8
#define RX_BUF_SIZE    2048

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso, cmd, status, css;
    uint16_t special;
} tx_desc_t;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length, checksum;
    uint8_t  status, errors;
    uint16_t special;
} rx_desc_t;

static volatile uint8_t *mmio_base   = NULL;
static mac_addr_t        our_mac;
static eth_rx_callback_t rx_cb       = NULL;
static int               e1000_ready = 0;

static tx_desc_t *tx_ring = NULL;
static rx_desc_t *rx_ring = NULL;
static uint8_t (*tx_buf)[ETH_FRAME_MAX] = NULL;
static uint8_t (*rx_buf)[RX_BUF_SIZE]   = NULL;

static uint32_t tx_tail = 0;
static uint32_t rx_tail = 0;

/* serial helpers */

static void e_hex8(uint8_t v)
{
    const char *h = "0123456789abcdef";
    char s[3];
    s[0] = h[v >> 4];
    s[1] = h[v & 0xF];
    s[2] = '\0';
    serial_write(s);
}

static void e_hex32(uint32_t v)
{
    serial_write("0x");
    for (int i = 28; i >= 0; i -= 4)
        e_hex8((uint8_t)((v >> i) & 0xF));
}

/* MMIO read/write */

static inline void e_wr(uint32_t reg, uint32_t val)
{
    *((volatile uint32_t *)(mmio_base + reg)) = val;
}

static inline uint32_t e_rd(uint32_t reg)
{
    return *((volatile uint32_t *)(mmio_base + reg));
}

/* EEPROM */

static uint16_t eeprom_read(uint8_t addr)
{
    e_wr(E1000_EERD, 1u | ((uint32_t)addr << 8));
    uint32_t v;
    uint32_t t = 100000;
    do { v = e_rd(E1000_EERD); } while (!(v & (1u << 4)) && --t);
    return (uint16_t)(v >> 16);
}

static void read_mac(void)
{
    uint16_t w0 = eeprom_read(0);
    uint16_t w1 = eeprom_read(1);
    uint16_t w2 = eeprom_read(2);

    our_mac.addr[0] = (uint8_t)(w0 & 0xFF);  our_mac.addr[1] = (uint8_t)(w0 >> 8);
    our_mac.addr[2] = (uint8_t)(w1 & 0xFF);  our_mac.addr[3] = (uint8_t)(w1 >> 8);
    our_mac.addr[4] = (uint8_t)(w2 & 0xFF);  our_mac.addr[5] = (uint8_t)(w2 >> 8);
}

/*  TX ring init */

static void init_tx(void)
{
    for (int i = 0; i < TX_DESC_COUNT; i++) {
        tx_ring[i].addr   = (uint64_t)(uintptr_t)tx_buf[i];
        tx_ring[i].status = TDESC_STA_DD;  /* mark all as done so send() can use them */
    }

    e_wr(E1000_TDBAL, (uint32_t)(uintptr_t)tx_ring);
    e_wr(E1000_TDBAH, 0);
    e_wr(E1000_TDLEN, TX_DESC_COUNT * sizeof(tx_desc_t));
    e_wr(E1000_TDH, 0);
    e_wr(E1000_TDT, 0);
    tx_tail = 0;

    e_wr(E1000_TCTL, TCTL_EN | TCTL_PSP | (0x0Fu << 4) | (0x040u << 12));
    e_wr(E1000_TIPG, 0x0060200A);
}

/* RX ring init */

static void init_rx(void)
{
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        rx_ring[i].addr   = (uint64_t)(uintptr_t)rx_buf[i];
        rx_ring[i].status = 0;
    }

    e_wr(E1000_RDBAL, (uint32_t)(uintptr_t)rx_ring);
    e_wr(E1000_RDBAH, 0);
    e_wr(E1000_RDLEN, RX_DESC_COUNT * sizeof(rx_desc_t));
    e_wr(E1000_RDH, 0);
    e_wr(E1000_RDT, RX_DESC_COUNT - 1);
    rx_tail = 0;

    /* program our MAC into the receive address filter */
    uint32_t ral = (uint32_t)our_mac.addr[0]
                 | ((uint32_t)our_mac.addr[1] <<  8)
                 | ((uint32_t)our_mac.addr[2] << 16)
                 | ((uint32_t)our_mac.addr[3] << 24);
    uint32_t rah = (uint32_t)our_mac.addr[4]
                 | ((uint32_t)our_mac.addr[5] << 8)
                 | (1u << 31);  /* address valid bit */

    e_wr(E1000_RAL0, ral);
    e_wr(E1000_RAH0, rah);

    /* clear multicast table */
    for (int i = 0; i < 128; i++)
        e_wr(E1000_MTA + i * 4, 0);

    e_wr(E1000_RCTL, RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);
}

/* IRQ handler */

void e1000_handle_irq(void)
{
    uint32_t icr = e_rd(E1000_ICR);

    if (icr & ICR_LSC)
        serial_write((e_rd(E1000_STATUS) & 2) ? "[e1000] link UP\n"
                                               : "[e1000] link DOWN\n");

    if (icr & (ICR_RXT0 | ICR_RXDMT0)) {
        while (rx_ring[rx_tail].status & RDESC_STA_DD) {
            uint16_t len = rx_ring[rx_tail].length;
            if (len > 0 && rx_cb)
                rx_cb((const uint8_t *)rx_buf[rx_tail], len);
            rx_ring[rx_tail].status = 0;
            e_wr(E1000_RDT, rx_tail);
            rx_tail = (rx_tail + 1) % RX_DESC_COUNT;
        }
    }
}

static void e1000_irq_handler(registers_t *regs)
{
    (void)regs;
    e1000_handle_irq();
}

/*  send a frame */

int e1000_send(const uint8_t *data, uint16_t len)
{
    if (!e1000_ready || len > ETH_FRAME_MAX) return -1;

    /* wait for the descriptor to be free */
    uint32_t t = 100000;
    while (!(tx_ring[tx_tail].status & TDESC_STA_DD) && --t);
    if (!t) {
        serial_write("[e1000] TX timeout\n");
        return -1;
    }

    for (uint16_t i = 0; i < len; i++)
        tx_buf[tx_tail][i] = data[i];

    tx_ring[tx_tail].length = len;
    tx_ring[tx_tail].cmd    = TDESC_CMD_EOP | TDESC_CMD_IFCS | TDESC_CMD_RS;
    tx_ring[tx_tail].status = 0;

    tx_tail = (tx_tail + 1) % TX_DESC_COUNT;
    e_wr(E1000_TDT, tx_tail);
    return 0;
}

/*                                  public helpers*/

void e1000_register_rx(eth_rx_callback_t cb) { rx_cb = cb; }

void e1000_get_mac(mac_addr_t *out)
{
    for (int i = 0; i < ETH_ADDR_LEN; i++)
        out->addr[i] = our_mac.addr[i];
}

void e1000_status(void)
{
    serial_write("[e1000] MAC: ");
    for (int i = 0; i < 6; i++) {
        e_hex8(our_mac.addr[i]);
        if (i < 5) serial_write(":");
    }
    serial_write("\n");
}

/* ── init ───── */

int e1000_init(void)
{
    pci_device_t dev;
    if (!pci_find_device(E1000_VENDOR, E1000_DEVICE, &dev)) {
        serial_write("[e1000] not found\n");
        return -1;
    }

    serial_write("[e1000] PCI slot=");
    e_hex8(dev.slot);
    serial_write(" IRQ=");
    e_hex8(dev.irq);
    serial_write("\n");

    pci_enable_device(&dev);

    /*
     * decode BAR0 — the raw value from PCI config space includes type bits
     * in the low nibble that must be masked off before using as an address:
     *   bit 0     = 0  → memory space (not I/O)
     *   bits[2:1] = 00 → 32-bit,  10 → 64-bit
     *   bit 3     = prefetchable
     *
     * pci.c's decode_bars() already masks these, but we re-read raw here
     * so we can log everything and detect the 64-bit case explicitly.
     */
    uint32_t bar0_raw  = pci_read32(dev.bus, dev.slot, dev.func, 0x10);
    uint32_t bar0_type = bar0_raw & 0x6;        /* bits[2:1] */
    uint32_t mmio_low  = bar0_raw & 0xFFFFFFF0; /* mask off type bits */
    uint32_t mmio_high = 0;

    if (bar0_type == 0x4) {
        /* 64-bit BAR — high 32 bits live in BAR1 */
        mmio_high = pci_read32(dev.bus, dev.slot, dev.func, 0x14);
    }

    serial_write("[e1000] BAR0 raw=");   e_hex32(bar0_raw);
    serial_write(" type=");              e_hex8((uint8_t)bar0_type);
    serial_write(" MMIO_low=");          e_hex32(mmio_low);
    serial_write(" MMIO_high=");         e_hex32(mmio_high);
    serial_write("\n");

    if (mmio_high != 0) {
        serial_write("[e1000] MMIO above 4GB — cannot map in 32-bit mode\n");
        return -1;
    }
    if (mmio_low == 0) {
        serial_write("[e1000] BAR0 is zero — BIOS did not assign MMIO\n");
        return -1;
    }

    mmio_base = (volatile uint8_t *)(uintptr_t)mmio_low;
    serial_write("[e1000] MMIO mapped at ");
    e_hex32(mmio_low);
    serial_write("\n");

    /* map 128KB (32 pages) of E1000 MMIO registers into the kernel page table */
    extern void  paging_map_page(void *, uint32_t, uint32_t, uint32_t);
    extern void *paging_kernel_directory(void);
    void *dir = paging_kernel_directory();
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t addr = mmio_low + i * 4096;
        paging_map_page(dir, addr, addr, 0x3);  /* PRESENT | WRITE */
    }
    serial_write("[e1000] MMIO pages mapped\n");

    /* software reset — wait for RST bit to clear */
    e_wr(E1000_CTRL, e_rd(E1000_CTRL) | CTRL_RST);
    uint32_t t = 100000;
    while ((e_rd(E1000_CTRL) & CTRL_RST) && --t);
    if (!t) {
        serial_write("[e1000] reset timeout\n");
        return -1;
    }
    serial_write("[e1000] reset OK\n");

    e_wr(E1000_CTRL, e_rd(E1000_CTRL) | CTRL_SLU | CTRL_ASDE);

    /* allocate descriptor rings and packet buffers on the heap */
    tx_ring = (tx_desc_t *)kcalloc(TX_DESC_COUNT, sizeof(tx_desc_t));
    rx_ring = (rx_desc_t *)kcalloc(RX_DESC_COUNT, sizeof(rx_desc_t));
    tx_buf  = (uint8_t (*)[ETH_FRAME_MAX])kmalloc(TX_DESC_COUNT * ETH_FRAME_MAX);
    rx_buf  = (uint8_t (*)[RX_BUF_SIZE])  kmalloc(RX_DESC_COUNT * RX_BUF_SIZE);

    if (!tx_ring || !rx_ring || !tx_buf || !rx_buf) {
        serial_write("[e1000] out of memory\n");
        return -1;
    }

    read_mac();
    serial_write("[e1000] MAC: ");
    for (int i = 0; i < 6; i++) {
        e_hex8(our_mac.addr[i]);
        if (i < 5) serial_write(":");
    }
    serial_write("\n");

    init_tx();
    init_rx();

    e_wr(E1000_IMS, ICR_RXT0 | ICR_RXDMT0 | ICR_LSC);

    extern void irq_register_handler(int irq, void (*h)(registers_t *));
    irq_register_handler(dev.irq, e1000_irq_handler);

    e1000_ready = 1;
    serial_write("[e1000] ready\n");
    return 0;
}
