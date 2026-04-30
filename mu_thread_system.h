#ifndef MU_THREAD_SYSTEM_H
#define MU_THREAD_SYSTEM_H

/*
    mu_thread_system.h
    ---------------------------------------------------------------------------
    Standalone C99 single-header thread system + atomics helpers.

    This header merges ideas from:
    - The-Forge/Common_3/Utilities/Threading/ThreadSystem.h/.c
    - The-Forge/Common_3/Utilities/Threading/Atomics.h

    Design goals:
    1) Keep the API close to The Forge thread system behavior.
    2) Keep it header-only and dependency-light.
    3) Preserve "dummy mode" when thread_count == 0.

    VISUAL MODEL

      Producers call mu_ts_add_tasks()
                     |
                     v
            +-------------------+
            | FIFO task buffer  |
            +-------------------+
                     |
                     v
         worker 1  worker 2  ... worker N
             |         |              |
             +---- execute TaskFunc --+

    TaskFunc signature:
      void (*fn)(void* user, uint64_t thread_id)

    thread_id:
    - 1..N for worker threads
    - UINT64_MAX when executed by assist()/dummy mode

    ---------------------------------------------------------------------------
*/

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#if defined(__linux__)
#include <sched.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Config                                                                    */
/* ------------------------------------------------------------------------- */

#ifndef MU_TS_INLINE
#define MU_TS_INLINE static inline
#endif

#ifndef MU_TS_ASSERT
#define MU_TS_ASSERT(x) assert(x)
#endif

#ifndef MU_TS_MALLOC
#define MU_TS_MALLOC(sz) malloc(sz)
#endif

#ifndef MU_TS_FREE
#define MU_TS_FREE(p) free(p)
#endif

#ifndef MU_TS_REALLOC
#define MU_TS_REALLOC(p, sz) realloc((p), (sz))
#endif

/* ------------------------------------------------------------------------- */
/* Atomics (relaxed primitives + acquire/release wrappers)                    */
/* ------------------------------------------------------------------------- */

typedef volatile uint32_t  mu_ts_atomic32_t;
typedef volatile uint64_t  mu_ts_atomic64_t;
typedef volatile uintptr_t mu_ts_atomicptr_t;

#if defined(_WIN32)

MU_TS_INLINE void mu_ts_memorybarrier_acquire(void)
{
    _ReadWriteBarrier();
}
MU_TS_INLINE void mu_ts_memorybarrier_release(void)
{
    _ReadWriteBarrier();
}

MU_TS_INLINE uint32_t mu_ts_atomic32_load_relaxed(mu_ts_atomic32_t* p)
{
    return *p;
}
MU_TS_INLINE uint32_t mu_ts_atomic32_store_relaxed(mu_ts_atomic32_t* p, uint32_t v)
{
    return (uint32_t)InterlockedExchange((volatile LONG*)p, (LONG)v);
}
MU_TS_INLINE uint32_t mu_ts_atomic32_add_relaxed(mu_ts_atomic32_t* p, int32_t v)
{
    return (uint32_t)InterlockedExchangeAdd((volatile LONG*)p, (LONG)v);
}
MU_TS_INLINE uint32_t mu_ts_atomic32_cas_relaxed(mu_ts_atomic32_t* p, uint32_t cmp, uint32_t v)
{
    return (uint32_t)InterlockedCompareExchange((volatile LONG*)p, (LONG)v, (LONG)cmp);
}

MU_TS_INLINE uint64_t mu_ts_atomic64_load_relaxed(mu_ts_atomic64_t* p)
{
    return *p;
}
MU_TS_INLINE uint64_t mu_ts_atomic64_store_relaxed(mu_ts_atomic64_t* p, uint64_t v)
{
    return (uint64_t)InterlockedExchange64((volatile LONG64*)p, (LONG64)v);
}
MU_TS_INLINE uint64_t mu_ts_atomic64_add_relaxed(mu_ts_atomic64_t* p, int64_t v)
{
    return (uint64_t)InterlockedExchangeAdd64((volatile LONG64*)p, (LONG64)v);
}
MU_TS_INLINE uint64_t mu_ts_atomic64_cas_relaxed(mu_ts_atomic64_t* p, uint64_t cmp, uint64_t v)
{
    return (uint64_t)InterlockedCompareExchange64((volatile LONG64*)p, (LONG64)v, (LONG64)cmp);
}

