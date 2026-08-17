#pragma once
#include <stdint.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPIN_LOCK_UNLOCKED { 0 }

static inline void spin_lock(spinlock_t *lock) {
    while (__atomic_exchange_n(&lock->lock, 1, __ATOMIC_ACQUIRE) != 0) {
        // Spin-wait optimization
        while (lock->lock != 0) {
            __asm__ volatile("pause");
        }
    }
}

static inline void spin_unlock(spinlock_t *lock) {
    __atomic_store_n(&lock->lock, 0, __ATOMIC_RELEASE);
}