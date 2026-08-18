#include "relis/printk.h"
#include "relis/sched.h"
#include "relis/irq.h"
#include "relis/syscall.h"
#include "relis/mm.h"
#include "relis/fs.h"
#include "relis/net.h"
#include "relis/types.h"
#include "relis/string.h"
#include "relis/smp.h"
#include "asm/smp_boot.h"
#include "arch/gdt.h"
#include "asm/apic.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "drivers/pci/pci.h"
#include "drivers/block/ata.h"
#include "fs/relisfs/relisfs.h"
#include "relis_nic.h"

extern struct file_system_type proc_fs_type;
extern void exec_user_program(uint8_t *elf_data);

extern uint8_t _binary_user_prog_elf_start[];
extern uint8_t _binary_user_prog_elf_end[];

static void init_task(void) {
    uint64_t prog_size = _binary_user_prog_elf_end - _binary_user_prog_elf_start;
    printk("Loading user program (%d bytes)...", prog_size);
    exec_user_program(_binary_user_prog_elf_start);
}

static void heartbeat_task(void) {
    while (1) {
        __asm__ volatile("hlt");
        schedule();
    }
}

void start_kernel(uint64_t mb_magic, void *mb_info) {
    (void)mb_magic; (void)mb_info;

    console_init();
    printk("=== RELIS 64-BIT KERNEL BOOTING ===");

    gdt_init();
    pmm_init(262144, 0);
    irq_init();
    sched_init();
    syscall_init();

    paging_init();

    apic_init();
    smp_init();

    timer_init(100);
    keyboard_init();

    vfs_init();
    ata_init();
    
    struct dentry *root = vfs_kern_mount(&relisfs_fs_type);
    printk("Root filesystem (relisfs) mounted at /");

    struct dentry *proc_root = proc_fs_type.mount(&proc_fs_type, NULL);
    if (root && proc_root) {
        kstrcpy(proc_root->d_name, "proc");
        root->d_children[root->d_child_count++] = proc_root;
    }
    printk("Proc filesystem mounted at /proc");

    net_init();
    pci_init();
    relis_nic_init();

    __asm__ volatile("sti");
    printk("Interrupts enabled");

    kernel_thread("init", init_task, 0, SCHED_NORMAL);
    kernel_thread("kworker", heartbeat_task, 0, SCHED_NORMAL);

    // Wake up APs AFTER interrupts are enabled
    smp_boot_apus();

    printk("RELIS Kernel  initialized. Kept Idling...");

    schedule();

    while (1) {
        __asm__ volatile("hlt");
        if (current_task->flags & TIF_NEED_RESCHED) {
            schedule();
        }
    }
}
