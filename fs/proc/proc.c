#include "relis/fs.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

static ssize_t proc_cpuinfo_read(struct file *f, char *buf, size_t count) {
    const char *info = "processor : 0\nvendor_id : RELIS\ncpu MHz : 3000\n";
    uint32_t len = kstrlen(info);
    if (f->f_pos >= len) return 0;
    
    uint32_t to_copy = len - f->f_pos;
    if (to_copy > count) to_copy = count;
    
    kmemcpy(buf, info + f->f_pos, to_copy);
    f->f_pos += to_copy;
    return to_copy;
}

static const struct file_operations cpuinfo_fops = {
    .read = proc_cpuinfo_read,
};

static struct dentry *proc_mount(struct file_system_type *fs, const char *dev) {
    (void)fs; (void)dev;
    struct dentry *root = kmalloc(sizeof(struct dentry));
    kmemset(root, 0, sizeof(struct dentry));
    kstrcpy(root->d_name, "proc");
    
    struct dentry *cpuinfo = kmalloc(sizeof(struct dentry));
    kmemset(cpuinfo, 0, sizeof(struct dentry));
    kstrcpy(cpuinfo->d_name, "cpuinfo");
    cpuinfo->d_parent = root;
    cpuinfo->d_inode = kmalloc(sizeof(struct inode));
    kmemset(cpuinfo->d_inode, 0, sizeof(struct inode));
    cpuinfo->d_inode->i_mode = 2; // file
    cpuinfo->d_inode->i_fop = &cpuinfo_fops;
    
    root->d_children[root->d_child_count++] = cpuinfo;
    return root;
}

struct file_system_type proc_fs_type = {
    .name = "proc",
    .mount = proc_mount,
};
