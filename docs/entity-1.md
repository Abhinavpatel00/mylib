# Building a Data-Oriented Entity System, Part 1 — Entities and Lifetime

This chapter rebuilds the basic entity abstraction with explicit data-oriented choices, enough to implement and ship, not just sketch. The aim is to keep entities cheap, cache-friendly, and safe to reference from many subsystems (including Lua) without pointer chasing or intrusive base classes.

## Goals

- Treat an entity as a value (an ID), not a heap object.
- Allow weak references that can be validated quickly and safely.
- Make creation/destruction O(1) and highly cacheable.
- Keep the representation Lua-friendly (fits in a 32-bit light userdata).

## Entity As a Number

We represent an entity as a 30-bit integer split into an index and a generation counter:

```cpp
const unsigned ENTITY_INDEX_BITS = 22;        // up to ~4 million concurrent entities
const unsigned ENTITY_GENERATION_BITS = 8;    // 256 generations per slot
const unsigned ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1;
const unsigned ENTITY_GENERATION_MASK = (1u << ENTITY_GENERATION_BITS) - 1;

struct Entity {
    unsigned id; // 0..2^30-1, fits in 32-bit pointer for Lua light userdata

    unsigned index() const      { return id & ENTITY_INDEX_MASK; }
    unsigned generation() const { return (id >> ENTITY_INDEX_BITS) & ENTITY_GENERATION_MASK; }
};
```

ASCII layout (little endian bits):

```
|31                9|8         0|
+-------------------+-----------+
|   generation (8)  | index (22)|
+-------------------+-----------+
```

### Why this split works

- Index gives O(1) array lookup instead of a hash/set.
- Generation distinguishes reused slots, preventing stale handles from aliasing live entities.
- Only 8 generation bits keeps per-entity metadata to 1 byte, improving cache density.

### Where it applies

- Real-time engines that pass entity references between C++ and scripting.
- Systems with millions of entities but short-lived handles.

### Limitations

- With 8 bits of generation, an index can wrap after 256 recycle cycles. Mitigate by delaying reuse.
- 22 index bits cap concurrent entities at ~4M; adjust if you target 64-bit only.

## EntityManager: Fast Weak Validation

We store just the generation byte per index plus a queue of free indices.

```cpp
class EntityManager {
    Array<uint8_t> generation;     // generation[i] for index i
    Deque<unsigned> free_indices;  // recycled indices, FIFO
    static const unsigned MIN_FREE = 1024; // reuse only when queue is warm

public:
    Entity create() {
        unsigned idx;
        if (free_indices.size() > MIN_FREE) {
            idx = free_indices.front();
            free_indices.pop_front();
        } else {
            generation.push_back(0);
            idx = generation.size() - 1;
            XENSURE(idx < (1u << ENTITY_INDEX_BITS));
        }
        return make_entity(idx, generation[idx]);
    }

    bool alive(Entity e) const {
        return generation[e.index()] == e.generation();
    }

    void destroy(Entity e) {
        const unsigned idx = e.index();
        ++generation[idx];          // bump generation to invalidate old handles
        free_indices.push_back(idx);
    }
};
```

### Why it works

- `alive()` becomes a single array load and compare—hot-path friendly.
- Destruction is O(1); no broadcasts to dependents are required because handles are weak.
- Delayed reuse (`MIN_FREE`) reduces risk of generation wrap collisions.

### Where it applies

- Engines with multithreaded gameplay and scripting where immediate notification is costly.
- Systems that snapshot or serialize entity IDs cheaply (plain integers).

### Limitations and failure modes

- Generation wrap: if you create/destroy more than `generation_range * MIN_FREE` entities on one slot, stale IDs could revalidate. Mitigate by increasing generation bits or MIN_FREE, or by using 64-bit IDs on platforms that allow it.
- `alive()` assumes the entity index exists; guard in debug builds to avoid out-of-range access on corrupt IDs.

## Optional HashSet Variant

If entity counts are tiny or sporadic, a hash set is simplest:

```cpp
HashSet<Entity> live;
Entity create() { Entity e{next_id++}; live.insert(e); return e; }
bool alive(Entity e) { return live.has(e); }
void destroy(Entity e) { live.erase(e); }
```

But the array+generation design dominates on performance once `alive()` sits in hot loops.

## Interaction with Components

- Components should never store raw pointers to other components owned by an entity; store the `Entity` ID.
- Each `ComponentManager` owns the mapping from `Entity -> component instance` and can call `alive()` to validate owners during GC or defrag.

## Serialization and Lua

- A 30-bit ID fits in Lua light userdata; two extra tag bits distinguish it from other light userdata types in the engine.
- For saves, write the integer value directly; no pointer patching required.

## Key Insights

- Treat entities as opaque values, not objects—this simplifies memory, lifetime, and cross-language boundaries.
- Weak references plus generations provide safety without notification storms.
- Arranging metadata for cache (1 byte per entity) matters as much as algorithmic complexity.
- Reuse policy (queue depth) is an explicit, tunable guard against handle resurrection.

## Quick Usage Trace

```
a = create()          // Entity id = (idx=0, gen=0)
b = create()          // Entity id = (idx=1, gen=0)
destroy(a)            // generation[0] becomes 1; a is now invalid
alive(a) => false
c = create()          // may reuse idx 0 only after MIN_FREE entries queued
```

With these foundations, later parts can layer components and transforms without fighting lifetime bugs or pointer churn.
