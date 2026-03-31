# Allocation Adventures 1: Rebuilding the `DataComponent` for Cache Coherency and Allocation Control

This article revisits the original `DataComponent` design and pushes it further into a rigorous, implementation-ready form.

Core objective: represent small per-entity dynamic data with:

1. Linear memory access patterns.
2. Very few allocations.
3. Predictable merge behavior for collaborative editing.
4. Straightforward serialization and relocation.

The original post gives a great intuition-driven journey. This rewrite keeps the same sequence, but adds explicit invariants, tradeoffs, and failure modes.

## Problem Framing

We want to store data like:

```text
name = "The One"
stats = {
  health = 100
  mana = 200
}
status_effects = {
  drunk = true
  delirious = true
}
```

Supported value types:

- `bool`
- `float`
- `string`
- `object`
- `array<float>`

Restriction: arrays are numeric only. General arrays (`array<any>`) are intentionally excluded.

### Why this restricted schema matters

The restriction is not arbitrary. It aligns with mergeability:

- Object edits become key-level set/replace operations.
- Strings and numeric arrays can be treated as monolithic values.
- Merge semantics stay local and deterministic.

If arbitrary nested arrays were allowed, merges need positional diffing, move detection, conflict policies, and often user-defined resolution logic.

## Baseline: Naive Tree of Heap Objects

A direct representation is pointer-heavy:

```cpp
enum class DataType { Bool, Float, String, Object, Array };

struct DataValue {
    DataType type;
    union {
        bool b;
        float f;
        std::string* s;
        std::vector<float>* array;
        std::map<std::string, DataValue>* object;
    };
};
```

### Why this performs poorly

- Pointer chasing hurts cache locality.
- Many tiny allocations increase allocator overhead.
- Fragmentation grows over time.
- Serialization requires graph walking and pointer patching.

ASCII view of a typical object graph:

```text
Entity
  |
  +--> map node ("stats") ----> map node ("health") ---> float
  |                           \
  |                            +-> map node ("mana") ---> float
  |
  +--> map node ("name") -----> std::string header -----> char buffer
```

## Transformation Pipeline

The original article optimizes in steps. We keep that sequence and make each step explicit.

---

## Step 1: Hash Keys

Replace string keys in lookup structures with fixed-size hashes.

```cpp
using KeyHash = uint32_t; // can be uint64_t in stricter builds
```

### Why it works

- Key comparison becomes integer comparison.
- Key storage is fixed width.
- Fewer allocations from key ownership.

### How it works

- Convert each key path segment (e.g. `stats`, `health`) into hash.
- Combine segment hashes into a path hash.

### Where it applies

- Systems where key enumeration is rare or optional.
- Data authored from known schemas.

### Limitations

- Hash collisions are real.
- Losing literal keys hurts diagnostics unless separately stored.

Practical mitigation:

- Prefer `uint64_t` in shipping tools when memory budget allows.
- Keep optional debug dictionary `hash -> original string` in editor builds.

---

## Step 2: Flatten Hierarchy into Path Keys

Instead of tree nodes, store leaf values keyed by full path hash:

```text
name
stats.health
stats.mana
status_effects.drunk
status_effects.delirious
```

### Why it works

Most queries are point lookups (`get(stats.health)`) rather than subtree iteration.

### How it works

- Canonicalize paths.
- Hash canonicalized path.
- Store entries in one flat collection.

### Where it applies

- Gameplay state where data is sparse and shallow.
- Override systems (defaults + per-instance patches).

### Limitations

- You cannot reconstruct subtree membership from hashes alone.
- Prefix queries require extra index or key material.

---

## Step 3: Switch AoS to SoA for Search Hot Path

### AoS layout

```cpp
struct Entry {
    KeyHash key;
    DataType type;
    Value value;
};
std::vector<Entry> entries;
```

### SoA layout

```cpp
std::vector<KeyHash> keys;
std::vector<DataType> types;
std::vector<Value> values;
```

