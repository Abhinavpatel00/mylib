# A Modern Rendering Architecture (Comprehensive Rewrite)

## Abstract

This document expands the original architecture proposal for a modern, explicit renderer in The Machinery.

The core design target is a thin, high-performance abstraction over explicit graphics APIs (initially Vulkan), with first-class support for:

- Flexible usage patterns (not only classic triangle rendering).
- Low-overhead command generation.
- Massive CPU parallelism.
- Explicit scheduling across many command buffers and devices.

The architecture separates **resource lifetime work** from **execution work**, then adds a deterministic sorting model to enable global ordering without forcing global serialization.

---

## 1) Design Goals

### 1.1 Flexibility

**WHY**  
Rendering is no longer a single graphics queue pushing draw calls. Engines need graphics + compute + tooling workloads, often in the same frame.

**HOW**  
Provide a backend-agnostic API that can represent draw, compute, resource updates, and device-aware execution.

**WHERE**  
Used by runtime rendering pipelines, offline GPU tooling, and experimental workloads (procedural generation, filtering, GPU-side utilities).

**LIMITATIONS**  
Flexibility raises API surface area; if not carefully constrained, complexity leaks to users.

### 1.2 Simplicity (Thin Abstraction)

**WHY**  
High-level convenience wrappers often hide costs, block optimization, and become difficult to map to new APIs.

**HOW**  
Model concepts close to explicit APIs: command buffers, explicit ownership, explicit submission, explicit affinity.

**WHERE**  
Critical in backend implementation (Vulkan/DX12 style) and performance debugging.

**LIMITATIONS**  
A thin API is not always easy for new users; “simple internals” does not imply “minimal user learning curve.”

### 1.3 Massive Parallelism

**WHY**  
Modern CPUs expose many cores; rendering prep must scale with job systems.

**HOW**  
Allow many command buffers/resource command buffers in flight, each written by one thread at a time.

**WHERE**  
Scene traversal, visibility, command generation, transient upload staging.

**LIMITATIONS**  
Requires strict ownership discipline. Accidental sharing causes data races and nondeterminism.

### 1.4 Scheduling as a First-Class Concern

**WHY**  
Without built-in scheduling semantics, users must build sorting/merge systems on top, often introducing extra buffering and synchronization overhead.

**HOW**  
Each regular command carries a 64-bit global sort key. Submission merges commands from many buffers and sorts once.

**WHERE**  
Cross-system coordination: render passes, post-process stacks, async jobs producing GPU work.

**LIMITATIONS**  
Sort-key design becomes a contract: poor key layout yields unstable ordering or unnecessary constraints.

---

## 2) High-Level Plugin Topology

```text
+---------------------------+
| Engine / Tools Systems    |
| (render features, jobs)   |
+-------------+-------------+
              |
              v
+---------------------------+
| renderer (API plugin)     |
| - backend interface       |
| - command APIs            |
+-------------+-------------+
              |
              v
+---------------------------+
| render-backend-vulkan     |
| - translates to Vulkan    |
| - owns final consume/reuse|
+---------------------------+
```

Key point: backend consumes renderer-produced data and decides when buffers are fully consumed and recyclable.

---

## 3) Core API Layers

## 3.1 `tm_renderer_backend_i`

Backend-facing interface exposed to higher-level systems.

### Responsibilities

1. Swap chain lifecycle (create/destroy/resize/present).
2. Command buffer and resource command buffer lifecycle + submission.
3. Device-aware resource ownership through `device_affinity` masks.

### Multi-GPU Affinity Model

```text
GPU0 -> bit 0  (0b0001)
GPU1 -> bit 1  (0b0010)
GPU2 -> bit 2  (0b0100)
...
Affinity for GPU0+GPU2 = 0b0101
```

This enables explicit ownership and submission targeting one or more GPUs.

### Why lifecycle calls live in backend

Pools can only recycle command buffers once GPU/backend consumption is complete. The backend is the authority for that completion boundary.

---

## 3.2 `tm_renderer_resource_command_buffer_i`

Resource-only stream for creation/destruction of GPU resources.

### Resource classes

