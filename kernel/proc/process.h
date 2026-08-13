/* =============================================================================
   RELIS — kernel/proc/process.h
   Process Control Block (PCB) and process manager API.

   Each process has:
     • A unique PID
     • A saved CPU register state (for context switching)
     • Its own page directory (virtual address space)
     • A kernel stack (used during syscalls and interrupts)
     • A user stack
     • State: RUNNING, READY, BLOCKED, ZOMBIE, SLEEPING
   ============================================================================= */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "mm/paging.h"

/* ── Process states ──────────────────────────────────────────────────────── */
typedef enum {
    PROC_RUNNING  = 0,   /* currently on the CPU                 */
    PROC_READY    = 1,   /* in the run queue, waiting for CPU    */
    PROC_BLOCKED  = 2,   /* waiting for I/O or an event          */
    PROC_SLEEPING = 3,   /* timer-blocked (timer_sleep)          */
    PROC_ZOMBIE   = 4,   /* exited, waiting for parent wait()    */
} proc_state_t;

/* ── Saved CPU registers — must match the layout pushed by switch_context ── */
typedef struct {
    uint32_t edi, esi, ebp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t eip;          /* instruction pointer to resume at    */
    uint32_t eflags;       /* must be restored with popf          */
    uint32_t esp;          /* kernel stack pointer                */
} cpu_state_t;

/* ── Process Control Block ───────────────────────────────────────────────── */
#define PROC_NAME_LEN   32
#define KERNEL_STACK_SZ 8192    /* 8 KiB kernel stack per process     */
#define USER_STACK_TOP  0xBFFFF000u

typedef struct process {
    uint32_t         pid;
    char             name[PROC_NAME_LEN];
    proc_state_t     state;
    cpu_state_t      cpu;             /* saved registers on context switch  */
    page_directory_t *page_dir;       /* this process's address space       */
    uint8_t          *kstack;         /* kernel stack base (kmalloc'd)      */
    uint32_t         kstack_top;      /* top of kernel stack (initial ESP)  */
    uint32_t         wake_tick;       /* for PROC_SLEEPING: wake-up tick    */
    int              exit_code;       /* set on exit()                      */
    struct process   *parent;         /* parent process pointer             */
    struct process   *next;           /* intrusive linked list for queues   */
} process_t;

/* ── Scheduler / process manager API ────────────────────────────────────── */

/* Initialise the process subsystem and create the idle process (PID 0) */
void proc_init(void);

/* Create a new kernel-space process running func() */
process_t *proc_create_kernel(const char *name, void (*func)(void));

/* Yield the CPU — invoke the round-robin scheduler */
void proc_yield(void);

/* Block the current process until a condition is cleared externally */
void proc_block(void);

/* Unblock a specific process (move it back to READY) */
void proc_unblock(process_t *p);

/* Sleep for ms milliseconds (uses PIT tick count) */
void proc_sleep(uint32_t ms);

/* Terminate the current process */
void proc_exit(int code);

/* Return current process PCB pointer */
process_t *proc_current(void);

/* Return process by PID, or NULL */
process_t *proc_get(uint32_t pid);

/* Dump process list to terminal */
void proc_list(void);

/* Called by the timer IRQ — may trigger a reschedule */
void proc_tick(void);
