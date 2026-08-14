#include "sched.h"
#include "relis/mm.h"
#include "relis/string.h"

void init_waitqueue_head(struct list_head *q) {
    INIT_LIST_HEAD(q);
}

void wake_up(struct list_head *q) {
    // Stub for waitqueue waking
    (void)q;
}
