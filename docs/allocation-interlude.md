# Allocation Interlude: Building a Buddy Allocator Animation in JavaScript

The original interlude makes a strong point: diagrams for systems programming do not have to be static images. They can be executable models.

This rewrite turns that idea into a structured technical guide.

## Why Programmatic Illustrations Matter

For allocator internals, static diagrams show final states but hide transitions. An animation can expose:

- split cascades,
- free-list mutations,
- merge propagation,
- and failure paths (out-of-memory).

That makes reasoning about invariants much easier than manually redrawing states.

## Scope of the Demo

The original JavaScript code is a visualization-oriented simulator of buddy allocation behavior, not a production allocator.

It intentionally prioritizes clarity over raw speed and includes:

- a tree view by levels,
- a textual console,
- current allocation list,
- and free-list link overlays.

## High-Level Architecture

```text
+--------------------+      next()      +----------------------+
|  Mutator Generator | ----------------> | State (Buddy.data)   |
|  (init/alloc/free) |                   | blockState/freeLists |
+--------------------+                   +----------+-----------+
                                                    |
                                                    | draw()
                                                    v
                                          +----------------------+
                                          | Canvas Renderer      |
                                          | tree + log + legend  |
                                          +----------------------+
```

### Core design choice

The animation is driven by generators (`function*`) and `yield` points. Each yield is a visible animation step.

This is a clean way to encode multi-step allocator operations without callback nesting.

## State Model

The original object has two domains:

1. `data`: mutable simulation state.
2. `layout`: rendering constants.

Representative state:

```js
{
  levels: 5,
  size: 128,            // root size (K units in the UI)
  blockState: [],       // "split" | "allocated" | "free" | undefined
  freeLists: [],        // array per level, holding block indices
  allocations: [],      // currently allocated block indices
  console: []           // log lines
}
```

### Block indexing

The demo uses implicit binary-tree indexing:

- root = `0`
- children of `i` are `2*i + 1` and `2*i + 2`
- level of block `i` is `floor(log2(i+1))`

This mirrors the same indexing scheme discussed in the allocator article.

## Rendering Model

The drawing is split into composable functions:

- `drawConsole`
- `drawAllocations`
- `drawTree`
- `drawLegend`

### Why this decomposition is effective

- Clear separation between model mutation and rendering.
- Easier to tune layout without touching allocator logic.
- Good foundation for swapping Canvas with SVG/WebGL later.

## Generator-Driven Simulation

Operations like `allocate()` and `free()` are generators, not plain functions.

### `split(level)`

Behavior:

1. Ensure a free block exists at current/ancestor level (recursive demand split).
2. Remove one block from free list.
3. Mark it `split`.
4. Create two children marked `free`.
5. Push children to next level free list.

### `allocate(size)`

Behavior:

1. Resolve target level from requested size.
2. Split as needed.
3. Pop one block at target level.
4. Mark `allocated`, append to allocation list.

### `free(p)` + `merge(p)`

Behavior:

1. Mark block `free` and return it to level list.
2. If buddy is free, remove both, mark parent free.
3. Recurse upward.

This is exactly the pedagogical core of buddy allocation.

## Why Generators Fit This Problem

Without generators, each operation would need a state machine with explicit program counters or nested timers.

Generators make state transitions linear in source code while still exposing frame-by-frame animation points.

```js
function* free(p) {
  log(`> free(${p})`);
  markFree(p);      yield;
  insertFree(p);    yield;
  yield* merge(p);  yield;
  log("ok");
}
```

This keeps educational code readable.

## Animation Loop

Original loop:

- call `mutator.next()` once per tick,
- clear canvas,
- redraw full scene,
- schedule next tick with `setTimeout`.

This is simple and deterministic at chosen interval (`animationStepMs`).

### Limitation

`setTimeout` is not synchronized with display refresh and can jitter.

### Better default today

Use `requestAnimationFrame` and accumulate elapsed time for fixed simulation steps.

## Technical Limitations in the Original Demo

1. Random test workload is non-deterministic.
- Reproducibility is hard.

2. No invariants checker.
- Visual state could drift without assertion failures.

3. Full redraw every frame.
- Fine for small scenes, but scales poorly.

4. No hi-DPI handling.
- Canvas can look blurry on high-density displays.

5. Browser compatibility note is now dated.
- ES2015 support is broadly available in modern browsers.

## Recommended Enhancements

### 1. Deterministic RNG

Seeded RNG allows replaying exact mutation traces.

### 2. Invariant assertions

After each mutator step, verify:

- every free-list index has state `free`,
- no block appears twice in free lists,
- parent `split` state matches children existence.

### 3. Instrumentation panel

Show:

- split count,
- merge count,
- allocation failures,
- occupancy per level.

### 4. Replay controls

