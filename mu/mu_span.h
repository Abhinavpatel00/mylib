#ifndef MU_SPAN_H
#define MU_SPAN_H

#include <stdint.h>

typedef struct ByteSpan {
    const void *data;
    uint32_t    size;
} ByteSpan;

#define BYTE_SPAN(value)                                                                                               \
    (ByteSpan) {.data = &(value), .size = (uint32_t)sizeof(value)}

#endif
