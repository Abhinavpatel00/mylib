# Data Structures Part 1: Bulk Data (Comprehensive Rewrite)

This article is a deep, practical guide to the first problem almost every real-time system faces:

> How do we store a large number of objects so that iteration is fast, mutation is affordable, references are safe, and memory is predictable?

The answer is not one container. The answer is a set of tradeoffs.

---

## 1) Problem model: what “bulk data” means

In this series, **bulk data** means a large collection of similar records:

- entities
- particles
- active sounds
- active fades/effects
- physics contacts
- render items

Most systems repeatedly do some mix of:

1. iterate all live items (usually every frame)
2. add new items
3. remove arbitrary items
4. find specific items quickly
5. reference items from other systems

### Core constraints

A practical storage design balances:

- **Iteration throughput** (cache behavior, branch behavior)
- **Insertion/deletion cost**
- **Reference stability** (pointer/index/ID validity)
- **Allocator behavior** (few large allocations vs many tiny ones)
- **Memory overhead** (holes, headers, slack capacity)

---

## 2) Baselines: static array and dynamic contiguous array

### Static fixed-capacity array

```c
enum { MAX_SOUNDS = 4096 };

typedef struct {
    sound_t data[MAX_SOUNDS];
    uint32_t count;
} sound_pool_t;
```

**Pros**
- zero reallocations
- stable addresses
- predictable memory
- simple implementation

**Cons**
- hard cap (too low fails, too high wastes memory)
- less flexible for unknown workloads

### Dynamic contiguous array (`std::vector` style)

```c
typedef struct {
    sound_t *data;
    uint32_t count;
    uint32_t capacity;
} sound_array_t;
```

**Pros**
- excellent iteration locality
- amortized constant append
- simple indexing

**Cons**
- occasional O(n) growth copies
- pointer invalidation on growth
- middle deletion is expensive unless you reorder

---

## 3) The first major choice: deletion policy

Deleting `a[i]` from an array leads to 3 classic policies.

### A. Shift-left delete (stable order)

```text
before: [a0][a1][a2][a3][a4]
delete a1
after : [a0][a2][a3][a4][  ]
```

- Cost: O(n)
- Keeps order
- Usually too expensive in hot paths

### B. Swap-delete (compact, unordered)

```text
before: [a0][a1][a2][a3][a4]
delete a1
after : [a0][a4][a2][a3][  ]
```

```c
static inline void remove_swap(sound_array_t *a, uint32_t i)
{
    uint32_t last = a->count - 1;
     a->data[i] = a->data[last];
    --a->count;
}
```

- Cost: O(1)
- Preserves compact live range
- Breaks order
- Requires fixing external index references to moved element

### C. Keep holes (stable slot identity)

```text
slots: [obj][free][obj][free][obj]
```

- Deletion O(1)
- Stable slot index
- Iteration must skip holes
- Needs free-list bookkeeping

---

## 4) Holes + free list: stable slots with cheap mutations

If you keep holes, you need quick reuse of free slots. Use a free list embedded in slot storage.

```c
typedef struct {
    uint32_t next_free;
} freelist_node_t;

typedef struct {
    uint32_t generation;
    uint8_t occupied;
    uint8_t pad[3];
    union {
        object_t object;
        freelist_node_t free;
    } u;
} slot_t;

typedef struct {
    slot_t *slots;
    uint32_t count_slots;
    uint32_t free_head;    // UINT32_MAX = none
} object_store_t;
```

### Allocation/deallocation sketch

```c
uint32_t alloc_slot(object_store_t *s)
{
    if (s->free_head != UINT32_MAX) {
        uint32_t i = s->free_head;
        s->free_head = s->slots[i].u.free.next_free;
        s->slots[i].occupied = 1;
        return i;
    }

    uint32_t i = s->count_slots++;
    s->slots[i].occupied = 1;
    return i;
}

void free_slot(object_store_t *s, uint32_t i)
{
    s->slots[i].occupied = 0;
    s->slots[i].generation++;
    s->slots[i].u.free.next_free = s->free_head;
    s->free_head = i;
}
```

### Why this works well

- O(1) add/remove
- stable slot identity (`index`)
- simple memory accounting
- good fit for ID/handle-based systems

---

## 5) Safe external references: handles + generation counters

A raw index is unsafe after reuse.

```text
slot 7: object A
delete A
slot 7: object B
```

Old index `7` now refers to wrong object. Fix with generation checking.

```c
typedef struct {
    uint32_t index;
    uint32_t generation;
} object_handle_t;

object_t *resolve(object_store_t *s, object_handle_t h)
{
    if (h.index >= s->count_slots)
        return 0;

    slot_t *slot = &s->slots[h.index];
    if (!slot->occupied)
        return 0;
    if (slot->generation != h.generation)
        return 0;

    return &slot->u.object;
}
```

This gives stale-handle detection with tiny overhead.

---

## 6) Allocation strategy: not just container strategy

Even if your logical structure is great, allocator policy can still dominate behavior.

### Geometric growth (classic vector)

```text
capacity: 8 -> 16 -> 32 -> 64 -> ...
```

- Append cost is amortized O(1)
- Growth events are O(n) spikes
- Reallocation moves objects

### Why spikes matter

Real-time systems care about worst frame, not only average. A single huge growth step can hitch.

### Better non-moving options

