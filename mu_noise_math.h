#ifndef MU_NOISE_MATH_H
#define MU_NOISE_MATH_H

/*
    mu_noise_math.h
    ---------------------------------------------------------------------------
    Standalone C99 single-header procedural noise library.

    Goal:
    - Provide practical "families" of noise used in games/tools.
    - Keep implementation dependency-free and data-oriented.
    - Explain the math visually and directly in code comments.

    Included noise families:
    1) White / hash noise      (discrete random values per lattice cell)
    2) Value noise             (interpolated lattice values)
    3) Gradient Perlin noise   (interpolated corner gradients)
    4) Simplex noise           (lower directional artifacts than grid noise)
    5) Worley / Cellular noise (distance to random feature points)
    6) Fractal compositions    (fBm, turbulence, billow, ridged)
    7) Domain warping          (warp coordinates before sampling)
    8) Curl noise              (divergence-free flow field from scalar/vector potential)
    9) OpenSimplex-inspired    (rotation-based simplex variants)
   10) Periodic wrappers       (tileable value/perlin/simplex)

    ---------------------------------------------------------------------------
    VISUAL QUICK MAP
    ---------------------------------------------------------------------------

      White noise:
        integer cell -> hash -> random scalar

      Value noise:
         c00 ----- c10
          |         |
          |   p     |    p is blended by smooth interpolation
          |         |
         c01 ----- c11

      Perlin noise:
         each corner stores a pseudo-random gradient direction g
         value at p is dot(g, p-corner), then smoothly interpolated

      Simplex 2D:
        skew square grid into equilateral triangles
        evaluate 3 triangle corners only

      Worley:
        each cell has feature point f
        return F1 (nearest dist), F2 (2nd nearest dist)

      fBm:
        sum_{octaves} amp_i * noise(freq_i * p)

      Domain warp:
        p' = p + warp_amp * W(freq * p)
        value = N(p')

      Curl (2D):
        v = ( dPsi/dy, -dPsi/dx )
        so div(v) = 0 (incompressible flow)

    ---------------------------------------------------------------------------
    Usage Example
    ---------------------------------------------------------------------------

      float n  = mu_noise_simplex2(12.3f, 9.1f, 1337u);
      float fb = mu_noise_fbm2(mu_noise_perlin2, 12.3f, 9.1f, 1337u, 6, 2.0f, 0.5f);
      mu_noise_worley2_result w = mu_noise_worley2(12.3f, 9.1f, 1337u, MU_NOISE_DIST_EUCLIDEAN);
    float p  = mu_noise_perlin2_periodic(12.3f, 9.1f, 128, 128, 1337u);
    float os = mu_noise_opensimplex2_2d(12.3f, 9.1f, 1337u);

    ---------------------------------------------------------------------------
*/

#include <stdint.h>
#include <math.h>
#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MU_NOISE_INLINE
#define MU_NOISE_INLINE static inline
#endif

#ifndef MU_NOISE_PI
#define MU_NOISE_PI 3.14159265358979323846f
#endif

/* ------------------------------------------------------------------------- */
/* Basic structs and enums                                                   */
/* ------------------------------------------------------------------------- */

typedef struct mu_noise_vec2 {
    float x;
    float y;
} mu_noise_vec2;

typedef struct mu_noise_vec3 {
    float x;
    float y;
    float z;
} mu_noise_vec3;

typedef enum mu_noise_distance_metric {
    MU_NOISE_DIST_EUCLIDEAN = 0,
    MU_NOISE_DIST_MANHATTAN = 1,
    MU_NOISE_DIST_CHEBYSHEV = 2
} mu_noise_distance_metric;

typedef struct mu_noise_worley2_result {
    float f1;
    float f2;
    int cell_id1;
    int cell_id2;
} mu_noise_worley2_result;

typedef struct mu_noise_worley3_result {
    float f1;
    float f2;
    int cell_id1;
    int cell_id2;
} mu_noise_worley3_result;

typedef float (*mu_noise_fn1)(float x, uint32_t seed);
typedef float (*mu_noise_fn2)(float x, float y, uint32_t seed);
typedef float (*mu_noise_fn3)(float x, float y, float z, uint32_t seed);

/* ------------------------------------------------------------------------- */
/* Scalar/vector helpers                                                     */
/* ------------------------------------------------------------------------- */

MU_NOISE_INLINE float mu_noise_abs(float x) { return fabsf(x); }
MU_NOISE_INLINE float mu_noise_floor(float x) { return floorf(x); }
MU_NOISE_INLINE float mu_noise_sqrt(float x) { return sqrtf(x); }

