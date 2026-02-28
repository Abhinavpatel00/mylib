// the aim is this lib is to fullfill my personal needs for drop in headers or header + impl to use in new projects i feel c is good language with some drawbacks for that i will  ofcourse designing my own language but i dont think i will make that language for shipping i enjoy programming so it is just for that c99 is good enough for shipping this lib is to make c tolerable and provide good data structures and algos
//

#ifndef FLOW_H
#define FLOW_H

// -----------------------------------------------------------------------------
// STB-style single-header configuration
//
// Usage (one .c/.cpp file):
//   #define FLOW_IMPLEMENTATION
//   #include "flow.h"
// -----------------------------------------------------------------------------

#ifndef FLOW_API
    #if defined(FLOW_STATIC)
        #define FLOW_API static
    #elif defined(_WIN32) && defined(FLOW_DLL_EXPORT)
        #define FLOW_API __declspec(dllexport)
    #elif defined(_WIN32) && defined(FLOW_DLL_IMPORT)
        #define FLOW_API __declspec(dllimport)
    #else
        #define FLOW_API extern
    #endif
#endif

#ifdef __cplusplus
    #define FLOW_EXTERN_C extern "C"
    #define FLOW_BEGIN_EXTERN_C extern "C" {
    #define FLOW_END_EXTERN_C }
#else
    #define FLOW_EXTERN_C extern
    #define FLOW_BEGIN_EXTERN_C
    #define FLOW_END_EXTERN_C
#endif

#ifndef FLOW_UNUSED
    #define FLOW_UNUSED(x) (void)(x)
#endif

#ifndef FLOW_NOOP
    #define FLOW_NOOP() do { } while(0)
#endif

#ifndef FLOW_DEPRECATED
    #if defined(_MSC_VER)
        #define FLOW_DEPRECATED(msg) __declspec(deprecated(msg))
    #elif defined(__GNUC__) || defined(__clang__)
        #define FLOW_DEPRECATED(msg) __attribute__((deprecated(msg)))
    #else
        #define FLOW_DEPRECATED(msg)
    #endif
#endif

FLOW_BEGIN_EXTERN_C

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
#define FLOW_RESTRICT restrict
#elif defined(_MSC_VER)
#define FLOW_RESTRICT __restrict
#else
#define FLOW_RESTRICT
#endif


#if defined(_MSC_VER)
#define FLOW_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FLOW_INLINE inline __attribute__((always_inline))
#else
#define FLOW_INLINE inline
#endif

#if defined(_MSC_VER)
#define FLOW_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define FLOW_NOINLINE __attribute__((noinline))
#else
#define FLOW_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FLOW_LIKELY(x) __builtin_expect(!!(x), 1)
#define FLOW_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FLOW_LIKELY(x) (x)
#define FLOW_UNLIKELY(x) (x)
#endif

#if defined(_MSC_VER)
#define FLOW_ALIGN(N) __declspec(align(N))
#else
#define FLOW_ALIGN(N) __attribute__((aligned(N)))
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define FLOW_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(__cplusplus)
#define FLOW_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define FLOW_STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FLOW_PREFETCH(addr) __builtin_prefetch(addr)
#else
#define FLOW_PREFETCH(addr)
#endif

#if defined(_MSC_VER)
#define FLOW_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define FLOW_DEBUG_BREAK() __builtin_trap()
#else
#define FLOW_DEBUG_BREAK() (*(volatile int*)0 = 0)
#endif
#define FLOW_OFFSET_OF(type, member) ((size_t)&(((type*)0)->member))

// If I have a pointer to a struct member, how do I get a pointer to the struct that contains it?
#define FLOW_CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - FLOW_OFFSET_OF(type, member)))
#if defined(__GNUC__) || defined(__clang__)
#define FLOW_MIN(a, b)                                                                                                 \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a < _b ? _a : _b;                                                                                             \
    })
#define FLOW_MAX(a, b)                                                                                                 \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a > _b ? _a : _b;                                                                                             \
    })
#else
#define FLOW_MIN(a, b) ((a) < (b) ? (a) : (b))
#define FLOW_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


#define FLOW_ASSERT(x)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(x))                                                                                                       \
            FLOW_DEBUG_BREAK();                                                                                        \
    } while(0)

#define FLOW_PANIC()                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        FLOW_DEBUG_BREAK();                                                                                            \
        *(volatile int*)0 = 0;                                                                                         \
    } while(0)


#define for_each(i, count) for(size_t i = 0; i < (count); i++)
#define FLOW_FOR_RANGE(i, begin, end) for(size_t i = (begin); i < (end); i++)
#define FLOW_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define FLOW_IS_POW2(x) (((x) & ((x) - 1)) == 0)
#define FLOW_KB(x) ((x) * 1024ull)
#define FLOW_MB(x) (FLOW_KB(x) * 1024ull)
#define FLOW_GB(x) (FLOW_MB(x) * 1024ull)
#define FLOW_STRINGIFY(x) #x
#define FLOW_TOSTRING(x) FLOW_STRINGIFY(x)

#define FLOW_CONCAT(a, b) a##b
#define FLOW_CONCAT2(a, b) FLOW_CONCAT(a, b)

#ifdef __cplusplus
#define FLOW_EXTERN extern "C"
#else
#define FLOW_EXTERN extern
#endif

#define FLOW_SWAP(TYPE, a, b)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        TYPE flow_t = (a);                                                                                             \
        (a)         = (b);                                                                                             \
        (b)         = flow_t;                                                                                          \
    } while(0)
// conventions 
//
//        flow_<module>_<action>
/*
 helpers for bit packing
 plan : there are mostly as many bit packing tricks as computer sciences the word on machines is usually bytes and we are allowed for operatio
 on bytes they are most efficient for cpus so why even consider bit packing if operation on bytes are more efficient the ans is memory and memory latency is far far more bottleneck than cpu ops one cache miss worth many many cpu instruction as motivated by example of taocp in cards u can use a u8 to pack info about a card in poker thats will help to load 8 cards in one cache line 
few extra bitwise ops are basically free compared to dragging more memory

*/

// bitset static without malloc + with malloc

#if defined(_MSC_VER) && !defined(__clang__)

#include <intrin.h>


static FLOW_INLINE int flow_trailing_zeroes_u64(uint64_t x)
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

static FLOW_INLINE int flow_leading_zeroes_u64(uint64_t x)
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

static FLOW_INLINE int flow_popcount_u64(uint64_t x)

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
static FLOW_INLINE uint32_t flow_trailing_zeroes_u64(uint64_t x)
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
static FLOW_INLINE uint32_t flow_leading_zeroes_u64(uint64_t x)
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
static FLOW_INLINE uint32_t flow_popcount_u64(uint64_t x)
{
    return (uint32_t)__builtin_popcountll(x);
}


#endif
//  can we  simulate generic from c11


typedef struct flow_bitset
{
    uint64_t* FLOW_RESTRICT data;      /* pointer to 64-bit word storage */
    size_t                  bit_count; /* number of logical bits stored */
    size_t                  word_cap;  /* allocated capacity in 64-bit words */
} flow_bitset;



/*
TODO:
implement stack, queue ,deque 
linked list with pointers and operation on it and static array version and may be mannaged  arena version(saves calling malloc everytime)
avl tree,binary tree with pointers and operation on it and static array version and may be mannaged  arena version(saves calling malloc everytime
- stack sort of knuth  



*/

FLOW_END_EXTERN_C

#ifdef FLOW_IMPLEMENTATION

// place non-static function definitions here

#endif /* FLOW_IMPLEMENTATION */

#endif /* FLOW_H */
