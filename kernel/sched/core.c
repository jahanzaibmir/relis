#include "sched.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

struct rq runqueues;
struct task_struct *current_task = 0;
static struct list_head global_tasks;
static uint32_t next_pid = 1;

extern void set_tss_rsp0(uint64_t rsp);
extern uint8_t stack_top[];

void sched_init(void) {
    INIT_LIST_HEAD(&runqueues.cfs_queue);
    INIT_LIST_HEAD(&runqueues.rt_queue);
    INIT_LIST_HEAD(&global_tasks);
    runqueues.nr_running = 0;
    runqueues.clock = 0;

    current_task = kmalloc(sizeof(struct task_struct));
    if (!current_task) {
        printk("PANIC: Failed to allocate initial task!");
        for (;;) __asm__ volatile("cli; hlt");
    }
    kmemset(current_task, 0, sizeof(struct task_struct));
    current_task->pid = 0;
    kstrcpy(current_task->name, "swapper");
    current_task->state = TASK_RUNNING;
    current_task->sched_class = &idle_sched_class;
    
    current_task->kernel_stack_top = (uint64_t*)stack_top;
    
    list_add(&current_task->tasks, &global_tasks);
    printk("Scheduler initialized (Class-based, Preemptive)");
}

static void thread_trampoline(void) {
    __asm__ volatile("sti");
    current_task->fn();
    current_task->state = TASK_ZOMBIE;
    while (1) schedule();
}

int kernel_thread(const char *name, void (*fn)(void), void *arg, int policy) {
    struct task_struct *p = kmalloc(sizeof(struct task_struct));
    if (!p) return -1;
    
    kmemset(p, 0, sizeof(struct task_struct));
    p->pid = next_pid++;
    kstrcpy(p->name, name);
    p->state = TASK_RUNNING;
    p->policy = policy;
    p->fn = fn;
    p->arg = arg;
    
    if (policy == SCHED_FIFO || policy == SCHED_RR) {
        p->prio = 50; 
        p->rt.prio = 50;
        p->sched_class = &rt_sched_class;
    } else {
        p->prio = DEFAULT_PRIO;
        p->se.weight = 1024;
        p->sched_class = &fair_sched_class;
    }
    
    p->kernel_stack_top = p->kernel_stack + (KERNEL_STACK_SIZE / 8);
    uint64_t *stack = p->kernel_stack_top;
    
    *--stack = (uint64_t)thread_trampoline;
    *--stack = 0; // RBP
    *--stack = 0; // RBX
    *--stack = 0; // R12
    *--stack = 0; // R13
    *--stack = 0; // R14
    *--stack = 0; // R15
    
    p->rsp = stack;
    
    if (p->sched_class->enqueue_task) {
        p->sched_class->enqueue_task(p);
    }
    list_add(&p->tasks, &global_tasks);
    return p->pid;
}

void wake_up_process(struct task_struct *p) {
    if (p && p->state == TASK_INTERRUPTIBLE) {
        p->state = TASK_RUNNING;
        if (p->sched_class->enqueue_task) {
            p->sched_class->enqueue_task(p);
        }
    }
}

void scheduler_tick(void) {
    runqueues.clock++;
    if (current_task->sched_class->task_tick) {
        current_task->sched_class->task_tick(current_task);
    }
}

void __schedule(void) {
    struct task_struct *prev = current_task;
    struct task_struct *next = NULL;
    
    if (rt_sched_class.pick_next_task) {
        next = rt_sched_class.pick_next_task();
    }
    if (!next && fair_sched_class.pick_next_task) {
        next = fair_sched_class.pick_next_task();
    }
    if (!next) {
        next = idle_sched_class.pick_next_task();
    }
    
    if (next == prev) {
        current_task->flags &= ~TIF_NEED_RESCHED;
        return;
    }
    
    // FIX: DO NOT dequeue here! pick_next_task_fair already rotated the queue.
    // If we dequeue, the task is lost when preempted!
    
    current_task = next;
    set_tss_rsp0((uint64_t)next->kernel_stack_top);
    switch_to(prev, next);
}

void schedule(void) {
    __schedule();
}
