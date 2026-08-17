#pragma once
#include "relis/list.h"
#include <stdint.h>

#define TASK_RUNNING   0
#define TASK_INTERRUPTIBLE 1
#define TASK_ZOMBIE    2
#define KERNEL_STACK_SIZE 16384
#define MAX_FDS 32

#define RELIS_SIGHUP    1
#define RELIS_SIGINT    2
#define RELIS_SIGKILL   9
#define RELIS_SIGCHLD   17
#define MAX_SIGNALS 32

#define MAX_SHM_REGIONS 4

#define SCHED_NORMAL 0
#define SCHED_FIFO   1
#define SCHED_RR     2

#define MAX_RT_PRIO 100
#define MAX_PRIO (MAX_RT_PRIO + 40)
#define DEFAULT_PRIO 120

#define TIF_NEED_RESCHED 1

struct task_struct;
struct file;

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
    uint64_t *rsp;
    uint32_t pid;
    uint32_t ppid;
    char name[32];
    uint32_t state;
    uint32_t flags;

    int policy;
    int prio;
    uint64_t cr3;

    // FIX: Virtual Memory Areas for Demand Paging
    uint64_t stack_start;
    uint64_t stack_end;

    union {
        struct sched_entity se;
        struct sched_rt_entity rt;
    };

    const struct sched_class *sched_class;
    struct list_head tasks;

    uint64_t kernel_stack[KERNEL_STACK_SIZE / 8];
    uint64_t *kernel_stack_top;

    struct file *files[MAX_FDS];

    uint64_t pending_signals;
    uint64_t signal_handlers[MAX_SIGNALS];

    uint64_t shm_pages[MAX_SHM_REGIONS];
    int shm_count;

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
