# The (Machinery) Network Frontier — Part 2 (Comprehensive Rewrite)

## Abstract

Part 1 introduced the foundational networking primitives (`Simulation Instance`, `Network Node`, `Pipe`, `Packet Type`, and delivery guarantees). Part 2 explains how those primitives are composed into practical, editor-facing multiplayer workflows.

This rewrite focuses on:

- conceptual precision,
- protocol implications of each feature,
- realistic limitations,
- and concrete patterns for transitioning prototype networking into production-quality game networking.

---

## 1) From primitives to workflows

Part 1’s low-level model is intentionally minimal. Part 2’s value is in how The Machinery exposes that model to users as configurable assets and graph/system hooks.

The key architectural idea:

```text
Low-level transport primitives
        +
Editor-configurable assets/interfaces
        +
Gamestate/Entity/Graph integration
        =
Practical multiplayer authoring surface
```

---

## 2) Network Node Asset

A **Network Node Asset** is a reusable role definition for networked simulation behavior.

Examples of what it can encode:

- whether incoming connection requests are accepted,
- which systems/engines run in the node’s simulation,
- what synchronization behaviors are active,
- role identity (e.g., `server`, `client`, custom roles).

### WHY this abstraction exists

Hardcoding role behavior in code makes iteration expensive and discourages experimentation.
Assetizing role behavior enables rapid role composition in-editor.

### HOW it works conceptually

```text
Network Node Asset (definition)
    -> instantiated as
Network Node (runtime endpoint + simulation)
```

### WHERE it applies

- standard client/server games,
- asymmetric topologies (spectator relay, host migration helpers),
- custom protocols beyond classic models.

### LIMITATIONS

- Asset flexibility can hide important protocol constraints unless teams document role contracts.
- Without governance, role proliferation can cause operational confusion.

---

## 3) Virtual Network Simulation (local/remote transparency)

The API is designed so local peers in the same executable appear equivalent (from user-facing behavior) to remote peers on separate machines.

### Core behavior

- `open_pipe()` and `send()` preserve API semantics regardless of locality.
- If destination is local, communication can be short-circuited in-process.
- Receiver interfaces still execute as if data arrived through the network path.

```text
Case A (remote): send -> UDP path -> receive
Case B (local):  send -> in-proc dispatch -> receive
```

### WHY this is powerful

- Multi-role debugging in one editor session.
- Lower setup friction for reproducing multiplayer bugs.
- Consistent gameplay code paths between local test and deployment.

### LIMITATIONS (critical)

- Local short-circuiting cannot perfectly mirror real network jitter, MTU fragmentation, kernel queue behavior, or NIC contention.
- Teams should still perform remote/distributed validation before shipping.

---

## 4) Component synchronization model

In The Machinery, multiplayer state propagation is largely component-driven.

When registering a component, adding `tm_component_network_replication_i` controls if/how it replicates.
No interface means no replication.

### Baseline automation path

Set `watch_timer > 0` to poll for changes periodically and replicate changed components.

```c
typedef struct replicated_component {
    float foo;
    float bar;
} replicated_component;

static tm_component_network_replication_i *replicated_component_network_replication =
    &(tm_component_network_replication_i){
        .watch_timer = 1.0f,
    };

tm_component_i component = {
    .name = "replicated component",
    .bytes = sizeof(replicated_component),
    .network_replication = replicated_component_network_replication,
};

tm_entity_api->register_component(ctx, &component);
```

Also required: entities must be marked for replication in Entity Tree; otherwise they are excluded.

### WHY this works for prototyping

- low ceremony,
- predictable periodic checks,
- immediate multiplayer behavior for gameplay experimentation.

### HOW changes are encoded initially

Without struct/member layout metadata, component data is treated as one binary blob; any change can trigger sending full component bytes.

### WHERE this is acceptable

- early prototype loops,
- low entity counts,
- LAN/high-bandwidth testing.

### LIMITATIONS

- bandwidth inefficiency for sparse changes,
- potential CPU overhead from polling many components,
- no semantic compression unless custom layout/watch strategies are added.

---

## 5) Gamestate synchronization pipeline

The Simulation and Entity APIs use Gamestate as the canonical state transfer mechanism between contexts.

### Frame-level pipeline (expanded)

```text
[1] propagate_network_changes_to_gamestate()
    - collect local changes
    - execute configured watches

[2] dump_uncompressed_changes()
    - serialize recent diffs into buffer(s)

[3] send diffs to interested nodes
    - packetized via network API

[4] on new connection
    - dump_all() full snapshot
    - send complete state bootstrap

[5] receiver side
    - load_uncompressed_changes() on arrival

[6] gamestate notifies entity context
    - apply changes (same family of mechanics as save-game load)
```

### WHY this architecture is attractive

- Reuses existing gamestate serialization/apply machinery.
- Provides a unified conceptual model for disk persistence and network replication.
- Keeps replication logic close to existing simulation data flow.

### WHERE it applies

- authoritative server publishing state to clients,
- late-join synchronization via full snapshot,
- editor-time simulation mirroring.

### LIMITATIONS

- Full snapshot for new peers may be expensive for large worlds.
- Uncompressed diff dumps can become costly at scale.
- Interest management quality determines practical network load.

---

## 6) Graph event and variable synchronization

Entity Graph supports explicit replication nodes for variables/events.
These nodes perform local action and emit remote replication.

### Event replication semantics

Replicated event payload includes:

- event hash/string identifier,
- target entity identifier,
- ordering/reliability behavior as configured.

### Ordered vs unordered channels

