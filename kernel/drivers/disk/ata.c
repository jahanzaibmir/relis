/*
 * kernel/drivers/disk/ata.c
 * ATA PIO driver — auto-probes all 4 IDE positions
 *
 * NOTE: interrupts are disabled (cli) during ALL read/write operations
 * and restored (sti) when done. PIO mode is not reentrant — if the timer
 * IRQ fires mid-operation the scheduler can context switch to another task
 * that also tries to use the ATA bus, corrupting both operations.
 * cli/sti is the correct fix for a single-threaded PIO driver.
 */
#include "ata.h"
#include "io.h"
#include "drivers/serial.h"
#include <stddef.h>

ata_drive_t ata_drive;

/* ── bus helpers ─────────────────────────────────────────────────────────── */

static inline void     b_outb(uint16_t base, uint8_t reg, uint8_t v)   { outb((uint16_t)(base + reg), v); }
static inline uint8_t  b_inb (uint16_t base, uint8_t reg)              { return inb((uint16_t)(base + reg)); }
static inline uint16_t b_inw (uint16_t base, uint8_t reg)              { return inw((uint16_t)(base + reg)); }
static inline void     b_outw(uint16_t base, uint8_t reg, uint16_t v)  { outw((uint16_t)(base + reg), v); }

/* 400ns delay — read alt status 4 times, result ignored */
static void b_delay(uint16_t ctrl)
{
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);
}

static int wait_bsy(uint16_t base)
{
    uint32_t t = 100000;
    while (b_inb(base, ATA_REG_STATUS) & ATA_SR_BSY)
        if (--t == 0) return -1;
    return 0;
}

/* ── probe one drive position ────────────────────────────────────────────── */

