# The Machinery Shader System (Part 3) — Comprehensive Rewrite

## Scope

This document expands the original Part 3 article into a deeper technical treatment of:

- Shader variation generation and runtime selection.
- Grouping constants/resources by update frequency.
- A unified “system” abstraction for shader code injection + data binding.
- A higher-level Material layer for multi-pass scheduling and binding unification.

The focus is practical architecture: what problem each concept solves, how it works, where it should be used, and what constraints it introduces.

---

## 1) Problem Framing: Why a New Shader Abstraction?

Traditional real-time shader pipelines accumulate complexity in three places:

1. **Variation explosion**: feature combinations (skinning, light maps, transparency, quality levels, etc.) multiply quickly.
2. **Runtime selection logic**: selecting the correct variant becomes brittle and ad-hoc.
3. **Data lifetime mismatch**: some inputs change once per frame, some per view, some per draw/object.

The core insight is that these are not separate problems. They can be solved by one compositional concept: **shader systems**.

### Key Insight Summary

- A shader variation can be described by *which systems are active*.
- A system can carry both code and bindings, so variant identity and data provisioning stay aligned.
- Update-frequency differences become a data-routing concern rather than a shader-authoring burden.

---

## 2) Shader Systems: Core Model

A shader author declares one or many variations to compile. Each variation references one or many systems.

A **system** contributes:

- Shader code (injection/imported functions/macros).
- Constants in its own constant-buffer block.
- Resources in its own resource-binder block.

The system declaration uses the same building blocks as a shader declaration, but compiles into a non-renderable object (e.g., `tm_shader_system_o`) used for instancing and updating constants/resources.

### WHY

- Keeps feature logic modular instead of embedding every feature in monolithic shader files.
- Aligns variant identity with actual data dependencies.
- Allows independent ownership of data domains (frame, view, lighting, deformation, etc.).

### HOW

1. Define a system declaration.
2. Compile it into a runtime system object.
3. Instantiate system constants/resources as needed.
4. Activate systems in a context (global scope) or pass explicitly (high-frequency scope).
5. Select shader variation from the active-system signature.

### WHERE

- Frame-global parameters (time, frame index).
- View/camera parameters.
- Lighting environment data.
- Optional feature toggles (quality tier, transparency mode, platform specialization).

### LIMITATIONS

- System granularity must be curated; too many tiny systems can increase bookkeeping.
- Variant selection strategy must remain stable and debuggable.
- Binding layout policy must prevent accidental duplication across systems.

---

## 3) Context Stack for Low-Frequency Data

For data that changes infrequently (frame/view/scene scope), systems are activated through a stack-like context object (`tm_shader_system_context_o`).

The context is propagated to rendering systems as immutable (`const`) data.

If a subsystem needs a temporary override (e.g., enable one extra system), it clones the context, mutates the clone, and passes the clone onward.

### WHY

- Enforces safe propagation in heavily parallel execution.
- Makes scope explicit: push/pop semantics naturally match frame/view/layer traversal.
- Prevents accidental global mutation races.

### HOW (Step-by-Step)

1. Build base context for frame.
2. Push frame-level systems.
3. For each view/layer, clone and push view/layer systems.
4. Pass context to draw/dispatch generation.
5. Pop/discard scoped contexts as traversal unwinds.

### WHERE

- Render graph traversal.
- Parallel culling + submission pipelines.
- Multi-camera setups where per-view overrides are common.

### LIMITATIONS

- Frequent cloning may increase CPU overhead if done carelessly.
- Deep nesting can make debugging active-system state harder without tooling.

### ASCII Diagram

```text
Frame Context
  ├─ [FrameSystem]
  ├─ View A Context (clone + ViewSystemA)
  │    ├─ Layer GBuffer (clone + DeferredSystem)
  │    └─ Layer Transparency (clone + TransparencySystem)
  └─ View B Context (clone + ViewSystemB)
       └─ Layer Shadow (clone + ShadowSystem)
```

---

## 4) Variation Selection: Ordered List vs Bitmask Signature

A strict ordered-list lookup requires every variation to list dependencies in the exact global order, including unused systems. This creates tight implicit coupling.

A looser approach assigns each system a unique bit in a (potentially arbitrary-length) bitmask. Active systems form a signature used to find compatible variants.

### WHY

- Reduces authoring friction.
- Decouples shader declarations from global activation order.
- Makes introduction of new systems safer.

### HOW

1. Assign each system a stable bit index.
2. Build active-system bitmask from context.
3. During material/shader lookup, match variants by required bits.
4. Optional: include quality/platform systems in the same mask mechanism.

### WHERE

- Runtime variant resolution.
- User settings / hardware capability specialization.

### LIMITATIONS

- Requires robust policy for bit allocation/versioning.
- Matching semantics need clarity (exact match vs required-subset match).
- Large system sets may require multi-word bitsets and careful hashing.

### ASCII Diagram

```text
Active systems:
  frame | view | deferred | skinning
Bitset:
  0001 0010 0101 1000 ...

Variant A requires: frame|view|deferred        -> match
Variant B requires: frame|view|forward         -> no match
Variant C requires: frame|view|deferred|skinning -> match
```

---

## 5) Higher Update Frequencies: Object/Instance-Scoped Systems

Some data changes too frequently for context-stack push/pop semantics (e.g., skinning or morph targets varying per instance).

The same system abstraction still works: instantiate per-object constants/resources and pass them explicitly into shader-selection/assembly calls instead of context activation.

### WHY

- Preserves one conceptual model across low- and high-frequency data.
- Avoids forcing per-object churn into global context state.

### HOW

