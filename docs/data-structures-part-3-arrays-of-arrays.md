# Data Structures Part 3: Arrays of Arrays (Comprehensive Rewrite)

Parts 1 and 2 covered fixed-size records and indexing them efficiently.

Now we address the hard case:

> What if each record contains variable-size data (child arrays, names, tags, paths, payload blobs)?

This is where many systems accidentally regress into allocator-heavy, fragmented, hard-to-profile designs.

---

## 1) Why this is a separate problem

Fixed-size bulk arrays work because every row has the same footprint.

```c
typedef struct {
    float x, y, z;
    uint32_t flags;
} object_t;
```

But variable-size fields break that:

```c
typedef struct {
    const char *name;      // dynamic string
    uint32_t num_children;
    uint32_t *children;    // dynamic array
} object_t;
```

Now each object can allocate separately, which introduces:

- allocator overhead per object
- fragmentation
- pointer chasing
- opaque memory usage

---

## 2) Strategy 1: cap sizes (most underrated option)

If domain permits hard limits, flatten variable fields into fixed slots.

```c
enum { MAX_NAME = 63, MAX_CHILDREN = 8 };

typedef struct {
    char name[MAX_NAME + 1];
    uint32_t num_children;
    uint32_t children[MAX_CHILDREN];
} object_t;
```

### Benefits

- returns to fixed-size bulk-data world
- zero per-object heap allocations
- excellent locality and predictability
- simplest debugging

### Costs

- memory waste from overprovisioning
- limit handling behavior must be defined

### Decision heuristic

Use caps if:

- limits are naturally small
- failure policy is easy/acceptable
- compatibility constraints are local (not long-lived external format)

---

## 3) Strategy 2: per-object heap allocations (baseline fallback)

Typical C++ model:

```c
typedef struct {
    std::string name;
    std::vector<uint32_t> children;
} object_t;
```

With 10k objects, this may imply tens of thousands of small allocations.

### Practical drawbacks at scale

- allocator metadata overhead becomes visible
- external fragmentation grows
- locality degrades (object core vs payload far apart)
- analysis is hard (memory scattered across many blocks)

Still acceptable for moderate sizes or tooling code where engineering simplicity wins.

---

## 4) Strategy 3: private heap/arena for this system

Allocate a big region and sub-allocate internally.

```text
VM reserve/alloc large block
 -> custom allocator serves object-owned arrays/strings
```

### Why this helps

- reduces system allocator calls
- centralizes accounting
- allows workload-specific policies

### Still unsolved by itself

If allocation sizes vary wildly, you still face fragmentation unless allocator policy handles it.

---

## 5) Strategy 4: chunked arrays-of-arrays (fixed-size chunk allocator)

Key idea: convert variable-length arrays into linked lists of **fixed-size chunks**.

All chunk allocations have identical size, so free-list reuse is perfect.

```c
enum { CHILD_CHUNK_SIZE = 16 };

typedef struct child_chunk_t {
    uint32_t child[CHILD_CHUNK_SIZE];
    uint32_t prev_chunk;
    uint32_t next_chunk;
} child_chunk_t;

typedef struct {
    const char *name;
    uint32_t num_children;
    uint32_t first_chunk; // index into chunk pool, UINT32_MAX = none
} object_t;
```

### Traversal model

```text
object -> chunk0 -> chunk1 -> chunk2 ...
```

Each chunk contributes up to `CHILD_CHUNK_SIZE` elements.

### Allocation model

- chunk pool is bulk array with free list (Part 1 pattern)
- no external fragmentation inside chunk pool

---

## 6) Chunked arrays performance profile

### Strengths

- O(1) chunk allocate/free
- stable chunk references
- predictable memory model
- allocator transparency
- no variable-size free-list classes needed

### Weaknesses

- random access by global child index is no longer O(1) unless extra metadata exists
- crossing chunk boundaries may miss cache
- internal fragmentation in tail chunk

Internal waste for one array of length `L`:

$$
waste = (C - (L \bmod C)) \bmod C
$$

where $C$ is chunk size.

Expected waste depends on distribution of `L`.

---

## 7) Picking chunk size scientifically

Tradeoff:

- larger chunk: fewer links, better sequential locality, more tail waste
- smaller chunk: less tail waste, more pointer chasing, more link overhead

### Useful baseline

Choose chunk so one chunk approximates cache line multiples.

Example with 32-bit indices and two link indices:

```text
chunk bytes = 4*C + 4 + 4 = 4C + 8
```

For 64-byte target:

$$
4C + 8 = 64 \Rightarrow C = 14
$$

Hence `C=14` is a common sweet spot.

---

## 8) Degenerate case `C = 1` and relation model

If chunk size is 1:

```text
parent -> first_child
child  -> next_sibling / prev_sibling
```

This becomes classic sibling linked lists.

Equivalent relational representation:

```text
(parent, child) rows
```

Then reusing Part 2 indices can make certain operations (remove specific child link, reverse lookups) O(1).

### Array vs relation

- explicit arrays: less overhead, straightforward parent->children iteration
- relation rows + indices: more flexible query patterns and deletions

---

## 9) Strings are not “just arrays of char” in practice

