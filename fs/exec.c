#include "relis/mm.h"
#include "relis/printk.h"
#include "relis/elf.h"
#include "relis/fs.h"
#include "relis/irq.h"
#include "relis/sched.h"
#include "relis/string.h"
#include "asm/pgtable.h"
#include <stdint.h>

long sys_execve(struct registers *regs, const char *path) {
    struct file *f = vfs_open(path);
    if (!f) return -1;

    uint8_t *elf_data = kmalloc(4096);
    if (vfs_read(f, (char*)elf_data, 4096) < 0) {
        kfree(elf_data);
        return -1;
    }

    elf64_hdr_t *hdr = (elf64_hdr_t*)elf_data;
    if (hdr->e_magic != ELF_MAGIC) {
        kfree(elf_data);
        return -1;
    }

    uint64_t new_cr3 = create_new_address_space();
    switch_address_space(new_cr3);

    arch_map_page(0xB8000, 0xB8000, PTE_WRITABLE | PTE_GLOBAL);

    elf64_phdr_t *phdr = (elf64_phdr_t*)(elf_data + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t pages = (phdr[i].p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;

            for (uint64_t j = 0; j < pages; j++) {
                uint64_t phys = alloc_page();
                arch_map_page(vaddr + j * PAGE_SIZE, phys, PTE_WRITABLE | PTE_USER);

                uint64_t offset = j * PAGE_SIZE;
                if (offset < filesz) {
                    uint64_t copy_len = PAGE_SIZE;
                    if (offset + copy_len > filesz) copy_len = filesz - offset;
                    kmemcpy(phys_to_virt(phys), elf_data + phdr[i].p_offset + offset, copy_len);
                }
            }
        }
    }

    // FIX: Set Stack VMA bounds. DO NOT allocate stack pages! (Demand Paging)
    uint64_t user_stack = 0x40000000;
    current_task->stack_start = user_stack - (4 * PAGE_SIZE); // 16KB stack
    current_task->stack_end = user_stack;

    current_task->cr3 = new_cr3;
    kstrcpy(current_task->name, path);

    regs->rip = hdr->e_entry;
    regs->rsp = user_stack - 16;
    regs->cs = 0x23;
    regs->ss = 0x1B;
    regs->rflags = 0x202;

    kfree(elf_data);
    printk("Execve: Loaded %s at RIP=0x%x", path, hdr->e_entry);
    return 0;
}

void exec_user_program(uint8_t *elf_data) {
    elf64_hdr_t *hdr = (elf64_hdr_t *)elf_data;
    if (hdr->e_magic != ELF_MAGIC) return;

    uint64_t new_cr3 = create_new_address_space();
    switch_address_space(new_cr3);

    arch_map_page(0xB8000, 0xB8000, PTE_WRITABLE | PTE_GLOBAL);

    elf64_phdr_t *phdr = (elf64_phdr_t *)(elf_data + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t pages = (phdr[i].p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;

            for (uint64_t j = 0; j < pages; j++) {
                uint64_t phys = alloc_page();
                arch_map_page(vaddr + j * PAGE_SIZE, phys, PTE_WRITABLE | PTE_USER);

                uint64_t offset = j * PAGE_SIZE;
                if (offset < filesz) {
                    uint64_t copy_len = PAGE_SIZE;
                    if (offset + copy_len > filesz) copy_len = filesz - offset;
                    kmemcpy(phys_to_virt(phys), elf_data + phdr[i].p_offset + offset, copy_len);
                }
            }
        }
    }

    // FIX: Set Stack VMA bounds. DO NOT allocate stack pages! (Demand Paging)
    uint64_t user_stack = 0x40000000;
    current_task->stack_start = user_stack - (4 * PAGE_SIZE);
    current_task->stack_end = user_stack;

    current_task->cr3 = new_cr3;

    extern void drop_to_user(uint64_t rsp, uint64_t rip);
    drop_to_user(user_stack - 16, hdr->e_entry);
}