#else

MU_TS_INLINE void mu_ts_memorybarrier_acquire(void)
{
    __asm__ __volatile__("" : : : "memory");
}
MU_TS_INLINE void mu_ts_memorybarrier_release(void)
{
    __asm__ __volatile__("" : : : "memory");
}

MU_TS_INLINE uint32_t mu_ts_atomic32_load_relaxed(mu_ts_atomic32_t* p)
{
    return *p;
}
MU_TS_INLINE uint32_t mu_ts_atomic32_store_relaxed(mu_ts_atomic32_t* p, uint32_t v)
{
    return (uint32_t)__sync_lock_test_and_set((volatile int32_t*)p, (int32_t)v);
}
MU_TS_INLINE uint32_t mu_ts_atomic32_add_relaxed(mu_ts_atomic32_t* p, int32_t v)
{
    return (uint32_t)__sync_fetch_and_add((volatile int32_t*)p, v);
}
MU_TS_INLINE uint32_t mu_ts_atomic32_cas_relaxed(mu_ts_atomic32_t* p, uint32_t cmp, uint32_t v)
{
    return (uint32_t)__sync_val_compare_and_swap((volatile int32_t*)p, (int32_t)cmp, (int32_t)v);
}

MU_TS_INLINE uint64_t mu_ts_atomic64_load_relaxed(mu_ts_atomic64_t* p)
{
    return *p;
}
MU_TS_INLINE uint64_t mu_ts_atomic64_store_relaxed(mu_ts_atomic64_t* p, uint64_t v)
{
    return (uint64_t)__sync_lock_test_and_set((volatile int64_t*)p, (int64_t)v);
}
MU_TS_INLINE uint64_t mu_ts_atomic64_add_relaxed(mu_ts_atomic64_t* p, int64_t v)
{
    return (uint64_t)__sync_fetch_and_add((volatile int64_t*)p, v);
}
MU_TS_INLINE uint64_t mu_ts_atomic64_cas_relaxed(mu_ts_atomic64_t* p, uint64_t cmp, uint64_t v)
{
    return (uint64_t)__sync_val_compare_and_swap((volatile int64_t*)p, (int64_t)cmp, (int64_t)v);
}

#endif

MU_TS_INLINE uint32_t mu_ts_atomic32_load_acquire(mu_ts_atomic32_t* p)
{
    uint32_t v = mu_ts_atomic32_load_relaxed(p);
    mu_ts_memorybarrier_acquire();
    return v;
}

MU_TS_INLINE uint32_t mu_ts_atomic32_store_release(mu_ts_atomic32_t* p, uint32_t v)
{
    mu_ts_memorybarrier_release();
    return mu_ts_atomic32_store_relaxed(p, v);
}

MU_TS_INLINE uint64_t mu_ts_atomic64_load_acquire(mu_ts_atomic64_t* p)
{
    uint64_t v = mu_ts_atomic64_load_relaxed(p);
    mu_ts_memorybarrier_acquire();
    return v;
}

MU_TS_INLINE uint64_t mu_ts_atomic64_store_release(mu_ts_atomic64_t* p, uint64_t v)
{
    mu_ts_memorybarrier_release();
    return mu_ts_atomic64_store_relaxed(p, v);
}

MU_TS_INLINE uint32_t mu_ts_atomic32_max_relaxed(mu_ts_atomic32_t* dst, uint32_t val)
{
    uint32_t prev = val;
    do
    {
        prev = mu_ts_atomic32_cas_relaxed(dst, prev, val);
    } while(prev < val);
    return prev;
}

MU_TS_INLINE uint64_t mu_ts_atomic64_max_relaxed(mu_ts_atomic64_t* dst, uint64_t val)
{
    uint64_t prev = val;
    do
    {
        prev = mu_ts_atomic64_cas_relaxed(dst, prev, val);
    } while(prev < val);
    return prev;
}

/* ------------------------------------------------------------------------- */
/* Platform sync/thread wrappers (public so other single headers can reuse)  */
/* ------------------------------------------------------------------------- */