MU_NOISE_INLINE float mu_noise_clamp(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

MU_NOISE_INLINE float mu_noise_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

MU_NOISE_INLINE float mu_noise_fract(float x)
{
    return x - floorf(x);
}

MU_NOISE_INLINE int mu_noise_mod_i(int x, int period)
{
    if (period <= 0) return x;
    {
        int m = x % period;
        return (m < 0) ? (m + period) : m;
    }
}

MU_NOISE_INLINE float mu_noise_wrap_period(float x, float period)
{
    if (period <= 0.0f) return x;
    {
        float m = fmodf(x, period);
        return (m < 0.0f) ? (m + period) : m;
    }
}

MU_NOISE_INLINE float mu_noise_smoothstep3(float t)
{
    /* cubic smoothstep: 3t^2 - 2t^3 */
    return t * t * (3.0f - 2.0f * t);
}

MU_NOISE_INLINE float mu_noise_smootherstep5(float t)
{
    /*
        quintic smoothstep: 6t^5 - 15t^4 + 10t^3

        Why quintic for gradient/simplex interpolation?
        - first derivative at boundaries is 0
        - second derivative at boundaries is 0
        This reduces visible grid seams.
    */
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

MU_NOISE_INLINE float mu_noise_remap_neg1_pos1_to_0_1(float v)
{
    return 0.5f * v + 0.5f;
}

MU_NOISE_INLINE mu_noise_vec2 mu_noise_v2(float x, float y)
{
    mu_noise_vec2 r;
    r.x = x;
    r.y = y;
    return r;
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_v3(float x, float y, float z)
{
    mu_noise_vec3 r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}

MU_NOISE_INLINE float mu_noise_v2_dot(mu_noise_vec2 a, mu_noise_vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

MU_NOISE_INLINE float mu_noise_v3_dot(mu_noise_vec3 a, mu_noise_vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

MU_NOISE_INLINE float mu_noise_v2_len(mu_noise_vec2 a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

MU_NOISE_INLINE float mu_noise_v3_len(mu_noise_vec3 a)
{
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

MU_NOISE_INLINE mu_noise_vec2 mu_noise_v2_add(mu_noise_vec2 a, mu_noise_vec2 b)
{
    return mu_noise_v2(a.x + b.x, a.y + b.y);
}

MU_NOISE_INLINE mu_noise_vec2 mu_noise_v2_mul(mu_noise_vec2 a, float s)
{
    return mu_noise_v2(a.x * s, a.y * s);
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_v3_add(mu_noise_vec3 a, mu_noise_vec3 b)
{
    return mu_noise_v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_v3_sub(mu_noise_vec3 a, mu_noise_vec3 b)
{
    return mu_noise_v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_v3_mul(mu_noise_vec3 a, float s)
{
    return mu_noise_v3(a.x * s, a.y * s, a.z * s);
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_v3_cross(mu_noise_vec3 a, mu_noise_vec3 b)
{
    return mu_noise_v3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

/* ------------------------------------------------------------------------- */
/* Hashes and randoms                                                        */
/* ------------------------------------------------------------------------- */

MU_NOISE_INLINE uint32_t mu_noise_hash_u32(uint32_t x)
{
    /* A small high-quality integer avalanche hash. */
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

MU_NOISE_INLINE uint32_t mu_noise_hash_combine(uint32_t h, uint32_t k)
{
    h ^= mu_noise_hash_u32(k + 0x9e3779b9U + (h << 6) + (h >> 2));
    return h;
}

MU_NOISE_INLINE uint32_t mu_noise_hash_1i(int x, uint32_t seed)
{
    uint32_t h = mu_noise_hash_u32(seed ^ 0xA1B3C6D9U);
    h = mu_noise_hash_combine(h, (uint32_t)x);
    return mu_noise_hash_u32(h);
}

MU_NOISE_INLINE uint32_t mu_noise_hash_2i(int x, int y, uint32_t seed)
{
    uint32_t h = mu_noise_hash_u32(seed ^ 0xD1B54A35U);
    h = mu_noise_hash_combine(h, (uint32_t)x);
    h = mu_noise_hash_combine(h, (uint32_t)y);
    return mu_noise_hash_u32(h);
}

MU_NOISE_INLINE uint32_t mu_noise_hash_3i(int x, int y, int z, uint32_t seed)
{
    uint32_t h = mu_noise_hash_u32(seed ^ 0x94D049BBU);
    h = mu_noise_hash_combine(h, (uint32_t)x);
    h = mu_noise_hash_combine(h, (uint32_t)y);
    h = mu_noise_hash_combine(h, (uint32_t)z);
    return mu_noise_hash_u32(h);
}

MU_NOISE_INLINE uint32_t mu_noise_hash_1i_periodic(int x, int period_x, uint32_t seed)
{
    return mu_noise_hash_1i(mu_noise_mod_i(x, period_x), seed);
}

MU_NOISE_INLINE uint32_t mu_noise_hash_2i_periodic(int x, int y, int period_x, int period_y, uint32_t seed)
{
    return mu_noise_hash_2i(mu_noise_mod_i(x, period_x), mu_noise_mod_i(y, period_y), seed);
}

MU_NOISE_INLINE uint32_t mu_noise_hash_3i_periodic(int x, int y, int z, int period_x, int period_y, int period_z, uint32_t seed)
{
    return mu_noise_hash_3i(
        mu_noise_mod_i(x, period_x),
        mu_noise_mod_i(y, period_y),
        mu_noise_mod_i(z, period_z),
        seed
    );
}

MU_NOISE_INLINE float mu_noise_hash_to_unit_float(uint32_t h)
{
    /* 24-bit mantissa-friendly mapping [0, 1). */
    return (float)(h >> 8) * (1.0f / 16777216.0f);
}

MU_NOISE_INLINE float mu_noise_hash_to_signed_float(uint32_t h)
{
    return mu_noise_hash_to_unit_float(h) * 2.0f - 1.0f;
}

/* ------------------------------------------------------------------------- */
/* 1) White noise                                                            */
/* ------------------------------------------------------------------------- */

/*
    White noise is "uncorrelated" between integer cells.

    N(i) = hash(i)

    Sampling at continuous x,y,z usually means taking floor first.
*/
MU_NOISE_INLINE float mu_noise_white1(float x, uint32_t seed)
{
    int ix = (int)floorf(x);
    return mu_noise_hash_to_signed_float(mu_noise_hash_1i(ix, seed));
}

MU_NOISE_INLINE float mu_noise_white2(float x, float y, uint32_t seed)
{
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    return mu_noise_hash_to_signed_float(mu_noise_hash_2i(ix, iy, seed));
}

MU_NOISE_INLINE float mu_noise_white3(float x, float y, float z, uint32_t seed)
{
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    int iz = (int)floorf(z);
    return mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix, iy, iz, seed));
}

/* ------------------------------------------------------------------------- */
/* 2) Value noise                                                            */
/* ------------------------------------------------------------------------- */

/*
    Value noise stores random scalar values at lattice corners and blends.

    1D:
      i0 = floor(x), i1=i0+1, t=fract(x)
      N(x) = lerp(v(i0), v(i1), fade(t))

    2D:
        v00 ---- v10
         |        |
         |   p    |    bilinear blend with smooth t_x,t_y
         |        |
        v01 ---- v11

    3D:
      same pattern with trilinear blend over 8 corners.
*/
MU_NOISE_INLINE float mu_noise_value1(float x, uint32_t seed)
{
    int i0 = (int)floorf(x);
    int i1 = i0 + 1;
    float t = x - (float)i0;
    float u = mu_noise_smootherstep5(t);

    float v0 = mu_noise_hash_to_signed_float(mu_noise_hash_1i(i0, seed));
    float v1 = mu_noise_hash_to_signed_float(mu_noise_hash_1i(i1, seed));

    return mu_noise_lerp(v0, v1, u);
}

MU_NOISE_INLINE float mu_noise_value2(float x, float y, uint32_t seed)
{
    int ix0 = (int)floorf(x);
    int iy0 = (int)floorf(y);
    int ix1 = ix0 + 1;
    int iy1 = iy0 + 1;

    float tx = x - (float)ix0;
    float ty = y - (float)iy0;
    float ux = mu_noise_smootherstep5(tx);
    float uy = mu_noise_smootherstep5(ty);

    float v00 = mu_noise_hash_to_signed_float(mu_noise_hash_2i(ix0, iy0, seed));
    float v10 = mu_noise_hash_to_signed_float(mu_noise_hash_2i(ix1, iy0, seed));
    float v01 = mu_noise_hash_to_signed_float(mu_noise_hash_2i(ix0, iy1, seed));
    float v11 = mu_noise_hash_to_signed_float(mu_noise_hash_2i(ix1, iy1, seed));

    float a = mu_noise_lerp(v00, v10, ux);
    float b = mu_noise_lerp(v01, v11, ux);
    return mu_noise_lerp(a, b, uy);
}

MU_NOISE_INLINE float mu_noise_value3(float x, float y, float z, uint32_t seed)
{
    int ix0 = (int)floorf(x);
    int iy0 = (int)floorf(y);
    int iz0 = (int)floorf(z);
    int ix1 = ix0 + 1;
    int iy1 = iy0 + 1;
    int iz1 = iz0 + 1;

    float tx = x - (float)ix0;
    float ty = y - (float)iy0;
    float tz = z - (float)iz0;

    float ux = mu_noise_smootherstep5(tx);
    float uy = mu_noise_smootherstep5(ty);
    float uz = mu_noise_smootherstep5(tz);

    float c000 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix0, iy0, iz0, seed));
    float c100 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix1, iy0, iz0, seed));
    float c010 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix0, iy1, iz0, seed));
    float c110 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix1, iy1, iz0, seed));
    float c001 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix0, iy0, iz1, seed));
    float c101 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix1, iy0, iz1, seed));
    float c011 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix0, iy1, iz1, seed));
    float c111 = mu_noise_hash_to_signed_float(mu_noise_hash_3i(ix1, iy1, iz1, seed));

    float x00 = mu_noise_lerp(c000, c100, ux);
    float x10 = mu_noise_lerp(c010, c110, ux);
    float x01 = mu_noise_lerp(c001, c101, ux);
    float x11 = mu_noise_lerp(c011, c111, ux);

    float y0 = mu_noise_lerp(x00, x10, uy);
    float y1 = mu_noise_lerp(x01, x11, uy);

    return mu_noise_lerp(y0, y1, uz);
}

