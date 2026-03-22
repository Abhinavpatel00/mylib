// the aim is this lib is to fullfill my personal needs for drop in headers or header + impl to use in new projects i feel c is good language with some drawbacks for that i will  ofcourse designing my own language but i dont think i will make that language for shipping i enjoy programming so it is just for that c99 is good enough for shipping this lib is to make c tolerable and provide good data structures and algos
//
#ifndef MU_H
#define MU_H
// -----------------------------------------------------------------------------
//  single-header configuration
//
// Usage (one .c/.cpp file):
//   #define MU_IMPLEMENTATION
//   #include "mu.h"
// -----------------------------------------------------------------------------
#include <assert.h>
#include <cstdint>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef MU_API

#define internal static
#define global static
#define local_persist static


#if defined(MU_STATIC)
#define MU_API static
#elif defined(_WIN32) && defined(MU_DLL_EXPORT)
#define MU_API __declspec(dllexport)
#elif defined(_WIN32) && defined(MU_DLL_IMPORT)
#define MU_API __declspec(dllimport)
#else
#define MU_API extern
#endif
#endif

#ifdef __cplusplus
#define MU_EXTERN_C extern "C"
#define MU_BEGIN_EXTERN_C extern "C" {
#define MU_END_EXTERN_C }
#else
#define MU_EXTERN_C extern
#define MU_BEGIN_EXTERN_C
#define MU_END_EXTERN_C
#endif

#ifndef MU_UNUSED
#define MU_UNUSED(x) (void)(x)
#endif

#ifndef MU_NOOP
#define MU_NOOP()                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
    } while(0)
#endif

#ifndef MU_DEPRECATED
#if defined(_MSC_VER)
#define MU_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define MU_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define MU_DEPRECATED(msg)
#endif
#endif

MU_BEGIN_EXTERN_C

#include <stdint.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <x86intrin.h>
#endif

// restrict is a promise to the compiler:
//
// “This pointer is the only way to access this memory.”
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define MU_RESTRICT restrict
#elif defined(_MSC_VER)
#define MU_RESTRICT __restrict
#else
#define MU_RESTRICT
#endif


#if defined(_MSC_VER)
#define MU_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define MU_INLINE inline __attribute__((always_inline))
#else
#define MU_INLINE inline
#endif

#if defined(_MSC_VER)
#define MU_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define MU_NOINLINE __attribute__((noinline))
#else
#define MU_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MU_LIKELY(x) __builtin_expect(!!(x), 1)
#define MU_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define MU_LIKELY(x) (x)
#define MU_UNLIKELY(x) (x)
#endif

#if defined(_MSC_VER)
#define MU_ALIGN(N) __declspec(align(N))
#else
#define MU_ALIGN(N) __attribute__((aligned(N)))
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define MU_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(__cplusplus)
#define MU_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define MU_STATIC_ASSERT_GLUE_(a, b) a##b
#define MU_STATIC_ASSERT_GLUE(a, b) MU_STATIC_ASSERT_GLUE_(a, b)
#define MU_STATIC_ASSERT(cond, msg) typedef char MU_STATIC_ASSERT_GLUE(mu_static_assertion_, __LINE__)[(cond) ? 1 : -1]
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MU_PREFETCH(addr) __builtin_prefetch(addr)
#else
#define MU_PREFETCH(addr)
#endif

#if defined(_MSC_VER)
#define MU_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define MU_DEBUG_BREAK() __builtin_trap()
#else
#define MU_DEBUG_BREAK() (*(volatile int*)0 = 0)
#endif
#define MU_OFFSET_OF(type, member) ((size_t)&(((type*)0)->member))

// If I have a pointer to a struct member, how do I get a pointer to the struct that contains it?
#define MU_CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - MU_OFFSET_OF(type, member)))
#if defined(__GNUC__) || defined(__clang__)
#define MU_MIN(a, b)                                                                                                   \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a < _b ? _a : _b;                                                                                             \
    })
#define MU_MAX(a, b)                                                                                                   \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a > _b ? _a : _b;                                                                                             \
    })
#else
#define MU_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MU_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


#define MU_ASSERT(x)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(x))                                                                                                       \
            MU_DEBUG_BREAK();                                                                                          \
    } while(0)

#define MU_PANIC()                                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        MU_DEBUG_BREAK();                                                                                              \
        *(volatile int*)0 = 0;                                                                                         \
    } while(0)


#define for_each(i, count) for(size_t i = 0; i < (count); i++)
#define MU_FOR_RANGE(i, begin, end) for(size_t i = (begin); i < (end); i++)
#define MU_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define MU_IS_POW2(x) (((x) & ((x) - 1)) == 0)
#define MU_KB(x) ((x) * 1024ull)
#define MU_MB(x) (MU_KB(x) * 1024ull)
#define MU_GB(x) (MU_MB(x) * 1024ull)

#define KB(x) ((x) * 1024ULL)
#define MB(x) ((x) * 1024ULL * 1024ULL)
#define GB(x) ((x) * 1024ULL * 1024ULL * 1024ULL)
#define PAD(name, size) uint8_t name[(size)]


#define MU_STRINGIFY(x) #x
#define MU_TOSTRING(x) MU_STRINGIFY(x)

#define MU_CONCAT(a, b) a##b
#define MU_CONCAT2(a, b) MU_CONCAT(a, b)
#define MU_CEIL(x, y) (((x) + (y) - 1) / (y))
#define MU_FLOOR(x, y) ((x) / (y))
#define MU_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define MU_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define MU_CLAMP(x, lo, hi) (MU_MIN(MU_MAX((x), (lo)), (hi)))

#define MU_BIT(n) (1ull << (n))
#define MU_HAS_FLAG(x, flag) (((x) & (flag)) != 0)
#define MU_SET_FLAG(x, flag) ((x) |= (flag))
#define MU_CLEAR_FLAG(x, flag) ((x) &= ~(flag))
#define MU_ABS(x) ((x) < 0 ? -(x) : (x))
#ifdef __cplusplus
#define MU_EXTERN extern "C"
#else
#define MU_EXTERN extern
#endif

#define MU_SWAP(TYPE, a, b)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        TYPE mu_t = (a);                                                                                               \
        (a)       = (b);                                                                                               \
        (b)       = mu_t;                                                                                              \
    } while(0)
// conventions
//
//        mu_<module>_<action>
/*
 helpers for bit packing
 plan : there are mostly as many bit packing tricks as computer sciences the word on machines is usually bytes and we are allowed for operatio
 on bytes they are most efficient for cpus so why even consider bit packing if operation on bytes are more efficient the ans is memory and memory latency is far far more bottleneck than cpu ops one cache miss worth many many cpu instruction as motivated by example of taocp in cards u can use a u8 to pack info about a card in poker thats will help to load 8 cards in one cache line 
few extra bitwise ops are basically free compared to dragging more memory

*/
/*

We sometimes need the raw bit pattern of a float. Not its value. Its binary representation.

Example float:

1.0f

Binary inside memory:

0 01111111 00000000000000000000000
^ ^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^
s exponent mantissa

But in C, if you do this:

uint32_t x = (uint32_t)f;

you are converting the number, not reading its bits.

Example:

float 1.5 → integer 1

The bit pattern is lost.

We need the exact bits, because float compression works by manipulating the exponent and mantissa directly.
So we use a union.

union {
    float    f;
    uint32_t u;
};

Both variables occupy the same memory.

Visualization:

memory (4 bytes)

[ byte ][ byte ][ byte ][ byte ]
     ↑ same memory

float view
uint32 view

So this works:

mu_float_bits fb;
fb.f = 1.0f;

printf("%u\n", fb.u);

No conversion happens.
We simply reinterpret the bits.

 C also allows memcpy for this.

*/

/*
===============================================================================
Mu Quantization Utilities
-------------------------------------------------------------------------------
Tools for compressing float data for rendering.

Includes:
    float -> half conversion
    half  -> float conversion
    float precision reduction
    UNORM quantization
    SNORM quantization

Used for:
    vertex packing
    texture coordinates
    normals
    GPU bandwidth reduction
===============================================================================
*/

/*
    reinterpret float bits as integer bits

    float layout (IEEE 754)

    [ sign | exponent | mantissa ]
       1        8          23
*/
typedef union
{
    float    f;
    uint32_t u;
} mu_float_bits;


/*
===============================================================================
float -> half float
-------------------------------------------------------------------------------
    Convert 32-bit float → 16-bit half float

    float layout:
    [s][eeeeeeee][mmmmmmmmmmmmmmmmmmmmmmm]
     1    8                23

    half layout:
    [s][eeeee][mmmmmmmmmm]
     1   5        10
===============================================================================
*/
MU_INLINE uint16_t mu_quantize_half(float v)
{
    mu_float_bits fb = {.f = v};
    uint32_t      ui = fb.u;
    /* extract sign and move to half-float position */
    uint32_t sign = (ui >> 16) & 0x8000;
    /* remove sign so we can work on exponent/mantissa */
    uint32_t em = ui & 0x7fffffff;


    /*
        adjust exponent bias

        float bias = 127
        half  bias = 15

        difference = 112
    */
    uint32_t half = (em - (112u << 23) + (1u << 12)) >> 13;

    /* undermu → 0 */
    if(em < (113u << 23))
        half = 0;

    /* overmu → infinity */
    if(em >= (143u << 23))
        half = 0x7c00;

    /* NaN → quiet NaN */
    if(em > (255u << 23))
        half = 0x7e00;
    return (uint16_t)(sign | half);
}


/*
===============================================================================
half -> float
===============================================================================
*/
MU_INLINE float mu_dequantize_half(uint16_t h)
{
    /* extract sign */
    uint32_t sign = ((uint32_t)(h & 0x8000)) << 16;

    /* exponent + mantissa */
    uint32_t em = h & 0x7fff;

    /*
        restore exponent bias

        half bias = 15
        float bias = 127
        difference = 112
    */

    uint32_t r = (em + (112u << 10)) << 13;

    /* denormals → zero */
    if(em < (1u << 10))
        r = 0;

    /* infinity / NaN */
    if(em >= (31u << 10))
        r += (112u << 23);

    mu_float_bits fb;
    fb.u = sign | r;

    return fb.f;
}


/*
===============================================================================
reduce float precision
-------------------------------------------------------------------------------
keep N mantissa bits (max 23)
   Reduce float precision.

    float mantissa = 23 bits

    If N = 10
    keep 10 bits, remove 13 bits.
original mantissa

mmmmmmmmmmmmmmmmmmmmmmm  (23 bits)

↓

after quantization (N=10)

mmmmmmmmmm0000000000000
===============================================================================
*/
MU_INLINE float mu_quantize_float(float v, int n)
{
    assert(n >= 0 && n <= 23);

    mu_float_bits fb = {.f = v};
    uint32_t      ui = fb.u;
    /* mask of bits to remove */
    uint32_t mask = (1u << (23 - n)) - 1;

    /* rounding offset */
    uint32_t round = (1u << (23 - n)) >> 1;

    /* isolate exponent */
    uint32_t exponent = ui & 0x7f800000;

    /* round and clear lower bits */
    uint32_t rounded = (ui + round) & ~mask;

    /* avoid touching inf/nan */
    if(exponent != 0x7f800000)
        ui = rounded;

    /* flush denormals */
    if(exponent == 0)
        ui = 0;
    fb.u = ui;
    return fb.f;
}


/*
===============================================================================
UNORM quantization
-------------------------------------------------------------------------------
float [0,1] → integer with N bits
===============================================================================
*/
MU_INLINE int mu_quantize_unorm(float v, int n)
{
    float scale = (float)((1 << n) - 1);

    if(v < 0.0f)
        v = 0.0f;
    if(v > 1.0f)
        v = 1.0f;

    return (int)(v * scale + 0.5f);
}


/*
===============================================================================
SNORM quantization
-------------------------------------------------------------------------------
float [-1,1] → signed integer
===============================================================================
*/
MU_INLINE int mu_quantize_snorm(float v, int n)
{
    float scale = (float)((1 << (n - 1)) - 1);
    float round = (v >= 0.0f) ? 0.5f : -0.5f;

    if(v < -1.0f)
        v = -1.0f;
    if(v > 1.0f)
        v = 1.0f;

    return (int)(v * scale + round);
}
MU_INLINE float mu_dequantize_unorm(int v, int n)
{
    float scale = (float)((1 << n) - 1);
    return (float)v / scale;
}

MU_INLINE float mu_dequantize_snorm(int v, int n)
{
    float scale = (float)((1 << (n - 1)) - 1);
    return (float)v / scale;
}
// bitset static without malloc + with malloc

#if defined(_MSC_VER) && !defined(__clang__)

#include <intrin.h>


static MU_INLINE int mu_trailing_zeroes_u64(uint64_t x)
{
    if(x == 0)
        return 64;

    unsigned long idx;

#if defined(_WIN64)
    _BitScanForward64(&idx, x);
    return (int)idx;
#else
    if((uint32_t)x != 0)
    {
        _BitScanForward(&idx, (uint32_t)x);
        return (int)idx;
    }
    _BitScanForward(&idx, (uint32_t)(x >> 32));
    return (int)(idx + 32);
#endif
}

