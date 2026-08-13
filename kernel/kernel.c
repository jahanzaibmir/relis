/*
 * RELIS — Elemental Low-level Instruction System
 * kernel/kernel.c — Kernel entry point
 */

#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "mm/paging.h"
#include "proc/process.h"
#include "syscall/syscall.h"
#include "fs/vfs.h"
#include "fs/stackfs.h"
#include "fs/initrd.h"
#include "drivers/disk/ata.h"
#include "drivers/pci/pci.h"
#include "drivers/net/e1000.h"
#include "net/net.h"
#include "kprintf.h"
#include "shell.h"   /* ADDED */
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr;
    uint32_t config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t fb_addr;
    uint32_t fb_pitch, fb_width, fb_height;
    uint8_t  fb_bpp, fb_type;
} multiboot_info_t;

#define MULTIBOOT_MAGIC_EXPECTED 0x2BADB002
#define KERNEL_HEAP_SIZE (4 * 1024 * 1024)
static uint8_t kernel_heap_region[KERNEL_HEAP_SIZE] __attribute__((aligned(16)));
extern uint32_t kernel_end;

static void klog(const char *s) {
    serial_write("[relis] ");
    serial_write(s);
    serial_write("\n");
}

static void disk_first_boot(void) {
    klog("first boot — formatting StackFS");
    if (stackfs_format("RELIS") != 0) { klog("ERROR: format failed"); return; }
    vfs_node_t *root = stackfs_mount();
    if (!root) { klog("ERROR: mount failed"); return; }
    vfs_mount("/disk", root);
    if (root->ops && root->ops->mkdir) {
        root->ops->mkdir(root, "etc");
        root->ops->mkdir(root, "home");
        root->ops->mkdir(root, "tmp");
        root->ops->mkdir(root, "var");
        root->ops->mkdir(root, "bin");
    }
    klog("first boot complete");
}

void kernel_main(uint32_t mb_magic, multiboot_info_t *mb_info) {
    serial_init();
    klog("=== RELIS booting ===");

    terminal_init();
    klog("terminal OK");

    if (mb_magic != MULTIBOOT_MAGIC_EXPECTED) {
        serial_write("[relis] PANIC: bad multiboot magic\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    klog("multiboot OK");

    gdt_init();
    klog("GDT ready");
    idt_init();
    klog("IDT ready");

    uint32_t total_kb = mb_info->mem_lower + mb_info->mem_upper;
    pmm_init(total_kb, (uint32_t)(uintptr_t)&kernel_end);
    klog("PMM ready");
    heap_init(kernel_heap_region, KERNEL_HEAP_SIZE);
    klog("heap ready");
    paging_init();
    klog("paging ready");

    timer_init(100);
    klog("timer 100Hz");
    keyboard_init();
    klog("keyboard ready");

    proc_init();
    klog("process manager ready");
    syscall_init();
    klog("syscall interface ready");

    vfs_init();
    initrd_init();
    klog("VFS + initrd mounted at /");

    if (ata_init() == 0) {
        klog("ATA disk found");
        if (stackfs_detect()) {
            vfs_node_t *disk_root = stackfs_mount();
            if (disk_root) vfs_mount("/disk", disk_root);
        } else {
            disk_first_boot();
        }
    }

    pci_init();
    if (e1000_init() == 0) {
        net_init();
        if (!dhcp_request()) {
            net_ip   = IP4(10, 0, 2, 15);
            net_gw   = IP4(10, 0, 2, 2);
            net_mask = IP4(255, 255, 255, 0);
            net_dns  = IP4(10, 0, 2, 3);
        }
    }

    __asm__ volatile("sti");
    klog("interrupts enabled");

    terminal_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_writeline("RELIS — Elemental Low-level Instruction System");
    terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("[relis] boot complete — %u KiB RAM\n", total_kb);

    /* We are bypassing the process manager for now to test the keyboard */
    /* proc_create_kernel("heartbeat", heartbeat_task); */

    /* LAUNCH THE SHELL */
    shell_run();

    for (;;) __asm__ volatile("cli; hlt");
}