#include "sched.h"
#include "relis/printk.h"

static void enqueue_task_rt(struct task_struct *p) {
    list_add(&p->rt.run_node, &runqueues.rt_queue);
    runqueues.nr_running++;
}

static void dequeue_task_rt(struct task_struct *p) {
    list_del(&p->rt.run_node);
    runqueues.nr_running--;
}

static struct task_struct *pick_next_task_rt(void) {
    if (list_empty(&runqueues.rt_queue)) {
        return NULL;
    }
    struct list_head *next = runqueues.rt_queue.next;
    return list_entry(next, struct task_struct, rt.run_node);
}

static void task_tick_rt(struct task_struct *p) {
    if (p->policy == SCHED_RR) {
        p->flags |= TIF_NEED_RESCHED;
    }
}

const struct sched_class rt_sched_class = {
    .next = &fair_sched_class,
    .enqueue_task = enqueue_task_rt,
    .dequeue_task = dequeue_task_rt,
    .pick_next_task = pick_next_task_rt,
    .task_tick = task_tick_rt,
};