/*
    Periodic/tileable value noise:
    - periods are in lattice cells.
    - period<=0 on any axis falls back to non-periodic hashing on that axis.
*/
MU_NOISE_INLINE float mu_noise_value1_periodic(float x, int period_x, uint32_t seed)
{
    if (period_x <= 0) return mu_noise_value1(x, seed);

    {
        int i0 = (int)floorf(x);
        int i1 = i0 + 1;
        float t = x - (float)i0;
        float u = mu_noise_smootherstep5(t);

        float v0 = mu_noise_hash_to_signed_float(mu_noise_hash_1i_periodic(i0, period_x, seed));
        float v1 = mu_noise_hash_to_signed_float(mu_noise_hash_1i_periodic(i1, period_x, seed));
        return mu_noise_lerp(v0, v1, u);
    }
}

MU_NOISE_INLINE float mu_noise_value2_periodic(float x, float y, int period_x, int period_y, uint32_t seed)
{
    if (period_x <= 0 || period_y <= 0) return mu_noise_value2(x, y, seed);

    {
        int ix0 = (int)floorf(x);
        int iy0 = (int)floorf(y);
        int ix1 = ix0 + 1;
        int iy1 = iy0 + 1;

        float tx = x - (float)ix0;
        float ty = y - (float)iy0;
        float ux = mu_noise_smootherstep5(tx);
        float uy = mu_noise_smootherstep5(ty);

        float v00 = mu_noise_hash_to_signed_float(mu_noise_hash_2i_periodic(ix0, iy0, period_x, period_y, seed));
        float v10 = mu_noise_hash_to_signed_float(mu_noise_hash_2i_periodic(ix1, iy0, period_x, period_y, seed));
        float v01 = mu_noise_hash_to_signed_float(mu_noise_hash_2i_periodic(ix0, iy1, period_x, period_y, seed));
        float v11 = mu_noise_hash_to_signed_float(mu_noise_hash_2i_periodic(ix1, iy1, period_x, period_y, seed));

        {
            float a = mu_noise_lerp(v00, v10, ux);
            float b = mu_noise_lerp(v01, v11, ux);
            return mu_noise_lerp(a, b, uy);
        }
    }
}

MU_NOISE_INLINE float mu_noise_value3_periodic(float x, float y, float z, int period_x, int period_y, int period_z, uint32_t seed)
{
    if (period_x <= 0 || period_y <= 0 || period_z <= 0) return mu_noise_value3(x, y, z, seed);

    {
        int ix0 = (int)floorf(x);
        int iy0 = (int)floorf(y);
        int iz0 = (int)floorf(z);
        int ix1 = ix0 + 1;
        int iy1 = iy0 + 1;
        int iz1 = iz0 + 1;

        float tx = x - (float)ix0;
        float ty = y - (float)iy0;
        float tz = z - (float)iz0;
        float ux = mu_noise_smootherstep5(tx);
        float uy = mu_noise_smootherstep5(ty);
        float uz = mu_noise_smootherstep5(tz);

        float c000 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix0, iy0, iz0, period_x, period_y, period_z, seed));
        float c100 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix1, iy0, iz0, period_x, period_y, period_z, seed));
        float c010 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix0, iy1, iz0, period_x, period_y, period_z, seed));
        float c110 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix1, iy1, iz0, period_x, period_y, period_z, seed));
        float c001 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix0, iy0, iz1, period_x, period_y, period_z, seed));
        float c101 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix1, iy0, iz1, period_x, period_y, period_z, seed));
        float c011 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix0, iy1, iz1, period_x, period_y, period_z, seed));
        float c111 = mu_noise_hash_to_signed_float(mu_noise_hash_3i_periodic(ix1, iy1, iz1, period_x, period_y, period_z, seed));

        {
            float x00 = mu_noise_lerp(c000, c100, ux);
            float x10 = mu_noise_lerp(c010, c110, ux);
            float x01 = mu_noise_lerp(c001, c101, ux);
            float x11 = mu_noise_lerp(c011, c111, ux);
            float y0 = mu_noise_lerp(x00, x10, uy);
            float y1 = mu_noise_lerp(x01, x11, uy);
            return mu_noise_lerp(y0, y1, uz);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 3) Gradient Perlin noise                                                  */
/* ------------------------------------------------------------------------- */

/*
    Perlin idea:
      - At each lattice corner, choose pseudo-random gradient vector g.
      - Compute local offset d = p - corner.
      - Corner contribution is dot(g, d).
      - Smoothly blend corner contributions.

    Why dot(g,d)?
      It creates directional variation while preserving continuity.
*/

