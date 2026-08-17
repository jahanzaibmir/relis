#pragma once
#include <stdint.h>
#include <stddef.h>
#include "relis/types.h"

struct file;
struct inode;

struct file_operations {
    ssize_t (*read)(struct file *, char *, size_t);
    ssize_t (*write)(struct file *, const char *, size_t);
};

struct inode {
    uint32_t i_ino;
    uint32_t i_mode; // 1=dir, 2=file
    uint32_t i_size;
    void *i_private;
    const struct file_operations *i_fop;
};

struct dentry {
    char d_name[64];
    struct inode *d_inode;
    struct dentry *d_parent;
    struct dentry *d_children[32];
    int d_child_count;
};

struct file {
    struct dentry *f_dentry;
    struct inode *f_inode;
    uint32_t f_pos;
};

struct file_system_type {
    const char *name;
    struct dentry *(*mount)(struct file_system_type *, const char *dev);
};

void vfs_init(void);
struct dentry *vfs_kern_mount(struct file_system_type *fs);
struct file *vfs_open(const char *path);
ssize_t vfs_read(struct file *f, char *buf, size_t count);
ssize_t vfs_write(struct file *f, const char *buf, size_t count);