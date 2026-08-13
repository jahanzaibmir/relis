/* kernel/drivers/disk/ata.h
 * ATA PIO driver — probes all 4 IDE positions, skips ATAPI devices
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* I/O base and control ports for both channels */
#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376

/* register offsets from base port */
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE       0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_CMD         0x07

/* status register bits */
#define ATA_SR_BSY   0x80   /* drive is busy */
#define ATA_SR_DRDY  0x40   /* drive ready */
#define ATA_SR_DRQ   0x08   /* data request — transfer ready */
#define ATA_SR_ERR   0x01   /* error occurred */

/* commands */
#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_FLUSH      0xE7

/* drive select values */
#define ATA_DRIVE_MASTER_CHS  0xA0
#define ATA_DRIVE_SLAVE_CHS   0xB0
#define ATA_DRIVE_MASTER_LBA  0xE0
#define ATA_DRIVE_SLAVE_LBA   0xF0

/* ATAPI signature in LBA_MID:LBA_HI after IDENTIFY — means it's a CD-ROM */
#define ATAPI_SIG_MID  0x14
#define ATAPI_SIG_HI   0xEB

#define ATA_SECTOR_SIZE  512

typedef struct {
    int      present;
    uint16_t base;
    uint16_t ctrl;
    uint8_t  drive_chs;
    uint8_t  drive_lba;
    uint32_t sectors;
    char     model[41];
    char     position[32];
} ata_drive_t;

extern ata_drive_t ata_drive;

int  ata_init(void);
int  ata_read (uint32_t lba, uint8_t count, void       *buf);
int  ata_write(uint32_t lba, uint8_t count, const void *buf);
void ata_flush(void);
