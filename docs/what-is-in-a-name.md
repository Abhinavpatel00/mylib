# What Is In a Name? — Robust Resource Referencing

Referencing resources looks trivial until renames, moves, merges, and tooling collide. This article unpacks path vs GUID vs hybrid schemes, explaining why each fails in practice and how to choose deliberately.

## Context

- Resources live in a version-controlled directory tree.
- References appear inside other resources *and* in code.
- We need stable links across renames, copies, localization variants, and distributed workflows.

## Option 1: By Path

```
texture = "textures/flowers/rose"
```

### Why it works

- Human-readable; failures are diagnosable.
- No sidecar metadata required.
- Code references stay meaningful (`spawn_unit("entities/ogre")`).

### Where it applies

- Pipelines with strong “rename via tool” discipline.
- Projects where readability and mergeability trump maximal automation.

### Limitations

- Renames break references unless a tool updates them or redirects exist.
- Canonicalization is mandatory: reject `././../` chaos so hashes and diffs stay stable.

## Option 2: By GUID

```
guid = "a54abf2e-d4a1-4f21-a0e5-8b2837b3b0e6"
```

### Why it’s attractive

- Renames/moves do not break references.
- No scanning or patching required when files move.

### Hidden pitfalls

- Copying a file duplicates the GUID → conflict; needs a resolver tool.
- Many asset formats lack a place to store the GUID, forcing sidecar metadata.
- References become opaque; when they break you have no clue what was intended.
- Code readability suffers; debugging broken content is slower.

## Option 3: Human “Name” Separate from Path

```
name = "garden-rose"
texture = "garden-rose"
```

### Why it rarely helps

- Renaming the *name* recreates the same breakage problem as paths.
- Now you juggle two identifiers (filename vs logical name) that can diverge.

## Option 4: Path + GUID Hybrid

```json
texture = {
    path = "textures/flowers/rose"
    guid = "a54abf2e-d4a1-4f21-a0e5-8b2837b3b0e6"
}
```

### Benefits

- GUID preserves linkage across moves.
- Path remains readable for debugging and code references.

### Costs

- Still needs metadata to store the GUID.
- Path can drift unless a rename tool patches references.
- More complexity (two identifiers to keep consistent).

## Distributed Workflow Hazards

- **Rename vs new reference race**: if Alice renames while Bob adds a reference, path-based links break unless the tool can reconcile. GUIDs survive renames but offer no clues when assets are missing.
- **Copy/paste**: duplicated GUIDs create silent conflicts; detect and regenerate GUIDs on copy.

## Localization and Variants

Path-based systems often use filename conventions (`rose.fr.dds`). GUID-only schemes must invent an equivalent mapping layer. This hidden complexity is easy to underestimate.

## Practical Guidance

1. **Prefer canonical paths** if transparency and debuggability matter and you already have (or can build) a rename-aware tool that patches references.
2. **Use GUIDs** when assets move frequently across large repos and you can enforce unique GUID generation and conflict checks on copy.
3. **Hybrid** only if you truly need both properties and can tolerate the extra tooling and metadata.
4. Regardless of scheme, enforce canonicalization and provide fast reverse-lookup tools (who references X?).

## Why Bitsquid Sticks with Paths

- Readability aids debugging, diffing, and merges.
- Existing dependency checker already patches references on rename; adding a reference cache makes it fast enough.
- Avoids sidecar metadata sprawl and opaque identifiers in code and content.

## Key Insights

- The “best” identifier depends on tooling maturity and collaboration style, not just theoretical safety.
- Opaque IDs trade easier renames for harder diagnostics; make that trade consciously.
- Whatever scheme you pick, invest in a robust rename/update tool and a reference index—you will need both.
