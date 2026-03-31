# Syncing a Data-Oriented ECS with a Stateful External System (Comprehensive Rewrite)

## Executive Summary

A data-oriented ECS excels at bulk processing, but synchronization with external **stateful** systems (physics, rendering backends, audio engines, scene graph services) introduces a mismatch:

- ECS prefers batched, cache-friendly, declarative processing.
- External systems often need explicit create/update/destroy and may keep hidden internal state.

A robust sync strategy must solve:
1. creation mirroring,
2. change propagation,
3. destruction mirroring,
while preserving performance and avoiding temporal bugs from out-of-sync worlds.

---

## Problem Setup

Assume an ECS with archetype/chunk storage and singleton components. Example:

```c
typedef struct tm_transform_t {
    tm_vec3_t pos;
    tm_vec4_t rot;
    tm_vec3_t scl;
} tm_transform_t;
```

A simple ECS system over contiguous arrays:

```c
void velocity_system(tm_transform_t *td, const tm_velocity_t *vd, uint32_t n, float dt)
{
    while (n--) {
        td->pos = tm_vec3_mul_add(td->pos, vd->vel, dt);
        ++td; ++vd;
    }
}
```

Mirroring to physics introduces an external side effect:

```c
void mirror_kinematics_system(tm_kinematic_actor_t *kd, const tm_transform_t *td, uint32_t n)
{
    while (n--) {
        kd->actor->setGlobalPose(to_physx(td));
        ++kd; ++td;
    }
}
```

---

## The Three Sync Cases

Every ECS → external sync mechanism must cover:

1. **Create**: when ECS gains physics-relevant data, create external representation.
2. **Mutate**: when ECS data changes, update external state.
3. **Destroy**: when ECS loses data or entity dies, remove external representation.

ASCII lifecycle:

```
ECS events:   [add components] [change values] [remove components/entity]
                  |                |                 |
External side: [create actor]  [set pose/state]   [destroy actor]
```

---

## Strategy 1: Brute Force (Batch Mirror)

### WHY
It is the simplest method with predictable code flow and excellent data locality.

### HOW
Each frame (or phase), iterate matching entities and push state out, optionally skipping unchanged items via dirty/version data.

```c
if (td[i].version != mirrored_version[i]) {
    kd[i].actor->setGlobalPose(to_physx(&td[i]));
    mirrored_version[i] = td[i].version;
}
```

### WHERE
- broad synchronization passes,
- data-oriented engines prioritizing throughput,
- systems where one-frame delay is acceptable or controlled.

### LIMITATIONS
- Can waste work when few objects changed.
- May create temporal mismatch windows (ECS already updated, external state not yet mirrored).
- External API calls may dominate cost if expensive.

---

## Strategy 1B: Hierarchical Dirty Flags

### WHY
Reduce scan cost for sparse changes while keeping batched model.

### HOW
Use a tree/segment hierarchy of “dirty” aggregates.

```
Level 0: [All Objects Dirty?]
  Level 1: [0..8191?] [8192..16383?]
    Level 2: subdivide further...
      Leaf: per-object dirty/version
```

Traversal skips clean subtrees; sparse updates become near `O(log N + K)` instead of `O(N)`.

### WHERE
- large mostly-static worlds,
- expensive per-object external calls,
- frequent sync passes.

### LIMITATIONS
- Extra bookkeeping on writes.
- Added structural complexity and memory overhead.
- Poorly tuned granularity can erase gains.

---

## Strategy 2: Orchestrated Propagation by Higher-Level Systems

### WHY
If mutation origin is known, notify consumers directly and avoid global scans.

### HOW
The mutating subsystem emits explicit notifications:

```text
Animation moved entity E -> notify PhysicsMirror(E)
Script changed transform(E) -> notify RenderSync(E)
```

### WHERE
- tightly controlled projects,
- fixed component vocabulary,
- teams willing to enforce strict mutation APIs.

### LIMITATIONS
- Strong coupling across systems.
- Fails in plugin/extensible ecosystems where interactions are open-ended.
- Easy to miss one write path and silently desynchronize state.

---

## Strategy 3: Callbacks / Observers

