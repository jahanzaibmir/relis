#include "relis/sched.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/spinlock.h"
#include "relis/fs.h"   // Added for struct file and file_operations
#include <stdint.h>

/// Guys these are pipes

typedef struct {
    uint8_t buffer[4096];
    uint32_t read_pos;
    uint32_t write_pos;
    spinlock_t lock;
} relis_pipe_t;

static ssize_t pipe_read(struct file *f, char *buf, size_t count) {
    relis_pipe_t *p = (relis_pipe_t*)f->f_inode->i_private;
    
    while (1) {
        spin_lock(&p->lock);
        if (p->read_pos != p->write_pos) {
            size_t to_copy = p->write_pos - p->read_pos;
            if (to_copy > count) to_copy = count;
            kmemcpy(buf, p->buffer + p->read_pos, to_copy);
            p->read_pos += to_copy;
            spin_unlock(&p->lock);
            return to_copy;
        }
        spin_unlock(&p->lock);
        schedule(); // Yield until data is available
    }
    return 0;
}

static ssize_t pipe_write(struct file *f, const char *buf, size_t count) {
    relis_pipe_t *p = (relis_pipe_t*)f->f_inode->i_private;
    
    spin_lock(&p->lock);
    size_t space = 4096 - p->write_pos;
    size_t to_copy = count < space ? count : space;
    kmemcpy(p->buffer + p->write_pos, buf, to_copy);
    p->write_pos += to_copy;
    spin_unlock(&p->lock);
    
    return to_copy;
}

static const struct file_operations pipe_read_fops = { .read = pipe_read };
static const struct file_operations pipe_write_fops = { .write = pipe_write };

long sys_pipe(int fds[2]) {
    relis_pipe_t *p = kmalloc(sizeof(relis_pipe_t));
    kmemset(p, 0, sizeof(relis_pipe_t));
    p->lock.lock = 0; //
    
    struct file *read_end = kmalloc(sizeof(struct file));
    read_end->f_inode = kmalloc(sizeof(struct inode));
    read_end->f_inode->i_fop = &pipe_read_fops;
    read_end->f_inode->i_private = p;
    read_end->f_pos = 0;
    
    struct file *write_end = kmalloc(sizeof(struct file));
    write_end->f_inode = kmalloc(sizeof(struct inode));
    write_end->f_inode->i_fop = &pipe_write_fops;
    write_end->f_inode->i_private = p;
    write_end->f_pos = 0;
    
    // Allocate FDs
    int rfd = -1, wfd = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!current_task->files[i]) {
            if (rfd == -1) {
                rfd = i;
                current_task->files[i] = read_end;
            } else {
                wfd = i;
                current_task->files[i] = write_end;
                break;
            }
        }
    }
    
    if (rfd == -1 || wfd == -1) return -1;
    
    // Return to user space
    fds[0] = rfd;
    fds[1] = wfd;
    return 0;
}



#define SHM_VADDR_START 0x60000000

long sys_shmget(size_t size) {
    (void)size; // We only support 4KB pages for now
    if (current_task->shm_count >= MAX_SHM_REGIONS) return -1;
    
    uint64_t phys = alloc_page();
    int id = current_task->shm_count++;
    current_task->shm_pages[id] = phys;
    return id;
}

long sys_shmat(int id) {
    if (id < 0 || id >= current_task->shm_count) return -1;
    
    // Map it into user space
    uint64_t virt = SHM_VADDR_START + (id * PAGE_SIZE);
    arch_map_page(virt, current_task->shm_pages[id], PTE_WRITABLE | PTE_USER);
    return virt;
}
