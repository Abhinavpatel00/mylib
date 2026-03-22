#define MU_IMPLEMENTATION
#include "mu.h"

#include <cstdio>

struct VisitState
{
    size_t count;
    size_t sum;
};

struct MultiIndexVisitState
{
    size_t count;
    size_t sum;
};

struct ChunkedVisitState
{
    size_t count;
    size_t sum;
};

struct BulkItem
{
    uint32_t value;
    uint32_t pad;
};

struct BulkVisitState
{
    size_t count;
    size_t sum;
};

static bool visit_collect(size_t value, void* user)
{
    VisitState* state = (VisitState*)user;
    state->count += 1;
    state->sum += value;
    return true;
}

static bool visit_multi_index_collect(uint32_t value, uint32_t node_index, void* user)
{
    MU_UNUSED(node_index);
    MultiIndexVisitState* state = (MultiIndexVisitState*)user;
    state->count += 1;
    state->sum += value;
    return true;
}

static bool visit_chunked_collect(uint32_t value, void* user)
{
    ChunkedVisitState* state = (ChunkedVisitState*)user;
    state->count += 1;
    state->sum += value;
    return true;
}

static bool visit_bulk_collect(uint32_t id, void* slot, void* user)
{
    MU_UNUSED(id);
    BulkItem* item = (BulkItem*)slot;
    BulkVisitState* state = (BulkVisitState*)user;
    state->count += 1;
    state->sum += item->value;
    return true;
}

