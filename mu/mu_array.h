

#pragma once

#include "mu_common.h"

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
#define array_clear(a) ((a) ? (array_header(a)->size = 0) : 0)
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