static int probe(uint16_t base, uint16_t ctrl,
                 uint8_t drv_chs, uint8_t drv_lba, const char *label)
{
    b_outb(base, ATA_REG_DRIVE, drv_chs);
    b_delay(ctrl);

    uint8_t st = b_inb(base, ATA_REG_STATUS);
    if (st == 0x00 || st == 0xFF) return 0;  /* nothing here */

    b_outb(base, ATA_REG_SECCOUNT, 0);
    b_outb(base, ATA_REG_LBA_LO,  0);
    b_outb(base, ATA_REG_LBA_MID, 0);
    b_outb(base, ATA_REG_LBA_HI,  0);
    b_outb(base, ATA_REG_CMD, ATA_CMD_IDENTIFY);
    b_delay(ctrl);

    st = b_inb(base, ATA_REG_STATUS);
    if (st == 0x00 || st == 0xFF) return 0;
    if (wait_bsy(base) < 0) return 0;

    uint8_t mid = b_inb(base, ATA_REG_LBA_MID);
    uint8_t hi  = b_inb(base, ATA_REG_LBA_HI);

    if (mid == ATAPI_SIG_MID && hi == ATAPI_SIG_HI) {
        serial_write("[ata] "); serial_write(label); serial_write(": ATAPI skipped\n");
        return 0;
    }
    if (mid != 0 || hi != 0) {
        serial_write("[ata] "); serial_write(label); serial_write(": non-ATA skipped\n");
        return 0;
    }

    /* wait for DRQ or ERR */
    uint32_t t = 100000;
    while (1) {
        st = b_inb(base, ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return 0;
        if (st & ATA_SR_DRQ) break;
        if (--t == 0) return 0;
    }

    /* read the 256-word IDENTIFY response */
    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = b_inw(base, ATA_REG_DATA);

    if (id[0] & 0x8000) return 0;  /* not a fixed disk */

    uint32_t sectors = ((uint32_t)id[61] << 16) | id[60];
    if (sectors == 0) return 0;

    ata_drive.present   = 1;
    ata_drive.base      = base;
    ata_drive.ctrl      = ctrl;
    ata_drive.drive_chs = drv_chs;
    ata_drive.drive_lba = drv_lba;
    ata_drive.sectors   = sectors;

    /* copy model string (words 27-46, big-endian byte order) */
    int mi = 0;
    for (int w = 27; w <= 46; w++) {
        ata_drive.model[mi++] = (char)(id[w] >> 8);
        ata_drive.model[mi++] = (char)(id[w] & 0xFF);
    }
    ata_drive.model[40] = '\0';

    /* trim trailing spaces */
    for (int i = 39; i >= 0 && ata_drive.model[i] == ' '; i--)
        ata_drive.model[i] = '\0';

    /* save position label */
    int li = 0;
    while (label[li] && li < 31) { ata_drive.position[li] = label[li]; li++; }
    ata_drive.position[li] = '\0';

    return 1;
}

/* ── init — probe all 4 positions, use first one found ───────────────────── */

int ata_init(void)
{
    ata_drive.present = 0;
    serial_write("[ata] probing...\n");

    /* probe slave first — on some systems master responds for both */
    if (probe(ATA_PRIMARY_BASE,   ATA_PRIMARY_CTRL,   ATA_DRIVE_SLAVE_CHS,  ATA_DRIVE_SLAVE_LBA,  "primary slave"))   goto found;
    if (probe(ATA_PRIMARY_BASE,   ATA_PRIMARY_CTRL,   ATA_DRIVE_MASTER_CHS, ATA_DRIVE_MASTER_LBA, "primary master"))  goto found;
    if (probe(ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, ATA_DRIVE_MASTER_CHS, ATA_DRIVE_MASTER_LBA, "secondary master")) goto found;
    if (probe(ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, ATA_DRIVE_SLAVE_CHS,  ATA_DRIVE_SLAVE_LBA,  "secondary slave")) goto found;

    serial_write("[ata] no disk found\n");
    return -1;

found:
    serial_write("[ata] disk: ");
    serial_write(ata_drive.position);
    serial_write(" — ");
    serial_write(ata_drive.model);
    serial_write("\n");
    return 0;
}

/* ── select drive and set LBA bits 24-27 ────────────────────────────────── */

static void ata_select(uint32_t lba)
{
    b_outb(ata_drive.base, ATA_REG_DRIVE,
           (uint8_t)(ata_drive.drive_lba | ((lba >> 24) & 0x0F)));
    b_delay(ata_drive.ctrl);
}

/* ── poll until BSY clears, optionally check DRQ ────────────────────────── */

static int ata_poll(int check_drq)
{
    uint32_t t = 100000;
    uint8_t st;

    while (1) {
        st = b_inb(ata_drive.base, ATA_REG_STATUS);
        if (!(st & ATA_SR_BSY)) break;
        if (--t == 0) return -1;
    }

    st = b_inb(ata_drive.base, ATA_REG_STATUS);
    if (st & ATA_SR_ERR) return -1;
    if (check_drq && !(st & ATA_SR_DRQ)) return -1;
    return 0;
}

/* ── read sectors — interrupts disabled for entire operation ─────────────── */

int ata_read(uint32_t lba, uint8_t count, void *buf)
{
    if (!ata_drive.present || !count) return -1;

    __asm__ volatile("cli");    /* disable interrupts — PIO is not reentrant */

    ata_select(lba);
    b_outb(ata_drive.base, ATA_REG_SECCOUNT, count);
    b_outb(ata_drive.base, ATA_REG_LBA_LO,  (uint8_t)(lba & 0xFF));
    b_outb(ata_drive.base, ATA_REG_LBA_MID, (uint8_t)((lba >> 8)  & 0xFF));
    b_outb(ata_drive.base, ATA_REG_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    b_outb(ata_drive.base, ATA_REG_CMD,      ATA_CMD_READ_PIO);

    uint16_t *p = (uint16_t *)buf;
    int result = 0;

    for (int s = 0; s < count; s++) {
        if (ata_poll(1) < 0) { result = -1; break; }
        for (int i = 0; i < 256; i++)
            *p++ = b_inw(ata_drive.base, ATA_REG_DATA);
        b_delay(ata_drive.ctrl);
    }

    __asm__ volatile("sti");
    return result;
}

/* ── write sectors — interrupts disabled for entire operation ────────────── */

int ata_write(uint32_t lba, uint8_t count, const void *buf)
{
    if (!ata_drive.present || !count) return -1;

    __asm__ volatile("cli");    /* disable interrupts — PIO is not reentrant */

    ata_select(lba);
    b_outb(ata_drive.base, ATA_REG_SECCOUNT, count);
    b_outb(ata_drive.base, ATA_REG_LBA_LO,  (uint8_t)(lba & 0xFF));
    b_outb(ata_drive.base, ATA_REG_LBA_MID, (uint8_t)((lba >> 8)  & 0xFF));
    b_outb(ata_drive.base, ATA_REG_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    b_outb(ata_drive.base, ATA_REG_CMD,      ATA_CMD_WRITE_PIO);

    const uint16_t *p = (const uint16_t *)buf;
    int result = 0;

    for (int s = 0; s < count; s++) {
        if (ata_poll(1) < 0) { result = -1; break; }
        for (int i = 0; i < 256; i++)
            b_outw(ata_drive.base, ATA_REG_DATA, *p++);
        b_delay(ata_drive.ctrl);
    }

    /* flush write cache before returning */
    if (result == 0) {
        b_outb(ata_drive.base, ATA_REG_DRIVE, ata_drive.drive_lba);
        b_outb(ata_drive.base, ATA_REG_CMD,   ATA_CMD_FLUSH);
        ata_poll(0);
    }

    __asm__ volatile("sti");
    return result;
}

/* ── flush write cache ───────────────────────────────────────────────────── */

void ata_flush(void)
{
    if (!ata_drive.present) return;
    __asm__ volatile("cli");
    b_outb(ata_drive.base, ATA_REG_DRIVE, ata_drive.drive_lba);
    b_outb(ata_drive.base, ATA_REG_CMD,   ATA_CMD_FLUSH);
    ata_poll(0);
    __asm__ volatile("sti");
}
