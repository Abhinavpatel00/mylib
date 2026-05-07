#ifndef MU_H
#define MU_H

#ifndef mu_malloc
#define mu_malloc(size) malloc(size)
#endif

#ifndef mu_calloc
#define mu_calloc(count, size) calloc((count), (size))
#endif

#ifndef mu_free
#define mu_free(ptr) free(ptr)
#endif

#include "mu/mu_common.h"
#include "mu/mu_allocators.h"
#include "mu/mu_array.h"
#include "mu/mu_bitpacking.h"
#include "mu/mu_bitset.h"
#include "mu/mu_bulk_storage.h"
#include "mu/mu_chunked_array.h"
#include "mu/mu_hash_table.h"
#include "mu/mu_id_pool.h"
#include "mu/mu_multi_index.h"
#include "mu/mu_pcg.h"
#include "mu/mu_perf.h"
//#include "mu/mu_rand.h"
#include "mu/mu_sparse_set.h"
#include "mu/mu_string.h"

#endif /* MU_H */