static MU_INLINE int mu_leading_zeroes_u64(uint64_t x)
{
    if(x == 0)
        return 64;

    unsigned long idx;

#if defined(_WIN64)
    _BitScanReverse64(&idx, x);
    return 63 - (int)idx;
#else
    if((x >> 32) != 0)
    {
        _BitScanReverse(&idx, (uint32_t)(x >> 32));
        return 31 - (int)idx;
    }
    _BitScanReverse(&idx, (uint32_t)x);
    return 63 - (int)idx;
#endif
}

static MU_INLINE int mu_popcount_u64(uint64_t x)

#if defined_WIN64)
    return (int)__popcnt64(x);
#else
    return (int)(__popcnt((uint32_t)x) + __popcnt((uint32_t)(x >> 32)));
#endif
}

#else /* GCC / Clang / others with builtins */
/* ------------------------------------------------------------------
   Count trailing zero bits in a 64-bit integer.

   Definition:
   Returns the number of consecutive zero bits starting from the
   least-significant bit (right side).

   Example:
       x = 0b001011000
                       ^^^
       Result = 3

   Notes:
   - If x == 0, returns 64.
   - Maps to TZCNT/BSF on x86.
------------------------------------------------------------------ */
static MU_INLINE uint32_t mu_trailing_zeroes_u64(uint64_t x)
{
    return __builtin_ctzll(x | (x == 0)) + ((uint32_t)(x == 0) * 64u);
}
/* ------------------------------------------------------------------
   Count leading zero bits in a 64-bit integer.

   Definition:
   Returns the number of consecutive zero bits starting from the
   most-significant bit (left side).

   Example (simplified):
       x = 0b00010010
           ^^^
       Result depends on full 64-bit layout.
       For 0x10, result = 59.

   Notes:
   - If x == 0, returns 64.
   - Maps to LZCNT/BSR on x86.
------------------------------------------------------------------ */
static MU_INLINE uint32_t mu_leading_zeroes_u64(uint64_t x)
{
    return __builtin_clzll(x | (x == 0)) + (uint32_t)(x == 0);
}
// note : 	lzcntq	only comes when we compile with  -O3  -O3 -S -fverbose-asm  -march=native else its mostly uses bsr and this version is branchless

/* ------------------------------------------------------------------
   Count number of bits set to 1 (population count).

   Definition:
   Returns how many bits are set in x.

   Example:
       x = 0b10110100
       Result = 4

   Notes:
   - Maps to POPCNT instruction.
   - Result is in range [0, 64].
------------------------------------------------------------------ */
static MU_INLINE uint32_t mu_popcount_u64(uint64_t x)
{
    return (uint32_t)__builtin_popcountll(x);
}


#endif


//  can we  simulate generic from c11
/*
 * there are tradeoffs here as everywhere in life i actually first thought to have bitset store bit count but that turns out a little cumbersome so we are going with word count 
*/

typedef struct mu_bitset
{
    uint64_t* MU_RESTRICT array;         /* pointer to 64-bit word storage */
    size_t                word_count;    /* number words stored */
    size_t                word_capacity; /* allocated capacity in 64-bit words */
} mu_bitset;

/* Create a new bitset. Return NULL in case of failure. */
mu_bitset* mu_bitset_create(void);

/* Create a new bitset able to contain size bits. Return NULL in case of failure. */
mu_bitset* mu_bitset_create_with_capacity(size_t size);

/* Free memory. */
void mu_bitset_free(mu_bitset* bitset);

/* Set all bits to zero. */
void mu_bitset_clear(mu_bitset* bitset);

/* Set all bits to one. */
void mu_bitset_fill(mu_bitset* bitset);

/* Create a copy. */
mu_bitset* mu_bitset_copy(const mu_bitset* src);

/* Resize in 64-bit words. New words are zeroed. */
bool mu_bitset_resize_words(mu_bitset* bs, size_t new_word_count);

/* Grow in 64-bit words. */
bool mu_bitset_grow(mu_bitset* bs, size_t new_word_count);

/* attempts to recover unused memory, return false in case of reallocation failure */
bool mu_bitset_trim(mu_bitset* bs);

/* shifts all bits by shift positions so values 1,2,10 become 1+shift,2+shift,10+shift */
void mu_bitset_shift_left(mu_bitset* bs, size_t shift);

/* shifts all bits by shift positions so values 1,2,10 become 1-shift,2-shift,10-shift */
void mu_bitset_shift_right(mu_bitset* bs, size_t shift);

/* Set/reset/test bits. */
void mu_bitset_set(mu_bitset* bs, size_t bit_index);
void mu_bitset_reset(mu_bitset* bs, size_t bit_index);
bool mu_bitset_test(const mu_bitset* bs, size_t bit_index);
void mu_bitset_enable_bit(mu_bitset* bs, size_t bit_index);
void mu_bitset_disable_bit(mu_bitset* bs, size_t bit_index);
void mu_bitset_set_bit(mu_bitset* bs, size_t bit_index, bool value);
bool mu_bitset_get_bit(const mu_bitset* bs, size_t bit_index);

/* Query sizes. */
static MU_INLINE size_t mu_bitset_size_in_bytes(const mu_bitset* bs)
{
    return bs->word_count * sizeof(uint64_t);
}

static MU_INLINE size_t mu_bitset_size_in_bits(const mu_bitset* bs)
{
    return bs->word_count * 64;
}

static MU_INLINE size_t mu_bitset_size_in_words(const mu_bitset* bs)
{
    return bs->word_count;
}

