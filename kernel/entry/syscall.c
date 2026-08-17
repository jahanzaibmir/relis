#include "relis/syscall.h"
#include "relis/printk.h"
#include "relis/sched.h"
#include "relis/mm.h"
#include "relis/fs.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include <stdint.h>

static ssize_t stdin_read(struct file *f, char *buf, size_t count) {
    (void)f;
    size_t i = 0;
    while (i < count) {
        char c = keyboard_poll();
        if (c) {
            buf[i++] = c;
        } else {
            schedule();
        }
    }
    return i;
}

static ssize_t stdout_write(struct file *f, const char *buf, size_t count) {
    (void)f;
    for (size_t i = 0; i < count; i++) {
        terminal_putchar(buf[i]);
        serial_putchar(buf[i]);
    }
    return count;
}

static const struct file_operations stdin_fops = { .read = stdin_read };
static const struct file_operations stdout_fops = { .write = stdout_write };

void task_setup_stdio(struct task_struct *t) {
    struct file *in = kmalloc(sizeof(struct file));
    in->f_inode = kmalloc(sizeof(struct inode));
    in->f_inode->i_fop = &stdin_fops;
    in->f_pos = 0;
    t->files[0] = in;

    struct file *out = kmalloc(sizeof(struct file));
    out->f_inode = kmalloc(sizeof(struct inode));
    out->f_inode->i_fop = &stdout_fops;
    out->f_pos = 0;
    t->files[1] = out;

    struct file *err = kmalloc(sizeof(struct file));
    err->f_inode = kmalloc(sizeof(struct inode));
    err->f_inode->i_fop = &stdout_fops;
    err->f_pos = 0;
    t->files[2] = err;
}

void syscall_init(void) {
    printk("Syscall interface initialized (int 0x80 active)");
}

static int alloc_fd(struct task_struct *t, struct file *f) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!t->files[i]) {
            t->files[i] = f;
            return i;
        }
    }
    return -1;
}

extern long sys_fork(struct registers *regs);
extern long sys_execve(struct registers *regs, const char *path);
extern long sys_kill(uint32_t pid, int sig); // FIX: Match uint32_t signature
extern long sys_sigaction(int sig, uint64_t handler);
extern long sys_pipe(int fds[2]);
extern long sys_shmget(size_t size);
extern long sys_shmat(int id);
extern void deliver_signals(struct registers *regs);

void syscall_dispatch(struct registers *regs) {
    struct task_struct *current = current_task;
    
    switch (regs->rax) {
        case SYS_WRITE: {
            int fd = (int)regs->rdi;
            const char *buf = (const char*)regs->rsi;
            size_t count = (size_t)regs->rdx;
            if (fd < 0 || fd >= MAX_FDS || !current->files[fd]) {
                regs->rax = (uint64_t)-1;
                return;
            }
            struct file *f = current->files[fd];
            if (f->f_inode->i_fop && f->f_inode->i_fop->write) {
                regs->rax = f->f_inode->i_fop->write(f, buf, count);
            } else {
                regs->rax = (uint64_t)-1;
            }
            return;
        }
        case SYS_READ: {
            int fd = (int)regs->rdi;
            char *buf = (char*)regs->rsi;
            size_t count = (size_t)regs->rdx;
            if (fd < 0 || fd >= MAX_FDS || !current->files[fd]) {
                regs->rax = (uint64_t)-1;
                return;
            }
            struct file *f = current->files[fd];
            if (f->f_inode->i_fop && f->f_inode->i_fop->read) {
                regs->rax = f->f_inode->i_fop->read(f, buf, count);
            } else {
                regs->rax = (uint64_t)-1;
            }
            return;
        }
        case SYS_OPEN: {
            const char *path = (const char*)regs->rdi;
            struct file *f = vfs_open(path);
            if (!f) {
                regs->rax = (uint64_t)-1;
                return;
            }
            regs->rax = alloc_fd(current, f);
            return;
        }
        case SYS_CLOSE: {
            int fd = (int)regs->rdi;
            if (fd >= 0 && fd < MAX_FDS && current->files[fd]) {
                kfree(current->files[fd]);
                current->files[fd] = NULL;
                regs->rax = 0;
            } else {
                regs->rax = (uint64_t)-1;
            }
            return;
        }
        case SYS_FORK:
            regs->rax = sys_fork(regs);
            return;
        case SYS_EXECVE:
            regs->rax = sys_execve(regs, (const char*)regs->rdi);
            return;
        case SYS_EXIT:
            printk("\n[Kernel] Process '%s' (PID %d) exited with status %d.", current->name, current->pid, (int)regs->rdi);
            current->state = TASK_ZOMBIE;
            schedule();
            return;
        case SYS_KILL:
            regs->rax = sys_kill((uint32_t)regs->rdi, (int)regs->rsi); // FIX: Cast to uint32_t
            return;
        case SYS_SIGACTION:
            regs->rax = sys_sigaction((int)regs->rdi, regs->rsi);
            return;
        case SYS_PIPE:
            regs->rax = sys_pipe((int*)regs->rdi);
            return;
        case SYS_SHMGET:
            regs->rax = sys_shmget((size_t)regs->rdi);
            return;
        case SYS_SHMAT:
            regs->rax = sys_shmat((int)regs->rdi);
            return;
        default:
            printk("Unknown syscall: %d", regs->rax);
            regs->rax = (uint64_t)-1;
            return;
    }
    
    // Deliver pending signals before returning to user space
    deliver_signals(regs);
}
