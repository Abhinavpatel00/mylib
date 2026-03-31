# Minimalist Container Library in C (Part 2, Comprehensive Rewrite)

## Abstract

Part 1 established a minimal container core: dynamic arrays (stretchy buffers) + hash tables.
This part shows how to construct richer structures from that core without introducing a large generic container framework.

The guiding principle is:

- Keep foundational abstractions small.
- Encode workload-specific behavior in local, explicit data structures.
- Prefer transparent memory layouts over convenience-heavy APIs.

---

## 1) First question: do you need a dynamic container at all?

Many systems default to dynamic allocation when a fixed-capacity array would be simpler and faster.

```c
enum { MAX_ITEMS = 64 };
uint32_t num_items = 0;
item_t items[MAX_ITEMS];
```

### Why fixed capacity can be superior

- stack allocation is possible,
- no heap churn,
- no relocation surprises,
- straightforward cache behavior,
- trivial serialization in many cases.

### Where this applies

- bounded UI lists,
- fixed-size worker pools,
- protocol-defined limits,
- short-lived scratch collections.

### Limitations

- You must choose and enforce boundaries.
- Incorrect assumptions can become production bugs.
- Public APIs interacting with unbounded external inputs often cannot rely on fixed caps.

### Boundary rule

If you define the API, you can define practical limits.
If an external system defines behavior as unbounded, your API likely must cope dynamically.

---

## 2) Fixed array idioms: explicit and fast

Common operations are tiny and clear in plain C:

```c
// push_back
items[num_items++] = x;

// pop_back
--num_items;

// swap-erase (unordered remove)
items[idx] = items[--num_items];
```

These one-liners are not "tricks"; they are standard systems-programming idioms.

### Iterator-style loop in C

```c
for (item_t *it = items; it != items + num_items; ++it) {
    // use *it
}
```

This style is explicit about memory traversal and pointer boundaries.

---

## 3) Minimal fixed hash tables

When capacity is known, a fixed-size open-addressing table can be enough.

```c
static const uint64_t HASH_UNUSED = 0xffffffffffffffffULL;

typedef struct hash32_static_t {
    uint64_t *keys;
    uint32_t *values;
    uint32_t n;
} hash32_static_t;

static inline void hash32_static_clear(hash32_static_t *h) {
    memset(h->keys, 0xff, sizeof(*h->keys) * h->n);
}

static inline void hash32_static_set(hash32_static_t *h, uint64_t key, uint32_t value) {
    uint32_t i = (uint32_t)(key % h->n);
    while (h->keys[i] != key && h->keys[i] != HASH_UNUSED)
        i = (i + 1) % h->n;
    h->keys[i] = key;
    h->values[i] = value;
}

static inline uint32_t hash32_static_get(const hash32_static_t *h, uint64_t key) {
    uint32_t i = (uint32_t)(key % h->n);
    while (h->keys[i] != key && h->keys[i] != HASH_UNUSED)
        i = (i + 1) % h->n;
    return h->keys[i] == HASH_UNUSED ? 0 : h->values[i];
}
```

### Why this works

- constant-size table means no resize logic,
- linear probing keeps memory contiguous,
- implementation can stay tiny and inlinable.

### Limitations

- probe chains degrade as load factor grows,
- capacity must be provisioned correctly,
- deletion handling requires tombstones or rebuild policy.

---

## 4) Strings in a minimalist C architecture

The approach here is pragmatic:

1. Use immutable/static strings where possible (`const char *`).
2. Use hashed identifiers in runtime hot paths.
3. Use dynamic string construction only at boundaries (paths, logs, messages, formatting).

### Temporary formatting over persistent ownership

Instead of building lots of heap-managed string objects:

```c
char *p = tm_temp_allocator_api.printf(ta, "%s/%s", dir, file);
open_file(p);
```

### Why this can be better than object-heavy string APIs

- explicit lifetime through allocator scope,
- fewer persistent allocations,
- often stack-backed temporary storage,
- no hidden copies from operator overloading.

### Limitation

You must not persist pointers past allocator lifetime.
This is powerful but requires lifetime discipline.

---

