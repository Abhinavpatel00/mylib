#ifndef MU_COMMON_H
#define MU_COMMON_H

#pragma  once 
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef MU_API
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
#define MU_NOOP() do { } while(0)
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

#if defined(_MSC_VER)
#define MU_RESTRICT __restrict
#define MU_INLINE __forceinline
#define MU_NOINLINE __declspec(noinline)
#define MU_ALIGN(N) __declspec(align(N))
#define MU_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define MU_RESTRICT restrict
#define MU_INLINE inline __attribute__((always_inline))
#define MU_NOINLINE __attribute__((noinline))
#define MU_ALIGN(N) __attribute__((aligned(N)))
#define MU_DEBUG_BREAK() __builtin_trap()
#else
#define MU_RESTRICT
#define MU_INLINE inline
#define MU_NOINLINE
#define MU_ALIGN(N)
#define MU_DEBUG_BREAK() (*(volatile int*)0 = 0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MU_LIKELY(x) __builtin_expect(!!(x), 1)
#define MU_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define MU_PREFETCH(addr) __builtin_prefetch(addr)
#else
#define MU_LIKELY(x) (x)
#define MU_UNLIKELY(x) (x)
#define MU_PREFETCH(addr)
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

#define MU_OFFSET_OF(type, member) ((size_t)&(((type*)0)->member))
#define MU_CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - MU_OFFSET_OF(type, member)))

#if defined(__GNUC__) || defined(__clang__)
#define MU_MIN(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#define MU_MAX(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })
#else
#define MU_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MU_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define MU_ASSERT(x) do { if(!(x)) MU_DEBUG_BREAK(); } while(0)
#define MU_PANIC() do { MU_DEBUG_BREAK(); *(volatile int*)0 = 0; } while(0)

#define MU_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define MU_IS_POW2(x) (((x) & ((x) - 1)) == 0)
#define MU_KB(x) ((x) * 1024ull)
#define MU_MB(x) (MU_KB(x) * 1024ull)
#define MU_GB(x) (MU_MB(x) * 1024ull)
#define MU_CEIL(x, y) (((x) + (y) - 1) / (y))
#define MU_FLOOR(x, y) ((x) / (y))
#define MU_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define MU_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define MU_CLAMP(x, lo, hi) (MU_MIN(MU_MAX((x), (lo)), (hi)))
#define MU_BIT(n) (1ull << (n))
#define MU_HAS_FLAG(x, flag) (((x) & (flag)) != 0)
#define MU_SET_FLAG(x, flag) ((x) |= (flag))
#define MU_CLEAR_FLAG(x, flag) ((x) &= ~(flag))

#define MU_SWAP(TYPE, a, b) do { TYPE mu_t = (a); (a) = (b); (b) = mu_t; } while(0)

#ifdef __cplusplus
#define MU_EXTERN extern "C"
#else
#define MU_EXTERN extern
#endif

MU_BEGIN_EXTERN_C


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






MU_END_EXTERN_C

#endif