Most application strings behave as immutable identifiers:

- compare
- lookup
- serialize
- display

Not random per-character insert/delete operations.

This suggests specialized handling:

1. mutable temporary buffer while editing/building
2. immutable stored representation after commit

---

## 10) String interning model

Interning stores only one copy of each unique string.

```text
"properties" encountered 1000 times -> one stored byte sequence + 1000 references
```

### Repository structure

```c
typedef struct {
    uint32_t size;
    char *buffer;             // packed string bytes
    hash64_to_u32_t by_hash;  // hash -> offset in buffer
} string_repo_t;
```

### Basic intern operation

1. hash input string
2. lookup hash entry
3. on hit: verify equality, return existing pointer/handle
4. on miss: append bytes, insert mapping

```c
const char *intern(string_repo_t *r, const char *s)
{
    uint64_t h = hash_str(s);
    uint32_t off = hash_get(&r->by_hash, h, UINT32_MAX);
    if (off != UINT32_MAX && strcmp(r->buffer + off, s) == 0)
        return r->buffer + off;

    uint32_t at = r->size;
    uint32_t n = (uint32_t)strlen(s) + 1;
    ensure_capacity(r, r->size + n);
    memcpy(r->buffer + at, s, n);
    r->size += n;
    hash_add(&r->by_hash, h, at);
    return r->buffer + at;
}
```

### Immediate win

Pointer equality can stand in for string equality *when both operands are interned in same repository*.

---

## 11) Repository lifetime design

Global repo is possible, but scoped repos are often safer:

- per-system
- per-level
- per-document/asset parse

Then freeing all related strings is trivial: drop repo wholesale.

This dramatically simplifies lifetime management for transient workloads (e.g., parser passes).

---

## 12) Reclaiming interned string memory: three options

### Option A: never reclaim until repo death

Best simplicity; often sufficient if churn is low.

### Option B: free-list in string buffer

Treat deleted regions as holes, manage by size classes, merge adjacent holes.

Essentially re-implement heap allocation internally.

### Option C: compacting handles

Return string handles (IDs), not raw pointers. Move strings during compaction and update handle table.

Higher complexity, strongest long-run memory control.

---

## 13) Block compaction approach (practical compromise)

Store strings in fixed-size blocks (e.g., 4 KiB).
Track per block:

- live bytes
- hole bytes

When hole ratio exceeds threshold (e.g., >50%), repack live strings within block and reclaim contiguous tail space.

```text
Before: [live][hole][live][hole][live]
After : [live][live][live][free....]
```

Works best with handles or relocation-aware references.

---

## 14) Combined architecture for dynamic fields

A robust pattern for large systems:

1. **Object core** in fixed-size bulk array (Part 1)
2. **Variable arrays** via chunk pools (this part)
3. **Relations/indices** for flexible lookup/edit (Part 2)
4. **Strings** in intern repositories with explicit lifetime domain

This preserves the key properties:

- predictable allocator behavior
- mostly contiguous hot data
- explicit ownership/lifetime
- scalable mutation

---

## 15) Mutation API sketch (children)

```c
void child_list_push(object_store_t *os, chunk_store_t *cs,
                     uint32_t object_idx, uint32_t child_idx);

bool child_list_remove(object_store_t *os, chunk_store_t *cs,
                       uint32_t object_idx, uint32_t child_idx);

void child_list_foreach(const object_store_t *os, const chunk_store_t *cs,
                        uint32_t object_idx,
                        void (*fn)(uint32_t child_idx, void *ud),
                        void *ud);
```

Keep all writes through these APIs to preserve invariants.

---

## 16) Complexity summary

Let:
- `k` = children in one object list
- `C` = chunk size

Chunked explicit-array representation:

- append child: amortized O(1)
- iterate children: O(k)
- remove known-position child: O(1) local (plus bookkeeping)
- remove by value (linear scan): O(k)

Relational representation with proper index:

- add relation row: O(1)
- remove known row: O(1)
- enumerate children of parent: O(k)
- reverse lookups (child->parents): O(k2) for that key set

---

## 17) Pitfalls checklist

1. **Mixing raw string pointers across repositories**
   - Pointer equality becomes invalid across repos.

2. **Assuming hash hit implies string equality**
   - Always confirm bytes unless collision-resistant identity is guaranteed.

3. **Chunk leak on object deletion**
   - Ensure full chunk chain is returned to free list.

4. **Iterator invalidation during child mutation**
   - Define clear mutation rules in iteration APIs.

5. **Unbounded growth from “never reclaim” in high churn system**
   - Use scoped repo or periodic compaction policy.

---

## 18) Recommended defaults

If you want practical defaults that scale:

- Use chunked children (`C ≈ 14..32` depending on profile)
- Keep object core compact and fixed-size
- Intern immutable strings in scoped repositories
- Avoid per-object general heap allocations in hot data paths
- Add relational indices only where query flexibility demands it

---

## 19) Closing

Arrays-of-arrays is where data-oriented designs either stay predictable or fall back into opaque allocator behavior.

The core insight is:

> Represent variable-size data with fixed-size building blocks and explicit indexing/lifetime domains.

That keeps performance characteristics understandable, tunable, and robust under scale.
