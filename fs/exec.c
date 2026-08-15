#include "relis/mm.h"
#include "relis/printk.h"
#include "relis/elf.h"
#include "asm/pgtable.h"
#include <stdint.h>

extern uint64_t load_elf(uint8_t *data);
extern void drop_to_user(uint64_t rsp, uint64_t rip);

void exec_user_program(uint8_t *elf_data) {
    // 1. Allocate a user stack (4 pages = 16KB) at 1GB virtual address
    uint64_t user_stack = 0x40000000;
    
    // FIX: Map 4 pages ending EXACTLY at 0x40000000
    // So we map 0x3FFFF000, 0x3FFFE000, 0x3FFFD000, 0x3FFFC000
    for (int i = 1; i <= 4; i++) {
        uint64_t phys = alloc_page();
        arch_map_page(user_stack - i * PAGE_SIZE, phys, PTE_WRITABLE | PTE_USER);
    }
    uint64_t user_rsp = user_stack - 16; // RSP is 0x3FFFFFF0, which is now safely mapped!
    
    // 2. Load the ELF binary and get entry point
    uint64_t entry = load_elf(elf_data);
    if (!entry) {
        printk("EXEC: Failed to load ELF binary");
        return;
    }
    
    printk("EXEC: Dropping to Ring 3 at RIP=0x%x, RSP=0x%x", entry, user_rsp);
    
    // 3. Drop to Ring 3!
    drop_to_user(user_rsp, entry);
}
