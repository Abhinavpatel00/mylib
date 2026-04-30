# Noise Math Visual Guide (C99 Library Companion)

This document explains the math and architecture behind [mu/mu_noise_math.h](../mu_noise_math.h).

## 1) Architecture at a Glance

Pipeline philosophy:

1. Hash integer lattice coordinates into reproducible pseudorandom values.
2. Build continuous noise by interpolating or summing local contributions.
3. Compose base noises into fractals and warped domains.
4. Convert scalar noise into vector fields when needed (curl).

Visual layer stack:

```text
hash/white
   |
   +--> value noise
   |
   +--> gradient noise (Perlin)
   |
   +--> simplex noise
   |
   +--> worley/cellular
           |
           +--> fractal combinations (fBm/turbulence/billow/ridged)
                     |
                     +--> domain warping
                     |
                     +--> curl fields
```

---

## 2) Hashing: Discrete Randomness

Noise starts from integer coordinates $\mathbf{i} \in \mathbb{Z}^n$.

We need a deterministic map:

$$
H: \mathbb{Z}^n \times \text{seed} \rightarrow [0, 2^{32})
$$

Then convert to float:

$$
u = \frac{H \gg 8}{2^{24}} \in [0,1), \quad \nu_s = 2\nu-1 \in [-1,1)
$$

Why this matters:

- Determinism: same seed and coordinates always return same value.
- No large tables needed.
- Spatial coherence is added later by interpolation/contribution kernels.

---

## 3) White Noise

White noise is uncorrelated per integer lattice site.

1D:

$$
N(x) = \text{hash}(\lfloor x \rfloor)
$$

2D / 3D: same idea using integer tuples.

Visual:

```text
x-axis cells:
| c0 | c1 | c2 | c3 |
  r0   r1   r2   r3

Each cell gets an unrelated random value.
```

Use when you need:

- random IDs,
- dithering,
- stochastic thresholds,
- seeds for other generators.

---

## 4) Value Noise

Value noise stores random corner values and interpolates smoothly.

2D formula over one cell:

$$
\begin{aligned}
&u = f(t_x),\quad v=f(t_y) \\
&a = \operatorname{lerp}(v_{00}, v_{10}, u) \\
&b = \operatorname{lerp}(v_{01}, v_{11}, u) \\
&N = \operatorname{lerp}(a,b,v)
\end{aligned}
$$

where $f$ is smoothstep/quintic fade.

Visual:

```text
v00 -------- v10
 |            |
 |      p     |   p blends corner values with smooth weights
 |            |
v01 -------- v11
```

Pros:

- cheap,
- smooth,
- easy to reason about.

Cons:

- can look "blobby" compared to gradient/simplex.

---

## 5) Fade Curves (Smooth Interpolation)

Library uses quintic smootherstep:

$$
f(t)=6t^5-15t^4+10t^3
$$

Boundary behavior:

$$
f(0)=0,\ f(1)=1,\ f'(0)=f'(1)=0,\ f''(0)=f''(1)=0
$$

Interpretation:

- zero slope at cell boundaries removes hard derivative jumps,
- zero second derivative further reduces shading artifacts.

---

## 6) Gradient (Perlin) Noise

Instead of random corner values, corners store random gradients $\mathbf{g}_i$.

Corner contribution:

$$
C_i = \mathbf{g}_i \cdot (\mathbf{p}-\mathbf{x}_i)
$$

Then smoothly interpolate all corner contributions.

Visual intuition:

```text
corner gradient arrows:
   ↗        ↙
   o--------o
   |   p    |
   o--------o
   ↘        ↖

Each arrow says how value rises/falls near that corner.
```

Why it looks better than value noise:

- local linear behavior around corners,
- reduced axis-aligned artifacts,
- better control of continuity.

---

## 7) Simplex Noise

Simplex replaces square/cube cells with simplices:

- 2D simplex: triangle,
- 3D simplex: tetrahedron.

### 7.1 2D skew/unskew

Skew transform:

$$
F_2 = \frac{\sqrt{3}-1}{2},\quad s=(x+y)F_2
$$

Unskew:

$$
G_2=\frac{3-\sqrt{3}}{6}
$$

Contribution kernel from a corner:

$$
 t = r^2 - ||\Delta||^2,
\quad n = \begin{cases}
 t^4(\mathbf{g}\cdot\Delta), & t > 0 \\
 0, & t\le 0
\end{cases}
$$

Visual:

```text
square grid  --skew-->  triangle grid

point p touches only 3 corners in 2D
(instead of 4 in value/perlin grid cell)
```

Benefits:

- less directional grid bias,
- fewer corners per evaluation in higher dimensions.

---

## 8) Worley / Cellular Noise

Each integer cell contains a random feature point.

For sample $\mathbf{p}$, compute sorted distances:

$$
F_1 = \min_i d(\mathbf{p},\mathbf{f}_i),\quad
F_2 = \text{2nd-min}_i\ d(\mathbf{p},\mathbf{f}_i)
$$

Common metrics:

- Euclidean: $\sqrt{dx^2+dy^2(+dz^2)}$
- Manhattan: $|dx|+|dy|(+|dz|)$
- Chebyshev: $\max(|dx|,|dy|,(|dz|))$

Visual:

```text
feature points (*) create cells by nearest distance

      *        |
   .       .   | boundary where nearest feature changes
-------------- +
 .      p    . |
      *       .|
```

Useful derived patterns:

