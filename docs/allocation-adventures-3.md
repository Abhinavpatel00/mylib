# Allocation Adventures 3: The Buddy Allocator, Fully Worked Through

This rewrite keeps the original intent but makes the allocator mechanics explicit enough to implement from scratch.

## Allocator Goals and Constraints

A general-purpose allocator must do all of this well enough at once:

1. Serve allocations quickly.
2. Free memory quickly.
3. Keep fragmentation manageable.
4. Keep metadata overhead low.
5. Preserve alignment guarantees.

The challenge is balancing these in the face of unpredictable allocation patterns.

## Fragmentation: Two Different Problems

### External fragmentation

Free bytes exist, but not in one contiguous block large enough for request.

```text
+--------------------------------------------------------------+
| used | free 64 | used | free 128 | used | free 64 | used ... |
+--------------------------------------------------------------+
```

Request for 200 bytes fails even though total free is 256.

### Internal fragmentation

Allocated block is larger than request due to size class rounding.

```text
request 13 KiB -> block class 16 KiB -> 3 KiB internal waste
```

Good allocators trade one kind of fragmentation against another based on workload.

## Why O(n) Free-Block Search Fails at Scale

A linked list of free blocks is simple but expensive:

- Search is O(n) by free-block count.
- Pointer chasing produces cache misses.
- Latency variance increases as heap ages.

Modern allocators avoid global linear scans in hot paths.

## Metadata Techniques and Their Costs

### In-place free list links

Store `prev/next` in free blocks themselves. Cheap and common.

### Preamble (header before payload)

Stores block size/type/state.

```text
+-------------------------------+
| preamble | user bytes | maybe |
+-------------------------------+
```

### Postamble (footer)

Supports left-neighbor discovery for coalescing.

### Cost reality

If allocator enforces 16-byte alignment and uses both header+footer, small allocations pay heavy overhead. This is why many allocators route tiny allocations through slab/size-class pools.

## Interface Matters: `free(ptr)` vs `free(ptr, size)`

`free(ptr, size)` is allocator-friendly because block size is explicit.

`free(ptr)` is common language ABI, but allocator must recover size via metadata or lookup structure.

The original article highlights this correctly: much runtime context already knows size, but C ABI does not pass it.

---

## Buddy Allocator Model

Start with one power-of-two region and recursively split blocks into equal buddies.

```text
level 0: [ entire region ]
level 1: [ left half ][ right half ]
level 2: [ quarter ][ quarter ][ quarter ][ quarter ]
...
```

A request is rounded to nearest supported block size (power-of-two class).

### Definitions

- `leaf_size`: minimum block size.
- `num_levels`: tree depth (`level 0` root, `level num_levels-1` leaves).
- `total_size = leaf_size << (num_levels - 1)`.
- `block_size(level) = total_size >> level`.
- `blocks_in_level(level) = 1 << level`.

(Equivalent formulas are possible depending on whether levels count from root or leaf. Use one convention consistently.)

### State visualization

```text
Legend: S=split, F=free, A=allocated

L0:               [S]
L1:         [S]         [F]
L2:      [S] [F]
L3:     [A][F]
```

## Fast Allocation via Per-Level Free Lists

Maintain one free list head per level:

```cpp
static constexpr uint32_t MAX_LEVELS = 32;
uint32_t free_head[MAX_LEVELS]; // node indices, 0 means empty
```

Allocation algorithm (`allocate(level)`):

1. If free list at `level` non-empty, pop and return.
2. Else allocate at `level-1` recursively.
3. Split that parent into two children at `level`.
4. Push both children into free list `level`.
5. Pop one child and return.

This gives near O(log N) path length with simple operations.

## Indexing Blocks Without Storing Pointers

A block can be identified by `(level, index_in_level)`.

Linear index in complete binary tree:

```text
global_index = (1 << level) + index_in_level - 1
```

Example:

```text
level 0: index 0
level 1: indices 1,2
level 2: indices 3,4,5,6
```

This mapping enables compact bitsets for metadata.

## Buddy Address Math

Given block pointer `p` at level `L`:

```text
i = (p - base) / block_size(L)          // index in level
buddy_i = i ^ 1                         // flip low bit
buddy_ptr = base + buddy_i*block_size(L)
```

This XOR trick is central: buddies differ only in the least significant bit of `index_in_level`.

---

## Deallocation and Merge Conditions

On free:

1. Mark block free.
2. Check buddy state.
3. If buddy free, remove both from level list.
4. Merge into parent block at `L-1`.
5. Repeat upward until buddy not free or root reached.

Naively checking if buddy is in free list is O(n). We need O(1)-ish metadata.

