# Minimalist Container Library in C (Part 1, Comprehensive Rewrite)

## Abstract

This article explains a minimalist strategy for container design in C: keep only two general-purpose dynamic containers in your core library—dynamic arrays and hash tables—and build everything else from those primitives or from small, local, purpose-built structures.

The central claim is not that “fewer containers are always better.” The claim is that in engine/middleware code, reducing foundational abstractions often improves long-term adaptability, performance transparency, and debugging leverage.

---

## 1) Why minimalism matters in a systems codebase

### The practical argument

Every foundational abstraction becomes part of your organization’s long-term maintenance surface:

- People must learn it.
- Tooling must understand it.
- Bugs must be diagnosed through it.
- Performance must be profiled across it.
- Ports and platform bring-up must preserve it.

A broad container catalog (lists, trees, deques, ropes, small-vectors, etc.) can be useful, but it multiplies these costs.

### Why this matters more for engine/platform teams

Application code can optimize for “feature shipped quickly.”
Engine/platform code often must optimize for “unknown future use cases by unknown downstream users.”

That shifts priorities:

- **Predictability over convenience**.
- **Simple internals over broad APIs**.
- **Control of memory behavior over abstraction breadth**.

### External dependency caution (not absolutism)

The minimalist stance does **not** imply “never use libraries.”
It implies “prefer dependencies whose behavior you can fully reason about under your constraints.”

Small, focused source libraries can fit this model.
Large, policy-heavy frameworks often do not.

---

## 2) The minimal core: arrays + hash tables

A surprisingly large fraction of real runtime data can be represented with:

1. **Dynamic contiguous storage** (array / stretchy buffer)
2. **Fast key lookup** (hash table)

Everything else is either:

- an adaptation of these two, or
- genuinely specialized enough to deserve a custom local structure.

### Where this applies

- Runtime registries
- Asset metadata tables
- ECS-style component side-data
- Command buffers
- Name lookup maps
- Sparse object catalogs

### Limitations

- You must be comfortable building small local data structures.
- You lose some out-of-the-box ergonomics offered by generic template libraries.
- You need discipline around ownership and lifetime, especially in plain C.

---

## 3) Stretchy buffers in C: the core mechanism

A stretchy buffer stores metadata in a hidden header located directly before the element data.
The user-facing value remains a normal typed pointer.

```c
my_type_t *a = NULL;
```

`NULL` means empty array.
When allocated, memory looks like this:

```text
+----------------------+------------------------------+
| array_header_t       | elements[0..capacity-1]      |
| size, capacity, ...  | (typed as my_type_t*)        |
+----------------------+------------------------------+
^                      ^
|                      |
base allocation ptr    a (what user holds)
```

### Typical header

```c
typedef struct array_header_t {
    uint32_t size;
    uint32_t capacity;
} array_header_t;
```

### Header recovery

```c
#define array_header(a) ((array_header_t *)((char *)(a) - sizeof(array_header_t)))
#define array_size(a)   ((a) ? array_header(a)->size : 0)
#define array_capacity(a) ((a) ? array_header(a)->capacity : 0)
```

### Push operation sketch

```c
#define array_full(a) ((a) && array_header(a)->size == array_header(a)->capacity)

#define array_push(a, item)                                      \
    do {                                                         \
        if (!(a) || array_full(a))                              \
            (a) = array_grow((a), sizeof(*(a)));                \
        (a)[array_header(a)->size++] = (item);                  \
    } while (0)
```

> In production code, prefer `do { ... } while (0)` style macros to avoid precedence/control-flow surprises.

---

## 4) Why this technique works

### Type preservation without templates

The pointer remains `T *`, so reads/writes stay type-checked by the compiler.
No `void *` user-side casts are needed for normal operations.

### Good locality

Header and data are adjacent; elements are contiguous.
This improves sequential scan performance and generally helps cache behavior.

### Zero-initialization friendliness

Because `NULL` is valid empty state, container fields in larger structs can often be initialized with `{0}` and become immediately usable.

---

## 5) Hidden costs and correctness constraints

### 1) Reallocation invalidates pointers

Any growth can move the allocation. Therefore:

- never store long-lived pointers into array elements unless you can guarantee no growth,
- prefer storing indices for external references.

### 2) Macro ergonomics vs debuggability

Macros provide pseudo-generic behavior in C, but:

- stack traces are noisier,
- stepping in debuggers is less direct,
- side-effect bugs become easier to introduce.

### 3) Distinguishing stretchy pointers from ordinary pointers

`char *x` might be plain memory or stretchy buffer data.
That ambiguity is real operationally (debugging, code review, API contracts).

Useful conventions:

- naming (`items_sb`, `names_arr`),
- API boundary rules (“only freed via array_free”),
- optional wrapper structs for public interfaces.

### 4) Alignment and ABI details

The header layout must preserve element alignment. Common solutions:

- place padding in header,
- allocate with alignment that satisfies `max(alignof(header), alignof(T))`,
- or store a header pointer just before data with an aligned data start.

