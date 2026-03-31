# Building a Data-Oriented Entity System, Part 4 — Entity Resources and Spawn Throughput

We now move from runtime layout to how entities are authored, compiled, and spawned quickly. The goal: binary resources that can be mmap’d/loaded and instantiated with minimal CPU and cache disruption.

## Static vs Dynamic Data

- **Static data**: baked, read-only, byte-order–fixed; safe to load and use without patching pointers.
- **Instance data**: per-entity, per-instance mutable state (colors, runtime flags, etc.).

Entities are highly dynamic: components can be added/removed, children reparented. Therefore entity resources contain *instructions* for building instance data, not shareable runtime state.

## Resource Layout v1: Naïve Hierarchy

```cpp
struct EntityResource {
    unsigned num_components;
    ComponentData components[num_components];
    unsigned num_children;
    EntityResource children[num_children];
};

struct ComponentData {
    unsigned component_identifier; // hashed name
    unsigned size;
    char data[size];               // component-specific payload
};
```

### Why this is insufficient

- Spawning walks components in entity order, interleaving different component types → poor I-cache and data-cache locality.
- Unknown components are skipped one-by-one instead of in bulk.

## Data-Oriented Rewrite: Group by Component Type

We batch similar operations together.

```cpp
struct EntityResource {
    unsigned num_entities;           // total, including children
    unsigned num_component_types;
    unsigned parent_index[num_entities]; // UINT_MAX for roots
    ComponentTypeData component_types[num_component_types];
};

struct ComponentTypeData {
    unsigned component_identifier;   // e.g., hash("transform")
    unsigned num_instances;          // how many instances of this type
    unsigned size;                   // bytes in instance_data
    unsigned entity_index[num_instances]; // which entity each instance belongs to
    char instance_data[size];        // tightly packed instances
};
```

ASCII spawn order comparison:

```
Before: A.Transform, A.Mesh, A.Actor, B.Transform, B.Mesh, ...
After:  Create entities, all Transforms, all Meshes, all Actors ...
```

### Why grouping wins

- Each component spawner runs in a tight loop over homogeneous data.
- Data for a component type is contiguous, improving cache/TLB hit rates.
- Unknown component types can be skipped with one pointer jump (`size`).

### Where it applies

- Level loads, prefab instantiation, mass spawning (10k+ entities).

### Limitations

- Requires a pre-pass to count component instances per type when compiling.
- `entity_index` indirection is necessary; ensure it stays 32-bit unless you exceed 4B entities per resource (unlikely).

## Representing Hierarchy

`parent_index[num_entities]` stores the parent entity index or `UINT_MAX` for roots:

```
parent_index = {UINT_MAX, 0, 1, 1, 2}

A --- B --- C --- E
      |
      +--- D
```

When spawning transforms, use this array to wire parent/child relationships without extra passes.

## Compiler/Spawner Registration

Decouple entity compiler and runtime spawner through registration.

```cpp
using CompileFn = Buffer (*)(const JsonData& cfg, CompileCtx& ctx);
void register_component_compiler(const char* name, CompileFn fn, int spawn_order);

using SpawnFn = void (*)(const Entity* entity_lookup,
                         unsigned num_instances,
                         const unsigned* entity_index,
                         const char* data);
void register_component_spawner(const char* name, SpawnFn fn);
```

- `spawn_order` enforces dependencies (e.g., transforms before meshes).
- Unknown component identifiers can be ignored or treated as errors, depending on build rules.

## Spawn Algorithm (Grouped)

1. Create all entities in one call: `Entity entities[num_entities] = em.create_batch(num_entities);`
2. For each `ComponentTypeData` in spawn order:
   - Look up spawner by `component_identifier`.
   - For `i in [0,num_instances)`, get `Entity e = entities[entity_index[i]]`.
   - Spawner reads instance payloads from `instance_data` and creates components.

This is linear in resource size and highly cache friendly.

## Endianness and Pointer Freedom

- All static resource data must be endian-swapped at build time; runtime does zero fixups.
- Offsets replace pointers everywhere; `entity_index` and `parent_index` are explicit indices.

## Handling Unknown Components

Because each `ComponentTypeData` block has a `size`, the spawner can skip an entire unknown type at once:

```cpp
cursor += sizeof(ComponentTypeDataHeader) + size;
```

Helpful for forward compatibility between tools and runtime.

## Performance Notes

- Spawning 10k entities becomes “tight loops over contiguous arrays,” not “many tiny function calls.”
- Grouping by type often halves instruction cache misses on real projects.
- Memory touch order becomes predictable, enabling prefetching if needed.

## Failure Modes & Mitigations

- **Mismatched entity counts**: validate that every `entity_index` < `num_entities` at build time.
- **Bad parent indices**: assert parents appear before children if your transform spawner expects that; otherwise store a `parent_index` map and resolve later.
- **Version drift**: include a version hash per component payload so spawners can reject incompatible data early.

## Key Insights

- Doing similar work together (batch by component type) matters more than linear traversal of the resource blob.
- Offsets and indices keep the blob pointer-free and relocation-friendly.
- Parent relationships are just data; keep them outside component payloads so multiple component types can consume them.
- Compile-time grouping adds a small cost once, but pays off on every spawn.

Next we’ll see how these resource structures feed into prefab authoring and overrides.
