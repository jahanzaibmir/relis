#include "gdt.h"
#include <stdint.h>

extern uint8_t stack_top[];

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_descriptor_t;

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

static gdt_entry_t gdt[8];
static gdt_descriptor_t gdtr;
static tss_t tss;

extern void gdt_flush(uint64_t gdtr_ptr);
extern void tss_flush(uint16_t tss_selector);

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].flags_limit = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    gdt[idx].access      = access;
}

static void gdt_set_tss(int idx, uint64_t base, uint32_t limit) {
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].flags_limit = (uint8_t)(((limit >> 16) & 0x0F) | 0x00);
    gdt[idx].access      = 0x89;
    
    gdt[idx + 1].limit_low = 0;
    gdt[idx + 1].base_low = 0;
    gdt[idx + 1].base_mid = 0;
    gdt[idx + 1].access = 0;
    gdt[idx + 1].flags_limit = 0;
    gdt[idx + 1].base_high = 0;
}

void gdt_init(void) {
    gdtr.limit = (uint16_t)(sizeof(gdt_entry_t) * 8 - 1);
    gdtr.base  = (uint64_t)(uintptr_t)&gdt;

    gdt_set_entry(0, 0, 0, 0, 0);               
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);   
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);   
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);   
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);   
    gdt_set_tss(5, (uint64_t)(uintptr_t)&tss, sizeof(tss) - 1); 

    gdt_flush((uint64_t)(uintptr_t)&gdtr);
    
    tss.rsp0 = (uint64_t)(uintptr_t)stack_top;
    tss_flush(0x28);
}
