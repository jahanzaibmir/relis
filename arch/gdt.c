<<<<<<< HEAD
#include "gdt.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint32_t reserved1;
    uint32_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint32_t reserved3;
    uint32_t reserved4;
    uint16_t reserved5;
    uint16_t iomap_base;
} tss_t;

static uint64_t gdt[8] = {
    0,                  // 0x00: Null
    0x00AF9A000000FFFF, // 0x08: Kernel CS (Base=0, Limit=4GB, P=1, DPL=0, S=1, Type=0xA, G=1, L=1)
    0x00CF92000000FFFF, // 0x10: Kernel DS (Base=0, Limit=4GB, P=1, DPL=0, S=1, Type=0x2, G=1, B=1)
    0x00CFF2000000FFFF, // 0x18: User DS
    0x00AFFA000000FFFF, // 0x20: User CS
    0,                  // 0x28: TSS (Part 1)
    0                   // 0x30: TSS (Part 2)
};

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_descriptor_t;

static gdt_descriptor_t gdtr;
tss_t tss;

// FIX: Export physical GDT info for AP trampoline
uint64_t gdtr_base;
uint16_t gdtr_limit;

uint64_t tss_rsp0 = 0;
static uint8_t intr_stack[16384] __attribute__((aligned(16)));

extern void gdt_flush(uint64_t gdtr_ptr);
extern void tss_flush(uint16_t tss_selector);

void set_tss_rsp0(uint64_t rsp) {
    tss_rsp0 = rsp;
    tss.rsp0 = rsp;
}

static void gdt_set_tss(uint64_t base, uint32_t limit) {
    uint64_t entry = 0;
    entry |= (limit & 0xFFFF);
    entry |= (uint64_t)((limit >> 16) & 0x0F) << 48;
    entry |= (base & 0xFFFFFF) << 16;
    entry |= ((base >> 24) & 0xFF) << 56;
    entry |= 0x89ULL << 40;
    gdt[5] = entry;

    gdt[6] = (base >> 32) & 0xFFFFFFFF;
}

void gdt_init(void) {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)(uintptr_t)&gdt;
    
    // FIX: Set exported variables
    gdtr_base = gdtr.base;
    gdtr_limit = gdtr.limit;

    gdt_set_tss((uint64_t)(uintptr_t)&tss, sizeof(tss) - 1);

    gdt_flush((uint64_t)(uintptr_t)&gdtr);

    tss_rsp0 = (uint64_t)(uintptr_t)&intr_stack[sizeof(intr_stack)];
    tss.rsp0 = tss_rsp0;
    tss_flush(0x28);
}
=======
#include "gdt.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint32_t reserved1;
    uint32_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint32_t reserved3;
    uint32_t reserved4;
    uint16_t reserved5;
    uint16_t iomap_base;
} tss_t;

// Hardcoded 64-bit GDT entries.
static uint64_t gdt[8] = {
    0,                  // 0x00: Null
    0x00AF9A000000FFFF, 
    0x00CF92000000FFFF, 
    0x00CFF2000000FFFF, // 0x18: User DS   (Base=0, Limit=4GB, P=1, DPL=3, S=1, Type=0x2, G=1, B=1)
    0x00AFFA000000FFFF, // 0x20: User CS   (Base=0, Limit=4GB, P=1, DPL=3, S=1, Type=0xA, G=1, L=1)
    0,                  // 0x28: TSS (Part 1)
    0                   // 0x30: TSS (Part 2)
};

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_descriptor_t;

static gdt_descriptor_t gdtr;
tss_t tss;

uint64_t tss_rsp0 = 0;
static uint8_t intr_stack[16384] __attribute__((aligned(16)));

extern void gdt_flush(uint64_t gdtr_ptr);
extern void tss_flush(uint16_t tss_selector);

void set_tss_rsp0(uint64_t rsp) {
    tss_rsp0 = rsp;
    tss.rsp0 = rsp;
}

static void gdt_set_tss(uint64_t base, uint32_t limit) {
    uint64_t entry = 0;
    entry |= (limit & 0xFFFF);
    entry |= (uint64_t)((limit >> 16) & 0x0F) << 48;
    entry |= (base & 0xFFFFFF) << 16;
    entry |= ((base >> 24) & 0xFF) << 56;
    entry |= 0x89ULL << 40; // Present, TSS
    gdt[5] = entry;
    
    gdt[6] = (base >> 32) & 0xFFFFFFFF;
}

void gdt_init(void) {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)(uintptr_t)&gdt;

    gdt_set_tss((uint64_t)(uintptr_t)&tss, sizeof(tss) - 1);

    gdt_flush((uint64_t)(uintptr_t)&gdtr);
    
    tss_rsp0 = (uint64_t)(uintptr_t)&intr_stack[sizeof(intr_stack)];
    tss.rsp0 = tss_rsp0;
    tss_flush(0x28);
}
>>>>>>> 0b969924ab1e63a6a73a64369a246c1e92a24a32
