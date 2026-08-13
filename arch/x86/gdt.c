/*
 * kernel/arch/x86/gdt.c
 */
#include "gdt.h"

/* each GDT entry is 8 bytes split across several bitfields */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;    /* lower 16 bits of segment limit        */
    uint16_t base_low;     /* lower 16 bits of base address         */
    uint8_t  base_mid;     /* middle 8 bits of base address         */
    uint8_t  access;       /* type, DPL, present flag               */
    uint8_t  flags_limit;  /* high 4 bits: flags, low 4: limit      */
    uint8_t  base_high;    /* upper 8 bits of base address          */
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;        
    uint32_t base;    
} gdt_descriptor_t;

/* access byte bits */
#define GDT_PRESENT    0x80   /* segment exists in memory           */
#define GDT_RING0      0x00   /* kernel privilege                   */
#define GDT_RING3      0x60   /* user privilege                     */
#define GDT_DESCRIPTOR 0x10   /* not a system segment               */
#define GDT_EXECUTABLE 0x08   /* code segment                       */
#define GDT_READABLE   0x02   /* readable (code) / writable (data)  */

/* flags nibble */
#define GDT_GRANULARITY 0x80  /* limit is in 4kb pages              */
#define GDT_32BIT       0x40  /* 32-bit protected mode              */

#define GDT_ENTRIES 5

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_descriptor_t gdtr;

/* fill in one 8-byte descriptor */
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].flags_limit = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    gdt[idx].access      = access;
}

/* defined in gdt_flush.asm — loads GDTR and far-jumps to reload CS */
extern void gdt_flush(uint32_t gdtr_ptr);

void gdt_init(void)
{
    gdtr.limit = (uint16_t)(sizeof(gdt_entry_t) * GDT_ENTRIES - 1);
    gdtr.base  = (uint32_t)(uintptr_t)&gdt;

    
    gdt_set_entry(0, 0, 0, 0, 0);

    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_RING0 | GDT_DESCRIPTOR | GDT_EXECUTABLE | GDT_READABLE,
                  GDT_GRANULARITY | GDT_32BIT);

    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_RING0 | GDT_DESCRIPTOR | GDT_READABLE,
                  GDT_GRANULARITY | GDT_32BIT);

    gdt_set_entry(3, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_RING3 | GDT_DESCRIPTOR | GDT_EXECUTABLE | GDT_READABLE,
                  GDT_GRANULARITY | GDT_32BIT);
    gdt_set_entry(4, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_RING3 | GDT_DESCRIPTOR | GDT_READABLE,
                  GDT_GRANULARITY | GDT_32BIT);

    gdt_flush((uint32_t)(uintptr_t)&gdtr);
}