/* Set algebra and counters. */
size_t mu_bitset_count(const mu_bitset* bs);
bool   mu_bitset_empty(const mu_bitset* bs);
size_t mu_bitset_minimum(const mu_bitset* bs);
size_t mu_bitset_maximum(const mu_bitset* bs);
bool   mu_bitset_equal(const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);
bool   mu_bitsets_disjoint(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
bool   mu_bitsets_intersect(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
bool   mu_bitset_contains_all(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
bool   mu_bitset_inplace_union(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
size_t mu_bitset_union_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
void   mu_bitset_inplace_intersection(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
size_t mu_bitset_intersection_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
void   mu_bitset_inplace_difference(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
size_t mu_bitset_difference_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
bool   mu_bitset_inplace_symmetric_difference(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
size_t mu_bitset_symmetric_difference_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2);
bool   mu_bitset_xor(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);
bool   mu_bitset_or(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);
bool   mu_bitset_and(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);
bool   mu_bitset_not(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a);
bool   mu_bitset_xor_assign(mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);
bool   mu_bitset_or_assign(mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);
bool   mu_bitset_and_assign(mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b);

/* Iteration helpers. */
typedef bool (*mu_bitset_iterator)(size_t value, void* param);
typedef mu_bitset_iterator mu_bitset_visit_fn;

void mu_bitset_traverse(const mu_bitset* bs, mu_bitset_visit_fn visitor, void* user);
void mu_bitset_traverse_range(const mu_bitset* bs, mu_bitset_visit_fn visitor, void* user, size_t begin, size_t count);

static MU_INLINE bool mu_bitset_next_set_bit(const mu_bitset* bs, size_t* i)
{
    size_t x = *i / 64;
    if(x >= bs->word_count)
    {
        return false;
    }
    uint64_t w = bs->array[x];
    w >>= (*i & 63);
    if(w != 0)
    {
        *i += (size_t)mu_trailing_zeroes_u64(w);
        return true;
    }
    x++;
    while(x < bs->word_count)
    {
        w = bs->array[x];
        if(w != 0)
        {
            *i = x * 64 + (size_t)mu_trailing_zeroes_u64(w);
            return true;
        }
        x++;
    }
    return false;
}

static MU_INLINE size_t mu_bitset_next_set_bits(const mu_bitset* bs, size_t* buffer, size_t capacity, size_t* startfrom)
{
    if(capacity == 0)
        return 0;

    size_t x = *startfrom / 64;
    if(x >= bs->word_count)
    {
        return 0;
    }

    uint64_t w = bs->array[x];
    w &= ~((UINT64_C(1) << (*startfrom & 63)) - 1);

    size_t howmany = 0;
    size_t base    = x << 6;
    while(howmany < capacity)
    {
        while(w != 0)
        {
            uint64_t t        = w & (~w + 1);
            int      r        = mu_trailing_zeroes_u64(w);
            buffer[howmany++] = (size_t)r + base;
            if(howmany == capacity)
                goto end;
            w ^= t;
        }
        x += 1;
        if(x == bs->word_count)
        {
            break;
        }
        base += 64;
        w = bs->array[x];
    }
end:
    if(howmany > 0)
    {
        *startfrom = buffer[howmany - 1];
    }
    return howmany;
}

static MU_INLINE bool mu_bitset_for_each(const mu_bitset* bs, mu_bitset_iterator iterator, void* ptr)
{
    size_t base = 0;
    for(size_t i = 0; i < bs->word_count; ++i)
    {
        uint64_t w = bs->array[i];
        while(w != 0)
        {
            uint64_t t = w & (~w + 1);
            int      r = mu_trailing_zeroes_u64(w);
            if(!iterator((size_t)r + base, ptr))
                return false;
            w ^= t;
        }
        base += 64;
    }
    return true;
}

void mu_bitset_print(const mu_bitset* b);

// What Problem It Solves
//
// You have IDs from:
//
// 0 ... maxID
//
// You want to:
//
// • allocate a single ID
// • allocate a contiguous block of IDs
// • free an ID or range
// • do it efficiently
// i mainly experimented with it for using it for bindless vulkan got it from nvpro and   https://www.humus.name/3D/MakeID.h
typedef struct
{
    uint32_t first;
    uint32_t last;
} mu_id_pool_range;

typedef struct
{
    mu_id_pool_range* ranges;
    uint32_t          count;
    uint32_t          capacity;
    uint32_t          max_id;
    uint32_t          used_ids;
} mu_id_pool;
void mu_id_pool_init(mu_id_pool* pool, uint32_t pool_size);
void mu_id_pool_deinit(mu_id_pool* pool);
void mu_id_pool_destroy_all(mu_id_pool* pool);

/* allocation */
bool mu_id_pool_create_id(mu_id_pool* pool, uint32_t* out_id);
bool mu_id_pool_create_range_id(mu_id_pool* pool, uint32_t* out_id, uint32_t count);

/* deallocation */
bool mu_id_pool_destroy_id(mu_id_pool* pool, uint32_t id);
bool mu_id_pool_destroy_range_id(mu_id_pool* pool, uint32_t id, uint32_t count);

/* queries */
bool mu_id_pool_is_range_available(const mu_id_pool* pool, uint32_t search_count);
void mu_id_pool_print_ranges(const mu_id_pool* pool);
void mu_id_pool_check_ranges(const mu_id_pool* pool);

uint32_t mu_id_pool_get_available_ids(const mu_id_pool* pool);
bool     mu_id_pool_is_id(const mu_id_pool* pool, uint32_t id);
uint32_t mu_id_pool_get_largest_continuous_range(const mu_id_pool* pool);


//    https://kernelnewbies.org/FAQ/LinkedLists
typedef struct mu_list_node
{
    int                  value;
    struct mu_list_node* next;
} mu_list_node;

typedef struct mu_list
{
    mu_list_node* head;
} mu_list;

void mu_list_init(mu_list* list);
void mu_list_clear(mu_list* list);

void mu_list_push_front(mu_list* list, int value);
void mu_list_push_back(mu_list* list, int value);

int mu_list_remove_first(mu_list* list, int value);
int mu_list_remove_all(mu_list* list, int value);

mu_list_node* mu_list_find(mu_list* list, int value);

size_t mu_list_length(const mu_list* list);
void   mu_list_reverse(mu_list* list);

void mu_list_print(const mu_list* list);

/// i dont think stack and deque provides any value as ds
///  queue is  interesting we might have many variations
///  intrusive ?? with array?? linked list may be may be not

// knuth problem 24 pg 329
typedef struct
{
    uint32_t* dense;   // size = capacity
    uint32_t* sparse;  // size = capacity
    uint32_t  size;    // number of active elements
    uint32_t  capacity;
} mu_sparse_set;

/* Initialization (caller provides memory) */
void mu_sparse_set_init(mu_sparse_set* set, uint32_t* dense_buffer, uint32_t* sparse_buffer, uint32_t capacity);

/* Basic operations */
void mu_sparse_set_clear(mu_sparse_set* set);
bool mu_sparse_set_contains(const mu_sparse_set* set, uint32_t value);
bool mu_sparse_set_add(mu_sparse_set* set, uint32_t value);
bool mu_sparse_set_remove(mu_sparse_set* set, uint32_t value);

/* Iteration */
static MU_INLINE uint32_t mu_sparse_set_at(const mu_sparse_set* set, uint32_t index)
{
    return set->dense[index];
}

typedef struct
{
    mu_list_node* last;
    uint64_t      size;
} mu_circular_list;

/* lifecycle */
void mu_circular_list_init(mu_circular_list* list);
void mu_circular_list_clear(mu_circular_list* list);

/* insertion */
void mu_circular_list_push_front(mu_circular_list* list, int value);
void mu_circular_list_push_back(mu_circular_list* list, int value);

/* removal */
int mu_circular_list_pop_front(mu_circular_list* list, int* out_value);
int mu_circular_list_remove_first(mu_circular_list* list, int value);

/* lookup */
mu_list_node* mu_circular_list_find(mu_circular_list* list, int value);

/* utility */
uint64_t mu_circular_list_length(const mu_circular_list* list);
void     mu_circular_list_print(const mu_circular_list* list);


typedef struct
{
    uint64_t state;
    uint64_t inc;
} mu_pcg32;

/* ------------------ Scalar ------------------ */

MU_INLINE void mu_pcg32_init(mu_pcg32* rng, uint64_t seed, uint64_t seq)
{
    rng->state = 0u;
    rng->inc   = (seq << 1u) | 1u;
    rng->state = rng->state * 6364136223846793005ULL + rng->inc;
    rng->state += seed;
    rng->state = rng->state * 6364136223846793005ULL + rng->inc;
}

MU_INLINE uint32_t mu_pcg32_next_u32(mu_pcg32* rng)
{
    uint64_t oldstate = rng->state;
    rng->state        = oldstate * 6364136223846793005ULL + rng->inc;

    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot        = (uint32_t)(oldstate >> 59u);

    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}


/* ------------------ SIMD Batch (AVX2) ------------------ */
//
// #if defined(__AVX2__)
// #include <immintrin.h>
//
// typedef struct
// {
//     __m256i state;
//     __m256i inc;
// } mu_pcg32x4;
//
// /* initialize 4 parallel streams */
// MU_INLINE void mu_pcg32x4_init(mu_pcg32x4* rng,
//                                    uint64_t      seed0,
//                                    uint64_t      seed1,
//                                    uint64_t      seed2,
//                                    uint64_t      seed3,
//                                    uint64_t      seq0,
//                                    uint64_t      seq1,
//                                    uint64_t      seq2,
//                                    uint64_t      seq3)
// {
//     __m256i seeds = _mm256_set_epi64x(seed3, seed2, seed1, seed0);
//     __m256i seqs  = _mm256_set_epi64x(seq3, seq2, seq1, seq0);
//
//     rng->state = _mm256_setzero_si256();
//     rng->inc   = _mm256_or_si256(_mm256_slli_epi64(seqs, 1), _mm256_set1_epi64x(1));
//
//     __m256i mul = _mm256_set1_epi64x(6364136223846793005ULL);
//
//     rng->state = _mm256_add_epi64(_mm256_mullo_epi64(rng->state, mul), rng->inc);
//
//     rng->state = _mm256_add_epi64(rng->state, seeds);
//
//     rng->state = _mm256_add_epi64(_mm256_mullo_epi64(rng->state, mul), rng->inc);
// }
//
// MU_INLINE __m256i mu_pcg32x4_next_u32(mu_pcg32x4* rng)
// {
//     __m256i oldstate = rng->state;
//     __m256i mul      = _mm256_set1_epi64x(6364136223846793005ULL);
//
//     rng->state = _mm256_add_epi64(_mm256_mullo_epi64(oldstate, mul), rng->inc);
//
//     __m256i xorshifted = _mm256_srli_epi64(_mm256_xor_si256(_mm256_srli_epi64(oldstate, 18), oldstate), 27);
//
//     __m256i rot = _mm256_srli_epi64(oldstate, 59);
//
//     __m256i xs32  = _mm256_cvtepi64_epi32(xorshifted);
//     __m256i rot32 = _mm256_cvtepi64_epi32(rot);
//
//     __m256i r1 = _mm256_srlv_epi32(xs32, rot32);
//     __m256i r2 =
//         _mm256_sllv_epi32(xs32, _mm256_and_si256(_mm256_sub_epi32(_mm256_set1_epi32(32), rot32), _mm256_set1_epi32(31)));
//
//     return _mm256_or_si256(r1, r2);
// }
//
//#endif

typedef struct
{
    uint8_t* memory;
    uint32_t size;
} mu_buffer;

typedef struct
{
    mu_buffer buffer;
    uint32_t  head;
} mu_linear_allocator;


MU_INLINE void mu_linear_init(mu_linear_allocator* a, void* memory, uint32_t size)
{
    a->buffer.memory = (uint8_t*)memory;
    a->buffer.size   = size;
    a->head          = 0;
}


MU_INLINE void* mu_linear_alloc(mu_linear_allocator* a, uint32_t size, uint32_t align)
{
    uint32_t head = MU_ALIGN_UP(a->head, align);

    if(head + size > a->buffer.size)
        return NULL;

    void* ptr = a->buffer.memory + head;
    a->head   = head + size;

    return ptr;
}
MU_INLINE void mu_linear_reset(mu_linear_allocator* a)
{
    a->head = 0;
}
typedef struct
{
    mu_buffer buffer;

    uint32_t head;
    uint32_t tail;

} mu_ring_allocator;
MU_INLINE void mu_ring_init(mu_ring_allocator* r, void* memory, uint32_t size)
{
    r->buffer.memory = (uint8_t*)memory;
    r->buffer.size   = size;
    r->head          = 0;
    r->tail          = 0;
}
MU_INLINE void* mu_ring_alloc(mu_ring_allocator* r, uint32_t size, uint32_t align, uint32_t* out_offset)
{
    uint32_t head = MU_ALIGN_UP(r->head, align);

    // Case 1: normal region
    if(head >= r->tail)
    {
        if(head + size <= r->buffer.size)
        {
            *out_offset = head;
            r->head     = head + size;
            return r->buffer.memory + head;
        }

        // wrap
        head = 0;
    }

    // Case 2: after wrap
    if(head + size <= r->tail)
    {
        *out_offset = head;
        r->head     = head + size;
        return r->buffer.memory + head;
    }

    return NULL;
}
MU_INLINE void mu_ring_free_to(mu_ring_allocator* r, uint32_t offset)
{
    r->tail = offset;
}

MU_INLINE uint32_t mu_ring_used(const mu_ring_allocator* r)
{
    if(r->head >= r->tail)
        return r->head - r->tail;

    return (r->buffer.size - r->tail) + r->head;
}


#if defined(_WIN32)
#include <windows.h>

static uint64_t mu_time_now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}

static double mu_time_freq()
{
    static double freq = 0;
    if(!freq)
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq = (double)f.QuadPart;
    }
    return freq;
}

#else
#include <time.h>

static uint64_t mu_time_now()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}

static double mu_time_freq()
{
    return 1e9;  // nanoseconds
}
#endif


#define MU_SCOPE_TIMER(name)                                                                                           \
    for(uint64_t __start = mu_time_now(), __once                                                   = 1; __once;        \
        printf("%s: %.3f ms\n", name, (mu_time_now() - __start) * 1000.0 / mu_time_freq()), __once = 0)


/*
USER POINTER (a)
        |
        v
+---------------------+-----------------------+
| header              | actual data           |
| size | capacity     | a[0] a[1] a[2] ...    |
+---------------------+-----------------------+
        ^
        |
   (a - header_size)

*/

typedef struct
{
    uint32_t size;
    uint32_t capacity;
} array_header_t;

#define array_header(a) ((array_header_t*)((char*)(a) - sizeof(array_header_t)))

#define array_size(a) ((a) ? array_header(a)->size : 0)
#define array_capacity(a) ((a) ? array_header(a)->capacity : 0)

#define array_free(a) ((a) ? free(array_header(a)), (a) = NULL : 0)

#define array_full(a) ((a) && array_size(a) >= array_capacity(a))

static void* array_grow(void* arr, size_t elem_size, uint32_t min_capacity)
{
    uint32_t new_capacity = 16;

    if(arr)
    {
        new_capacity = array_capacity(arr) * 2;
    }

    if(new_capacity < min_capacity)
        new_capacity = min_capacity;

    size_t new_size = sizeof(array_header_t) + new_capacity * elem_size;

    array_header_t* new_header;

    if(arr)
    {
        new_header = (array_header_t*)realloc(array_header(arr), new_size);
    }
    else
    {
        new_header       = (array_header_t*)malloc(new_size);
        new_header->size = 0;
    }

    new_header->capacity = new_capacity;

    return (char*)new_header + sizeof(array_header_t);
}
#define array_push(a, val)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(a) || array_full(a))                                                                                      \
        {                                                                                                              \
            (a) = array_grow((a), sizeof(*(a)), array_size(a) + 1);                                                    \
        }                                                                                                              \
        (a)[array_header(a)->size++] = (val);                                                                          \
    } while(0)

#define array_reserve(a, n) ((!(a) || array_capacity(a) < (n)) ? (a = array_grow((a), sizeof(*(a)), (n))) : 0)

#define array_pop(a) ((a) ? --array_header(a)->size : 0)

#define array_back(a) ((a)[array_header(a)->size - 1])
// ============================================================
// HASH TABLE (uint64 -> uint64)
// ============================================================


typedef struct
{
    uint32_t  capacity;
    uint32_t  count;
    uint64_t* keys;
    uint64_t* values;
} hash_t;

#define HASH_EMPTY 0

// ------------------------------------------------------------
// Simple mix (not crypto, just decent)
// ------------------------------------------------------------
static uint64_t hash_u64(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

// ------------------------------------------------------------
static void hash_init(hash_t* h, uint32_t cap)
{
    h->capacity = cap;
    h->count    = 0;
    h->keys     = (uint64_t*)calloc(cap, sizeof(uint64_t));
    h->values   = (uint64_t*)calloc(cap, sizeof(uint64_t));
}

// ------------------------------------------------------------
static void hash_free(hash_t* h)
{
    free(h->keys);
    free(h->values);
}

// ------------------------------------------------------------
// Linear probing
//
// index = hash % capacity
// if collision → move forward
//
// Worst case: O(n)
// Average: O(1)
// ------------------------------------------------------------
static uint32_t hash_find_slot(hash_t* h, uint64_t key)
{
    uint32_t i = hash_u64(key) % h->capacity;

    while(h->keys[i] != HASH_EMPTY && h->keys[i] != key)
    {
        i = (i + 1) % h->capacity;
    }

    return i;
}

// ------------------------------------------------------------
static void hash_put(hash_t* h, uint64_t key, uint64_t value)
{
    uint32_t i = hash_find_slot(h, key);

    if(h->keys[i] == HASH_EMPTY)
    {
        h->count++;
    }

    h->keys[i]   = key;
    h->values[i] = value;
}

// ------------------------------------------------------------
static uint64_t hash_get(hash_t* h, uint64_t key, uint64_t def)
{
    uint32_t i = hash_find_slot(h, key);

    if(h->keys[i] == HASH_EMPTY)
        return def;

    return h->values[i];
}



/*
    ┌──────────────────────────────────────────────┐
    │              mu_hash32_static                │
    └──────────────────────────────────────────────┘
// taken  from our  machinery blog
    A fixed-size hash table with:
    - open addressing (linear probing)
    - no allocations (you provide memory)
    - no resizing (your problem, not mine)
    - O(1) average lookup (until you mess up load factor)

    Ideal for:
    - resource lookup (texture_id, mesh_id)
    - entity → component index mapping
    - hot-path systems where malloc = sin

    Not ideal for:
    - dynamic growth
    - deletion-heavy workloads
    - people who don't understand capacity planning
*/


/*
    Sentinel value representing EMPTY slot

    ASCII:
        keys: [FFFF][FFFF][FFFF] → all unused

    Why 0xFF?
    Because memset can blast it fast.
*/
#define MU_HASH_UNUSED 0xffffffffffffffffULL


typedef struct mu_hash32_static_t
{
    uint64_t* keys;    // hashed keys
    uint32_t* values;  // associated values
    uint32_t  n;       // capacity (fixed)

    /*
        MEMORY MODEL:

        keys:   [k0][k1][k2][k3]...[kn]
        values: [v0][v1][v2][v3]...[vn]

    */

} mu_hash32_static_t;


/*
    Initialize with user-provided memory

    You allocate memory → we just hook into it
*/
static inline void mu_hash32_static_init(mu_hash32_static_t* h, uint64_t* keys, uint32_t* values, uint32_t n)
{
    h->keys   = keys;
    h->values = values;
    h->n      = n;

    /*
        Immediately clear → mark all as unused
    */
    memset(h->keys, 0xFF, sizeof(uint64_t) * n);
}


/*
    Clear table (reset all keys)

    ASCII before:
        [K1][K2][K3]

    after:
        [--][--][--]
*/
static inline void mu_hash32_static_clear(mu_hash32_static_t* h)
{
    memset(h->keys, 0xFF, sizeof(uint64_t) * h->n);
}


/*
    Insert or overwrite key → value

    Core idea:
        i = hash % n

        if occupied → probe forward

    ASCII collision chain:

        index:  0   1   2   3
                K1  K2  --  --

        insert K3:
            → 0 busy
            → 1 busy
            → 2 empty → place
*/
static inline void mu_hash32_static_set(mu_hash32_static_t* h, uint64_t key, uint32_t value)
{
    uint32_t i = key % h->n;

    /*
        Linear probing loop

        keep walking until:
        - same key (overwrite)
        - empty slot (insert)
    */
    while(h->keys[i] != key && h->keys[i] != MU_HASH_UNUSED)
    {
        i = (i + 1) % h->n;
    }

    h->keys[i]   = key;
    h->values[i] = value;
}


/*
    Lookup key → value

    Returns:
        0 if not found
        (so don't store 0 as a meaningful value unless you're into pain)
*/
static inline uint32_t mu_hash32_static_get(const mu_hash32_static_t* h, uint64_t key)
{
    uint32_t i = key % h->n;

    /*
        Probe until:
        - key found → return
        - empty slot → not found
    */
    while(h->keys[i] != key && h->keys[i] != MU_HASH_UNUSED)
    {
        i = (i + 1) % h->n;
    }

    return (h->keys[i] == MU_HASH_UNUSED) ? 0 : h->values[i];
}

// -----------------------------------------------------------------------------
// Data Structures Part 2: Indices (key -> many values)
//
// Uses a hash map from key to first node and a circular doubly linked list of
// nodes per key for O(1) add/remove when node index is known.
// -----------------------------------------------------------------------------

#define MU_MULTI_INDEX_NONE UINT32_MAX

typedef struct mu_multi_index_node
{
    uint64_t key;
    uint32_t value;
    uint32_t prev;
    uint32_t next;
    uint32_t alive;
} mu_multi_index_node;

typedef struct mu_multi_index
{
    mu_multi_index_node* nodes;
    uint32_t             node_count;
    uint32_t             node_capacity;
    uint32_t             free_head;

    uint64_t* map_keys;
    uint32_t* map_values;
    uint8_t*  map_states;
    uint32_t  map_capacity;
    uint32_t  map_count;
} mu_multi_index;

typedef bool (*mu_multi_index_visit_fn)(uint32_t value, uint32_t node_index, void* user);

void     mu_multi_index_init(mu_multi_index* index, uint32_t initial_node_capacity, uint32_t initial_map_capacity);
void     mu_multi_index_deinit(mu_multi_index* index);
uint32_t mu_multi_index_add(mu_multi_index* index, uint64_t key, uint32_t value);
bool     mu_multi_index_remove(mu_multi_index* index, uint32_t node_index);
uint32_t mu_multi_index_first(const mu_multi_index* index, uint64_t key);
uint32_t mu_multi_index_next(const mu_multi_index* index, uint32_t start_node, uint32_t node_index);
bool     mu_multi_index_node_valid(const mu_multi_index* index, uint32_t node_index);
uint32_t mu_multi_index_value(const mu_multi_index* index, uint32_t node_index);
uint64_t mu_multi_index_key(const mu_multi_index* index, uint32_t node_index);
uint32_t mu_multi_index_count_key(const mu_multi_index* index, uint64_t key);
void     mu_multi_index_visit_key(const mu_multi_index* index, uint64_t key, mu_multi_index_visit_fn visitor, void* user);

// -----------------------------------------------------------------------------
// Data Structures Part 3: Arrays of arrays (fixed-size child chunks)
//
// Multiple logical arrays can share one chunk pool. Each logical array is
// represented by first/last chunk plus total count.
// -----------------------------------------------------------------------------

#define MU_CHUNKED_U32_NONE UINT32_MAX

#ifndef MU_ARRAY_OF_ARRAYS_CHUNK_SIZE
#define MU_ARRAY_OF_ARRAYS_CHUNK_SIZE 14u
#endif

typedef struct mu_chunked_u32_chunk
{
    uint32_t values[MU_ARRAY_OF_ARRAYS_CHUNK_SIZE];
    uint32_t used;
    uint32_t prev_chunk;
    uint32_t next_chunk;
    uint32_t free_next;
} mu_chunked_u32_chunk;

typedef struct mu_chunked_u32_pool
{
    mu_chunked_u32_chunk* chunks;
    uint32_t              chunk_count;
    uint32_t              chunk_capacity;
    uint32_t              free_head;
} mu_chunked_u32_pool;

typedef struct mu_chunked_u32_array
{
    uint32_t first_chunk;
    uint32_t last_chunk;
    uint32_t count;
} mu_chunked_u32_array;

typedef bool (*mu_chunked_u32_visit_fn)(uint32_t value, void* user);

void mu_chunked_u32_pool_init(mu_chunked_u32_pool* pool, uint32_t initial_capacity);
void mu_chunked_u32_pool_deinit(mu_chunked_u32_pool* pool);

void mu_chunked_u32_array_init(mu_chunked_u32_array* array);
void mu_chunked_u32_array_clear(mu_chunked_u32_pool* pool, mu_chunked_u32_array* array);
bool mu_chunked_u32_array_push(mu_chunked_u32_pool* pool, mu_chunked_u32_array* array, uint32_t value);
bool mu_chunked_u32_array_pop(mu_chunked_u32_pool* pool, mu_chunked_u32_array* array, uint32_t* out_value);
bool mu_chunked_u32_array_get(const mu_chunked_u32_pool* pool, const mu_chunked_u32_array* array, uint32_t index,
    uint32_t* out_value);
void mu_chunked_u32_array_visit(
    const mu_chunked_u32_pool* pool, const mu_chunked_u32_array* array, mu_chunked_u32_visit_fn visitor, void* user);

// -----------------------------------------------------------------------------
// Data Structures Part 1: Bulk data with holes + weak handles
//
// Slot 0 is a freelist header. Free slots are linked through their slot memory
// (first u32 in slot storage). Generations support weak-handle validation.
// -----------------------------------------------------------------------------

typedef struct mu_weak_handle
{
    uint32_t id;
    uint32_t generation;
} mu_weak_handle;

typedef struct mu_bulk_storage
{
    uint8_t*  slots;
    uint32_t* generations;
    uint8_t*  live;
    size_t    slot_size;
    uint32_t  slot_capacity;
    uint32_t  live_count;
    uint32_t  next_unused;
} mu_bulk_storage;

typedef bool (*mu_bulk_storage_visit_fn)(uint32_t id, void* slot, void* user);

bool         mu_bulk_storage_init(mu_bulk_storage* storage, size_t slot_size, uint32_t initial_slot_capacity);
void         mu_bulk_storage_deinit(mu_bulk_storage* storage);
uint32_t     mu_bulk_storage_alloc(mu_bulk_storage* storage);
bool         mu_bulk_storage_free(mu_bulk_storage* storage, uint32_t id);
void*        mu_bulk_storage_ptr(mu_bulk_storage* storage, uint32_t id);
const void*  mu_bulk_storage_ptr_const(const mu_bulk_storage* storage, uint32_t id);
bool         mu_bulk_storage_is_live(const mu_bulk_storage* storage, uint32_t id);
mu_weak_handle mu_bulk_storage_make_handle(const mu_bulk_storage* storage, uint32_t id);
bool         mu_bulk_storage_validate_handle(const mu_bulk_storage* storage, mu_weak_handle handle);
void*        mu_bulk_storage_resolve_handle(mu_bulk_storage* storage, mu_weak_handle handle);
const void*  mu_bulk_storage_resolve_handle_const(const mu_bulk_storage* storage, mu_weak_handle handle);
void         mu_bulk_storage_visit_live(mu_bulk_storage* storage, mu_bulk_storage_visit_fn visitor, void* user);

/*









*/



MU_END_EXTERN_C

/*
TODO:
implement stack, queue ,deque,fast or sparse sets         https://github.com/ericherman/libfastset and topo sort 
linked list with pointers and operation on it and static array version and may be mannaged  arena version(saves calling malloc everytime)
avl tree,binary tree with pointers and operation on it and static array version and may be mannaged  arena version(saves calling malloc everytime
- stack sort of knuth  
*/
#ifdef MU_IMPLEMENTATION
// • Computes required number of 64-bit words to hold size bits.
mu_bitset* mu_bitset_create()
{
    mu_bitset* bitset = NULL;
    /* Allocate the bitset itself. */
    bitset                = (mu_bitset*)malloc(sizeof(mu_bitset));
    bitset->array         = NULL;
    bitset->word_count    = 0;
    bitset->word_capacity = 0;
    return bitset;
}
/*
You want to store size bits.
Your storage is uint64_t.
Each uint64_t holds:
8 bytes × 8 bits = 64 bits.
So the real question is:
“How many 64-bit words do I need to store size bits?”
*/
mu_bitset* mu_bitset_create_with_capacity(size_t size)
{
    mu_bitset* bitset = (mu_bitset*)malloc(sizeof(mu_bitset));
    //“bits per word = bytes per word × 8”
    bitset->word_count    = MU_CEIL(size, sizeof(uint64_t) * 8);
    bitset->word_capacity = bitset->word_count;

    bitset->array = (uint64_t*)calloc(bitset->word_count, sizeof(uint64_t));
    return bitset;
}


void mu_bitset_free(mu_bitset* bitset)
{
    free(bitset->array);
    free(bitset);
}

void mu_bitset_clear(mu_bitset* bitset)
{
    memset(bitset->array, 0, sizeof(uint64_t) * bitset->word_count);
}

void mu_bitset_fill(mu_bitset* bitset)
{
    memset(bitset->array, 0xff, sizeof(uint64_t) * bitset->word_count);
}

/*
Mu-prefixed aliases keep the public API naming consistent.
These functions intentionally stay minimal and avoid redundant checks.
*/

mu_bitset* mu_bitset_copy(const mu_bitset* src)
{
    mu_bitset* copy     = (mu_bitset*)malloc(sizeof *copy);
    copy->word_count    = src->word_count;
    copy->word_capacity = src->word_count;

    if(src->word_count == 0)
    {
        copy->array = NULL;
        return copy;
    }

    copy->array = (uint64_t*)malloc(sizeof(uint64_t) * src->word_count);
    memcpy(copy->array, src->array, sizeof(uint64_t) * src->word_count);
    return copy;
}


static bool mu_bitset_resize_impl(mu_bitset* bs, size_t new_word_count, bool padwithzeroes)
{


    if(new_word_count > SIZE_MAX / 64)
        return false;

    if(new_word_count > bs->word_capacity)
    {

        uint64_t* newarray;
        size_t    new_word_cap = (UINT64_C(0xFFFFFFFFFFFFFFFF) >> mu_leading_zeroes_u64(new_word_count)) + 1;
        newarray               = (uint64_t*)realloc(bs->array, sizeof(uint64_t) * new_word_cap);
        bs->word_capacity      = new_word_cap;
        bs->array              = newarray;
    }


    /* Zero new region if expanding */
    if(padwithzeroes && new_word_count > bs->word_count)
    {
        size_t delta = new_word_count - bs->word_count;
        memset(bs->array + bs->word_count, 0, delta * sizeof(uint64_t));
    }


    bs->word_count = new_word_count;
    return true;
}

bool mu_bitset_resize_words(mu_bitset* bs, size_t new_word_count)
{
    return mu_bitset_resize_impl(bs, new_word_count, true);
}

/*
Map a bit index to storage location:
  word index = bit / 64
  bit offset = bit % 64
*/
void mu_bitset_set(mu_bitset* bs, size_t bit_index)
{
    size_t word_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    if(word_index >= bs->word_count)
        mu_bitset_resize_impl(bs, word_index + 1, true);

    bs->array[word_index] |= (UINT64_C(1) << bit_offset);
}

void mu_bitset_reset(mu_bitset* bs, size_t bit_index)
{
    size_t word_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    if(word_index >= bs->word_count)
        return;

    bs->array[word_index] &= ~(UINT64_C(1) << bit_offset);
}

bool mu_bitset_test(const mu_bitset* bs, size_t bit_index)
{
    size_t word_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    if(word_index >= bs->word_count)
        return false;

    return (bs->array[word_index] & (UINT64_C(1) << bit_offset)) != 0;
}

void mu_bitset_enable_bit(mu_bitset* bs, size_t bit_index)
{
    mu_bitset_set(bs, bit_index);
}

void mu_bitset_disable_bit(mu_bitset* bs, size_t bit_index)
{
    mu_bitset_reset(bs, bit_index);
}

void mu_bitset_set_bit(mu_bitset* bs, size_t bit_index, bool value)
{
    if(value)
    {
        mu_bitset_set(bs, bit_index);
    }
    else
    {
        mu_bitset_reset(bs, bit_index);
    }
}

bool mu_bitset_get_bit(const mu_bitset* bs, size_t bit_index)
{
    return mu_bitset_test(bs, bit_index);
}
/*
Goal:
Shift the entire bitset left by shift bits.
Equivalent to:

bitset *= 2^shift

Think of the bitset as one giant binary number spread across 64-bit chunks.

Memory layout (little-endian words):

array[0]  = lowest  64 bits
array[1]  = next    64 bits
...
array[n-1]= highest 64 bits
*/

void mu_bitset_shift_left(mu_bitset* bs, size_t shift)
{
    if(shift == 0 || bs->word_count == 0)
        return;

    size_t word_shift = shift / 64;  // whole 64-bit blocks
    size_t bit_shift  = shift % 64;  // remaining bits
    size_t old_count  = bs->word_count;

    /* Ensure enough space */
    size_t new_count = old_count + word_shift + (bit_shift ? 1 : 0);
    mu_bitset_resize_impl(bs, new_count, true);  // this zeroes new region

    uint64_t* a = bs->array;

    /*
        We move from high → low to avoid overwriting data
        we still need.

        VISUAL EXAMPLE (shift = 70):

            word_shift = 1
            bit_shift  = 6

        Before:
            [ w3 | w2 | w1 | w0 ]

        After:
            [  0 | new3 | new2 | new1 | new0 ]

        where:
            new_i = (old_i << 6) | (old_(i-1) >> 58)
    */

    if(bit_shift == 0)
    {
        /* Pure word shift (easy case) */

        for(size_t i = old_count; i > 0; --i)
            a[i - 1 + word_shift] = a[i - 1];
    }
    else
    {
        /* Word + bit shift */

        /* Highest word: only left shift, no carry-in */
        a[old_count - 1 + word_shift + 1] = a[old_count - 1] >> (64 - bit_shift);

        for(size_t i = old_count - 1; i > 0; --i)
        {
            a[i + word_shift] = (a[i] << bit_shift) | (a[i - 1] >> (64 - bit_shift));
        }

        /* Lowest word: only left shift */
        a[word_shift] = a[0] << bit_shift;
    }

    /* Zero-fill newly created lowest words */
    for(size_t i = 0; i < word_shift; ++i)
        a[i] = 0;
}
void mu_bitset_shift_right(mu_bitset* bs, size_t shift)
{
    if(shift == 0 || bs->word_count == 0)
        return;

    size_t word_shift = shift / 64;  // whole-word shift
    size_t bit_shift  = shift % 64;  // remaining bit shift
    size_t old_count  = bs->word_count;

    if(word_shift >= old_count)
    {
        /* Everything shifts out */
        mu_bitset_resize_impl(bs, 0, false);
        return;
    }

    uint64_t* a = bs->array;

    /*
        VISUAL EXAMPLE (shift = 70):

            word_shift = 1
            bit_shift  = 6

        Before:
            [ w3 | w2 | w1 | w0 ]

        After:
            [ new2 | new1 | new0 ]

        where:
            new_i = (old_(i+1) << (64-6)) | (old_i >> 6)
    */

    if(bit_shift == 0)
    {
        /* Pure word shift */

        for(size_t i = 0; i < old_count - word_shift; ++i)
            a[i] = a[i + word_shift];
    }
    else
    {
        /* Word + bit shift */

        for(size_t i = 0; i + word_shift + 1 < old_count; ++i)
        {
            a[i] = (a[i + word_shift] >> bit_shift) | (a[i + word_shift + 1] << (64 - bit_shift));
        }

        /* Highest remaining word: no carry-in */
        a[old_count - word_shift - 1] = a[old_count - 1] >> bit_shift;
    }

    /* Logical shrink */
    mu_bitset_resize_impl(bs, old_count - word_shift, false);
}


bool mu_bitset_grow(mu_bitset* bs, size_t new_word_count)
{
    if(new_word_count < bs->word_count)
    {
        return false;
    }
    if(new_word_count > SIZE_MAX / 64)
    {
        return false;
    }
    if(bs->word_capacity < new_word_count)
    {
        uint64_t* newarray;
        size_t    newcapacity = bs->word_capacity;
        if(newcapacity == 0)
        {
            newcapacity = 1;
        }
        while(newcapacity < new_word_count)
        {
            newcapacity *= 2;
        }
        if((newarray = (uint64_t*)realloc(bs->array, sizeof(uint64_t) * newcapacity)) == NULL)
        {
            return false;
        }
        bs->word_capacity = newcapacity;
        bs->array         = newarray;
    }
    memset(bs->array + bs->word_count, 0, sizeof(uint64_t) * (new_word_count - bs->word_count));
    bs->word_count = new_word_count;
    return true;
}

size_t mu_bitset_count(const mu_bitset* bs)
{
    size_t card = 0;
    size_t k    = 0;
    for(; k + 7 < bs->word_count; k += 8)
    {
        card += mu_popcount_u64(bs->array[k]);
        card += mu_popcount_u64(bs->array[k + 1]);
        card += mu_popcount_u64(bs->array[k + 2]);
        card += mu_popcount_u64(bs->array[k + 3]);
        card += mu_popcount_u64(bs->array[k + 4]);
        card += mu_popcount_u64(bs->array[k + 5]);
        card += mu_popcount_u64(bs->array[k + 6]);
        card += mu_popcount_u64(bs->array[k + 7]);
    }
    for(; k + 3 < bs->word_count; k += 4)
    {
        card += mu_popcount_u64(bs->array[k]);
        card += mu_popcount_u64(bs->array[k + 1]);
        card += mu_popcount_u64(bs->array[k + 2]);
        card += mu_popcount_u64(bs->array[k + 3]);
    }
    for(; k < bs->word_count; k++)
    {
        card += mu_popcount_u64(bs->array[k]);
    }
    return card;
}

bool mu_bitset_inplace_union(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    for(size_t k = 0; k < minlength; ++k)
    {
        b1->array[k] |= b2->array[k];
    }
    if(b2->word_count > b1->word_count)
    {
        size_t oldsize = b1->word_count;
        if(!mu_bitset_resize_words(b1, b2->word_count))
            return false;
        memcpy(b1->array + oldsize, b2->array + oldsize, (b2->word_count - oldsize) * sizeof(uint64_t));
    }
    return true;
}

bool mu_bitset_empty(const mu_bitset* bs)
{
    for(size_t k = 0; k < bs->word_count; k++)
    {
        if(bs->array[k] != 0)
        {
            return false;
        }
    }
    return true;
}

size_t mu_bitset_minimum(const mu_bitset* bs)
{
    for(size_t k = 0; k < bs->word_count; k++)
    {
        uint64_t w = bs->array[k];
        if(w != 0)
        {
            return mu_trailing_zeroes_u64(w) + k * 64;
        }
    }
    return SIZE_MAX;
}

size_t mu_bitset_maximum(const mu_bitset* bs)
{
    for(size_t k = bs->word_count; k > 0; k--)
    {
        uint64_t w = bs->array[k - 1];
        if(w != 0)
        {
            return 63 - mu_leading_zeroes_u64(w) + (k - 1) * 64;
        }
    }
    return 0;
}

/* Returns true if bitsets share no common elements, false otherwise.
 *
 * Performs early-out if common element found. */
bool mu_bitsets_disjoint(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;

    for(size_t k = 0; k < minlength; k++)
    {
        if((b1->array[k] & b2->array[k]) != 0)
            return false;
    }
    return true;
}

/* Returns true if bitsets contain at least 1 common element, false if they are
 * disjoint.
 *
 * Performs early-out if common element found. */
bool mu_bitsets_intersect(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;

    for(size_t k = 0; k < minlength; k++)
    {
        if((b1->array[k] & b2->array[k]) != 0)
            return true;
    }
    return false;
}

/* Returns true if b has any bits set in or after b->array[starting_loc]. */
static bool mu_bitset_any_bits_set(const mu_bitset* b, size_t starting_loc)
{
    if(starting_loc >= b->word_count)
    {
        return false;
    }
    for(size_t k = starting_loc; k < b->word_count; k++)
    {
        if(b->array[k] != 0)
            return true;
    }
    return false;
}

bool mu_bitset_equal(const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;

    for(size_t k = 0; k < min_size; ++k)
    {
        if(a->array[k] != b->array[k])
        {
            return false;
        }
    }

    if(a->word_count > b->word_count)
    {
        return !mu_bitset_any_bits_set(a, b->word_count);
    }

    return !mu_bitset_any_bits_set(b, a->word_count);
}

/* Returns true if b1 has all of b2's bits set.
 *
 * Performs early out if a bit is found in b2 that is not found in b1. */
bool mu_bitset_contains_all(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t min_size = b1->word_count;
    if(b1->word_count > b2->word_count)
    {
        min_size = b2->word_count;
    }
    for(size_t k = 0; k < min_size; k++)
    {
        if((b1->array[k] & b2->array[k]) != b2->array[k])
        {
            return false;
        }
    }
    if(b2->word_count > b1->word_count)
    {
        /* Need to check if b2 has any bits set beyond b1's array */
        return !mu_bitset_any_bits_set(b2, b1->word_count);
    }
    return true;
}

size_t mu_bitset_union_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t answer    = 0;
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    for(; k + 3 < minlength; k += 4)
    {
        answer += mu_popcount_u64(b1->array[k] | b2->array[k]);
        answer += mu_popcount_u64(b1->array[k + 1] | b2->array[k + 1]);
        answer += mu_popcount_u64(b1->array[k + 2] | b2->array[k + 2]);
        answer += mu_popcount_u64(b1->array[k + 3] | b2->array[k + 3]);
    }
    for(; k < minlength; ++k)
    {
        answer += mu_popcount_u64(b1->array[k] | b2->array[k]);
    }
    if(b2->word_count > b1->word_count)
    {
        for(; k + 3 < b2->word_count; k += 4)
        {
            answer += mu_popcount_u64(b2->array[k]);
            answer += mu_popcount_u64(b2->array[k + 1]);
            answer += mu_popcount_u64(b2->array[k + 2]);
            answer += mu_popcount_u64(b2->array[k + 3]);
        }
        for(; k < b2->word_count; ++k)
        {
            answer += mu_popcount_u64(b2->array[k]);
        }
    }
    else
    {
        for(; k + 3 < b1->word_count; k += 4)
        {
            answer += mu_popcount_u64(b1->array[k]);
            answer += mu_popcount_u64(b1->array[k + 1]);
            answer += mu_popcount_u64(b1->array[k + 2]);
            answer += mu_popcount_u64(b1->array[k + 3]);
        }
        for(; k < b1->word_count; ++k)
        {
            answer += mu_popcount_u64(b1->array[k]);
        }
    }
    return answer;
}

void mu_bitset_inplace_intersection(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    for(; k < minlength; ++k)
    {
        b1->array[k] &= b2->array[k];
    }
    for(; k < b1->word_count; ++k)
    {
        b1->array[k] = 0;
    }
}

size_t mu_bitset_intersection_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t answer    = 0;
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    for(size_t k = 0; k < minlength; ++k)
    {
        answer += mu_popcount_u64(b1->array[k] & b2->array[k]);
    }
    return answer;
}

void mu_bitset_inplace_difference(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    for(; k < minlength; ++k)
    {
        b1->array[k] &= ~(b2->array[k]);
    }
}

size_t mu_bitset_difference_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    size_t answer    = 0;
    for(; k < minlength; ++k)
    {
        answer += mu_popcount_u64(b1->array[k] & ~(b2->array[k]));
    }
    for(; k < b1->word_count; ++k)
    {
        answer += mu_popcount_u64(b1->array[k]);
    }
    return answer;
}

