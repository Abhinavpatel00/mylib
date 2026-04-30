#include "mu_sparse_set.h"

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
