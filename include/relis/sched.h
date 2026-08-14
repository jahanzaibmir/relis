#pragma once
#include "relis/list.h"
#include <stdint.h>

#define TASK_RUNNING   0
#define TASK_INTERRUPTIBLE 1
#define TASK_ZOMBIE    2
#define KERNEL_STACK_SIZE 16384

#define SCHED_NORMAL 0
#define SCHED_FIFO   1
#define SCHED_RR     2

#define MAX_RT_PRIO 100
#define MAX_PRIO (MAX_RT_PRIO + 40)
#define DEFAULT_PRIO 120

#define TIF_NEED_RESCHED 1

struct task_struct;

struct sched_class {
    const struct sched_class *next;
    void (*enqueue_task)(struct task_struct *p);
    void (*dequeue_task)(struct task_struct *p);
    struct task_struct *(*pick_next_task)(void);
    void (*task_tick)(struct task_struct *p);
};

struct sched_entity {
    uint64_t exec_start;
    uint64_t sum_exec_runtime;
    uint64_t vruntime;
    uint32_t weight;
    struct list_head run_node;
};

struct sched_rt_entity {
    int prio;
    struct list_head run_node;
};

struct task_struct {
    uint64_t *rsp;       // MUST BE FIRST FIELD FOR SWITCH.ASM
    uint32_t pid;
    char name[32];
    uint32_t state;
    uint32_t flags;      // TIF_NEED_RESCHED lives here
    
    int policy;
    int prio;
    
    union {
        struct sched_entity se;
        struct sched_rt_entity rt;
    };
    
    const struct sched_class *sched_class;
    struct list_head tasks;
    
    uint64_t stack[KERNEL_STACK_SIZE / 8];
    
    void (*fn)(void);
    void *arg;
};

extern struct task_struct *current_task;

void sched_init(void);
void schedule(void);
void scheduler_tick(void);
void switch_to(struct task_struct *prev, struct task_struct *next);
int kernel_thread(const char *name, void (*fn)(void), void *arg, int policy);
void wake_up_process(struct task_struct *p);
