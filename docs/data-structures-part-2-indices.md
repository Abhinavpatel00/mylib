# Data Structures Part 2: Indices (Comprehensive Rewrite)

Part 1 solved storage. Part 2 solves retrieval.

A large unsorted array is excellent for updates and iteration, but terrible for selective queries.
This article shows how to add **indices** so queries become fast without abandoning bulk-data layout.

---

## 1) Query problem statement

Suppose we store all observations as one big array:

```c
typedef struct entity_t entity_t;

typedef struct {
    entity_t *observed;
    entity_t *observer;
} observation_t;

observation_t *observations;
uint32_t num_observations;
```

Typical queries:

1. Notify all observers of entity `A`
   - `SELECT * WHERE observed == A`
2. Remove all links related to entity `B`
   - `SELECT * WHERE observed == B OR observer == B`

Brute-force scan:

```c
for (uint32_t i = 0; i < num_observations; ++i) {
    if (observations[i].observed == A) {
        notify(observations[i].observer);
    }
}
```

Complexity is O(n) per query. Works for tiny sets, degrades with scale.

---

## 2) What an index is in this context

An **index** is auxiliary data maintained alongside main storage to accelerate lookups.

You pay:
- extra memory
- update cost on insert/delete/modify

You gain:
- faster query time

For game/runtime systems, equality lookups dominate, so hash-based indexing is usually preferable to sorted-tree indexing.

---

## 3) Single-result hash index (`K -> V`)

If key is unique (or treated as unique), map key to one row.

```c
typedef struct hash64_to_u32_t hash64_to_u32_t;

hash64_to_u32_t by_observed; // key(entity*) -> observation index

observation_t *find_by_observed(entity_t *e)
{
    uint64_t key = (uint64_t)e;
    uint32_t idx = hash_get(&by_observed, key, UINT32_MAX);
    if (idx == UINT32_MAX)
        return 0;
    return observations + idx;
}
```

### Key notes

- If key already fits in 64 bits (`pointer`, ID), no extra hash of struct needed.
- If key is larger (e.g. string), hash input first.
- Keep table values as indices into bulk arrays (compact, easy relocation handling).

---

## 4) Maintaining index correctness

Indices are only useful if always updated with source-of-truth array.

```c
void add_observation(const observation_t *o)
{
    uint32_t idx = append_to_bulk_array(o);
    hash_add(&by_observed, (uint64_t)o->observed, idx);
}

void remove_observation(uint32_t idx)
{
    observation_t *o = observations + idx;
    hash_remove(&by_observed, (uint64_t)o->observed);
    remove_from_bulk_array(idx);
}
```

### Invariants to enforce

1. Every live row has valid entries in all required indices.
2. No index entry points to dead row.
3. Row mutation updates old key path and new key path.

Violation of any invariant leads to heisenbugs.

---

## 5) One-to-many query (`K -> set<V>`) and why naïve approaches fail

Real query is typically `K -> many rows`.

### Naïve A: map to vector-of-indices

```text
A -> [5, 19, 22, ...]
```

Works functionally, but often causes:

- thousands of tiny allocations
- fragmentation
- bad allocator/cache behavior
- hard-to-predict mutation costs

### Naïve B: unordered multimap

Insertion/lookup are fine, but delete-by-value can degrade:

```text
remove specific row r from key K
-> find all values for K
-> scan bucket to locate r
```

If `m` rows share the same key, delete is O(m). Repeated removals can create O(n²)-like workloads.

---

## 6) Practical one-to-many index: hash to first node + intrusive linked rings

Use a hybrid:

1. Hash table maps `K -> one member of set`.
2. Rows in same set are linked by intrusive `next/prev` pointers.

```c
typedef struct {
    entity_t *observed;
    entity_t *observer;
    uint32_t prev_by_observed;
    uint32_t next_by_observed;
} observation_t;

hash64_to_u32_t first_by_observed; // key -> any row index in set
```

### Topology

Circular doubly linked list per key:

```text
key A -> i7
i7 <-> i13 <-> i42 <-> (back to i7)
```

Circular form avoids null-edge special cases.

### Query path

1. Find `first = hash_get(key)` O(1)
2. Walk ring until back to `first` O(m)

Exactly proportional to number of matches.

---

## 7) O(1) insertion/removal mechanics

### Insert row into key ring

```c
static void ring_insert_after(observation_t *rows, uint32_t a, uint32_t b)
{
    uint32_t c = rows[a].next_by_observed;
    rows[b].prev_by_observed = a;
    rows[b].next_by_observed = c;
    rows[a].next_by_observed = b;
    rows[c].prev_by_observed = b;
}
```

If key has no ring yet, node points to itself.

### Remove row from key ring

