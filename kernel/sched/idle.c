#include "sched.h"

static struct task_struct *pick_next_task_idle(void) {
    return current_task; 
}

const struct sched_class idle_sched_class = {
    .next = NULL,
    .enqueue_task = NULL,
    .dequeue_task = NULL,
    .pick_next_task = pick_next_task_idle,
    .task_tick = NULL,
};
