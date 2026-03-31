# Managing Coupling, Part 2 — Polling, Callbacks, and Events Without Tangling Systems

Low-level systems often need to notify higher-level code (e.g., animation wants gameplay to know a foot hit the ground). The lower layer should not know the higher layer exists. This part compares three patterns—polling, callbacks, events—on when, why, and how to use each while preserving decoupling and performance.

## Pattern 1: Polling

**How**: Higher-level code asks every frame, “did X happen?”

```cpp
if (download.complete()) handle_file(download.result());
if (input.is_pressed(A)) controller.jump();
```

**Why it works in games**
- We already tick every frame; checking a handful of flags is cheap.
- Keeps control flow local; easier to read and debug than asynchronous callbacks.

**Where it applies**
- Small sets of items: file downloads, server browsing, save/load status, controller input, animation event flags.

**Limitations**
- Explodes for O(N²) queries (all physics collisions). At that point you’re effectively building an event system.

## Pattern 2: Callbacks

**How**: Low-level code records callback records and high-level code executes them at a controlled time.

### Immediate vs deferred
- **Immediate**: execute at the point of generation. Bad for caches, threading, and can invalidate iterators mid-loop.
- **Deferred (preferred)**: push to a queue, then let high-level code run `execute_callbacks()` once per frame.

ASCII timeline (deferred):

```
[Frame]: physics step -> queue contact callbacks -> ... -> gameplay executes callbacks -> done
```

**Data structure**

```cpp
struct Callback16 {
    void (*f)(void);   // cast by caller
    char data[12];     // payload
};
Vector<Callback16> queue;
```

Dispatch:

```cpp
typedef void (*AnimEventCb)(void*, uint32_t);
for (auto& cb : queue) {
    auto f = reinterpret_cast<AnimEventCb>(cb.f);
    f(cb.data, event_id);
}
```

**Why this keeps coupling low**
- Low-level system only knows “I emit a record”; high-level owns execution order and lifetime.
- Thread-friendly: SPUs/worker threads can fill queues; main thread merges and executes.

**Limitations**
- Payload typing is manual; misuse crashes loudly (acceptable for engine code).
- Must guard against destroyed targets; pair with ID-based handles from Part 1.

## Pattern 3: Events

**How**: Low-level writes event enums + payloads into a buffer; high-level consumes in bulk.

Buffer layout:

```
[event_enum][payload][event_enum][payload]...
```

**When to prefer events over callbacks**
- You want batch processing: e.g., scan all collision events to find ones over a force threshold.
- Multiple actions may be taken per event type.

**Why it stays decoupled**
- Low-level only publishes data; high-level interprets meaning.
- Buffers are POD; easy to move, copy, merge, DMA.

## Choosing the Right Tool

- **Default to polling** when the set is small and locality matters.
- **Deferred callbacks** when you need specific notifications tied to specific objects.
- **Events** when consuming in bulk or when many producers exist.

## Implementation Checklist (Decoupled and Fast)

- Queue callbacks/events; execute at a known point.
- Use POD payloads; avoid allocating or touching high-level objects in low-level code.
- Reference external objects via IDs, not pointers.
- Keep event/ callback APIs per-subsystem; avoid one global “god bus.”

## Anti-Pattern: Global Switchboard

A magic event bus that routes everything to anyone reintroduces tight coupling, hidden dependencies, and unpredictable order. Keep publish/consume relationships explicit per subsystem.

## Key Insights

- In games, polling is often simplest and fast enough; don’t fear it.
- Deferred execution keeps caches warm, avoids iterator invalidation, and enables threading.
- Events and callbacks are just data streams; treat them as such to move them across cores and queues.
- Explicit relationships beat global buses; decoupling is about controlling *who* talks to *whom* and *when*.
