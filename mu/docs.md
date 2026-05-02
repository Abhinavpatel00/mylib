┌─────────────────────────────────────────────────────────────┐
│                    PICK BY ACCESS PATTERN                   │
├─────────────────────┬───────────────────────────────────────┤
│ Need                │ Use                                   │
├─────────────────────┼───────────────────────────────────────┤
│ Dense hot iteration │ flat array / vector / frame arena     │
│ Stable handles      │ mu_bulk_storage                       │
│ Dense active IDs    │ mu_sparse_set                         │
│ key -> one value    │ mu_hash32_static / hash_t             │
│ key -> many values  │ mu_multi_index                        │
│ packed booleans     │ mu_bitset                             │
│ ranged IDs          │ mu_id_pool                            │
│ variable children   │ mu_chunked_u32_array                  │
│ packed strings      │ mu_string_arena                       │
│ intrusive lists     │ mu_pool_link / mu_index_list          │
└─────────────────────┴───────────────────────────────────────┘
