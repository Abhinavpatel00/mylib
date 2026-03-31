# A Taxonomy of Bugs (Comprehensive Rewrite)

## Executive Summary

Most debugging failures are not about intelligence; they are about mismatched mental models.

A practical model:
1. Reproduce reliably.
2. Observe real execution (debugger, traces, sanitizers).
3. Compare observed behavior to expected behavior.
4. Repair code, assumptions, or design.

This taxonomy expands common bug classes and, for each major class, gives **WHY / HOW / WHERE / LIMITATIONS** so the guidance is actionable.

---

## Baseline Debugging Loop

```
Symptom
  -> Repro case
    -> Observe state transitions
      -> Find model mismatch
        -> Fix + regression test
```

When a bug resists this loop, it usually means one of these is broken:
- you cannot reproduce,
- you are observing the wrong layer,
- the bug is architectural, not local,
- ownership/responsibility is unclear.

---

## 1) The Typo

### WHY
Human text production is noisy. Brains auto-correct patterns while reading, so obvious text errors can stay invisible.

### HOW
Use compiler and editor pressure to catch mistakes early:
- max warnings + warnings-as-errors,
- anti-shadowing warnings,
- formatter on save/commit,
- naming conventions that reduce index confusion,
- narrow variable scope + `const`.

Example typo class:

```c
if (set_x) pos.x = new_pos.x;
if (set_y) pos.x = new_pos.y;  // typo: should write y
```

Defensive loop style:

```c
for (uint32_t item_i = 0; item_i < n_items; ++item_i) {
    for (uint32_t child_i = 0; child_i < n_children; ++child_i) {
        // harder to confuse than i/j in nested loops
    }
}
```

### WHERE
- repetitive boilerplate,
- copied/edited code blocks,
- nested loops and similarly named variables.

### LIMITATIONS
- Tooling reduces, not eliminates, typos.
- Aggressive warnings may require occasional suppression and discipline.

---

## 2) The Logical Error

### WHY
The code is syntactically valid but encodes the wrong algorithm.

### HOW
- Step through with concrete inputs.
- Simplify expressions and branching.
- Prefer linear code paths over “micro-optimized” branchy variants.
- Encapsulate frequent logic in tested helpers.

Common off-by-one trap:

```c
memmove(arr + i, arr + i + 1, (num_items - i) * sizeof(*arr)); // wrong
--num_items;
```

Corrected:

```c
memmove(arr + i, arr + i + 1, (num_items - i - 1) * sizeof(*arr));
--num_items;
```

### WHERE
- index math,
- boundary conditions,
- code with rare branch paths.

### LIMITATIONS
- “Fast paths” can create undertested branches.
- Macros/helpers centralize logic but can hide complexity if overused.

---

## 3) The Unexpected Initial Condition

### WHY
Algorithm is correct **under assumptions** that runtime data violates.

### HOW
Make preconditions explicit with asserts/validation:

```c
void add_flag(flag_t flag)
{
    assert(num_flags < MAX_FLAGS);
    flags[num_flags++] = flag;
}
```

Differentiate:
- caller contract violation,
- callee robustness requirement,
- truly impossible state.

### WHERE
- fixed-capacity arrays,
- parsing and external inputs,
- state machine transitions.

### LIMITATIONS
- Asserts catch bugs in debug/dev builds; production may need defensive error paths.
- “No limits” is impossible in practice; every system has boundaries.

---

## 4) Memory Leak

### WHY
Allocated resources are never released (memory, file handles, threads, locks).

### HOW
Instrument allocation APIs, track owner/context, and aggregate by subsystem.

```c
void *p = my_malloc(size, __FILE__, __LINE__);
...
my_free(p, __FILE__, __LINE__);
```

System-level accounting pattern:

```
System Allocator Counter
  alloc +size
  free  -size
shutdown: assert(counter == 0)
```

### WHERE
- long-running tools/services,
- editor sessions,
- plugin boundaries.

### LIMITATIONS
- Full instrumentation has overhead.
- GC/reference-counted languages replace memory leaks with reference-retention leaks.

---

## 5) Memory Overwrite (Use-After-Free / Buffer Overflow)

### WHY
Writes target memory not owned by the writer; corruption often appears far from root cause.

### HOW
Use an end-of-page debug allocator via virtual memory so invalid writes fault immediately.

ASCII layout:

```
| mapped page(s) ..........object|
| guard/unmapped page ...........| <- overflow hits here => access violation
```

Minimal allocator idea:

```c
void *eop_malloc(uint64_t size)
{
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    char *base = vm_map(pages * PAGE_SIZE);
    return base + (pages * PAGE_SIZE - size);
}
```

On free, unmap pages so stale writes crash instantly.

### WHERE
- C/C++ manual memory management,
- custom allocators,
- serialization/networking decode code.

### LIMITATIONS
- High memory overhead due to page granularity.
- Best for diagnostic builds, not always-on production.
- Some overflows may still evade detection without extra guard-page tactics.

---

## 6) Race Condition

### WHY
Concurrent reads/writes interact with timing, memory ordering, and synchronization mistakes.