1. Keep low-frequency systems in context.
2. Build per-instance system instances for deformation data.
3. Feed both sets into the selection/binding assembly path.
4. Emit final bind packet for draw/dispatch.

### WHERE

- Character animation.
- Per-object procedural deformation.
- Object-local data reused across multiple views/material passes.

### LIMITATIONS

- Higher per-draw assembly cost if not cached.
- Lifetime management of many system instances must be tight.

---

## 6) Introducing Tier 1: Materials as Scheduling + Unification Layer

The article separates concerns into two tiers:

- **Tier 0**: shader systems and variant compilation/selection.
- **Tier 1**: **Material**, a higher-level object representing multiple shader entries mapped to frame execution points.

A Material element references:

- Target execution location (layer/render-graph pass).
- Shader declaration to use.
- Systems required for that material pass.

### WHY

- Avoids overloading “shader” with multi-pass scheduling semantics.
- Makes frame placement explicit via render-graph linkage.
- Supports one asset describing multiple passes (g-buffer, emissive, transparency, shadow, picking).

### HOW

1. Author material entries (layer + shader + systems).
2. Compile referenced shader declarations into required variations.
3. Build shared binding metadata across all material shaders.
4. At runtime, gather all shader variants that match active systems (not just first hit).

### WHERE

- Deferred + forward hybrid pipelines.
- Multi-pass object rendering.
- Selection/picking and shadow variants from same authored source.

### LIMITATIONS

- Material compilation can be heavier due to cross-pass superset analysis.
- Debugging “why this pass executed” requires good introspection.

### Example Material Representation

```json
{
  "material": [
    { "layer": "g-buffer",      "shader": "default", "systems": ["deferred"] },
    { "layer": "emissive",      "shader": "default", "systems": ["emissive"] },
    { "layer": "transparency",  "shader": "default", "systems": ["transparency"] },
    { "layer": "opaque_forward", "shader": "default", "systems": ["forward"] },
    {                          "shader": "default", "systems": ["shadow_mapping"] },
    {                          "shader": "picking", "systems": ["picking"] }
  ]
}
```

---

## 7) Shared Bindings Across Material Passes

A critical design point is that all compiled shaders in a material share a unified binding representation:

- A **superset** constant-buffer layout of all referenced constants.
- A **superset** resource-binder layout of all referenced resources.

### WHY

Without this, overlapping parameters used by multiple passes must be broadcast into separate buffers/binders manually, causing duplication and error-prone updates.

### HOW

1. During material compile, aggregate referenced constants/resources across passes.
2. Build canonical slots/offsets.
3. Generate per-pass shader views into that canonical representation.

### WHERE

- Multi-pass materials with shared inputs (albedo, transforms, feature toggles).

### LIMITATIONS

- Superset layouts can include rarely used fields (memory overhead).
- Layout evolution must remain backward-compatible or versioned.

### ASCII Diagram

```text
Pass A (g-buffer)   ┐
Pass B (emissive)   ├──> Material Compiler -> Unified Constant/Resource Layout
Pass C (shadow)     ┘

Runtime update once -> visible to all relevant passes
```

---

## 8) End-to-End Runtime Flow

```text
[Authoring]
  Shader declarations + System declarations + Material entries
      |
      v
[Compilation]
  Build system objects
  Build shader variants
  Build material shared bindings
      |
      v
[Frame Runtime]
  Build context stack (frame/view/layer)
  Build per-instance system data
  Resolve matching variants
  Bind unified material resources/constants
  Submit draw/dispatch
```

---

## 9) Practical Implementation Guidance

### Variant Matching Policy

Define matching semantics early:

- **Exact-match**: active bits must equal variant bits.
- **Subset-match**: variant-required bits must be subset of active bits.

Subset matching is usually more composable, but requires deterministic tie-break rules.

### Data Lifetime & Caching

- Cache resolved variant handles by `(material, active_bitset_signature)`.
- Separate cache keys for per-instance data where needed.
- Keep context clone operations shallow and immutable-friendly.

### Debuggability

Add inspection output for:

- Active systems at draw time.
- Chosen variant(s) and reason.
- Missing required systems.
- Material pass execution decisions.

---

## 10) Design Trade-Offs

1. **Flexibility vs Predictability**
   - More composability can make behavior less obvious without tooling.
2. **Superset Unification vs Memory Footprint**
   - Fewer update calls, but potentially larger binding payloads.
3. **Unified Abstraction vs Cost Transparency**
   - Same interface for low/high frequency data is elegant, but runtime costs differ.

---

## 11) Key Insights (Condensed)

- Treat features as **systems**, not hardcoded shader branches.
- Use **bitmask signatures** to decouple variation selection from fragile ordering.
- Keep low-frequency data in an immutable **context stack**; pass high-frequency data explicitly.
- Introduce **materials** as tier-1 objects for multi-pass scheduling and binding unification.
- Build shared binding layouts to avoid duplicated resource/constant broadcasting.

---

## 12) Minimal Pseudocode Sketch

```cpp
// Build active signature.
Bitset active = context.active_system_bits();
active |= instance_systems.required_bits();

// Resolve all material passes that match current signature.
auto passes = material.resolve_passes(active);

for (auto &pass : passes) {
    auto variant = pass.shader.resolve_variant(active);
    BindPacket bind = material.build_bind_packet(pass, context, instance_systems);
    cmd.bind_pipeline(variant.pipeline);
    cmd.bind_resources(bind.resources);
    cmd.bind_constants(bind.constants);
    cmd.draw(draw_args);
}
```

This captures the architecture intent: variant identity and data provisioning are driven by the same system set.
