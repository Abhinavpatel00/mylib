

#include "mu_common.h"
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

