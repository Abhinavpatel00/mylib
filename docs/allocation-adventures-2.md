# Allocation Adventures 2: Arrays of Arrays Without Allocation Chaos

This article deepens the original question:

How do you represent `array<array<T>>` in a data-oriented system when each inner array grows and shrinks dynamically?

Target case from the original post: per-entity tags.

```cpp
// Conceptual baseline, but allocation-heavy:
std::vector<std::vector<unsigned>> tags_per_entity;
```

Goal: preserve dynamic behavior while reducing allocation count, improving cache locality, and keeping mutation costs predictable.

## Problem Model

We have many entities. Each entity has 0..N tags (`unsigned` hash ids). We need to support:

- Add/remove tags.
- Enumerate tags of one entity.
- Eventually (outside this article), reverse lookup: entities by tag.

Key tension:

- Local updates are per-entity.
- Memory behavior is global across all entities.

Naive nested vectors often degenerate into many small heap allocations with poor spatial locality.

## Evaluation Criteria

A design is good only if it performs across these dimensions:

1. Allocation pressure: how many independent heap objects?
2. Traversal locality: sequential or pointer-chasing?
3. Mutation cost: append/remove complexity.
4. Memory overhead: metadata versus payload ratio.
5. Fragmentation behavior over long runs.

---

## Approach 1: Fixed Capacity Per Entity

```cpp
constexpr uint32_t MAX_TAGS = 8;

struct Tags {
    uint32_t count;
    uint32_t values[MAX_TAGS];
};

Array<Tags> data;
```

### Why it works

All data sits in one contiguous buffer. Access is trivial and branch-light.

### How it works

- Each entity owns an inline fixed array.
- Insert fails or truncates once `count == MAX_TAGS`.

### Where it applies

- Problems with hard natural bound (e.g., 4 neighbors in a grid cell).
- Strong project constraints known up front.

### Limitations

- Wasted space for sparse entities.
- Hard gameplay/design cap leaks into content pipeline.
- Hard to reuse across unrelated projects.

Memory utilization example (`MAX_TAGS=8`):

```text
avg tags/entity = 2  => 25% payload utilization
avg tags/entity = 4  => 50%
avg tags/entity = 7  => 87.5%
```

If utilization stays low, dynamic representation is usually better.

---

## Approach 2: Linked Lists Backed by a Single Node Pool

Instead of one allocation per entity-list, store all nodes in one global node array.

```cpp
struct Node {
    uint32_t tag;
    uint32_t next; // 0 = nil
};

Array<uint32_t> head_by_entity; // index into nodes, 0 = empty
Array<Node> nodes;
```

### Why it works

- Allocation count collapses to a few backing arrays.
- O(1) insert at head.

### How it works

- `nodes` is the storage pool.
- `head_by_entity[e]` points to first node in entity chain.
- `next` stores node index (offset) instead of pointer.

ASCII:

```text
head_by_entity:
E0 -> 12 -> 7 -> 21 -> 0
E1 -> 0
E2 -> 5 -> 9 -> 0

nodes array:
idx: 5   7   9   12  21
tag: A   C   B   D   E
```

### Nil value convention

Using `0` as nil has practical benefits:

- zero-initialized memory is valid empty state
- simple checks (`if (next) ...`)
- stable sentinel if index type changes

Cost: index 0 is unavailable as data node.

### Limitations

- Traversal may jump across memory and miss cache.
- 50%+ metadata overhead for small payload node (`tag + next`).

---

## Approach 2b: Chunked Linked Nodes

Increase payload per node to amortize pointer/index overhead.

```cpp
constexpr uint32_t TAGS_PER_NODE = 8;

struct Node {
    uint32_t count;
    uint32_t tags[TAGS_PER_NODE];
    uint32_t next;
};
```

### Why it works

One pointer hop yields multiple tags.

### Tradeoff geometry

- Bigger node: better locality per hop, worse slack when underfilled.
- Smaller node: less slack, more pointer hops.

Payload ratio (ignoring `count`):

```text
full node, TAGS_PER_NODE=8:
payload = 8 * 4 = 32 bytes
metadata = next (4) + count (4) = 8 bytes
payload ratio = 80%
```

But at average fill = 3 tags/node:

```text
payload = 12 bytes used out of 32 payload capacity
effective payload ratio ~= 30%
```

### Where it applies

- Tag counts are modest and clustered around node size.
- Append/remove frequency is moderate.

### Limitations

