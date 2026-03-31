# Multithreaded Gameplay — Island-Based Event Processing

Goal: exploit cores without forcing gameplay programmers into lock hell. The model below keeps mental overhead low and data races contained.

## Core Idea

- Gameplay runs as **event handlers**, not per-entity `update()` loops.
- Each event involves a **small set of entities** (e.g., `[player, ammo_pack]`). Handler may touch only those entities.
- Per frame, events induce an equivalence relation over entities; connected components become **islands** processed independently on separate cores.

ASCII island notion:

```
(player) --pickup--> (ammo)
(boss)   --hit-->    (rocket)
(minions mutually interacting) -> large island
```

## Frame Pipeline

1. Gather all events for the frame.
2. Build an entity graph; edges connect entities co-mentioned in any event.
3. Compute connected components → islands.
4. Assign islands to worker cores.
5. Process events within each island sequentially on its core.
6. Submit side effects to global systems via thread-safe queues (spawn FX, play sounds, etc.).

## Why This Works

- **Data isolation**: handlers can mutate only their island’s entities; no locks inside islands.
- **Parallelizable**: islands run concurrently; cost scales with number/size of islands.
- **Predictable**: ordering within an island is explicit; no cross-island order dependence.

## Concerns and Mitigations

### 1) Giant “player island”

Risk: everything connects to the player, collapsing parallelism.

Mitigations:
- Split events into phases (e.g., perception, combat, loot) and rebuild islands per phase.
- Design events to minimize unnecessary cross-entity references (e.g., broadcast area triggers instead of per-entity links).
- Allow manual partition hints for known hotspots (e.g., NPCs interacting only with local squad leader).

### 2) Programmer usability

Restrictions: handlers may only touch referenced entities.

Support tools:
- Static lint pass to flag out-of-island accesses.
- Clear API for queuing requests to global systems (fx/sound/telemetry) so programmers don’t cheat.
- Unit tests per handler to validate no hidden dependencies.

## Global Systems Access

Provide queued, thread-safe interfaces:

```cpp
fx_queue.enqueue({spawn_fx, pos});
sound_queue.enqueue({play, "footstep", chan});
```

Global consumers drain queues on main thread or designated systems thread; islands never share global mutable state directly.

## Limitations

- Highly interconnected gameplay scenarios may still bottleneck on one island; split gameplay into phases or rethink event granularity.
- Requires robust event generation; missing events mean missing logic.

## Key Insights

- Build parallelism from data dependency (connected components), not from guessing safe locks.
- Event-based islands keep gameplay logic simple and race-free while leveraging cores.
- Queue side effects to globals; don’t let islands poke shared state.
