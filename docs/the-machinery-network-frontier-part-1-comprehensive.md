# The (Machinery) Network Frontier — Part 1 (Comprehensive Rewrite)

## Abstract

This article presents the architectural foundations of The Machinery’s networking layer from a systems-design perspective. The original post introduces goals and base terminology; this rewrite deepens those ideas into design principles, failure models, and implementation implications.

The core thesis is:

- Treat networking as a first-class simulation concern, not a bolt-on transport utility.
- Build for debuggability and protocol evolution from day one.
- Offer default automation for prototyping, while preserving explicit control for optimization.

---

## 1) Context and intent

The Machinery aims to make multiplayer development practical from inside the editor, before production infrastructure exists. That means the networking stack must support two conflicting needs simultaneously:

1. **Fast prototyping**: low setup cost, immediate iteration.
2. **Long-term correctness/performance**: explicit semantics, inspectability, and room for game-specific protocol design.

This tension is the central design problem solved in Part 1.

---

## 2) Design goals, expanded

### 2.1 Debuggability

#### WHY it matters

Distributed state introduces combinatorial failure modes:

- packet delay/reordering,
- diverging simulation timelines,
- authority mistakes,
- race-like logic bugs across endpoints.

Debugging this with external tools alone is slow and fragile.

#### HOW the architecture addresses it

- Multiple network roles (e.g., client/server/login) can run in the same editor process.
- The same APIs are used whether peers are local or remote.
- The model anticipates packet-history introspection (network profiler workflows).

#### WHERE it applies

- Reproducing bugs that require coordinated multi-role behavior.
- Verifying temporal assumptions (e.g., “this ACK arrived two frames late”).
- Testing degraded network conditions without staging full infrastructure.

#### LIMITATIONS

- In-process simulation cannot perfectly emulate all OS/NIC/kernel-level behavior.
- Production-grade determinism requires stronger capture/replay tooling and strict versioned protocol handling.

---

### 2.2 Easy evolution and maintenance

#### WHY it matters

Game networking differs radically by genre and topology:

- lockstep RTS,
- client-authoritative action prototypes,
- server-authoritative shooter,
- P2P experiments.

A rigid built-in model blocks valid designs.

#### HOW

- Packet “kinds” are defined through API registration rather than hardcoded global schema.
- Delivery semantics are attached to packet type.
- Network interfaces modularize behavior (bootstrap, accept, receive).

#### WHERE

- Rapid protocol iteration during development.
- Introducing game-specific packet classes without rewriting network internals.
- Transitioning from generic replication to custom deltas/commands.

#### LIMITATIONS

- Flexibility increases responsibility: teams must define authority, trust boundaries, and abuse controls themselves.
- Without conventions, packet taxonomies can fragment and become inconsistent.

---

### 2.3 Low friction between single-player and multiplayer

#### WHY

Retrofitting multiplayer late is costly because local-only code path assumptions leak everywhere.

#### HOW

- Event-centric simulation model maps naturally to remote event propagation.
- Network node abstraction allows local and remote execution to share behavior contract.
- Role-aware logic can branch by node type while preserving one gameplay graph.

#### WHERE

- Projects shipping both offline and online modes.
- Teams gradually introducing multiplayer into existing single-player mechanics.

#### LIMITATIONS

- Some designs still require architectural rework (prediction, rollback, authority separation).
- “Same gameplay logic in both modes” is a goal, not a guarantee, for all genres.

---

### 2.4 Automation first, optimization later

#### WHY

Protocol micro-optimization too early kills iteration speed.

#### HOW

- Provide coarse default replication behavior that “just works.”
- Keep APIs explicit enough to replace defaults with game-specific strategies.

#### WHERE

- Prototyping, vertical slices, mechanic validation.

#### LIMITATIONS

- Default replication is often bandwidth-inefficient.
- Shipping at scale requires custom packet design, selective replication, and tighter reliability choices.

---

## 3) Core abstractions and data model

Part 1 defines a minimal vocabulary. Here we formalize each concept.

---

### 3.1 Simulation Instance

A simulation instance is an isolated runtime world with its own entity context and update loop.

