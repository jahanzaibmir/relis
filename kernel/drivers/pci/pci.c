/*
 * RELIS — kernel/drivers/pci/pci.c
 * PCI bus enumeration — bus 0, config mechanism #1.
 *
 * Uses serial_write exclusively (not kprintf) because this runs before
 * the framebuffer is guaranteed to be ready.
 */
#include "pci.h"
#include "io.h"
#include "drivers/serial.h"
#include <stddef.h>

/* ── Serial helpers ──────────────────────────────────────────────────────── */
static void pci_hex8(uint8_t v) {
    const char *h = "0123456789ABCDEF";
    char s[3] = { h[v >> 4], h[v & 0xF], '\0' };
    serial_write(s);
}
static void pci_hex16(uint16_t v) {
    pci_hex8((uint8_t)(v >> 8));
    pci_hex8((uint8_t)(v & 0xFF));
}
static void pci_dec8(uint8_t v) {
    if (v >= 100) serial_putchar((char)('0' + v / 100));
    if (v >=  10) serial_putchar((char)('0' + (v / 10) % 10));
    serial_putchar((char)('0' + v % 10));
}

/* ── Build PCI config-space address word ─────────────────────────────────── */
static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (1u << 31)
         | ((uint32_t)bus    << 16)
         | ((uint32_t)slot   << 11)
         | ((uint32_t)func   <<  8)
         | ((uint32_t)offset & 0xFC);
}

/* ── Config space read/write ─────────────────────────────────────────────── */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    return inl(PCI_DATA);
}
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    return (uint16_t)((inl(PCI_DATA) >> ((off & 2) * 8)) & 0xFFFF);
}
uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    return (uint8_t)((inl(PCI_DATA) >> ((off & 3) * 8)) & 0xFF);
}
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val) {
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    outl(PCI_DATA, val);
}
void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint16_t val) {
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    uint32_t old   = inl(PCI_DATA);
    int      shift = (off & 2) * 8;
    old = (old & ~((uint32_t)0xFFFF << shift)) | ((uint32_t)val << shift);
    outl(PCI_DATA, old);
}

/* ── Decode BARs ─────────────────────────────────────────────────────────── */
static void decode_bars(uint8_t bus, uint8_t slot, uint8_t func, pci_device_t *dev) {
    for (int i = 0; i < 6; i++) {
        uint8_t  reg = (uint8_t)(PCI_BAR0 + i * 4);
        uint32_t bar = pci_read32(bus, slot, func, reg);
        if (!bar) { dev->bar[i] = 0; dev->bar_is_mmio[i] = 0; continue; }
        if (bar & 1) {
            dev->bar[i]         = bar & 0xFFFFFFFC;  /* I/O space */
            dev->bar_is_mmio[i] = 0;
        } else {
            dev->bar[i]         = bar & 0xFFFFFFF0;  /* MMIO */
            dev->bar_is_mmio[i] = 1;
        }
    }
}

/* ── Find a device by vendor + device ID ─────────────────────────────────── */
int pci_find_device(uint16_t vendor, uint16_t device, pci_device_t *dev) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id  = pci_read32(0, slot, 0, PCI_VENDOR_ID);
        uint16_t vid = (uint16_t)(id & 0xFFFF);
        uint16_t did = (uint16_t)(id >> 16);
        if (vid == 0xFFFF) continue;
        if (vid == vendor && did == device) {
            dev->bus        = 0;
            dev->slot       = slot;
            dev->func       = 0;
            dev->vendor_id  = vid;
            dev->device_id  = did;
            dev->class_code = pci_read8(0, slot, 0, PCI_CLASS_CODE);
            dev->subclass   = pci_read8(0, slot, 0, PCI_SUBCLASS);
            dev->irq        = pci_read8(0, slot, 0, PCI_INTERRUPT_LINE);
            decode_bars(0, slot, 0, dev);
            return 1;
        }
    }
    return 0;
}

/* ── Enable bus mastering + memory/IO decode ─────────────────────────────── */
void pci_enable_device(pci_device_t *dev) {
    uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, PCI_COMMAND);
    cmd |= (uint16_t)(PCI_CMD_BUSMASTER | PCI_CMD_MEM | PCI_CMD_IO);
    pci_write16(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
}

/* ── Scan bus 0 and log every device via serial ──────────────────────────── */
void pci_init(void) {
    serial_write("[pci] scanning bus 0\n");
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id  = pci_read32(0, slot, 0, PCI_VENDOR_ID);
        uint16_t vid = (uint16_t)(id & 0xFFFF);
        if (vid == 0xFFFF) continue;
        uint16_t did = (uint16_t)(id >> 16);
        uint8_t  cls = pci_read8(0, slot, 0, PCI_CLASS_CODE);
        uint8_t  sub = pci_read8(0, slot, 0, PCI_SUBCLASS);
        uint8_t  irq = pci_read8(0, slot, 0, PCI_INTERRUPT_LINE);
        serial_write("[pci] slot ");
        pci_dec8(slot);
        serial_write("  ");
        pci_hex16(vid); serial_write(":"); pci_hex16(did);
        serial_write("  cls="); pci_hex8(cls); serial_write(":"); pci_hex8(sub);
        serial_write("  irq="); pci_dec8(irq);
        serial_write("\n");
    }
    serial_write("[pci] scan complete\n");
}
