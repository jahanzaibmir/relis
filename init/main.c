// init/main.c
#include "relis/printk.h"
#include "relis/sched.h"
#include "relis/irq.h"
#include "relis/syscall.h"
#include "relis/mm.h"
#include "relis/fs.h"
#include "relis/net.h"
#include "relis/types.h"
#include "arch/x86/gdt.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"

extern struct file_system_type proc_fs_type;
extern struct file_system_type ramfs_fs_type;

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
} multiboot_info_t;

void start_kernel(uint32_t mb_magic, multiboot_info_t *mb_info) {
    (void)mb_magic;
    
    console_init();
    printk("=== RELIS KERNEL BOOTING ===");

    gdt_init();
    pmm_init(mb_info->mem_upper, 0); 
    irq_init();  
    sched_init(); 
    syscall_init();

    timer_init(100);
    keyboard_init();

    vfs_init();
    
    // Mount ramfs as root
    struct dentry *root = vfs_kern_mount(&ramfs_fs_type);
    printk("Root filesystem (ramfs) mounted at /");

    // Mount proc and attach it to the root directory
    struct dentry *proc_root = proc_fs_type.mount(&proc_fs_type, NULL);
    if (root && proc_root) {
        kstrcpy(proc_root->d_name, "proc");
        root->d_children[root->d_child_count++] = proc_root;
    }
    printk("Proc filesystem mounted at /proc");

    // Initialize the network subsystem
    net_init();

    __asm__ volatile("sti");
    printk("Interrupts enabled");

    struct file *f = vfs_open("/proc/cpuinfo");
    if (f) {
        char buf[128];
        ssize_t bytes = vfs_read(f, buf, 127);
        if (bytes > 0) {
            buf[bytes] = '\0';
            printk("Read from /proc/cpuinfo: %s", buf);
        }
    } else {
        printk("Failed to open /proc/cpuinfo");
    }

    printk("RELIS Kernel v1.0 initialized. Idling...");

    while (1) {
        __asm__ volatile("hlt");
    }
}