# The Machinery Shader System (Part 2, Comprehensive Rewrite)

## Abstract

This document expands the second shader-system article, focusing on shader declarations and abstract binding.

The key shift is from “raw shader source files” to **composable declaration objects** that can be merged into final shaders while preserving backend freedom to change resource/constant binding models.

---

## 1) Why This Part Exists

Before discussing higher-level IO and variation management, a lower-level declaration model is required:

- Describe shader code and stage linkage.
- Describe resources/constants/state blocks in one schema.
- Compose reusable pieces through deterministic merge rules.

Without this layer, authoring reuse and backend refactoring become brittle.

---

## 2) `tm_shader_declaration_api` Model

A declaration object (`tm_shader_declaration_o`) may contain:

- Shader source code snippets.
- Stage-to-stage linkage declarations.
- Imports/exports for constants and resources.
- Pipeline state block declarations.
- Sampler state declarations.
- Stage attributes (e.g., compute group size).

All fields are optional.

Final shaders are assembled by stacking multiple declarations with merge rules.

```text
Decl A (base states + helpers)
Decl B (material imports + stage code)
Decl C (platform override)
        |
        v
Merged declaration -> compile
```

---

## 3) Example Declaration (Cleaned Pattern)

```json
{
  "depth_stencil_states": {
    "depth_test_enable": false,
    "depth_write_enable": false,
    "depth_compare_op": "greater"
  },
  "raster_states": {
    "polygon_mode": "fill"
  },
  "samplers": {
    "clamp_point": {
      "min_filter": "point",
      "mag_filter": "point",
      "mip_mode": "point",
      "address_u": "clamp",
      "address_v": "clamp",
      "address_w": "clamp"
    }
  },
  "imports": [
    { "name": "texture",  "type": "texture_2d" },
    { "name": "near_far", "type": "float2" },
    { "name": "sampler",  "type": "sampler", "sampler": "clamp_point" }
  ],
  "common": [[
    "float linearize(float d, float n, float f) { return (n*f)/(f - d*(f-n)); }"
  ]],
  "vertex_shader": {
    "import_system_semantics": ["vertex_id"],
    "exports": [{ "name": "uv", "type": "float2" }],
    "code": [["// fullscreen triangle vertex code"]]
  },
  "pixel_shader": {
    "exports": [{ "name": "color", "type": "float" }],
    "code": [["// sample depth and output linearized value"]]
  }
}
```

The original article uses this shape to demonstrate an intentionally simple full-screen linear-depth pass.

---

## 4) Generated Binding Abstraction (`load_` / `get_`)

From shader-author perspective:

- Constants are accessed via generated `load_<name>()` helpers.
- Resources are accessed via generated `get_<name>()` helpers.
- Arrays use index arguments to helper calls.

### WHY

Decouple authored shader code from concrete binding slots, descriptor layouts, and memory packing decisions.

### HOW

Compilation step injects helper declarations + backend-aware plumbing.

### WHERE

Used by all authored stage code that references imported data.

### LIMITATIONS

Debugging generated code paths can be harder than explicit bindings.

---

## 5) Stage Linkage Abstraction

Stage I/O structs and linkage glue are generated from:

- Previous stage `exports`.
- Requested system semantics (`import_system_semantics`, `export_system_semantics`).

Shader author writes to canonical `input` and `output` structs while packing/unpacking logic is generated.

```text
VS exports -----> generated interpolator layout -----> PS input
      \                                                /
       +----- author code sees stable structs --------+
```

### WHY

Allows experimentation with interpolator packing without rewriting shader source.

### LIMITATION

Generated linkage logic must remain transparent enough for diagnostics.

---

## 6) Merge Rules and Reuse Strategy

Multiple declarations are stacked and merged.

### Typical rules

- Code blocks (`[[ ... ]]`): concatenate in declaration order.
- State blocks: key-wise merge; later declarations override earlier values.

### Practical effect

Enables libraries of:

- Shared helper functions/macros.
- Baseline state presets.
- Reusable stage snippets.

This significantly reduces duplication and enables layered overrides.

---

## 7) Binding Implementation Reality (Vulkan Context)

The article describes an attempted fully bindless resource model per shader type, then a practical fallback:

- Intended: unbounded arrays per resource type in shared binder.
- Constraint encountered: unbounded indexing support differs by resource class.
- Current pragmatic model: one resource binder per shader instance.

Constants are packed per shader instance into a buffer; push constants carry indexing/selection data.

### WHY this matters

Architecture must tolerate backend capability surprises without breaking authored shaders.

### LIMITATION

Per-instance binders may increase descriptor management overhead versus ideal bindless models.

---

## 8) End-to-End Flow

```text
Author declarations (JSON/API)
        |
        v
Merge declarations (rules)
        |
        v
Generate:
  - helper accessors (load_/get_)
  - stage linkage structs/glue
  - final stage sources
        |
        v
Low-level compile (state + stage blobs)
        |
        v
Runtime shader instance with abstracted bindings
```

---

## 9) Code Example: Author-Facing Stage Snippet

```hlsl
Texture2D depth_tex = get_texture();
float2 nf = load_near_far();
float depth = depth_tex.Sample(get_sampler(), input.uv).x;
output.color = linearize(depth, nf.x, nf.y);
return output;
```

This code remains stable even if descriptor set layouts, buffer packing, or binding indices are reworked internally.

---

## 10) WHY / HOW / WHERE / LIMITATIONS Summary

- **WHY**: preserve long-term flexibility in binding/layout strategy and reduce shader duplication.
- **HOW**: composable declaration objects + generated access/linkage code + deterministic merging.
- **WHERE**: foundation for upcoming shader-IO abstractions, material variants, and update-frequency grouping.
- **LIMITATIONS**: generated indirection complexity, backend capability constraints, evolving descriptor models.

---

## Key Insights

- A declaration system is a structural prerequisite for scalable shader authoring.
- Binding abstraction is primarily a refactorability strategy, not just convenience.
- Mergeable declarations convert shader authoring from monolithic files to reusable composition.
- Practical backend constraints will force implementation changes; stable author-facing contracts are the protection mechanism.
