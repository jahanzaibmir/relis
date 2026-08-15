#include "pci.h"
#include "arch/io.h"
#include "relis/printk.h"
#include "relis/string.h"
#include <stdint.h>

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | 
           ((uint32_t)func << 8) | (offset & 0xFC);
}

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
    uint32_t old = inl(PCI_DATA);
    int shift = (off & 2) * 8;
    old = (old & ~((uint32_t)0xFFFF << shift)) | ((uint32_t)val << shift);
    outl(PCI_DATA, old);
}

void pci_enable_device(pci_device_t *dev) {
    uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, PCI_COMMAND);
    cmd |= (uint16_t)(PCI_CMD_BUSMASTER | PCI_CMD_MEM | PCI_CMD_IO);
    pci_write16(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
}

int pci_find_device(uint16_t vendor, uint16_t device, pci_device_t *out_dev) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device) {
            *out_dev = pci_devices[i];
            return 1;
        }
    }
    return 0;
}

static const char* pci_get_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network Controller";
        case 0x03: return "Display Controller";
        case 0x04: return "Multimedia Controller";
        case 0x06: return "Bridge Device";
        case 0x0C: return "Serial Bus";
        default:   return "Unknown";
    }
}

static void pci_decode_bars(pci_device_t *dev) {
    int bar_limit = (dev->header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE ? 2 : 6;
    for (int i = 0; i < bar_limit; i++) {
        uint8_t reg = PCI_BAR0 + i * 4;
        uint32_t bar_lo = pci_read32(dev->bus, dev->slot, dev->func, reg);
        if (bar_lo == 0) { dev->bar[i] = 0; dev->bar_type[i] = 0; continue; }
        if (bar_lo & 1) {
            dev->bar[i] = bar_lo & 0xFFFFFFFC;
            dev->bar_type[i] = PCI_BAR_TYPE_IO;
        } else {
            uint8_t type = (bar_lo >> 1) & 3;
            if (type == PCI_BAR_TYPE_MMIO32) {
                dev->bar[i] = bar_lo & 0xFFFFFFF0;
                dev->bar_type[i] = PCI_BAR_TYPE_MMIO32;
            } else if (type == PCI_BAR_TYPE_MMIO64) {
                uint32_t bar_hi = pci_read32(dev->bus, dev->slot, dev->func, reg + 4);
                dev->bar[i] = ((uint64_t)bar_hi << 32) | (bar_lo & 0xFFFFFFF0);
                dev->bar_type[i] = PCI_BAR_TYPE_MMIO64;
                i++;
            }
        }
    }
}

void pci_scan_bus(uint8_t bus);

static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    if (pci_device_count >= MAX_PCI_DEVICES) return;
    uint32_t id = pci_read32(bus, slot, func, PCI_VENDOR_ID);
    uint16_t vid = id & 0xFFFF;
    if (vid == 0xFFFF) return;

    pci_device_t *dev = &pci_devices[pci_device_count++];
    kmemset(dev, 0, sizeof(pci_device_t));
    
    dev->bus = bus; dev->slot = slot; dev->func = func;
    dev->vendor_id = vid; dev->device_id = id >> 16;
    
    uint32_t class_rev = pci_read32(bus, slot, func, PCI_CLASS_REVISION);
    dev->class_code = (class_rev >> 24) & 0xFF;
    dev->subclass = (class_rev >> 16) & 0xFF;
    dev->prog_if = (class_rev >> 8) & 0xFF;
    
    dev->header_type = pci_read8(bus, slot, func, PCI_HEADER_TYPE);
    dev->irq = pci_read8(bus, slot, func, PCI_INTERRUPT_LINE);
    
    pci_decode_bars(dev);
    printk("PCI %d:%d.%d - %x:%x (%s)", bus, slot, func, vid, dev->device_id, pci_get_class_name(dev->class_code));

    if (dev->class_code == 0x06 && dev->subclass == 0x04) {
        uint8_t sec_bus = pci_read8(bus, slot, func, PCI_SECONDARY_BUS);
        if (sec_bus != 0) { pci_scan_bus(sec_bus); }
    }
}

void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        if (pci_read32(bus, slot, 0, PCI_VENDOR_ID) == 0xFFFFFFFF) continue;
        uint8_t header_type = pci_read8(bus, slot, 0, PCI_HEADER_TYPE);
        if (header_type & PCI_HEADER_TYPE_MULTIFUNC) {
            for (uint8_t func = 0; func < 8; func++) {
                if (pci_read32(bus, slot, func, PCI_VENDOR_ID) != 0xFFFFFFFF) { pci_scan_function(bus, slot, func); }
            }
        } else { pci_scan_function(bus, slot, 0); }
    }
}

void pci_init(void) {
    printk("PCI: Scanning bus 0...");
    pci_device_count = 0;
    pci_scan_bus(0);
    printk("PCI: Scan complete. Found %d devices.", pci_device_count);
}
