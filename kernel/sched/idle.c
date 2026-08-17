#include "sched.h"

static struct task_struct *pick_next_task_idle(void) {
    return current_task; 
}

// FIX: The idle task MUST yield on every tick so other tasks can run!
static void task_tick_idle(struct task_struct *p) {
    p->flags |= TIF_NEED_RESCHED;
}

const struct sched_class idle_sched_class = {
    .next = NULL,
    .enqueue_task = NULL,
    .dequeue_task = NULL,
    .pick_next_task = pick_next_task_idle,
    .task_tick = task_tick_idle,
};