### HOW
- Confirm by forcing single-thread mode.
- Add temporary serialization/critical sections to isolate region.
- Use ThreadSanitizer or equivalent.
- Prefer simple concurrency patterns over clever lock-free code.

Threading bug isolation flow:

```
Bug present?
  -> force single-thread
      fixed? yes -> likely race
      no    -> investigate other bug classes
```

### WHERE
- job systems,
- lock-free queues,
- shared caches/global registries.

### LIMITATIONS
- Heisenbugs: instrumentation changes timing.
- Sanitizers can miss some patterns or be too slow for full workloads.
- Correctness often depends on architecture-specific memory model details.

---

## 7) Design Flaw

### WHY
No local patch can fix behavior because the abstraction itself is wrong or ambiguous.

Classic ambiguous API:

```c
const char *ensure_html_encoded(const char *s);
```

Ambiguity: cannot infer whether `"&lt;"` is already encoded or literal input that still needs encoding.

### HOW
- Split ambiguous APIs into explicit operations.
- Encode state in types/data flow instead of heuristic inference.
- Rework call sites, not only implementation internals.

### WHERE
- convenience APIs with hidden mode switches,
- under-specified contracts,
- overloaded functions with subtle behavior differences.

### LIMITATIONS
- Usually requires refactor/migration.
- Backward compatibility pressure can delay the real fix.

---

## 8) Third-Party Bug

### WHY
Failure lies in dependency behavior, unclear docs, integration mismatch, or your misuse.

### HOW
Work by support/access tier:
1. responsive vendor + clear reproduction,
2. source access: diagnose/patch locally,
3. black box: probe behavior and build constraints/workarounds.

### WHERE
- SDKs, drivers, middleware, closed-source services.

### LIMITATIONS
- Blocked by vendor timelines.
- Workarounds add technical debt.
- Root cause confidence can stay low without source.

---

## 9) Failed Specification (You Are the Third Party)

### WHY
Users misuse API because contract is unclear or API shape invites invalid states.

### HOW
Design APIs to make invalid usage hard:

```c
typedef struct profiler_scope_t profiler_scope_t;

profiler_scope_t profiler_begin_scope(const char *name);
void profiler_end_scope(profiler_scope_t scope);
```

Compared to global `begin()`/`end()` without token, this enforces pairing and supports stronger runtime checks.

### WHERE
- public SDK APIs,
- plugin interfaces,
- stateful begin/end protocols.

### LIMITATIONS
- Better API design reduces but does not eliminate misuse.
- Requires docs, samples, and diagnostics for full effectiveness.

---

## 10) Hard-to-Reproduce Bug

### WHY
Low reproduction probability prevents interactive debugging.

### HOW
Two-pronged approach:
1. raise reproduction rate via stress amplification,
2. capture better diagnostics when failure occurs (stack traces, ring-buffer logs, dumps, remote debugging).

Diagnostic capture pattern:

```
runtime events -> circular in-memory log
if failure detected -> flush buffer + stack + context snapshot
```

### WHERE
- production-only failures,
- hardware/driver-specific issues,
- intermittent timing-dependent bugs.

### LIMITATIONS
- Logging overhead and privacy/security constraints.
- Multiple instrumentation iterations often required.

---

## 11) Statistical Bug Triage

### WHY
At scale, qualitative per-report debugging is impossible; prioritization must be data-driven.

### HOW
- collect crash reports automatically,
- cluster by signature (stack + error + version),
- rank by user impact and frequency,
- focus engineering on top clusters.

### WHERE
- products with large user base,
- CI telemetry and crash reporting pipelines.

### LIMITATIONS
- Signature clustering is imperfect (same bug, different stacks; different bugs, similar stacks).
- Rare severe bugs may be under-ranked without severity weighting.

---

## 12) Compiler Bug

### WHY
Compiler incorrectly transforms valid source into wrong machine code.

### HOW
Validate systematically:
- change compiler/toolchain,
- vary optimization level,
- inspect generated assembly,
- rule out undefined behavior before blaming compiler.

Undefined behavior trap example:

```c
int foo(int x) { return (x + 1) > x; }
```

With signed-overflow UB assumptions, optimizer may fold this to constant true.

### WHERE
- high optimization levels,
- edge-case templates/macros/intrinsics,
- new compiler releases.

### LIMITATIONS
- Rare but real.
- Workaround often means code reshaping, not immediate compiler fix.

---

## Cross-Cutting Prevention Strategy

1. **Tooling first**: warnings, sanitizers, formatters, static analysis.
2. **Instrumentation by default**: memory/accounting/trace hooks available on demand.
3. **Simple code paths**: avoid unnecessary branches and stateful API surprises.
4. **Explicit contracts**: assertions, typed tokens, documented preconditions.
5. **Debuggability architecture**: pluggable allocators, reproducible test harnesses, subsystem isolation toggles.

---

## Final Takeaway

A “taxonomy of bugs” is useful only if it changes your engineering system.

The core move is to shift from heroic debugging to **designed observability and explicit contracts**:
- make failures happen closer to root cause,
- make assumptions executable (assertions, types, invariants),
- make bug volume measurable and prioritizable.

Then bugs stop being mysterious events and become routine operational work.