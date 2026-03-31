# Garbage Collection, Allocation Granularity, and System-Scoped Heaps

This rewrite deepens the original argument: the real enemy of performance is not garbage collection per se, but proliferating tiny allocations. Bigger, coarser allocations help both GC’d and manually managed languages, and they unlock simpler, more reliable memory debugging.

## Why Worry About GC Time?

- Soft-realtime games dislike unpredictable stalls.
- Even incremental collectors consume CPU that appears “non-productive.”
- Yet GC buys safety and simplicity: fewer ownership bugs, easier cross-system data sharing.

Key premise: reduce the collector’s surface area by reducing allocation count and increasing allocation size.

## Data-Oriented Design Changes Allocation Shape

Data-oriented design (DoD) favors tight, homogeneous arrays over scattered objects. This naturally aggregates memory into larger chunks.

### Bullet Example in Lua

**Object-per-bullet (many tiny allocations):**

```lua
function Bullet:update(dt)
    self.position = self.position + self.velocity * dt
end

function Bullets:update(dt)
    for _, b in ipairs(self.bullets) do
        b:update(dt)
    end
end
```

**Array-of-values (few large allocations):**

```lua
function Bullets:update(dt)
    for i = 1, #self.pos do
        self.pos[i] = self.pos[i] + dt * self.vel[i]
    end
end
```

ASCII layout contrast:

```
Object-per-bullet:    [obj][obj][obj][obj]...    (headers + pointers, scattered)
DoD arrays:           [pos][pos][pos]...[vel][vel][vel]... (contiguous)
```

Measured on LuaJIT with many bullets:

- DoD update ≈ **50× faster** (cache-friendly tight loop).
- GC time ≈ **½** of object version, even though bullet logic itself allocates nothing.

### Why GC Shrinks Here

- Fewer live objects → smaller mark set, faster traversal.
- Larger allocations → fewer metadata entries for the collector to visit.
- Less pointer chasing → better cache during GC marking as well as during gameplay.

### Where This Applies

- Scripting layers that manage gameplay state (Lua, JS, Python).
- Native code too: custom allocators benefit the same way; a malloc that sees 10 buffers beats one seeing 10,000.

### Limitations

- If arrays resize frequently, you can still create churn; reserve/grow geometrically.
- Over-aggregating unrelated data can hurt locality; group by access pattern, not by whim.

## Small Allocations Hurt Even Without GC

Independent of GC, many tiny allocations cause:

- Poor cache/TLB locality (headers and payloads interleaved unpredictably).
- Higher allocator overhead and lock contention.
- External fragmentation: free space exists but not in the needed shapes.
- Harder attribution: which system owns which bytes?

## System-Scoped, Coarse Allocations in C++

Instead of a single kitchen-sink heap, give each subsystem a handful of big blocks and let it sub-allocate.

### How

1. Global allocator hands out page- or megabyte-sized slabs, tagged by system.
2. Each system implements a fit-for-purpose sub-allocator (pool, arena, freelist, etc.).
3. Systems track their own usage and lifetime; teardown frees whole slabs at once.

Minimal sketch:

```cpp
void* slab = page_allocator.allocate_pages(num_pages); // global API
PhysicsPools pools{slab, num_pages * PAGE_SIZE};

// physics code allocates from pools, never from global malloc
RigidBody* rb = pools.rigidbody_pool.allocate();
```

### Why it works

- Local knowledge: systems know alignment, lifetime groups, and growth patterns.
- Accounting: few top-level allocations, each labeled → easy per-system budgets.
- Fast shutdown: free slabs; no per-object frees.
- Isolation: bugs overwrite intra-system data; page faults surface dangling pointers.

### Where it applies

- Engines with many semi-independent systems (audio, animation, rendering, AI).
- Tools pipelines where subsystems can own their memory life cycle.

### Limitations

- Requires discipline and allocator expertise in each system.
- Internal fragmentation exists inside slabs; must be monitored per system.
- Peaks must be estimated to size slabs; underestimation causes growth, overestimation wastes memory.

## Whole-Page Global Allocations

Taking it further: restrict the global allocator to page-size granularity (e.g., 4 KiB or 64 KiB pages). All finer-grained allocation happens inside system slabs.

ASCII view:

```
Global (page allocator): [page][page][page][page]...
System sub-allocators:   page -> [pool of bullets]
                         page -> [audio buffers]
                         page -> [scratch arenas]
```

### External vs Internal Fragmentation

- **External fragmentation** (global heap) nearly eliminated: pages are uniform and can be remapped/returned wholesale.
- **Internal fragmentation** rises: average half-page waste per slab on random sizes.

Why internal is nicer:

- Scoped: waste is attributable to a system; you can resize or repack just there.
- Debuggable: OOM is per-system; adjust budgets or content locally instead of “hoping” a global defrag helps.

### 64-bit Address Space Helps

More pages mean more virtual address headroom; fewer individual allocations mean fewer pointers, mitigating the cost of 64-bit pointers.

## Operational Guidance

- **Design for bulk**: allocate arrays, arenas, or pools instead of per-object new/delete.
- **Grow geometrically**: resize buffers by ×1.5–×2 to amortize allocations and stabilize pointer counts.
- **Tag allocations**: per-system labels make memory audits trivial.
- **Instrument GC/allocators**: track object counts and allocation size histograms to see wins from aggregation.
- **Introduce gradually**: convert one system at a time to page-backed slabs; leave messy legacy code on the kitchen-sink heap or give it a private heap.

## Key Insights

- GC overhead correlates strongly with allocation *count*, not just allocated *bytes*.
- Data-oriented layouts reduce both CPU time and GC time by consolidating allocations.
- System-scoped slab allocation brings the same benefits to manual-memory languages and improves debuggability.
- Swapping external fragmentation for internal, per-system fragmentation makes memory issues local and fixable.

By reshaping allocations—fewer, larger, system-owned—you improve performance, make GC less scary, and turn memory debugging from a global crisis into a local, solvable task.