bool mu_bitset_inplace_symmetric_difference(mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    for(; k < minlength; ++k)
    {
        b1->array[k] ^= b2->array[k];
    }
    if(b2->word_count > b1->word_count)
    {
        size_t oldsize = b1->word_count;
        if(!mu_bitset_resize_words(b1, b2->word_count))
            return false;
        memcpy(b1->array + oldsize, b2->array + oldsize, (b2->word_count - oldsize) * sizeof(uint64_t));
    }
    return true;
}

size_t mu_bitset_symmetric_difference_count(const mu_bitset* MU_RESTRICT b1, const mu_bitset* MU_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    size_t answer    = 0;
    for(; k < minlength; ++k)
    {
        answer += mu_popcount_u64(b1->array[k] ^ b2->array[k]);
    }
    if(b2->word_count > b1->word_count)
    {
        for(; k < b2->word_count; ++k)
        {
            answer += mu_popcount_u64(b2->array[k]);
        }
    }
    else
    {
        for(; k < b1->word_count; ++k)
        {
            answer += mu_popcount_u64(b1->array[k]);
        }
    }
    return answer;
}

bool mu_bitset_xor(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;
    size_t max_size = a->word_count > b->word_count ? a->word_count : b->word_count;

    if(!mu_bitset_resize_words(out, max_size))
    {
        return false;
    }

    size_t k = 0;
    for(; k < min_size; ++k)
    {
        out->array[k] = a->array[k] ^ b->array[k];
    }

    if(a->word_count > b->word_count)
    {
        for(; k < max_size; ++k)
        {
            out->array[k] = a->array[k];
        }
    }
    else
    {
        for(; k < max_size; ++k)
        {
            out->array[k] = b->array[k];
        }
    }
    return true;
}

