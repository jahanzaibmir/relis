#pragma once
#include "relis/list.h"
#include <stdint.h>

#define TASK_RUNNING   0
#define TASK_INTERRUPTIBLE 1
#define TASK_ZOMBIE    2
#define KERNEL_STACK_SIZE 16384

struct task_struct {
    uint64_t *rsp;       // MUST BE FIRST FIELD FOR SWITCH.ASM
    uint32_t pid;
    char name[32];
    uint32_t state;
    uint32_t flags;
    
    uint64_t stack[KERNEL_STACK_SIZE / 8]; 
    
    void (*fn)(void); 
    void *arg;
    
    struct list_head list;
};

extern struct task_struct *current_task;

void sched_init(void);
void schedule(void);
void switch_to(struct task_struct *prev, struct task_struct *next);
int kernel_thread(const char *name, void (*fn)(void), void *arg);
