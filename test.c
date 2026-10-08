#include "mu.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ *
 * Macro layer smoke test                                              *
 * ------------------------------------------------------------------ */

MU_INLINE int sum_array(const int* MU_RESTRICT arr, size_t count)
{
    int sum = 0;
    for (size_t i = 0; i < count; ++i)
        sum += arr[i];
    return sum;
}

MU_NOINLINE static int slow_identity(int x) { return x; }

MU_ALIGN(64) static uint8_t aligned_buffer[64];

typedef struct { int id; double value; } Example;
typedef struct { int header; Example example; } Wrapper;

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);              \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

int main(void)
{
    printf("=== mu macros ===\n");

    MU_STATIC_ASSERT(sizeof(uint64_t) == 8, "u64_must_be_8_bytes");

    int arr[] = {1, 2, 3, 4, 5};
    CHECK(MU_ARRAY_COUNT(arr) == 5);
    CHECK(sum_array(arr, MU_ARRAY_COUNT(arr)) == 15);
    CHECK(slow_identity(42) == 42);

    CHECK(MU_MIN(10, 20) == 10);
    CHECK(MU_MAX(10, 20) == 20);
    CHECK(MU_CLAMP(50, 0, 10) == 10);

    int a = 5, b = 9;
    MU_SWAP(int, a, b);
    CHECK(a == 9 && b == 5);

    CHECK(MU_IS_POW2(64));
    CHECK(!MU_IS_POW2(70));
    CHECK(!MU_IS_POW2(0));

    CHECK(MU_KB(1) == 1024ull);
    CHECK(MU_MB(1) == 1024ull * 1024ull);
    CHECK(MU_GB(1) == 1024ull * 1024ull * 1024ull);

    CHECK(MU_ALIGN_UP(5, 4) == 8);
    CHECK(MU_ALIGN_DOWN(5, 4) == 4);

    CHECK(((uintptr_t)aligned_buffer % 64) == 0);
    CHECK(MU_TOSTRING(Hello) != NULL);
    int MU_CONCAT(test_, 123) = 123;
    CHECK(test_123 == 123);

    Wrapper w;
    w.header        = 11;
    w.example.id    = 99;
    w.example.value = 3.14;
    CHECK(MU_OFFSET_OF(Wrapper, example) == offsetof(Wrapper, example));
    Wrapper* recovered = MU_CONTAINER_OF(&w.example, Wrapper, example);
    CHECK(recovered->header == 11);

    uint64_t mask = 0x000000000000000Bull; /* popcount 3 */
    CHECK(mu_trailing_zeroes_u64(mask) == 0);
    CHECK(mu_leading_zeroes_u64(mask) == 60);
    CHECK(mu_popcount_u64(mask) == 3);
    CHECK(mu_trailing_zeroes_u64(0) == 64);
    CHECK(mu_leading_zeroes_u64(0) == 64);

    MU_ASSERT(sum_array(arr, MU_ARRAY_COUNT(arr)) == 15);

    printf("=== containers ===\n");

    /* array (stretchy buffer) */
    {
        float* v = NULL;
        array_push(v, 1.0f);
        array_push(v, 2.0f);
        CHECK(array_size(v) == 2);
        CHECK(array_back(v) == 2.0f);
        array_free(v);
        CHECK(v == NULL);
    }

    /* bitset */
    {
        mu_bitset* bs = mu_bitset_create_with_capacity(130);
        mu_bitset_set(bs, 1);
        mu_bitset_set(bs, 64);
        mu_bitset_set(bs, 129);
        CHECK(mu_bitset_count(bs) == 3);
        CHECK(mu_bitset_test(bs, 64));

        size_t it = 0, seen = 0;
        while (mu_bitset_next_set_bit(bs, &it)) { ++seen; ++it; }
        CHECK(seen == 3);

        mu_bitset* other = mu_bitset_create_with_capacity(130);
        mu_bitset_set(other, 64);
        CHECK(mu_bitset_intersection_count(bs, other) == 1);
        CHECK(mu_bitset_union_count(bs, other) == 3);

        mu_bitset_free(other);
        mu_bitset_free(bs);
    }

    /* sparse set */
    {
        mu_sparse_set s;
        mu_sparse_set_init(&s, 256);
        mu_sparse_set_insert(&s, 7);
        mu_sparse_set_insert(&s, 200);
        CHECK(mu_sparse_set_contains(&s, 200));
        CHECK(s.count == 2);
        mu_sparse_set_remove(&s, 7);
        CHECK(!mu_sparse_set_contains(&s, 7));
        CHECK(s.count == 1);
        mu_sparse_set_destroy(&s);
    }

    /* hash table (owned) */
    {
        mu_hash32_t h;
        mu_hash32_init(&h, 64);
        mu_hash32_set(&h, 1234u, 42u);
        uint32_t out = 0;
        CHECK(mu_hash32_get(&h, 1234u, &out));
        CHECK(out == 42u);
        CHECK(!mu_hash32_get(&h, 999u, &out));
        mu_hash32_destroy(&h);
    }

    /* bulk storage + generational handles */
    {
        mu_bulk_storage store;
        mu_bulk_storage_init(&store, sizeof(Example), 8);
        uint32_t id = mu_bulk_storage_alloc(&store);
        Example* e  = (Example*)mu_bulk_storage_ptr(&store, id);
        e->id = 5;
        mu_weak_handle h = mu_bulk_storage_make_handle(&store, id);
        CHECK(mu_bulk_storage_validate_handle(&store, h));
        mu_bulk_storage_free(&store, id);
        CHECK(mu_bulk_storage_resolve_handle(&store, h) == NULL);
        mu_bulk_storage_deinit(&store);
    }

    /* id pool */
    {
        mu_id_pool pool;
        mu_id_pool_init(&pool, 16);
        uint32_t first = 0;
        CHECK(mu_id_pool_create_range_id(&pool, &first, 4));
        CHECK(first == 0);
        CHECK(mu_id_pool_destroy_range_id(&pool, first, 4));
        mu_id_pool_deinit(&pool);
    }

    /* multi index */
    {
        mu_multi_index idx;
        mu_multi_index_init(&idx, 16, 16);
        mu_multi_index_add(&idx, 3u, 100u);
        mu_multi_index_add(&idx, 3u, 101u);
        uint32_t first = mu_multi_index_first(&idx, 3u);
        uint32_t n = 0;
        for (n = first; mu_multi_index_node_valid(&idx, n);
             n = mu_multi_index_next(&idx, first, n))
            ;
        CHECK(mu_multi_index_count_key(&idx, 3u) == 2);
        mu_multi_index_deinit(&idx);
    }

    /* length index */
    {
        MuLengthIndex li;
        mu_length_index_init(&li);
        mu_length_index_rebuild(&li);
        mu_length_index_destroy(&li);
    }

    /* ------------------------------------------------------------------ */
    printf("=== hierarchical bitset ===\n");
    /* ------------------------------------------------------------------ */
    {
        enum { HBITS = 65536u };
        static uint8_t hb_ref[HBITS];
        mu_hier_bitset bs;
        CHECK(mu_hier_bitset_init(&bs, (uint32_t)HBITS));
        CHECK(mu_hier_bitset_empty(&bs));
        CHECK(mu_hier_bitset_find_first(&bs) == MU_INVALID_INDEX);
        CHECK(mu_hier_bitset_any(&bs) == false);

        /* deterministic pseudo-random pattern (duplicates collapse) */
        uint32_t seed = 12345u;
        for (uint32_t i = 0; i < 500u; ++i)
        {
            seed       = seed * 1664525u + 1013904223u;
            uint32_t b = seed % (uint32_t)HBITS;
            hb_ref[b]  = 1;
            mu_hier_bitset_set(&bs, b);
        }

        uint32_t expected = 0;
        for (uint32_t i = 0; i < (uint32_t)HBITS; ++i)
            if (hb_ref[i])
                ++expected;
        CHECK(expected > 0);
        CHECK(mu_hier_bitset_any(&bs));
        CHECK(mu_hier_bitset_capacity(&bs) == (uint32_t)HBITS);
        CHECK(mu_hier_bitset_popcount(&bs) == expected);

        /* point queries match the reference */
        for (uint32_t i = 0; i < (uint32_t)HBITS; i += 97u)
            CHECK(mu_hier_bitset_test(&bs, i) == (hb_ref[i] != 0));

        /* in-bounds and out-of-bounds */
        CHECK(mu_hier_bitset_test(&bs, (uint32_t)HBITS) == false);
        mu_hier_bitset_set(&bs, (uint32_t)HBITS); /* ignored */

        /* full ascending iteration covers exactly the reference */
        uint32_t seen   = 0;
        uint32_t prev   = MU_INVALID_INDEX;
        bool     ordered = true;
        for (uint32_t i = mu_hier_bitset_find_first(&bs); i != MU_INVALID_INDEX;
             i           = mu_hier_bitset_find_next(&bs, i))
        {
            if (i >= (uint32_t)HBITS || !hb_ref[i])
                CHECK(false);
            if (prev != MU_INVALID_INDEX && i <= prev)
                ordered = false;
            prev = i;
            ++seen;
            if (seen > (uint32_t)HBITS)
            {
                CHECK(false);
                break;
            }
        }
        CHECK(ordered);
        CHECK(seen == expected);

        /* first / next anchors */
        uint32_t first_ref = MU_INVALID_INDEX;
        for (uint32_t i = 0; i < (uint32_t)HBITS; ++i)
            if (hb_ref[i])
            {
                first_ref = i;
                break;
            }
        CHECK(mu_hier_bitset_find_first(&bs) == first_ref);

        /* clearing every bit leaves an empty set (exercises the summary chain) */
        for (uint32_t i = 0; i < (uint32_t)HBITS; ++i)
            if (hb_ref[i])
            {
                mu_hier_bitset_reset(&bs, i);
                hb_ref[i] = 0;
            }
        CHECK(mu_hier_bitset_empty(&bs));
        CHECK(mu_hier_bitset_popcount(&bs) == 0);
        CHECK(mu_hier_bitset_find_first(&bs) == MU_INVALID_INDEX);

        mu_hier_bitset_clear(&bs);
        mu_hier_bitset_destroy(&bs);

        /* tiny domain => level_count == 1 (no summary level at all) */
        {
            mu_hier_bitset tiny;
            CHECK(mu_hier_bitset_init(&tiny, 100u));
            mu_hier_bitset_set(&tiny, 0u);
            mu_hier_bitset_set(&tiny, 63u);
            mu_hier_bitset_set(&tiny, 64u);
            mu_hier_bitset_set(&tiny, 99u);
            CHECK(mu_hier_bitset_popcount(&tiny) == 4);
            CHECK(mu_hier_bitset_find_first(&tiny) == 0u);
            CHECK(mu_hier_bitset_find_next(&tiny, 0u) == 63u);
            CHECK(mu_hier_bitset_find_next(&tiny, 63u) == 64u);
            CHECK(mu_hier_bitset_find_next(&tiny, 64u) == 99u);
            CHECK(mu_hier_bitset_find_next(&tiny, 99u) == MU_INVALID_INDEX);
            mu_hier_bitset_destroy(&tiny);
        }

        /* static variant: must not free caller memory */
        {
            uint32_t need = mu_hier_bitset_words_needed((uint32_t)HBITS);
            uint64_t* sbuf = (uint64_t*)MU_MALLOC((size_t)need * sizeof(uint64_t));
            CHECK(sbuf != NULL);
            if (sbuf)
            {
                mu_hier_bitset sbs;
                CHECK(mu_hier_bitset_init_static(&sbs, sbuf, need, (uint32_t)HBITS));
                mu_hier_bitset_set(&sbs, 1234u);
                mu_hier_bitset_set(&sbs, 65535u);
                CHECK(mu_hier_bitset_test(&sbs, 1234u));
                CHECK(mu_hier_bitset_popcount(&sbs) == 2);
                CHECK(mu_hier_bitset_find_first(&sbs) == 1234u);
                CHECK(mu_hier_bitset_find_next(&sbs, 1234u) == 65535u);
                CHECK(mu_hier_bitset_find_next(&sbs, 65535u) == MU_INVALID_INDEX);
                mu_hier_bitset_destroy(&sbs);
                MU_FREE(sbuf);
            }
        }
    }

    /* ------------------------------------------------------------------ */
    printf("=== chunked bitset ===\n");
    /* ------------------------------------------------------------------ */
    {
        enum { CBITS = 200000u };
        static uint8_t cb_ref[CBITS];
        mu_chunked_bitset bs;
        mu_chunked_bitset_init(&bs);
        CHECK(mu_chunked_bitset_empty(&bs));
        CHECK(mu_chunked_bitset_find_first(&bs, 0) == MU_INVALID_INDEX);

        uint32_t seed = 777u;
        for (uint32_t i = 0; i < 3000u; ++i)
        {
            seed       = seed * 1664525u + 1013904223u;
            uint32_t b = seed % (uint32_t)CBITS;
            cb_ref[b]  = 1;
            mu_chunked_bitset_set(&bs, b);
        }

        uint32_t expected = 0;
        for (uint32_t i = 0; i < (uint32_t)CBITS; ++i)
            if (cb_ref[i])
                ++expected;
        CHECK(expected > 0);
        CHECK(mu_chunked_bitset_any(&bs));
        CHECK(mu_chunked_bitset_popcount(&bs) == expected);
        CHECK(mu_chunked_bitset_capacity(&bs) > 0u);
        /* 512 bits/chunk over a 200000-bit domain => real growth happened */
        CHECK(mu_chunked_bitset_chunk_count(&bs) >= (uint32_t)(CBITS / 512u));

        for (uint32_t i = 0; i < (uint32_t)CBITS; i += 101u)
            CHECK(mu_chunked_bitset_test(&bs, i) == (cb_ref[i] != 0));
        CHECK(mu_chunked_bitset_test(&bs, (uint32_t)CBITS) == false);

        uint32_t seen    = 0;
        uint32_t prev    = MU_INVALID_INDEX;
        bool     ordered = true;
        for (uint32_t i = mu_chunked_bitset_find_first(&bs, 0); i != MU_INVALID_INDEX;
             i           = mu_chunked_bitset_find_next(&bs, i))
        {
            if (i >= (uint32_t)CBITS || !cb_ref[i])
                CHECK(false);
            if (prev != MU_INVALID_INDEX && i <= prev)
                ordered = false;
            prev = i;
            ++seen;
            if (seen > (uint32_t)CBITS)
            {
                CHECK(false);
                break;
            }
        }
        CHECK(ordered);
        CHECK(seen == expected);

        /* find_first from a mid-range start */
        uint32_t mid_ref = MU_INVALID_INDEX;
        for (uint32_t i = CBITS / 2u; i < (uint32_t)CBITS; ++i)
            if (cb_ref[i])
            {
                mid_ref = i;
                break;
            }
        CHECK(mu_chunked_bitset_find_first(&bs, CBITS / 2u) == mid_ref);

        /* reset everything */
        for (uint32_t i = 0; i < (uint32_t)CBITS; ++i)
            if (cb_ref[i])
            {
                mu_chunked_bitset_reset(&bs, i);
                cb_ref[i] = 0;
            }
        CHECK(mu_chunked_bitset_empty(&bs));
        CHECK(mu_chunked_bitset_popcount(&bs) == 0);
        CHECK(mu_chunked_bitset_find_first(&bs, 0) == MU_INVALID_INDEX);

        /* set again then clear wholesale */
        mu_chunked_bitset_set(&bs, 0u);
        mu_chunked_bitset_set(&bs, (uint32_t)CBITS - 1u);
        CHECK(mu_chunked_bitset_popcount(&bs) == 2);
        mu_chunked_bitset_clear(&bs);
        CHECK(mu_chunked_bitset_popcount(&bs) == 0);
        CHECK(mu_chunked_bitset_capacity(&bs) == 0u);
        mu_chunked_bitset_destroy(&bs);

        /* static variant: bounded, and must not free caller memory */
        {
            const uint32_t bits = 4096u;
            uint32_t nw    = mu_chunked_bitset_words_needed(bits);
            uint32_t sw    = mu_chunked_bitset_summary_words_needed(bits);
            uint64_t* pw   = (uint64_t*)MU_MALLOC((size_t)nw * sizeof(uint64_t));
            uint64_t* sm   = (uint64_t*)MU_MALLOC((size_t)sw * sizeof(uint64_t));
            CHECK(pw != NULL && sm != NULL);
            if (pw && sm)
            {
                mu_chunked_bitset sbs;
                uint32_t chunks = (bits + 511u) / 512u;
                CHECK(mu_chunked_bitset_init_static(&sbs, pw, sm, chunks, bits));
                mu_chunked_bitset_set(&sbs, 10u);
                mu_chunked_bitset_set(&sbs, 4095u);
                CHECK(mu_chunked_bitset_test(&sbs, 10u));
                CHECK(mu_chunked_bitset_test(&sbs, 4095u));
                CHECK(mu_chunked_bitset_popcount(&sbs) == 2);
                CHECK(mu_chunked_bitset_find_first(&sbs, 0) == 10u);
                CHECK(mu_chunked_bitset_find_first(&sbs, 4096u) == MU_INVALID_INDEX);
                mu_chunked_bitset_destroy(&sbs);
                MU_FREE(pw);
                MU_FREE(sm);
            }
        }
    }

    /* ------------------------------------------------------------------ */
    printf("=== bloom filter ===\n");
    /* ------------------------------------------------------------------ */
    {
        const uint64_t N = 1000ull;

        /* classic */
        {
            mu_bloom bf;
            CHECK(mu_bloom_init_per_item(&bf, N, 10u));
            CHECK(mu_bloom_bit_count(&bf) >= N * 10ull);
            CHECK(mu_bloom_hash_count_for(10u) == 7u);
            CHECK(mu_bloom_hash_count_for(1u) == 1u);

            for (uint64_t i = 0; i < N; ++i)
                mu_bloom_add(&bf, i + 0x9E3779B97F4A7C15ull);

            /* no false negatives, ever */
            for (uint64_t i = 0; i < N; ++i)
                if (!mu_bloom_test(&bf, i + 0x9E3779B97F4A7C15ull))
                    CHECK(false);

            /* false positives must stay rare */
            uint32_t fp = 0;
            for (uint64_t i = N; i < N * 2ull; ++i)
                if (mu_bloom_test(&bf, i + 0x9E3779B97F4A7C15ull))
                    ++fp;
            CHECK(fp < 150u);

            double fill = mu_bloom_fill_ratio(&bf);
            CHECK(fill > 0.0 && fill < 1.0);

            mu_bloom_clear(&bf);
            CHECK(mu_bloom_popcount(&bf) == 0);
            CHECK(mu_bloom_fill_ratio(&bf) == 0.0);
            mu_bloom_destroy(&bf);
        }

        /* blocked: same guarantees, one word touched per probe */
        {
            mu_bloom_blocked bf;
            CHECK(mu_bloom_blocked_init_per_item(&bf, N, 10u));

            for (uint64_t i = 0; i < N; ++i)
                mu_bloom_blocked_add(&bf, i + 0x9E3779B97F4A7C15ull);

            for (uint64_t i = 0; i < N; ++i)
                if (!mu_bloom_blocked_test(&bf, i + 0x9E3779B97F4A7C15ull))
                    CHECK(false);

            uint32_t fp = 0;
            for (uint64_t i = N; i < N * 2ull; ++i)
                if (mu_bloom_blocked_test(&bf, i + 0x9E3779B97F4A7C15ull))
                    ++fp;
            CHECK(fp < 150u);

            double fill = mu_bloom_blocked_fill_ratio(&bf);
            CHECK(fill > 0.0 && fill < 1.0);

            mu_bloom_blocked_clear(&bf);
            CHECK(mu_bloom_blocked_popcount(&bf) == 0);
            mu_bloom_blocked_destroy(&bf);
        }

        /* static variants must not free caller memory */
        {
            static uint64_t bloom_words[64];
            static uint64_t bloom_cells[64];
            mu_bloom  a;
            mu_bloom_blocked b;
            CHECK(mu_bloom_init_static(&a, bloom_words, 64u, 7u));
            mu_bloom_add(&a, 42ull);
            CHECK(mu_bloom_test(&a, 42ull));
            mu_bloom_destroy(&a);

            CHECK(mu_bloom_blocked_init_static(&b, bloom_cells, 64u, 7u));
            mu_bloom_blocked_add(&b, 42ull);
            CHECK(mu_bloom_blocked_test(&b, 42ull));
            mu_bloom_blocked_destroy(&b);
        }
    }

    if (failures == 0)
        printf("\nAll checks passed.\n");
    else
        printf("\n%d check(s) failed.\n", failures);

    return failures == 0 ? 0 : 1;
}
