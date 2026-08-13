/* =============================================================================
 RELIS — kernel/fs/vfs.h                             *
 Virtual File System interface.
 ============================================================================= */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define VFS_FILE      0x01
#define VFS_DIR       0x02
#define VFS_CHARDEV   0x04
#define VFS_BLOCKDEV  0x08
#define VFS_SYMLINK   0x10

#define VFS_NAME_MAX  128
#define VFS_PATH_MAX  512

struct vfs_node;
struct dirent;

typedef struct vfs_ops {
    int32_t (*read)   (struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buf);
    int32_t (*write)  (struct vfs_node *node, uint32_t offset, uint32_t size, const uint8_t *buf);
    void    (*open)   (struct vfs_node *node);
    void    (*close)  (struct vfs_node *node);
    struct dirent    *(*readdir)(struct vfs_node *node, uint32_t idx);
    struct vfs_node  *(*finddir)(struct vfs_node *node, const char *name);
    int     (*create) (struct vfs_node *dir, const char *name, uint32_t flags);
    int     (*mkdir)  (struct vfs_node *dir, const char *name);
    int     (*unlink) (struct vfs_node *dir, const char *name);
} vfs_ops_t;

typedef struct vfs_node {
    char        name[VFS_NAME_MAX];
    uint32_t    flags;
    uint32_t    inode;
    uint32_t    size;
    uint32_t    uid, gid;
    uint32_t    perm;
    uint32_t    atime, mtime, ctime;
    vfs_ops_t  *ops;
    void       *fs_data;
    struct vfs_node *ptr;
} vfs_node_t;

typedef struct dirent {
    char     name[VFS_NAME_MAX];
    uint32_t inode;
} dirent_t;

void        vfs_init(void);
int         vfs_mount(const char *path, vfs_node_t *node);
vfs_node_t *vfs_open(const char *path);
vfs_node_t *vfs_root(void);
int         vfs_node_is_mount_root(vfs_node_t *node);  /* returns 1 if node must not be freed */

int32_t     vfs_read   (vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);
int32_t     vfs_write  (vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buf);
void        vfs_close  (vfs_node_t *node);
dirent_t   *vfs_readdir(vfs_node_t *node, uint32_t idx);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
