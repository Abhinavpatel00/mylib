# Building an Engine Plugin System — Minimal, Versioned, and Data-Oriented

A plugin system lets teams extend an engine without forking its source. This rewrite details the tradeoffs, shows compact C APIs, and explains how to keep plugins compatible across engine evolution.

## Goals

- Ship plugins as DLLs/so files—no engine rebuilds for feature extensions.
- Keep coupling low: small, C-ABI-stable interfaces.
- Allow engine and plugins to query each other’s APIs by version.
- Make plugin installation simple (drop-in).

## Why Not Just Modify the Engine?

- Rebuild cost and toolchain friction for every tweak.
- Merge hell when upstream changes land.
- Refactors in engine internals would constantly break local changes.
- Sharing binaries is easier than sharing patches.

## Two APIs to Design

1. **Engine → Plugin**: lifecycle callbacks and any plugin-exposed functions.
2. **Plugin → Engine**: services the plugin calls (spawn, render, Lua hooks, etc.).

Both must be versioned so they can change independently.

## Avoiding a Giant Shared DLL

Common but brittle approach: place shared functions in one massive DLL that both engine and plugins link against. This creates strong coupling; refactors ripple through every plugin.

Better: pass function tables (C structs of function pointers) at runtime. No link-time dependency beyond a tiny header.

ASCII comparison:

```
[Engine] ---link---> [Shared DLL] <---link--- [Plugin]   (tight coupling)

[Engine] --passes--> {EngineApi*} --> [Plugin]            (loose coupling)
```

## Engine API as Queried Interfaces

Define small, versioned API structs and a `GetApiFunction` dispatcher.

```cpp
// api_ids
#define WORLD_API_ID 0
#define LUA_API_ID   1

struct WorldApi_v0 {
    void (*spawn_unit)(World* world, const char* name, float pos[3]);
};

struct WorldApi_v1 {
    void (*spawn_unit)(World* world, const char* name, float pos[3], float rot[4]);
};

struct LuaApi_v0 {
    void (*add_module_function)(const char* module, const char* name, lua_CFunction f);
    double (*to_number)(lua_State*, int);
    void (*push_number)(lua_State*, double);
};

typedef void* (*GetApiFunction)(unsigned api, unsigned version);
```

Engine exposes a single entry point to plugins:

```cpp
void* get_engine_api(unsigned api, unsigned version) {
    if (api == WORLD_API_ID && version == 1) return &world_api_v1;
    if (api == WORLD_API_ID && version == 0) return &world_api_v0;
    if (api == LUA_API_ID   && version == 0) return &lua_api_v0;
    return nullptr;
}
```

## Plugin Side: Query, Then Use

```cpp
static WorldApi_v1* world_api = nullptr;
static LuaApi_v0*   lua_api   = nullptr;

void init(GetApiFunction get_engine_api) {
    world_api = (WorldApi_v1*)get_engine_api(WORLD_API_ID, 1);
    lua_api   = (LuaApi_v0*)get_engine_api(LUA_API_ID, 0);

    if (lua_api)
        lua_api->add_module_function("Plugin", "test", test);
}
```

### Why this pattern works

- Plugins are built against headers only; no binary link to engine internals.
- Versioning allows additive and breaking changes without killing old plugins.
- APIs are small and data-oriented—plain function pointers, no C++ ABI fragility.

## Plugin API: Symmetric Querying

Expose plugin functionality through the same query mechanism so the engine can evolve too.

```cpp
#define PLUGIN_API_ID 2

struct PluginApi_v0 {
    void (*init)(GetApiFunction get_engine_api);
};

extern "C" __declspec(dllexport)
void* get_plugin_api(unsigned api, unsigned version) {
    if (api == PLUGIN_API_ID && version == 0) {
        static PluginApi_v0 api0{ init };
        return &api0;
    }
    return nullptr;
}
```

Engine loads a plugin like this:

```cpp
void load_plugin(const char* path) {
    HMODULE mod = LoadLibrary(path);
    if (!mod) return;
    auto get_plugin_api = (GetApiFunction)GetProcAddress(mod, "get_plugin_api");
    if (!get_plugin_api) return;
    auto* p = (PluginApi_v0*)get_plugin_api(PLUGIN_API_ID, 0);
    if (!p) return;
    p->init(get_engine_api);
}
```

## Versioning Strategy

- **Additive changes**: extend structs and bump version (v1, v2...).
- **Breaking changes**: keep old versions available as long as feasible; remove only with explicit major release.
- **Discoverability**: provide a tiny JSON or header in the SDK listing supported versions.

## Lua vs C for Engine Calls

Routing all engine calls through Lua would be simple but:

- Exposes too much low-level API to Lua.
- Adds marshalling overhead between C++ plugin and Lua.

C function tables keep things minimal and fast while leaving Lua as an optional layer for gameplay-facing hooks.

## Handling Opaque Types Safely

Forward-declare types (`struct World; struct lua_State;`) and only pass pointers around; plugins treat them opaquely. This keeps binary interfaces stable even if internals change.

## Failure Modes and Mitigations

- **API mismatch**: plugin asks for v1 but engine only has v0 → return null; plugin should fall back or fail with a clear error.
- **ABI drift**: keep all API structs POD and compiled with the same calling convention.
- **Symbol resolution**: enforce consistent exported names (`get_plugin_api`) and document them.

## Key Insights

- Runtime API querying beats monolithic shared libraries for long-term compatibility.
- Keep APIs C-like: POD structs of function pointers, no templates or STL types crossing the boundary.
- Versioning must be explicit and symmetric (engine APIs and plugin APIs both queried).
- Minimal, well-factored interfaces encourage refactoring inside the engine without breaking plugins.

This structure scales from tiny “add one Lua function” plugins to large systems that need render, physics, and scripting services, without freezing the engine’s internal architecture.