MU_NOISE_INLINE mu_noise_vec2 mu_noise_grad2_from_hash(uint32_t h)
{
    /* 8 directions around the unit circle (cheap and isotropic enough). */
    switch (h & 7U)
    {
        case 0U: return mu_noise_v2( 1.0f,  0.0f);
        case 1U: return mu_noise_v2(-1.0f,  0.0f);
        case 2U: return mu_noise_v2( 0.0f,  1.0f);
        case 3U: return mu_noise_v2( 0.0f, -1.0f);
        case 4U: return mu_noise_v2( 0.70710678f,  0.70710678f);
        case 5U: return mu_noise_v2(-0.70710678f,  0.70710678f);
        case 6U: return mu_noise_v2( 0.70710678f, -0.70710678f);
        default: return mu_noise_v2(-0.70710678f, -0.70710678f);
    }
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_grad3_from_hash(uint32_t h)
{
    /* 12 classic Perlin gradients in 3D. */
    switch (h % 12U)
    {
        case 0U:  return mu_noise_v3( 1.0f,  1.0f,  0.0f);
        case 1U:  return mu_noise_v3(-1.0f,  1.0f,  0.0f);
        case 2U:  return mu_noise_v3( 1.0f, -1.0f,  0.0f);
        case 3U:  return mu_noise_v3(-1.0f, -1.0f,  0.0f);
        case 4U:  return mu_noise_v3( 1.0f,  0.0f,  1.0f);
        case 5U:  return mu_noise_v3(-1.0f,  0.0f,  1.0f);
        case 6U:  return mu_noise_v3( 1.0f,  0.0f, -1.0f);
        case 7U:  return mu_noise_v3(-1.0f,  0.0f, -1.0f);
        case 8U:  return mu_noise_v3( 0.0f,  1.0f,  1.0f);
        case 9U:  return mu_noise_v3( 0.0f, -1.0f,  1.0f);
        case 10U: return mu_noise_v3( 0.0f,  1.0f, -1.0f);
        default:  return mu_noise_v3( 0.0f, -1.0f, -1.0f);
    }
}

MU_NOISE_INLINE float mu_noise_perlin1(float x, uint32_t seed)
{
    int i0 = (int)floorf(x);
    int i1 = i0 + 1;

    float t = x - (float)i0;
    float u = mu_noise_smootherstep5(t);

    float g0 = (mu_noise_hash_1i(i0, seed) & 1U) ? 1.0f : -1.0f;
    float g1 = (mu_noise_hash_1i(i1, seed) & 1U) ? 1.0f : -1.0f;

    float n0 = g0 * t;
    float n1 = g1 * (t - 1.0f);

    /* Scale to roughly [-1,1]. */
    return 2.0f * mu_noise_lerp(n0, n1, u);
}

MU_NOISE_INLINE float mu_noise_perlin2(float x, float y, uint32_t seed)
{
    int ix0 = (int)floorf(x);
    int iy0 = (int)floorf(y);
    int ix1 = ix0 + 1;
    int iy1 = iy0 + 1;

    float tx = x - (float)ix0;
    float ty = y - (float)iy0;

    float ux = mu_noise_smootherstep5(tx);
    float uy = mu_noise_smootherstep5(ty);

    mu_noise_vec2 g00 = mu_noise_grad2_from_hash(mu_noise_hash_2i(ix0, iy0, seed));
    mu_noise_vec2 g10 = mu_noise_grad2_from_hash(mu_noise_hash_2i(ix1, iy0, seed));
    mu_noise_vec2 g01 = mu_noise_grad2_from_hash(mu_noise_hash_2i(ix0, iy1, seed));
    mu_noise_vec2 g11 = mu_noise_grad2_from_hash(mu_noise_hash_2i(ix1, iy1, seed));

    float n00 = mu_noise_v2_dot(g00, mu_noise_v2(tx,        ty));
    float n10 = mu_noise_v2_dot(g10, mu_noise_v2(tx - 1.0f, ty));
    float n01 = mu_noise_v2_dot(g01, mu_noise_v2(tx,        ty - 1.0f));
    float n11 = mu_noise_v2_dot(g11, mu_noise_v2(tx - 1.0f, ty - 1.0f));

    float a = mu_noise_lerp(n00, n10, ux);
    float b = mu_noise_lerp(n01, n11, ux);

    /* empirical normalization */
    return 1.41421356f * mu_noise_lerp(a, b, uy);
}

MU_NOISE_INLINE float mu_noise_perlin3(float x, float y, float z, uint32_t seed)
{
    int ix0 = (int)floorf(x);
    int iy0 = (int)floorf(y);
    int iz0 = (int)floorf(z);
    int ix1 = ix0 + 1;
    int iy1 = iy0 + 1;
    int iz1 = iz0 + 1;

    float tx = x - (float)ix0;
    float ty = y - (float)iy0;
    float tz = z - (float)iz0;

    float ux = mu_noise_smootherstep5(tx);
    float uy = mu_noise_smootherstep5(ty);
    float uz = mu_noise_smootherstep5(tz);

    mu_noise_vec3 g000 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix0, iy0, iz0, seed));
    mu_noise_vec3 g100 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix1, iy0, iz0, seed));
    mu_noise_vec3 g010 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix0, iy1, iz0, seed));
    mu_noise_vec3 g110 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix1, iy1, iz0, seed));
    mu_noise_vec3 g001 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix0, iy0, iz1, seed));
    mu_noise_vec3 g101 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix1, iy0, iz1, seed));
    mu_noise_vec3 g011 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix0, iy1, iz1, seed));
    mu_noise_vec3 g111 = mu_noise_grad3_from_hash(mu_noise_hash_3i(ix1, iy1, iz1, seed));

    float n000 = mu_noise_v3_dot(g000, mu_noise_v3(tx,        ty,        tz));
    float n100 = mu_noise_v3_dot(g100, mu_noise_v3(tx - 1.0f, ty,        tz));
    float n010 = mu_noise_v3_dot(g010, mu_noise_v3(tx,        ty - 1.0f, tz));
    float n110 = mu_noise_v3_dot(g110, mu_noise_v3(tx - 1.0f, ty - 1.0f, tz));
    float n001 = mu_noise_v3_dot(g001, mu_noise_v3(tx,        ty,        tz - 1.0f));
    float n101 = mu_noise_v3_dot(g101, mu_noise_v3(tx - 1.0f, ty,        tz - 1.0f));
    float n011 = mu_noise_v3_dot(g011, mu_noise_v3(tx,        ty - 1.0f, tz - 1.0f));
    float n111 = mu_noise_v3_dot(g111, mu_noise_v3(tx - 1.0f, ty - 1.0f, tz - 1.0f));

    float x00 = mu_noise_lerp(n000, n100, ux);
    float x10 = mu_noise_lerp(n010, n110, ux);
    float x01 = mu_noise_lerp(n001, n101, ux);
    float x11 = mu_noise_lerp(n011, n111, ux);

    float y0 = mu_noise_lerp(x00, x10, uy);
    float y1 = mu_noise_lerp(x01, x11, uy);

    return 1.15470054f * mu_noise_lerp(y0, y1, uz);
}

MU_NOISE_INLINE float mu_noise_perlin1_periodic(float x, int period_x, uint32_t seed)
{
    if (period_x <= 0) return mu_noise_perlin1(x, seed);

    {
        int i0 = (int)floorf(x);
        int i1 = i0 + 1;
        float t = x - (float)i0;
        float u = mu_noise_smootherstep5(t);

        float g0 = (mu_noise_hash_1i_periodic(i0, period_x, seed) & 1U) ? 1.0f : -1.0f;
        float g1 = (mu_noise_hash_1i_periodic(i1, period_x, seed) & 1U) ? 1.0f : -1.0f;
        float n0 = g0 * t;
        float n1 = g1 * (t - 1.0f);
        return 2.0f * mu_noise_lerp(n0, n1, u);
    }
}

MU_NOISE_INLINE float mu_noise_perlin2_periodic(float x, float y, int period_x, int period_y, uint32_t seed)
{
    if (period_x <= 0 || period_y <= 0) return mu_noise_perlin2(x, y, seed);

    {
        int ix0 = (int)floorf(x);
        int iy0 = (int)floorf(y);
        int ix1 = ix0 + 1;
        int iy1 = iy0 + 1;

        float tx = x - (float)ix0;
        float ty = y - (float)iy0;
        float ux = mu_noise_smootherstep5(tx);
        float uy = mu_noise_smootherstep5(ty);

        mu_noise_vec2 g00 = mu_noise_grad2_from_hash(mu_noise_hash_2i_periodic(ix0, iy0, period_x, period_y, seed));
        mu_noise_vec2 g10 = mu_noise_grad2_from_hash(mu_noise_hash_2i_periodic(ix1, iy0, period_x, period_y, seed));
        mu_noise_vec2 g01 = mu_noise_grad2_from_hash(mu_noise_hash_2i_periodic(ix0, iy1, period_x, period_y, seed));
        mu_noise_vec2 g11 = mu_noise_grad2_from_hash(mu_noise_hash_2i_periodic(ix1, iy1, period_x, period_y, seed));

        float n00 = mu_noise_v2_dot(g00, mu_noise_v2(tx,        ty));
        float n10 = mu_noise_v2_dot(g10, mu_noise_v2(tx - 1.0f, ty));
        float n01 = mu_noise_v2_dot(g01, mu_noise_v2(tx,        ty - 1.0f));
        float n11 = mu_noise_v2_dot(g11, mu_noise_v2(tx - 1.0f, ty - 1.0f));

        {
            float a = mu_noise_lerp(n00, n10, ux);
            float b = mu_noise_lerp(n01, n11, ux);
            return 1.41421356f * mu_noise_lerp(a, b, uy);
        }
    }
}

