/*
 * RELIS — kernel/arch/x86/gdt.h
 * Global Descriptor Table — sets up x86 memory segmentation.
 *
 * Segments configured:
 *   0x00  null descriptor
 *   0x08  kernel code  (ring 0, executable, readable)
 *   0x10  kernel data  (ring 0, writable)
 *   0x18  user code    (ring 3, executable, readable)
 *   0x20  user data    (ring 3, writable)
 */
#pragma once
#include <stdint.h>

void gdt_init(void);
