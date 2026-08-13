#pragma once
#include "relis/list.h"
#include "relis/spinlock.h"
#include <stdint.h>

#define TASK_RUNNING   0
#define KERNEL_STACK_SIZE 8192

struct task_struct {
    uint32_t pid;
    char name[32];
    uint32_t state;
    uint32_t *esp;
    uint32_t stack[KERNEL_STACK_SIZE / 4];
    struct list_head list;
};

extern struct task_struct *current_task;

void sched_init(void);
void schedule(void);
void switch_to(struct task_struct *prev, struct task_struct *next);
int kernel_thread(const char *name, void (*fn)(void), void *arg);