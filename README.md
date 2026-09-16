# mylib (`mu`)

A C99 single-header collection of data structures, allocators, math, ECS, and
concurrency primitives for engine and tools code. One umbrella header:

```c
#include "mu.h"
```

Full documentation lives in [docs/README.md](docs/README.md).

## Pick by access pattern

| Need                     | Use                          |
|--------------------------|------------------------------|
| Dense hot iteration      | `array` / frame arena        |
| Non-owning view          | `mu_span`                    |
| Stable handles           | `mu_bulk_storage`            |
| Dense active IDs         | `mu_sparse_set`              |
| key -> one value         | `mu_hash32` / `mu_hash64`    |
| key -> many values       | `mu_multi_index`             |
| packed booleans          | `mu_bitset`                  |
| ranged IDs               | `mu_id_pool`                 |
| variable children        | `mu_chunked_u32_array`       |
| packed strings           | `mu_string_arena`            |
| intrusive lists          | `mu_pool_link` / `mu_index_list` |

## Layout

- `mu.h` — umbrella header for the core (`mu/` directory)
- `mu/` — core data structures, allocators, math, RNG, profiling
- `mu_ecs.h`, `mu_thread_system.h`, `mu_task_scheduler.h` — engine subsystems
- `mu_geometric_algebra.h`, `mu_noise_math.h`, `mu_roguelike.h` — domain math
- `allocators.h`, `offset_allocator.h`, `mu_mmgr.h` — allocator suite
- `docs/` — detailed per-module documentation

## Building the tests

```sh
cc -std=c99 -O2 test.c -o test && ./test          # C compile smoke test
c++ -std=c++17 -O2 test.cpp -o test_cpp && ./test_cpp
```
