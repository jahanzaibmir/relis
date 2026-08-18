#pragma once
#include "relis/sched.h"
#include "relis/list.h"
#include <stdint.h>

struct rq {
    struct task_struct *curr;
    struct list_head cfs_queue;
    struct list_head rt_queue;
    uint64_t nr_running;
    uint64_t clock;
};

extern struct rq runqueues;
extern const struct sched_class fair_sched_class;
extern const struct sched_class rt_sched_class;
extern const struct sched_class idle_sched_class;

// : Expose these so fork.c can use them
extern struct list_head global_tasks;
extern uint32_t next_pid;

void __schedule(void);
uint64_t sched_clock(void);