### Why it works

Linear scans for key lookup touch only `keys[]`, increasing useful bytes per cache line.

### How it works

- Keep index `i` aligned across arrays: `keys[i]`, `types[i]`, `values[i]`.
- Search `keys[]` first; then access corresponding type/value.

### Where it applies

- Read-mostly lookup tables.
- Small/medium entry counts where hash table overhead is not worth it.

### Limitations

- Mutating (insert/remove) requires synchronized moves across arrays.
- Binary search needs sorted `keys[]`; insertion cost becomes O(n).

---

## Step 4: Co-allocate Header Arrays

Three vectors imply at least three allocations. Co-allocate as one buffer:

```text
+-----------------------------------------------------+
| keys[capacity] | types[capacity] | values[capacity] |
+-----------------------------------------------------+
```

```cpp
struct HeaderStorage {
    uint32_t size;
    uint32_t capacity;
    uint8_t* base;
    KeyHash* keys;
    DataType* types;
    Value* values;
};
```

### Why it works

- Allocation count drops.
- Reallocation/copy logic becomes centralized.
- Better memory accounting and profiling.

### Alignment caution

The sub-array offsets must be aligned for their element type. Always round each segment start to `alignof(T)`.

---

## Step 5: Remove STL from Payload Values

Original payload pointers:

```cpp
std::string* s;
std::vector<float>* array;
```

Replace with POD descriptors:

```cpp
struct Slice {
    uint32_t offset;
    uint32_t size_bytes;
};

struct Value {
    union {
        bool b;
        float f;
        Slice payload; // used by string and float-array
    };
};
```

### Why it works

- Eliminates second-level allocators (`string`/`vector` internals).
- Enables relocation via offsets.
- Simplifies save/load (`memcpy`-friendly metadata).

### Limitation

Need explicit type-aware interpretation:

- `type == String`: payload bytes are UTF-8 chars.
- `type == Array`: payload bytes are `float[N]`.

---

## Step 6: Put All Variable-Size Payloads in One Buffer

Separate payload arena:

```text
payload arena
+--------------------------------------------------------------+
| "hello\0" | [1.0,2.0] | "mana\0" | [0.0,0.2,0.4] | free... |
+--------------------------------------------------------------+
```

Simple bump allocator:

```cpp
struct PayloadArena {
    uint8_t* data;
    uint32_t capacity;
    uint32_t used;
};

uint32_t arena_alloc(PayloadArena& a, uint32_t n, uint32_t align) {
    uint32_t p = (a.used + (align - 1)) & ~(align - 1);
    if (p + n > a.capacity) return UINT32_MAX;
    a.used = p + n;
    return p;
}
```

### Handling growth and holes

If a value grows, append new payload and update its offset. Old space becomes garbage (hole).

```text
before grow: [strA][arrB][strC][free...]
after  grow: [hole][arrB][strC][strA_big][free...]
```

Defragmentation strategies:

- Lazy: wait until arena resize, repack then.
- Active: compact when fragmentation ratio crosses threshold.

---

## Step 7: Compress Value References

The original post shows `uint16_t offset + uint16_t size` for 64 KiB arenas.

```cpp
struct TinySlice {
    uint16_t offset;
    uint16_t size;
};
```

### Why this is attractive

- `Value` stays 4 bytes.
- Lower header footprint improves scan and copy costs.

### Where it applies

- Small per-component payload budgets.
- Systems with strict size caps.

### Limitation

- Hard ceiling at 65535 bytes.
- Requires graceful overflow policy.

Robust approach:

- Start with tiny representation.
- Promote component to a large-format variant when exceeded.

---

## Step 8: Merge Header and Payload into One Bidirectional Buffer

Final layout from the original idea:

```text
single buffer
+------------------------------------------------------------------+
| Header grows -> |............. shared free .............| <- Data |
+------------------------------------------------------------------+
```