bool mu_bitset_or(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;
    size_t max_size = a->word_count > b->word_count ? a->word_count : b->word_count;

    if(!mu_bitset_resize_words(out, max_size))
    {
        return false;
    }

    size_t k = 0;
    for(; k < min_size; ++k)
    {
        out->array[k] = a->array[k] | b->array[k];
    }

    if(a->word_count > b->word_count)
    {
        for(; k < max_size; ++k)
        {
            out->array[k] = a->array[k];
        }
    }
    else
    {
        for(; k < max_size; ++k)
        {
            out->array[k] = b->array[k];
        }
    }
    return true;
}

bool mu_bitset_and(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;
    size_t max_size = a->word_count > b->word_count ? a->word_count : b->word_count;

    if(!mu_bitset_resize_words(out, max_size))
    {
        return false;
    }

    size_t k = 0;
    for(; k < min_size; ++k)
    {
        out->array[k] = a->array[k] & b->array[k];
    }

    for(; k < max_size; ++k)
    {
        out->array[k] = 0;
    }
    return true;
}

bool mu_bitset_not(mu_bitset* MU_RESTRICT out, const mu_bitset* MU_RESTRICT a)
{
    if(!mu_bitset_resize_words(out, a->word_count))
    {
        return false;
    }

    for(size_t k = 0; k < a->word_count; ++k)
    {
        out->array[k] = ~a->array[k];
    }
    return true;
}

bool mu_bitset_xor_assign(mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    return mu_bitset_inplace_symmetric_difference(a, b);
}