## Metadata Compression: XOR Merge Map

Original post's key insight: store one bit per buddy pair:

```text
pair_bit = is_left_free XOR is_right_free
```

On each alloc/free of either buddy: flip pair bit.

When freeing one block, its own state is known to be free; pair bit tells whether buddy is also free.

- `pair_bit = 0` => both same => buddy also free -> merge.
- `pair_bit = 1` => buddy allocated -> stop.

This reduces merge-state storage to half a bit per block.

### Overhead estimate

Let minimum block size be `leaf_size` bytes.

- one bit per block => overhead `1/(8*leaf_size)`
- half bit per block => overhead `1/(16*leaf_size)`

For `leaf_size = 128`, half-bit overhead is ~0.049%.

---

## Supporting `free(ptr)` Without Preambles

If caller does not provide size, allocator needs level `L` for `ptr`.

Original post proposes split-state bitset:

- one bit per internal node: whether node has been split.
- walk from root toward leaves along pointer path until first unsplit ancestor identifies allocation level.

Pseudo:

```cpp
uint32_t find_level_from_ptr(void* p) {
    uint32_t level = 0;
    while (level + 1 < num_levels) {
        if (!is_split(node_for_ptr_at_level(p, level))) {
            return level;
        }
        ++level;
    }
    return num_levels - 1;
}
```

This avoids per-allocation preambles while keeping metadata compact.

### Cost

- Extra traversal on `free(ptr)`.
- Bounded by `num_levels` (typically small).

### Hybrid API strategy

Offer both:

- `free(ptr, size)` fast path.
- `free(ptr)` compatibility path.

---

## Memory Layout: Put Metadata Inside Managed Buffer

To avoid requesting separate OS allocation for metadata, reserve initial blocks for metadata itself.

```text
+--------------------------------------------------------------+
| metadata blocks (marked allocated) | usable buddy space ...  |
+--------------------------------------------------------------+
```

Important bootstrapping rule:

- allocator initialization must not depend on metadata that is not yet initialized.

Usually solved with a tiny special-case bootstrap allocator path.

## Non-Power-of-Two Usable Regions

Buddy prefers power-of-two total size, but real available memory may be irregular.

Technique from original article:

1. Build next larger power-of-two buddy arena.
2. Mark leading/trailing region outside actual usable bytes as permanently allocated.
3. Offset exposed base pointer so client-visible range matches real memory.

This salvages almost all memory with minimal complexity increase.

---

## Complexity Summary

Let `L = num_levels`.

- Allocate: O(L) worst case (split chain).
- Free with known level: O(L) worst case (merge chain).
- Free without size: O(L) for level discovery + O(L) merge.

Constant factors are low if:

- free lists are intrusive,
- metadata is bit-packed,
- and pointer/index math is branch-light.

## When Buddy Allocator Works Well

1. Allocation sizes naturally near powers of two.
2. Buffers grow geometrically (`x2` pattern).
3. Large-block churn where coalescing matters.
4. Systems valuing simple deterministic behavior.

## Where It Performs Poorly

1. Heavy tiny-allocation traffic (better with slab/segregated fit).
2. Many awkward non-power-of-two sizes (internal waste).
3. Workloads requiring very fine-grained reuse efficiency.

Practical fix: use buddy for medium/large blocks and route tiny allocations to slabs.

---

## Connection Back to Dynamic Buffers

From part 2: vectors typically grow geometrically.

Buddy allocator natively serves geometric capacities:

```text
cap: 64 -> 128 -> 256 -> 512 ...
buddy classes line up exactly.
```

This removes the worst internal-fragmentation concern in that specific use case because each grown buffer tends to fully utilize its assigned block before next growth.

## Reference Pseudocode

```cpp
struct Buddy {
    uint8_t* base;
    uint32_t total_size;
    uint32_t leaf_size;
    uint32_t num_levels;

    uint32_t free_head[MAX_LEVELS];
    Bitset split_map;      // internal nodes
    Bitset merge_xor_map;  // buddy-pair xor bits
};

void* alloc(Buddy& b, uint32_t bytes);
void  free_known(Buddy& b, void* p, uint32_t bytes);
void  free_unknown(Buddy& b, void* p);
```

## Key Insights

1. Buddy allocator simplicity comes from rigid block geometry and strong invariants.
2. Metadata design determines whether implementation remains elegant or becomes expensive.
3. `free(ptr)` convenience pushes hidden complexity into allocator internals.
4. Internal fragmentation is not uniformly bad; it depends on request distribution.
5. Buddy is most powerful when paired with workload-aware front-ends (e.g., slabs for small objects, buddy for growing buffers).