### WHY
Immediate propagation can eliminate out-of-sync windows.

### HOW
Register callbacks on component changes; mutators invoke `notify()`.

```c
void set_transform(entity_t e, tm_transform_t t)
{
    transform[e] = t;
    notify_component_changed(e, TM_COMPONENT_TRANSFORM);
}
```

### WHERE
- operations requiring immediate consistency,
- parent-child transform propagation,
- add/remove setup/teardown hooks.

### LIMITATIONS
- Hard for parallel schedulers to reason about unknown callback side effects.
- Immediate callbacks can hurt cache behavior and predictability.
- Deferred callbacks lose immediacy and become another batch queue.
- Recursive or reentrant callback chains are bug-prone.

---

## Strategy 4: Lists of Changed Entities

### WHY
Track only what changed, with explicit polling points.

### HOW
Writers append to per-component (or per-component-per-archetype) change lists:

```c
changelist_transform.push_back(entity);
```

Consumers poll and filter relevant lists.

### WHERE
- moderate change rates,
- systems needing deterministic “process changes now” phases,
- workloads with multiple sync points per frame.

### LIMITATIONS
- Requires dedup policy (same entity changed many times).
- Bookkeeping overhead on every write.
- One global list is noisy; many specialized lists increase management complexity.

---

## Strategy 5: Reuse Entity-Type Filtering via “Helper Components”

### WHY
Leverage ECS’s strongest primitive (archetype filtering) to represent changed subsets.

### HOW
When `Transform` mutates, add helper component `TransformChanging` and run systems on:

```
(Transform, TransformChanging, KinematicActor)
```

### WHERE
- if changed subset benefits from physical regrouping,
- where hot loops over changed items dominate.

### LIMITATIONS
- Add/remove helper components migrates entities between archetypes (copy/move cost).
- Can explode number of entity types.
- Smaller batches may reduce SIMD/cache efficiency.

---

## Why Out-of-Sync Bugs Matter (Temporal Correctness)

Even if each subsystem is “locally correct,” frame-phase lag causes gameplay bugs.

Example timeline:

```
t0: Script moves car in ECS
t1: Gameplay raycast queries physics (still old pose)
t2: Mirror pass updates physics pose
```

At high speed, one frame of lag can mean meter-scale mismatch and missed hits.

### Practical implications
- Define strict frame ordering contracts.
- Prefer immediate update only where correctness requires it.
- Avoid ad-hoc mixed models that are hard to reason about.

---

## Recommended Hybrid Policy

No single mechanism dominates all cases. A practical engine policy:

1. **Default**: brute-force batch mirroring with versions/dirty bits.
2. **Scale fix**: hierarchical dirty flags if profiling shows sparse-update pain.
3. **Lifecycle hooks**: callbacks for add/remove (create/destroy external objects).
4. **Immediate correctness islands**: targeted callbacks for strict invariants (e.g., parent-child transforms).
5. **Avoid broad orchestration coupling** unless the subsystem boundary is intentionally closed.

ASCII decision flow:

```
Need sync?
  -> Is one-frame lag acceptable?
       yes -> Batch mirror
             -> Too slow when sparse? add hierarchical dirty
       no  -> Immediate callback or tightly constrained orchestrated path
```

---

## Design Checklist (WHY/HOW/WHERE/LIMITATIONS in Practice)

For each sync boundary (ECS↔Physics, ECS↔Renderer, ECS↔Audio):

- **WHY**: What correctness property is required? (eventual vs immediate consistency)
- **HOW**: Which mechanism enforces it? (batch, callbacks, lists, helper components)
- **WHERE**: In what frame phase/thread does it execute?
- **LIMITATIONS**: What can still fail? (lag, missed notifications, archetype churn, lock contention)

If this is written down explicitly, most “mysterious” sync bugs become straightforward scheduling bugs.

---

## Final Takeaway

The central insight is not “pick one change-tracking system.” It is:

- preserve ECS bulk-processing strengths,
- selectively buy immediacy only where correctness needs it,
- keep synchronization semantics explicit at subsystem boundaries.

A disciplined hybrid architecture beats a theoretically pure but impractical one.