- Fragmentation at node granularity.
- More complex split/merge logic when inserting/removing mid-node.

---

## Locality Recovery by Chain Clustering

The original post notes an important idea: linked lists are only bad when links jump far.

If nodes belonging to one chain are physically adjacent, traversal becomes near-linear.

```text
nodes memory:
[A1][A2][A3][B1][C1][C2]

Chain A: A1->A2->A3
Chain C: C1->C2
```

### How to maintain this without full sort

A full sort after each mutation is too expensive. Use incremental maintenance:

- During each traversal, spend a small fixed relocation budget.
- Move recently used chains toward contiguous regions.
- Update `next` links and affected heads.

This is effectively online defragmentation with bounded per-frame cost.

### Limitation

Requires relocation-safe references (indices/handles). Raw external pointers break this approach.

---

## Approach 3: Custom Allocator Specialized for Pattern

A common objection is: "Isn't this rewriting `malloc` badly?"

The critical answer is scope.

Generic allocators optimize for unknown mixed workloads. A subsystem allocator can exploit known invariants and therefore be both simpler and faster.

Two high-value invariants from this problem:

1. No external pointers into inner-array storage.
2. Inner storage usually grows geometrically (vector-style capacity growth).

### Invariant 1: No external pointers

If only the component owns references, blocks are relocatable. That unlocks defragmentation and compaction.

### Invariant 2: Geometric growth

Dynamic arrays typically grow by factor `g` (`g=2` common).

For a vector that doubles capacity:

- Reallocation happens when size reaches capacity.
- Copy cost is bursty but amortized O(1) per push.

#### Why doubling gives amortized O(1)

Capacities: `1, 2, 4, 8, ..., 2^k`.

Total elements moved to reach size `n` is approximately:

```text
1 + 2 + 4 + ... + n/2 < n
```

So total extra copy work is O(n) for n pushes, hence O(1) amortized per push.

If you grow by a fixed constant `c` each time, reallocations remain O(n) in count and average push cost trends toward O(n).

### Real-time caveat

Amortized O(1) still contains occasional O(n) spikes. Real-time systems may need:

- capped migration per frame,
- segmented/chunked vectors,
- or reserve-before-use policies.

---

## Why This Naturally Leads to a Buddy Allocator

If buffers grow in powers of two and allocator serves power-of-two blocks efficiently, the match is structural.

Buddy allocator properties align well:

- split larger block into equal buddies
- merge free buddies quickly
- efficient power-of-two block management

This directly motivates the transition to the next article.

---

## Practical Design Matrix

```text
Option                    Locality     Alloc Count    Waste Pattern          Complexity
---------------------------------------------------------------------------------------
Fixed per-entity          Excellent    Minimal        Slack per entity        Low
Linked single-tag nodes   Poor-Mid     Minimal        Pointer overhead        Low-Mid
Linked chunked nodes      Mid-Good     Minimal        Slack per node          Mid
Custom relocatable alloc  Good         Minimal        Managed/compacted       High
```

No universally best structure exists; choose by workload shape.

---

## Worked Example: Tag Distribution Driven Choice

Assume 100k entities with histogram:

- 70% have 0-2 tags
- 25% have 3-6 tags
- 5% have 7-20 tags

Reasonable design:

1. Inline small buffer in entity (say 4 tags).
2. Overflow to pooled chunk nodes for tail cases.
3. Periodically compact overflow pool.

This hybrid keeps common case contiguous while avoiding global hard caps.

---

## Key Insights

1. `vector<vector<T>>` is convenient but often allocates at the wrong granularity.
2. Linked structures are not automatically slow; physical layout determines real cost.
3. Metadata-to-payload ratio is as important as algorithmic complexity.
4. Custom allocators are justified when subsystem invariants are strong and explicit.
5. Geometric growth is the bridge between dynamic arrays and buddy-based allocation.

## Minimal Pseudocode for Pool-Backed Linked Chunks

```cpp
uint32_t add_tag(EntityId e, uint32_t tag) {
    uint32_t head = head_by_entity[e];

    if (head != 0 && nodes[head].count < TAGS_PER_NODE) {
        nodes[head].tags[nodes[head].count++] = tag;
        return head;
    }

    uint32_t n = alloc_node();
    nodes[n].count = 1;
    nodes[n].tags[0] = tag;
    nodes[n].next = head;
    head_by_entity[e] = n;
    return n;
}
```

This form preserves the spirit of the original article while making the tradeoffs explicit and implementation-ready.
