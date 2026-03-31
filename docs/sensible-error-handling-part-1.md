# Sensible Error Handling, Part 1 — Unexpected Errors and the Crash Philosophy

This part focuses on the class of failures callers cannot sanely recover from. The aim is to design APIs that stay readable, keep caller burden low, and surface bugs early.

## Taxonomy of Failures

- **Expected errors**: caller anticipates them and has a plan (network drop, removable media pulled).
- **Unexpected errors**: caller has no reasonable recovery path (corrupted state, out-of-memory, invalid arguments, missing critical data).
- **Warnings**: questionable situations where execution can continue (deprecated use, suspicious data). Covered in Part 3.

Context matters: `FileNotFound` is expected when opening a user config, but unexpected when loading mandatory game bundles.

## Policy for Unexpected Errors

> **Crash fast with an informative report.**

### Why crashing improves quality

- Eliminates silent corruption and “limp-along” behavior that hides bugs.
- Removes error-handling clutter from callers; APIs promise to either succeed or crash.
- Forces fixes early in development; crashes cannot be ignored the way warnings can.

### How to express this in APIs

Design APIs without error returns for unexpected failures:

```cpp
bool exists(const char* path);           // for expected absence
Archive open(const char* path);          // crashes if malformed/missing
```

Inputs are validated internally; on violation, the process aborts with a clear message.

### Where this applies

- Game engines and tools where user safety is not at stake and crashes are acceptable during development.
- Subsystems whose callers cannot recover meaningfully (resource loading, allocators, invariant checks).

### Limitations

- Not suitable for safety-critical software.
- Requires robust testing pipelines to catch crashes before release builds; release builds may translate asserts into user-facing error screens if needed.

## Assertions vs “Patch It Up” Code

Temptation: return best-effort values (empty file, partial JSON) instead of crashing.

Why this is harmful:

- Programmers waste time inventing fragile fallback logic; cascading failures appear later and are harder to diagnose.
- “Stern” log messages get ignored; project accumulates latent glitches.

A deliberate crash makes ownership clear: the API owns correctness; the caller writes simpler, clearer code.

## Exceptions? No Thanks

Exceptions blur the line between expected and unexpected errors and introduce hidden control flow.

Questions every caller must now ask: which functions throw, which exceptions, do I need transactional semantics everywhere, what about constructors/destructors?

The crash model is simpler: success or abort. For expected failures, expose them explicitly (Part 2) rather than via exceptions.

## Crafting Useful Crash Reports

A good crash should answer “what, where, and with what context.”

### Components of a report

- **Message**: human-readable description.
- **Call stack**: translated to file/line when possible.
- **Context stack**: high-level breadcrumbs (e.g., which level/unit/material was being processed).

### Variadic assert macro

```cpp
#if defined(DEVELOPMENT)
#   define XASSERT(test, msg, ...) \
        do { if (!(test)) error(__LINE__, __FILE__, "Assert: %s\n" msg, #test, __VA_ARGS__); } while (0)
#else
#   define XASSERT(test, msg, ...) ((void)0)
#endif
```

### Context stack via RAII

```cpp
__THREAD Array<const char*>* ctx_name;
__THREAD Array<const char*>* ctx_data;

class ErrorContext {
public:
    ErrorContext(const char* name, const char* data) {
        ctx_name->push_back(name); ctx_data->push_back(data);
    }
    ~ErrorContext() {
        ctx_name->pop_back(); ctx_data->pop_back();
    }
};
```

Usage:

```cpp
void init_level(const char* level) {
    ErrorContext ec("Spawning level", level);
    load_level(level);    // crashes will print stacked contexts
}
```

### Sample output

```
Spawning level: big_world
Spawning unit: big_bird
Applying material: feathers
Assert failed: texture != NULL
    Texture not loaded: yellow_feathers
    material_manager.cpp:1337
```

### Platform notes

- Windows: generate stacks with `StackWalk64`; resolve with Sym* APIs.
- Use thread-local context stacks for multi-threaded systems.

## Key Insights

- Separate unexpected errors from expected ones; treat the former as irrecoverable.
- Crashing simplifies APIs and forces timely fixes.
- Rich crash reports (message + stack + context) turn crashes into fast diagnostics.
- Avoid exceptions for unexpected errors; they complicate reasoning and blur responsibilities.

Part 2 covers expected errors; Part 3 covers warnings.
