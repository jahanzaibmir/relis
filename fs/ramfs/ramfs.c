#include "relis/fs.h"
#include "relis/mm.h"
#include "relis/string.h"

static struct dentry *ramfs_mount(struct file_system_type *fs, const char *dev) {
    (void)fs; (void)dev;
    struct dentry *root = kmalloc(sizeof(struct dentry));
    kmemset(root, 0, sizeof(struct dentry));
    kstrcpy(root->d_name, "/");
    return root;
}

struct file_system_type ramfs_fs_type = {
    .name = "ramfs",
    .mount = ramfs_mount,
};
