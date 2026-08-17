#include "relis/elf.h"
#include "relis/mm.h"
#include "relis/printk.h"
#include "asm/pgtable.h"
#include <stdint.h>

uint64_t load_elf(uint8_t *data) {
    elf64_hdr_t *hdr = (elf64_hdr_t *)data;
    
    if (hdr->e_magic != ELF_MAGIC) {
        printk("ELF: Invalid magic number!");
        return 0;
    }
    
    elf64_phdr_t *phdr = (elf64_phdr_t *)(data + hdr->e_phoff);
    
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t memsz = phdr[i].p_memsz;
            
            uint64_t pages = (memsz + PAGE_SIZE - 1) / PAGE_SIZE;
            
            for (uint64_t j = 0; j < pages; j++) {
                uint64_t phys = alloc_page();
                arch_map_page(vaddr + j * PAGE_SIZE, phys, PTE_WRITABLE | PTE_USER);
                
                // FIX: Copy data to the ACTUAL physical page using the direct map!
                uint64_t offset_in_segment = j * PAGE_SIZE;
                if (offset_in_segment < filesz) {
                    uint64_t copy_len = PAGE_SIZE;
                    if (offset_in_segment + copy_len > filesz) {
                        copy_len = filesz - offset_in_segment;
                    }
                    
                    uint8_t *src = data + phdr[i].p_offset + offset_in_segment;
                    uint8_t *dst = (uint8_t*)phys_to_virt(phys);
                    for (uint64_t k = 0; k < copy_len; k++) {
                        dst[k] = src[k];
                    }
                }
            }
            
            printk("ELF: Loaded segment at 0x%x (filesz=%d, memsz=%d)", vaddr, filesz, memsz);
        }
    }
    
    return hdr->e_entry;
}