## 5) Append-style dynamic strings with stretchy buffers

For incremental string building, append formatted chunks into `char` array storage.

```c
static void array_vprintf(char **a, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    int32_t n = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);

    uint32_t an = array_size(*a);
    array_ensure(*a, an + (uint32_t)n + 1);
    vsnprintf(*a + an, (size_t)n + 1, fmt, args);
    array_resize(*a, an + (uint32_t)n);
}

static void array_printf(char **a, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    array_vprintf(a, fmt, args);
    va_end(args);
}
```

### Important mechanics

- `va_copy` is required because `va_list` is consumable.
- Reserve space for terminating `\0`, but keep logical size excluding it.
- Result behaves as both dynamic array and C string.

ASCII state after each append:

```text
logical bytes: [H][e][l][l][o]
backing bytes: [H][e][l][l][o][\0]
logical size = 5, capacity >= 6
```

---

## 6) Arrays of arrays in plain C

Stretchy buffers treat elements as raw bytes—there are no constructors/destructors.
So nested ownership must be explicit.

```c
typedef struct item_t {
    int32_t *ints;  // nested stretchy buffer
} item_t;

item_t *items = NULL; // outer stretchy buffer
```

Before freeing `items`, free each inner buffer:

```c
for (item_t *it = items; it != items + array_size(items); ++it)
    array_free(it->ints);
array_free(items);
```

### Why this model remains viable

If your runtime tracks allocations and reports leaks with source location, manual cleanup errors become highly detectable.

### Limitations

- Requires explicit teardown discipline.
- Complex ownership trees need clear conventions.
- Error paths must also clean up correctly.

---

## 7) Arrays of immutable strings: packed-block design

A high-value optimization: pack string bytes in one contiguous block and store pointers (or offsets) into that block.

```text
strings[] (index)          block (bytes)
+-------+--------+         +--------------------------------+
| [0] --+------->|-------> "alpha\0beta\0gamma\0..."       |
| [1] --+----+   |         +--------------------------------+
| [2] --+--+ |   |
+-------+--|-+---+
           | |
           v v
         offsets into block
```

Possible shape:

```c
typedef struct array_of_strings {
    char *block;        // byte storage
    uint64_t used;      // used bytes in block
    char **strings;     // index of string starts
} array_of_strings;
```

Push operation sketch:

```c
void push_string(array_of_strings *a, const char *s)
{
    uint64_t n = (uint64_t)strlen(s) + 1;
    array_ensure(a->block, a->used + n);
    array_push(a->strings, a->block + a->used);
    memcpy(a->block + a->used, s, (size_t)n);
    a->used += n;
}
```

### Why it works

- avoids per-string allocator overhead,
- improves locality for string payloads,
- reduces over-allocation compared to per-string dynamic buffers.

### Pointer vs offset indexing

If `block` can reallocate, stored pointers become invalid.
Use **offsets** instead when growth relocates memory.

```text
store: uint32_t offset
resolve: ptr = block + offset
```

---

## 8) Mutation and garbage in packed string blocks

If strings are replaced/removed, old bytes become garbage.

Two strategies:

1. **Allocator-like reuse** inside block (free lists by span size).
2. **Compaction/copying GC** when garbage ratio passes threshold.

Compaction sketch:

```text
old block: [alive][garbage][alive][garbage][alive]
copy alive spans --> new block contiguous
rewrite string index entries to new offsets
swap blocks, free old
```

This is often enough for configs, metadata, and document-style stores where string churn is moderate.

---

## 9) String interning integration

Combine packed string storage with hash lookup:

- hash string content,
- if present: return existing handle,
- else append bytes to block and register handle.

Benefits:

- deduplicates repeated keys/values,
- speeds equality checks (handle compare),
- often reduces memory sharply for structured text formats.

This is especially effective for JSON-like datasets with repeated property names.

---

## 10) Lists and trees via index-based nodes in arrays

Instead of pointer-linked heap nodes, store nodes in arrays and use indices.

```c
typedef struct node_t {
    uint32_t prev;
    uint32_t next;
    item_t data;
} node_t;

node_t *nodes = NULL; // stretchy buffer
```

