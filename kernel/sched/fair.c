#include "sched.h"
#include "relis/printk.h"

static void enqueue_task_fair(struct task_struct *p) {
    // FIX: Add to tail so tasks run in the order they were created (FIFO)
    list_add_tail(&p->se.run_node, &runqueues.cfs_queue);
    runqueues.nr_running++;
}

static void dequeue_task_fair(struct task_struct *p) {
    list_del(&p->se.run_node);
    runqueues.nr_running--;
}

static struct task_struct *pick_next_task_fair(void) {
    if (list_empty(&runqueues.cfs_queue)) {
        return NULL;
    }
    struct list_head *next = runqueues.cfs_queue.next;
    
    // Rotate the queue: move the picked task to the back so others get a turn.
    list_del(next);
    list_add_tail(next, &runqueues.cfs_queue);
    
    return list_entry(next, struct task_struct, se.run_node);
}

static void task_tick_fair(struct task_struct *p) {
    p->se.sum_exec_runtime++;
    p->se.vruntime += (1024 / p->se.weight);
    p->flags |= TIF_NEED_RESCHED;
}

const struct sched_class fair_sched_class = {
    .next = &idle_sched_class,
    .enqueue_task = enqueue_task_fair,
    .dequeue_task = dequeue_task_fair,
    .pick_next_task = pick_next_task_fair,
    .task_tick = task_tick_fair,
};
