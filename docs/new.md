Prepared expanded rewrites for all five requested articles. Copy each block into the corresponding `docs/` file (one file per article) and save with a descriptive name such as `docs/simple-perfect-murmur-hashing.md`, etc.

---

### docs/simple-perfect-murmur-hashing.md
# Simple, Perfect Murmur Hashing — When and How to Use Fixed Hashes

## Why Fix a Hash at Build Time
- Perfect hashing (no collisions) eliminates runtime lookups, chaining, and branches.
- Deterministic layout improves cache friendliness and simplifies serialization.
- Ideal for static asset tables, string IDs, network protocol enums.

## Core Idea
Precompute the hash seed that makes MurmurHash (or Murmur2/3 variants) collision-free for your key set. At runtime, hash with that seed and index a fixed array.

## How To Build It
1. Gather all keys (e.g., asset paths) at build time.
2. Choose table size N ≥ number_of_keys / load_factor (e.g., 1.3×).
3. Repeatedly:
   - Pick a random seed.
   - Hash all keys; if any collisions, retry.
4. Emit:
   - The chosen seed.
   - The table size and the value array indexed by `hash(key, seed) % N`.

Pseudocode (builder):
```python
def make_perfect_table(keys, values, load=1.3):
    n = int(len(keys) * load)
    while True:
        seed = rand32()
        slots = [None] * n
        for k,v in zip(keys, values):
            i = murmur32(k, seed) % n
            if slots[i] is not None:
                break  # collision, try new seed
            slots[i] = v
        else:
            return seed, slots  # collision-free
```

Runtime lookup:
```cpp
Value lookup(const char* k) {
    uint32_t h = murmur32(k, SEED);
    return table[h % TABLE_SIZE];
}
```

## Where It Shines
- Compile-time string-to-ID maps (animation event names, console commands).
- Network protocol dispatch tables.
- Asset GUID → metadata.

## Limitations
- Key set must be closed at build time; adding keys requires rebuilding.
- Space rises with low load factors; choose N to fit caches.
- Murmur is not cryptographically secure; unsuitable for adversarial inputs.

## Key Insights
- “Perfect” is cheap when the key set is static and small enough to brute-force seeds.
- Murmur’s speed and diffusion make it a practical backbone; the seed search makes it collision-free for your set.

---

### docs/murmur-hash-inverse.md
# Inverting MurmurHash for Small Domains — Feasibility and Methods

## Why You Might Care
- Reverse-engineering a hashed identifier when the original key space is tiny (e.g., a few thousand asset names).
- Validating that a “secret” hash does not actually hide data.

## How Murmur Spreads Bits
- Murmur32 is a sequence of multiplies, rotates, xors. It is not one-way crypto; it is a fast mixer.
- Over 32 bits, the mapping is many-to-one for large domains but often injective over small enumerated sets.

## Practical Inversion Strategy
- Enumerate all candidate strings from your constrained domain, hash them, and compare.
- For very small domains, precompute a reverse map: `hash -> candidate`.
- If seeds/lengths are unknown, brute-force them jointly; seeds are 32-bit but you can bound them if build scripts constrain choices.

Pseudocode:
```python
def invert_murmur(target, candidates, seeds):
    for s in seeds:
        for c in candidates:
            if murmur32(c, s) == target:
                yield (c, s)
```

## When This Works
- Domain ≪ 2^32 (e.g., ~10^5 names) → enumeration is trivial.
- Known or small seed set.
- Short ASCII identifiers (tool-generated names, enums).

## When It Fails
- Large/unbounded domains.
- Unknown seed across full 32-bit space (cost ~2^63 ops to search string+seed jointly).
- If length is hidden and strings can contain nulls or wide chars, search space explodes.

## Mitigation (if you need opacity)
- Use a keyed HMAC or SipHash instead of plain Murmur.
- Keep seeds secret and use salts per table.
- Avoid assuming “hashing” is anonymization.

## Key Insight
Murmur is not encryption. Over small domains with known seeds, inversion is just search. Treat it as an identifier mixer, not as a secrecy mechanism.

---

### docs/a-star-is-overrated.md
# A* Is Overrated — Choose the Right Pathfinding Tool for the Job

## The Thesis
A* is a great general-purpose shortest-path algorithm on graphs with admissible heuristics, but many game problems benefit from simpler, cheaper, or more specialized approaches.

## When A* Excels
- Sparse graphs with moderate branching factor.
- Good, cheap heuristics (grid Manhattan/Euclidean).
- Need optimality or bounded suboptimality.

## Where A* Underperforms
- Dense graphs → node expansions explode.
- Poor heuristics → effectively Dijkstra’s.
- Dynamic obstacles → repeated replans are costly.
- Very large open spaces → frontier balloons.

## Alternatives and When to Use Them
- **Dijkstra with buckets**: when all edges are small integers; fast and simple.
- **Jump Point Search**: grids with uniform costs; prunes symmetric expansions.
- **Fringe Search / IDA***: memory-limited contexts; iterative deepening.
- **Hierarchical Pathfinding (HPA*, HH)*: large maps; preprocess clusters/portals to reduce search.
- **Flow fields (integration fields)**: many agents to the same goal; single solve feeds all.
- **Navigation meshes + string-pulling**: reduce graph size and post-process for smoothness.
- **Cooperative/Windowed planning**: multi-agent collisions handled during or after search.
- **Goal Bounding / Landmarks (ALT)**: precompute bounds to tighten heuristics on static maps.
- **Any-angle (Theta*, Lazy Theta*, ANYA)**: when grid-induced zig-zag is unacceptable.