#if defined(_WIN32)

typedef HANDLE             mu_ts_thread_handle;
typedef CRITICAL_SECTION   mu_ts_mutex;
typedef CONDITION_VARIABLE mu_ts_cond;

typedef DWORD(WINAPI* mu_ts_thread_entry)(LPVOID);

MU_TS_INLINE bool mu_ts_mutex_init(mu_ts_mutex* m)
{
    InitializeCriticalSection(m);
    return true;
}
MU_TS_INLINE void mu_ts_mutex_exit(mu_ts_mutex* m)
{
    DeleteCriticalSection(m);
}
MU_TS_INLINE void mu_ts_mutex_lock(mu_ts_mutex* m)
{
    EnterCriticalSection(m);
}
MU_TS_INLINE void mu_ts_mutex_unlock(mu_ts_mutex* m)
{
    LeaveCriticalSection(m);
}

MU_TS_INLINE bool mu_ts_cond_init(mu_ts_cond* c)
{
    InitializeConditionVariable(c);
    return true;
}
MU_TS_INLINE void mu_ts_cond_exit(mu_ts_cond* c)
{
    (void)c;
}
MU_TS_INLINE void mu_ts_cond_wake_one(mu_ts_cond* c)
{
    WakeConditionVariable(c);
}
MU_TS_INLINE void mu_ts_cond_wake_all(mu_ts_cond* c)
{
    WakeAllConditionVariable(c);
}
MU_TS_INLINE bool mu_ts_cond_wait(mu_ts_cond* c, mu_ts_mutex* m, uint32_t timeout_ms)
{
    DWORD t = (timeout_ms == UINT32_MAX) ? INFINITE : timeout_ms;
    return SleepConditionVariableCS(c, m, t) != 0;
}

MU_TS_INLINE bool mu_ts_thread_start(mu_ts_thread_handle* out, mu_ts_thread_entry fn, void* user)
{
    HANDLE h = CreateThread(NULL, 0, fn, user, 0, NULL);
    if(!h)
        return false;
    *out = h;
    return true;
}
MU_TS_INLINE void mu_ts_thread_join(mu_ts_thread_handle h)
{
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
}
MU_TS_INLINE void mu_ts_thread_detach(mu_ts_thread_handle h)
{
    CloseHandle(h);
}
MU_TS_INLINE uint64_t mu_ts_cpu_count(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (uint64_t)info.dwNumberOfProcessors;
}

#else

typedef pthread_t       mu_ts_thread_handle;
typedef pthread_mutex_t mu_ts_mutex;
typedef pthread_cond_t  mu_ts_cond;

typedef void* (*mu_ts_thread_entry)(void*);

MU_TS_INLINE bool mu_ts_mutex_init(mu_ts_mutex* m)
{
    return pthread_mutex_init(m, NULL) == 0;
}
MU_TS_INLINE void mu_ts_mutex_exit(mu_ts_mutex* m)
{
    pthread_mutex_destroy(m);
}
MU_TS_INLINE void mu_ts_mutex_lock(mu_ts_mutex* m)
{
    pthread_mutex_lock(m);
}
MU_TS_INLINE void mu_ts_mutex_unlock(mu_ts_mutex* m)
{
    pthread_mutex_unlock(m);
}

MU_TS_INLINE bool mu_ts_cond_init(mu_ts_cond* c)
{
    return pthread_cond_init(c, NULL) == 0;
}
MU_TS_INLINE void mu_ts_cond_exit(mu_ts_cond* c)
{
    pthread_cond_destroy(c);
}
MU_TS_INLINE void mu_ts_cond_wake_one(mu_ts_cond* c)
{
    pthread_cond_signal(c);
}
MU_TS_INLINE void mu_ts_cond_wake_all(mu_ts_cond* c)
{
    pthread_cond_broadcast(c);
}

