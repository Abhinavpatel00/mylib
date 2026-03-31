# The Machinery Shader System (Part 1, Comprehensive Rewrite)

## Abstract

This document expands the first shader-system article into a detailed technical design review.

The central problem is not just “compile shader code.” A production shader system must coordinate:

- Authoring model and variation generation.
- Resource/constant binding contracts.
- Pipeline state composition.
- Runtime loading, reloading, and platform portability.

The approach here builds a low-level, backend-implemented compiler interface first, then layers high-level authoring abstractions later.

---

## 1) Why Shader Systems Are Hard

A shader system tends to become a cross-cutting dependency for nearly all rendering subsystems.

### Core pressures

- Different rendering subsystems want different binding/update strategies.
- Variation space can explode compile times.
- Front-end expressiveness and backend refactorability are in tension.
- Platform/API differences keep moving the goalposts.

```text
Authoring UX <-> Compilation <-> Runtime Binding <-> Backend API
        ^                                   |
        +---------- long-term coupling -----+
```

### Practical takeaway

A “perfect” shader system is unlikely; the real target is a stable, evolvable compromise.

---

## 2) System Goals (Two Tiers)

## 2.1 Tier 0: Shaders

A **Shader** is close to a complete pipeline state object:

- Active shader stages.
- Stage linkage.
- Relevant pipeline states.

No multi-pass scheduling or context switching at this tier.

## 2.2 Tier 1: Materials

A **Material** groups one or more shaders and introduces:

- Multi-pass sequencing / render-layer scheduling.
- Execution contexts (e.g., viewport shading vs shadow map path).

```text
Material
  |- Context: main_view -> [Shader A, Shader B]
  |- Context: shadow    -> [Shader C]
```

### WHY this split

Separates low-level correctness/portability concerns from high-level content orchestration.

### LIMITATION

Requires disciplined boundaries; features can be tempting to add in the wrong tier.

---

## 3) Technical Design Goals

- Group constants/resources by update frequency (frame/view/object/material).
- Avoid duplicated stage-local copies of shared data.
- Support efficient batched updates.
- Hide low-level memory/binding layout from authored shader code.
- Enable rapid iteration + hot reload.
- Align with explicit APIs (Vulkan/DX12) and bindless/GPU-driven trajectories.

---

## 4) Low-Level Runtime Shader Resource

The runtime render resource for a shader is a bundle of backend-specific blobs:

```c
enum tm_renderer_shader_stage {
    TM_RENDERER_SHADER_STAGE_VERTEX,
    TM_RENDERER_SHADER_STAGE_TESS_CONTROL,
    TM_RENDERER_SHADER_STAGE_TESS_EVAL,
    TM_RENDERER_SHADER_STAGE_GEOMETRY,
    TM_RENDERER_SHADER_STAGE_PIXEL,
    TM_RENDERER_SHADER_STAGE_COMPUTE,
    TM_RENDERER_SHADER_STAGE_MAX
};

typedef struct tm_renderer_shader_blob_t {
    uint64_t size;
    uint8_t *data;
} tm_renderer_shader_blob_t;

typedef struct tm_renderer_shader_t {
    tm_renderer_shader_blob_t raster_states;
    tm_renderer_shader_blob_t depth_stencil_states;
    tm_renderer_shader_blob_t blend_states;
    tm_renderer_shader_blob_t multi_sample_states;
    tm_renderer_shader_blob_t stages[TM_RENDERER_SHADER_STAGE_MAX];
} tm_renderer_shader_t;
```

This is intentionally backend-facing: blobs are opaque at high level and interpreted by the active backend.

---

## 5) `tm_renderer_shader_compiler_api`

Each backend implements a low-level compiler interface with three jobs:

1. Reflect/enumerate supported state blocks and valid values.
2. Compile state blocks into backend-native binary form.
3. Compile stage source into backend-native shader binary.

### WHY backend-implemented

State capabilities differ by API/vendor/extensions; reflection keeps the front-end adaptable.

### WHERE used

Shader import/build pipeline, hot reload path, editor tooling.

### LIMITATIONS

Cross-backend consistency depends on how well capability differences are normalized.

---

## 6) State Block Reflection and Compilation

A state block is assembled as key/value pairs and compiled through backend API.

```c
typedef struct tm_renderer_state_value_pair_t {
    uint32_t state;
    union {
        uint32_t enum_value;
        uint32_t uint32_value;
        float    float_value;
    };
} tm_renderer_state_value_pair_t;
```

### Flow

```text
enumerate blocks/states -> build key/value array -> compile_state_block()
                                          |
                                          v
                              backend-specific blob
```

### WHY this model

- Front-end can be data-driven and validated.
- Backend can expose extra API-specific states.

### LIMITATION

If many backend-specific states are used heavily, shader portability drops.

---

## 7) Stage Compilation Interface

```c
tm_renderer_shader_blob_t (*compile_shader)(
    tm_renderer_shader_compiler_o *inst,
    const char *source,
    const char *entry_point,
    uint32_t source_language,
    uint32_t stage);
```

Inputs define source text, language (HLSL/GLSL/etc.), entry point, and target stage.

### WHY this API shape

Small and explicit; avoids policy-heavy compile interface at low level.

### LIMITATION

Higher-level concerns (macro sets, variants, include graphs) must be handled above this layer.

---

## 8) Language Choice and Toolchain Strategy

Original direction: author in HLSL, generate SPIR-V, keep path open to DX12 backend.

This is a practical interoperability choice, not ideological preference.

### Real-world constraints

- Toolchains evolve quickly.
- Different industries use different shading languages.
- A single universal source language is unlikely.

### Architecture consequence

Treat source language as a compile-time parameter and keep backend compile path swappable.

---

## 9) End-to-End Compilation Sketch

```text
[Declaration/Data]
      |
      v
[High-level shader system]
  - chooses stages
  - resolves states
  - generates stage source
      |
      v
[tm_renderer_shader_compiler_api]
  - compile_state_block()
  - compile_shader(stage)
      |
      v
[tm_renderer_shader_t blobs]
      |
      v
[Render backend creates/uses pipeline artifacts]
```

---

## 10) WHY / HOW / WHERE / LIMITATIONS Summary

- **WHY**: unify shader authoring/runtime management across rendering systems and platforms.
- **HOW**: low-level backend compiler API with reflective state blocks + stage compilation.
- **WHERE**: foundation for all shader assets used by render graph driven passes/materials.
- **LIMITATIONS**: variant explosion, backend divergence, toolchain churn, and unavoidable front-end/back-end tension.

---

## Key Insights

- Start with low-level compile/reflection contracts before building higher-level authoring systems.
- Keep runtime shader representation backend-opaque but structurally consistent.
- State reflection is a portability and extensibility mechanism, not only editor convenience.
- Shader language/toolchain choices should remain replaceable architecture decisions.
