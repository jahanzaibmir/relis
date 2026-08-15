#include "ata.h"
#include "arch/io.h"
#include "relis/printk.h"
#include <stdint.h>

#define ATA_DATA  0x1F0
#define ATA_ERR   0x1F1
#define ATA_COUNT 0x1F2
#define ATA_LBAlo 0x1F3
#define ATA_LBAmid 0x1F4
#define ATA_LBAhi 0x1F5
#define ATA_DRV   0x1F6
#define ATA_CMD   0x1F7
#define ATA_STATUS 0x1F7

static void ata_wait_bsy(void) {
    int timeout = 1000000;
    while ((inb(ATA_STATUS) & 0x80) && --timeout);
}

static int ata_wait_drq(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint8_t s = inb(ATA_STATUS);
        if (s & 0x08) return 1; // DRQ is set
        if (s & 0x01) return 0; // ERR bit is set
        if (s & 0x20) return 0; // DF (Drive Fault) is set
    }
    return 0; // Timed out
}

int ata_read_block(uint32_t lba, uint8_t *buf) {
    outb(ATA_DRV, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT, 1);
    outb(ATA_LBAlo, lba & 0xFF);
    outb(ATA_LBAmid, (lba >> 8) & 0xFF);
    outb(ATA_LBAhi, (lba >> 16) & 0xFF);
    outb(ATA_CMD, 0x20); // READ SECTORS

    ata_wait_bsy();
    if (!ata_wait_drq()) {
        printk("ATA: Read error at LBA %d", lba);
        return -1;
    }

    insw(ATA_DATA, buf, 256);
    return 0;
}

int ata_write_block(uint32_t lba, const uint8_t *buf) {
    outb(ATA_DRV, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT, 1);
    outb(ATA_LBAlo, lba & 0xFF);
    outb(ATA_LBAmid, (lba >> 8) & 0xFF);
    outb(ATA_LBAhi, (lba >> 16) & 0xFF);
    outb(ATA_CMD, 0x30); // WRITE SECTORS

    ata_wait_bsy();
    if (!ata_wait_drq()) {
        printk("ATA: Write error at LBA %d", lba);
        return -1;
    }

    outsw(ATA_DATA, buf, 256);
    
    // Flush cache and wait for it to finish
    outb(ATA_CMD, 0xE7);
    ata_wait_bsy();
    return 0;
}

void ata_init(void) {
    printk("Block device: ATA PIO initialized");
}
