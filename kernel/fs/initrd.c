/* =============================================================================
   RELIS — kernel/fs/initrd.c
   Builds a hard-coded in-memory filesystem and mounts it as root "/".
   This gives RELIS real file content without needing any disk driver.

   Directory tree created:
     /
     ├── etc/
     │   ├── hostname      "stackos\n"
     │   └── motd          welcome message
     ├── bin/              (directory, programs go here later)
     └── README            short description
   ============================================================================= */
#include "initrd.h"
#include "vfs.h"
#include "kprintf.h"
#include "mm/heap.h"
#include <stddef.h>
#include <stdint.h>

/* ── Hard-coded file content ─────────────────────────────────────────────── */
static const char content_hostname[] = "stackos\n";

static const char content_motd[] =
    "Welcome to RELIS!\n"
    "A hand-crafted OS built from scratch.\n"
    "Type 'help' in the shell for available commands.\n";

static const char content_readme[] =
    "RELIS v0.2.0-alpha\n"
    "========================\n"
    "This is the initrd root filesystem.\n"
    "\n"
    "Directories:\n"
    "  /etc   — system configuration\n"
    "  /bin   — user programs (coming soon)\n"
    "\n"
    "Built with love and assembly.\n";

/* ── Node pool — we allocate VFS nodes from here ─────────────────────────── */
#define NODE_POOL_SIZE 64
static vfs_node_t node_pool[NODE_POOL_SIZE];
static int node_pool_idx = 0;

static vfs_node_t *alloc_node(void) {
    if (node_pool_idx >= NODE_POOL_SIZE) return NULL;
    vfs_node_t *n = &node_pool[node_pool_idx++];
    /* zero it */
    uint8_t *b = (uint8_t *)n;
    for (size_t i = 0; i < sizeof(vfs_node_t); i++) b[i] = 0;
    return n;
}

/* ── Directory child list ─────────────────────────────────────────────────── */
#define MAX_CHILDREN 16
typedef struct {
    vfs_node_t *children[MAX_CHILDREN];
    int         count;
} dir_data_t;

static dir_data_t dir_data_pool[16];
static int        dir_data_idx = 0;

static dir_data_t *alloc_dir_data(void) {
    if (dir_data_idx >= 16) return NULL;
    dir_data_t *d = &dir_data_pool[dir_data_idx++];
    d->count = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) d->children[i] = NULL;
    return d;
}

/* ── VFS operations for initrd files ────────────────────────────────────── */
static int32_t initrd_read(vfs_node_t *node, uint32_t offset,
                            uint32_t size, uint8_t *buf) {
    const uint8_t *data = (const uint8_t *)node->fs_data;
    if (!data || offset >= node->size) return 0;
    uint32_t avail = node->size - offset;
    if (size > avail) size = avail;
    for (uint32_t i = 0; i < size; i++) buf[i] = data[offset + i];
    return (int32_t)size;
}

static int32_t initrd_write(vfs_node_t *node, uint32_t offset,
                             uint32_t size, const uint8_t *buf) {
    (void)node; (void)offset; (void)size; (void)buf;
    return -1;  /* read-only filesystem */
}

static dirent_t *initrd_readdir(vfs_node_t *node, uint32_t idx) {
    dir_data_t *d = (dir_data_t *)node->fs_data;
    if (!d || (int)idx >= d->count) return NULL;
    static dirent_t de;
    vfs_node_t *child = d->children[idx];
    /* copy name */
    size_t i = 0;
    while (i < VFS_NAME_MAX - 1 && child->name[i]) {
        de.name[i] = child->name[i]; i++;
    }
    de.name[i] = '\0';
    de.inode = child->inode;
    return &de;
}

static vfs_node_t *initrd_finddir(vfs_node_t *node, const char *name) {
    dir_data_t *d = (dir_data_t *)node->fs_data;
    if (!d) return NULL;
    for (int i = 0; i < d->count; i++) {
        vfs_node_t *c = d->children[i];
        /* strcmp */
        const char *a = c->name, *b = name;
        while (*a && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return c;
    }
    return NULL;
}

static vfs_ops_t file_ops = {
    .read    = initrd_read,
    .write   = initrd_write,
    .open    = NULL,
    .close   = NULL,
    .readdir = NULL,
    .finddir = NULL,
    .create  = NULL,
    .mkdir   = NULL,
    .unlink  = NULL,
};

static vfs_ops_t dir_ops = {
    .read    = NULL,
    .write   = NULL,
    .open    = NULL,
    .close   = NULL,
    .readdir = initrd_readdir,
    .finddir = initrd_finddir,
    .create  = NULL,
    .mkdir   = NULL,
    .unlink  = NULL,
};

/* ── Helper: make a file node ─────────────────────────────────────────────── */
static uint32_t inode_counter = 1;

static vfs_node_t *make_file(const char *name,
                              const char *data, uint32_t size) {
    vfs_node_t *n = alloc_node();
    if (!n) return NULL;
    /* copy name */
    size_t i = 0;
    while (i < VFS_NAME_MAX - 1 && name[i]) { n->name[i] = name[i]; i++; }
    n->name[i] = '\0';
    n->flags   = VFS_FILE;
    n->inode   = inode_counter++;
    n->size    = size;
    n->perm    = 0644;
    n->ops     = &file_ops;
    n->fs_data = (void *)data;
    return n;
}

/* ── Helper: make a directory node ───────────────────────────────────────── */
static vfs_node_t *make_dir(const char *name) {
    vfs_node_t *n = alloc_node();
    dir_data_t *d = alloc_dir_data();
    if (!n || !d) return NULL;
    size_t i = 0;
    while (i < VFS_NAME_MAX - 1 && name[i]) { n->name[i] = name[i]; i++; }
    n->name[i] = '\0';
    n->flags   = VFS_DIR;
    n->inode   = inode_counter++;
    n->size    = 0;
    n->perm    = 0755;
    n->ops     = &dir_ops;
    n->fs_data = d;
    return n;
}

/* ── Helper: add a child to a directory ──────────────────────────────────── */
static void dir_add(vfs_node_t *dir, vfs_node_t *child) {
    dir_data_t *d = (dir_data_t *)dir->fs_data;
    if (!d || d->count >= MAX_CHILDREN) return;
    d->children[d->count++] = child;
}

/* ── Public init ──────────────────────────────────────────────────────────── */
void initrd_init(void) {
    /* Build the tree */
    vfs_node_t *root = make_dir("/");

    /* /etc */
    vfs_node_t *etc      = make_dir("etc");
    vfs_node_t *hostname = make_file("hostname",
                                     content_hostname,
                                     sizeof(content_hostname) - 1);
    vfs_node_t *motd     = make_file("motd",
                                     content_motd,
                                     sizeof(content_motd) - 1);
    dir_add(etc, hostname);
    dir_add(etc, motd);
    dir_add(root, etc);

    /* /bin */
    vfs_node_t *bin = make_dir("bin");
    dir_add(root, bin);

    /* /README */
    vfs_node_t *readme = make_file("README",
                                   content_readme,
                                   sizeof(content_readme) - 1);
    dir_add(root, readme);

    /* Mount at "/" */
    vfs_mount("/", root);

    kprintf("[initrd] Root filesystem ready: %d nodes\n", node_pool_idx);
}
