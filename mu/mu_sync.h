#pragma once

#include "mu_common.h"

/* ---------------------------------------------------------------------------
 * mu_sync — atomics, spinlock, rwlock, seqlock.
 *
 * WHY: mu_thread_system / mu_task_scheduler own threads, but there was no
 * home for the primitives those threads synchronise with (the scattered
 * mu_ts_atomic* types). One header, one naming scheme, one set of memory
 * orderings.
 *
 * MEMORY ORDERING RULE (this is the whole game):
 *   ACQUIRE on load  => no memory access inside the critical section may be
 *                       hoisted ABOVE the load.
 *   RELEASE on store  => no memory access inside the critical section may be
 *                       sunk BELOW the store.
 *   Pair them and the critical section is fenced against the rest of the
 *   world without a full barrier on every operation (which is what a mutex
 *   costs you). x86 gives you this for free; ARM does not, hence explicit
 *   orders rather than relaxed.
 *
 * CHOICES:
 *   spinlock   — uncontended lock/unlock is one atomic RMW; correct for
 *                short critical sections. Always pairs its wait with a CPU
 *                pause so a spinning thread does not stall the pipeline or
 *                starve the core it shares with the holder.
 *   rwlock     — many readers, exclusive writer. Optimistic reader count, so
 *                an uncontended read is one atomic add + one sub.
 *   seqlock    — for lock-free READ of plain (non-atomic) data: the writer
 *                bumps an odd counter around the update, readers retry if
 *                they saw an odd or changed counter. Reads scale to any
 *                number of cores and cost zero writes.
 *
 * CONTRACT: not recursive (a lock you already hold will deadlock you).
 * --------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- *
 * Portability shims
 * -------------------------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)
#  define MU_SYNC_GNU_ATOMICS 1
#elif defined(_MSC_VER)
#  define MU_SYNC_MSVC_ATOMICS 1
#  include <intrin.h>
#else
#  define MU_SYNC_NO_ATOMICS 1
#endif

#if defined(__GNUC__) || defined(__clang__)
#  if defined(__i386__) || defined(__x86_64__)
#    define MU_CPU_RELAX() __builtin_ia32_pause()
#  elif defined(__aarch64__) || defined(__arm__)
#    define MU_CPU_RELAX() __asm__ __volatile__("yield" ::: "memory")
#  else
#    define MU_CPU_RELAX() ((void)0)
#  endif
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#  define MU_CPU_RELAX() _mm_pause()
#else
#  define MU_CPU_RELAX() ((void)0)
#endif

/* Plain 32-bit value that participates in the memory model. */
typedef struct mu_atomic_u32
{
    volatile uint32_t v;
} mu_atomic_u32;

typedef struct mu_atomic_ptr
{
    void* volatile v; /* the POINTER is atomic, not the pointee */
} mu_atomic_ptr;

MU_INLINE void mu_atomic_store_u32(mu_atomic_u32* a, uint32_t value)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    __atomic_store_n(&a->v, value, __ATOMIC_RELEASE);
#elif defined(MU_SYNC_MSVC_ATOMICS)
    _InterlockedExchange((volatile long*)&a->v, (long)value);
#else
    a->v = value;
#endif
}

MU_INLINE uint32_t mu_atomic_load_u32(const mu_atomic_u32* a)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    return __atomic_load_n(&a->v, __ATOMIC_ACQUIRE);
#elif defined(MU_SYNC_MSVC_ATOMICS)
    return (uint32_t)_ReadWriteBarrier(), (uint32_t)a->v;
#else
    return a->v;
#endif
}

/* Compare-and-swap. Returns true if the stored value was `expected`. */
MU_INLINE bool mu_atomic_cas_u32(mu_atomic_u32* a, uint32_t* expected, uint32_t desired)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    return __atomic_compare_exchange_n(&a->v, expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#elif defined(MU_SYNC_MSVC_ATOMICS)
    long prev = _InterlockedCompareExchange((volatile long*)&a->v, (long)desired, (long)*expected);
    if ((uint32_t)prev == *expected)
        return true;
    *expected = (uint32_t)prev;
    return false;
#else
    if (a->v == *expected)
    {
        a->v = desired;
        return true;
    }
    *expected = a->v;
    return false;
#endif
}

/* Atomic read-modify-write: returns the PREVIOUS value. */
MU_INLINE uint32_t mu_atomic_fetch_add_u32(mu_atomic_u32* a, uint32_t value)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    return __atomic_fetch_add(&a->v, value, __ATOMIC_ACQ_REL);
#elif defined(MU_SYNC_MSVC_ATOMICS)
    return (uint32_t)_InterlockedExchangeAdd((volatile long*)&a->v, (long)value);
#else
    uint32_t old = a->v;
    a->v = old + value;
    return old;
#endif
}

MU_INLINE uint32_t mu_atomic_exchange_u32(mu_atomic_u32* a, uint32_t value)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    return __atomic_exchange_n(&a->v, value, __ATOMIC_ACQ_REL);
#elif defined(MU_SYNC_MSVC_ATOMICS)
    return (uint32_t)_InterlockedExchange((volatile long*)&a->v, (long)value);
#else
    uint32_t old = a->v;
    a->v = value;
    return old;
#endif
}

MU_INLINE void mu_atomic_store_ptr(mu_atomic_ptr* a, void* value)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    __atomic_store_n(&a->v, value, __ATOMIC_RELEASE);
#else
    a->v = value;
#endif
}

MU_INLINE void* mu_atomic_load_ptr(const mu_atomic_ptr* a)
{
#if defined(MU_SYNC_GNU_ATOMICS)
    return __atomic_load_n(&a->v, __ATOMIC_ACQUIRE);
#else
    return a->v;
#endif
}

