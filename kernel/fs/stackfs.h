/* =============================================================================
 S tackOS — kernel/fs/bliz*zf*s.h
 StackFS — a simple flat filesystem for persistent storage.

 Inode size audit:
 type(1) + _pad(1) + perm(2) + size(4) + created(4) + modified(4)
 + blocks[12](48) + name[56](56) = 120 bytes per inode

 Disk layout (all sizes in 512-byte sectors):
 Sector 0       : Superblock
 Sector 1-30    : Inode table  (128 inodes × 120 bytes = 15360 bytes = 30 sectors)
 Sector 31-62   : Bitmap       (free block tracking, 32 sectors)
 Sector 63+     : Data blocks  (4096 bytes each = 8 sectors each)

 With a 64 MiB image (131072 sectors):
 Data blocks = (131072 - 63) / 8 = 16376 blocks = ~64 MiB usable

 An inode stores: name, size, type, timestamps, and up to 12 direct
 block pointers. Max file size = 12 × 4096 = 48 KiB (expandable later).
 ============================================================================= */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "fs/vfs.h"

/* ── Filesystem constants ────────────────────────────────────────────────── */
#define STACKFS_MAGIC       0x57AC4B05   /* "STACKFS"                        */
#define STACKFS_VERSION     1
#define STACKFS_SECTOR_SIZE 512
#define STACKFS_BLOCK_SIZE  4096         /* 8 sectors per block              */
#define STACKFS_NAME_MAX    56
#define STACKFS_MAX_INODES  128
#define STACKFS_DIRECT_PTRS 12

/*
 * Sector layout — recalculated to match actual inode struct size (120 bytes).
 *
 * 128 inodes × 120 bytes = 15360 bytes = 30 sectors for the inode table.
 * (The original code reserved only 8 sectors = space for 34 inodes,
 *  causing load_inodes/save_inodes to read/write past the reserved area.)
 */
#define STACKFS_SB_SECTOR    0    /* superblock (1 sector)                   */
#define STACKFS_INODE_START  1    /* inode table starts here                 */
#define STACKFS_INODE_SECS   30   /* 30 sectors × 512 = 15360 = 128 × 120   */
#define STACKFS_BITMAP_START 31   /* block bitmap                            */
#define STACKFS_BITMAP_SECS  32   /* 32 sectors = 131072 bits = 131072 blks  */
#define STACKFS_DATA_START   63   /* data blocks start here                  */

/* Inode types */
#define SFS_TYPE_FREE  0
#define SFS_TYPE_FILE  1
#define SFS_TYPE_DIR   2

/* ── On-disk superblock (fits in one 512-byte sector) ────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t total_blocks;    /* total data blocks on disk              */
    uint32_t free_blocks;     /* free data blocks                       */
    uint32_t total_inodes;    /* always STACKFS_MAX_INODES              */
    uint32_t free_inodes;
    uint32_t root_inode;      /* inode number of root directory         */
    uint32_t created;         /* creation timestamp                     */
    char     label[32];       /* volume label                           */
    uint8_t  _pad[512 - 60];
} stackfs_super_t;

/*
 * On-disk inode — 120 bytes each.
 * 128 inodes fit in 30 sectors (15360 bytes).
 *
 * Do NOT change the field layout without recalculating STACKFS_INODE_SECS
 * and re-running stackfs_format() to rewrite the disk.
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;                          /* SFS_TYPE_*               */
    uint8_t  _pad;
    uint16_t perm;                          /* permission bits          */
    uint32_t size;                          /* file size in bytes       */
    uint32_t created;
    uint32_t modified;
    uint32_t blocks[STACKFS_DIRECT_PTRS];   /* data block numbers       */
    char     name[STACKFS_NAME_MAX];        /* file/dir name            */
} stackfs_inode_t;

/* Compile-time size check — will error if the struct ever drifts */
_Static_assert(sizeof(stackfs_inode_t) == 120,
               "stackfs_inode_t size mismatch — update STACKFS_INODE_SECS");

/* ── In-memory directory entry (stored in dir data blocks) ──────────────── */
typedef struct __attribute__((packed)) {
    uint32_t inode;
    char     name[STACKFS_NAME_MAX];
} stackfs_dirent_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Format the disk with StackFS (erases all data) */
int  stackfs_format(const char *label);

/* Mount StackFS — returns VFS root node or NULL */
vfs_node_t *stackfs_mount(void);

/* Check if disk has a valid StackFS superblock */
int  stackfs_detect(void);