## Practical Guidelines
- Start by shrinking the graph: navmeshes, waypoints, portals.
- Exploit common goals with flow fields.
- Cache and reuse paths; partial repairs beat full replans.
- Tune heuristics to your cost structure; admissibility may be optional if you only need “good enough.”
- Profile open/closed-list costs; priority queues can dominate runtime.

## Limitations of “Just Use A*”
- Memory: open/closed sets can balloon on big maps.
- Replanning frequency: dynamic worlds need incremental or real-time variants (D*, LPA*, RTAA*).
- Edge costs: non-uniform, dynamic costs reduce heuristic effectiveness.

## Key Insights
- Optimality is often negotiable; performance and predictability are not.
- Domain structure (shared goals, static topology, uniform costs) should dictate the algorithm.
- A* is a tool, not a default. Pick the simplest algorithm that meets your gameplay and performance needs.

---

### docs/a-new-data-storage-model.md
# A New Data Storage Model — Separable, Streamable, and Versioned Assets

## Problem Statement
Traditional “kitchen sink” binary blobs entangle structure, payload, and patching logic. This makes streaming, versioning, and platform variation painful.

## Design Goals
- **Separation of static vs. instance data**: keep immutable bulk separate from per-instance overrides.
- **Pointer-free blobs**: offsets enable relocation, streaming, and mmap without fixups.
- **Versioned schemas**: allow evolution without breaking loaders.
- **Chunkable/streamable**: load what’s needed, when it’s needed.

## Proposed Layout
```
ResourceBlob
  header { version, chunk_count, string_table_offset, ... }
  chunk_table[chunk_count] { type, offset, size, flags }
  chunks...
    chunk(type=mesh)    { offsets into vertex/index buffers }
    chunk(type=material){ params, texture refs (by id/path) }
    chunk(type=anim)    { track streams, compression params }
  string_table: null-terminated UTF-8
```
- All references are offsets or stable IDs; no raw pointers.
- Chunks are independently compressed; stream friendly.
- Instance data lives elsewhere; references static chunk IDs.

## Why This Works
- **Streaming**: chunk table drives on-demand loads; no monolithic seeks.
- **Versioning**: new chunk types/fields can be added; old loaders skip unknown types via offsets.
- **Cross-platform**: endian-aware fields; no pointer patching.
- **Tool chain**: easy to generate/edit chunks; can gzip/pack independently.

## Instance/Override Model
```
Instance
  refs: resource_id
  overrides: { component_id -> data }
```
- Prefab-like layering: base resource + deltas.
- Keeps runtime memory low; only touched fields are overridden.

## Where It Applies
- Game asset bundles (meshes, mats, anims, levels).
- Networked or hot-reloaded assets.
- Any system needing relocatable, incremental loading.

## Limitations
- Requires disciplined schema evolution and tooling.
- Chunk granularity must balance I/O overhead vs. locality.
- String table indirection can complicate debugging; provide dump tools.

## Key Insights
- Offsets + chunk tables beat pointer-rich blobs for streaming and compatibility.
- Treat resources as composable chunks with explicit schemas; let instances carry overrides.
- Versioning is a first-class concern; design for skipping/adding chunks safely.

---

### docs/four-meditations-on-bad-design.md
# Four Meditations on Bad Design — Patterns That Decay Systems

## 1) Cleverness Over Clarity
- **Symptom**: inscrutable metaprogramming, dense one-liners.
- **Cost**: harder onboarding, fragile optimization assumptions.
- **Antidote**: write for the maintainer; measure before “clever”; prefer boring, explicit code.

## 2) Global, Implicit Coupling
- **Symptom**: singletons, hidden globals, “manager” classes everyone touches.
- **Cost**: invisible dependencies, order bugs, no test seams.
- **Antidote**: explicit dependencies, constructor injection, small interfaces; push cross-system glue to a quarantined layer (scripting/flow).

## 3) Leaky Abstractions and Forced Frameworks
- **Symptom**: one-size-fits-all serialization/RTTI/base classes.
- **Cost**: freezes engine evolution, blocks threading/cache-friendly layouts.
- **Antidote**: subsystem-owned persistence; pointer-free blobs; avoid mandatory inheritance.

## 4) Over-Optimizing the Wrong Thing
- **Symptom**: premature micro-opts that complicate code while big-ticket wins (better data layout, fewer allocations, streaming) are ignored.
- **Cost**: complexity without measurable gain.
- **Antidote**: profile-guided changes; start with data layout and algorithmic wins; measure cache/TLB/alloc churn.

## Cross-Cutting Themes
- Design for change, not heroics: simple data, explicit dependencies, and localized complexity age better.
- Keep “messy” coordination at the boundaries (gameplay scripting) to protect engine cores.
- Clarity is a performance feature; debuggable code ships faster.

## Key Insights
- Most “bad design” is debt from hidden coupling and unjustified cleverness.
- Prefer boring, explicit, data-oriented solutions over magical frameworks.
- Optimize where it matters: data layout, allocations, streaming—not gratuitous micro-tricks.

---

These rewrites deepen explanations, add rationale/limitations, and supply code/diagrams where helpful. Let me know if you want any section adjusted or split into separate files automatically.
