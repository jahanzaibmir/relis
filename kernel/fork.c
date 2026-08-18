#include "sched/sched.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"
#include "relis/irq.h"
#include <stdint.h>

extern void isr_common_return(void);

long sys_fork(struct registers *regs) {
    struct task_struct *parent = current_task;
    struct task_struct *child = kmalloc(sizeof(struct task_struct));
    if (!child) return -1;

    kmemcpy(child, parent, sizeof(struct task_struct));
    child->pid = next_pid++;
    child->ppid = parent->pid;
    child->state = TASK_RUNNING;
    child->pending_signals = 0;

    
    child->cr3 = clone_address_space();

    // Clone kernel stack
    child->kernel_stack_top = child->kernel_stack + (KERNEL_STACK_SIZE / 8);

    struct registers *child_regs = (struct registers*)((uint64_t)child->kernel_stack + KERNEL_STACK_SIZE - sizeof(struct registers));
    kmemcpy(child_regs, regs, sizeof(struct registers));
    child_regs->rax = 0;

    uint64_t *stack = (uint64_t*)child_regs;
    *--stack = (uint64_t)isr_common_return;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;

    child->rsp = stack;

    INIT_LIST_HEAD(&child->se.run_node);

    if (child->sched_class->enqueue_task) {
        child->sched_class->enqueue_task(child);
    }
    list_add(&child->tasks, &global_tasks);

    printk("Forked process %s (PID %d) -> PID %d (CoW)", parent->name, parent->pid, child->pid);
    
    parent->pending_signals |= (1ULL << RELIS_SIGCHLD);
    
    return child->pid;
}