If event type is marked “receive in order,” per-type sequence is preserved.
Unordered types may arrive in any sequence relative to each other.

#### Example reasoning

Given sequence:

```text
foo1, bar1, foo2, bar2
```

If `foo` ordered and `bar` unordered, only `foo1 -> foo2` order is guaranteed.
`bar` instances may interleave before/after in many permutations.

### WHY this granularity matters

Games often have mixed semantics:

- command chains needing order,
- telemetry/FX signals where latest or eventual arrival is enough.

Treating all events as globally ordered is usually unnecessary and harmful to latency.

### LIMITATIONS

- Designers must understand semantic consequences; wrong markings create subtle bugs.
- Cross-type causal dependencies are not automatically preserved by per-type ordering.

---

## 7) Network Switch graph node

The `network switch` node is role-aware branching for graph execution.

### Behavior

- Branches execution by network node type (e.g., `server`, `client`).
- If node has no associated role name (catch-all simulation), all branches execute sequentially.

```text
network_switch(role)
  server -> authoritative path
  client -> presentation/input path
  none   -> execute all (single-player catch-all)
```

### WHY it reduces friction

- Keeps single-player and multiplayer logic in one graph ecosystem.
- Avoids duplicating full logic graphs for role variants.

### WHERE it applies

- shared mechanics with role-specific side effects,
- local offline mode derived from multiplayer codebase.

### LIMITATIONS

- Executing all paths in catch-all mode can hide authority bugs unless carefully guarded.
- Branch complexity can grow; clear role contracts still needed.

---

## 8) Network Profiler as operational tooling

The Network Profiler is not a cosmetic feature; it is a core verification surface.

Capabilities described:

- inspect active nodes and pipes,
- inject artificial latency,
- throttle upload/download bandwidth,
- pause simulation globally,
- inspect packet traffic and status (ACKs, out-of-order counts, etc.).

### WHY this matters

Network behavior is temporal. Without history-aware introspection, debugging often collapses into guesswork.

### Typical debugging loop

```text
1) Reproduce issue with local multi-node simulation
2) Inject latency/jitter constraints
3) Pause all simulation instances
4) Inspect packet type flow and status counters
5) Validate expected ordering/reliability outcomes
6) Adjust packet classes or replication rules
```

### WHERE it applies

- desync diagnosis,
- burst traffic analysis,
- verifying retransmission and ordering assumptions,
- onboarding teams to protocol behavior.

### LIMITATIONS

- Tooling visibility depends on instrumentation depth.
- In-editor conditions are synthetic approximations; production telemetry remains essential.

---

## 9) Practical architecture guidance (beyond baseline)

### 9.1 Split packet taxonomy early

Do not keep one broad “game update” packet forever.
Partition by semantic lifetime and correctness requirements.

```text
- Input commands (ordered, reliable)
- State deltas (possibly unreliable, frequent)
- Critical events (reliable, idempotent handlers)
- Diagnostics/admin (separate channel policy)
```

### 9.2 Treat full-state sync as bootstrap path only

Use full dumps for join/recovery; move routine operation to deltas + interest filtering.

### 9.3 Make handlers idempotent where possible

Given retries/duplicates in real networks, idempotent event application reduces edge-case corruption.

### 9.4 Plan role authority explicitly

`network switch` makes branching easy; authority model must still be designed intentionally.

---

## 10) Mini case studies

### Case A: Fast prototype co-op sandbox

- Enable watch-based replication on key gameplay components.
- Use default node assets (`server`, `client`) with small custom receivers.
- Iterate mechanics first, then measure packet volume in profiler.

**Expected trade-off**: rapid development, high temporary bandwidth cost.

---

### Case B: Competitive action game hardening

- Move from blob component replication to member-level or command-based updates.
- Restrict ordered reliable traffic to truly sequence-sensitive events.
- Introduce stricter authority checks in receiver interfaces.

**Expected trade-off**: more protocol engineering, lower latency variance and better scale.

---

## 11) Conceptual comparison with alternative approaches

### Compared with rigid built-in high-level replication frameworks

**Machinery approach strengths**

- More composable and role-flexible.
- Better alignment with custom game protocol needs.
- Strong editor-time introspection story.

**Potential weakness**

- More design responsibility pushed onto game team.
- Requires stronger network literacy to avoid misuse.

---

## 12) Key insights summary

1. **Network Node Assets** convert protocol roles into reusable authoring units.
2. **Virtual network simulation** accelerates debugging by preserving API behavior across local/remote topology.
3. **Component + Gamestate integration** provides a practical default replication pipeline.
4. **Graph-level event/variable replication** enables gameplay-authoring workflows without abandoning protocol control.
5. **Network profiler instrumentation** is essential for correctness and optimization, not optional polish.

---

## 13) Limitations and what remains for low-level deep dive

Part 2 intentionally stays high-level. Production readiness still depends on lower-level topics such as:

- retransmission window mechanics,
- sequence-number management,
- ACK compression strategies,
- congestion/back-pressure policy,
- robust handshake hardening,
- serialization compatibility/versioning.

These are the bridge from “works in editor” to “ships safely at scale.”

---

## Closing

Part 2 demonstrates that The Machinery’s networking strategy is fundamentally about **composable simulation architecture plus strong debugging ergonomics**. The editor-facing abstractions are designed to minimize early friction while preserving a clear path toward specialized protocol optimization.

The right way to use this stack is iterative:

1. start with defaults to validate game design,
2. instrument behavior aggressively,
3. progressively replace generic replication with semantics-driven packet protocols.