MU_INLINE void mu_atomic_init_u32(mu_atomic_u32* a, uint32_t value)
{
    a->v = value;
}

/* -------------------------------------------------------------------------- *
 * Spinlock — one word, pause-spinning, no syscalls.
 * -------------------------------------------------------------------------- */

typedef struct mu_spinlock
{
    mu_atomic_u32 state; /* 0 = free, 1 = held */
} mu_spinlock;

MU_INLINE void mu_spinlock_init(mu_spinlock* s)
{
    mu_atomic_init_u32(&s->state, 0);
}

MU_INLINE void mu_spinlock_lock(mu_spinlock* s)
{
    for (;;)
    {
        /* Fast path: try to grab the free lock with a single RMW. */
        uint32_t expected = 0;
        if (mu_atomic_cas_u32(&s->state, &expected, 1))
            return;

        /* Contended: read-spin instead of hammering the cache line with
           CAS. The line is in nobody's exclusive state while we spin, so
           this costs far less coherence traffic. */
        while (mu_atomic_load_u32(&s->state) != 0)
            MU_CPU_RELAX();
    }
}

MU_INLINE bool mu_spinlock_try_lock(mu_spinlock* s)
{
    uint32_t expected = 0;
    return mu_atomic_cas_u32(&s->state, &expected, 1);
}

MU_INLINE void mu_spinlock_unlock(mu_spinlock* s)
{
    mu_atomic_store_u32(&s->state, 0);
}

/* -------------------------------------------------------------------------- *
 * Reader/writer lock — shared reads, exclusive writes.
 *
 * Layout: low 31 bits = active readers, high bit = writer pending/held.
 * A writer sets the flag, then waits for readers to drain; readers that see
 * the flag wait for it to clear. Readers never block each other.
 * -------------------------------------------------------------------------- */

#define MU_RWLOCK_WRITER 0x80000000u

typedef struct mu_rwlock
{
    mu_atomic_u32 state;
} mu_rwlock;

MU_INLINE void mu_rwlock_init(mu_rwlock* l)
{
    mu_atomic_init_u32(&l->state, 0);
}

MU_INLINE void mu_rwlock_lock_read(mu_rwlock* l)
{
    for (;;)
    {
        uint32_t s = mu_atomic_load_u32(&l->state);
        while (s & MU_RWLOCK_WRITER)
        {
            MU_CPU_RELAX();
            s = mu_atomic_load_u32(&l->state);
        }
        uint32_t expected = s;
        if (mu_atomic_cas_u32(&l->state, &expected, s + 1u))
            return;
    }
}

MU_INLINE void mu_rwlock_unlock_read(mu_rwlock* l)
{
    mu_atomic_fetch_add_u32(&l->state, (uint32_t)-1);
}

MU_INLINE void mu_rwlock_lock_write(mu_rwlock* l)
{
    for (;;)
    {
        uint32_t expected = 0;
        if (mu_atomic_cas_u32(&l->state, &expected, MU_RWLOCK_WRITER))
            break;
        MU_CPU_RELAX();
    }

    /* Now wait for in-flight readers to leave. */
    while ((mu_atomic_load_u32(&l->state) & ~MU_RWLOCK_WRITER) != 0)
        MU_CPU_RELAX();
}

MU_INLINE void mu_rwlock_unlock_write(mu_rwlock* l)
{
    mu_atomic_store_u32(&l->state, 0);
}

MU_INLINE bool mu_rwlock_try_lock_write(mu_rwlock* l)
{
    uint32_t expected = 0;
    return mu_atomic_cas_u32(&l->state, &expected, MU_RWLOCK_WRITER);
}

/* -------------------------------------------------------------------------- *
 * Seqlock — lock-free reads of plain data.
 *
 * The writer brackets its update with seq = 2k -> 2k+1 -> 2k+2.
 * Readers snapshot seq, read the data WITHOUT locking, then re-check seq:
 * if it changed (or is odd) the read overlapped a write and they retry.
 *
 * Best for a handful of scalars published at ~frame rate and read by many
 * threads: zero writes on the read side, so it scales perfectly, unlike a
 * rwlock where every reader does a CAS on one shared line.
 * -------------------------------------------------------------------------- */

typedef struct mu_seqlock
{
    mu_atomic_u32 seq;
} mu_seqlock;

MU_INLINE void mu_seqlock_init(mu_seqlock* l)
{
    mu_atomic_init_u32(&l->seq, 0);
}

/* Writer side. Bracket the update: */
MU_INLINE void mu_seqlock_write_begin(mu_seqlock* l)
{
    uint32_t s = mu_atomic_load_u32(&l->seq);
    mu_atomic_store_u32(&l->seq, s + 1u); /* becomes odd: readers spin/retry */
}

MU_INLINE void mu_seqlock_write_end(mu_seqlock* l)
{
    uint32_t s = mu_atomic_load_u32(&l->seq);
    mu_atomic_store_u32(&l->seq, s + 1u); /* even again: update visible */
}

MU_INLINE uint32_t mu_seqlock_read_begin(const mu_seqlock* l)
{
    uint32_t s = mu_atomic_load_u32(&l->seq);
    while (s & 1u)
    {
        MU_CPU_RELAX();
        s = mu_atomic_load_u32(&l->seq);
    }
    return s;
}

/* Returns false if a write overlapped the read (retry then). */
MU_INLINE bool mu_seqlock_read_end(const mu_seqlock* l, uint32_t snapshot)
{
    return mu_atomic_load_u32(&l->seq) == snapshot;
}