- `tm_renderer_buffer_t` (linear buffers: vertex/index/constant/raw).
- `tm_renderer_image_buffer_t` (textures/render targets).
- `tm_renderer_shader_t` (backend-specific compiled shader representation).

### Internal write model

```text
command_headers[]: small fixed records
payload_arena:     variable-size command data

header[i].payload_ptr -> payload_arena offset
```

Memory is block-allocated (2 MB blocks in the original design intent) and recycled after backend completion.

### Map-create path

For upload-at-create, `map_create` returns writable memory so callers fill initial payload before submission.

### Why updates/resizes are not here

Update/resize operations must participate in scheduling relative to draw/compute. Therefore they belong in regular command buffers.

**LIMITATION**: split APIs improve clarity but require users to pick the correct command path.

---

## 3.3 `tm_renderer_command_buffer_i`

General execution stream for non-lifecycle work:

- Draw/dispatch commands.
- Resource updates/resizes.
- Render target/state transitions at command level.
- Higher-level render operations.

### Distinguishing feature: sortable commands

Each command has a 64-bit sort key. At submission:

1. Merge command arrays from many buffers.
2. Sort by key.
3. Emit backend commands in sorted order.

```text
CB A: [k=10 draw][k=90 dispatch]
CB B: [k=20 barrier][k=80 draw]

Merged+sorted:
[k=10 draw][k=20 barrier][k=80 draw][k=90 dispatch]
```

This enables broad parallel command authoring while preserving deterministic global order.

---

## 4) Concurrency and Ownership Model

## 4.1 Free-threaded by instance, not by shared mutation

- Many buffers can be authored in parallel.
- A single buffer is single-writer at a given time.

This gives scalable production without expensive lock-heavy internal structures.

## 4.2 Pooling and recycling

- Buffers allocated from pools.
- Submission marks buffers for backend consumption.
- Recycle only after backend confirms completion.

This avoids per-frame heap churn and keeps memory locality predictable.

---

## 5) Example Usage Flow

```c
// Pseudocode
uint32_t affinity = gpu0_mask | gpu1_mask;

tm_renderer_resource_cb *rcb = backend->create_resource_cb(affinity);
resource_api->create_buffer(rcb, &vb_desc);
void *init = resource_api->map_create_buffer(rcb, &staging_desc);
memcpy(init, vertices, size);
backend->submit_resource_cb(rcb);

tm_renderer_cb *cb = backend->create_command_cb(affinity);
renderer_api->set_render_target(cb, gbuffer_rt, sort_key(10));
renderer_api->draw(cb, mesh, material, sort_key(20));
backend->submit_command_cb(cb);
```

The API shape keeps lifecycle work explicit and execution work sortable.

---

## 6) Practical Sort-Key Design

A useful 64-bit layout often encodes coarse-to-fine ordering:

```text
[ queue : 4 ][ pass : 12 ][ material : 20 ][ depth-bin : 16 ][ local : 12 ]
```

- Coarse fields ensure major pipeline phases.
- Fine fields preserve local ordering and state coherence.

**LIMITATION**: this is workload-dependent; no single packing is universally optimal.

---

## 7) Why This Architecture Works

- Scales command generation with job systems.
- Preserves explicit API mapping for Vulkan/DX12.
- Supports multi-GPU through affinity without forcing one model.
- Keeps backend ownership of completion-sensitive lifecycle decisions.
- Adds low-level, deterministic scheduling primitive (sort keys) that decouples producers.

---

## 8) Known Boundaries and Future Pressure

- Resource model likely needs an additional resource-binding descriptor abstraction as pipeline complexity grows.
- Sort-key-driven ordering solves many scheduling problems, but not all dependency hazards; higher-level graph systems are still needed.
- Multi-GPU gains depend on copy/sync overhead; explicit control does not guarantee speedups.

These constraints motivate the next layer: render-graph-style dependency modeling over this low-level command substrate.

---

## Key Insights

- Thin abstraction + explicit lifecycle + sortable command streams is a strong base for modern rendering.
- Splitting resource creation from execution commands improves correctness and scheduling clarity.
- Backend-managed recycling is essential for safe pooling in asynchronous GPU pipelines.
- Global sort keys are a minimal but powerful primitive for cross-system ordering.
