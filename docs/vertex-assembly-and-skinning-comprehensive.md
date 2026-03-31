# Vertex Assembly and Skinning — Comprehensive Rewrite

## Scope

This document deepens the original article’s architecture around:

- Replacing fixed-function style vertex input assumptions with explicit shader-side loading.
- Using shader systems as abstract data-access interfaces.
- Implementing flexible skinning with variable influence counts per vertex.
- Decoupling ECS rendering components through `tm_component_shader_system_i`.

The guiding principle is to keep shader authoring simple while preserving backend freedom.

---

## 1) Architectural Motivation

The article starts from a practical observation: the shader-system abstraction solved variation and binding management, but also unintentionally became a strong interface boundary for **data access**.

That boundary is valuable for vertex assembly and skinning because it removes hard dependencies on a specific mesh storage format or fixed input-assembler behavior.

### WHY

- Shader code should describe surface/lighting intent, not storage quirks.
- Engine backend should be free to reorganize/compress/procedurally derive vertex data.
- A single material/shader should work across heterogeneous mesh assets.

### HOW

- Provide loader functions like `load_position()`, `load_texcoord0()`, etc.
- Back those functions with a system implementation chosen by active shader systems.
- Keep shader-side call sites stable while backend data representation evolves.

### WHERE

- Any renderer that must support multiple mesh encodings.
- Data-oriented engines where storage format changes are common.

### LIMITATIONS

- Explicit loads shift responsibility to shader codegen/runtime conventions.
- Poorly designed loader interfaces can hide expensive access patterns.

---

## 2) Vertex Assembly Without Traditional Vertex Buffers

Instead of relying on IA-bound vertex attribute streams, vertex data is loaded explicitly from `ByteAddressBuffer` resources.

From the shader author’s perspective:

```hlsl
tm_vertex_loader_context ctx;
init_vertex_loader_context(ctx, input.instance_id);

float4 local_pos = load_position(ctx, input.vertex_id, 0);
float2 uv       = load_texcoord0(ctx, input.vertex_id, 0);
```

The call sites stay stable; implementations can vary from direct buffer fetch to procedural generation.

### WHY

- Decouples shader logic from fixed vertex layout assumptions.
- Enables richer access patterns (neighbor vertices, alternate sets, index reads).
- Makes fallback behavior explicit when channels are missing.

### HOW

A loader context typically carries:

- Active channel mask.
- Vertex count per set.
- Channel byte offsets.
- Channel strides.

Loaders compute byte address from `(set, vertex_id, semantic)` and fetch from the corresponding buffer.

### WHERE

- Pipelines with optional channels (multiple UV sets, optional tangents, etc.).
- Content where compression/quantization strategies differ across assets.

### LIMITATIONS

- Random access patterns can hurt cache/coalescing if unmanaged.
- Requires disciplined conventions for semantic IDs and format decoding.

### ASCII Diagram

```text
Vertex Shader Input:
  instance_id, vertex_id
      |
      v
init_vertex_loader_context()
      |
      v
load_*() interface
  ├─ direct ByteAddressBuffer fetch
  ├─ decompression/quantization decode
  └─ procedural generation / fallback
      |
      v
canonical vertex attributes for shading
```

---

## 3) What the Abstraction Enables

The article highlights capabilities that are difficult or awkward in strict IA-centric flows.

### Major Idea A — Arbitrary Vertex Access

You can fetch any vertex of interest, not only the currently processed one.

- **WHY**: enables adjacency-aware effects, reconstruction, and custom interpolation workflows.
- **HOW**: loader APIs take explicit `vertex_id`.
- **WHERE**: mesh processing in compute/mesh amplification stages.
- **LIMITATIONS**: extra loads can become bandwidth-heavy.

### Major Idea B — Optional Index Data Exposure

Expose index-buffer reads to shader code.

- **WHY**: unlocks full mesh topology access.
- **HOW**: add `load_index()` family in same abstraction.
- **WHERE**: topology-aware effects and custom primitive expansion.
- **LIMITATIONS**: can increase complexity of stage portability.

### Major Idea C — Encoding Flexibility

