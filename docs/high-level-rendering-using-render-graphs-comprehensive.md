# High-Level Rendering Using Render Graphs (Comprehensive Rewrite)

## Abstract

This document deepens the original Render Graph proposal for The Machinery.

The objective is to make complex rendering pipelines easier to author and evolve while preserving low-level control over scheduling, synchronization, and platform-specific execution concerns (multi-queue, async compute, multi-GPU).

Render Graphs are presented as a high-level orchestration layer on top of a lower-level renderer command architecture.

---

## 1) Problem Context

Modern real-time rendering pipelines must handle:

- Heterogeneous hardware from mobile to multi-GPU desktop.
- Graphics + compute overlap opportunities.
- Rapidly changing algorithms (forward+, clustered, GPU-driven, etc.).
- Short iteration loops for visual and systems experimentation.

A purely hand-scheduled pass list becomes fragile as pipelines scale.

---

## 2) Core Concept

A Render Graph is composed from three building blocks:

1. **Passes** — units of declared read/write/create intent and executable work.
2. **Modules** — ordered pass collections with composition/extension mechanisms.
3. **Graph Builder** — dependency analyzer and scheduler.

```text
Passes -> Module -> Graph Builder -> execution plan
                                 -> CPU schedule
                                 -> GPU schedule
                                 -> barriers/copies/sync
```

---

## 3) Pass API and Semantics

## 3.1 API Shape

```c
struct tm_render_graph_pass_api {
    void (*setup_pass)(void *inst, tm_render_graph_builder_o *builder);
    void (*execute_pass)(void *inst, uint64_t sort_key,
                         tm_renderer_command_buffer_i *commands,
                         tm_render_graph_o *graph);
};
```

## 3.2 `setup_pass()`

Declares intent:

- Resources to create.
- Resources/CPU buffers to read.
- Resources/CPU buffers to write.
- Access mode and stage usage.
- Load behavior (load/discard/clear).
- Queue preference (graphics vs async compute).
- Eligibility for remote GPU execution.

### WHY

Declarative setup exposes dependencies before execution, enabling automatic planning.

### HOW

The pass publishes dependency metadata; no heavy work should run here.

### WHERE

Frame build phase, serial or lightly threaded pre-execution stage.

### LIMITATIONS

Incorrect declarations cause invalid schedules or hidden hazards.

## 3.3 `execute_pass()`

Emits actual command payload into renderer command buffers.

Important decoupling:

- CPU timing of `execute_pass()` is graph-scheduled.
- GPU order is ultimately decided by command sort keys and queue assignment.

This allows CPU and GPU schedules to be optimized differently.

---

## 4) Scheduling Dimensions

## 4.1 Multi-Queue (Graphics + Async Compute)

Pass can request async compute if it is compute-only.

```text
Graphics Queue: [GBuffer]----[Lighting]----[Composite]
Compute Queue :      [Cull]--[SSR]---[Denoise]
                          \   sync   /
```

### WHY

Overlap compute with graphics to reduce idle periods.

### LIMITATION

Potential gains depend on hardware queue concurrency and synchronization cost.

## 4.2 Multi-GPU

Passes may be flagged schedulable on other GPUs.

### WHY

Distribute heavy workloads.

### HOW

System inserts required cross-GPU resource copies and synchronization.

### LIMITATION

Copy overhead can erase gains; policy must be data-sensitive.

---

## 5) Modules, Composition, and Extension

A **Module** is an ordered collection of passes and extension points.

- Insertion order acts as a primary scheduling hint.
- Graph Builder can still reorder when dependencies require.
- Modules can be appended to modules.
- Extension points expose stable injection locations for external/custom workloads.

```text
Base Module:
  Pass A -> [Extension: AFTER_OPAQUE] -> Pass B

Game Module appends:
  CustomRefraction -> CustomFog

Effective order is resolved with dependency constraints.
```

### WHY

Encapsulates rendering feature chunks and supports “Lego-style” pipeline assembly.

### LIMITATION

Overuse of extension points can recreate hidden coupling if contracts are vague.

---

## 6) Sub-Modules for On-Demand Intra-Layer Work

Some effects (e.g., refraction/frosted glass) require copying/filtering currently written targets right before rendering specific objects.

A global one-time prepass is insufficient for layered refractive interactions.

### Proposed solution

**Sub-Modules**: module-like units that are not pre-scheduled globally, but scheduled dynamically by the material system inside a host layer context.

```text
Layer Pass (opaque/translucent)
  -> draw object 1
  -> inject Sub-Module (copy + blur)
  -> draw object 2 using filtered buffer
```

### WHY

Supports object-local “ad hoc” GPU work with correct state awareness.

### LIMITATION

Dynamic insertion increases scheduling complexity and requires strong state-tracking guarantees.

---

## 7) Graph Builder Responsibilities

After pass registration, `validate_and_build()` performs:

1. Validation (name collisions, malformed dependencies).
2. Root-pass discovery (externally required outputs).
3. Dependency traversal from roots.
4. Construction of CPU and GPU scheduling graphs.
5. Identification of cullable passes.
6. Resource lifetime extraction for transient allocations.
7. Barrier/sync/copy planning.

```text
[Root Passes]
     |
 dependency walk
     v
[Required pass set] -> [CPU DAG] + [GPU DAG]
                    -> [resource lifetimes]
                    -> [barrier/sync plan]
```

### Key insight

The system builds **two related but distinct plans**:

- CPU execution dependencies.
- GPU command dependencies.

This separation is essential for exploiting job parallelism while still producing valid GPU timelines.

---

## 8) Execute Phase

`execute()` runs pass execution through the job system as wide as dependencies allow.

Passes can spawn additional jobs and command buffers for expensive work (e.g., culling/traversal).

Output commands are then merged/sorted by the lower-level renderer architecture.

---

## 9) Code Example (Minimal Pattern)

```c
void setup_pass(MyPass *p, tm_render_graph_builder_o *b) {
    rg_read_texture(b, p->depth, TM_STAGE_PIXEL);
    rg_write_rt(b, p->linear_depth_rt, TM_LOAD_DISCARD);
    rg_allow_async_compute(b, false);
    rg_add_pass(b, p, /*root=*/false, /*layer=*/"post_linear_depth");
}

void execute_pass(MyPass *p, uint64_t sort_key,
                  tm_renderer_command_buffer_i *cmd,
                  tm_render_graph_o *g) {
    tm_renderer_api->set_render_target(cmd, p->linear_depth_rt, sort_key + 1);
    tm_renderer_api->draw_fullscreen_triangle(cmd, p->shader, sort_key + 2);
}
```

This demonstrates the setup/execute split and late GPU ordering.

---

## 10) Data-Driven vs Code-Driven Authoring

The architecture is API-first, but can be wrapped by data-driven front ends.

Original position: code-driven plugin authoring + hot reload can provide fast iteration without sacrificing expressiveness (loops, complex control flow).

### Practical reading

- Data-driven layers are useful for configuration-heavy composition.
- Code remains valuable for nontrivial procedural logic.
- A hybrid often wins.

---

## Key Insights

- Render Graphs turn dependency declaration into a scheduling asset.
- Setup/execute separation enables robust optimization and cleaner reasoning.
- Modules and extension points provide composability without forcing monolithic pipelines.
- CPU and GPU schedules must be modeled separately to unlock parallelism safely.
- Dynamic Sub-Modules address effects that cannot be expressed as static global pass order.
