#include "ds.h"

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
