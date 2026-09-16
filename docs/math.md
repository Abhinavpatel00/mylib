# Math: noise, geometric algebra, quantization

---

## mu_noise_math.h — procedural noise

Standalone C99 single-header noise library, dependency-free and
data-oriented. Every family works in 1D/2D/3D unless noted, all take a
`uint32_t seed`, and gradient/value outputs are roughly `[-1, 1]` (Worley
distances are `[0, ~sqrt(3)]`).

### Included families

1. White / hash noise — uncorrelated random per lattice cell
2. Value noise — interpolated lattice scalars
3. Gradient (Perlin) noise — interpolated corner gradients
4. Simplex noise — triangular/simplex cells, fewer directional artifacts
5. Worley / cellular — F1/F2 distance to per-cell feature points
6. Fractal compositions — fBm, turbulence, billow, ridged
7. Domain warping — warp coordinates before sampling
8. Curl noise — divergence-free 2D flow from a scalar potential
9. OpenSimplex-inspired — rotation-based simplex variants
10. Periodic wrappers — tileable value/perlin/simplex

### Visual quick map

```
White noise:   integer cell -> hash -> random scalar

Value noise:    c00 ----- c10
                 |         |
                 |    p    |     p blended by smooth interpolation
                 |         |
                c01 ----- c11

Perlin:         each corner holds a pseudo-random gradient g
                value = dot(g, p - corner), smoothly interpolated

Simplex 2D:     skew square grid into equilateral triangles
                evaluate only 3 triangle corners

Worley:         each cell holds a feature point f
                F1 = nearest distance, F2 = second nearest

fBm:            sum over octaves: amp_i * noise(freq_i * p)

Domain warp:    p' = p + warp_amp * W(freq * p); value = N(p')

Curl (2D):      v = ( dPsi/dy, -dPsi/dx )   -> div(v) = 0
```

### Usage

```c
float n  = mu_noise_simplex2(12.3f, 9.1f, 1337u);
float fb = mu_noise_fbm2(mu_noise_perlin2, 12.3f, 9.1f, 1337u, 6, 2.0f, 0.5f);
float p  = mu_noise_perlin2_periodic(12.3f, 9.1f, 128, 128, 1337u);
float os = mu_noise_opensimplex2_2d(12.3f, 9.1f, 1337u);

mu_noise_worley2_result w =
    mu_noise_worley2(12.3f, 9.1f, 1337u, MU_NOISE_DIST_EUCLIDEAN);
/* w.f1, w.f2 distances; w.cell_id1/2 identify the two nearest cells */
```

Distance metrics: `MU_NOISE_DIST_EUCLIDEAN`, `MU_NOISE_DIST_MANHATTAN`,
`MU_NOISE_DIST_CHEBYSHEV`.

### Naming pattern

```
mu_noise_<family><dim>[_periodic](coords..., seed[, period_x, ...])
```

Periods are in lattice cells; `period <= 0` on an axis falls back to
non-periodic hashing on that axis. Tileable simplex is achieved by
domain-wrapped blending of four (or eight) shifted samples rather than
natively periodic simplex.

### Hashing internals

All noise derives from one integer avalanche hash (`mu_noise_hash_u32`,
two multiply-xorshift rounds), combined over coordinates and seed via
`mu_noise_hash_combine`. Lattice scalars map through
`mu_noise_hash_to_unit_float` (24-bit mantissa-friendly `[0,1)`) or to
signed `[-1, 1]`.

Interpolation uses the quintic smootherstep (`6t^5 - 15t^4 + 10t^3`) —
zero first and second derivatives at cell boundaries, which visibly kills
grid seams compared to plain smoothstep.

### Choosing a family

| Need | Use |
|---|---|
| Fast uncorrelated jitter | `mu_noise_white*` |
| Cheap smooth blobs | `mu_noise_value*` |
| Classic terrain texture | `mu_noise_perlin*` |
| No grid-aligned artifacts | `mu_noise_simplex*` / opensimplex |
| Cells/veins/cracks | `mu_noise_worley*` (`F2-F1` for borders) |
| Flow fields (fluid/smoke look) | curl noise |
| Seamless tiles | `*_periodic` variants |

---

## mu_geometric_algebra.h — PGA / conformal toolkit

