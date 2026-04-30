#ifndef MU_DS_H
#define MU_DS_H

#include "common.h"
#include "bitset.h"

MU_BEGIN_EXTERN_C
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

MU_END_EXTERN_C

#endif