bool mu_bitset_or_assign(mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    return mu_bitset_inplace_union(a, b);
}

bool mu_bitset_and_assign(mu_bitset* MU_RESTRICT a, const mu_bitset* MU_RESTRICT b)
{
    mu_bitset_inplace_intersection(a, b);
    return true;
}

void mu_bitset_traverse(const mu_bitset* bs, mu_bitset_visit_fn visitor, void* user)
{
    size_t i = 0;
    while(mu_bitset_next_set_bit(bs, &i))
    {
        if(!visitor(i, user))
        {
            return;
        }
        ++i;
    }
}

void mu_bitset_traverse_range(const mu_bitset* bs, mu_bitset_visit_fn visitor, void* user, size_t begin, size_t count)
{
    if(count == 0)
    {
        return;
    }

    size_t end = begin + count;
    if(end < begin)
    {
        end = SIZE_MAX;
    }

    size_t i = begin;
    while(mu_bitset_next_set_bit(bs, &i) && i < end)
    {
        if(!visitor(i, user))
        {
            return;
        }
        ++i;
    }
}

bool mu_bitset_trim(mu_bitset* bs)
{
    size_t newsize = bs->word_count;
    while(newsize > 0)
    {
        if(bs->array[newsize - 1] == 0)
            newsize -= 1;
        else
            break;
    }
    if(bs->word_capacity == newsize)
        return true;

    uint64_t* newarray;
    if((newarray = (uint64_t*)realloc(bs->array, sizeof(uint64_t) * newsize)) == NULL)
    {
        return false;
    }
    bs->array         = newarray;
    bs->word_capacity = newsize;
    bs->word_count    = newsize;
    return true;
}

void mu_bitset_print(const mu_bitset* b)
{
    printf("{");
    for(size_t i = 0; mu_bitset_next_set_bit(b, &i); i++)
    {
        printf("%zu, ", i);
    }
    printf("}");
}

enum
{
    MU_MULTI_INDEX_MAP_EMPTY = 0,
    MU_MULTI_INDEX_MAP_USED  = 1,
    MU_MULTI_INDEX_MAP_TOMB  = 2,
};

static uint32_t mu_multi_index_next_pow2_u32(uint32_t value)
{
    uint32_t n = 1;
    while(n < value)
        n <<= 1;
    return n;
}

static uint32_t mu_multi_index_map_find_slot(const mu_multi_index* index, uint64_t key)
{
    uint32_t cap       = index->map_capacity;
    uint32_t i         = (uint32_t)(hash_u64(key) % cap);
    uint32_t first_tomb = MU_MULTI_INDEX_NONE;

    for(;;)
    {
        uint8_t state = index->map_states[i];
        if(state == MU_MULTI_INDEX_MAP_EMPTY)
            return first_tomb != MU_MULTI_INDEX_NONE ? first_tomb : i;

        if(state == MU_MULTI_INDEX_MAP_TOMB)
        {
            if(first_tomb == MU_MULTI_INDEX_NONE)
                first_tomb = i;
        }
        else if(index->map_keys[i] == key)
        {
            return i;
        }

        i = (i + 1) % cap;
    }
}

static uint32_t mu_multi_index_map_get(const mu_multi_index* index, uint64_t key)
{
    uint32_t cap = index->map_capacity;
    if(cap == 0)
        return MU_MULTI_INDEX_NONE;

    uint32_t i = (uint32_t)(hash_u64(key) % cap);
    for(;;)
    {
        uint8_t state = index->map_states[i];
        if(state == MU_MULTI_INDEX_MAP_EMPTY)
            return MU_MULTI_INDEX_NONE;

        if(state == MU_MULTI_INDEX_MAP_USED && index->map_keys[i] == key)
            return index->map_values[i];

        i = (i + 1) % cap;
    }
}

static bool mu_multi_index_map_resize(mu_multi_index* index, uint32_t new_capacity)
{
    uint64_t* new_keys   = (uint64_t*)malloc(sizeof(uint64_t) * new_capacity);
    uint32_t* new_values = (uint32_t*)malloc(sizeof(uint32_t) * new_capacity);
    uint8_t*  new_states = (uint8_t*)malloc(sizeof(uint8_t) * new_capacity);

    if(!new_keys || !new_values || !new_states)
    {
        free(new_keys);
        free(new_values);
        free(new_states);
        return false;
    }

    memset(new_states, MU_MULTI_INDEX_MAP_EMPTY, new_capacity * sizeof(uint8_t));

    uint64_t* old_keys    = index->map_keys;
    uint32_t* old_values  = index->map_values;
    uint8_t*  old_states  = index->map_states;
    uint32_t  old_capacity = index->map_capacity;

    index->map_keys     = new_keys;
    index->map_values   = new_values;
    index->map_states   = new_states;
    index->map_capacity = new_capacity;
    index->map_count    = 0;

    for(uint32_t i = 0; i < old_capacity; ++i)
    {
        if(old_states[i] == MU_MULTI_INDEX_MAP_USED)
        {
            uint32_t slot             = mu_multi_index_map_find_slot(index, old_keys[i]);
            index->map_keys[slot]     = old_keys[i];
            index->map_values[slot]   = old_values[i];
            index->map_states[slot]   = MU_MULTI_INDEX_MAP_USED;
            index->map_count         += 1;
        }
    }

    free(old_keys);
    free(old_values);
    free(old_states);
    return true;
}

static bool mu_multi_index_map_set(mu_multi_index* index, uint64_t key, uint32_t value)
{
    if(index->map_capacity == 0)
    {
        if(!mu_multi_index_map_resize(index, 16))
            return false;
    }

    if((index->map_count + 1) * 10 >= index->map_capacity * 7)
    {
        if(!mu_multi_index_map_resize(index, index->map_capacity * 2))
            return false;
    }

    uint32_t slot = mu_multi_index_map_find_slot(index, key);
    if(index->map_states[slot] != MU_MULTI_INDEX_MAP_USED)
    {
        index->map_count += 1;
    }

    index->map_keys[slot]   = key;
    index->map_values[slot] = value;
    index->map_states[slot] = MU_MULTI_INDEX_MAP_USED;
    return true;
}

static void mu_multi_index_map_remove(mu_multi_index* index, uint64_t key)
{
    uint32_t cap = index->map_capacity;
    if(cap == 0)
        return;

    uint32_t i = (uint32_t)(hash_u64(key) % cap);
    for(;;)
    {
        uint8_t state = index->map_states[i];
        if(state == MU_MULTI_INDEX_MAP_EMPTY)
            return;

        if(state == MU_MULTI_INDEX_MAP_USED && index->map_keys[i] == key)
        {
            index->map_states[i] = MU_MULTI_INDEX_MAP_TOMB;
            index->map_count -= 1;
            return;
        }

        i = (i + 1) % cap;
    }
}

static bool mu_multi_index_grow_nodes(mu_multi_index* index, uint32_t min_capacity)
{
    uint32_t new_capacity = index->node_capacity ? index->node_capacity * 2 : 64;
    while(new_capacity < min_capacity)
        new_capacity *= 2;

    mu_multi_index_node* new_nodes =
        (mu_multi_index_node*)realloc(index->nodes, sizeof(mu_multi_index_node) * new_capacity);
    if(!new_nodes)
        return false;

    index->nodes         = new_nodes;
    index->node_capacity = new_capacity;
    return true;
}

void mu_multi_index_init(mu_multi_index* index, uint32_t initial_node_capacity, uint32_t initial_map_capacity)
{
    memset(index, 0, sizeof(*index));
    index->free_head = MU_MULTI_INDEX_NONE;

    if(initial_node_capacity)
    {
        uint32_t cap = mu_multi_index_next_pow2_u32(initial_node_capacity);
        index->nodes = (mu_multi_index_node*)malloc(sizeof(mu_multi_index_node) * cap);
        if(index->nodes)
            index->node_capacity = cap;
    }

    if(initial_map_capacity)
    {
        uint32_t cap = mu_multi_index_next_pow2_u32(initial_map_capacity);
        mu_multi_index_map_resize(index, cap < 16 ? 16 : cap);
    }
}

void mu_multi_index_deinit(mu_multi_index* index)
{
    free(index->nodes);
    free(index->map_keys);
    free(index->map_values);
    free(index->map_states);
    memset(index, 0, sizeof(*index));
    index->free_head = MU_MULTI_INDEX_NONE;
}

uint32_t mu_multi_index_add(mu_multi_index* index, uint64_t key, uint32_t value)
{
    uint32_t node_index;

    if(index->free_head != MU_MULTI_INDEX_NONE)
    {
        node_index       = index->free_head;
        index->free_head = index->nodes[node_index].next;
    }
    else
    {
        if(index->node_count == index->node_capacity)
        {
            if(!mu_multi_index_grow_nodes(index, index->node_count + 1))
                return MU_MULTI_INDEX_NONE;
        }

        node_index = index->node_count;
        index->node_count += 1;
    }

    uint32_t first = mu_multi_index_map_get(index, key);
    mu_multi_index_node* node = &index->nodes[node_index];

    node->key   = key;
    node->value = value;
    node->alive = 1;

    if(first == MU_MULTI_INDEX_NONE)
    {
        node->prev = node_index;
        node->next = node_index;
        if(!mu_multi_index_map_set(index, key, node_index))
        {
            node->alive = 0;
            node->next  = index->free_head;
            index->free_head = node_index;
            return MU_MULTI_INDEX_NONE;
        }
    }
    else
    {
        uint32_t second = index->nodes[first].next;
        node->prev = first;
        node->next = second;
        index->nodes[first].next = node_index;
        index->nodes[second].prev = node_index;
    }

    return node_index;
}

bool mu_multi_index_remove(mu_multi_index* index, uint32_t node_index)
{
    if(node_index >= index->node_count)
        return false;

    mu_multi_index_node* node = &index->nodes[node_index];
    if(!node->alive)
        return false;

    uint32_t prev = node->prev;
    uint32_t next = node->next;
    uint64_t key  = node->key;

    index->nodes[prev].next = next;
    index->nodes[next].prev = prev;

    uint32_t first = mu_multi_index_map_get(index, key);
    if(first == node_index)
    {
        if(next == node_index)
            mu_multi_index_map_remove(index, key);
        else
            mu_multi_index_map_set(index, key, next);
    }

    node->alive = 0;
    node->next  = index->free_head;
    index->free_head = node_index;
    return true;
}

uint32_t mu_multi_index_first(const mu_multi_index* index, uint64_t key)
{
    return mu_multi_index_map_get(index, key);
}

uint32_t mu_multi_index_next(const mu_multi_index* index, uint32_t start_node, uint32_t node_index)
{
    if(!mu_multi_index_node_valid(index, node_index))
        return MU_MULTI_INDEX_NONE;
    if(!mu_multi_index_node_valid(index, start_node))
        return MU_MULTI_INDEX_NONE;

    uint32_t next = index->nodes[node_index].next;
    return next == start_node ? MU_MULTI_INDEX_NONE : next;
}

bool mu_multi_index_node_valid(const mu_multi_index* index, uint32_t node_index)
{
    return node_index < index->node_count && index->nodes[node_index].alive != 0;
}

uint32_t mu_multi_index_value(const mu_multi_index* index, uint32_t node_index)
{
    return index->nodes[node_index].value;
}

uint64_t mu_multi_index_key(const mu_multi_index* index, uint32_t node_index)
{
    return index->nodes[node_index].key;
}

uint32_t mu_multi_index_count_key(const mu_multi_index* index, uint64_t key)
{
    uint32_t first = mu_multi_index_first(index, key);
    if(first == MU_MULTI_INDEX_NONE)
        return 0;

    uint32_t count = 0;
    uint32_t idx   = first;
    do
    {
        count += 1;
        idx = index->nodes[idx].next;
    } while(idx != first);

    return count;
}

void mu_multi_index_visit_key(const mu_multi_index* index, uint64_t key, mu_multi_index_visit_fn visitor, void* user)
{
    uint32_t first = mu_multi_index_first(index, key);
    if(first == MU_MULTI_INDEX_NONE)
        return;

    uint32_t idx = first;
    do
    {
        if(!visitor(index->nodes[idx].value, idx, user))
            return;
        idx = index->nodes[idx].next;
    } while(idx != first);
}

static bool mu_chunked_u32_pool_grow(mu_chunked_u32_pool* pool, uint32_t min_capacity)
{
    uint32_t old_capacity = pool->chunk_capacity;
    uint32_t new_capacity = old_capacity ? old_capacity * 2 : 64;
    while(new_capacity < min_capacity)
        new_capacity *= 2;

    mu_chunked_u32_chunk* new_chunks =
        (mu_chunked_u32_chunk*)realloc(pool->chunks, sizeof(mu_chunked_u32_chunk) * new_capacity);
    if(!new_chunks)
        return false;

    pool->chunks = new_chunks;

    for(uint32_t i = old_capacity; i < new_capacity; ++i)
    {
        pool->chunks[i].used      = 0;
        pool->chunks[i].prev_chunk = MU_CHUNKED_U32_NONE;
        pool->chunks[i].next_chunk = MU_CHUNKED_U32_NONE;
        pool->chunks[i].free_next  = (i + 1 < new_capacity) ? (i + 1) : pool->free_head;
    }

    pool->free_head = old_capacity;
    pool->chunk_capacity = new_capacity;
    return true;
}

