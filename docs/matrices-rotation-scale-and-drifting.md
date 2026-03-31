# Matrices, Rotation, Scale, and Drifting — Understanding and Fixing Numerical Creep

Rotating a scaled transform stored as a 4×4 matrix can slowly distort its scale due to floating-point error. This article explains why, quantifies the drift, and offers four fixes with tradeoffs.

## Why Scale Drifts When Stored in a Matrix

Rotation and scale share the upper-left 3×3 of a Matrix4x4:

```
[ R*S | t ]
[ 0   | 1 ]
```

To change rotation without changing scale you typically:
1) Extract scale from the 3×3.
2) Build rotation matrix.
3) Reapply scale and write back.

Each extraction/reapply introduces tiny rounding error. Repeating every frame causes **linear error growth** (e ∝ frames) because error is proportional to current scale, not current error.

## Measured Drift (example)

```
Error   Frames  Time @60Hz
1e-6      202    3 s
1e-4    14393    4 min
1e-3   100575   28 min
```

For animated nodes, 0.1% scale error appears within minutes—visible and unacceptable for a general engine.

## Why Translation+Rotation Don’t Drift

Translation is in the 4th column; rotation in 3×3. Setting one doesn’t numerically perturb the other. Scale shares storage with rotation, so operations intermingle.

## Four Fixes

### 1) Store rotation and scale separately (preferred)

`Pose { Vector3 t; Matrix3x3 r; Vector3 s; }`

- Rotation edits don’t touch scale → no drift.
- Memory: 15 floats vs 16 for Matrix4x4.
- Compose/invert by converting to temp Matrix4x4; estimated ~0.2% frame cost (scene graph transforms ~12% heavier but small overall).

**Where**: engines willing to refactor transforms; codebases valuing correctness over tiny perf cost.

### 2) Require rotation+scale to be set together

Provide `set_rotation_and_scale(pose, rot, s)`; disallow “rotate-only”.

- Removes feedback loop if callers never extract scale from the matrix.
- Zero extra math; but shifts burden to callers to track true scale externally.
- Error-prone: any user doing `s=scale(pose); set_rotation_and_scale(...,s);` reintroduces drift.

**Where**: teams with strict APIs and code reviews; minimal code change.

### 3) Quantize returned scale

Make `scale(pose)` snap to discrete values (e.g., nearest 1e-4). Each rotation step may perturb scale slightly, but next call snaps it back, breaking the feedback loop.

- Avoids drift while keeping Matrix4x4 API.
- Supports animated scaling because `set_scale()` still accepts arbitrary values; quantization only affects reads.
- Must choose quantization step fine enough for authoring needs; large scales may need geometric quantization.

### 4) Remove systematic bias (hard)

If errors were random, growth would be √N, not N. Bias likely comes from consistent rounding direction in the extraction/reapply math. Eliminating bias would slow growth dramatically, but requires deep FP analysis and is brittle across platforms.

## Decision Guide

- Want correctness + clarity? **Separate rotation and scale**.
- Need minimal refactor? **Set rotation+scale together** but enforce via API/lints.
- Stuck with matrices? **Quantize reads** for a pragmatic fix.
- Enthusiastic about numerics? Explore **bias removal**, but expect complexity.

## Key Insights

- Drift is a feedback loop: extract → quantize → reapply → repeat.
- Scale drift is linear because each step is proportional to scale, not accumulated error.
- Splitting data (R, S, T) or quantizing reads breaks the loop with minimal runtime cost.