```text
Editor Process
├─ Simulate Tab A -> Simulation Instance A
├─ Simulate Tab B -> Simulation Instance B
└─ Preview Tab    -> Simulation Instance C
```

#### WHY this boundary is useful

- Isolation avoids accidental shared-state coupling.
- Multi-role multiplayer testing can happen inside one process.

#### LIMITATIONS

- Shared executable still shares some global process-level resources unless explicitly separated.

---

### 3.2 Network Node

A network node binds a simulation instance to an addressable endpoint (`ip:port`) and role-specific behavior.

```text
Simulation Instance + Endpoint + Interfaces => Network Node
```

Think of nodes as graph vertices in a communication topology.

#### WHERE it applies

- client/server,
- dedicated services (matchmaker/login),
- test harness nodes.

---

### 3.3 Pipe (unidirectional channel)

A pipe models one-way communication from node `n1` to node `n2`.

```text
n1 ----pipe_id=42----> n2
n2 ----pipe_id=7-----> n1   (separate pipe)
```

#### WHY unidirectional?

- Simpler sequencing/reliability accounting per direction.
- Clear ownership of send state.

#### LIMITATIONS

- Bidirectional logical sessions require managing two channels.
- Tooling must present paired pipes coherently for usability.

---

## 4) Handshake model (pipe establishment)

When `n1` wants to send, it requests a pipe to `n2`.

### Baseline flow

```text
(1) n1 wants n1->n2 pipe
(2) n1 allocates local pipe_id (monotonic uint32)
(3) n1 -> n2 : PIPE_REQUEST{pipe_id}
(4) n2 applies accept policy
(5) n2 -> n1 : PIPE_RESPONSE{pipe_id, accepted?}
(6) if accepted, n1 starts sending payload packets
```

### Duplicate handling

If `n2` already accepted a given request, duplicate requests can be answered idempotently with success.

#### WHY this works

- Monotonic IDs avoid local reuse ambiguity.
- Explicit accept step supports role/policy checks.
- Idempotent responses reduce duplicate request hazards.

#### LIMITATIONS (important)

As stated in the original source, the handshake is prototype-grade, not production-final. Missing hardening typically includes:

- authentication/authorization,
- anti-spoofing/session binding,
- replay protection,
- timeout/backoff policy formalization,
- robust reconnect semantics after partial failures.

---

## 5) Packet types as semantic contracts

Every transmitted payload is tagged with a packet type.

```c
tm_strhash_t packet_hash = tm_murmur_hash_string("test_packet");
tm_network_api->define_packet(
    network,
    packet_hash,
    TM_NETWORK_GURANTEED_DELIVERY__DELIVER_ALL_ALLOW_DUPLICATES
);

uint16_t port = 555;
tm_network_node_o *node = tm_network_api->create_node(network, port);
tm_network_pipe_id pipe = tm_network_api->open_pipe(network, node,
    (tm_network_address_t){ .ip = "127.0.0.1", .port = 666 });

while (application_running) {
    char packet_data[64];
    snprintf(packet_data, sizeof(packet_data), "hello world");
    tm_network_api->send(network, pipe, packet_hash,
        packet_data, (uint32_t)strlen(packet_data));
}
```

(Uses `snprintf()` for bounded formatting.)

### WHY packet typing matters

- Receiver dispatch is deterministic by type.
- Reliability and ordering can be bound to semantics, not transport globally.
- Profiling/diagnostics can aggregate by behavior class.

### WHERE it applies

- separating high-frequency ephemeral updates from must-deliver events,
- staged protocol evolution by adding new packet classes.

### LIMITATIONS

- Hash-based identity can collide in theory (low probability with good hashes, but non-zero).
- Unversioned packet payloads become brittle over time.

Recommendation: pair type hashes with schema/version fields in production protocols.

---

## 6) Delivery guarantees over UDP

The transport under the hood is UDP. Delivery semantics are implemented above transport per packet type.

### Why UDP + custom reliability can be desirable

- Avoid head-of-line blocking inherent in strict ordered reliable streams.
- Customize guarantees per gameplay signal type.

### Typical semantic categories (conceptual)

