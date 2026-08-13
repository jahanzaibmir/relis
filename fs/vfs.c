#include "relis/fs.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

static struct dentry *root_dentry = NULL;

void vfs_init(void) {
    printk("VFS core initialized");
}

struct dentry *vfs_kern_mount(struct file_system_type *fs) {
    if (!fs || !fs->mount) return NULL;
    root_dentry = fs->mount(fs, NULL);
    return root_dentry;
}

struct file *vfs_open(const char *path) {
    if (!root_dentry || path[0] != '/') return NULL;
    path++; // Skip leading slash

    struct dentry *current_d = root_dentry;
    char name[64];
    int i = 0;

    while (*path) {
        if (*path == '/') {
            name[i] = '\0';
            if (i > 0) {
                int found = 0;
                for (int j = 0; j < current_d->d_child_count; j++) {
                    if (kstrcmp(current_d->d_children[j]->d_name, name) == 0) {
                        current_d = current_d->d_children[j];
                        found = 1;
                        break;
                    }
                }
                if (!found) return NULL;
            }
            i = 0;
        } else {
            name[i++] = *path;
        }
        path++;
    }
    
    name[i] = '\0';
    if (i > 0) {
        int found = 0;
        for (int j = 0; j < current_d->d_child_count; j++) {
            if (kstrcmp(current_d->d_children[j]->d_name, name) == 0) {
                current_d = current_d->d_children[j];
                found = 1;
                break;
            }
        }
        if (!found) return NULL;
    }

    struct file *f = kmalloc(sizeof(struct file));
    f->f_dentry = current_d;
    f->f_inode = current_d->d_inode;
    f->f_pos = 0;
    return f;
}

ssize_t vfs_read(struct file *f, char *buf, size_t count) {
    if (!f || !f->f_inode || !f->f_inode->i_fop || !f->f_inode->i_fop->read) return -1;
    return f->f_inode->i_fop->read(f, buf, count);
}

ssize_t vfs_write(struct file *f, const char *buf, size_t count) {
    if (!f || !f->f_inode || !f->f_inode->i_fop || !f->f_inode->i_fop->write) return -1;
    return f->f_inode->i_fop->write(f, buf, count);
}
