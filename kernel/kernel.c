/*
 * RELIS — Elemental Low-level Instruction System
 * kernel/kernel.c — Kernel entry point
 *
 * Boot sequence:
 *   serial → terminal → GDT → IDT → PMM → heap → paging →
 *   timer → keyboard → proc → syscall → VFS → initrd →
 *   ATA → StackFS → PCI → E1000 → network → interrupts → shell
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
#include <stdint.h>

/* Multiboot information structure (Multiboot v1) */
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

/* Kernel heap */
#define KERNEL_HEAP_SIZE (4 * 1024 * 1024)   /* 4 MiB */
static uint8_t kernel_heap_region[KERNEL_HEAP_SIZE] __attribute__((aligned(16)));
extern uint32_t kernel_end;

/* Internal logging helpers */
static void klog(const char *s) {
    serial_write("[relis] ");
    serial_write(s);
    serial_write("\n");
}

/* Disk first-boot: format StackFS and create base directory tree */
static void disk_first_boot(void) {
    klog("first boot — formatting StackFS");

    if (stackfs_format("RELIS") != 0) {
        klog("ERROR: format failed");
        return;
    }

    vfs_node_t *root = stackfs_mount();
    if (!root) {
        klog("ERROR: mount failed after format");
        return;
    }
    vfs_mount("/disk", root);

    /* Create base directory tree */
    if (root->ops && root->ops->mkdir) {
        root->ops->mkdir(root, "etc");
        root->ops->mkdir(root, "home");
        root->ops->mkdir(root, "tmp");
        root->ops->mkdir(root, "var");
        root->ops->mkdir(root, "bin");
        klog("created /disk/{etc,home,tmp,var,bin}");
    }

    /* Write /disk/etc/motd */
    vfs_node_t *etc = vfs_open("/disk/etc");
    if (etc && etc->ops && etc->ops->create) {
        etc->ops->create(etc, "motd", 0);
        vfs_node_t *etc2 = vfs_open("/disk/etc");
        if (etc2) {
            vfs_node_t *motd = vfs_finddir(etc2, "motd");
            if (motd) {
                const char *msg = "Welcome to RELIS.\nType 'help' for available commands.\n";
                uint32_t len = 0;
                while (msg[len]) len++;
                vfs_write(motd, 0, len, (const uint8_t *)msg);
                kfree(motd);
            }
            kfree(etc2);
        }
        kfree(etc);
    }

    klog("first boot complete");
}

/* Background heartbeat process */
static void heartbeat_task(void) {
    while (1) {
        proc_sleep(5000);
        proc_yield();
    }
}

/* Kernel main entry point */
void kernel_main(uint32_t mb_magic, multiboot_info_t *mb_info) {

    /* Serial must come first — used for all early logging */
    serial_init();
    klog("=== RELIS booting ===");

    terminal_init();
    klog("terminal OK");

    if (mb_magic != MULTIBOOT_MAGIC_EXPECTED) {
        serial_write("[relis] PANIC: bad multiboot magic\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    klog("multiboot OK");

    /* Core hardware setup */
    gdt_init();
    klog("GDT ready");

    idt_init();
    klog("IDT ready");

    /* Memory management */
    uint32_t total_kb = mb_info->mem_lower + mb_info->mem_upper;
    pmm_init(total_kb, (uint32_t)(uintptr_t)&kernel_end);
    klog("PMM ready");

    heap_init(kernel_heap_region, KERNEL_HEAP_SIZE);
    klog("heap ready (4 MiB)");

    paging_init();
    klog("paging ready");

    /* Devices */
    timer_init(100);
    klog("timer 100Hz");

    keyboard_init();
    klog("keyboard ready");

    /* Process and syscall subsystems */
    proc_init();
    klog("process manager ready");

    syscall_init();
    klog("syscall interface ready");

    /* Virtual filesystem */
    vfs_init();
    initrd_init();
    klog("VFS + initrd mounted at /");

    /* Disk */
    if (ata_init() == 0) {
        klog("ATA disk found");
        if (stackfs_detect()) {
            klog("StackFS detected — mounting");
            vfs_node_t *disk_root = stackfs_mount();
            if (disk_root) {
                vfs_mount("/disk", disk_root);
                klog("/disk mounted");
            } else {
                klog("ERROR: StackFS mount failed");
            }
        } else {
            klog("no StackFS — running first-boot format");
            disk_first_boot();
        }
    } else {
        klog("no ATA disk found — running diskless");
    }

    /* PCI enumeration */
    pci_init();
    klog("PCI enumeration done");

    /* Network */
    if (e1000_init() == 0) {
        klog("E1000 NIC found");
        net_init();
        klog("network stack ready");

        if (dhcp_request()) {
            klog("DHCP: got IP address");
        } else {
            klog("DHCP: timeout — using fallback 10.0.2.15");
            net_ip   = IP4(10, 0, 2, 15);
            net_gw   = IP4(10, 0, 2, 2);
            net_mask = IP4(255, 255, 255, 0);
            net_dns  = IP4(10, 0, 2, 3);
        }
    } else {
        klog("no E1000 NIC — network disabled");
    }

    /* Enable interrupts */
    __asm__ volatile("sti");
    klog("interrupts enabled");

    /* Boot complete */
    terminal_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_writeline("RELIS — Elemental Low-level Instruction System");
    terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    kprintf("[relis] boot complete — %u KiB RAM\n", total_kb);

    /* Launch background tasks */
    proc_create_kernel("heartbeat", heartbeat_task);

    /* Should never reach here */
    for (;;) __asm__ volatile("cli; hlt");
}