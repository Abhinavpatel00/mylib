# The Story Behind The Truth: Designing a Data Model (Comprehensive Rewrite)

This document is a deep technical expansion of the design rationale behind The Truth data model.

The fundamental question:

> If tools, runtime, collaboration, undo, and versioning all touch the same project data, what data model gives the best long-term leverage?

---

## 1) What a data model is (and why it matters)

A data model is the structured contract for representing, validating, transforming, and persisting application state.

Possible families:

- textual structured (`JSON`, `XML` + schema)
- binary schema-based (`ASN.1`, protobuf-like)
- relational DB
- document/NoSQL
- content-addressed object graphs (git-like)
- custom domain-specific hybrids

A single application may use multiple representations for the same conceptual data:

```text
Authoring representation -> Working in-memory representation -> Runtime-optimized representation
```

---

## 2) Why games/tools are different from typical apps

Large projects contain gigabytes of data and strict startup/runtime budgets.

You usually cannot afford:

- parse-and-transform everything at startup
- expensive schema conversion at every launch

So pipelines typically introduce a compile/bake stage:

```text
Editable source assets -> data compile -> runtime streamable package
```

This is operationally effective but introduces model fragmentation unless the overarching data model is deliberate.

---

## 3) Why invest in model-level features

If capabilities are implemented at model layer once, all subsystems inherit them.

### Features that benefit from model-level treatment

1. backward/forward compatibility
2. dependency tracking
3. copy/paste cloning semantics
4. undo/redo history semantics
5. realtime collaboration replication
6. offline collaboration + mergeability

Without model support, each tool subsystem reimplements these behaviors inconsistently.

---

## 4) Compatibility example: schema evolution

Without model support, code tends toward version branching:

```c
if (version == V1_0) { ... }
else if (version == V1_1) { ... }
```

With structured model + defaults + unknown-field tolerance:

- backward compatibility often becomes natural
- forward compatibility may be partially possible (ignore unknown fields)

Good model design shifts effort from per-tool ad hoc branching to declarative evolution policy.

---

## 5) Historical baseline: Bitsquid/Stingray model

### 5.1 Architecture shape

- authoritative editor/source data on disk (mostly JSON)
- multiple semi-independent tools
- compile step produces runtime-optimized package data

ASCII view:

```text
[Tool A]--\
[Tool B]---(JSON/files on disk)---[Compiler]---[Runtime package]
[Tool C]--/
```

### 5.2 Strengths

- subsystem independence
- clear runtime/editor separation
- easy to swap isolated tools

### 5.3 Pain points

- duplicated functionality across tools
- weak real-time inter-tool coherence
- delayed viewport/runtime reflection (save + compile needed)
- complex hacks for “live-ish” behavior
- disk-centric model weak for in-memory operations (undo/copy/realtime sync)

---

## 6) The Truth model: object/property memory-authoritative core

### 6.1 Core abstraction

Data represented as typed objects with typed properties.

Property kinds include:

- scalar primitives
- strings
- references
- sub-objects
- sets/lists of references/subobjects
- opaque buffers

### 6.2 Central shift

**Authoritative state is in memory**, not files.

Disk formats become persistence/projection formats, not the active canonical synchronization substrate.

ASCII:

```text
          +----------------------+
Tools --->|                      |
Runtime ->|      The Truth       |<--- Plugins
          |   (in-memory model)  |
          +----------+-----------+
                     |
              Save/Load projections
         (git-friendly text, fast binary, etc.)
```

---

## 7) Why memory-authoritative unlocks capabilities

With shared in-memory model and uniform mutation events:

- tool windows auto-observe each other’s edits
- runtime/editor can co-edit same live state
- copy/paste maps to object clone semantics
- undo/redo maps to model delta history
- realtime collaboration maps to mutation stream replication

This collapses large swaths of bespoke editor glue code.

---

## 8) Object data vs buffer data split

A key design distinction:

1. **Object data**: semantically meaningful fields (mergeable/introspectable)
2. **Buffer data**: opaque bulk blobs (textures/meshes/audio payloads)

Why this split matters:

- object representation has metadata overhead and semantics cost
- buffers dominate bytes but need throughput/storage efficiency

Rule of thumb:

- high-structure, editor-facing graph/config/state => object data
- large homogeneous numeric/byte payload => buffer data

---

## 9) References: IDs vs paths

### ID references

Pros:
- stable identity
- fast resolution
- easier integrity guarantees

