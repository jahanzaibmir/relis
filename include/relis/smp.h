#pragma once
#include <stdint.h>
#include "relis/sched.h"

#define MAX_CPUS 4

struct cpu_info {
    uint8_t cpu_id;
    uint8_t apic_id;
    uint32_t flags;
    struct task_struct *current_task;
    struct task_struct *idle_task;
};

extern struct cpu_info cpus[MAX_CPUS];
extern int num_cpus;

void smp_init(void);
struct cpu_info* get_cpu(uint8_t cpu_id);
struct cpu_info* get_current_cpu(void);

void ap_trampoline(void);
