#include "relis/sched.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

LIST_HEAD(runqueue);
struct task_struct *current_task = 0;
static uint32_t next_pid = 1;

void sched_init(void) {
    printk("Scheduler initialized (64-bit Preemptive)");
    current_task = kmalloc(sizeof(struct task_struct));
    if (!current_task) {
        printk("PANIC: Failed to allocate initial task!");
        for (;;) __asm__ volatile("cli; hlt");
    }
    kmemset(current_task, 0, sizeof(struct task_struct));
    current_task->pid = 0;
    kstrcpy(current_task->name, "swapper");
    current_task->state = TASK_RUNNING;
    INIT_LIST_HEAD(&current_task->list);
    list_add(&current_task->list, &runqueue);
}

static void thread_trampoline(void) {
    __asm__ volatile("sti");
    current_task->fn();
    current_task->state = TASK_ZOMBIE;
    while (1) schedule();
}

int kernel_thread(const char *name, void (*fn)(void), void *arg) {
    struct task_struct *p = kmalloc(sizeof(struct task_struct));
    if (!p) return -1;
    
    kmemset(p, 0, sizeof(struct task_struct));
    p->pid = next_pid++;
    kstrcpy(p->name, name);
    p->state = TASK_RUNNING;
    p->fn = fn;
    p->arg = arg;
    
    uint64_t *stack = p->stack + (KERNEL_STACK_SIZE / 8);
    
    *--stack = (uint64_t)thread_trampoline;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    
    p->rsp = stack;
    list_add(&p->list, &runqueue);
    return p->pid;
}

void schedule(void) {
    if (list_empty(&runqueue)) return;
    
    struct task_struct *prev = current_task;
    struct list_head *next_list = current_task->list.next;
    if (next_list == &runqueue) next_list = next_list->next; 
    struct task_struct *next = list_entry(next_list, struct task_struct, list);
    
    if (prev == next) return;
    
    current_task = next;
    switch_to(prev, next);
}