Any quantization/compression can be hidden behind loaders.

- **WHY**: storage efficiency without leaking format details into material shaders.
- **HOW**: decode in loader functions.
- **WHERE**: large asset sets, streaming-heavy projects.
- **LIMITATIONS**: decode ALU cost must be profiled.

### Major Idea D — Graceful Missing-Channel Handling

Absent channels can return defaults.

- **WHY**: one shader can run across assets with different channel completeness.
- **HOW**: `has_channel()` checks + default values.
- **WHERE**: tools/editor workflows and mixed-quality content.
- **LIMITATIONS**: defaults must be semantically correct (e.g., normal/tangent conventions).

### Major Idea E — Multiple Vertex Data Sets

Support morph targets or alternate streams naturally.

- **WHY**: richer deformation pipelines.
- **HOW**: include `set` argument in loader APIs.
- **WHERE**: blend shapes, LOD blending, variant geometry streams.
- **LIMITATIONS**: memory pressure and synchronization complexity.

---

## 4) Loader Interface Design (Reference Pattern)

A representative minimal pattern:

```hlsl
#define VERTEX_SEMANTIC_POSITION 0
#define VERTEX_SEMANTIC_NORMAL   1
#define VERTEX_SEMANTIC_MAX_CHANNELS 16

struct tm_vertex_loader_context {
    uint active_channels;
    uint num_vertices;
    uint offsets[VERTEX_SEMANTIC_MAX_CHANNELS];
    uint strides[VERTEX_SEMANTIC_MAX_CHANNELS];
};

bool has_channel(tm_vertex_loader_context ctx, uint semantic) {
    return (ctx.active_channels & (1 << semantic)) != 0;
}
```

### WHY

- Small, predictable context object.
- Decoding logic localized.

### HOW

- Semantic constants define API contract.
- Offsets/strides support SoA/AoS-like layouts and per-channel packing.

### WHERE

- General-purpose vertex input abstraction in HLSL-like shading pipelines.

### LIMITATIONS

- Semantic-ID management becomes long-term compatibility surface.

---

## 5) Skinning Model with Variable Influence Count

Instead of fixed `N` bone indices/weights in each vertex, each vertex stores one `uint skin_data`:

- Upper 8 bits: number of influences (`0..255`).
- Lower 24 bits: byte address to an influence array.

Influence entries are stored as:

```hlsl
struct tm_bone_influence_t {
    uint index;
    float weight;
};
```

### WHY

- Eliminates wasted storage for vertices needing fewer influences.
- Supports vertices requiring more than common fixed limits.

### HOW

1. Read `skin_data` from vertex channel.
2. Decode count/address.
3. Loop over influence list.
4. Fetch bone matrix by influence index.
5. Accumulate weighted transformed position.

### WHERE

- Characters with non-uniform influence distributions.
- Toolchains where authoring may exceed fixed 4/8 influence assumptions.

### LIMITATIONS

- Extra memory indirection per vertex.
- Needs careful alignment and bounds validation in content pipeline.

### ASCII Diagram

```text
Per-vertex channel: skin_data (32-bit)
  [ count:8 | address:24 ]
                |
                v
      influence array in buffer
      [ (bone_idx, weight), ... ]
                |
                v
      weighted matrix accumulation
```

---

## 6) Current/Previous Frame Bone Data for Velocity

To support temporal effects (e.g., TAA requiring motion vectors), skinning exposes both current and previous frame bone transforms.

The article describes a ping-pong update scheme to avoid unnecessary copying.

### WHY

- Motion vector correctness depends on consistent previous-frame transforms.
- Ping-pong avoids full-array shuffle each frame.

### HOW

- Maintain two matrix regions/buffers.
- Write current frame into one, keep previous in the other.
- Provide shader constants with byte offsets to both.

### WHERE

- Temporal pipelines with per-pixel velocity.

### LIMITATIONS

- Requires strict frame boundary discipline.
- History invalidation needed for teleports/reset events.

### ASCII Timeline

```text
Frame 0: write A (f0), read B (unused)
Frame 1: write B (f1), read A (f0)
Frame 2: write A (f2), read B (f1)
... alternating each frame
```

