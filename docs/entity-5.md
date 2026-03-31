# Building a Data-Oriented Entity System, Part 5 — Prefabs and Overrides

Prefabs let one authored entity definition drive many instances while allowing per-instance edits. This part defines the JSON authoring format, explains merge semantics, and shows a compilation strategy that flattens prefab chains efficiently.

## Authoring Format (SJSON)

We use SJSON, a relaxed superset of JSON (optional root braces, `=` as `:`, optional commas, comments, triple-quoted strings). A raw entity definition:

```json
components = {
    "c80f6004-427f-4662-a705-b89fef7abae7" = {
        type = "transform"
        pos = [0 0 0]
    }
    "86cbf596-8a55-492e-a03b-5761b5e80000" = {
        type = "mesh"
        scene = "scenes/box"
        mesh = "g_box"
        shadow_caster = true
        material_map = {
            default = { resource = "scenes/box" }
        }
    }
}
children = { }
```

### Why GUID keys

- Object members merge cleanly in source control (different GUID keys rarely collide).
- Arrays are treated as opaque blobs in merges; using objects avoids conflict churn when teammates add components.

## Prefab Reference and Overrides

To reuse an entity as a prefab:

```json
prefab = "entities/box"
```

We then allow additive, modifying, and deleting operations without breaking the prefab link:

```json
components = {            // new components not in prefab
    "new-comp" = { type = "light" intensity = 2.0 }
}

modified_components = {   // partial overrides of prefab components
    "c80f6004-427f-4662-a705-b89fef7abae7" = { pos = [1 0 0] }
}

deleted_components = {    // remove components from prefab
    "obsolete-guid" = {}
}

children = { ... }        // brand new children
modified_children = { ... } // overrides applied recursively to prefab children
deleted_children = { ... }
```

### Why allow all three operations

- Users can change anything without “breaking” the prefab link.
- Large scenes stay maintainable: one tree change updates all instances unless explicitly overridden.

## Merge Semantics (Flattening)

To build the final entity used at runtime, we conceptually perform these steps for the whole prefab chain (prefab may have its own prefab, etc.):

1. Load prefab entity JSON (recursively flatten its prefab first).
2. Apply `components` (additive).
3. Apply `deleted_components` (remove matching GUIDs).
4. Apply `modified_components` (override fields on matching GUIDs).
5. Repeat the same three steps for `children`, traversing by GUID.

Memoize loaded prefabs to avoid re-reading the same files across thousands of entities in a level.

### Why this works

- Produces a fully realized entity definition (“merged view”) ready for compilation into the grouped binary format from Part 4.
- Keeps authoring flexible while keeping runtime simple—no live prefab links are needed after spawn.

### Limitations

- Deep prefab chains increase load time; cap chain depth or flatten at build time.
- Conflicting overrides (two levels modify the same field) should be detected and resolved during compile with clear diagnostics.

## Level Files vs Entity Files

- A level currently holds a table of entities by GUID:

```json
entities = {
    "guid-a" = { prefab = "entities/box" }
    "guid-b" = { prefab = "entities/box" }
    "guid-c" = { prefab = "entities/box" }
}
```

- Future direction: make the level itself an entity; children become the level contents. The same prefab machinery handles both cases.

## Compile-Time Memoization

Because prefab reuse is common, cache parsed prefab JSON keyed by path+mtime during compilation. This prevents O(N^2) parsing when many entities share the same prefab.

## Code-Level Summary of Compile Path

```cpp
Json flatten_entity(path):
    if cache.contains(path) return cache[path];
    Json e = parse_sjson(path);
    if (e.prefab) {
        Json base = flatten_entity(e.prefab);
        apply_add_delete_modify(base.components, e.components,
                                e.deleted_components, e.modified_components);
        apply_children(base.children, e.children,
                       e.deleted_children, e.modified_children);
        e = base;
    }
    cache[path] = e;
    return e;
```

The flattened JSON then feeds the binary compiler from Part 4 (group-by-component).

## Key Insights

- Prefabs are just entity resources plus a merge policy; keeping the merge rules explicit makes tooling and source control friendlier.
- GUID-keyed objects make collaborative editing tractable; avoid arrays for mergeable lists.
- Compile-time flattening removes runtime dependency on prefab links, simplifying streaming and save/load.
- Memoization is essential when thousands of instances share the same prefab.
