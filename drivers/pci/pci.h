/*
 * RELIS — drivers/pci/pci.h
 * Top-tier PCI bus enumeration and config space access.
 */
#pragma once
#include <stdint.h>

/* Config space access ports */
#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

/* Config space register offsets */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_CLASS_REVISION  0x08
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS_CODE      0x0B
#define PCI_CACHE_LINE_SIZE 0x0C
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

/* Bridge registers */
#define PCI_PRIMARY_BUS     0x18
#define PCI_SECONDARY_BUS   0x19
#define PCI_SUBORDINATE_BUS 0x1A

/* Command register bits */
#define PCI_CMD_IO        (1 << 0)
#define PCI_CMD_MEM       (1 << 1)
#define PCI_CMD_BUSMASTER (1 << 2)

/* Header Types */
#define PCI_HEADER_TYPE_NORMAL 0
#define PCI_HEADER_TYPE_BRIDGE 1
#define PCI_HEADER_TYPE_CARDBUS 2
#define PCI_HEADER_TYPE_MULTIFUNC 0x80

/* BAR Types */
#define PCI_BAR_TYPE_IO    1
#define PCI_BAR_TYPE_MMIO32 0
#define PCI_BAR_TYPE_MMIO64 2

#define MAX_PCI_DEVICES 32

typedef struct {
    uint8_t  bus, slot, func;
    uint16_t vendor_id, device_id;
    uint8_t  class_code, subclass, prog_if;
    uint8_t  header_type;
    uint8_t  irq;
    uint64_t bar[6];
    uint8_t  bar_type[6];
    int      bar_count;
} pci_device_t;

/* Config space read/write */
uint32_t pci_read32 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint16_t pci_read16 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint8_t  pci_read8  (uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void     pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v);
void     pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint16_t v);

int  pci_find_device  (uint16_t vendor, uint16_t device, pci_device_t *dev);
void pci_enable_device(pci_device_t *dev);
void pci_init         (void);