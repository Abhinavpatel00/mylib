# Concurrency: `mu_thread_system.h` + `mu_task_scheduler.h`

Two layers:

1. **`mu_thread_system.h`** — a fixed worker pool plus portable atomics,
   mutexes, and condition variables.
2. **`mu_task_scheduler.h`** — a job scheduler built on top: priorities,
   counters/groups, dependency tracking, and parallel-for.

Both are `static inline` single headers with malloc/free override macros
(`MU_TS_MALLOC`, `MU_SCHED_MALLOC`, ...).

---

## mu_thread_system.h — worker pool + primitives

### Portability layer

| Group | Windows | POSIX |
|---|---|---|
| Thread handle | `HANDLE` | `pthread_t` |
| Mutex | `CRITICAL_SECTION` | `pthread_mutex_t` |
| Condvar | `CONDITION_VARIABLE` | `pthread_cond_t` |

```c
bool mu_ts_thread_start(mu_ts_thread_handle* out, mu_ts_thread_entry fn, void* user);
void mu_ts_thread_join(mu_ts_thread_handle h);
void mu_ts_thread_detach(mu_ts_thread_handle h);
uint64_t mu_ts_cpu_count(void);
```

### Atomics (`mu/mu_atomic.h` — the single implementation)

All atomics live in `mu/mu_atomic.h`. `mu_sync.h`, `mu_thread_system.h`,
and `mu_task_scheduler.h` add no primitives of their own — they only
compose these. Canonical types are small structs, acquire/release ordering
is explicit (correct on ARM, nearly free on x86):

```c
mu_atomic32_t v;  mu_atomic32_init(&v, 0);   /* struct wrapping one word */

mu_atomic32_load_relaxed(&v);  mu_atomic32_load_acquire(&v);
mu_atomic32_store_relaxed(&v, x); mu_atomic32_store_release(&v, x);
mu_atomic32_fetch_add(&v, delta);            /* returns previous           */
mu_atomic32_exchange(&v, x);                 /* returns previous           */
mu_atomic32_compare_exchange(&v, &expected, x); /* bool form; refreshes expected */
mu_atomic32_fetch_max(&v, x);                /* CAS loop, returns previous */

mu_cpu_relax(); /* pause/yield inside spin loops */
```

Same `mu_atomic64_*` and `mu_atomicptr_*` families. `mu_ts_atomic32_t` and
the `mu_ts_atomic32_*` names remain as thin aliases over this backend, so
existing thread-system / scheduler code keeps compiling.

### Mutex / condvar

```c
mu_ts_mutex m;  mu_ts_mutex_init(&m);  mu_ts_mutex_lock(&m);
mu_ts_mutex_unlock(&m);  mu_ts_mutex_exit(&m);

mu_ts_cond c;   mu_ts_cond_init(&c);
mu_ts_cond_wait(&c, &m, timeout_ms);   /* returns bool (false on timeout) */
mu_ts_cond_wake_one(&c);  mu_ts_cond_wake_all(&c);
mu_ts_cond_exit(&c);
```

### The pool

```c
typedef void (*mu_ts_task_fn)(void* user, uint64_t thread_id);

typedef struct mu_ts_init_desc
{
    /* worker count (0 = hardware concurrency), queue sizing flags */
} mu_ts_init_desc;
static const mu_ts_init_desc MU_TS_INIT_DESC_DEFAULT = { ... };

typedef struct mu_ts_system* mu_ts_handle;

mu_ts_handle h;
mu_ts_init(&h, NULL);              /* NULL desc = defaults            */
mu_ts_add_task(h, my_fn, user);                    /* single task     */
mu_ts_add_tasks(h, my_fn, count, user_size, user_array);  /* batch: one task per element */
mu_ts_wait_idle(h);                /* block until queue drained       */
mu_ts_wait_idle_timeout(h, ms);    /* false on timeout                */
mu_ts_exit(h, NULL);
```

- **Work model:** producers push tasks; workers wake, steal, execute, and
  sleep via condvars when idle. The `mu_ts_exit_desc` controls shutdown
  behavior (drain vs abandon).
- `mu_ts_add_tasks` is the hot path: it enqueues `count` invocations over a
  contiguous user array (`user_size` = per-element stride) without one
  allocation per task.
- Every task receives `(user_data, thread_id)` — `thread_id` lets you pin
  per-worker scratch without TLS.

---

## mu_task_scheduler.h — jobs with priorities and dependencies

Built on `mu_thread_system.h`. Adds a ready/pending task graph with
priorities, completion counters, and group waits.

### Priorities

```c
typedef enum mu_sched_priority
{
    /* high / normal / low tiers; clamped via mu_sched__clamp_priority */
} mu_sched_priority;
```

High-priority tasks jump ahead of queued low-priority work; low-priority
tasks also yield during `assist` (see below) so foreground work finishes
first.

### Task descriptor

```c
typedef void (*mu_sched_task_fn)(void* user, uint32_t worker_index);

typedef struct mu_sched_task_desc
{
    mu_sched_task_fn fn;
    void*            user;
    mu_sched_priority priority;
    mu_sched_group*   group;    /* optional completion group to signal */
} mu_sched_task_desc;
```

### Scheduler lifetime

```c
mu_task_scheduler* s;
mu_sched_init(&s, NULL);           /* starts its own thread system     */
mu_sched_exit(&s);
```

### Submitting work

```c
mu_sched_task_desc d = { .fn = job, .user = ctx, .priority = MU_SCHED_PRIORITY_NORMAL };
mu_sched_submit(&s, &d);           /* single task                      */

mu_sched_submit_tasks(&s, fn, user_array, count, user_size, priority, group);
                                   /* count tasks over a user array    */
```

### Counters and groups

```c
typedef struct mu_sched_counter mu_sched_counter;   /* atomic + condvar */
typedef mu_sched_counter mu_sched_group;

mu_sched_group_init(&g, initial);   /* counter starts at N             */
/* ... submit tasks with .group = &g ... each completion decrements   */
mu_sched_group_wait(&g);            /* block until zero                */
mu_sched_group_wait_timeout(&g, ms);
mu_sched_group_count(&g);
mu_sched_group_exit(&g);
```

Same primitives under plain `mu_sched_counter_*` names. This is the
dependency mechanism: a task can *add* to a counter for downstream work,
then another thread waits on it.

### Waiting vs assisting

```c
mu_sched_wait_idle(&s);              /* block until everything drains   */
mu_sched_wait_idle_timeout(&s, ms);
mu_sched_assist(&s);                 /* don't block — run pending tasks
                                        yourself until queue empties    */
```

`assist` is the important one for frame loops: main-thread waiters execute
jobs instead of sleeping, keeping total wall time down.

### parallel_for

Range-parallel helper that splits `[begin, end)` across workers:

```c
typedef void (*mu_sched_parallel_for_fn)(uint32_t begin, uint32_t end, uint32_t worker_index, void* user);

mu_sched_parallel_for(&s, begin, end, fn, user /*, min batch */);
```

The range is chunked; each chunk runs on some worker with its exclusive
`[begin, end)` slice and the executing worker's index for scratch arrays.
Combine with `mu_sched_group` to await the whole range.

### Practical notes

- Task `user` data must stay alive until the task runs (or the group it
  belongs to completes) — the scheduler copies pointers, not buffers.
- Priorities affect *submission order*, not preemption: a running low-pri
  task is never interrupted.
- All wait functions have `_timeout` variants; use them in interactive
  paths so a stuck task can't freeze your frame loop forever.
