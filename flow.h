// the aim is this lib is to fullfill my personal needs for drop in headers or header + impl to use in new projects i feel c is good language with some drawbacks for that i will  ofcourse designing my own language but i dont think i will make that language for shipping i enjoy programming so it is just for that c99 is good enough for shipping this lib is to make c tolerable and provide good data structures and algos
//
#ifndef FLOW_H
#define FLOW_H

// -----------------------------------------------------------------------------
//  single-header configuration
//
// Usage (one .c/.cpp file):
//   #define FLOW_IMPLEMENTATION
//   #include "flow.h"
// -----------------------------------------------------------------------------

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
#define FLOW_NOOP()                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
    } while(0)
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
#define FLOW_STATIC_ASSERT_GLUE_(a, b) a##b
#define FLOW_STATIC_ASSERT_GLUE(a, b) FLOW_STATIC_ASSERT_GLUE_(a, b)
#define FLOW_STATIC_ASSERT(cond, msg)                                                                                  \
    typedef char FLOW_STATIC_ASSERT_GLUE(flow_static_assertion_, __LINE__)[(cond) ? 1 : -1]
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
#define FLOW_CEIL(x, y) (((x) + (y) - 1) / (y))
#define FLOW_FLOOR(x, y) ((x) / (y))
#define FLOW_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define FLOW_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define FLOW_CLAMP(x, lo, hi) (FLOW_MIN(FLOW_MAX((x), (lo)), (hi)))

#define FLOW_BIT(n) (1ull << (n))
#define FLOW_HAS_FLAG(x, flag) (((x) & (flag)) != 0)
#define FLOW_SET_FLAG(x, flag) ((x) |= (flag))
#define FLOW_CLEAR_FLAG(x, flag) ((x) &= ~(flag))
#define FLOW_ABS(x) ((x) < 0 ? -(x) : (x))
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
/*
 * there are tradeoffs here as everywhere in life i actually first thought to have bitset store bit count but that turns out a little cumbersome so we are going with word count 
*/

typedef struct flow_bitset
{
    uint64_t* FLOW_RESTRICT array;         /* pointer to 64-bit word storage */
    size_t                  word_count;    /* number words stored */
    size_t                  word_capacity; /* allocated capacity in 64-bit words */
} flow_bitset;

/* Create a new bitset. Return NULL in case of failure. */
flow_bitset* flow_bitset_create(void);

/* Create a new bitset able to contain size bits. Return NULL in case of failure. */
flow_bitset* flow_bitset_create_with_capacity(size_t size);

/* Free memory. */
void flow_bitset_free(flow_bitset* bitset);

/* Set all bits to zero. */
void flow_bitset_clear(flow_bitset* bitset);

/* Set all bits to one. */
void flow_bitset_fill(flow_bitset* bitset);

/* Create a copy. */
flow_bitset* flow_bitset_copy(const flow_bitset* src);

/* Resize in 64-bit words. New words are zeroed. */
bool flow_bitset_resize_words(flow_bitset* bs, size_t new_word_count);

/* Grow in 64-bit words. */
bool flow_bitset_grow(flow_bitset* bs, size_t new_word_count);

/* attempts to recover unused memory, return false in case of reallocation failure */
bool flow_bitset_trim(flow_bitset* bs);

/* shifts all bits by shift positions so values 1,2,10 become 1+shift,2+shift,10+shift */
void flow_bitset_shift_left(flow_bitset* bs, size_t shift);

/* shifts all bits by shift positions so values 1,2,10 become 1-shift,2-shift,10-shift */
void flow_bitset_shift_right(flow_bitset* bs, size_t shift);

/* Set/reset/test bits. */
void flow_bitset_set(flow_bitset* bs, size_t bit_index);
void flow_bitset_reset(flow_bitset* bs, size_t bit_index);
bool flow_bitset_test(const flow_bitset* bs, size_t bit_index);
void flow_bitset_enable_bit(flow_bitset* bs, size_t bit_index);
void flow_bitset_disable_bit(flow_bitset* bs, size_t bit_index);
void flow_bitset_set_bit(flow_bitset* bs, size_t bit_index, bool value);
bool flow_bitset_get_bit(const flow_bitset* bs, size_t bit_index);

/* Query sizes. */
static FLOW_INLINE size_t flow_bitset_size_in_bytes(const flow_bitset* bs)
{
    return bs->word_count * sizeof(uint64_t);
}

