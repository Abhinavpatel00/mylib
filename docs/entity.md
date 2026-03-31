# Entity System Notes — Series Overview and Prefab Editing Rules

This note ties together the entity series and records the rules for applying prefab modifications safely.

## Series Map

- Part 1 — Entity IDs and lifetime (weak handles, generations).
- Part 2 — Components (SoA storage, lookup, GC).
- Part 3 — Transform component (hierarchies, immediate updates).
- Part 4 — Entity resources (grouped spawn, component batching).
- Part 5 — Prefabs (overrides, deletion, merge semantics).

## Prefab Editing Rules

When editing an entity that instantiates a prefab:

- **Add component to child**: perform the edit on that specific child entity, not on the parent prefab, unless you want all instances to inherit it.
- **Delete/modify scope**: `deleted_*` and `modified_*` apply only to matching GUIDs in the prefab chain; they never touch siblings unless explicitly targeted.
- **Locality**: changes live in the entity resource that instantiates the prefab; the original prefab file remains unchanged.

### Example: Add a component to a child instance

```json
prefab = "entities/house"

modified_children = [
    {
        id = "child-window-guid"
        components = [
            {
                type = "light"
                color = [1 0.9 0.8]
                intensity = 2.0
            }
        ]
    }
]
```

- `id` references the child’s GUID in the prefab.
- The new component is local to this instantiation; other houses stay unchanged.

## Key Insights

- Keep edits scoped: modify the smallest entity that expresses the change.
- Use GUID-targeted modifications to keep source control merges stable.
- Treat the prefab chain as data to be merged at build time, not as runtime state.