static uint32_t mu_chunked_u32_pool_alloc_chunk(mu_chunked_u32_pool* pool)
{
    if(pool->free_head == MU_CHUNKED_U32_NONE)
    {
        if(!mu_chunked_u32_pool_grow(pool, pool->chunk_capacity + 1))
            return MU_CHUNKED_U32_NONE;
    }

    uint32_t chunk_index = pool->free_head;
    mu_chunked_u32_chunk* chunk = &pool->chunks[chunk_index];
    pool->free_head = chunk->free_next;

    chunk->used = 0;
    chunk->prev_chunk = MU_CHUNKED_U32_NONE;
    chunk->next_chunk = MU_CHUNKED_U32_NONE;
    chunk->free_next  = MU_CHUNKED_U32_NONE;

    if(chunk_index >= pool->chunk_count)
        pool->chunk_count = chunk_index + 1;

    return chunk_index;
}

static void mu_chunked_u32_pool_free_chunk(mu_chunked_u32_pool* pool, uint32_t chunk_index)
{
    mu_chunked_u32_chunk* chunk = &pool->chunks[chunk_index];
    chunk->used      = 0;
    chunk->prev_chunk = MU_CHUNKED_U32_NONE;
    chunk->next_chunk = MU_CHUNKED_U32_NONE;
    chunk->free_next  = pool->free_head;
    pool->free_head  = chunk_index;
}

void mu_chunked_u32_pool_init(mu_chunked_u32_pool* pool, uint32_t initial_capacity)
{
    memset(pool, 0, sizeof(*pool));
    pool->free_head = MU_CHUNKED_U32_NONE;

    if(initial_capacity)
        mu_chunked_u32_pool_grow(pool, initial_capacity);
}

void mu_chunked_u32_pool_deinit(mu_chunked_u32_pool* pool)
{
    free(pool->chunks);
    memset(pool, 0, sizeof(*pool));
    pool->free_head = MU_CHUNKED_U32_NONE;
}

void mu_chunked_u32_array_init(mu_chunked_u32_array* array)
{
    array->first_chunk = MU_CHUNKED_U32_NONE;
    array->last_chunk  = MU_CHUNKED_U32_NONE;
    array->count       = 0;
}

void mu_chunked_u32_array_clear(mu_chunked_u32_pool* pool, mu_chunked_u32_array* array)
{
    uint32_t chunk_index = array->first_chunk;
    while(chunk_index != MU_CHUNKED_U32_NONE)
    {
        uint32_t next = pool->chunks[chunk_index].next_chunk;
        mu_chunked_u32_pool_free_chunk(pool, chunk_index);
        chunk_index = next;
    }

    mu_chunked_u32_array_init(array);
}

bool mu_chunked_u32_array_push(mu_chunked_u32_pool* pool, mu_chunked_u32_array* array, uint32_t value)
{
    if(array->last_chunk == MU_CHUNKED_U32_NONE)
    {
        uint32_t first = mu_chunked_u32_pool_alloc_chunk(pool);
        if(first == MU_CHUNKED_U32_NONE)
            return false;

        array->first_chunk = first;
        array->last_chunk  = first;
    }

    uint32_t last_index = array->last_chunk;
    mu_chunked_u32_chunk* last = &pool->chunks[last_index];
    if(last->used == MU_ARRAY_OF_ARRAYS_CHUNK_SIZE)
    {
        uint32_t next = mu_chunked_u32_pool_alloc_chunk(pool);
        if(next == MU_CHUNKED_U32_NONE)
            return false;

        pool->chunks[next].prev_chunk = last_index;
        pool->chunks[last_index].next_chunk = next;
        array->last_chunk = next;
        last = &pool->chunks[next];
    }

    last->values[last->used] = value;
    last->used += 1;
    array->count += 1;
    return true;
}

bool mu_chunked_u32_array_pop(mu_chunked_u32_pool* pool, mu_chunked_u32_array* array, uint32_t* out_value)
{
    if(array->count == 0)
        return false;

    uint32_t last_index = array->last_chunk;
    mu_chunked_u32_chunk* last = &pool->chunks[last_index];
    last->used -= 1;

    if(out_value)
        *out_value = last->values[last->used];

    array->count -= 1;

    if(last->used == 0)
    {
        uint32_t prev = last->prev_chunk;
        if(prev != MU_CHUNKED_U32_NONE)
        {
            pool->chunks[prev].next_chunk = MU_CHUNKED_U32_NONE;
            array->last_chunk = prev;
        }
        else
        {
            array->first_chunk = MU_CHUNKED_U32_NONE;
            array->last_chunk  = MU_CHUNKED_U32_NONE;
        }

        mu_chunked_u32_pool_free_chunk(pool, last_index);
    }

    return true;
}

bool mu_chunked_u32_array_get(const mu_chunked_u32_pool* pool, const mu_chunked_u32_array* array, uint32_t index,
    uint32_t* out_value)
{
    if(index >= array->count)
        return false;

    uint32_t chunk_index = array->first_chunk;
    uint32_t local_index = index;
    while(chunk_index != MU_CHUNKED_U32_NONE)
    {
        const mu_chunked_u32_chunk* chunk = &pool->chunks[chunk_index];
        if(local_index < chunk->used)
        {
            *out_value = chunk->values[local_index];
            return true;
        }

        local_index -= chunk->used;
        chunk_index = chunk->next_chunk;
    }

    return false;
}

void mu_chunked_u32_array_visit(
    const mu_chunked_u32_pool* pool, const mu_chunked_u32_array* array, mu_chunked_u32_visit_fn visitor, void* user)
{
    uint32_t chunk_index = array->first_chunk;
    while(chunk_index != MU_CHUNKED_U32_NONE)
    {
        const mu_chunked_u32_chunk* chunk = &pool->chunks[chunk_index];
        for(uint32_t i = 0; i < chunk->used; ++i)
        {
            if(!visitor(chunk->values[i], user))
                return;
        }
        chunk_index = chunk->next_chunk;
    }
}

static MU_INLINE uint32_t* mu_bulk_storage_free_header(mu_bulk_storage* storage)
{
    return (uint32_t*)(storage->slots + storage->slot_size * 0u);
}

static MU_INLINE uint32_t* mu_bulk_storage_slot_next_ptr(mu_bulk_storage* storage, uint32_t id)
{
    return (uint32_t*)(storage->slots + storage->slot_size * id);
}

static bool mu_bulk_storage_grow(mu_bulk_storage* storage, uint32_t min_capacity)
{
    uint32_t new_capacity = storage->slot_capacity ? storage->slot_capacity * 2u : 64u;
    while(new_capacity < min_capacity)
        new_capacity *= 2u;

    uint8_t*  new_slots = (uint8_t*)realloc(storage->slots, storage->slot_size * (size_t)new_capacity);
    uint32_t* new_generations = (uint32_t*)realloc(storage->generations, sizeof(uint32_t) * (size_t)new_capacity);
    uint8_t*  new_live = (uint8_t*)realloc(storage->live, sizeof(uint8_t) * (size_t)new_capacity);

    if(!new_slots || !new_generations || !new_live)
    {
        return false;
    }

    uint32_t old_capacity = storage->slot_capacity;
    storage->slots        = new_slots;
    storage->generations  = new_generations;
    storage->live         = new_live;
    storage->slot_capacity = new_capacity;

    memset(storage->slots + storage->slot_size * (size_t)old_capacity, 0,
        storage->slot_size * (size_t)(new_capacity - old_capacity));
    memset(storage->generations + old_capacity, 0, sizeof(uint32_t) * (size_t)(new_capacity - old_capacity));
    memset(storage->live + old_capacity, 0, sizeof(uint8_t) * (size_t)(new_capacity - old_capacity));

    return true;
}

bool mu_bulk_storage_init(mu_bulk_storage* storage, size_t slot_size, uint32_t initial_slot_capacity)
{
    memset(storage, 0, sizeof(*storage));
    if(slot_size < sizeof(uint32_t))
        return false;

    storage->slot_size = slot_size;

    uint32_t cap = initial_slot_capacity < 2u ? 2u : initial_slot_capacity;
    if(!mu_bulk_storage_grow(storage, cap))
        return false;

    storage->next_unused = 1u;
    *mu_bulk_storage_free_header(storage) = 0u;
    return true;
}

void mu_bulk_storage_deinit(mu_bulk_storage* storage)
{
    free(storage->slots);
    free(storage->generations);
    free(storage->live);
    memset(storage, 0, sizeof(*storage));
}

uint32_t mu_bulk_storage_alloc(mu_bulk_storage* storage)
{
    uint32_t id = *mu_bulk_storage_free_header(storage);
    if(id != 0u)
    {
        *mu_bulk_storage_free_header(storage) = *mu_bulk_storage_slot_next_ptr(storage, id);
    }
    else
    {
        id = storage->next_unused;
        if(id >= storage->slot_capacity)
        {
            if(!mu_bulk_storage_grow(storage, storage->slot_capacity + 1u))
                return 0u;
        }
        storage->next_unused = id + 1u;
        if(storage->generations[id] == 0u)
            storage->generations[id] = 1u;
    }

    storage->live[id] = 1u;
    storage->live_count += 1u;
    return id;
}

bool mu_bulk_storage_free(mu_bulk_storage* storage, uint32_t id)
{
    if(id == 0u || id >= storage->next_unused)
        return false;
    if(!storage->live[id])
        return false;

    storage->live[id] = 0u;
    storage->live_count -= 1u;

    storage->generations[id] += 1u;
    if(storage->generations[id] == 0u)
        storage->generations[id] = 1u;

    uint32_t* slot_next = mu_bulk_storage_slot_next_ptr(storage, id);
    *slot_next = *mu_bulk_storage_free_header(storage);
    *mu_bulk_storage_free_header(storage) = id;
    return true;
}

void* mu_bulk_storage_ptr(mu_bulk_storage* storage, uint32_t id)
{
    if(id == 0u || id >= storage->next_unused)
        return NULL;
    if(!storage->live[id])
        return NULL;
    return storage->slots + storage->slot_size * (size_t)id;
}

const void* mu_bulk_storage_ptr_const(const mu_bulk_storage* storage, uint32_t id)
{
    if(id == 0u || id >= storage->next_unused)
        return NULL;
    if(!storage->live[id])
        return NULL;
    return storage->slots + storage->slot_size * (size_t)id;
}

bool mu_bulk_storage_is_live(const mu_bulk_storage* storage, uint32_t id)
{
    if(id == 0u || id >= storage->next_unused)
        return false;
    return storage->live[id] != 0u;
}

mu_weak_handle mu_bulk_storage_make_handle(const mu_bulk_storage* storage, uint32_t id)
{
    mu_weak_handle handle;
    handle.id         = id;
    handle.generation = (id < storage->next_unused) ? storage->generations[id] : 0u;
    return handle;
}

bool mu_bulk_storage_validate_handle(const mu_bulk_storage* storage, mu_weak_handle handle)
{
    if(handle.id == 0u || handle.id >= storage->next_unused)
        return false;
    if(!storage->live[handle.id])
        return false;
    return storage->generations[handle.id] == handle.generation;
}

void* mu_bulk_storage_resolve_handle(mu_bulk_storage* storage, mu_weak_handle handle)
{
    if(!mu_bulk_storage_validate_handle(storage, handle))
        return NULL;
    return storage->slots + storage->slot_size * (size_t)handle.id;
}

const void* mu_bulk_storage_resolve_handle_const(const mu_bulk_storage* storage, mu_weak_handle handle)
{
    if(!mu_bulk_storage_validate_handle(storage, handle))
        return NULL;
    return storage->slots + storage->slot_size * (size_t)handle.id;
}

void mu_bulk_storage_visit_live(mu_bulk_storage* storage, mu_bulk_storage_visit_fn visitor, void* user)
{
    for(uint32_t id = 1u; id < storage->next_unused; ++id)
    {
        if(!storage->live[id])
            continue;

        void* slot_ptr = storage->slots + storage->slot_size * (size_t)id;
        if(!visitor(id, slot_ptr, user))
            return;
    }
}


bool mu_id_pool_is_id(const mu_id_pool* pool, uint32_t id)
{
    uint32_t i0 = 0;
    uint32_t i1 = pool->count - 1;

    for(;;)
    {
        uint32_t i = (i0 + i1) / 2;

        if(id < pool->ranges[i].first)
        {
            if(i == i0)
                return true;

            i1 = i - 1;
        }
        else if(id > pool->ranges[i].last)
        {
            if(i == i1)
                return true;

            i0 = i + 1;
        }
        else
        {
            return false;
        }
    }
}

uint32_t mu_id_pool_get_available_ids(const mu_id_pool* pool)
{
    uint32_t count = pool->count;

    for(uint32_t i = 0; i < pool->count; ++i)
    {
        count += pool->ranges[i].last - pool->ranges[i].first;
    }

    return count;
}
uint32_t mu_id_pool_get_largest_continuous_range(const mu_id_pool* pool)
{
    uint32_t max_count = 0;

    for(uint32_t i = 0; i < pool->count; ++i)
    {
        uint32_t count = pool->ranges[i].last - pool->ranges[i].first + 1;

        if(count > max_count)
            max_count = count;
    }

    return max_count;
}
/* internal helpers */