static FLOW_INLINE size_t flow_bitset_size_in_bits(const flow_bitset* bs)
{
    return bs->word_count * 64;
}

static FLOW_INLINE size_t flow_bitset_size_in_words(const flow_bitset* bs)
{
    return bs->word_count;
}

/* Set algebra and counters. */
size_t flow_bitset_count(const flow_bitset* bs);
bool   flow_bitset_empty(const flow_bitset* bs);
size_t flow_bitset_minimum(const flow_bitset* bs);
size_t flow_bitset_maximum(const flow_bitset* bs);
bool   flow_bitset_equal(const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);
bool   flow_bitsets_disjoint(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
bool   flow_bitsets_intersect(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
bool   flow_bitset_contains_all(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
bool   flow_bitset_inplace_union(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
size_t flow_bitset_union_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
void   flow_bitset_inplace_intersection(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
size_t flow_bitset_intersection_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
void   flow_bitset_inplace_difference(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
size_t flow_bitset_difference_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
bool   flow_bitset_inplace_symmetric_difference(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
size_t flow_bitset_symmetric_difference_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2);
bool flow_bitset_xor(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);
bool flow_bitset_or(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);
bool flow_bitset_and(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);
bool flow_bitset_not(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a);
bool flow_bitset_xor_assign(flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);
bool flow_bitset_or_assign(flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);
bool flow_bitset_and_assign(flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b);

/* Iteration helpers. */
typedef bool (*flow_bitset_iterator)(size_t value, void* param);
typedef flow_bitset_iterator flow_bitset_visit_fn;

void flow_bitset_traverse(const flow_bitset* bs, flow_bitset_visit_fn visitor, void* user);
void flow_bitset_traverse_range(const flow_bitset* bs, flow_bitset_visit_fn visitor, void* user, size_t begin, size_t count);

static FLOW_INLINE bool flow_bitset_next_set_bit(const flow_bitset* bs, size_t* i)
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
        *i += (size_t)flow_trailing_zeroes_u64(w);
        return true;
    }
    x++;
    while(x < bs->word_count)
    {
        w = bs->array[x];
        if(w != 0)
        {
            *i = x * 64 + (size_t)flow_trailing_zeroes_u64(w);
            return true;
        }
        x++;
    }
    return false;
}

static FLOW_INLINE size_t flow_bitset_next_set_bits(const flow_bitset* bs, size_t* buffer, size_t capacity, size_t* startfrom)
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
            int      r        = flow_trailing_zeroes_u64(w);
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

static FLOW_INLINE bool flow_bitset_for_each(const flow_bitset* bs, flow_bitset_iterator iterator, void* ptr)
{
    size_t base = 0;
    for(size_t i = 0; i < bs->word_count; ++i)
    {
        uint64_t w = bs->array[i];
        while(w != 0)
        {
            uint64_t t = w & (~w + 1);
            int      r = flow_trailing_zeroes_u64(w);
            if(!iterator((size_t)r + base, ptr))
                return false;
            w ^= t;
        }
        base += 64;
    }
    return true;
}

void flow_bitset_print(const flow_bitset* b);


/*
TODO:
implement stack, queue ,deque 
linked list with pointers and operation on it and static array version and may be mannaged  arena version(saves calling malloc everytime)
avl tree,binary tree with pointers and operation on it and static array version and may be mannaged  arena version(saves calling malloc everytime
- stack sort of knuth  



*/
// • Allocates the struct.
// • Computes required number of 64-bit words to hold size bits.
flow_bitset* flow_bitset_create()
{
    flow_bitset* bitset = NULL;
    /* Allocate the bitset itself. */
    bitset                = (flow_bitset*)malloc(sizeof(flow_bitset));
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
flow_bitset* flow_bitset_create_with_capacity(size_t size)
{
    flow_bitset* bitset = (flow_bitset*)malloc(sizeof(flow_bitset));
    //“bits per word = bytes per word × 8”
    bitset->word_count    = FLOW_CEIL(size, sizeof(uint64_t) * 8);
    bitset->word_capacity = bitset->word_count;

    bitset->array = (uint64_t*)calloc(bitset->word_count, sizeof(uint64_t));
    return bitset;
}


void flow_bitset_free(flow_bitset* bitset)
{
    free(bitset->array);
    free(bitset);
}

static void flow_bitset_clear_impl(flow_bitset* bitset)
{
    memset(bitset->array, 0, sizeof(uint64_t) * bitset->word_count);
}

static void flow_bitset_fill_impl(flow_bitset* bitset)
{
    memset(bitset->array, 0xff, sizeof(uint64_t) * bitset->word_count);
}

/*
Flow-prefixed aliases keep the public API naming consistent.
These functions intentionally stay minimal and avoid redundant checks.
*/
void flow_bitset_clear(flow_bitset* bitset)
{
    flow_bitset_clear_impl(bitset);
}

void flow_bitset_fill(flow_bitset* bitset)
{
    flow_bitset_fill_impl(bitset);
}

static flow_bitset* flow_bitset_copy_impl(const flow_bitset* src)
{
    flow_bitset* copy   = (flow_bitset*)malloc(sizeof *copy);
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

flow_bitset* flow_bitset_copy(const flow_bitset* src)
{
    return flow_bitset_copy_impl(src);
}

static bool flow_bitset_resize_impl(flow_bitset* bs, size_t new_word_count, bool padwithzeroes)
{


    if(new_word_count > SIZE_MAX / 64)
        return false;

    if(new_word_count > bs->word_capacity)
    {

        uint64_t* newarray;
        size_t    new_word_cap = (UINT64_C(0xFFFFFFFFFFFFFFFF) >> flow_leading_zeroes_u64(new_word_count)) + 1;
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

bool flow_bitset_resize_words(flow_bitset* bs, size_t new_word_count)
{
    return flow_bitset_resize_impl(bs, new_word_count, true);
}

/*
Map a bit index to storage location:
  word index = bit / 64
  bit offset = bit % 64
*/
void flow_bitset_set(flow_bitset* bs, size_t bit_index)
{
    size_t word_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    if(word_index >= bs->word_count)
        flow_bitset_resize_impl(bs, word_index + 1, true);

    bs->array[word_index] |= (UINT64_C(1) << bit_offset);
}

void flow_bitset_reset(flow_bitset* bs, size_t bit_index)
{
    size_t word_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    if(word_index >= bs->word_count)
        return;

    bs->array[word_index] &= ~(UINT64_C(1) << bit_offset);
}

bool flow_bitset_test(const flow_bitset* bs, size_t bit_index)
{
    size_t word_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    if(word_index >= bs->word_count)
        return false;

    return (bs->array[word_index] & (UINT64_C(1) << bit_offset)) != 0;
}

void flow_bitset_enable_bit(flow_bitset* bs, size_t bit_index)
{
    flow_bitset_set(bs, bit_index);
}

void flow_bitset_disable_bit(flow_bitset* bs, size_t bit_index)
{
    flow_bitset_reset(bs, bit_index);
}

void flow_bitset_set_bit(flow_bitset* bs, size_t bit_index, bool value)
{
    if(value)
    {
        flow_bitset_set(bs, bit_index);
    }
    else
    {
        flow_bitset_reset(bs, bit_index);
    }
}

bool flow_bitset_get_bit(const flow_bitset* bs, size_t bit_index)
{
    return flow_bitset_test(bs, bit_index);
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

void flow_bitset_shift_left(flow_bitset* bs, size_t shift)
{
    if(shift == 0 || bs->word_count == 0)
        return;

    size_t word_shift = shift / 64;  // whole 64-bit blocks
    size_t bit_shift  = shift % 64;  // remaining bits
    size_t old_count  = bs->word_count;

    /* Ensure enough space */
    size_t new_count = old_count + word_shift + (bit_shift ? 1 : 0);
    flow_bitset_resize_impl(bs, new_count, true);  // this zeroes new region

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
void flow_bitset_shift_right(flow_bitset* bs, size_t shift)
{
    if(shift == 0 || bs->word_count == 0)
        return;

    size_t word_shift = shift / 64;  // whole-word shift
    size_t bit_shift  = shift % 64;  // remaining bit shift
    size_t old_count  = bs->word_count;

    if(word_shift >= old_count)
    {
        /* Everything shifts out */
        flow_bitset_resize_impl(bs, 0, false);
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
    flow_bitset_resize_impl(bs, old_count - word_shift, false);
}


bool flow_bitset_grow(flow_bitset* bs, size_t new_word_count)
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

size_t flow_bitset_count(const flow_bitset* bs)
{
    size_t card = 0;
    size_t k    = 0;
    for(; k + 7 < bs->word_count; k += 8)
    {
        card += flow_popcount_u64(bs->array[k]);
        card += flow_popcount_u64(bs->array[k + 1]);
        card += flow_popcount_u64(bs->array[k + 2]);
        card += flow_popcount_u64(bs->array[k + 3]);
        card += flow_popcount_u64(bs->array[k + 4]);
        card += flow_popcount_u64(bs->array[k + 5]);
        card += flow_popcount_u64(bs->array[k + 6]);
        card += flow_popcount_u64(bs->array[k + 7]);
    }
    for(; k + 3 < bs->word_count; k += 4)
    {
        card += flow_popcount_u64(bs->array[k]);
        card += flow_popcount_u64(bs->array[k + 1]);
        card += flow_popcount_u64(bs->array[k + 2]);
        card += flow_popcount_u64(bs->array[k + 3]);
    }
    for(; k < bs->word_count; k++)
    {
        card += flow_popcount_u64(bs->array[k]);
    }
    return card;
}

bool flow_bitset_inplace_union(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    for(size_t k = 0; k < minlength; ++k)
    {
        b1->array[k] |= b2->array[k];
    }
    if(b2->word_count > b1->word_count)
    {
        size_t oldsize = b1->word_count;
        if(!flow_bitset_resize_words(b1, b2->word_count))
            return false;
        memcpy(b1->array + oldsize, b2->array + oldsize, (b2->word_count - oldsize) * sizeof(uint64_t));
    }
    return true;
}

bool flow_bitset_empty(const flow_bitset* bs)
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

size_t flow_bitset_minimum(const flow_bitset* bs)
{
    for(size_t k = 0; k < bs->word_count; k++)
    {
        uint64_t w = bs->array[k];
        if(w != 0)
        {
            return flow_trailing_zeroes_u64(w) + k * 64;
        }
    }
    return SIZE_MAX;
}

size_t flow_bitset_maximum(const flow_bitset* bs)
{
    for(size_t k = bs->word_count; k > 0; k--)
    {
        uint64_t w = bs->array[k - 1];
        if(w != 0)
        {
            return 63 - flow_leading_zeroes_u64(w) + (k - 1) * 64;
        }
    }
    return 0;
}

/* Returns true if bitsets share no common elements, false otherwise.
 *
 * Performs early-out if common element found. */
bool flow_bitsets_disjoint(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
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
bool flow_bitsets_intersect(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
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
static bool flow_bitset_any_bits_set(const flow_bitset* b, size_t starting_loc)
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

bool flow_bitset_equal(const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
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
        return !flow_bitset_any_bits_set(a, b->word_count);
    }

    return !flow_bitset_any_bits_set(b, a->word_count);
}

/* Returns true if b1 has all of b2's bits set.
 *
 * Performs early out if a bit is found in b2 that is not found in b1. */
bool flow_bitset_contains_all(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
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
        return !flow_bitset_any_bits_set(b2, b1->word_count);
    }
    return true;
}

size_t flow_bitset_union_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
{
    size_t answer    = 0;
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    for(; k + 3 < minlength; k += 4)
    {
        answer += flow_popcount_u64(b1->array[k] | b2->array[k]);
        answer += flow_popcount_u64(b1->array[k + 1] | b2->array[k + 1]);
        answer += flow_popcount_u64(b1->array[k + 2] | b2->array[k + 2]);
        answer += flow_popcount_u64(b1->array[k + 3] | b2->array[k + 3]);
    }
    for(; k < minlength; ++k)
    {
        answer += flow_popcount_u64(b1->array[k] | b2->array[k]);
    }
    if(b2->word_count > b1->word_count)
    {
        for(; k + 3 < b2->word_count; k += 4)
        {
            answer += flow_popcount_u64(b2->array[k]);
            answer += flow_popcount_u64(b2->array[k + 1]);
            answer += flow_popcount_u64(b2->array[k + 2]);
            answer += flow_popcount_u64(b2->array[k + 3]);
        }
        for(; k < b2->word_count; ++k)
        {
            answer += flow_popcount_u64(b2->array[k]);
        }
    }
    else
    {
        for(; k + 3 < b1->word_count; k += 4)
        {
            answer += flow_popcount_u64(b1->array[k]);
            answer += flow_popcount_u64(b1->array[k + 1]);
            answer += flow_popcount_u64(b1->array[k + 2]);
            answer += flow_popcount_u64(b1->array[k + 3]);
        }
        for(; k < b1->word_count; ++k)
        {
            answer += flow_popcount_u64(b1->array[k]);
        }
    }
    return answer;
}

void flow_bitset_inplace_intersection(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
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

size_t flow_bitset_intersection_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
{
    size_t answer    = 0;
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    for(size_t k = 0; k < minlength; ++k)
    {
        answer += flow_popcount_u64(b1->array[k] & b2->array[k]);
    }
    return answer;
}

void flow_bitset_inplace_difference(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    for(; k < minlength; ++k)
    {
        b1->array[k] &= ~(b2->array[k]);
    }
}

size_t flow_bitset_difference_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    size_t answer    = 0;
    for(; k < minlength; ++k)
    {
        answer += flow_popcount_u64(b1->array[k] & ~(b2->array[k]));
    }
    for(; k < b1->word_count; ++k)
    {
        answer += flow_popcount_u64(b1->array[k]);
    }
    return answer;
}

bool flow_bitset_inplace_symmetric_difference(flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
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
        if(!flow_bitset_resize_words(b1, b2->word_count))
            return false;
        memcpy(b1->array + oldsize, b2->array + oldsize, (b2->word_count - oldsize) * sizeof(uint64_t));
    }
    return true;
}

size_t flow_bitset_symmetric_difference_count(const flow_bitset* FLOW_RESTRICT b1, const flow_bitset* FLOW_RESTRICT b2)
{
    size_t minlength = b1->word_count < b2->word_count ? b1->word_count : b2->word_count;
    size_t k         = 0;
    size_t answer    = 0;
    for(; k < minlength; ++k)
    {
        answer += flow_popcount_u64(b1->array[k] ^ b2->array[k]);
    }
    if(b2->word_count > b1->word_count)
    {
        for(; k < b2->word_count; ++k)
        {
            answer += flow_popcount_u64(b2->array[k]);
        }
    }
    else
    {
        for(; k < b1->word_count; ++k)
        {
            answer += flow_popcount_u64(b1->array[k]);
        }
    }
    return answer;
}

bool flow_bitset_xor(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;
    size_t max_size = a->word_count > b->word_count ? a->word_count : b->word_count;

    if(!flow_bitset_resize_words(out, max_size))
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

bool flow_bitset_or(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;
    size_t max_size = a->word_count > b->word_count ? a->word_count : b->word_count;

    if(!flow_bitset_resize_words(out, max_size))
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

bool flow_bitset_and(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
{
    size_t min_size = a->word_count < b->word_count ? a->word_count : b->word_count;
    size_t max_size = a->word_count > b->word_count ? a->word_count : b->word_count;

    if(!flow_bitset_resize_words(out, max_size))
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

bool flow_bitset_not(flow_bitset* FLOW_RESTRICT out, const flow_bitset* FLOW_RESTRICT a)
{
    if(!flow_bitset_resize_words(out, a->word_count))
    {
        return false;
    }

    for(size_t k = 0; k < a->word_count; ++k)
    {
        out->array[k] = ~a->array[k];
    }
    return true;
}

bool flow_bitset_xor_assign(flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
{
    return flow_bitset_inplace_symmetric_difference(a, b);
}

bool flow_bitset_or_assign(flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
{
    return flow_bitset_inplace_union(a, b);
}

bool flow_bitset_and_assign(flow_bitset* FLOW_RESTRICT a, const flow_bitset* FLOW_RESTRICT b)
{
    flow_bitset_inplace_intersection(a, b);
    return true;
}

void flow_bitset_traverse(const flow_bitset* bs, flow_bitset_visit_fn visitor, void* user)
{
    size_t i = 0;
    while(flow_bitset_next_set_bit(bs, &i))
    {
        if(!visitor(i, user))
        {
            return;
        }
        ++i;
    }
}

void flow_bitset_traverse_range(const flow_bitset* bs, flow_bitset_visit_fn visitor, void* user, size_t begin, size_t count)
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
    while(flow_bitset_next_set_bit(bs, &i) && i < end)
    {
        if(!visitor(i, user))
        {
            return;
        }
        ++i;
    }
}

bool flow_bitset_trim(flow_bitset* bs)
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

void flow_bitset_print(const flow_bitset* b)
{
    printf("{");
    for(size_t i = 0; flow_bitset_next_set_bit(b, &i); i++)
    {
        printf("%zu, ", i);
    }
    printf("}");
}

FLOW_END_EXTERN_C

#ifdef FLOW_IMPLEMENTATION

// place non-static function definitions here

#endif /* FLOW_IMPLEMENTATION */

#endif /* FLOW_H */