### Circular list with dummy head

Use sentinel node at index `0`.

```text
empty list:
head(0).next = 0
head(0).prev = 0

non-empty:
head <-> a <-> b <-> c <-> head
```

Why this is excellent:

- no null special cases,
- uniform insert/remove logic,
- fewer branches and edge bugs.

### Front insert sketch

```c
uint32_t head = 0;
uint32_t n = alloc_node(nodes);

nodes[n].next = nodes[head].next;
nodes[n].prev = head;
nodes[nodes[n].next].prev = n;
nodes[head].next = n;
```

### Remove sketch

```c
nodes[nodes[n].next].prev = nodes[n].prev;
nodes[nodes[n].prev].next = nodes[n].next;
```

---

## 11) Reclaiming removed nodes

Removal creates holes in node arrays.

### Option A: freelist inside same array

Reserve another sentinel (e.g., index `1`) to manage free nodes.

```text
nodes[0] = active-list sentinel
nodes[1] = free-list sentinel
```

On remove: unlink from active list, push to free list.
On alloc: pop from free list, else grow array.

### Option B: swap-with-last compaction

Move last node into hole and patch references.

Use only if external handles can tolerate index changes (or if you have a remap layer).

---

## 12) Trees as index graphs

Trees can be stored exactly the same way:

- node payloads in array,
- child/parent/sibling fields as indices,
- free node management via freelist.

### Why this can beat pointer trees

- fewer tiny allocations,
- denser memory for traversal,
- easier serialization and snapshotting.

### Limitation

If frequent structural edits require stable external node references, you need stable handles and possibly indirection tables.

---

## 13) When to build specialized structures

A specialized structure is justified when generic array/hash cannot express workload constraints without significant waste.

Examples:

- append-only snapshots where old readers must stay valid,
- huge logical arrays where contiguous reallocation hitches are unacceptable,
- fixed-memory ring buffers for telemetry/logging,
- lock-free producer-consumer pipelines with strict latency bounds.

### Core rule

Do not build “generic specialized containers.”
If a structure is truly specialized, keep it local and explicit to that subsystem.

This avoids creating another broad abstraction family that reintroduces the same maintenance burden minimalism tries to eliminate.

---

## 14) Engineering discipline required by this approach

Minimal container stacks work best with these practices:

- allocation tracking with leak reports,
- clear ownership conventions,
- explicit lifetime scopes (especially temp allocators),
- profiling that validates locality assumptions,
- aggressive assertions around index validity and sentinel invariants.

Without those, manual memory and index-based composition can become brittle.

---

## 15) Comparative perspective

### Compared to template-heavy generic container ecosystems

**Strengths**

- lower conceptual overhead in core runtime,
- transparent memory behavior,
- easier targeted optimization,
- smaller foundational surface area.

**Weaknesses**

- less ergonomic for one-off coding,
- more explicit lifetime/cleanup code,
- fewer compile-time guarantees about ownership semantics.

The right choice depends on team skills, domain constraints, and operational goals.

---

## 16) Summary of key insights

1. **Fixed-size structures are often enough** and can be superior when bounds are real.
2. **Dynamic strings are a boundary concern**, not a universal runtime primitive.
3. **Packed string blocks + indexing** outperform per-string heap objects in many immutable workloads.
4. **Index-based node graphs** are a practical replacement for pointer-heavy lists/trees.
5. **Specialization is local by design**—optimize where workload justifies it, not globally by default.

---

## 17) Practical checklist for applying Part 2 patterns

- Confirm whether bounds are truly fixed before using dynamic containers.
- For dynamic strings, decide between temporary formatting and append-buffer building.
- For arrays-of-strings, choose pointer or offset index based on relocation behavior.
- For index-based lists/trees, define sentinel layout and free-node policy up front.
- Add invariants/tests for index validity, freelist correctness, and compaction behavior.

---

## Closing

The key lesson is not “arrays and hashes solve everything.”
The lesson is that a small, explicit core plus deliberate local specialization can outperform a large generic container framework in systems code—both technically and organizationally—when you care deeply about memory behavior, observability, and long-term changeability.
