# Sensible Error Handling, Part 2 — Expected Errors Without Caller Pain

Expected errors are failures the caller must anticipate and handle (network timeouts, removable media missing). The goal is to expose them explicitly while minimizing clutter and ambiguity.

## Design Rule

> **Minimize points of failure and minimize kinds of failure.**

- **Points**: restrict where an error can surface. Fewer checks → fewer code paths.
- **Kinds**: use small, function-specific enums or booleans, not sprawling integer codes.

## Shaping APIs for Expected Errors

Structure APIs so that only a few functions can fail, and only in well-defined ways.

```cpp
class SaveSystem {
public:
    struct Data { const char* p; unsigned len; };
    enum LoadResult { IN_PROGRESS, COMPLETED, FAILED };

    unsigned num_saved_games();
    LoadId   start_loading_game(unsigned i);   // may initiate async I/O
    LoadResult load_result(LoadId id);         // single place to observe failure
    Data     loaded_data(LoadId id);           // only valid if COMPLETED
    void     free_data(LoadId id);
};
```

Only `load_result()` requires an error check; failure semantics are binary.

### Why this works

- Callers see a tiny decision surface: poll until `COMPLETED` or `FAILED`.
- Error meaning is unambiguous; logging carries extra detail if needed.

### Where it applies

- Asynchronous or multi-step workflows (loading, network requests, background compilation).
- Interfaces where most functions are pure queries or simple state changes.

### Limitations

- Over-binarizing can hide actionable nuances; add scoped enums when callers truly need detail.

## Returning Results and Errors Together

Prefer returning small structs instead of “out parameters” when multiple values are needed:

```cpp
struct SaveResult {
    enum Error { NO_ERROR, DISK_FULL, WRITE_ERROR } error;
    unsigned saved_bytes;
};
SaveResult save_result(SaveId id);
```

Why:

- Clear ownership of both data and status.
- Avoids awkward “return in parameter” patterns except for heap-owning types.

## Avoiding Exceptions for Expected Errors

Exceptions introduce hidden control flow and “infectious” declarations. Even with throw-specs, callers must reason about transactional safety everywhere.

Instead:

- Make the possible failures part of the explicit API surface (enums/booleans).
- Keep error sites localized so callers know where to branch.

## Example: Function-Specific Enum

```cpp
enum LoadResult { IN_PROGRESS, COMPLETED, FILE_NOT_FOUND, FILE_COULD_NOT_BE_READ, FILE_CORRUPTED };
LoadResult load_result(LoadId id);
```

Small, precise enumerations trump generic integers. Avoid overlaps and ambiguity (e.g., `EWOULDBLOCK` vs `EAGAIN`).

## Checklist for Expected-Error APIs

- [ ] Only designated functions can fail; document them clearly.
- [ ] Use minimal, specific enums or booleans for failure modes.
- [ ] Return data and status together when practical.
- [ ] Log rich diagnostics for failures without burdening the caller with details.

## Key Insights

- Caller effort scales with the number of branches and error meanings; minimize both.
- Expected errors belong in the API surface—explicit, small, and localized.
- Struct return values make multi-result functions readable and less error-prone.
- Exceptions add hidden paths; explicit enums keep control flow obvious.

Part 3 addresses warnings: making them visible, actionable, and eventually disappear.
