# Multi-Threading The Truth (Comprehensive Rewrite)

This document is a detailed reconstruction and expansion of the design space behind a multi-threaded object database-like runtime system (“The Truth”).

The core challenge is:

> Build a shared data system where reads are cheap, writes are scalable, consistency is clear, and correctness survives real-world edge cases.

---

## 1) System context: what “The Truth” is

The Truth is a centralized in-memory data model with:

- object IDs (`uint64_t`)
- object types (schema)
- typed properties (bool/int/float/string/reference/set/subobject/buffer)
- change tracking and notifications

Basic usage shape:

```c
uint64_t id = truth_create_object(tt, asset_type);
truth_set_string(tt, id, ASSET_NAME_PROPERTY, "foo");
```

This system is the data interchange layer between tools, runtime/editor subsystems, and plugins.

---

## 2) Design goals under load

A single global lock is simple but destroys scalability.

Target behavior:

1. **Reads** should be lock-free or near-lock-free in the common path.
2. **Writes** may synchronize, but ideally only at object granularity.
3. Readers should never observe torn values or torn object states.
4. System must provide a clearly stated consistency model.

---

## 3) Core technique: copy-on-write object versions

### 3.1 Why property-level writes are risky

If writers mutate in place and readers are unsynchronized, readers may see partial updates.

### 3.2 Version replacement model

Keep a table from object ID to pointer of current immutable object version.
Writer flow:

1. read current pointer
2. clone object
3. modify clone
4. atomically publish pointer to clone

ASCII overview:

```text
ID -> [current object pointer]

before publish:
ID -> ObjV1
writer edits clone ObjV2 privately

after publish (atomic ptr swap):
ID -> ObjV2
```

Readers with pointer to `ObjV1` still see a coherent snapshot.

---

## 4) Explicit API shape: read/write/commit

To avoid hidden repeated copies and mixed-property snapshots:

```c
const object_o *r = truth_read(tt, id);
float x = truth_get_float(tt, r, POSITION_X);
float y = truth_get_float(tt, r, POSITION_Y);
float z = truth_get_float(tt, r, POSITION_Z);

object_o *w = truth_write(tt, id);
truth_set_float(tt, w, POSITION_Y, 5.0f);
truth_set_float(tt, w, POSITION_Z, 5.0f);
truth_commit(tt, w);
```

Properties are read from one consistent object version.

---

## 5) Consistency levels and conflict semantics

### 5.1 Torn values

Avoided by atomic aligned scalar/pointer operations.

### 5.2 Torn object snapshots

Avoided by publishing whole-object pointer atomically.

### 5.3 Write-write conflicts

If two writers clone same base and both commit, last-writer-wins unless conflict detection is used.

### 5.4 Cross-object consistency

Without transaction support, readers can observe mixed epochs across objects.

---

## 6) CAS-based optimistic commit

Use compare-and-swap on table entry:

- commit succeeds only if current pointer still equals writer’s base pointer
- otherwise commit fails, writer retries on newer base

```c
bool truth_try_commit(truth_t *tt, id_t id, object_o *expected_old, object_o *new_obj)
{
    return atomic_compare_exchange_strong(&tt->table[id], &expected_old, new_obj);
}
```

### Tradeoff

- stronger conflict semantics
- retry complexity and potential writer churn under contention

---

## 7) Multi-object transactions via root indirection

To commit a set of object updates atomically, publish a new root pointer to a versioned lookup structure.

```text
RootR0 -> table T0
RootR1 -> table T1 (shares most unchanged nodes, swaps changed leaves)
```

Reader sees either `R0` or `R1`—never partial cross-object publish.

### Practical limitation

Single root hot spot can increase commit contention.
Hierarchical/persistent structures reduce copy cost but not root publication contention.

---

## 8) Chosen consistency model in practice

For engine/tool data (non-financial, non-ACID domain):

- often acceptable: object-level coherence, eventual cross-object coherence
- optional stronger path: `try_commit()` for contested paths
- resolve semantic conflicts at higher system level (animation vs physics authority), not low-level locks

---

## 9) Memory reclamation is the hard part

Once new version is published, old version cannot be immediately freed because readers may still hold pointers.

### 9.1 Why naive refcount read path fails

Sequence `load ptr -> increment refcount` is not atomic as a pair.
Object can be reclaimed between steps.

### 9.2 Safe approaches

1. **Epoch/frame-based reclamation** (very practical in real-time loops)
2. **Hazard pointers**
3. **Read-side critical sections with global quiescence tracking**
4. **RCU-like schemes**