Standalone C99 single-header geometric algebra. **Requires cglm** for the
vector backend (found via `__has_include`, or point `MU_GA_CGLM_HEADER` at
your umbrella header).

Design target:

- Rigid 2D and 3D PGA primitives (flat points, lines, planes)
- 2D/3D **motors** (dual-quaternion-style rigid motion) and **flectors**
  (reflection operators)
- Conformal 2D and 3D primitives (round points, dipoles, circles, spheres)
- Join (wedge) and meet (antiwedge) constructions in Terathon style

### Basis layout (types and their coordinate meaning)

```
Rigid 2D (PGA-like)
    FlatPoint2: p = x e1 + y e2 + z e3
    Line2:      g = x e23 + y e31 + z e12
    Join: p ^ q -> line        Meet: g ^ h -> flat point

Rigid 3D (PGA-like)
    FlatPoint3: p = x e1 + y e2 + z e3 + w e4
    Line3:      l = v_x e41 + v_y e42 + v_z e43 + m_x e23 + m_y e31 + m_z e12
    Plane3:     g = x e234 + y e314 + z e124 + w e321
    Join: p ^ q -> line, line ^ p -> plane
    Meet: g ^ h -> line, g ^ line -> point

Conformal 2D
    RoundPoint2: (x,y,z,w)      Dipole2: line g + flat point p
    Circle2:     (w,x,y,z)

Conformal 3D
    RoundPoint3: (x,y,z,w,u)    Dipole3: (v,m,p)
    Circle3:     (g,v,m)        Sphere3: (u,x,y,z,w)
```

The header is organized into sections: scalar helpers, rigid 2D, rigid 3D,
motor/flector 2D, motor/flector 3D, conformal 2D, conformal 3D, and tiny
usage examples at the bottom. It is intentionally verbose — the math lives
in comments next to the code.

### When this beats matrices

- One type (motor) represents rotation + translation with clean
  composition and interpolation (screw motion / dual-lerp), no gimbal
  worries, no matrix-vs-quaternion split brain.
- Join/meet answer "line through two points", "intersection of two planes",
  "plane through point and line" as single products instead of hand-derived
  cross/dot recipes.
- Reflections are first-class (flectors): mirror transforms compose like any
  other motion.

---

## mu_bitpacking.h — float quantization

Utilities for shrinking float data for rendering: vertex packing, texture
coordinates, normals, GPU bandwidth reduction. All `MU_INLINE`.

### Float bit reinterpretation

```c
typedef union { float f; uint32_t u; } mu_float_bits;
```

Casting `float` to `uint32_t` converts the value; this union (or `memcpy`)
reads the actual IEEE 754 bit pattern — required because quantization
manipulates exponent/mantissa bits directly.

```
float layout:   [ sign | exponent | mantissa ]
                   1        8          23
half layout:    [ sign | exp | mantissa ]
                   1      5       10
```

### API

| Function | Direction | Notes |
|---|---|---|
| `mu_quantize_half(v)` | f32 → f16 | exponent bias adjust (127→15); underflow→0, overflow→inf `0x7c00`, NaN→`0x7e00` |
| `mu_dequantize_half(h)` | f16 → f32 | denormals flushed to zero; inf/NaN preserved |
| `mu_quantize_float(v, n)` | f32 → f32 | keep `n` mantissa bits (0..23) with round-to-nearest; leaves inf/NaN untouched, flushes denormals |
| `mu_quantize_unorm(v, n)` | `[0,1]` → int | n-bit unsigned with rounding; clamps input |
| `mu_dequantize_unorm(v, n)` | int → `[0,1]` | inverse |
| `mu_quantize_snorm(v, n)` | `[-1,1]` → int | n-bit signed, symmetric scale `(1<<(n-1))-1`, sign-aware rounding; clamps input |
| `mu_dequantize_snorm(v, n)` | int → `[-1,1]` | inverse |

### Typical use

```c
/* pack a normal into 10 bits per component */
uint32_t qx = (uint32_t)mu_quantize_snorm(nx, 10);
uint32_t qy = (uint32_t)mu_quantize_snorm(ny, 10);
uint32_t qz = (uint32_t)mu_quantize_snorm(nz, 10);

/* or shrink a float to 10 mantissa bits before storing */
float reduced = mu_quantize_float(1.234567f, 10);
```