In simple C99 contexts, ensure your allocator and header placement cannot violate alignment for expected element types.

---

## 6) Growth policy and complexity model

Suppose capacity doubles on overflow.

- Push is **amortized O(1)**.
- Single push on growth boundary is **O(n)** due to copy.
- Iteration is **O(n)** and highly cache-friendly.

ASCII growth timeline:

```text
size/cap: 0/0
push -> alloc cap=8
size/cap: 1/8 ... 8/8
push 9th -> realloc cap=16, copy 8
size/cap: 9/16 ...
```

Choosing growth factor:

- 2.0x: fewer reallocations, more slack memory
- 1.5x: tighter memory, more frequent copies

Pick based on allocation pressure vs throughput sensitivity.

---

## 7) Hash table simplification: hash keys before insertion

In many real systems, source keys are strings but runtime usage is mostly lookup by key—not key enumeration.

Instead of storing full variable-length keys in the table, store stable hashed keys (e.g., 64-bit).

```text
source key (string) --hash--> uint64_t key_id --lookup--> value/index
```

This removes variable-length key storage from the hash table itself.

### Why this works

- Fixed-width keys simplify probing/storage.
- Fewer allocations and less pointer chasing.
- Better data density and easier copying.

### Collision model

This design assumes collision probability is acceptably low for your key volume and hash width.

For 64-bit hashes and typical engine-scale tables, collision risk is usually negligible in practice, but never mathematically zero.

If your correctness model cannot tolerate any collision risk:

- use wider hashes (e.g., 128-bit),
- or keep collision-resolution payload (e.g., secondary fingerprint or original key storage).

---

## 8) Value indirection: store indices, not payload objects

Instead of storing heavy values in the hash table:

- store values in a dense array,
- store array indices in the hash table.

```text
hash key (u64) ---> index (u32/u64) ---> values[index]
```

This frequently improves memory efficiency because hash tables are sparse by design (they contain holes).

### Example shape

```c
typedef struct hash_t {
    uint32_t num_buckets;
    uint64_t *keys;    // hashed keys
    uint64_t *values;  // usually indices
} hash_t;
```

```c
uint64_t hash_lookup(const hash_t *h, uint64_t key, uint64_t default_value);
```

### Example usage

```c
hash_t info_lookup = {0};
info_t *info = NULL; // stretchy array

array_push(info, my_info);
hash_add(&info_lookup, hash_string("my name"), array_size(info) - 1);
```

---

## 9) Deletion strategies with index-backed values

If hash values are indices into `values[]`, deletion needs policy.

### Strategy A: freelist (stable indices)

- Mark deleted slots reusable.
- Reinsertions reuse holes.
- External indices remain valid.

**Pros**: stable handles
**Cons**: potential fragmentation, more bookkeeping

### Strategy B: swap-with-last (dense packing)

- Move last element into deleted slot.
- Update moved element’s hash mapping.

**Pros**: dense array, excellent iteration locality
**Cons**: indices/handles change unless externally abstracted

Pick based on handle stability requirements.

---

## 10) Why this two-container model scales surprisingly far

### It enforces useful design pressure

When a problem appears to “need a special container,” you must ask:

- Is this truly a structural requirement?
- Or can array + hash plus a small indexing layer solve it?

That pressure removes accidental abstraction and often yields clearer data flow.

### It encourages data-oriented layout

- contiguous arrays for traversal-heavy paths,
- hash lookups only at edges,
- explicit index relations instead of nested pointer graphs.

### It keeps core APIs tiny

A small foundation is easier to teach, audit, port, and optimize.

---

## 11) Known limitations and when to break minimalism

Use specialized structures when the generic pair is a poor fit:

- strict ordering with many middle inserts/deletes,
- workload-specific lock-free constraints,
- very large data where contiguous growth hitches are unacceptable,
- latency-critical pipelines requiring bounded worst-case operations.

Minimalism is a default, not a law.

---

## 12) Practical implementation checklist

If you build this in C, validate these points early:

- Header alignment is correct for all element types you store.
- `array_grow()` handles `NULL` and preserves old data.
- APIs define ownership and free paths clearly.
- No code stores stale pointers across possible growth.
- Hash table defines collision behavior and sentinel values.
- Deletion policy is explicit and tested.

---

## 13) Summary of key insights

1. **Small foundation, broad leverage**: arrays + hash tables cover most runtime needs.
2. **Stretchy buffers provide pseudo-generics in C** while preserving typed pointers.
3. **Pre-hashing keys and indexing values** can drastically simplify hash-table design.
4. **Deletion/index policy is architectural**, not incidental—decide it deliberately.
5. **Minimalism works when paired with explicit constraints** and strong engineering discipline.

---

## 14) Suggested follow-up topics

Part 2 extends this approach to higher-level structures built from the same primitives:

- fixed-capacity containers,
- strings and append buffers,
- arrays-of-arrays and arrays-of-strings,
- list/tree representations via index-based nodes,
- when to write specialized local structures.