### 9.3 Frame-epoch model (common game/editor fit)

Assume most reads complete within a frame/update cycle.
Retire old objects to garbage list tagged with retire epoch.
Collect only after all threads have advanced beyond safe epoch.

ASCII timeline:

```text
epoch 100: publish ObjV2, retire ObjV1@100
...
all readers >= 101
=> ObjV1 reclaimable
```

Long-lived background readers need explicit `lock_read()/unlock_read()` or hazard registration.

---

## 10) Lookup table design under concurrent readers

`std::vector<object*>` with realloc is unsafe for lock-free readers (table relocation races).

### 10.1 Flat huge array

Safe but requires fixed max objects.

### 10.2 Multi-level sparse table

Example 10-10-10 split for ~1B slots:

```c
typedef struct { object_o *objects[1024]; } object_block_t;
typedef struct { object_block_t *blocks[1024]; } object_super_block_t;

typedef struct {
    object_super_block_t *super_blocks[1024];
} truth_t;
```

Lookup:

```c
object_o *o = tt->super_blocks[id.super_block]
                 ->blocks[id.block]
                 ->objects[id.index];
```

Advantages:

- lock-free reads with stable table topology
- bounded sparse overhead
- scalable capacity without giant contiguous allocation

---

## 11) Allocation and ID generation contention

Even with lock-free reads, writes still contend on:

- ID allocation
- block/superblock creation
- memory allocator internals
- retire queue synchronization

Mitigations:

1. thread-local ID ranges (batch reserve)
2. per-thread object pools/slabs
3. lock sharding (by ID hash)
4. high-performance multi-thread allocator

---

## 12) Sync primitive choices: pragmatic guidance

### Critical section / mutex

- robust under preemption
- OS can sleep waiter and schedule owner
- good default when lock hold times are short

### Spinlock

- low overhead if waits are tiny and no preemption
- bad under preemption/oversubscription (burn + latency)

### Lock-free

- progress advantages in some scenarios
- significantly harder correctness story
- benefits can disappear if other dependencies still lock

Key point:

> Lock-free is a progress-property tool, not a universal performance magic trick.

---

## 13) Fine-grained lock footprint issues

If millions of logical objects need independent locking, per-object heavy lock structs are memory-expensive.

Alternatives:

1. lock striping: fixed lock array indexed by hash(ID)
2. compact OS primitives (e.g., SRW lock / futex-like wait-address)
3. custom compact lock word + wait primitive

---

## 14) Reference pseudocode: safe-ish writer path

```c
object_o *truth_write(truth_t *tt, id_t id)
{
    object_o *cur = atomic_load_explicit(&tt->ptr[id], memory_order_acquire);
    object_o *copy = clone_object(cur);
    copy->meta.base_ptr = cur;
    copy->meta.id = id;
    return copy;
}

void truth_commit_last_writer_wins(truth_t *tt, object_o *w)
{
    id_t id = w->meta.id;
    object_o *old = atomic_exchange_explicit(&tt->ptr[id], w, memory_order_acq_rel);
    retire_object(tt, old);
}

bool truth_try_commit_cas(truth_t *tt, object_o *w)
{
    id_t id = w->meta.id;
    object_o *expected = w->meta.base_ptr;
    if (atomic_compare_exchange_strong_explicit(
            &tt->ptr[id], &expected, w,
            memory_order_acq_rel, memory_order_acquire)) {
        retire_object(tt, expected);
        return true;
    }
    destroy_object(w);
    return false;
}
```

---

## 15) Common failure modes checklist

1. freeing retired versions too early
2. ABA-like pointer recycling without generation safety
3. vector/table relocation races with lock-free readers
4. hidden allocator lock contention dominating writes
5. semantic conflicts handled too low (data races disguised as business logic races)
6. no backoff or batching under high CAS failure rates

---

## 16) Recommended architecture for this domain

1. immutable object versions + atomic pointer publish
2. lock-free read fast path
3. write cloning + explicit commit API
4. epoch-based reclamation for frame-bounded readers
5. hierarchical pointer table
6. striped synchronization for writer-side shared structures
7. optional CAS commit for conflict-sensitive writers

This gives practical throughput with manageable complexity.

---

## 17) Final perspective

The headline pattern is simple:

> Readers are fast because they read immutable snapshots; writers pay the synchronization and lifecycle costs.

The difficulty is not the snapshot idea itself—it is lifecycle management, contention points, and consistency boundaries.

Multi-threaded data systems become reliable when those boundaries are explicit in both API and implementation.
