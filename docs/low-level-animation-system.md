# BitSquid Low-Level Animation System — Compression, Quantization, and Rationale

The low-level animator has one job: given animation data, return bone poses at time *t*. Everything else (blending, IK, state machines) is high-level. Performance hinges on two principles:

1. **Touch as little memory as possible** — compress curves aggressively without violating visual error budgets.
2. **Touch memory linearly** — evaluation should be streamable and cache-friendly.

## Compression by Curve Fitting

We fit a parametric curve to raw key samples until error < threshold.

### Curve choice: Hermite with implicit derivatives

- Store only original samples: `(t_i, D_i)` for each DOF (position or rotation component).
- For *T* in `[t_i, t_{i+1}]`, let `u = (T - t_i) / (t_{i+1} - t_i)` and evaluate Hermite where tangents are derived from neighbors (no extra storage).

Why this works:
- Few coefficients; compact.
- Uses only original data and scalars in [0,1], making quantization predictable.
- Smooth enough for gameplay visuals; evaluation cost is modest.

Limitations: other splines (Bezier, B-spline, Catmull-Rom) can trade compression vs. continuity; Hermite is a pragmatic middle ground.

### Fitting algorithm (greedy subdivision)

1. Start with endpoints.
2. Compute max error over all segments.
3. Split worst segment at mid-time; insert a new sample.
4. Repeat until max error ≤ threshold.

This is simple, monotonic, and respects discontinuities (insert duplicate time with different values when needed).

## Local vs. Global Space Compression

- **Global**: avoids hierarchical error propagation → can allow higher thresholds, but motion is more complex (harder to compress) and must be converted back for blending.
- **Local (chosen)**: motion simpler per bone; integrates naturally with blending. Slightly more propagation error, but controlled by per-bone thresholds.

## Quantization

Goal: fit in 16-byte aligned, streamable chunks while keeping visible error below tolerance.

- **Vector3**: 16 bits/component over [-10 m, 10 m] → ≈0.3 mm resolution.
- **Quaternion**:
  - Store index of largest component in 2 bits.
  - Store remaining three components with 10 bits each, quantized in (-1/√2, 1/√2).
  - Reconstruct largest component from unit-length constraint.
  - Precision ≈ 0.0014 per stored component.

Bit cost per sample (including time):

```
Vector3 point : 48 bits (data) + 16 bits (time)
Quaternion     : 32 bits (data) + 16 bits (time)
```

## Why It Performs

- Minimal data: fewer cache lines per pose.
- Linear access: per-track streams support prefetch/DMA and gzip/IO streaming.

## Known Tradeoffs

- Hermite with implicit tangents may underperform specialized schemes for specific motions.
- Local-space compression introduces hierarchy error; choose per-bone thresholds carefully.
- Quaternion scheme assumes unit length and rejects extreme values; validate inputs.

