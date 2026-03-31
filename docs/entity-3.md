# Building a Data-Oriented Entity System, Part 3 — Transform Component and Hierarchies

This part turns the abstract component framework into a concrete, shipping-quality transform system. We focus on hierarchy management, update strategy, and cache-aware storage.

## What the Transform Component Must Provide

- Local transform relative to parent.
- Derived world transform.
- Parent/child relationships (scene graph for entities).
- Optional existence: logic-only entities pay zero cost.

## Why Optional Transforms

Forcing every entity to carry a transform bloats memory and complicates purely logical entities. Making transforms optional keeps entity cost minimal and lets non-spatial systems ignore transform bookkeeping.

## Multiple Worlds

A world is an independent container of entities and components. The same entity ID may have a transform in several worlds (e.g., inventory vs gameplay). Each world owns its own `TransformComponent` manager.

### Implication

- You can render or simulate the same entity differently per world.
- Component managers remain world-local, avoiding cross-world locks.

## Two Scene Graphs: Entity vs Model

- **Entity scene graph**: links between entities (e.g., car → wheel).
- **Model scene graph**: internal nodes of a model (e.g., bones).

Decouple them:

- Model graph computes poses relative to the entity transform, not world space.
- This removes update-order coupling and allows parallel animation evaluation.

## Update Strategy: Immediate vs Deferred

### Deferred (dirty flag)

```
[ node_1 ] -> [ node_2 ] -> ... -> [ node_n ]
```
- Changing any local pose marks dirty; world poses recomputed later in batch.
- Cost: one world recompute per dirty subtree => O(n) even if many edits.
- Risk: queries before recompute see stale world transforms.

### Immediate (on write)

- Changing a local pose immediately recomputes world for that node and its subtree.
- Accurate queries at all times.
- Worst case O(n^2) if many nodes in one chain change in the same frame.

### Chosen: Immediate

- Long moving chains are more common in model graphs than entity graphs.
- Typical entity hierarchies are shallow (n <= ~5) and rarely all moving simultaneously.
- Correctness and predictability outweigh occasional extra work.

### Escape hatch

Provide an API to set many locals then recompute once, if profiling shows trouble.

## Data Layout

Single buffer, SoA style, plus intrusive links:

```cpp
struct Instance { int i; }; // -1 invalid

struct InstanceData {
    unsigned size;
    unsigned capacity;
    void*    buffer;

    Entity*    entity;        // owner
    Matrix4x4* local;         // local to parent
    Matrix4x4* world;         // cached world
    Instance*  parent;
    Instance*  first_child;
    Instance*  next_sibling;
    Instance*  prev_sibling;
};
```

Hierarchy links are indices, not pointers, so swap-erase remains valid with patch-up.

ASCII view:

```
parent[i] ----> (index of parent instance)
first_child[i] -> child0 -> child1 -> ...
next_sibling   -> sibling chain
```

## Setting a Local Transform (Immediate Update)

```cpp
void set_local(Instance i, const Matrix4x4& m) {
    data.local[i.i] = m;
    Matrix4x4 p = is_valid(data.parent[i.i])
        ? data.world[data.parent[i.i].i]
        : matrix4x4_identity();
    propagate(p, i);
}

void propagate(const Matrix4x4& parent_world, Instance i) {
    data.world[i.i] = parent_world * data.local[i.i];
    for (Instance c = data.first_child[i.i]; is_valid(c); c = data.next_sibling[c.i])
        propagate(data.world[i.i], c);
}
```

### Why this is acceptable

- Depth-first walk touches contiguous arrays; world/local/links are co-located per slice.
- Hierarchies are shallow; recursion depth is small. Replace with iterative stack if needed.

## Swap-Erase with Hierarchy Safety

Swapping instances requires repairing all link fields that referenced the swapped indices. Safe procedure using the last slot as temporary:

```
[A] <-> [tmp]
[B] <-> [A]
[tmp] <-> [B]
```

1. Move A to temp (end of array) and retarget any parent/child/sibling links pointing to A.
2. Move B into A's old slot; retarget links that referenced B.
3. Move temp into B's old slot; retarget links that referenced temp.

Keeping all link updates in three disjoint steps avoids mid-swap dangling references.

## Supporting Optional Parent Data

Parent relationships can also be stored in the transform component itself (as above) or passed via an explicit parent index array when spawning from resources (see Part 4). Both work; the important part is to keep the link data local to the transform component to avoid global lookups during traversal.

## Sorting for Cache (Optional)

Even with immediate updates, keeping parent and children adjacent improves cache hits. Periodically partition the array so that each subtree occupies a compact range; stable IDs are maintained via handles, not array positions.

## Key Insights

- Decouple model-space and entity-space graphs to unlock parallel animation.
- Immediate updates favor correctness and simpler mental model; worst-case cost is acceptable for shallow graphs.
- Use indices for hierarchy links to remain swap-erase friendly.
- Treat swap-erase as a three-step link-repair operation, not just a memcpy.

With transforms in place, the next part can focus on how entities and components are compiled and spawned efficiently from resource data.
