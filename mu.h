#pragma once
#ifndef MU_H
#define MU_H

/*
 * mu.h — umbrella header for the core `mu` library.
 *
 * All macros are provided by mu_macros.h. Override any MU_* macro before
 * including this header to redirect allocation, assertions, etc.
 */

#include "mu_macros.h"

/* Legacy lowercase override names, kept as aliases of the unified macros. */
#ifndef mu_malloc
#  define mu_malloc MU_MALLOC
#endif
#ifndef mu_calloc
#  define mu_calloc MU_CALLOC
#endif
#ifndef mu_realloc
#  define mu_realloc MU_REALLOC
#endif
#ifndef mu_free
#  define mu_free MU_FREE
#endif

#include "mu/mu_common.h"
#include "mu/mu_allocators.h"
#include "mu/mu_array.h"
#include "mu/mu_bitpacking.h"
#include "mu/mu_bitset.h"
#include "mu/mu_hier_bitset.h"
#include "mu/mu_chunked_bitset.h"
#include "mu/mu_bloom.h"
#include "mu/mu_bulk_storage.h"
#include "mu/mu_chunked_array.h"
#include "mu/mu_hash_table.h"
#include "mu/mu_id_pool.h"
#include "mu/mu_length_index.h"
#include "mu/mu_multi_index.h"
#include "mu/mu_pcg.h"
#include "mu/mu_perf.h"
#include "mu/mu_span.h"
#include "mu/mu_sparse_set.h"
#include "mu/mu_string.h"

/* mu/mu_rand.h is intentionally not part of the umbrella; include it directly
   if you want its competing RNG alongside mu_pcg32. */

#endif /* MU_H */