int main()
{
    int  total  = 0;
    int  passed = 0;
    bool failed = false;

    auto check = [&](bool condition, const char* label) {
        ++total;
        if(condition)
        {
            ++passed;
            std::printf("[PASS] %s\n", label);
        }
        else
        {
            failed = true;
            std::printf("[FAIL] %s\n", label);
        }
    };

    // baseline creation / set / test / reset / clear / fill / copy
    mu_bitset* bs = mu_bitset_create_with_capacity(130);
    check(bs != nullptr, "create_with_capacity(130)");
    mu_bitset_set(bs, 63);
    mu_bitset_set(bs, 64);
    mu_bitset_set(bs, 129);
    mu_bitset_enable_bit(bs, 0);

    mu_bitset_print(bs);
    std::printf("Initial bs = ");
    mu_bitset_print(bs);
    std::printf("\n");
    check(mu_bitset_test(bs, 0), "set/test bit 0");
    check(mu_bitset_test(bs, 63), "set/test bit 63");
    check(mu_bitset_test(bs, 64), "set/test bit 64");
    check(mu_bitset_test(bs, 129), "set/test bit 129");
    check(!mu_bitset_test(bs, 65), "unset bit 65 remains false");

    mu_bitset_reset(bs, 64);
    check(!mu_bitset_test(bs, 64), "reset bit 64");
    mu_bitset_disable_bit(bs, 63);
    check(!mu_bitset_get_bit(bs, 63), "disable/get bit 63");
    mu_bitset_set_bit(bs, 63, true);
    check(mu_bitset_get_bit(bs, 63), "set_bit(true) sets bit 63");
    mu_bitset_set_bit(bs, 63, false);
    check(!mu_bitset_get_bit(bs, 63), "set_bit(false) resets bit 63");

    mu_bitset_clear(bs);
    check(!mu_bitset_test(bs, 0), "clear resets bit 0");
    check(!mu_bitset_test(bs, 63), "clear resets bit 63");
    check(!mu_bitset_test(bs, 129), "clear resets bit 129");

    mu_bitset_fill(bs);
    check(mu_bitset_test(bs, 0), "fill sets bit 0");
    check(mu_bitset_test(bs, 63), "fill sets bit 63");

    mu_bitset* copy = mu_bitset_copy(bs);
    check(copy != nullptr, "copy bitset");
    check(mu_bitset_test(copy, 0) == mu_bitset_test(bs, 0), "copy parity bit 0");
    check(mu_bitset_test(copy, 63) == mu_bitset_test(bs, 63), "copy parity bit 63");
    check(mu_bitset_test(copy, 129) == mu_bitset_test(bs, 129), "copy parity bit 129");

    // grow / resize / trim
    mu_bitset* g = mu_bitset_create();
    check(g != nullptr, "create empty bitset g");
    check(mu_bitset_grow(g, 3), "grow g to 3 words");
    check(mu_bitset_test(g, 0) == false, "grow zero-initializes new words");
    mu_bitset_set(g, 130);
    check(mu_bitset_test(g, 130), "set/test bit 130 in g");
    check(!mu_bitset_grow(g, 2), "grow rejects shrinking request");

    check(mu_bitset_resize_words(g, 6), "resize g to 6 words");
    check(mu_bitset_test(g, 130), "resize keeps existing bits");
    check(mu_bitset_resize_words(g, 2), "resize g down to 2 words");
    check(!mu_bitset_test(g, 130), "downsize drops out-of-range bits");
    mu_bitset_set(g, 1);
    check(mu_bitset_trim(g), "trim g");
    check(mu_bitset_test(g, 1), "trim keeps remaining set bits");

    // prepare sets for set-algebra APIs
    mu_bitset* a = mu_bitset_create();
    mu_bitset* b = mu_bitset_create();
    check(a != nullptr, "create bitset a");
    check(b != nullptr, "create bitset b");
    mu_bitset_set(a, 1);
    mu_bitset_set(a, 3);
    mu_bitset_set(a, 64);
    mu_bitset_set(a, 130);

    mu_bitset_set(b, 3);
    mu_bitset_set(b, 5);
    mu_bitset_set(b, 64);
    mu_bitset_set(b, 200);

    // count / min / max / empty
    check(mu_bitset_count(a) == 4, "count(a) == 4");
    check(mu_bitset_minimum(a) == 1, "minimum(a) == 1");
    check(mu_bitset_maximum(a) == 130, "maximum(a) == 130");
    check(!mu_bitset_empty(a), "a is not empty");
    mu_bitset* e = mu_bitset_create();
    check(e != nullptr, "create bitset e");
    check(mu_bitset_empty(e), "e is empty");
    check(mu_bitset_minimum(e) == SIZE_MAX, "minimum(e) == SIZE_MAX");
    check(mu_bitset_maximum(e) == 0, "maximum(e) == 0");

    // equality with different backing sizes but same logical set bits
    mu_bitset* eq1 = mu_bitset_create();
    mu_bitset* eq2 = mu_bitset_create();
    check(eq1 != nullptr, "create bitset eq1");
    check(eq2 != nullptr, "create bitset eq2");
    mu_bitset_set(eq1, 5);
    mu_bitset_set(eq2, 5);
    check(mu_bitset_resize_words(eq2, 5), "resize eq2 to 5 words");
    check(mu_bitset_equal(eq1, eq2), "equal handles trailing zero words");
    mu_bitset_set(eq2, 66);
    check(!mu_bitset_equal(eq1, eq2), "equal detects difference");

    // disjoint / intersect / contains_all
    check(!mu_bitsets_disjoint(a, b), "a and b are not disjoint");
    check(mu_bitsets_intersect(a, b), "a and b intersect");
    mu_bitset* d = mu_bitset_create();
    check(d != nullptr, "create bitset d");
    mu_bitset_set(d, 999);
    check(mu_bitsets_disjoint(a, d), "a and d are disjoint");
    check(!mu_bitsets_intersect(a, d), "a and d do not intersect");

    mu_bitset* u = mu_bitset_copy(a);
    check(u != nullptr, "copy a into u");
    check(mu_bitset_inplace_union(u, b), "inplace_union(u, b)");
    check(mu_bitset_contains_all(u, a), "u contains all bits of a");
    check(mu_bitset_contains_all(u, b), "u contains all bits of b");
    check(!mu_bitset_contains_all(a, b), "a does not contain all bits of b");
    check(mu_bitset_union_count(a, b) == 6, "union_count(a, b) == 6");

    // intersection and intersection_count
    mu_bitset* i = mu_bitset_copy(a);
    check(i != nullptr, "copy a into i");
    mu_bitset_inplace_intersection(i, b);
    check(mu_bitset_count(i) == 2, "count(intersection) == 2");
    check(mu_bitset_test(i, 3), "intersection contains bit 3");
    check(mu_bitset_test(i, 64), "intersection contains bit 64");
    check(mu_bitset_intersection_count(a, b) == 2, "intersection_count(a, b) == 2");

    // difference and difference_count
    mu_bitset* diff = mu_bitset_copy(a);
    check(diff != nullptr, "copy a into diff");
    mu_bitset_inplace_difference(diff, b);
    check(mu_bitset_count(diff) == 2, "count(difference) == 2");
    check(mu_bitset_test(diff, 1), "difference contains bit 1");
    check(mu_bitset_test(diff, 130), "difference contains bit 130");
    check(mu_bitset_difference_count(a, b) == 2, "difference_count(a, b) == 2");

    // symmetric difference and symmetric_difference_count
    mu_bitset* sx = mu_bitset_copy(a);
    check(sx != nullptr, "copy a into sx");
    check(mu_bitset_inplace_symmetric_difference(sx, b), "inplace_symmetric_difference(sx, b)");
    check(mu_bitset_count(sx) == 4, "count(symmetric_difference) == 4");
    check(mu_bitset_test(sx, 1), "symmetric difference contains bit 1");
    check(mu_bitset_test(sx, 5), "symmetric difference contains bit 5");
    check(mu_bitset_test(sx, 130), "symmetric difference contains bit 130");
    check(mu_bitset_test(sx, 200), "symmetric difference contains bit 200");
    check(mu_bitset_symmetric_difference_count(a, b) == 4, "symmetric_difference_count(a, b) == 4");

    // out-parameter bitwise operators
    mu_bitset* out = mu_bitset_create();
    check(out != nullptr, "create bitset out");
    check(mu_bitset_or(out, a, b), "bitset_or(out, a, b)");
    check(mu_bitset_count(out) == 6, "or count == 6");
    check(mu_bitset_xor(out, a, b), "bitset_xor(out, a, b)");
    check(mu_bitset_count(out) == 4, "xor count == 4");
    check(mu_bitset_and(out, a, b), "bitset_and(out, a, b)");
    check(mu_bitset_count(out) == 2, "and count == 2");

    check(mu_bitset_not(out, a), "bitset_not(out, a)");
    check(!mu_bitset_get_bit(out, 1), "not clears previously set bit");
    check(mu_bitset_get_bit(out, 2), "not sets previously unset bit");

    // assign operators
    mu_bitset* asg = mu_bitset_copy(a);
    check(asg != nullptr, "copy a into asg");
    check(mu_bitset_or_assign(asg, b), "or_assign(asg, b)");
    check(mu_bitset_count(asg) == 6, "or_assign count == 6");
    check(mu_bitset_and_assign(asg, b), "and_assign(asg, b)");
    check(mu_bitset_count(asg) == 4, "and_assign count == 4");
    check(mu_bitset_xor_assign(asg, b), "xor_assign(asg, b)");
    check(mu_bitset_count(asg) == 0, "xor_assign count == 0");

    // traversal helpers
    VisitState all = {0, 0};
    mu_bitset_traverse(a, visit_collect, &all);
    check(all.count == 4, "traverse visits all set bits");
    check(all.sum == (1 + 3 + 64 + 130), "traverse sum is correct");

    VisitState range = {0, 0};
    mu_bitset_traverse_range(a, visit_collect, &range, 2, 64);
    check(range.count == 2, "traverse_range count in [2,66)");
    check(range.sum == (3 + 64), "traverse_range sum in [2,66)");

    // Part 2: multi-index (key -> circular list of values)
    mu_multi_index index;
    mu_multi_index_init(&index, 0, 0);

    uint32_t n0 = mu_multi_index_add(&index, 42, 10);
    uint32_t n1 = mu_multi_index_add(&index, 42, 20);
    uint32_t n2 = mu_multi_index_add(&index, 7, 99);
    check(n0 != MU_MULTI_INDEX_NONE, "multi-index add n0");
    check(n1 != MU_MULTI_INDEX_NONE, "multi-index add n1");
    check(n2 != MU_MULTI_INDEX_NONE, "multi-index add n2");

    check(mu_multi_index_count_key(&index, 42) == 2, "multi-index count key 42 == 2");
    check(mu_multi_index_count_key(&index, 7) == 1, "multi-index count key 7 == 1");
    check(mu_multi_index_count_key(&index, 1234) == 0, "multi-index count missing key == 0");

    MultiIndexVisitState mi = {0, 0};
    mu_multi_index_visit_key(&index, 42, visit_multi_index_collect, &mi);
    check(mi.count == 2, "multi-index visit count key 42");
    check(mi.sum == 30, "multi-index visit sum key 42");

    uint32_t first42 = mu_multi_index_first(&index, 42);
    check(first42 != MU_MULTI_INDEX_NONE, "multi-index first key 42 exists");
    if(first42 != MU_MULTI_INDEX_NONE)
    {
        uint32_t second42 = mu_multi_index_next(&index, first42, first42);
        check(second42 != MU_MULTI_INDEX_NONE, "multi-index next returns second");
        if(second42 != MU_MULTI_INDEX_NONE)
        {
            uint32_t third42 = mu_multi_index_next(&index, first42, second42);
            check(third42 == MU_MULTI_INDEX_NONE, "multi-index next loops back to none");
        }
    }

    check(mu_multi_index_remove(&index, n0), "multi-index remove n0");
    check(!mu_multi_index_node_valid(&index, n0), "multi-index n0 invalid after remove");
    check(mu_multi_index_count_key(&index, 42) == 1, "multi-index key 42 count after remove");
    check(mu_multi_index_remove(&index, n1), "multi-index remove n1");
    check(mu_multi_index_first(&index, 42) == MU_MULTI_INDEX_NONE, "multi-index key 42 empty after removes");
    check(mu_multi_index_count_key(&index, 7) == 1, "multi-index key 7 unaffected");
    mu_multi_index_deinit(&index);

    // Part 3: arrays-of-arrays with fixed-size chunks
    mu_chunked_u32_pool  pool;
    mu_chunked_u32_array carr;
    mu_chunked_u32_pool_init(&pool, 0);
    mu_chunked_u32_array_init(&carr);

    for(uint32_t v = 0; v < 40; ++v)
    {
        check(mu_chunked_u32_array_push(&pool, &carr, v), "chunked array push");
    }
    check(carr.count == 40, "chunked array count == 40");

    uint32_t got = 0;
    check(mu_chunked_u32_array_get(&pool, &carr, 0, &got) && got == 0, "chunked get index 0");
    check(mu_chunked_u32_array_get(&pool, &carr, 13, &got) && got == 13, "chunked get index 13");
    check(mu_chunked_u32_array_get(&pool, &carr, 14, &got) && got == 14, "chunked get index 14");
    check(mu_chunked_u32_array_get(&pool, &carr, 39, &got) && got == 39, "chunked get index 39");
    check(!mu_chunked_u32_array_get(&pool, &carr, 40, &got), "chunked get out of range fails");

    ChunkedVisitState cvs = {0, 0};
    mu_chunked_u32_array_visit(&pool, &carr, visit_chunked_collect, &cvs);
    check(cvs.count == 40, "chunked visit count == 40");
    check(cvs.sum == ((39 * 40) / 2), "chunked visit sum == 0..39");

    for(uint32_t expect = 39; expect >= 30; --expect)
    {
        uint32_t popped = 0;
        check(mu_chunked_u32_array_pop(&pool, &carr, &popped), "chunked pop succeeds");
        check(popped == expect, "chunked pop order is LIFO");
        if(expect == 30)
            break;
    }
    check(carr.count == 30, "chunked count after pops == 30");
    check(mu_chunked_u32_array_get(&pool, &carr, 29, &got) && got == 29, "chunked tail value after pops");

    mu_chunked_u32_array_clear(&pool, &carr);
    check(carr.count == 0, "chunked clear resets count");
    check(carr.first_chunk == MU_CHUNKED_U32_NONE, "chunked clear resets first chunk");
    check(carr.last_chunk == MU_CHUNKED_U32_NONE, "chunked clear resets last chunk");
    mu_chunked_u32_pool_deinit(&pool);

    // Part 1: bulk data with holes + weak handles
    mu_bulk_storage storage;
    check(mu_bulk_storage_init(&storage, sizeof(BulkItem), 8), "bulk storage init");

    uint32_t id_a = mu_bulk_storage_alloc(&storage);
    uint32_t id_b = mu_bulk_storage_alloc(&storage);
    uint32_t id_c = mu_bulk_storage_alloc(&storage);
    check(id_a != 0, "bulk alloc id_a");
    check(id_b != 0, "bulk alloc id_b");
    check(id_c != 0, "bulk alloc id_c");

    BulkItem* a_item = (BulkItem*)mu_bulk_storage_ptr(&storage, id_a);
    BulkItem* b_item = (BulkItem*)mu_bulk_storage_ptr(&storage, id_b);
    BulkItem* c_item = (BulkItem*)mu_bulk_storage_ptr(&storage, id_c);
    check(a_item != nullptr, "bulk ptr id_a");
    check(b_item != nullptr, "bulk ptr id_b");
    check(c_item != nullptr, "bulk ptr id_c");
    if(a_item)
        a_item->value = 10;
    if(b_item)
        b_item->value = 20;
    if(c_item)
        c_item->value = 30;

    mu_weak_handle hb = mu_bulk_storage_make_handle(&storage, id_b);
    check(mu_bulk_storage_validate_handle(&storage, hb), "bulk weak handle valid before free");
    check(mu_bulk_storage_resolve_handle(&storage, hb) == b_item, "bulk resolve handle returns b");

    check(mu_bulk_storage_free(&storage, id_b), "bulk free id_b");
    check(!mu_bulk_storage_is_live(&storage, id_b), "bulk id_b not live after free");
    check(!mu_bulk_storage_validate_handle(&storage, hb), "bulk weak handle invalid after free");
    check(mu_bulk_storage_resolve_handle(&storage, hb) == nullptr, "bulk resolve stale handle null");

    uint32_t id_d = mu_bulk_storage_alloc(&storage);
    check(id_d == id_b, "bulk reuses hole slot");
    BulkItem* d_item = (BulkItem*)mu_bulk_storage_ptr(&storage, id_d);
    check(d_item != nullptr, "bulk ptr id_d");
    if(d_item)
        d_item->value = 40;

    mu_weak_handle hd = mu_bulk_storage_make_handle(&storage, id_d);
    check(mu_bulk_storage_validate_handle(&storage, hd), "bulk new weak handle valid");

    BulkVisitState bvs = {0, 0};
    mu_bulk_storage_visit_live(&storage, visit_bulk_collect, &bvs);
    check(bvs.count == 3, "bulk visit count == 3");
    check(bvs.sum == (10 + 30 + 40), "bulk visit sum matches live values");

    check(!mu_bulk_storage_free(&storage, 0), "bulk free rejects header slot");
    check(!mu_bulk_storage_free(&storage, 999999), "bulk free rejects out-of-range id");

    mu_bulk_storage_deinit(&storage);

    mu_bitset_free(asg);
    mu_bitset_free(out);
    mu_bitset_free(sx);
    mu_bitset_free(diff);
    mu_bitset_free(i);
    mu_bitset_free(u);
    mu_bitset_free(d);
    mu_bitset_free(eq2);
    mu_bitset_free(eq1);
    mu_bitset_free(e);
    mu_bitset_free(b);
    mu_bitset_free(a);
    mu_bitset_free(g);
    mu_bitset_free(copy);
    mu_bitset_free(bs);

    std::printf("\nSummary: %d/%d checks passed\n", passed, total);
    if(failed)
    {
        std::puts("test.cpp: some mu_bitset tests failed");
        return 1;
    }

    std::puts("test.cpp: all mu_bitset tests passed");
    return 0;
}
