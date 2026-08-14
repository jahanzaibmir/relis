#include "relis/printk.h"
#include "relis/sched.h"
#include "relis/irq.h"
#include "relis/syscall.h"
#include "relis/mm.h"
#include "relis/fs.h"
#include "relis/net.h"
#include "relis/types.h"
#include "relis/string.h"
#include "arch/gdt.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "relis_nic.h" // <--- Added

extern struct file_system_type proc_fs_type;
extern struct file_system_type ramfs_fs_type;

static void heartbeat_task(void) {
    while (1) { __asm__ volatile("hlt"); }
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

    timer_init(100);
    keyboard_init();

    vfs_init();
    struct dentry *root = vfs_kern_mount(&ramfs_fs_type);
    printk("Root filesystem (ramfs) mounted at /");

    struct dentry *proc_root = proc_fs_type.mount(&proc_fs_type, NULL);
    if (root && proc_root) {
        kstrcpy(proc_root->d_name, "proc");
        root->d_children[root->d_child_count++] = proc_root;
    }
    printk("Proc filesystem mounted at /proc");

    net_init();
    pci_init();
    relis_nic_init(); // <--- Added

    __asm__ volatile("sti");
    printk("Interrupts enabled");

    uint32_t *test_vm = vmalloc(4096);
    if (test_vm) {
        test_vm[0] = 0xDEADBEEF;
    }

    struct file *f = vfs_open("/proc/cpuinfo");
    if (f) {
        char buf[128];
        ssize_t bytes = vfs_read(f, buf, 127);
        if (bytes > 0) {
            buf[bytes] = '\0';
            printk("Read from /proc/cpuinfo: %s", buf);
        }
    }
    
    kernel_thread("kworker", heartbeat_task, 0, SCHED_NORMAL);

    printk("RELIS Kernel v1.0 (x86_64) initialized. Idling...");

    while (1) { __asm__ volatile("hlt"); }
}