- Header arrays grow upward from low addresses.
- Payload arena grows downward from high addresses.
- Reallocate only when fronts collide.

### Why it works

Both sides share one free region; no stranded slack between independent buffers.

### How it works

```cpp
struct DataComponent {
    uint8_t* buffer;
    uint32_t capacity;

    uint32_t count;
    uint32_t header_end;   // grows up
    uint32_t payload_begin; // grows down

    KeyHash* keys;
    DataType* types;
    Value* values;
};
```

Allocation checks:

- Header insertion needs `new_header_end <= payload_begin`.
- Payload insertion needs `payload_begin - payload_size >= header_end`.

---

## End-to-End Operation Sketch

### Lookup

1. Hash canonical path.
2. Linear scan `keys[]` (or binary search if sorted).
3. Read `types[i]`, decode `values[i]`.

### Insert/Update scalar (`bool`/`float`)

- Upsert entry.
- Store inline value in union.

### Insert/Update variable-size (`string`/`float[]`)

- Allocate payload slice.
- Copy bytes.
- Store offset/size descriptor.

### Delete

- Remove header slot (swap-remove or stable-shift).
- Optionally leave payload hole; reclaim at compaction.

---

## Complexity and Footprint

Assume `N` entries and total payload `P` bytes.

- Lookup (linear): O(N)
- Insert/Delete (unsorted + swap-remove): O(1) metadata, O(payload copy)
- Insert/Delete (sorted): O(N)
- Reallocation: O(N + P)

Per-entry metadata (typical compact form):

- key: 4 bytes
- type: 1 byte (+padding)
- value: 4 bytes

Rough order: 12 bytes/entry before alignment.

---

## Design Limits and Failure Modes

1. Hash collision corruption risk.
- Mitigation: 64-bit hashes + debug collision checks.

2. Loss of subtree enumeration.
- Mitigation: optional prefix index or side table of decoded paths.

3. Payload fragmentation under churn.
- Mitigation: periodic compaction, generations, or copy-on-write snapshots.

4. Reallocation stalls.
- Mitigation: reserve aggressively and grow geometrically.

5. Alignment bugs in manual packing.
- Mitigation: centralize offset math and assert alignment.

---

## Practical Example: Character Sheet Mutation

Operation sequence:

1. `set(stats.health, 100.0f)`
2. `set(name, "The One")`
3. `set(status_effects.drunk, true)`
4. `set(name, "The One Reborn")` (grows payload)

Memory evolution:

```text
t0: [header small][.........................free.........................]
t1: [header bigger][.........................free..............]["The One"]
t2: [header bigger][.........................free..............]["The One"]
t3: [header bigger][........free....]["The One"]["The One Reborn"]
                        ^ old name becomes hole
```

Compaction later rewrites live payloads densely and patches offsets.

---

## Key Insights

1. Most wins come from representation, not micro-optimizing code paths.
2. Flattening and hashing trade introspection for performance; do it intentionally.
3. A single relocatable buffer unlocks cheap copy, snapshot, and serialization.
4. Internal fragmentation is acceptable if lifecycle makes compaction predictable.
5. Data-oriented design is not anti-abstraction; it is explicit about costs.

## Minimal Reference Implementation Skeleton

```cpp
struct DataComponent {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t count;
    uint32_t header_end;
    uint32_t payload_begin;

    KeyHash* keys;
    DataType* types;
    Value* values;
};

bool set_float(DataComponent& dc, KeyHash key, float v);
bool set_bool(DataComponent& dc, KeyHash key, bool v);
bool set_string(DataComponent& dc, KeyHash key, const char* s, uint32_t len);
bool set_float_array(DataComponent& dc, KeyHash key, const float* a, uint32_t n);
bool erase(DataComponent& dc, KeyHash key);
bool get(const DataComponent& dc, KeyHash key, /*out*/ Value& v, /*out*/ DataType& t);
```

This preserves the original article's intent while making the design concrete enough to implement directly.