1. **Virtual-memory reserved region + incremental commit**
2. **Fixed-size blocks/pages**
3. **Growing blocks kept forever (segmented vector)**

---

## 7) Blocked storage compromise

Fixed-size blocks avoid moving live objects.

```text
block 0: [0..255]
block 1: [256..511]
...
```

```c
typedef struct {
    object_t **blocks;
    uint32_t items_per_block;
    uint32_t count;
} block_store_t;

static inline object_t *at(block_store_t *s, uint32_t i)
{
    return &s->blocks[i / s->items_per_block][i % s->items_per_block];
}
```

**Tradeoff profile**
- no relocation of old objects
- stable pointers inside blocks
- slightly worse locality than one contiguous array
- simple index math

---

## 8) AoS vs SoA: layout for compute behavior

### AoS (Array of Structures)

```c
typedef struct {
    vec3 pos;
    vec3 vel;
    float age;
    float lifetime;
} particle_t;

particle_t particles[N];
```

### SoA (Structure of Arrays)

```c
typedef struct {
    vec3 *pos;
    vec3 *vel;
    float *age;
    float *lifetime;
    uint32_t n;
} particle_set_t;
```

### Access intuition

```text
AoS cache line: [pos vel age life][pos vel age life]...
SoA age line : [age0 age1 age2 age3 ...]
```

If your hot loop only updates `age`, SoA fetches mostly useful bytes.

### SIMD-friendly batched SoA

```c
typedef struct {
    float age[8];
    float pos_x[8], pos_y[8], pos_z[8];
    float vel_x[8], vel_y[8], vel_z[8];
} particle_block8_t;
```

This keeps vector width alignment and improves arithmetic throughput.

---

## 9) Detailed tradeoff matrix

| Decision | Fast iteration | Fast delete | Stable references | Memory efficiency | Simplicity |
|---|---:|---:|---:|---:|---:|
| Shift-delete contiguous | High | Low | Low | High | High |
| Swap-delete contiguous | High | High | Low (unless remapped) | High | High |
| Holes + free list | Medium (depends on hole rate) | High | High | Medium | Medium |
| VM-reserved contiguous | High | depends on delete policy | High if no move | High at scale | Medium |
| Blocked storage | Medium-High | depends on policy | High | High | Medium |
| SoA hot fields | Very high in targeted loops | varies | index-based | Medium | Lower |

---

## 10) Practical implementation patterns

### Pattern A: “general gameplay objects”

- Holes + free list
- generation handles
- optional secondary indices
- blocked or VM-reserved backing

Best when safety and flexibility matter more than peak streaming throughput.

### Pattern B: “tight numeric simulation”

- tightly packed arrays
- swap-delete
- index remap table
- SoA (or blocked SoA)

Best when per-frame hot loop throughput dominates.

### Pattern C: “hybrid”

Keep authoritative storage AoS/handles, build temporary SoA views for expensive kernels.

```text
authoritative AoS -> gather hot fields -> SIMD process -> scatter results back
```

Often yields most of SoA speed without permanent SoA complexity everywhere.

---

## 11) Common failure modes (and fixes)

1. **Dangling pointers after growth**
   - Fix: non-moving storage or handle indirection.

2. **O(n²) behavior from repeated shift deletes**
   - Fix: swap-delete or holes.

3. **Index stale bugs after swap-delete**
   - Fix: maintain reverse map `object_id -> current_index`.

4. **Too many tiny allocations**
   - Fix: pooled pages, blocks, or VM reservation.

5. **Poor SoA maintainability**
   - Fix: split only truly hot fields.

---

## 12) Complexity summary

Assume `n` live items, `h` holes, `m` matches in a query result.

- Iterate packed array: O(n)
- Iterate holes array: O(n + h-skip overhead)
- Allocate with free list: O(1)
- Delete to free list: O(1)
- Delete swap-pop: O(1)
- Shift delete: O(n)
- Handle validation: O(1)

Amortized append for geometric growth remains O(1), but worst-case step is O(n).

---

## 13) Recommended default (sane baseline)

For most engine/runtime systems:

1. Use **AoS slot storage with holes**.
2. Keep a **free list** in unused slots.
3. Use **generation handles** for external references.
4. Back with **VM reservation + commit** when available, else fixed-size blocks.
5. Add specialized SoA or packed views only where profiling proves value.

This gives an excellent balance of simplicity, safety, and performance.

---

## 14) Minimal reference implementation sketch

```c
typedef struct {
    uint32_t generation;
    uint8_t occupied;
    uint8_t pad[3];
    union {
        object_t object;
        uint32_t next_free;
    } u;
} slot_t;

typedef struct {
    slot_t *slot;
    uint32_t count;
    uint32_t free_head;
} store_t;

typedef struct {
    uint32_t index;
    uint32_t generation;
} handle_t;

handle_t store_create(store_t *s, const object_t *in);
void store_destroy(store_t *s, handle_t h);
object_t *store_get(store_t *s, handle_t h);
```

With this tiny API, you get stable identity, stale-handle safety, and O(1) mutation.

---

## 15) Closing

Bulk data design is mostly about choosing what must stay stable and what may move.

- If iteration speed is king, keep data dense.
- If stable identity is king, keep slots stable and add indirection.
- If both matter, use hybrid approaches intentionally.

The right design is not the fanciest container — it is the one that matches your dominant access pattern and failure tolerance.
