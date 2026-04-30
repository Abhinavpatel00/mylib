
#pragma  once 



#include "mu_common.h"
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

