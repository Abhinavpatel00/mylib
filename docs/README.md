# `mu` Library Documentation

C99-first collection of data structures, allocators, math, ECS, and concurrency
primitives for engine and tools code. One umbrella header for the core:

```c
#include "mu.h"   /* core: containers, allocators, bitsets, spans, RNG, perf */
```

Subsystems ship as their own single headers:

```c
#include "mu_ecs.h"               /* entities, pools, transforms, prefabs      */
#include "mu_thread_system.h"     /* worker pool + atomics + primitives        */
#include "mu_task_scheduler.h"    /* priorities, dependencies, parallel_for    */
#include "mu_geometric_algebra.h" /* PGA / conformal math (requires cglm)      */
#include "mu_noise_math.h"        /* value/perlin/simplex/worley/fBm/warp      */
#include "mu_roguelike.h"         /* grid gen, FOV, pathing, schedulers        */
#include "allocators.h"           /* arena, pool, buddy                        */
#include "offset_allocator.h"     /* two-level bin free-space allocator        */
#include "mu_mmgr.h"              /* leak/corruption tracking malloc shim      */
#include "mu_min_containers.h"    /* standalone stretchy buffer + hash64       */
```

## Documentation index

| Doc | Contents |
|-----|----------|
| [core.md](core.md) | `mu_common.h` macros, bit intrinsics, `mu_allocators.h` (linear/ring), `mu_perf.h`, `mu_pcg.h` |
| [allocators.md](allocators.md) | `allocators.h` arena/pool/buddy, `offset_allocator.h`, `mu_mmgr.h` |
| [containers.md](containers.md) | `array`, `mu_span`, `mu_string_arena`, `mu_pool_link`/lists/freelists, `mu_min_containers.h` |
| [sets-and-ids.md](sets-and-ids.md) | `mu_bitset`, `mu_sparse_set`, `mu_bulk_storage`, `mu_id_pool`, hash tables (`mu_hash32`, `mu_hash64`), `mu_multi_index`, `mu_chunked_u32_array` |
| [math.md](math.md) | `mu_noise_math.h`, `mu_geometric_algebra.h`, `mu_bitpacking.h` quantization |
| [ecs.md](ecs.md) | `mu_ecs.h`: entity manager, pools, transforms, prefabs, resource registry |
| [concurrency.md](concurrency.md) | `mu_thread_system.h`, `mu_task_scheduler.h` |
| [roguelike.md](roguelike.md) | `mu_roguelike.h`: RNG, grids, generation, FOV, pathfinding, tick scheduler |

## Design rules (apply to everything)

- **C99, single header, no dependencies** except where noted (cglm for GA,
  pthreads/Win32 for threading).
- **Customization via macros, not vtables.** Override `mu_malloc` / `mu_free`,
  `MU_TS_MALLOC`, `MUC_MALLOC`, etc. before including to redirect allocation.
- **`MU_API` / `MU_STATIC` linkage model.** Define `MU_STATIC` to make all
  `mu_*` symbols `static` (single-TU stashing), or `MU_DLL_EXPORT` /
  `MU_DLL_IMPORT` on Windows for shared libraries.
- **Assert on contract violations** (`MU_ASSERT`), never on recoverable
  runtime conditions. Out-of-memory returns `NULL`/`false`; capacity bugs
  assert loudly.
- **Offsets over pointers** wherever identity must survive serialization or
  realloc (indices, generations, handles).

## Build & test

```sh
# core C tests
cc -O2 -I. test.c mu/mu_bitset.c mu/mu_sparse_set.c mu/mu_id_pool.c \
   mu/mu_multi_index.c mu/mu_chunked_array.c mu/mu_bulk_storage.c -o test

# full C++ test suite (bitsets + spans + all containers)
c++ -std=c++17 -O2 -fpermissive -I. test.cpp mu/*.c -o test_cpp
./test_cpp     # Summary: N/N checks passed
```

Note: `mu/mu_perf.h` uses `clock_gettime`; strict `-std=c99` needs
`-D_POSIX_C_SOURCE=199309L` on glibc.