Cons:
- less human-readable
- less naturally contextual/dynamic

### Path references

Pros:
- human-readable/editable
- context-relative resolution possible
- can represent not-yet-existing targets

Cons:
- fragile under rename/move
- slower resolution
- harder global integrity guarantees

Pragmatic model:

- use IDs as default strong references
- allow path-like/dynamic references in targeted scripting or contextual resolution domains

---

## 10) Sub-objects and ownership semantics

Sub-object is not just reference; it encodes ownership/lifecycle semantics.

Typical behavior:

- cloning owner clones sub-object graph
- plain references remain references (not deep-copied)

This distinction is essential for predictable copy/paste, prefab-like instancing, and dependency cleanup.

---

## 11) Collaboration model implications

### Realtime collaboration

If edits are normalized as model deltas, replication is straightforward:

```text
(op, object_id, property_id, old_value, new_value, metadata)
```

### Offline collaboration (VCS)

Still requires merge-friendly persistence projections.
Memory-authoritative core does not remove need for text/merge strategy; it changes where semantics live.

---

## 12) Performance and scale tradeoffs

Truth-like object model is flexible, but not free:

- less compact than tightly packed runtime buffers
- more pointer indirection and metadata
- requires runtime extraction/projection for critical paths

Hence dual-representation strategy remains valid:

```text
authoring/truth graph -> bake/extract -> runtime packed formats
```

---

## 13) Partial loading and huge projects

For very large projects, full in-memory residency is infeasible.

Needed mechanisms:

1. segmented namespaces/chunks
2. lazy load with dependency-aware prefetch
3. unloaded placeholder handles
4. deterministic invalidation and reload semantics

A mature model must define behavior for “referenced but not currently loaded” objects.

---

## 14) Disk projection strategy (single model, multiple encodings)

You generally want at least two persistence projections:

1. **merge-friendly textual projection** for collaboration
2. **fast-load binary projection** for local speed

Both map to/from same in-memory canonical graph.

ASCII:

```text
            +-----------------+
            |   The Truth     |
            +----+-------+----+
                 |       |
     text projection   binary projection
   (merge, review)       (fast load)
```

---

## 15) Data model as product leverage

The biggest payoff is organizational, not just technical:

- new tools/plugins inherit undo/clone/ref integrity/collab from platform
- less bespoke UI-state plumbing per feature
- consistent behavior across ecosystem

This reduces “30 minutes runtime + 1 week tooling” asymmetry.

---

## 16) Risks and unresolved tensions

Even with strong model architecture, open questions remain:

1. where exactly runtime extraction happens and how incremental it is
2. memory budget strategy for giant datasets
3. merge semantics for complex object graph edits
4. balancing strict references with useful dynamic contextual links
5. ensuring schema evolution remains disciplined over years

These are not one-time choices; they are ongoing governance problems.

---

## 17) Migration and evolution philosophy

A pragmatic approach:

- define strong direction and invariants
- implement early
- collect usage reality
- iterate/refactor with measured rewrites

Over-planning before implementation often optimizes for assumptions instead of real workloads.

---

## 18) Minimal conceptual API sketch

```c
id_t truth_create_object(truth_t *tt, type_t type);
void truth_destroy_object(truth_t *tt, id_t id);

const object_o *truth_read(truth_t *tt, id_t id);
object_o *truth_write(truth_t *tt, id_t id);
void truth_commit(truth_t *tt, object_o *w);

bool truth_set_ref(truth_t *tt, object_o *w, prop_t p, id_t target);
id_t truth_get_ref(const object_o *r, prop_t p);

id_t truth_clone_subgraph(truth_t *tt, id_t root);
```

Operations like copy/paste, undo, and replication can be layered over this uniform mutation/read protocol.

---

## 19) Practical heuristics for adopting this model

1. keep schema explicit and introspectable
2. separate object semantics from opaque buffers early
3. make mutation stream first-class (for undo/collab)
4. define ownership vs reference semantics precisely
5. design persistence as projection, not canonical authority
6. add strict validation and invariant checks in dev builds

---

## 20) Final takeaway

The major conceptual move is:

> Put semantics in one shared in-memory model, then project out to multiple persistence/runtime forms as needed.

This turns data modeling from “file format choice” into “platform capability choice.”

When done well, The Truth-style architecture is less about storing bytes and more about enabling reliable, scalable tool/runtime behavior across an entire engine ecosystem.