- cell interior: $F_1$,
- edge emphasis: $F_2-F_1$,
- crack-like masks: threshold of $(F_2-F_1)$.

---

## 9) Fractal Compositions

Base noise alone is single-scale. Natural detail is multi-scale.

### 9.1 fBm

$$
\text{fBm}(\mathbf{p}) = \sum_{i=0}^{k-1} a_i N(f_i\mathbf{p})
$$

with

$$
f_{i+1}=f_i\cdot\lambda,\quad a_{i+1}=a_i\cdot g
$$

- $\lambda$ = lacunarity (>1, often 2)
- $g$ = gain (<1, often 0.5)

### 9.2 Turbulence

Same as fBm but uses absolute value:

$$
\sum a_i |N(f_i\mathbf{p})|
$$

### 9.3 Billow

$$
\sum a_i (2|N|-1)
$$

Gives rounded cloud-like masses.

### 9.4 Ridged

$$
s_i = (o - |N|)^2,
\quad s_i \leftarrow s_i\cdot w_i,
\quad w_{i+1}=\operatorname{clamp}(s_i\cdot g,0,1)
$$

Produces crisp mountain ridges.

---

## 10) Domain Warping

Warp coordinates before final sample:

$$
\mathbf{p}' = \mathbf{p} + A\,\mathbf{W}(f_w\mathbf{p})
$$

Then evaluate base noise:

$$
N_{warp}(\mathbf{p}) = N(\mathbf{p}')
$$

Visual:

```text
regular coordinate grid:
+---+---+---+
|   |   |   |
+---+---+---+

warped grid:
~\__/~\___/~~
  /~~\_/~~\

Sampling on warped coordinates bends iso-lines and breaks repetition.
```

---

## 11) Curl Noise (Flow Fields)

### 11.1 2D scalar potential

Given $\Psi(x,y)$:

$$
\mathbf{v}=(\partial\Psi/\partial y, -\partial\Psi/\partial x)
$$

This guarantees zero divergence:

$$
\nabla\cdot\mathbf{v}=0
$$

so flow is incompressible.

### 11.2 3D vector potential

Given $\mathbf{A}=(A_x,A_y,A_z)$:

$$
\nabla\times\mathbf{A} = \left(
\frac{\partial A_z}{\partial y}-\frac{\partial A_y}{\partial z},
\frac{\partial A_x}{\partial z}-\frac{\partial A_z}{\partial x},
\frac{\partial A_y}{\partial x}-\frac{\partial A_x}{\partial y}
\right)
$$

Library approximates derivatives using central differences:

$$
\frac{\partial f}{\partial x}\approx\frac{f(x+\epsilon)-f(x-\epsilon)}{2\epsilon}
$$

---

## 12) Practical Presets

Good defaults:

- Terrain macro shape: simplex fBm, 4-6 octaves.
- Mountain ridges: ridged Perlin/simplex mixed into macro shape.
- Clouds/smoke: billow or turbulence.
- Organic stone/wood veins: domain-warped simplex.
- Cellular patterns: Worley with $F_2-F_1$.

Suggested parameter ranges:

- octaves: 3 to 8
- lacunarity: 1.8 to 2.3
- gain: 0.35 to 0.65
- warp_amp: 0.1 to 1.5 (scale-dependent)
- warp_freq: usually same or slightly lower than base freq

---

## 13) Cost and Complexity

Approximate relative cost per sample:

1. White / hash: very low
2. Value: low
3. Perlin: low-medium
4. Simplex: medium
5. Worley (F1/F2): medium-high (neighbor search)
6. Fractal wrappers: base cost multiplied by octaves
7. Domain warp: roughly base + warp cost
8. Curl: multiple base evaluations for finite differences

Rule of thumb:

- If evaluated per-pixel in real-time, keep octaves modest.
- Prebake when possible for static assets.

---

## 14) Mapping to Library API

Primary calls in [mu/mu_noise_math.h](../mu_noise_math.h):

- White: `mu_noise_white1/2/3`
- Value: `mu_noise_value1/2/3`
- Perlin: `mu_noise_perlin1/2/3`
- Simplex: `mu_noise_simplex2/3`
- Worley: `mu_noise_worley2/3`
- Fractals: `mu_noise_fbm*`, `mu_noise_turbulence*`, `mu_noise_billow*`, `mu_noise_ridged*`
- Warp: `mu_noise_domain_warp*`, `mu_noise_domain_warped*`
- Curl: `mu_noise_curl2`, `mu_noise_curl3`

---

## 15) Minimal Sampling Example

```c
#include "mu_noise_math.h"

float sample_height(float x, float y, uint32_t seed) {
    float macro = mu_noise_fbm2(mu_noise_simplex2, x * 0.002f, y * 0.002f, seed, 5, 2.0f, 0.5f);
    float ridge = mu_noise_ridged2(mu_noise_perlin2, x * 0.010f, y * 0.010f, seed + 17u, 4, 2.0f, 2.0f, 1.0f);
    return 0.75f * macro + 0.25f * ridge;
}
```

---

## 16) Validation Checklist

When adding a new noise family:

1. Confirm output range expectation (for example, roughly $[-1,1]$).
2. Verify determinism with fixed seed and integer coordinates.
3. Check continuity class at cell boundaries if interpolation is involved.
4. Evaluate isotropy (avoid directional artifacts if unwanted).
5. Profile octaves and warp/curl variants under real scene load.

---

End of guide.