```text
A) Unreliable / unordered
   - latest state hints, transient FX
B) Reliable / may duplicate
   - idempotent events where loss is worse than duplicate
C) Reliable / ordered
   - sequence-dependent commands
```

The API’s packet-definition model is designed to map data types into such classes.

### LIMITATIONS

- Application-level reliability introduces complexity: ACK tracking, retransmission windows, duplicate suppression, reorder buffers.
- Incorrect guarantee selection can silently damage gameplay behavior.

---

## 7) Network interfaces: modular behavior injection

Interfaces are callbacks attached to node behavior at key lifecycle points.

Examples from the source model:

- `receiver`: handles incoming packets and dispatches meaningfully.
- `bootstrapper`: runs after node creation (initial connections/setup).
- `accepter`: decides whether incoming pipe requests are accepted.

Registration style (as in standard Machinery interface patterns):

```c
static void bootstrap_client(struct tm_network_o *network,
    void *network_context,
    struct tm_network_node_o *node)
{
    tm_network_pipe_id pipe = tm_network_api->open_pipe(network, node,
        (tm_network_address_t){ .ip = "127.0.0.1", .port = 666 });
    (void)pipe;
}

static tm_network_bootstrap_i *bootstrap_client_i = &(tm_network_bootstrap_i){
    .name = "bootstrap_client",
    .bootstrap = bootstrap_client,
};

tm_add_or_remove_implementation(reg, load,
    tm_network_bootstrap_i, bootstrap_client_i);
```

### WHY interface modularity is valuable

- clear separation of transport concerns from game logic,
- composable behavior per node role,
- easier testability of protocol logic chunks.

### LIMITATIONS

- Callback ordering/ownership contracts must be explicit.
- Multiple handlers of the same type need deterministic dispatch policy.

---

## 8) End-to-end message lifecycle (conceptual)

```text
[Game Logic]
    |
    | define_packet(type, guarantee)
    v
[Network API]
    |
    | open_pipe(n1->n2) -> handshake
    v
[Pipe Established]
    |
    | send(pipe, type, payload)
    v
[Transport Layer (UDP + reliability machinery)]
    |
    v
[Receiver Interfaces on n2]
    |
    v
[Gameplay/System Dispatch]
```

This decomposition keeps policy at the packet/interface level while transport remains generic.

---

## 9) Practical usage patterns

### Pattern A: rapid prototype defaults

- define few packet types,
- broad reliability settings,
- fast iteration in-editor with local multi-node simulation.

### Pattern B: progressive hardening

- split packet taxonomy by latency sensitivity,
- tighten receiver validation,
- add schema/versioning,
- improve handshake security/session model.

### Pattern C: custom protocol architectures

- client-server with authoritative simulation,
- hybrid or service-oriented setups,
- experimental P2P overlays.

---

## 10) Common failure modes and mitigation ideas

1. **Ambiguous authority boundaries**
   - Mitigation: define authoritative owner per replicated domain.
2. **Overusing reliable-ordered channels**
   - Mitigation: classify packets by semantic need.
3. **No protocol version discipline**
   - Mitigation: embed packet version + migration strategy.
4. **Weak duplicate handling**
   - Mitigation: idempotent handlers and sequence checks where needed.
5. **Insufficient observability**
   - Mitigation: packet tracing and per-type metrics from day one.

---

## 11) Key insights summary

1. Networking architecture is primarily a **debuggability and evolution** problem, not only a bandwidth problem.
2. The `node -> pipe -> packet type -> interface` stack is small but expressive.
3. Per-packet delivery semantics are essential for gameplay-correct latency behavior.
4. In-editor multi-role execution is a force multiplier for multiplayer iteration.
5. Prototype-friendly defaults are valuable only if the system remains explicitly optimizable.

---

## 12) What Part 2 builds on this foundation

Part 2 introduces higher-level user-facing constructs that compose these primitives:

- Network Node Assets,
- virtual network simulation behavior in-editor,
- component and gamestate synchronization,
- graph event/variable replication,
- role-based execution switching,
- network profiling workflows.

These mechanisms are the bridge from transport concepts to daily gameplay authoring.