Add pause/step/backtrack with timeline snapshots.

### 5. `requestAnimationFrame`

Use display-synced rendering while keeping fixed-step simulation semantics.

## Modernized Minimal Skeleton

```html
<canvas id="buddy" width="1000" height="560"></canvas>
<script type="module">
class BuddyViz {
  constructor() {
    this.data = {
      levels: 6,
      size: 256,
      blockState: [],
      freeLists: Array.from({ length: 6 }, () => []),
      allocations: [],
      log: []
    };
    this.stepMs = 200;
    this.accum = 0;
    this.lastTs = 0;
    this.mutator = this.animate();
  }

  *animate() {
    while (true) {
      yield* this.init();
      for (let i = 0; i < 120; i++) {
        if (Math.random() < 0.6 || this.data.allocations.length === 0) {
          const block = Math.floor(Math.random() * ((1 << this.data.levels) - 1));
          const level = Math.floor(Math.log2(block + 1));
          yield* this.allocate(this.sizeOfLevel(level));
        } else {
          const idx = Math.floor(Math.random() * this.data.allocations.length);
          yield* this.free(this.data.allocations[idx]);
        }
      }
      this.reset();
      yield;
    }
  }

  sizeOfLevel(level) { return this.data.size / (1 << level); }
  levelOfSize(size) { return Math.log2(this.data.size / size); }

  *init() {
    this.data.blockState.fill(undefined);
    this.data.freeLists.forEach(fl => fl.length = 0);
    this.data.allocations.length = 0;
    this.data.blockState[0] = "free";
    this.data.freeLists[0].push(0);
    yield;
  }

  *allocate(size) {
    const L = this.levelOfSize(size);
    if (this.data.freeLists[L].length === 0 && L > 0) yield* this.split(L - 1);
    if (this.data.freeLists[L].length === 0) return;
    const p = this.data.freeLists[L].shift();
    this.data.blockState[p] = "allocated";
    this.data.allocations.push(p);
    yield;
  }

  *split(level) {
    if (level < 0) return;
    if (this.data.freeLists[level].length === 0) yield* this.split(level - 1);
    if (this.data.freeLists[level].length === 0) return;
    const p = this.data.freeLists[level].shift();
    this.data.blockState[p] = "split";
    const a = 2 * p + 1, b = 2 * p + 2;
    this.data.blockState[a] = "free";
    this.data.blockState[b] = "free";
    this.data.freeLists[level + 1].push(a, b);
    yield;
  }

  *free(p) {
    const level = Math.floor(Math.log2(p + 1));
    this.data.blockState[p] = "free";
    this.data.freeLists[level].push(p);
    yield* this.merge(p);
    this.data.allocations = this.data.allocations.filter(x => x !== p);
    yield;
  }

  *merge(p) {
    const level = Math.floor(Math.log2(p + 1));
    if (level === 0) return;
    const buddy = (p & 1) ? (p + 1) : (p - 1);
    if (this.data.blockState[buddy] !== "free") return;

    this.data.freeLists[level] = this.data.freeLists[level].filter(x => x !== p && x !== buddy);
    this.data.blockState[p] = undefined;
    this.data.blockState[buddy] = undefined;

    const parent = Math.floor((p - 1) / 2);
    this.data.blockState[parent] = "free";
    this.data.freeLists[level - 1].push(parent);
    yield* this.merge(parent);
  }

  reset() {
    this.data.blockState = [];
    this.data.freeLists = Array.from({ length: this.data.levels }, () => []);
    this.data.allocations = [];
  }

  tick(ts) {
    if (!this.lastTs) this.lastTs = ts;
    this.accum += (ts - this.lastTs);
    this.lastTs = ts;

    while (this.accum >= this.stepMs) {
      this.mutator.next();
      this.accum -= this.stepMs;
    }

    this.draw();
    requestAnimationFrame((t) => this.tick(t));
  }

  draw() {
    // Same conceptual draw pipeline as original: clear + draw tree/log/legend.
  }
}

const viz = new BuddyViz();
requestAnimationFrame((t) => viz.tick(t));
</script>
```

## Where This Approach Applies

- Educational visualizations of allocator/data-structure behavior.
- Debug dashboards for runtime systems.
- Tooling where step-by-step transitions are more valuable than throughput.

## Where It Does Not

- High-performance production allocator path.
- Massive-scale simulation without incremental rendering.
- Environments requiring strict deterministic frame timing with browser variability.

## Key Insights

1. Executable diagrams reveal dynamic invariants that static diagrams hide.
2. Generator functions are an elegant fit for staged operations.
3. The same tree/index formulas can power both allocator implementation and visualization.
4. Simulation code should still enforce invariants; visual correctness is not proof of logical correctness.
5. Small visualization tools can materially improve systems-level understanding and debugging speed.
