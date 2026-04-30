#include "mu_common.h"
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

