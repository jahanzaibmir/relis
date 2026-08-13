/* =============================================================================
   RELIS — kernel/fs/initrd.h / initrd.c
   Initial RAM Disk (initrd) — a simple read-only in-memory filesystem.

   Format (packed into a flat byte array):
     [initrd_header_t]
     [initrd_file_header_t] × nfiles
     [file data for file 0]
     [file data for file 1]
     ...

   The initrd is generated at build time by tools/mkinitrd.c and embedded
   into the kernel image (or passed as a Multiboot module). For now we create
   a hard-coded initrd in BSS so the OS boots without any disk.
   ============================================================================= */
#pragma once
#include "vfs.h"
#include <stdint.h>

#define INITRD_MAGIC    0x57AC1D15   /* "STACKDISC" */
#define INITRD_MAXFILES 32
#define INITRD_NAMEMAX  64

/* One entry per file in the header table */
typedef struct __attribute__((packed)) {
    char     name[INITRD_NAMEMAX];
    uint32_t offset;     /* byte offset from start of data region */
    uint32_t size;       /* size in bytes                         */
    uint32_t flags;      /* VFS_FILE or VFS_DIR                   */
} initrd_file_header_t;

/* Master header at the very start */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t nfiles;
    initrd_file_header_t files[INITRD_MAXFILES];
} initrd_header_t;

/* Create and mount the initrd at "/" */
void initrd_init(void);
