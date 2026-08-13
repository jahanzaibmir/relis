/* =============================================================================
   RELIS — kernel/proc/process.c
   Process manager: creation, scheduling (round-robin), context switching,
   sleep, block/unblock, exit, and process list.
   ============================================================================= */
#include "process.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "drivers/timer.h"
#include "drivers/vga.h"
#include "kprintf.h"
#include <stddef.h>

/* ── Globals ─────────────────────────────────────────────────────────────── */
#define MAX_PROCS 64

static process_t  proc_table[MAX_PROCS];
static uint32_t   next_pid    = 0;
static process_t *current     = NULL;   /* currently running process     */
static process_t *run_queue   = NULL;   /* head of the READY queue       */

extern void switch_context(cpu_state_t *old_cpu, cpu_state_t *new_cpu);

/* ── Internal helpers ────────────────────────────────────────────────────── */

/* Copy a string into a fixed buffer */
static void kstrcpy_n(char *dst, const char *src, size_t max) {
    size_t i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* Append p to the tail of the run queue */
static void enqueue(process_t *p) {
    p->next = NULL;
    if (!run_queue) { run_queue = p; return; }
    process_t *t = run_queue;
    while (t->next) t = t->next;
    t->next = p;
}

/* Remove and return the head of the run queue */
static process_t *dequeue(void) {
    if (!run_queue) return NULL;
    process_t *p = run_queue;
    run_queue = run_queue->next;
    p->next = NULL;
    return p;
}

/* Find a free slot in the process table */
static process_t *alloc_proc(void) {
    for (int i = 0; i < MAX_PROCS; i++)
        if (proc_table[i].state == PROC_ZOMBIE && proc_table[i].pid == 0)
            return &proc_table[i];
    return NULL;
}

/* ── Idle process — runs when nothing else is ready ─────────────────────── */
static void idle_task(void) {
    while (1) __asm__ volatile ("hlt");
}

/* ── Initialise process subsystem ────────────────────────────────────────── */
void proc_init(void) {
    /* Zero the whole table; pid==0 && state==ZOMBIE marks a free slot */
    for (int i = 0; i < MAX_PROCS; i++) {
        proc_table[i].pid   = 0;
        proc_table[i].state = PROC_ZOMBIE;
        proc_table[i].next  = NULL;
    }

    /* Create the idle process (PID 0) — runs when run queue is empty */
    process_t *idle  = &proc_table[0];
    idle->pid        = next_pid++;
    idle->state      = PROC_READY;
    idle->page_dir   = paging_kernel_directory();
    idle->parent     = NULL;
    kstrcpy_n(idle->name, "idle", PROC_NAME_LEN);

    /* Allocate idle kernel stack */
    idle->kstack     = (uint8_t *)kmalloc(KERNEL_STACK_SZ);
    idle->kstack_top = (uint32_t)(uintptr_t)(idle->kstack + KERNEL_STACK_SZ);

    /* Set up cpu state so idle_task() runs on first switch */
    idle->cpu.eip    = (uint32_t)(uintptr_t)idle_task;
    idle->cpu.esp    = idle->kstack_top - 4;
    idle->cpu.eflags = 0x202;   /* IF=1, reserved bit 1 */

    /* Idle starts as "current" so the first proc_yield has somewhere to save */
    current = idle;

    kprintf("[proc] Process manager ready — idle PID 0\n");
}

/* ── Create a kernel-mode process ────────────────────────────────────────── */
process_t *proc_create_kernel(const char *name, void (*func)(void)) {
    process_t *p = alloc_proc();
    if (!p) { kprintf("[proc] ERROR: process table full\n"); return NULL; }

    p->pid      = next_pid++;
    p->state    = PROC_READY;
    p->parent   = current;
    p->exit_code = 0;
    kstrcpy_n(p->name, name, PROC_NAME_LEN);

    /* Each kernel process shares the kernel page directory */
    p->page_dir = paging_kernel_directory();

    /* Allocate kernel stack */
    p->kstack     = (uint8_t *)kmalloc(KERNEL_STACK_SZ);
    p->kstack_top = (uint32_t)(uintptr_t)(p->kstack + KERNEL_STACK_SZ);

    /* Pre-load a stack frame so switch_context jumps to func() on first run:
       Lay out stack (growing down):
         [kstack_top - 4]  = 0          (fake return addr — proc_exit if func returns)
         [kstack_top - 8]  = eip = func  (switch_context does: mov esp,[]; push eip; ret)
       switch_context restores esp = kstack_top - 4, then push eip makes it kstack_top-8,
       then ret pops func. */
    uint32_t *sp = (uint32_t *)(uintptr_t)p->kstack_top;
    *(--sp) = (uint32_t)(uintptr_t)proc_exit;  /* return address if func() returns */
    *(--sp) = 0;                                /* dummy arg to proc_exit           */

    p->cpu.eip    = (uint32_t)(uintptr_t)func;
    p->cpu.esp    = (uint32_t)(uintptr_t)sp;
    p->cpu.eflags = 0x202;

    enqueue(p);
    kprintf("[proc] Created kernel process '%s' PID %u\n", name, p->pid);
    return p;
}

/* ── Internal schedule — pick next READY process and switch ──────────────── */
static void schedule(void) {
    /* Wake any sleeping processes whose timer has expired */
    uint64_t now = timer_ticks();
    for (int i = 0; i < MAX_PROCS; i++) {
        process_t *p = &proc_table[i];
        if (p->state == PROC_SLEEPING && p->pid != 0 && now >= p->wake_tick) {
            p->state = PROC_READY;
            enqueue(p);
        }
    }

    process_t *prev = current;
    process_t *next = dequeue();

    /* If no one is ready, fall back to idle (proc_table[0]) */
    if (!next) next = &proc_table[0];

    if (next == prev) return;   /* nothing to switch to */

    /* Put prev back on run queue if it's still runnable */
    if (prev->state == PROC_RUNNING) {
        prev->state = PROC_READY;
        enqueue(prev);
    }

    next->state = PROC_RUNNING;
    current     = next;

    paging_switch_directory(next->page_dir);
    switch_context(&prev->cpu, &next->cpu);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void proc_yield(void) {
    schedule();
}

void proc_block(void) {
    current->state = PROC_BLOCKED;
    schedule();
}

void proc_unblock(process_t *p) {
    if (p && p->state == PROC_BLOCKED) {
        p->state = PROC_READY;
        enqueue(p);
    }
}

void proc_sleep(uint32_t ms) {
    current->wake_tick = timer_ticks() + (uint64_t)(ms / 10);  /* 100 Hz → 10ms/tick */
    current->state     = PROC_SLEEPING;
    schedule();
}

void proc_exit(int code) {
    current->exit_code = code;
    current->state     = PROC_ZOMBIE;
    kprintf("[proc] PID %u '%s' exited with code %d\n",
            current->pid, current->name, code);
    schedule();
    /* If schedule() ever returns here, spin */
    for (;;) __asm__ volatile ("hlt");
}

process_t *proc_current(void) { return current; }

process_t *proc_get(uint32_t pid) {
    for (int i = 0; i < MAX_PROCS; i++)
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_ZOMBIE)
            return &proc_table[i];
    return NULL;
}

/* Called by timer IRQ every tick — preempt after ~20ms (2 ticks at 100Hz) */
static uint32_t tick_counter = 0;
void proc_tick(void) {
    if (++tick_counter >= 2) {
        tick_counter = 0;
        schedule();
    }
}

/* ── Process list dump ────────────────────────────────────────────────────── */
static const char *state_name(proc_state_t s) {
    switch (s) {
        case PROC_RUNNING:  return "RUNNING";
        case PROC_READY:    return "READY  ";
        case PROC_BLOCKED:  return "BLOCKED";
        case PROC_SLEEPING: return "SLEEP  ";
        case PROC_ZOMBIE:   return "ZOMBIE ";
        default:            return "UNKNOWN";
    }
}

void proc_list(void) {
    terminal_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_writeline("PID  NAME             STATE    KSTACK");
    terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    for (int i = 0; i < MAX_PROCS; i++) {
        process_t *p = &proc_table[i];
        if (p->pid == 0 && p->state == PROC_ZOMBIE) continue;
        kprintf("%-4u %-16s %s  0x%x\n",
                p->pid, p->name,
                state_name(p->state),
                p->kstack_top);
    }
}
