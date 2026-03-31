# Building a Data-Oriented Entity System, Part 2 — Components That Scale

We now design component storage that respects cache behavior, avoids allocation thrash, and keeps lookups predictable. The point is not just “use SoA,” but to reason about when and why each layout choice pays off.

## Requirements

- Fast lookup from `Entity` to component instance.
- Dense iteration for updates.
- Minimal allocations and easy relocation/compaction.
- Optional support for multiple instances per entity.

## Chosen Example

A toy `PointMassComponent` stores:

```cpp
Entity   entity;       // owner
float    mass;
Vector3  position;
Vector3  velocity;
Vector3  acceleration;
```

Even if you never ship this exact component, the design patterns generalize.

## Layout: Structure of Arrays in One Allocation

We keep field arrays contiguous and co-allocated:

```
buffer:
+----------------+--------------+--------------+--------------+----------------+
| entity[]       | mass[]       | position[]   | velocity[]   | acceleration[] |
+----------------+--------------+--------------+--------------+----------------+
```

```cpp
struct InstanceData {
    unsigned size;      // live elements
    unsigned capacity;  // allocated slots
    void*    buffer;    // single block backing all arrays

    Entity*   entity;
    float*    mass;
    Vector3*  position;
    Vector3*  velocity;
    Vector3*  acceleration;
};
```

### Why this works

- One allocation minimizes allocator overhead and improves TLB/cache behavior.
- SoA keeps hot fields separate; physics update touches `position/velocity/acceleration` without loading `mass` or `entity`.
- Tight packing makes swap-erase O(1) and cache-friendly.

### Where it applies

- Components updated in large batches every frame.
- Systems that occasionally change layout; single backing buffer simplifies migration.

### Limitations

- Manual pointer math requires careful alignment; pad each slice to `alignof(T)`.
- Growing capacity copies all arrays; mitigate by geometric growth (×2).

### Allocation routine sketch

```cpp
void allocate(unsigned new_cap) {
    XENSURE(new_cap > data.size);

    const size_t bytes = new_cap * ( sizeof(Entity) + sizeof(float)
                                   + 3 * sizeof(Vector3) );
    InstanceData nd{};
    nd.buffer = allocator.allocate(bytes);
    nd.size = data.size;
    nd.capacity = new_cap;

    uint8_t* p = static_cast<uint8_t*>(nd.buffer);
    nd.entity       = reinterpret_cast<Entity*>(p);              p += new_cap*sizeof(Entity);
    nd.mass         = reinterpret_cast<float*>(p);               p += new_cap*sizeof(float);
    nd.position     = reinterpret_cast<Vector3*>(p);             p += new_cap*sizeof(Vector3);
    nd.velocity     = reinterpret_cast<Vector3*>(p);             p += new_cap*sizeof(Vector3);
    nd.acceleration = reinterpret_cast<Vector3*>(p);

    memcpy(nd.entity,       data.entity,       data.size*sizeof(Entity));
    memcpy(nd.mass,         data.mass,         data.size*sizeof(float));
    memcpy(nd.position,     data.position,     data.size*sizeof(Vector3));
    memcpy(nd.velocity,     data.velocity,     data.size*sizeof(Vector3));
    memcpy(nd.acceleration, data.acceleration, data.size*sizeof(Vector3));

    allocator.deallocate(data.buffer);
    data = nd;
}
```

## Mapping Entity → Instance

We need an indirection to avoid holes in the dense arrays. Options:

- **Array map (`index -> instance`)**: `Array<unsigned> map; map[e.index()] = instance;` Fast but wastes space if few entities use the component.
- **Hash map**: `HashMap<Entity, unsigned> map;` Compact, slightly slower but still O(1) average.

### Guidance

- Use array map for near-universal components (e.g., transforms) or when you need maximum speed in hot lookups.
- Use hash map for sparse components to save memory and avoid cold cache lines full of “unused”.

## API Handles

Expose a lightweight handle to avoid multiple lookups per field access:

```cpp
struct Instance { int i; };

Instance lookup(Entity e) {
    auto it = map.find(e);
    return it ? Instance{it->second} : Instance{-1};
}

float mass(Instance h) const { return data.mass[h.i]; }
void  set_mass(Instance h, float m) { data.mass[h.i] = m; }
```

### Why

- Caller performs one map lookup, then uses the handle for multiple field reads/writes.
- Keeps component manager free to change internal layout without leaking pointers.

## Multiple Components Per Entity

Add a `next_instance` index to create a per-entity intrusive list:

```cpp
Array<int> next_instance; // size == data.capacity, -1 sentinel
Array<int> head_by_entity; // same index space as Entity.index()
```

### Limitations

- Adds one int per instance.
- Traversal remains cache-friendly because instances stay dense; only the linkage is scattered.

## Update Pass

```cpp
void simulate(float dt) {
    for (unsigned i = 0; i < data.size; ++i) {
        data.velocity[i]     += data.acceleration[i] * dt;
        data.position[i]     += data.velocity[i] * dt;
    }
}
```

Sequential access maximizes cache line utilization and is trivial to SIMD or parallelize.

## Destruction: Swap-Erase

Keep arrays dense by moving the last element into the removed slot and fixing the map:

```cpp
void destroy(unsigned i) {
    const unsigned last = data.size - 1;
    const Entity e      = data.entity[i];
    const Entity last_e = data.entity[last];

    data.entity[i]       = data.entity[last];
    data.mass[i]         = data.mass[last];
    data.position[i]     = data.position[last];
    data.velocity[i]     = data.velocity[last];
    data.acceleration[i] = data.acceleration[last];

    map[last_e] = i;
    map.erase(e);
    --data.size;
}
```

### Limitation

- Order of instances becomes unstable; if stable iteration order matters, use a freelist + “gap filling” strategy instead, at the cost of occasional holes.

## Garbage Collecting on Entity Death

Two strategies:

1. **Callbacks**: components register a destroy callback with `EntityManager` for immediate cleanup.
2. **Lazy GC**: periodically sample random instances and delete those whose owners are dead:

```cpp
void gc(const EntityManager& em) {
    unsigned hits = 0;
    while (data.size && hits < 4) {
        unsigned i = random_in_range(0u, data.size - 1);
        if (em.alive(data.entity[i])) { ++hits; continue; }
        hits = 0;
        destroy(i);
    }
}
```

### Why lazy GC can win

- Near-zero cost when no entities die.
- Cleans up rapidly when many die.
- Avoids storing per-entity component lists.

### Limitation

- Components linger for up to a few frames; acceptable for purely gameplay data, not for scarce external resources.

## Key Insights

- Dense SoA + single allocation buys both performance and simplicity.
- Choose indirection (array vs hash) based on sparsity, not habit.
- Swap-erase is the right default; reach for stable ordering only when required.
- GC strategy should match component semantics: immediate for resource-owning, lazy for pure data.

Armed with this, the transform component in the next part can focus on hierarchy logic instead of basic storage.