MU_NOISE_INLINE float mu_noise_perlin3_periodic(float x, float y, float z, int period_x, int period_y, int period_z, uint32_t seed)
{
    if (period_x <= 0 || period_y <= 0 || period_z <= 0) return mu_noise_perlin3(x, y, z, seed);

    {
        int ix0 = (int)floorf(x);
        int iy0 = (int)floorf(y);
        int iz0 = (int)floorf(z);
        int ix1 = ix0 + 1;
        int iy1 = iy0 + 1;
        int iz1 = iz0 + 1;

        float tx = x - (float)ix0;
        float ty = y - (float)iy0;
        float tz = z - (float)iz0;

        float ux = mu_noise_smootherstep5(tx);
        float uy = mu_noise_smootherstep5(ty);
        float uz = mu_noise_smootherstep5(tz);

        mu_noise_vec3 g000 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix0, iy0, iz0, period_x, period_y, period_z, seed));
        mu_noise_vec3 g100 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix1, iy0, iz0, period_x, period_y, period_z, seed));
        mu_noise_vec3 g010 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix0, iy1, iz0, period_x, period_y, period_z, seed));
        mu_noise_vec3 g110 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix1, iy1, iz0, period_x, period_y, period_z, seed));
        mu_noise_vec3 g001 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix0, iy0, iz1, period_x, period_y, period_z, seed));
        mu_noise_vec3 g101 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix1, iy0, iz1, period_x, period_y, period_z, seed));
        mu_noise_vec3 g011 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix0, iy1, iz1, period_x, period_y, period_z, seed));
        mu_noise_vec3 g111 = mu_noise_grad3_from_hash(mu_noise_hash_3i_periodic(ix1, iy1, iz1, period_x, period_y, period_z, seed));

        float n000 = mu_noise_v3_dot(g000, mu_noise_v3(tx,        ty,        tz));
        float n100 = mu_noise_v3_dot(g100, mu_noise_v3(tx - 1.0f, ty,        tz));
        float n010 = mu_noise_v3_dot(g010, mu_noise_v3(tx,        ty - 1.0f, tz));
        float n110 = mu_noise_v3_dot(g110, mu_noise_v3(tx - 1.0f, ty - 1.0f, tz));
        float n001 = mu_noise_v3_dot(g001, mu_noise_v3(tx,        ty,        tz - 1.0f));
        float n101 = mu_noise_v3_dot(g101, mu_noise_v3(tx - 1.0f, ty,        tz - 1.0f));
        float n011 = mu_noise_v3_dot(g011, mu_noise_v3(tx,        ty - 1.0f, tz - 1.0f));
        float n111 = mu_noise_v3_dot(g111, mu_noise_v3(tx - 1.0f, ty - 1.0f, tz - 1.0f));

        {
            float x00 = mu_noise_lerp(n000, n100, ux);
            float x10 = mu_noise_lerp(n010, n110, ux);
            float x01 = mu_noise_lerp(n001, n101, ux);
            float x11 = mu_noise_lerp(n011, n111, ux);
            float y0 = mu_noise_lerp(x00, x10, uy);
            float y1 = mu_noise_lerp(x01, x11, uy);
            return 1.15470054f * mu_noise_lerp(y0, y1, uz);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 4) Simplex noise                                                          */
/* ------------------------------------------------------------------------- */

/*
    SIMPLEX 2D IDEA (visual):

        square grid --(skew)--> equilateral triangle grid

        We find triangle containing point p, then evaluate ONLY 3 corners.

        contribution from each corner c:
          t = r^2 - ||p-c||^2
          if t > 0:  n += t^4 * dot(grad(c), p-c)

        t^4 gives compact support and smooth decay.
*/

MU_NOISE_INLINE float mu_noise_simplex2(float x, float y, uint32_t seed)
{
    const float F2 = 0.36602540378443864676f; /* 0.5*(sqrt(3)-1) */
    const float G2 = 0.21132486540518711775f; /* (3-sqrt(3))/6 */

    float s = (x + y) * F2;
    int i = (int)floorf(x + s);
    int j = (int)floorf(y + s);

    float t = (float)(i + j) * G2;
    float X0 = (float)i - t;
    float Y0 = (float)j - t;
    float x0 = x - X0;
    float y0 = y - Y0;

    int i1;
    int j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else         { i1 = 0; j1 = 1; }

    float x1 = x0 - (float)i1 + G2;
    float y1 = y0 - (float)j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;

    float n0 = 0.0f;
    float n1 = 0.0f;
    float n2 = 0.0f;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 > 0.0f)
    {
        mu_noise_vec2 g0 = mu_noise_grad2_from_hash(mu_noise_hash_2i(i, j, seed));
        t0 *= t0;
        n0 = t0 * t0 * mu_noise_v2_dot(g0, mu_noise_v2(x0, y0));
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0.0f)
    {
        mu_noise_vec2 g1 = mu_noise_grad2_from_hash(mu_noise_hash_2i(i + i1, j + j1, seed));
        t1 *= t1;
        n1 = t1 * t1 * mu_noise_v2_dot(g1, mu_noise_v2(x1, y1));
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0.0f)
    {
        mu_noise_vec2 g2 = mu_noise_grad2_from_hash(mu_noise_hash_2i(i + 1, j + 1, seed));
        t2 *= t2;
        n2 = t2 * t2 * mu_noise_v2_dot(g2, mu_noise_v2(x2, y2));
    }

    return 70.0f * (n0 + n1 + n2);
}

MU_NOISE_INLINE float mu_noise_simplex3(float x, float y, float z, uint32_t seed)
{
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;

    float s = (x + y + z) * F3;
    int i = (int)floorf(x + s);
    int j = (int)floorf(y + s);
    int k = (int)floorf(z + s);

    float t = (float)(i + j + k) * G3;
    float X0 = (float)i - t;
    float Y0 = (float)j - t;
    float Z0 = (float)k - t;

    float x0 = x - X0;
    float y0 = y - Y0;
    float z0 = z - Z0;

    int i1, j1, k1;
    int i2, j2, k2;

    if (x0 >= y0)
    {
        if (y0 >= z0)
        {
            i1 = 1; j1 = 0; k1 = 0;
            i2 = 1; j2 = 1; k2 = 0;
        }
        else if (x0 >= z0)
        {
            i1 = 1; j1 = 0; k1 = 0;
            i2 = 1; j2 = 0; k2 = 1;
        }
        else
        {
            i1 = 0; j1 = 0; k1 = 1;
            i2 = 1; j2 = 0; k2 = 1;
        }
    }
    else
    {
        if (y0 < z0)
        {
            i1 = 0; j1 = 0; k1 = 1;
            i2 = 0; j2 = 1; k2 = 1;
        }
        else if (x0 < z0)
        {
            i1 = 0; j1 = 1; k1 = 0;
            i2 = 0; j2 = 1; k2 = 1;
        }
        else
        {
            i1 = 0; j1 = 1; k1 = 0;
            i2 = 1; j2 = 1; k2 = 0;
        }
    }

    float x1 = x0 - (float)i1 + G3;
    float y1 = y0 - (float)j1 + G3;
    float z1 = z0 - (float)k1 + G3;

    float x2 = x0 - (float)i2 + 2.0f * G3;
    float y2 = y0 - (float)j2 + 2.0f * G3;
    float z2 = z0 - (float)k2 + 2.0f * G3;

    float x3 = x0 - 1.0f + 3.0f * G3;
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;

    float n0 = 0.0f;
    float n1 = 0.0f;
    float n2 = 0.0f;
    float n3 = 0.0f;

    float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
    if (t0 > 0.0f)
    {
        mu_noise_vec3 g0 = mu_noise_grad3_from_hash(mu_noise_hash_3i(i, j, k, seed));
        t0 *= t0;
        n0 = t0 * t0 * mu_noise_v3_dot(g0, mu_noise_v3(x0, y0, z0));
    }

    float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
    if (t1 > 0.0f)
    {
        mu_noise_vec3 g1 = mu_noise_grad3_from_hash(mu_noise_hash_3i(i + i1, j + j1, k + k1, seed));
        t1 *= t1;
        n1 = t1 * t1 * mu_noise_v3_dot(g1, mu_noise_v3(x1, y1, z1));
    }

    float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
    if (t2 > 0.0f)
    {
        mu_noise_vec3 g2 = mu_noise_grad3_from_hash(mu_noise_hash_3i(i + i2, j + j2, k + k2, seed));
        t2 *= t2;
        n2 = t2 * t2 * mu_noise_v3_dot(g2, mu_noise_v3(x2, y2, z2));
    }

    float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
    if (t3 > 0.0f)
    {
        mu_noise_vec3 g3 = mu_noise_grad3_from_hash(mu_noise_hash_3i(i + 1, j + 1, k + 1, seed));
        t3 *= t3;
        n3 = t3 * t3 * mu_noise_v3_dot(g3, mu_noise_v3(x3, y3, z3));
    }

    return 32.0f * (n0 + n1 + n2 + n3);
}

