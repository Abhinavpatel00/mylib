# Managing Coupling, Part 1 — Principles and Tactics for Decoupled Engines

Coupling is the hidden cost of change. Reducing it keeps systems understandable, movable, and optimizable in isolation. This part reframes the problem, explains why coupling creeps in, and offers concrete tactics.

## What Coupling Really Is

- **Definition**: the degree to which one module must change when another changes.
- **Axes**: compile-time (includes/templates), link-time (symbols), runtime (globals/singletons), data (shared schemas), behavioral (ordering assumptions).

Minimal coupling target:

```
[EffectSystem]
    own data + update()
    exposed API (IDs, commands)
(no other system needs its headers or memory layout)
```

## Why Coupling Hurts

- Iteration drag: small changes ripple into rebuilds and retests.
- Feature fear: simple requests become multi-day debugging marathons.
- Parallelism blocked: shared headers/data force serial work.
- Optimizations boxed in: memory layout and threading choices leak across systems.

If you prefer to prototype in a toy app instead of your engine, your coupling is too high.

## Four Practical Defenses

### 1) Treat “engine-wide frameworks” with suspicion

Root classes, mandatory RTTI/reflection, global serialization, or reference-counting layers enforce a single worldview on every subsystem.

**Why risky**
- One bad decision (e.g., reference counting that forbids move-only types) blocks threading or cache-friendly layouts across the codebase.
- Paradox of “maintainability”: once the framework infects everything, refactoring it is nearly impossible.

**Better**
- Let each subsystem own its persistence and lifetime rules; e.g., write `save()/load()` per system instead of one giant serializer.

### 2) Mediate low-level interactions through a high-level layer

Let gameplay (Lua/visual scripting) orchestrate cross-system effects instead of wiring systems together.

Example: footsteps

```
Gameplay
  - polls animation events
  - queries ground material
  - triggers sound system
```

Engine systems stay unaware of each other; messy coordination is quarantined in one layer (the language boundary acts as a firewall).

### 3) Allow selective code duplication

Overzealous reuse increases coupling. Sometimes 10 minutes of reimplementation beats a permanent dependency.

Cases where duplication is cheaper:
- Tools vs engine (different language, endianness needs).
- Simple utilities (parsing a small header) that would force shared headers, build flags, or template baggage.
- Narrow string usage where `const char*` + `strcmp` suffice; no need to drag `std::string` everywhere.

**Principle**: writing code is not the problem; entangling code is. Duplicate when reuse would introduce enduring coupling.

### 4) Refer to foreign objects by IDs, not pointers

Direct pointers or shared_ptrs couple lifetimes and layouts. IDs preserve ownership and allow internal rearrangement.

**Pattern**

```
// external view
using EffectId = uint32_t; // POD handle
EffectId spawn_effect(...);
void set_param(EffectId, ...);

// internal map
struct Slot { uint32_t gen; Effect* ptr; };
Slot lookup[MAX_EFFECTS];
```

Use bits for index + generation to detect stale handles. For many objects, widen to 64-bit IDs. Lookup table beats `std::map` for speed.

## Recognize Coupling Creep

Deadline hacks (“just reach into that system”) accumulate. Guardrails:
- Code reviews flag backdoors between systems.
- Keep “messy” glue confined to scripting layer.
- Periodically prune globals and include fan-out.

## Limits and Tradeoffs

- Some coupling is necessary (physics depends on collision data); fence it and document it.
- Duplication is good only when bounded and intentional; avoid copy-paste of complex algorithms.
- Over-isolation can obscure intent; expose minimal, purposeful APIs.

## Key Insights

- Coupling is multi-dimensional; tackle compile, runtime, data, and behavior separately.
- Frameworks can freeze an engine; prefer subsystem-local solutions.
- IDs + lookup tables let systems own lifetime and layout while exposing stable handles.
- Strategic duplication can lower coupling and accelerate change.
