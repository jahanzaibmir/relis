#include "relis/sched.h"
#include "relis/mm.h"
#include "relis/string.h"
#include "relis/printk.h"

LIST_HEAD(runqueue);
spinlock_t rq_lock = SPIN_LOCK_UNLOCKED;
struct task_struct *current_task = 0;
static uint32_t next_pid = 1;

void sched_init(void) {
    printk("Scheduler initialized");
    current_task = kmalloc(sizeof(struct task_struct));
    if (!current_task) {
        printk("PANIC: Failed to allocate initial task!");
        for (;;) __asm__ volatile("cli; hlt");
    }
    kmemset(current_task, 0, sizeof(struct task_struct));
    current_task->pid = 0;
    kstrcpy(current_task->name, "idle");
    current_task->state = TASK_RUNNING;
    INIT_LIST_HEAD(&current_task->list);
    list_add(&current_task->list, &runqueue);
}

int kernel_thread(const char *name, void (*fn)(void), void *arg) {
    spin_lock(&rq_lock);
    struct task_struct *new_task = kmalloc(sizeof(struct task_struct));
    if (!new_task) {
        spin_unlock(&rq_lock);
        return -1;
    }
    kmemset(new_task, 0, sizeof(struct task_struct));
    new_task->pid = next_pid++;
    kstrcpy(new_task->name, name);
    new_task->state = TASK_RUNNING;
    
    uint32_t *stack = new_task->stack + (KERNEL_STACK_SIZE / 4);
    *--stack = (uint32_t)arg;       
    *--stack = (uint32_t)fn;        
    *--stack = 0;                   
    *--stack = 0;                   
    *--stack = 0;                   
    *--stack = 0;                   
    
    new_task->esp = stack;
    list_add(&new_task->list, &runqueue);
    spin_unlock(&rq_lock);
    return new_task->pid;
}

void schedule(void) {
    spin_lock(&rq_lock);
    if (list_empty(&runqueue)) {
        spin_unlock(&rq_lock);
        return;
    }
    
    struct task_struct *prev = current_task;
    struct list_head *next_list = current_task->list.next;
    if (next_list == &runqueue) next_list = next_list->next; 
    struct task_struct *next = list_entry(next_list, struct task_struct, list);
    
    if (prev == next) {
        spin_unlock(&rq_lock);
        return;
    }
    
    current_task = next;
    spin_unlock(&rq_lock);
    switch_to(prev, next);
}