```c
static void ring_remove(observation_t *rows, uint32_t i)
{
    uint32_t p = rows[i].prev_by_observed;
    uint32_t n = rows[i].next_by_observed;
    rows[p].next_by_observed = n;
    rows[n].prev_by_observed = p;
}
```

Hash head update rules:

- if removed node is current head and ring had more nodes: head = next
- if removed node was only node: remove hash entry

All O(1).

---

## 8) Cache behavior reality check

Linked traversal is pointer chasing. Rows may be far apart physically.

```text
ring order: i9 -> i2001 -> i17 -> i45000
memory jumps on each hop
```

But query consumers usually need those exact rows anyway, so this is often acceptable.

### Alternative: clustered physical ordering

If you physically cluster rows by key, query iteration becomes contiguous and cache-friendly, but:

- row moves become common
- pointer/index stability weakens
- maintenance complexity rises

Most runtime systems prefer stable identities over perfect clustering.

---

## 9) Multiple indices on same rows

You usually need multiple query paths:

- by `observed`
- by `observer`
- maybe by state/resource/type

Each independent index needs its own link fields.

```c
typedef struct {
    entity_t *observed;
    entity_t *observer;

    uint32_t prev_by_observed;
    uint32_t next_by_observed;

    uint32_t prev_by_observer;
    uint32_t next_by_observer;
} observation_t;
```

### Decoupled variant

Keep link pointers in parallel arrays to avoid bloating payload struct.

```c
typedef struct {
    uint32_t prev;
    uint32_t next;
} link_pair_t;

typedef struct {
    hash64_to_u32_t first;
    link_pair_t *links; // one per row
} index_t;
```

This scales better when indices are numerous or dynamically enabled.

---

## 10) Spatial queries with fuzzy hashing

Range queries in 2D/3D are different from exact-key lookups.

### Grid quantization

Choose cell size `s` and map position `p` to integer cell coordinates:

$$
cell_x = \lfloor p_x / s \rfloor, \quad cell_y = \lfloor p_y / s \rfloor, \quad cell_z = \lfloor p_z / s \rfloor
$$

Then hash `(cell_x, cell_y, cell_z)` as key.

```c
uint64_t key2(int64_t ix, int64_t iy)
{
    uint64_t a = (uint64_t)ix * 0x9E3779B185EBCA87ULL;
    uint64_t b = (uint64_t)iy * 0xC2B2AE3D27D4EB4FULL;
    return a ^ (b + (a << 6) + (a >> 2));
}
```

### Query neighborhood

For radius `r`, you must test all overlapping cells, not just home cell.

In `d` dimensions, a one-cell-thick neighborhood already grows roughly as $3^d$; this is manageable in 2D/3D, less so in high dimensions.

### Post-filtering

Cell membership is broad phase. Always confirm exact distance:

```c
if (distance_sq(candidate->pos, q) <= r*r)
    accept(candidate);
```

---

## 11) Choosing cell size for fuzzy hashing

Use occupancy-guided sizing rather than “match every query radius exactly”.

Good heuristic:

- tune cell size so typical neighborhood query touches a small, bounded candidate count (e.g., 8–32 objects)

If query radius is much larger, more cells are touched but true result size is also larger anyway. Candidate work often scales with density first, cell size second.

---

## 12) Complexity summary

Let:
- `n` = total rows
- `m` = rows for one key

Operations with hash + intrusive ring:

- insert row: O(1)
- remove row by handle/index: O(1)
- find first for key: O(1)
- iterate all matches: O(m)
- full scan fallback: O(n)

This is usually the best practical profile for runtime entity relationships.

---

## 13) Failure modes and hardening checklist

1. **Forgot to update one index on mutation**
   - Add centralized mutation API and debug assertions.

2. **Ring corruption (bad prev/next)**
   - Add optional validation pass in debug builds.

3. **Swap-delete row move not reflected in indices**
   - Prefer stable slots or perform full move-fix callbacks.

4. **Hash key collisions for custom packed keys**
   - Keep equality check where necessary.

5. **Mixed lifetime of pointers used as keys**
   - Use stable IDs where pointer lifetime is uncertain.

---

## 14) Recommended baseline architecture

For high-performance mutable sets with fast selective queries:

1. Keep authoritative rows in bulk array storage from Part 1.
2. For each query dimension, add:
   - `hash key -> first row`
   - intrusive doubly linked ring fields on rows (or parallel link arrays)
3. Update all indices in one central add/remove/mutate path.
4. Use debug validators liberally.

This gives deterministic O(1) mutation and excellent lookup behavior without nested tiny containers.

---

## 15) Closing

Indices are not “database-only ideas.” They are the practical bridge between:

- cache-friendly bulk storage
- and fast game/runtime queries

The key insight is simple:

> Store data once in bulk form, then add small, purpose-built index structures that let you reach exactly the subset you need.

Part 3 then tackles the last major gap: data records with dynamically sized fields (arrays-of-arrays and strings).