/* ------------------------------------------------------------------------- */
/* 4b) OpenSimplex2 / OpenSimplex2S-inspired variants                       */
/* ------------------------------------------------------------------------- */

/*
    These variants are intentionally lightweight and branch-free wrappers
    around the core simplex sampler:

    - OpenSimplex2-like: rotate domain before simplex sampling to reduce
      axis-aligned artifacts.
    - OpenSimplex2S-like: blend two differently shifted OpenSimplex2-like
      samples for a softer, less directional spectrum.

    Notes:
    - They are compatible in spirit/style with OpenSimplex2 families.
    - They keep this header compact and dependency-free.
*/
MU_NOISE_INLINE float mu_noise_opensimplex2_2d(float x, float y, uint32_t seed)
{
    const float r = 0.70710678118654752440f;
    float xr = (x + y) * r;
    float yr = (y - x) * r;
    return mu_noise_simplex2(xr, yr, seed ^ 0x9E3779B9u);
}

MU_NOISE_INLINE float mu_noise_opensimplex2s_2d(float x, float y, uint32_t seed)
{
    float a = mu_noise_opensimplex2_2d(x, y, seed);
    float b = mu_noise_opensimplex2_2d(x + 31.416f, y - 19.731f, seed ^ 0x85EBCA77u);
    return 0.5f * (a + b);
}

MU_NOISE_INLINE float mu_noise_opensimplex2_3d(float x, float y, float z, uint32_t seed)
{
    const float k1 = 0.70710678118654752440f; /* 1/sqrt(2) */
    const float k2 = 0.40824829046386301637f; /* 1/sqrt(6) */
    const float k3 = 0.57735026918962576451f; /* 1/sqrt(3) */

    float xr = (x - y) * k1;
    float yr = (x + y - 2.0f * z) * k2;
    float zr = (x + y + z) * k3;

    return mu_noise_simplex3(xr, yr, zr, seed ^ 0xC2B2AE35u);
}

MU_NOISE_INLINE float mu_noise_opensimplex2s_3d(float x, float y, float z, uint32_t seed)
{
    float a = mu_noise_opensimplex2_3d(x, y, z, seed);
    float b = mu_noise_opensimplex2_3d(x + 17.0f, y - 23.0f, z + 11.0f, seed ^ 0x27D4EB2Fu);
    return 0.5f * (a + b);
}

/*
    Tileable simplex wrappers.

    Classic simplex itself is not naturally periodic in Cartesian coordinates.
    To build seamless tiles, we periodicize by blending wrapped domain samples.

    2D visual:
       N(x,y) = bilerp(
         S(xr,      yr),
         S(xr-px,   yr),
         S(xr,    yr-py),
         S(xr-px, yr-py),
         tx, ty)

      xr = wrap(x, px), tx = xr / px, etc.
*/
MU_NOISE_INLINE float mu_noise_simplex2_periodic(float x, float y, int period_x, int period_y, uint32_t seed)
{
    if (period_x <= 0 || period_y <= 0) return mu_noise_simplex2(x, y, seed);

    {
        float px = (float)period_x;
        float py = (float)period_y;
        float xr = mu_noise_wrap_period(x, px);
        float yr = mu_noise_wrap_period(y, py);
        float tx = xr / px;
        float ty = yr / py;

        float n00 = mu_noise_simplex2(xr,      yr,      seed);
        float n10 = mu_noise_simplex2(xr - px, yr,      seed);
        float n01 = mu_noise_simplex2(xr,      yr - py, seed);
        float n11 = mu_noise_simplex2(xr - px, yr - py, seed);

        {
            float a = mu_noise_lerp(n00, n10, tx);
            float b = mu_noise_lerp(n01, n11, tx);
            return mu_noise_lerp(a, b, ty);
        }
    }
}

