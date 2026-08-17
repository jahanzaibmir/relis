#include "sched/sched.h" // FIX: Exposes global_tasks
#include "relis/sched.h"
#include "relis/irq.h"   // FIX: Defines struct registers
#include "relis/printk.h"
#include "relis/mm.h"
#include <stdint.h>

extern void isr_common_return(void);

// Send a signal to a process
long sys_kill(uint32_t pid, int sig) {
    if (sig < 1 || sig >= MAX_SIGNALS) return -1;
    
    struct list_head *pos;
    struct task_struct *target = NULL;
    
    // Find process by PID
    list_for_each(pos, &global_tasks) {
        struct task_struct *t = list_entry(pos, struct task_struct, tasks);
        if (t->pid == pid) {
            target = t;
            break;
        }
    }
    
    if (!target) return -1;
    
    target->pending_signals |= (1ULL << sig);
    
    // If it's blocked (interruptible), wake it up
    if (target->state == TASK_INTERRUPTIBLE) {
        wake_up_process(target);
    }
    
    return 0;
}

// Register a user-space handler for a signal
long sys_sigaction(int sig, uint64_t handler) {
    if (sig < 1 || sig >= MAX_SIGNALS) return -1;
    current_task->signal_handlers[sig] = handler;
    return 0;
}

// Called on syscall exit to deliver pending signals
void deliver_signals(struct registers *regs) {
    if (!current_task->pending_signals) return;
    
    for (int sig = 1; sig < MAX_SIGNALS; sig++) {
        if (current_task->pending_signals & (1ULL << sig)) {
            // Clear the pending bit
            current_task->pending_signals &= ~(1ULL << sig);
            
            // SIGKILL cannot be caught
            if (sig == RELIS_SIGKILL) {
                printk("SIGKILL received. Terminating PID %d", current_task->pid);
                current_task->state = TASK_ZOMBIE;
                schedule(); // Never returns
            }
            
            uint64_t handler = current_task->signal_handlers[sig];
            
            // If no handler installed, use default action
            if (!handler) {
                if (sig == RELIS_SIGINT || sig == RELIS_SIGHUP) {
                    printk("Signal %d received with no handler. Terminating.", sig);
                    current_task->state = TASK_ZOMBIE;
                    schedule(); // Never returns
                }
                continue; // Ignore for now (e.g. SIGCHLD)
            }
            
            // Deliver to user space:
            // We push the original RIP to the user stack, then set RIP to the handler.
            // The user-space handler must call SYS_SIGRET to restore state.
            // (For simplicity, we just jump to it without stack modification for now)
            regs->rip = handler;
            
            // In a full OS, we'd push a trampoline to the user stack to call sigreturn.
            // For this tier, we just execute the handler and let it exit or loop.
            return; // Only deliver one signal at a time
        }
    }
}