static void mu_id_pool_insert_range(mu_id_pool* pool, uint32_t index)
{
    if(pool->count >= pool->capacity)
    {
        pool->capacity = pool->capacity ? pool->capacity * 2 : 1;
        pool->ranges   = (mu_id_pool_range*)realloc(pool->ranges, pool->capacity * sizeof(mu_id_pool_range));
        assert(pool->ranges);
    }

    memmove(pool->ranges + index + 1, pool->ranges + index, (pool->count - index) * sizeof(mu_id_pool_range));

    pool->count++;
}

static void mu_id_pool_destroy_range(mu_id_pool* pool, uint32_t index)
{
    pool->count--;

    memmove(pool->ranges + index, pool->ranges + index + 1, (pool->count - index) * sizeof(mu_id_pool_range));
}

/* public API */

void mu_id_pool_init(mu_id_pool* pool, uint32_t pool_size)
{
    assert(pool);
    assert(!pool->ranges);
    assert(pool_size);

    uint32_t max_id = pool_size - 1;

    pool->ranges = (mu_id_pool_range*)malloc(sizeof(mu_id_pool_range));
    assert(pool->ranges);

    pool->ranges[0].first = 0;
    pool->ranges[0].last  = max_id;

    pool->count    = 1;
    pool->capacity = 1;
    pool->max_id   = max_id;
    pool->used_ids = 0;
}

void mu_id_pool_deinit(mu_id_pool* pool)
{
    assert(pool);
    assert(pool->used_ids == 0);

    if(pool->ranges)
    {
        free(pool->ranges);
        pool->ranges   = NULL;
        pool->count    = 0;
        pool->capacity = 0;
        pool->max_id   = 0;
        pool->used_ids = 0;
    }
}

void mu_id_pool_destroy_all(mu_id_pool* pool)
{
    uint32_t pool_size = pool->max_id + 1;
    pool->used_ids     = 0;

    mu_id_pool_deinit(pool);
    mu_id_pool_init(pool, pool_size);
}

bool mu_id_pool_create_id(mu_id_pool* pool, uint32_t* out_id)
{
    if(pool->ranges[0].first <= pool->ranges[0].last)
    {
        *out_id = pool->ranges[0].first;

        if(pool->ranges[0].first == pool->ranges[0].last && pool->count > 1)
        {
            mu_id_pool_destroy_range(pool, 0);
        }
        else
        {
            pool->ranges[0].first++;
        }

        pool->used_ids++;
        return true;
    }

    return false;
}

bool mu_id_pool_create_range_id(mu_id_pool* pool, uint32_t* out_id, uint32_t count)
{
    for(uint32_t i = 0; i < pool->count; ++i)
    {
        uint32_t range_count = 1 + pool->ranges[i].last - pool->ranges[i].first;

        if(count <= range_count)
        {
            *out_id = pool->ranges[i].first;

            if(count == range_count && i + 1 < pool->count)
            {
                mu_id_pool_destroy_range(pool, i);
            }
            else
            {
                pool->ranges[i].first += count;
            }

            pool->used_ids += count;
            return true;
        }
    }

    return false;
}

bool mu_id_pool_destroy_id(mu_id_pool* pool, uint32_t id)
{
    return mu_id_pool_destroy_range_id(pool, id, 1);
}

bool mu_id_pool_destroy_range_id(mu_id_pool* pool, uint32_t id, uint32_t count)
{
    uint32_t end_id = id + count;
    assert(end_id <= pool->max_id + 1);

    uint32_t i0 = 0;
    uint32_t i1 = pool->count - 1;

    for(;;)
    {
        uint32_t i = (i0 + i1) / 2;

        if(id < pool->ranges[i].first)
        {
            if(end_id >= pool->ranges[i].first)
            {
                if(end_id != pool->ranges[i].first)
                    return false;

                if(i > i0 && id - 1 == pool->ranges[i - 1].last)
                {
                    pool->ranges[i - 1].last = pool->ranges[i].last;
                    mu_id_pool_destroy_range(pool, i);
                }
                else
                {
                    pool->ranges[i].first = id;
                }

                pool->used_ids -= count;
                return true;
            }

            if(i != i0)
            {
                i1 = i - 1;
            }
            else
            {
                mu_id_pool_insert_range(pool, i);
                pool->ranges[i].first = id;
                pool->ranges[i].last  = end_id - 1;

                pool->used_ids -= count;
                return true;
            }
        }
        else if(id > pool->ranges[i].last)
        {
            if(id - 1 == pool->ranges[i].last)
            {
                if(i < i1 && end_id == pool->ranges[i + 1].first)
                {
                    pool->ranges[i].last = pool->ranges[i + 1].last;
                    mu_id_pool_destroy_range(pool, i + 1);
                }
                else
                {
                    pool->ranges[i].last += count;
                }

                pool->used_ids -= count;
                return true;
            }

            if(i != i1)
            {
                i0 = i + 1;
            }
            else
            {
                mu_id_pool_insert_range(pool, i + 1);
                pool->ranges[i + 1].first = id;
                pool->ranges[i + 1].last  = end_id - 1;

                pool->used_ids -= count;
                return true;
            }
        }
        else
        {
            return false;
        }
    }
}

bool mu_id_pool_is_range_available(const mu_id_pool* pool, uint32_t search_count)
{
    for(uint32_t i = 0; i < pool->count; ++i)
    {
        uint32_t count = pool->ranges[i].last - pool->ranges[i].first + 1;

        if(count >= search_count)
            return true;
    }

    return false;
}

void mu_id_pool_print_ranges(const mu_id_pool* pool)
{
    for(uint32_t i = 0; i < pool->count; ++i)
    {
        if(pool->ranges[i].first < pool->ranges[i].last)
            printf("%u-%u", pool->ranges[i].first, pool->ranges[i].last);
        else if(pool->ranges[i].first == pool->ranges[i].last)
            printf("%u", pool->ranges[i].first);
        else
            printf("-");

        if(i + 1 < pool->count)
            printf(", ");
    }

    printf("\n");
}

void mu_id_pool_check_ranges(const mu_id_pool* pool)
{
    for(uint32_t i = 0; i < pool->count; ++i)
    {
        assert(pool->ranges[i].last <= pool->max_id);

        if(pool->ranges[i].first == pool->ranges[i].last + 1)
            continue;

        assert(pool->ranges[i].first <= pool->ranges[i].last);
        assert(pool->ranges[i].first <= pool->max_id);
    }
}


/// --------------------------------------------------------------------

void mu_list_init(mu_list* list)
{
    list->head = NULL;
}

void mu_list_clear(mu_list* list)
{
    mu_list_node* curr = list->head;

    while(curr)
    {
        mu_list_node* next = curr->next;
        free(curr);
        curr = next;
    }

    list->head = NULL;
}

void mu_list_push_front(mu_list* list, int value)
{
    mu_list_node* n = (mu_list_node*)malloc(sizeof(mu_list_node));
    n->value        = value;
    n->next         = list->head;
    list->head      = n;
}

void mu_list_push_back(mu_list* list, int value)
{
    mu_list_node** pp = &list->head;

    while(*pp)
        pp = &(*pp)->next;

    mu_list_node* n = (mu_list_node*)malloc(sizeof(mu_list_node));
    n->value        = value;
    n->next         = NULL;

    *pp = n;
}


int mu_list_remove_first(mu_list* list, int value)
{
    mu_list_node** pp = &list->head;

    while(*pp && (*pp)->value != value)
        pp = &(*pp)->next;

    if(*pp)
    {
        mu_list_node* victim = *pp;
        *pp                  = victim->next;
        free(victim);
        return 1;
    }

    return 0;
}


int mu_list_remove_all(mu_list* list, int value)
{
    mu_list_node** pp      = &list->head;
    int            removed = 0;

    while(*pp)
    {
        if((*pp)->value == value)
        {
            mu_list_node* victim = *pp;
            *pp                  = victim->next;
            free(victim);
            removed++;
        }
        else
        {
            pp = &(*pp)->next;
        }
    }

    return removed;
}


mu_list_node* mu_list_find(mu_list* list, int value)
{
    mu_list_node* curr = list->head;

    while(curr)
    {
        if(curr->value == value)
            return curr;

        curr = curr->next;
    }

    return NULL;
}

size_t mu_list_length(const mu_list* list)
{
    size_t        count = 0;
    mu_list_node* curr  = list->head;

    while(curr)
    {
        count++;
        curr = curr->next;
    }

    return count;
}

void mu_list_reverse(mu_list* list)
{
    mu_list_node* prev = NULL;
    mu_list_node* curr = list->head;

    while(curr)
    {
        mu_list_node* next = curr->next;
        curr->next         = prev;
        prev               = curr;
        curr               = next;
    }

    list->head = prev;
}

void mu_list_print(const mu_list* list)
{
    mu_list_node* curr = list->head;

    while(curr)
    {
        printf("%d ", curr->value);
        curr = curr->next;
    }

    printf("\n");
}


void mu_sparse_set_init(mu_sparse_set* set, uint32_t* dense_buffer, uint32_t* sparse_buffer, uint32_t capacity)
{
    set->dense    = dense_buffer;
    set->sparse   = sparse_buffer;
    set->size     = 0;
    set->capacity = capacity;
}

/* O(1) */
void mu_sparse_set_clear(mu_sparse_set* set)
{
    set->size = 0;
}

/* O(1) */
bool mu_sparse_set_contains(const mu_sparse_set* set, uint32_t value)
{
    if(value >= set->capacity)
        return 0;

    uint32_t idx = set->sparse[value];

    return (idx < set->size) && (set->dense[idx] == value);
}

/* O(1) */
bool mu_sparse_set_add(mu_sparse_set* set, uint32_t value)
{
    assert(value < set->capacity);

    if(mu_sparse_set_contains(set, value))
        return 0;  // already present

    assert(set->size < set->capacity);

    uint32_t idx = set->size;

    set->dense[idx]    = value;
    set->sparse[value] = idx;
    set->size++;

    return 1;
}

/* O(1) swap-remove */
bool mu_sparse_set_remove(mu_sparse_set* set, uint32_t value)
{
    if(!mu_sparse_set_contains(set, value))
        return 0;

    uint32_t idx      = set->sparse[value];
    uint32_t last_idx = set->size - 1;
    uint32_t last_val = set->dense[last_idx];

    /* Move last element into removed slot */
    set->dense[idx]       = last_val;
    set->sparse[last_val] = idx;

    set->size--;

    return 1;
}


void mu_circular_list_init(mu_circular_list* list)
{
    list->last = NULL;
    list->size = 0;
}

void mu_circular_list_clear(mu_circular_list* list)
{
    if(!list->last)
        return;

    mu_list_node* first = list->last->next;
    mu_list_node* curr  = first;

    do
    {
        mu_list_node* next = curr->next;
        free(curr);
        curr = next;
    } while(curr != first);

    list->last = NULL;
    list->size = 0;
}

void mu_circular_list_push_front(mu_circular_list* list, int value)
{
    mu_list_node* n = (mu_list_node*)malloc(sizeof(mu_list_node));
    n->value        = value;
    if(!list->last)
    {

        /*
 
We’re inserting the first node ever.
In a circular list with one node:
That node must point to itself.
   */


        n->next    = n;
        list->last = n;
    }
    else
    {
        n->next          = list->last->next;
        list->last->next = n;
    }

    list->size++;
}
void mu_circular_list_push_back(mu_circular_list* list, int value)
{
    mu_circular_list_push_front(list, value);
    list->last = list->last->next;
}
int mu_circular_list_pop_front(mu_circular_list* list, int* out_value)
{
    if(!list->last)
        return 0;

    mu_list_node* first = list->last->next;
    *out_value          = first->value;

    if(first == list->last)
    {
        free(first);
        list->last = NULL;
    }
    else
    {
        list->last->next = first->next;
        free(first);
    }

    list->size--;
    return 1;
}

int mu_circular_list_remove_first(mu_circular_list* list, int value)
{
    if(!list->last)
        return 0;

    mu_list_node* prev  = list->last;
    mu_list_node* curr  = list->last->next;
    mu_list_node* first = curr;

    do
    {
        if(curr->value == value)
        {
            if(curr == prev)  // single node
            {
                free(curr);
                list->last = NULL;
            }
            else
            {
                prev->next = curr->next;
                if(curr == list->last)
                    list->last = prev;

                free(curr);
            }

            list->size--;
            return 1;
        }

        prev = curr;
        curr = curr->next;

    } while(curr != first);

    return 0;
}
mu_list_node* mu_circular_list_find(mu_circular_list* list, int value)
{
    if(!list->last)
        return NULL;

    mu_list_node* curr  = list->last->next;
    mu_list_node* first = curr;

    do
    {
        if(curr->value == value)
            return curr;

        curr = curr->next;

    } while(curr != first);

    return NULL;
}
void mu_circular_list_print(const mu_circular_list* list)
{
    if(!list->last)
    {
        printf("(empty)\n");
        return;
    }

    mu_list_node* curr  = list->last->next;
    mu_list_node* first = curr;

    do
    {
        printf("%d ", curr->value);
        curr = curr->next;
    } while(curr != first);

    printf("\n");
}

#endif /* MU_IMPLEMENTATION */

#endif /* MU_H */