MU_NOISE_INLINE float mu_noise_simplex3_periodic(float x, float y, float z, int period_x, int period_y, int period_z, uint32_t seed)
{
    if (period_x <= 0 || period_y <= 0 || period_z <= 0) return mu_noise_simplex3(x, y, z, seed);

    {
        float px = (float)period_x;
        float py = (float)period_y;
        float pz = (float)period_z;
        float xr = mu_noise_wrap_period(x, px);
        float yr = mu_noise_wrap_period(y, py);
        float zr = mu_noise_wrap_period(z, pz);
        float tx = xr / px;
        float ty = yr / py;
        float tz = zr / pz;

        float n000 = mu_noise_simplex3(xr,      yr,      zr,      seed);
        float n100 = mu_noise_simplex3(xr - px, yr,      zr,      seed);
        float n010 = mu_noise_simplex3(xr,      yr - py, zr,      seed);
        float n110 = mu_noise_simplex3(xr - px, yr - py, zr,      seed);
        float n001 = mu_noise_simplex3(xr,      yr,      zr - pz, seed);
        float n101 = mu_noise_simplex3(xr - px, yr,      zr - pz, seed);
        float n011 = mu_noise_simplex3(xr,      yr - py, zr - pz, seed);
        float n111 = mu_noise_simplex3(xr - px, yr - py, zr - pz, seed);

        {
            float x00 = mu_noise_lerp(n000, n100, tx);
            float x10 = mu_noise_lerp(n010, n110, tx);
            float x01 = mu_noise_lerp(n001, n101, tx);
            float x11 = mu_noise_lerp(n011, n111, tx);
            float y0 = mu_noise_lerp(x00, x10, ty);
            float y1 = mu_noise_lerp(x01, x11, ty);
            return mu_noise_lerp(y0, y1, tz);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 5) Worley / cellular noise                                                */
/* ------------------------------------------------------------------------- */

/*
    Worley noise stores random feature points inside each integer cell.

    For sample p, search neighboring cells and compute distances to features.

      F1 = nearest distance
      F2 = second nearest distance

    Typical textures:
      - veins/cracks: F2 - F1
      - cells: F1
      - borders: step(F2 - F1)
*/

MU_NOISE_INLINE float mu_noise_distance2(mu_noise_vec2 d, mu_noise_distance_metric metric)
{
    if (metric == MU_NOISE_DIST_MANHATTAN)
    {
        return mu_noise_abs(d.x) + mu_noise_abs(d.y);
    }
    if (metric == MU_NOISE_DIST_CHEBYSHEV)
    {
        float ax = mu_noise_abs(d.x);
        float ay = mu_noise_abs(d.y);
        return (ax > ay) ? ax : ay;
    }
    return mu_noise_v2_len(d);
}

MU_NOISE_INLINE float mu_noise_distance3(mu_noise_vec3 d, mu_noise_distance_metric metric)
{
    if (metric == MU_NOISE_DIST_MANHATTAN)
    {
        return mu_noise_abs(d.x) + mu_noise_abs(d.y) + mu_noise_abs(d.z);
    }
    if (metric == MU_NOISE_DIST_CHEBYSHEV)
    {
        float ax = mu_noise_abs(d.x);
        float ay = mu_noise_abs(d.y);
        float az = mu_noise_abs(d.z);
        float m = (ax > ay) ? ax : ay;
        return (m > az) ? m : az;
    }
    return mu_noise_v3_len(d);
}

MU_NOISE_INLINE mu_noise_worley2_result mu_noise_worley2(float x, float y, uint32_t seed, mu_noise_distance_metric metric)
{
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);

    mu_noise_worley2_result out;
    out.f1 = FLT_MAX;
    out.f2 = FLT_MAX;
    out.cell_id1 = 0;
    out.cell_id2 = 0;

    for (int oy = -1; oy <= 1; ++oy)
    {
        for (int ox = -1; ox <= 1; ++ox)
        {
            int cx = ix + ox;
            int cy = iy + oy;

            uint32_t h = mu_noise_hash_2i(cx, cy, seed);
            float fx = mu_noise_hash_to_unit_float(h);
            float fy = mu_noise_hash_to_unit_float(mu_noise_hash_u32(h ^ 0x68bc21ebu));

            mu_noise_vec2 d = mu_noise_v2(((float)cx + fx) - x, ((float)cy + fy) - y);
            float dist = mu_noise_distance2(d, metric);
            int cell_id = (int)mu_noise_hash_u32(h ^ 0x9e3779b9u);

            if (dist < out.f1)
            {
                out.f2 = out.f1;
                out.cell_id2 = out.cell_id1;
                out.f1 = dist;
                out.cell_id1 = cell_id;
            }
            else if (dist < out.f2)
            {
                out.f2 = dist;
                out.cell_id2 = cell_id;
            }
        }
    }

    return out;
}

MU_NOISE_INLINE mu_noise_worley3_result mu_noise_worley3(float x, float y, float z, uint32_t seed, mu_noise_distance_metric metric)
{
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    int iz = (int)floorf(z);

    mu_noise_worley3_result out;
    out.f1 = FLT_MAX;
    out.f2 = FLT_MAX;
    out.cell_id1 = 0;
    out.cell_id2 = 0;

    for (int oz = -1; oz <= 1; ++oz)
    {
        for (int oy = -1; oy <= 1; ++oy)
        {
            for (int ox = -1; ox <= 1; ++ox)
            {
                int cx = ix + ox;
                int cy = iy + oy;
                int cz = iz + oz;

                uint32_t h = mu_noise_hash_3i(cx, cy, cz, seed);
                float fx = mu_noise_hash_to_unit_float(h);
                float fy = mu_noise_hash_to_unit_float(mu_noise_hash_u32(h ^ 0x68bc21ebu));
                float fz = mu_noise_hash_to_unit_float(mu_noise_hash_u32(h ^ 0x02e5be93u));

                mu_noise_vec3 d = mu_noise_v3(((float)cx + fx) - x, ((float)cy + fy) - y, ((float)cz + fz) - z);
                float dist = mu_noise_distance3(d, metric);
                int cell_id = (int)mu_noise_hash_u32(h ^ 0x85ebca6bu);

                if (dist < out.f1)
                {
                    out.f2 = out.f1;
                    out.cell_id2 = out.cell_id1;
                    out.f1 = dist;
                    out.cell_id1 = cell_id;
                }
                else if (dist < out.f2)
                {
                    out.f2 = dist;
                    out.cell_id2 = cell_id;
                }
            }
        }
    }

    return out;
}

/* ------------------------------------------------------------------------- */
/* 6) Fractal combinations                                                   */
/* ------------------------------------------------------------------------- */

/*
    fBm (fractal Brownian motion):

      result = Σ_{i=0..oct-1} amp_i * base(freq_i * p)
      freq_{i+1} = freq_i * lacunarity
      amp_{i+1}  = amp_i  * gain

    Typical defaults:
      lacunarity = 2.0
      gain       = 0.5
      octaves    = 4..8
*/

MU_NOISE_INLINE float mu_noise_fbm1(mu_noise_fn1 base, float x, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * base(x * freq, seed + (uint32_t)i * 1013u);
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

MU_NOISE_INLINE float mu_noise_fbm2(mu_noise_fn2 base, float x, float y, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * base(x * freq, y * freq, seed + (uint32_t)i * 1013u);
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

MU_NOISE_INLINE float mu_noise_fbm3(mu_noise_fn3 base, float x, float y, float z, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * base(x * freq, y * freq, z * freq, seed + (uint32_t)i * 1013u);
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

/*
    Turbulence:
      like fBm, but uses absolute value of base signal.

      turbulence = Σ amp_i * |noise(freq_i * p)|
*/
MU_NOISE_INLINE float mu_noise_turbulence2(mu_noise_fn2 base, float x, float y, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        float n = base(x * freq, y * freq, seed + (uint32_t)i * 1013u);
        sum += amp * mu_noise_abs(n);
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

MU_NOISE_INLINE float mu_noise_turbulence3(mu_noise_fn3 base, float x, float y, float z, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        float n = base(x * freq, y * freq, z * freq, seed + (uint32_t)i * 1013u);
        sum += amp * mu_noise_abs(n);
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

/*
    Billow noise:
      billow = 2*|noise| - 1

    It makes rounded "cloud/pillow" structures.
*/
MU_NOISE_INLINE float mu_noise_billow2(mu_noise_fn2 base, float x, float y, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        float n = base(x * freq, y * freq, seed + (uint32_t)i * 1013u);
        n = 2.0f * mu_noise_abs(n) - 1.0f;
        sum += amp * n;
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

MU_NOISE_INLINE float mu_noise_billow3(mu_noise_fn3 base, float x, float y, float z, uint32_t seed, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        float n = base(x * freq, y * freq, z * freq, seed + (uint32_t)i * 1013u);
        n = 2.0f * mu_noise_abs(n) - 1.0f;
        sum += amp * n;
        norm += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

/*
    Ridged multifractal (Musgrave style flavor):

      signal = offset - |noise|
      signal = signal^2
      signal *= weight
      weight = clamp(signal * gain, 0, 1)
      sum += signal * amp

    Good for sharp mountain ridges.
*/
MU_NOISE_INLINE float mu_noise_ridged2(mu_noise_fn2 base, float x, float y, uint32_t seed, int octaves, float lacunarity, float gain, float offset)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    float weight = 1.0f;

    for (int i = 0; i < octaves; ++i)
    {
        float n = base(x * freq, y * freq, seed + (uint32_t)i * 1013u);
        float signal = offset - mu_noise_abs(n);
        signal *= signal;
        signal *= weight;

        weight = mu_noise_clamp(signal * gain, 0.0f, 1.0f);
        sum += signal * amp;

        freq *= lacunarity;
        amp *= 0.5f;
    }

    return mu_noise_clamp(sum * 2.0f - 1.0f, -1.0f, 1.0f);
}

MU_NOISE_INLINE float mu_noise_ridged3(mu_noise_fn3 base, float x, float y, float z, uint32_t seed, int octaves, float lacunarity, float gain, float offset)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    float weight = 1.0f;

    for (int i = 0; i < octaves; ++i)
    {
        float n = base(x * freq, y * freq, z * freq, seed + (uint32_t)i * 1013u);
        float signal = offset - mu_noise_abs(n);
        signal *= signal;
        signal *= weight;

        weight = mu_noise_clamp(signal * gain, 0.0f, 1.0f);
        sum += signal * amp;

        freq *= lacunarity;
        amp *= 0.5f;
    }

    return mu_noise_clamp(sum * 2.0f - 1.0f, -1.0f, 1.0f);
}

/* ------------------------------------------------------------------------- */
/* 7) Domain warp                                                            */
/* ------------------------------------------------------------------------- */

/*
    Domain warping injects complexity by perturbing coordinates first.

      p' = p + A * W(p * f)
      out = N(p')

    where W is often simplex/perlin and N can be any base noise.
*/
MU_NOISE_INLINE mu_noise_vec2 mu_noise_domain_warp2(mu_noise_fn2 warp, float x, float y, uint32_t seed, float warp_amp, float warp_freq)
{
    float qx = warp(x * warp_freq, y * warp_freq, seed ^ 0xA511E9B3u);
    float qy = warp((x + 19.19f) * warp_freq, (y - 33.47f) * warp_freq, seed ^ 0x63D83595u);
    return mu_noise_v2(x + warp_amp * qx, y + warp_amp * qy);
}

MU_NOISE_INLINE mu_noise_vec3 mu_noise_domain_warp3(mu_noise_fn3 warp, float x, float y, float z, uint32_t seed, float warp_amp, float warp_freq)
{
    float qx = warp(x * warp_freq, y * warp_freq, z * warp_freq, seed ^ 0xA511E9B3u);
    float qy = warp((x + 19.19f) * warp_freq, (y - 33.47f) * warp_freq, (z + 11.73f) * warp_freq, seed ^ 0x63D83595u);
    float qz = warp((x - 47.11f) * warp_freq, (y +  7.23f) * warp_freq, (z + 29.91f) * warp_freq, seed ^ 0xC2B2AE35u);
    return mu_noise_v3(x + warp_amp * qx, y + warp_amp * qy, z + warp_amp * qz);
}

MU_NOISE_INLINE float mu_noise_domain_warped2(mu_noise_fn2 base, mu_noise_fn2 warp, float x, float y, uint32_t seed, float warp_amp, float warp_freq)
{
    mu_noise_vec2 p = mu_noise_domain_warp2(warp, x, y, seed, warp_amp, warp_freq);
    return base(p.x, p.y, seed);
}

MU_NOISE_INLINE float mu_noise_domain_warped3(mu_noise_fn3 base, mu_noise_fn3 warp, float x, float y, float z, uint32_t seed, float warp_amp, float warp_freq)
{
    mu_noise_vec3 p = mu_noise_domain_warp3(warp, x, y, z, seed, warp_amp, warp_freq);
    return base(p.x, p.y, p.z, seed);
}

/* ------------------------------------------------------------------------- */
/* 8) Curl noise                                                             */
/* ------------------------------------------------------------------------- */

/*
    Curl noise builds a divergence-free vector field.

    2D scalar potential Psi(x,y):
      v = ( dPsi/dy, -dPsi/dx )

    Finite difference approximation:
      dPsi/dx ≈ (Psi(x+e,y)-Psi(x-e,y))/(2e)
      dPsi/dy ≈ (Psi(x,y+e)-Psi(x,y-e))/(2e)
*/
MU_NOISE_INLINE mu_noise_vec2 mu_noise_curl2(mu_noise_fn2 potential, float x, float y, uint32_t seed, float eps)
{
    float ex = (eps > 0.0f) ? eps : 1.0e-3f;

    float p_x1 = potential(x + ex, y, seed);
    float p_x0 = potential(x - ex, y, seed);
    float p_y1 = potential(x, y + ex, seed);
    float p_y0 = potential(x, y - ex, seed);

    float dpsi_dx = (p_x1 - p_x0) / (2.0f * ex);
    float dpsi_dy = (p_y1 - p_y0) / (2.0f * ex);

    return mu_noise_v2(dpsi_dy, -dpsi_dx);
}

/*
    3D curl from vector potential A = (Ax, Ay, Az):

      curl(A) = (
        dAz/dy - dAy/dz,
        dAx/dz - dAz/dx,
        dAy/dx - dAx/dy
      )

    We generate Ax,Ay,Az from the same scalar noise with different seeds.
*/
MU_NOISE_INLINE mu_noise_vec3 mu_noise_curl3(mu_noise_fn3 scalar, float x, float y, float z, uint32_t seed, float eps)
{
    float ex = (eps > 0.0f) ? eps : 1.0e-3f;

    uint32_t sx = seed ^ 0xA511E9B3u;
    uint32_t sy = seed ^ 0x63D83595u;
    uint32_t sz = seed ^ 0xC2B2AE35u;

    /* dAz/dy and dAy/dz */
    float Az_y1 = scalar(x, y + ex, z, sz);
    float Az_y0 = scalar(x, y - ex, z, sz);
    float Ay_z1 = scalar(x, y, z + ex, sy);
    float Ay_z0 = scalar(x, y, z - ex, sy);

    /* dAx/dz and dAz/dx */
    float Ax_z1 = scalar(x, y, z + ex, sx);
    float Ax_z0 = scalar(x, y, z - ex, sx);
    float Az_x1 = scalar(x + ex, y, z, sz);
    float Az_x0 = scalar(x - ex, y, z, sz);

    /* dAy/dx and dAx/dy */
    float Ay_x1 = scalar(x + ex, y, z, sy);
    float Ay_x0 = scalar(x - ex, y, z, sy);
    float Ax_y1 = scalar(x, y + ex, z, sx);
    float Ax_y0 = scalar(x, y - ex, z, sx);

    float dAz_dy = (Az_y1 - Az_y0) / (2.0f * ex);
    float dAy_dz = (Ay_z1 - Ay_z0) / (2.0f * ex);

    float dAx_dz = (Ax_z1 - Ax_z0) / (2.0f * ex);
    float dAz_dx = (Az_x1 - Az_x0) / (2.0f * ex);

    float dAy_dx = (Ay_x1 - Ay_x0) / (2.0f * ex);
    float dAx_dy = (Ax_y1 - Ax_y0) / (2.0f * ex);

    return mu_noise_v3(
        dAz_dy - dAy_dz,
        dAx_dz - dAz_dx,
        dAy_dx - dAx_dy
    );
}

/* ------------------------------------------------------------------------- */
/* Convenience recipes                                                       */
/* ------------------------------------------------------------------------- */

/*
    Useful one-liners for common terrain/material patterns.
*/
MU_NOISE_INLINE float mu_noise_terrain_height(float x, float y, uint32_t seed)
{
    float n_continent = mu_noise_fbm2(mu_noise_simplex2, x * 0.0009f, y * 0.0009f, seed + 11u, 5, 2.0f, 0.5f);
    float n_detail    = mu_noise_ridged2(mu_noise_perlin2, x * 0.0070f, y * 0.0070f, seed + 29u, 4, 2.0f, 2.0f, 1.0f);
    return 0.75f * n_continent + 0.25f * n_detail;
}

MU_NOISE_INLINE float mu_noise_marble(float x, float y, uint32_t seed)
{
    float warp = mu_noise_fbm2(mu_noise_simplex2, x * 1.5f, y * 1.5f, seed + 3u, 4, 2.0f, 0.5f);
    float t = x * 3.14159265f + 2.0f * warp;
    return sinf(t);
}

MU_NOISE_INLINE float mu_noise_cells_edge(float x, float y, uint32_t seed)
{
    mu_noise_worley2_result w = mu_noise_worley2(x, y, seed, MU_NOISE_DIST_EUCLIDEAN);
    return w.f2 - w.f1;
}

#ifdef __cplusplus
}
#endif

#endif /* MU_NOISE_MATH_H */
