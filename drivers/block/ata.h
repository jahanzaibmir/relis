#pragma once
#include <stdint.h>

int ata_read_block(uint32_t lba, uint8_t *buf);
int ata_write_block(uint32_t lba, const uint8_t *buf);
void ata_init(void);