MU_TS_INLINE bool mu_ts_cond_wait(mu_ts_cond* c, mu_ts_mutex* m, uint32_t timeout_ms)
{
    if(timeout_ms == UINT32_MAX)
    {
        return pthread_cond_wait(c, m) == 0;
    }

    struct timespec ts;
#if defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec  = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000L;
#endif

    ts.tv_sec += (time_t)(timeout_ms / 1000U);
    ts.tv_nsec += (long)((timeout_ms % 1000U) * 1000000UL);
    if(ts.tv_nsec >= 1000000000L)
    {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    return pthread_cond_timedwait(c, m, &ts) == 0;
}

MU_TS_INLINE bool mu_ts_thread_start(mu_ts_thread_handle* out, mu_ts_thread_entry fn, void* user)
{
    return pthread_create(out, NULL, fn, user) == 0;
}
MU_TS_INLINE void mu_ts_thread_join(mu_ts_thread_handle h)
{
    pthread_join(h, NULL);
}
MU_TS_INLINE void mu_ts_thread_detach(mu_ts_thread_handle h)
{
    pthread_detach(h);
}
MU_TS_INLINE uint64_t mu_ts_cpu_count(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if(n <= 0)
        return 0;
    return (uint64_t)n;
}

#endif

/* ------------------------------------------------------------------------- */
/* Thread system API                                                          */
/* ------------------------------------------------------------------------- */

typedef void (*mu_ts_task_fn)(void* user, uint64_t thread_id);

typedef struct mu_ts_init_desc
{
    bool        set_affinity_mask;
    uint64_t    affinity_mask[16];
    uint64_t    thread_count;
    const char* thread_name;
} mu_ts_init_desc;

typedef struct mu_ts_exit_desc
{
    bool abandon_tasks;
    bool detach_threads;
} mu_ts_exit_desc;

typedef struct mu_ts_info
{
    uint64_t    thread_count;
    uint64_t    executed_thread_count;
    uint64_t    active_thread_count;
    const char* thread_name;
} mu_ts_info;

struct mu_ts_system;
typedef struct mu_ts_system* mu_ts_handle;

static const mu_ts_init_desc MU_TS_INIT_DESC_DEFAULT = {
    false,
    {0},
    UINT64_MAX,
    NULL,
};

static const mu_ts_exit_desc MU_TS_EXIT_DESC_DEFAULT = {
    false,
    false,
};

typedef struct mu_ts_task
{
    mu_ts_task_fn fn;
    void*         user;
} mu_ts_task;

typedef struct mu_ts_worker_bootstrap
{
    struct mu_ts_system* sys;
    uint64_t             worker_id;
    bool                 set_affinity;
    uint64_t             affinity_mask[16];
} mu_ts_worker_bootstrap;

typedef struct mu_ts_system
{
    mu_ts_mutex mutex;
    mu_ts_cond  condition_tasks;
    mu_ts_cond  condition_idle;

    const char* name;
    uint64_t    thread_count;

    mu_ts_thread_handle*    threads;
    mu_ts_worker_bootstrap* boot;

    mu_ts_task* tasks;
    uint64_t    tasks_taken;
    uint64_t    tasks_queued;
    uint64_t    tasks_cap;

    mu_ts_atomic32_t activated_thread_count_atomic;
    mu_ts_atomic32_t active_thread_count_atomic;
    uint32_t         idle_thread_count;

    bool stop_abandon;
    bool stop;
} mu_ts_system;

MU_TS_INLINE bool mu_ts__ensure_task_capacity(mu_ts_system* t, uint64_t need)
{
    const uint64_t kChunk = 128;
    if(need <= t->tasks_cap)
        return true;

    uint64_t new_cap = (need + (kChunk - 1)) / kChunk;
    new_cap *= kChunk;

    mu_ts_task* p = (mu_ts_task*)MU_TS_REALLOC(t->tasks, (size_t)(new_cap * sizeof(mu_ts_task)));
    if(!p)
        return false;

    t->tasks     = p;
    t->tasks_cap = new_cap;
    return true;
}

MU_TS_INLINE void mu_ts__compact_queue(mu_ts_system* t)
{
    uint64_t scheduled = t->tasks_queued - t->tasks_taken;

    if(t->tasks_taken > scheduled * 3)
    {
        if(scheduled)
        {
            memmove(t->tasks, t->tasks + t->tasks_taken, (size_t)(scheduled * sizeof(mu_ts_task)));
        }
        t->tasks_queued = scheduled;
        t->tasks_taken  = 0;
    }

    if(t->tasks_cap > 256 && scheduled <= 128)
    {
        mu_ts_task* p = (mu_ts_task*)MU_TS_REALLOC(t->tasks, 128 * sizeof(mu_ts_task));
        if(p)
        {
            t->tasks     = p;
            t->tasks_cap = 128;
        }
    }
}

MU_TS_INLINE mu_ts_task mu_ts__get_task(mu_ts_system* t, uint64_t tid, bool blocking)
{
    mu_ts_task task;
    task.fn   = NULL;
    task.user = NULL;

    if(t->stop_abandon)
        return task;

    mu_ts_mutex_lock(&t->mutex);

    bool idle_set = false;
    for(;;)
    {
        if(t->tasks_taken < t->tasks_queued)
        {
            task = t->tasks[t->tasks_taken++];
            break;
        }

        if(t->stop)
            break;

        if(!blocking)
            break;

        if(!idle_set && tid != UINT64_MAX)
        {
            idle_set = true;
            ++t->idle_thread_count;
        }
        mu_ts_cond_wake_all(&t->condition_idle);
        (void)mu_ts_cond_wait(&t->condition_tasks, &t->mutex, UINT32_MAX);
    }

    if(idle_set)
    {
        --t->idle_thread_count;
    }

    mu_ts__compact_queue(t);
    mu_ts_mutex_unlock(&t->mutex);
    return task;
}

#if defined(_WIN32)
static DWORD WINAPI mu_ts__worker_entry(LPVOID arg)
#else
static void* mu_ts__worker_entry(void* arg)
#endif
{
    mu_ts_worker_bootstrap* boot = (mu_ts_worker_bootstrap*)arg;
    mu_ts_system*           t    = boot->sys;
    uint64_t                tid  = boot->worker_id;

#if !defined(_WIN32)
    (void)boot;
    (void)t;
    (void)tid;
    /*
        Affinity mask and per-thread naming are intentionally no-op here to keep
        strict C99 portability without requiring platform-specific pthread
        extension prototypes.
    */
#endif

    (void)mu_ts_atomic32_add_relaxed(&t->activated_thread_count_atomic, 1);

    for(;;)
    {
        mu_ts_task task = mu_ts__get_task(t, tid, true);
        if(task.fn)
        {
            task.fn(task.user, tid);
            mu_ts_mutex_lock(&t->mutex);
            mu_ts_cond_wake_all(&t->condition_idle);
            mu_ts_mutex_unlock(&t->mutex);
            continue;
        }
        if(t->stop || t->stop_abandon)
        {
            break;
        }
    }

    (void)mu_ts_atomic32_add_relaxed(&t->active_thread_count_atomic, -1);
    mu_ts_mutex_lock(&t->mutex);
    mu_ts_cond_wake_all(&t->condition_idle);
    mu_ts_mutex_unlock(&t->mutex);

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

MU_TS_INLINE bool mu_ts_init(mu_ts_handle* out, const mu_ts_init_desc* in_desc)
{
    mu_ts_init_desc        local_desc = MU_TS_INIT_DESC_DEFAULT;
    const mu_ts_init_desc* desc       = in_desc ? in_desc : &local_desc;

    if(!out)
        return false;
    *out = NULL;

    if(desc->thread_count == 0)
    {
        return true;
    }

    uint64_t cpu_count = mu_ts_cpu_count();
    uint64_t count     = desc->thread_count;

    if(count > cpu_count)
        count = cpu_count;
    if(count == 0)
        return false;

    mu_ts_system* t = (mu_ts_system*)MU_TS_MALLOC(sizeof(mu_ts_system));
    if(!t)
        return false;
    memset(t, 0, sizeof(*t));

    t->threads = (mu_ts_thread_handle*)MU_TS_MALLOC((size_t)(count * sizeof(mu_ts_thread_handle)));
    t->boot    = (mu_ts_worker_bootstrap*)MU_TS_MALLOC((size_t)(count * sizeof(mu_ts_worker_bootstrap)));

    if(!t->threads || !t->boot)
    {
        MU_TS_FREE(t->threads);
        MU_TS_FREE(t->boot);
        MU_TS_FREE(t);
        return false;
    }

    t->name         = desc->thread_name ? desc->thread_name : "mu_ts";
    t->thread_count = count;

    bool mutex_ok = mu_ts_mutex_init(&t->mutex);
    bool tasks_ok = false;
    bool idle_ok  = false;

    if(mutex_ok)
        tasks_ok = mu_ts_cond_init(&t->condition_tasks);
    if(mutex_ok && tasks_ok)
        idle_ok = mu_ts_cond_init(&t->condition_idle);

    if(!mutex_ok || !tasks_ok || !idle_ok)
    {
        if(idle_ok)
            mu_ts_cond_exit(&t->condition_idle);
        if(tasks_ok)
            mu_ts_cond_exit(&t->condition_tasks);
        if(mutex_ok)
            mu_ts_mutex_exit(&t->mutex);
        MU_TS_FREE(t->threads);
        MU_TS_FREE(t->boot);
        MU_TS_FREE(t);
        return false;
    }

    if(!mu_ts__ensure_task_capacity(t, 128))
    {
        mu_ts_cond_exit(&t->condition_tasks);
        mu_ts_cond_exit(&t->condition_idle);
        mu_ts_mutex_exit(&t->mutex);
        MU_TS_FREE(t->threads);
        MU_TS_FREE(t->boot);
        MU_TS_FREE(t);
        return false;
    }

    mu_ts_atomic32_store_relaxed(&t->activated_thread_count_atomic, 0);
    mu_ts_atomic32_store_relaxed(&t->active_thread_count_atomic, (uint32_t)count);

    for(uint64_t i = 0; i < count; ++i)
    {
        t->boot[i].sys          = t;
        t->boot[i].worker_id    = i + 1;
        t->boot[i].set_affinity = desc->set_affinity_mask;
        memcpy(t->boot[i].affinity_mask, desc->affinity_mask, sizeof(t->boot[i].affinity_mask));

        if(!mu_ts_thread_start(&t->threads[i], mu_ts__worker_entry, &t->boot[i]))
        {
            mu_ts_mutex_lock(&t->mutex);
            t->stop         = true;
            t->stop_abandon = true;
            mu_ts_cond_wake_all(&t->condition_tasks);
            mu_ts_mutex_unlock(&t->mutex);

            for(uint64_t j = 0; j < i; ++j)
            {
                mu_ts_thread_join(t->threads[j]);
            }

            mu_ts_cond_exit(&t->condition_tasks);
            mu_ts_cond_exit(&t->condition_idle);
            mu_ts_mutex_exit(&t->mutex);
            MU_TS_FREE(t->tasks);
            MU_TS_FREE(t->threads);
            MU_TS_FREE(t->boot);
            MU_TS_FREE(t);
            return false;
        }
    }

    *out = t;
    return true;
}

MU_TS_INLINE void mu_ts_exit(mu_ts_handle* h, const mu_ts_exit_desc* in_desc)
{
    mu_ts_exit_desc        local_desc = MU_TS_EXIT_DESC_DEFAULT;
    const mu_ts_exit_desc* desc       = in_desc ? in_desc : &local_desc;

    if(!h || !*h)
        return;

    mu_ts_system* t = *h;
    *h              = NULL;

    mu_ts_mutex_lock(&t->mutex);
    t->stop = true;
    if(desc->abandon_tasks)
        t->stop_abandon = true;
    mu_ts_cond_wake_all(&t->condition_tasks);
    mu_ts_mutex_unlock(&t->mutex);

    for(uint64_t i = 0; i < t->thread_count; ++i)
    {
        if(!desc->detach_threads)
            mu_ts_thread_join(t->threads[i]);
        else
            mu_ts_thread_detach(t->threads[i]);
    }

    mu_ts_cond_exit(&t->condition_tasks);
    mu_ts_cond_exit(&t->condition_idle);
    mu_ts_mutex_exit(&t->mutex);

    MU_TS_FREE(t->tasks);
    MU_TS_FREE(t->threads);
    MU_TS_FREE(t->boot);
    MU_TS_FREE(t);
}

MU_TS_INLINE void mu_ts_add_tasks(mu_ts_handle h, mu_ts_task_fn fn, uint64_t count, uint64_t user_size, void* user_array)
{
    if(!fn || count == 0)
        return;

    if(!h)
    {
        uint8_t* users = (uint8_t*)user_array;
        for(uint64_t i = 0; i < count; ++i)
        {
            void* user = users ? (void*)(users + i * user_size) : NULL;
            fn(user, UINT64_MAX);
        }
        return;
    }

    mu_ts_system* t = h;
    mu_ts_mutex_lock(&t->mutex);

    uint64_t offset = t->tasks_queued;
    t->tasks_queued += count;

    if(!mu_ts__ensure_task_capacity(t, t->tasks_queued))
    {
        t->tasks_queued = offset;
        mu_ts_mutex_unlock(&t->mutex);
        return;
    }

    uint8_t* users = (uint8_t*)user_array;
    for(uint64_t i = 0; i < count; ++i)
    {
        t->tasks[offset + i].fn   = fn;
        t->tasks[offset + i].user = users ? (void*)(users + i * user_size) : NULL;
    }

    if(count == 1)
        mu_ts_cond_wake_one(&t->condition_tasks);
    else
        mu_ts_cond_wake_all(&t->condition_tasks);

    mu_ts_mutex_unlock(&t->mutex);
}

MU_TS_INLINE void mu_ts_add_task(mu_ts_handle h, mu_ts_task_fn fn, void* user)
{
    mu_ts_add_tasks(h, fn, 1, 0, user);
}

MU_TS_INLINE bool mu_ts_assist(mu_ts_handle h)
{
    if(!h)
        return false;

    mu_ts_task task = mu_ts__get_task(h, UINT64_MAX, false);
    if(!task.fn)
        return false;

    task.fn(task.user, UINT64_MAX);

    mu_ts_mutex_lock(&h->mutex);
    mu_ts_cond_wake_all(&h->condition_idle);
    mu_ts_mutex_unlock(&h->mutex);

    return true;
}

MU_TS_INLINE bool mu_ts_wait_idle_timeout(mu_ts_handle h, uint32_t timeout_ms)
{
    if(!h)
        return true;

    mu_ts_system* t = h;

    mu_ts_mutex_lock(&t->mutex);

    bool           idle       = false;
    uint64_t       elapsed_ms = 0;
    const uint32_t slice_ms   = 5;

    for(;;)
    {
        uint64_t active = (uint64_t)mu_ts_atomic32_load_relaxed(&t->active_thread_count_atomic);
        idle            = (t->tasks_taken >= t->tasks_queued) && (t->idle_thread_count >= active);
        if(idle || timeout_ms == 0)
            break;

        if(timeout_ms != UINT32_MAX)
        {
            uint32_t wait_ms = (uint32_t)((timeout_ms - elapsed_ms) < slice_ms ? (timeout_ms - elapsed_ms) : slice_ms);
            if(wait_ms == 0)
                break;
            (void)mu_ts_cond_wait(&t->condition_idle, &t->mutex, wait_ms);
            elapsed_ms += wait_ms;
            if(elapsed_ms >= timeout_ms)
                break;
        }
        else
        {
            (void)mu_ts_cond_wait(&t->condition_idle, &t->mutex, UINT32_MAX);
        }
    }

    mu_ts_mutex_unlock(&t->mutex);
    return idle;
}

MU_TS_INLINE bool mu_ts_is_idle(mu_ts_handle h)
{
    return mu_ts_wait_idle_timeout(h, 0);
}

MU_TS_INLINE void mu_ts_wait_idle(mu_ts_handle h)
{
    (void)mu_ts_wait_idle_timeout(h, UINT32_MAX);
}

MU_TS_INLINE void mu_ts_get_info(mu_ts_handle h, mu_ts_info* out_info)
{
    if(!out_info)
        return;
    memset(out_info, 0, sizeof(*out_info));

    if(!h)
        return;

    out_info->thread_count          = h->thread_count;
    out_info->executed_thread_count = (uint64_t)mu_ts_atomic32_load_relaxed(&h->activated_thread_count_atomic);
    out_info->active_thread_count   = (uint64_t)mu_ts_atomic32_load_relaxed(&h->active_thread_count_atomic);
    out_info->thread_name           = h->name;
}

#ifdef __cplusplus
}
#endif

#endif /* MU_THREAD_SYSTEM_H */
