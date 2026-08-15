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
#include "drivers/pci/pci.h"
#include "drivers/block/ata.h"
#include "fs/relisfs/relisfs.h"
#include "relis_nic.h"

// these symbols are custom addresses generated automatically by the linker scripts during the final build process
// they do not store ordinary numbers or string data but instead mark the start boundary and end boundary of our packed application binary file
// by subtracting the raw address value of the starting boundary symbol from the raw address value of the ending boundary symbol we can easily compute the exact file footprint size in bytes without needing a file system directory or partition lookup table
extern struct file_system_type proc_fs_type;
extern void exec_user_program(uint8_t *elf_data);

extern uint8_t _binary_user_prog_elf_start[];
extern uint8_t _binary_user_prog_elf_end[];

// this is the primary bootstrapping worker task that the scheduling engine executes first when multitasking comes online
// it performs simple pointer subtraction across our embedded binary symbols to figure out how large the compiled application binary is
// next it sends a formatted message string to our screen output console giving a status readout of the loading operation in bytes
// then it calls our ring 3 switch routine passing it the raw address layout pointer so the kernel can start executing user code loops
static void init_task(void) {
    uint64_t prog_size = _binary_user_prog_elf_end - _binary_user_prog_elf_start;
    printk("Loading user program (%d bytes)...", prog_size);
    exec_user_program(_binary_user_prog_elf_start);
}

// this is a secondary background monitoring thread that handles power management and system state balancing
// it spins inside an absolute infinite loop that stops the cpu from spinning hot and wasting millions of clock cycles when no tasks have real work to process
// the inline block runs the x86 assembly hlt command which puts the physical processor execution pipeline completely to sleep until an external pin or internal timer triggers an interrupt signal
// the very microsecond a timer tick keyboard stroke or network packet wakes up the cpu core it immediately jumps out of the sleep state and calls schedule to find out which active task needs to run next
static void heartbeat_task(void) {
    while (1) {
        __asm__ volatile("hlt");
        schedule();
    }
}

void start_kernel(uint64_t mb_magic, void *mb_info) {
    (void)mb_magic; (void)mb_info;

    console_init();
    printk("RELIS 64-BIT KERNEL BOOTING");

    // gdt init creates our basic code segments data segments and task state segments to establish ring boundaries and cpu thread safety
    gdt_init();
    // pmm init builds out the physical page allocation maps constructing a trackable block bitmap that manages 262144 free pages starting at block zero
    pmm_init(262144, 0);
    // irq init configures the programmable interrupt controller and registers handlers to trap critical cpu errors or hardware events safely
    irq_init();
    // sched init initializes our priority task queues allocates base thread contexts and sets up head structures for our round robin processing loop
    sched_init();
    // syscall init writes to advanced model specific registers on the cpu so that user applications can use fast syscall assembly instructions to drop into kernel routines instantly
    syscall_init();

    // paging init instantiates our page tables maps out structural memory zones sets memory protections and writes the base table directory address directly into the cr3 register
    // this instantly switches the processor out of raw physical execution and turns on a protected virtual memory layout that isolates kernel space from user land data corruption
    paging_init();

    timer_init(100);
    keyboard_init();

    vfs_init();
    ata_init();
    
    struct dentry *root = vfs_kern_mount(&relisfs_fs_type);
    printk("Root filesystem (relisfs) mounted at /");

    // mounts our process tracking interface system which lets user space look at current task properties and system resource structures like ordinary files
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

    printk("RELIS Kernel initialized. Idling...");

    schedule();

    // this is our absolute final fallback infinite loop that acts as the baseline power saver loop for our primary bootstrap processor core
    // if every thread in the operating system enters a blocked sleep state while waiting for network packets hard drive data or keyboard presses the cpu falls straight through to this line
    // it executes the assembly halt instruction to freeze the execution core in a cool low power state until a hardware device signals an interrupt that raises the thread reschedule flag bit
    while (1) {
        __asm__ volatile("hlt");
        if (current_task->flags & TIF_NEED_RESCHED) {
            schedule();
        }
    }
}
