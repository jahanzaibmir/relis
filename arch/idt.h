#pragma once
#include <stdint.h>

void idt_init(void);
void idt_load(uint64_t idtr_ptr);