#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* assume your macro block is included above this line */

/* -------------------------------------------------- */
/* Test structs for OFFSET + CONTAINER_OF            */
/* -------------------------------------------------- */

typedef struct
{
    int    id;
    double value;
} Example;

typedef struct
{
    int     header;
    Example example;
} Wrapper;


/* -------------------------------------------------- */
/* FORCE INLINE + RESTRICT demo                      */
/* -------------------------------------------------- */

static FLOW_INLINE int sum_array(int* FLOW_RESTRICT arr, size_t count)
{
    int sum = 0;
    for(size_t i = 0; i < count; ++i)
    {
        if(FLOW_LIKELY(arr[i] >= 0))
            sum += arr[i];
        else
            sum -= arr[i];
    }
    return sum;
}

/* NOINLINE demo */
static FLOW_NOINLINE int slow_identity(int x)
{
    return x;
}


/* -------------------------------------------------- */
/* Aligned buffer demo                               */
/* -------------------------------------------------- */

FLOW_ALIGN(64) static uint8_t aligned_buffer[64];


/* -------------------------------------------------- */
/* MAIN                                               */
/* -------------------------------------------------- */

int main(void)
{
    printf("=== FLOW macro demo ===\n\n");

    /* STATIC ASSERT */
    FLOW_STATIC_ASSERT(sizeof(uint64_t) == 8, "u64_must_be_8_bytes");

    /* ARRAY COUNT */
    int arr[] = {1, -2, 3, 4, -5};
    printf("Array count: %zu\n", FLOW_ARRAY_COUNT(arr));

    /* RESTRICT + LIKELY */
    int total = sum_array(arr, FLOW_ARRAY_COUNT(arr));
    printf("Sum (abs style): %d\n", total);

    /* NOINLINE */
    printf("Noinline test: %d\n", slow_identity(42));

    /* PREFETCH */
    FLOW_PREFETCH(&arr[0]);

    /* MIN/MAX */
    printf("Min(10,20): %d\n", FLOW_MIN(10, 20));
    printf("Max(10,20): %d\n", FLOW_MAX(10, 20));

    /* SWAP */
    int a = 5, b = 9;
    FLOW_SWAP(int, a, b);
    printf("Swap result: a=%d b=%d\n", a, b);

    /* POW2 */
    printf("Is 64 pow2? %d\n", FLOW_IS_POW2(64));
    printf("Is 70 pow2? %d\n", FLOW_IS_POW2(70));

    /* Memory size helpers */
    printf("1 KB = %llu\n", FLOW_KB(1));
    printf("1 MB = %llu\n", FLOW_MB(1));
    printf("1 GB = %llu\n", FLOW_GB(1));

    /* STRINGIFY */
    printf("Stringify test: %s\n", FLOW_TOSTRING(Hello Flow));

    /* CONCAT */
    int FLOW_CONCAT(test_, 123) = 777;
    printf("Concat variable value: %d\n", test_123);

    /* OFFSET + CONTAINER_OF */
    Wrapper w;
    w.header        = 11;
    w.example.id    = 99;
    w.example.value = 3.14;

    printf("Offset of example: %zu\n", FLOW_OFFSET_OF(Wrapper, example));

    Example* e_ptr     = &w.example;
    Wrapper* recovered = FLOW_CONTAINER_OF(e_ptr, Wrapper, example);

    printf("Recovered header via container_of: %d\n", recovered->header);

    /* ALIGN */
    printf("Aligned buffer address mod 64: %llu\n", (unsigned long long)((uintptr_t)aligned_buffer % 64));

    /* Bit operations */
uint64_t mask = __rdtsc();
    printf("Trailing zeroes: %d\n", flow_trailing_zeroes_u64(mask));

    printf("Leading zeroes: %d\n", flow_leading_zeroes_u64(mask));

    printf("Popcount: %d\n", flow_popcount_u64(mask));

    /* ASSERT */
    FLOW_ASSERT(flow_popcount_u64(mask) == 3);

    printf("\nAll tests completed.\n");

    return 0;
}
