#include "relisfs.h"
#include "drivers/block/ata.h"
#include "relis/mm.h"
#include "relis/printk.h"
#include "relis/string.h"
#include <stdint.h>

#define RELISFS_MAGIC 0x52454C49 // "RELI"

typedef struct {
    uint32_t magic;
    uint32_t total_inodes;
    uint32_t inode_table_lba;
    uint32_t data_block_lba;
} relisfs_superblock_t;

typedef struct {
    uint32_t used;
    uint32_t size;
    uint32_t direct_block;
    char name[64];
} relisfs_inode_t;

static uint8_t block_buf[512];

static ssize_t relisfs_read(struct file *f, char *buf, size_t count) {
    relisfs_inode_t *inode = (relisfs_inode_t*)f->f_inode->i_private;
    if (!inode || !inode->used) return -1;

    if (f->f_pos >= inode->size) return 0;
    if (f->f_pos + count > inode->size) count = inode->size - f->f_pos;

    if (ata_read_block(inode->direct_block, block_buf) < 0) return -1;
    kmemcpy(buf, block_buf + f->f_pos, count);
    f->f_pos += count;
    return count;
}

static ssize_t relisfs_write(struct file *f, const char *buf, size_t count) {
    relisfs_inode_t *inode = (relisfs_inode_t*)f->f_inode->i_private;
    if (!inode || !inode->used) return -1;

    if (ata_read_block(inode->direct_block, block_buf) < 0) return -1;
    kmemcpy(block_buf + f->f_pos, buf, count);
    if (ata_write_block(inode->direct_block, block_buf) < 0) return -1;

    f->f_pos += count;
    if (f->f_pos > inode->size) {
        inode->size = f->f_pos;
        // Update inode on disk
        if (ata_read_block(1, block_buf) == 0) {
            kmemcpy(block_buf + (sizeof(relisfs_inode_t) * 1), inode, sizeof(relisfs_inode_t));
            ata_write_block(1, block_buf);
        }
    }
    return count;
}

static const struct file_operations relisfs_fops = {
    .read = relisfs_read,
    .write = relisfs_write,
};

static struct dentry *relisfs_mount(struct file_system_type *fs, const char *dev) {
    (void)fs; (void)dev;
    if (ata_read_block(0, block_buf) < 0) {
        printk("RELISFS: Failed to read disk!");
        return NULL;
    }

    relisfs_superblock_t *sb = (relisfs_superblock_t*)block_buf;

    if (sb->magic != RELISFS_MAGIC) {
        printk("RELISFS: No filesystem found. Formatting...");
        
        sb->magic = RELISFS_MAGIC;
        sb->total_inodes = 64;
        sb->inode_table_lba = 1;
        sb->data_block_lba = 2;
        ata_write_block(0, block_buf);

        // Initialize inode table
        kmemset(block_buf, 0, 512);
        ata_write_block(1, block_buf);
        
        // Create root directory (Inode 1)
        relisfs_inode_t root_inode;
        kmemset(&root_inode, 0, sizeof(root_inode));
        root_inode.used = 1;
        root_inode.direct_block = 2;
        kstrcpy(root_inode.name, "/");
        
        ata_read_block(1, block_buf);
        kmemcpy(block_buf + (sizeof(relisfs_inode_t) * 1), &root_inode, sizeof(root_inode));
        ata_write_block(1, block_buf);
        
        printk("RELISFS: Formatted and mounted at /");
    } else {
        printk("RELISFS: Found existing filesystem. Mounted at /");
    }

    struct dentry *root = kmalloc(sizeof(struct dentry));
    kmemset(root, 0, sizeof(struct dentry));
    kstrcpy(root->d_name, "/");
    root->d_inode = kmalloc(sizeof(struct inode));
    kmemset(root->d_inode, 0, sizeof(struct inode));
    root->d_inode->i_fop = &relisfs_fops;
    
    // Load root inode from disk into memory
    ata_read_block(1, block_buf);
    relisfs_inode_t *disk_inode = kmalloc(sizeof(relisfs_inode_t));
    kmemcpy(disk_inode, block_buf + (sizeof(relisfs_inode_t) * 1), sizeof(relisfs_inode_t));
    root->d_inode->i_private = disk_inode;

    return root;
}

struct file_system_type relisfs_fs_type = {
    .name = "relisfs",
    .mount = relisfs_mount,
};
