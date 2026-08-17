#pragma once
#include <stdint.h>

/* GDT Selector Definitions */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS   0x18
#define USER_CS   0x20
#define TSS_SEL   0x28

/* this initialisez the Global Descriptor Table and Task State Segment */
void gdt_init(void);

//used by the scheduler on context switches
void set_tss_rsp0(uint64_t rsp);