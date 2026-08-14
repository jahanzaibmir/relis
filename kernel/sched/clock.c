#include "sched.h"
#include <stdint.h>

extern volatile uint64_t jiffies;

uint64_t sched_clock(void) {
    return jiffies;
}

uint64_t rq_clock(struct rq *rq) {
    return rq->clock;
}

void update_rq_clock(struct rq *rq) {
    rq->clock = sched_clock();
}
