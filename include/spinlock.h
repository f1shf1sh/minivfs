#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <stdatomic.h>
#include <sched.h>     // for sched_yield()
#include <assert.h>

// ------------------------------
// Spinlock 基础定义
// ------------------------------

typedef struct {
    atomic_flag locked;
#ifdef DEBUG_SPINLOCK
    const char *owner; // 调试信息：记录哪个函数持有锁
#endif
} spinlock_t;

// 初始化
static inline void spin_init(spinlock_t *lk) {
    assert(lk);
    atomic_flag_clear(&lk->locked);
#ifdef DEBUG_SPINLOCK
    lk->owner = NULL;
#endif
}

// 加锁
static inline void spin_lock(spinlock_t *lk) {
    assert(lk);
    while (atomic_flag_test_and_set_explicit(&lk->locked, memory_order_acquire)) {
#if defined(__x86_64__) || defined(__i386__)
        __asm__ volatile("pause");
#endif
        // 防止过度占用 CPU
        sched_yield();
    }
#ifdef DEBUG_SPINLOCK
    lk->owner = __func__;
#endif
}

// 尝试加锁（非阻塞）
static inline int spin_trylock(spinlock_t *lk) {
    assert(lk);
    return !atomic_flag_test_and_set_explicit(&lk->locked, memory_order_acquire);
}

// 解锁
static inline void spin_unlock(spinlock_t *lk) {
    assert(lk);
#ifdef DEBUG_SPINLOCK
    lk->owner = NULL;
#endif
    atomic_flag_clear_explicit(&lk->locked, memory_order_release);
}

// ------------------------------
// 通用接口封装（方便替换原有锁）
// ------------------------------

typedef spinlock_t lock_t;

#define lock_init(lk)      spin_init(lk)
#define lock_acquire(lk)   spin_lock(lk)
#define lock_try(lk)       spin_trylock(lk)
#define lock_release(lk)   spin_unlock(lk)

#endif // _SPINLOCK_H_