---

## 7) Skinning Shader Path (Decomposed)

### Step 1: Initialize Skin Header

- Check if skin channel exists.
- Load packed `skin_data`.
- Decode influence count/address.

### Step 2: Accumulate Weighted Transform

- Iterate influence range.
- Fetch `(bone_idx, weight)`.
- Load matrix from active bone-buffer region.
- Accumulate `weight * (M * p)`.

### Step 3: Use both Current and Previous

- Evaluate position with current matrices and previous matrices to derive motion where needed.

### WHY

- Keeps runtime code straightforward despite flexible encoding.

### LIMITATIONS

- Influence loops are variable-length; worst-case vertices cost more.

---

## 8) ECS Decoupling via `tm_component_shader_system_i`

A major extension in the article is not only rendering technique, but plugin architecture:

- One component issues draws/dispatches.
- Another component wants to expose shader data.
- They may live in separate plugins and must remain uncoupled.

`tm_component_shader_system_i` solves this by registering providers in an API registry and letting rendering systems enumerate/update them.

### WHY

- Preserves plugin independence.
- Avoids hard component-to-component references.
- Enables conditional work: update expensive systems only when visible.

### HOW

1. Component plugin implements `tm_component_shader_system_i`.
2. Registers under a known name.
3. Render component enumerates all providers.
4. Calls provider update only when component passes visibility criteria.
5. Receives `tm_component_shader_system_t` with system + optional constants/resources.

### WHERE

- Skinning, cloth, procedural deformation, per-object lighting caches.

### LIMITATIONS

- Requires robust ownership/lifetime rules for returned instances.
- Enumeration order and conflict policy should be deterministic.

---

## 9) Viewer-Scoped Activation (`active_viewer_mask`)

`active_viewer_mask` controls for which viewer types a component system is enabled (viewport camera, shadow camera, etc.).

### WHY

- Avoids running work where it has no downstream consumer.
- Supports pass/view-specific behavior without hardcoding in renderer.

### HOW

- Provider computes a bitmask per update call.
- Renderer activates system only for matching viewers.

### WHERE

- Shadow-only deformations, reflection-only data, editor-only overlays.

### LIMITATIONS

- Mask semantics must remain stable across engine modules.

---

## 10) End-to-End Flow

```text
[ECS Entity]
  Components from independent plugins
      |
      v
[Render Component]
  Enumerate tm_component_shader_system_i providers
  Frustum/visibility gate updates
      |
      v
Collect component shader systems
  (system + constants + resources + viewer mask)
      |
      v
Shader variation selection + binding assembly
      |
      v
Draw/Dispatch with consistent loader/skinning behavior
```

---

## 11) Practical Trade-Off Summary

1. **Flexibility vs Raw Simplicity**
   - Explicit loading is flexible, but more moving parts than fixed IA setup.
2. **Space Efficiency vs Indirection Cost**
   - Packed skin header saves memory, introduces pointer-like lookup.
3. **Decoupling vs Discoverability**
   - Plugin independence is strong, but requires tooling to inspect active providers.

---

## 12) Key Insights (Condensed)

- Vertex assembly as a shader-system interface decouples authoring from storage.
- Explicit loaders make optional channels and multiple data sets first-class.
- Variable-length skinning metadata (`count + address`) supports broad influence ranges efficiently.
- Ping-pong frame data cleanly supports temporal velocity requirements.
- `tm_component_shader_system_i` enables cross-plugin rendering data sharing without coupling.

---

## 13) Minimal Pseudocode Sketch

```cpp
VertexLoaderContext ctx = init_vertex_loader_context(instance_id);
float4 p_local = load_position(ctx, vertex_id, 0);

SkinHeader sh = init_skin_header(ctx, vertex_id);
float4 p_curr = skin_point(sh, p_local, skin.current_offset, skin.bone_buffer);
float4 p_prev = skin_point(sh, p_local, skin.prev_offset,    skin.bone_buffer);

MotionVector mv = project(p_curr) - project(p_prev);
```

This preserves the article’s central idea: keep shader call sites stable and expressive while backend systems provide the concrete data path.
