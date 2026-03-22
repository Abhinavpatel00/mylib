
# Data Structures Part 1: Bulk Data

This is a deep rewrite of the original article, focused on the design space for storing large numbers of objects in a system that cares about performance, cache behavior, pointer stability, deletion, and predictable memory use.

The central question is simple:

> How should a program store many objects so that creation, deletion, iteration, and querying remain fast?

The answer depends on what you optimize for. There is no single container that is best for every workload. In practice, a good bulk-data container is usually a compromise among:

- iteration speed
- insertion cost
- deletion cost
- memory locality
- stable references
- support for sparse or dense occupancy
- ability to add indices or secondary lookup structures

This article explores the main choices and shows why real-world engine code often evolves into custom storage structures rather than using only `std::vector` or raw linked lists.

---

## The shape of the problem

Suppose you have a large set of gameplay objects, particles, entities, render items, or simulation records. You want to:

- iterate over all live objects every frame
- add new objects dynamically
- remove arbitrary objects
- keep objects in memory compact
- possibly refer to objects from elsewhere in the program

The tension is immediate:

- arrays are great for iteration and cache locality
- linked structures are great for stable references and cheap insertion/deletion
- hash tables are great for lookup but bad for iteration locality
- object-oriented containers are convenient, but often hide expensive memory traffic

The rest of the article is about building a bulk-data container that behaves well under pressure.

---

## The simplest model: a flat array

The most obvious container is a contiguous array.

```c
typedef struct {
    object_t *data;
    uint32_t  n;
    uint32_t  capacity;
} object_array_t;
```

### Why arrays are attractive

Arrays are easy to reason about:

- `a[i]` is `O(1)`
- iteration is sequential
- sequential memory access is cache friendly
- vectorized code likes contiguous data
- the data structure is easy to serialize and debug

ASCII view:

```text
data:

index:   0      1      2      3      4      5
        +------+------+------+------+------+------+
value:  | o0   | o1   | o2   | o3   | o4   | o5   |
        +------+------+------+------+------+------+

n = 6
capacity >= 6
```

### The catch

The costs show up when objects are added and removed frequently:

- inserting in the middle requires shifting elements
- deleting in the middle requires shifting elements, or creating holes
- growing the array may reallocate memory
- pointers into the array may become invalid if the array moves

This means that a raw array is only ideal when:

- the number of items is known up front, or
- growth is rare, or
- the program can tolerate moving objects around

---

## Bulk data and object movement

The moment you store objects in a resizable array, you must decide what happens when the array grows.

### Reallocation example

```text
before:

heap block A
+----+----+----+
| o0 | o1 | o2 |
+----+----+----+

after growth:

heap block B
+----+----+----+----+----+
| o0 | o1 | o2 | o3 | o4 |
+----+----+----+----+----+

block A is freed, objects are copied or moved
```

If outside code holds `object_t *` pointers into the array, those pointers may become invalid after growth.

That is the first big lesson:

> If references must remain valid, either prevent objects from moving, or introduce an indirection layer.

---

## Static arrays and fixed-capacity storage

If you know the maximum number of objects in advance, a fixed-size array can be excellent.

```c
enum { MAX_OBJECTS = 4096 };

typedef struct {
    object_t objects[MAX_OBJECTS];
    uint32_t  n;
} object_pool_t;
```

### Benefits

- no reallocation
- object addresses never change
- simple code
- very predictable memory usage

### Costs

- the maximum size is baked in
- unused capacity is permanently reserved
- if the estimate is wrong, the container fails or wastes memory

This is often fine for small subsystems, but it is less attractive for general-purpose storage.

---

## `std::vector` as a baseline

In C++, `std::vector` is a strong default:

- contiguous storage
- amortized `O(1)` push-back
- familiar interface
- good iteration performance

But `std::vector` is not a universal answer.

### Important caveats

1. **Object relocation may be expensive**

   If elements have non-trivial constructors, destructors, or move operations, reallocation can do more work than a plain byte copy.

2. **Deletion in the middle is expensive**

   `erase()` shifts all later elements.

3. **Pointer stability is weak**

   A `push_back()` that triggers reallocation invalidates pointers and iterators.

4. **Debug behavior may be poor**

   Some library implementations have very slow debug-mode containers.

In performance-sensitive systems, a custom storage layer is often built to make the tradeoffs explicit.

---

## Deletion strategy

Deletion is the first major design fork.

Suppose you remove `a[i]` from an array of live objects.

There are three common strategies:

### 1. Shift everything down

```text
before: [a0][a1][a2][a3][a4]
delete a1
after:  [a0][a2][a3][a4][  ]
```

This preserves order, but costs `O(n)` time in the worst case.

### 2. Swap with the last element

```text
before: [a0][a1][a2][a3][a4]
delete a1
after:  [a0][a4][a2][a3][  ]
```

This is `O(1)` but destroys ordering.

### 3. Leave a hole

```text
before: [a0][a1][a2][a3][a4]
delete a1
after:  [a0][hole][a2][a3][a4]
```

This keeps existing indices stable, but introduces fragmentation and requires bookkeeping to reuse holes.

The right answer depends on whether order matters and whether references must survive deletion.

---

## Swap-delete in practice

If order is irrelevant, swap-delete is often the most practical choice.

```c
void object_array_remove(object_array_t *a, uint32_t i) {
    uint32_t last = a->n - 1;
    if (i != last) {
        a->data[i] = a->data[last];
    }
    --a->n;
}
```

### Why this is attractive

- constant-time deletion
- no holes
- contiguous live range remains compact
- good cache behavior during iteration

### Why this can still be tricky

If external code tracks object positions by index, then moving the last element into the deleted slot changes that element’s index. Any secondary structure that stores indices must be updated.

That means swap-delete is usually paired with some form of handle, indirection, or index-update callback.

---

## Holes and free slots

If you want stable numeric indices, you can keep holes.

```text
slots:
index:   0      1      2      3      4      5
        +------+------+------+------+------+------+
value:  | o0   | free | o2   | free | o4   | o5   |
        +------+------+------+------+------+------+

free list: 1 -> 3 -> ...
```

The container stores either:

- a live object, or
- a free-list node

This is useful when:

- object indices are used as handles
- deletions are common
- stable slot identity matters

### Free-list storage pattern

```c
typedef struct slot_t {
    union {
        object_t object;
        uint32_t next_free;
    } u;
    uint32_t generation;
    bool     occupied;
} slot_t;
```

However, the free-list approach adds complexity:

- holes waste memory until reused
- iteration must skip holes
- object layouts become more complicated
- index validity needs extra rules

---

## Weak pointers and generation counters

If objects can be deleted and their slots reused, then a plain index is dangerous.

Imagine this sequence:

```text
slot 7 -> object A
delete A
slot 7 -> object B
```

If some old code still stores `7`, it may accidentally refer to `B`.

The standard fix is to pair the slot index with a generation counter.

```c
typedef struct {
    uint32_t index;
    uint32_t generation;
} object_handle_t;
```

ASCII view:

```text
slot 7:
+-----------+------------+-----------+
| occupied  | generation | object B  |
+-----------+------------+-----------+

handle:
index = 7
generation = 3

valid only if handle.generation == slot[7].generation
```

### Validation rule

To validate a handle:

1. look up the slot by index
2. check that the slot is occupied
3. compare generations
4. return a pointer only if they match

This gives you a cheap stale-reference detector.

---

## Permanent pointers versus stable handles

There are two broad philosophies:

### Permanent pointer model

Allocate each object in a way that its address never changes.

Pros:

- direct pointers are possible
- easy API for clients

Cons:

- more fragmentation
- more complicated allocation strategy
- harder to compact data

### Handle model

Use integer handles that map to storage slots.

Pros:

- movable storage
- stale references detectable
- can compact or reorganize storage internally

Cons:

- every access involves indirection
- API is slightly more complex

In engine code, handles are often the most robust approach when objects must be deleted and recreated frequently.

---

## Indirection through lookup tables

If the container stores a compact array but you need stable references, an index or hash table can act as a translation layer.

Example:

```text
external handle -> lookup table -> slot -> object
```

ASCII sketch:

```text
handle: [ id=17, gen=4 ]
                |
                v
lookup table:  slot 17 -----> object data
```

This allows the container to reorganize memory while preserving the public identity of objects.

---

## Allocation strategy

Storage layout is only half the story. Allocation policy matters just as much.

### Geometric growth

Most dynamic arrays grow geometrically, often by a factor like 2.

```text
capacity: 8 -> 16 -> 32 -> 64 -> 128
```

This gives amortized constant-time append because resizing happens rarely.

### Why this is good

- few reallocations
- low average append cost
- easy to implement

### Why this may still be wrong

If object creation is interleaved with pointer capture, growth can invalidate references at the worst possible moment.

Example bug pattern:

```text
1. allocate item_1 and store a pointer to it
2. allocate item_2
3. allocation of item_2 grows the array
4. item_1 moves
5. stored pointer to item_1 becomes invalid
```

This kind of failure is dangerous because it may only appear after an unrelated change in workload or content size.

---

## Fixed-size blocks as a compromise

One way to avoid relocation is to allocate storage in chunks rather than as one ever-growing buffer.

```text
block 0: [o0][o1][o2][o3]
block 1: [o4][o5][o6][o7]
block 2: [o8][o9][  ][  ]
```

This gives:

- stable addresses inside each block
- reduced relocation pressure
- moderate locality

But the price is that indexing becomes slightly more complicated.

For example:

```c
object_t *get(object_blocks_t *b, uint32_t i) {
    return &b->blocks[i / b->items_per_block][i % b->items_per_block];
}
```

### Tradeoff summary

- simpler than a linked list
- less compact than a single array
- more stable than a reallocating vector

---

## Array of Structures versus Structure of Arrays

This is one of the most important choices in data-oriented design.

### Array of Structures (AoS)

```c
typedef struct {
    vec3 pos;
    vec3 vel;
    float age;
    float lifetime;
} particle_t;

particle_t particles[MAX_PARTICLES];
```

Memory layout:

```text
[pos vel age lifetime][pos vel age lifetime][pos vel age lifetime]...
```

### Structure of Arrays (SoA)

```c
typedef struct {
    vec3  *pos;
    vec3  *vel;
    float *age;
    float *lifetime;
    uint32_t n;
} particle_set_t;
```

Memory layout:

```text
pos:       [p0][p1][p2][p3]...
vel:       [v0][v1][v2][v3]...
age:       [a0][a1][a2][a3]...
lifetime:  [l0][l1][l2][l3]...
```

### When AoS is better

- most algorithms touch all fields of each object
- object identity matters more than SIMD throughput
- code simplicity is valuable

### When SoA is better

- algorithms touch only a subset of fields
- sequential processing of one field at a time dominates
- vectorization matters
- cache efficiency is critical

### Example

Suppose `tick()` only updates age:

```c
for (uint32_t i = 0; i < n; ++i) {
    age[i] += dt;
}
```

In SoA, this touches only the `age` array.

In AoS, it loads entire objects even though most fields are unused.

---

## Why SoA can be dramatically faster

The speedup comes from several places:

- fewer bytes fetched from memory
- better cache line utilization
- easier SIMD vectorization
- less wasted bandwidth

Example cache-line intuition:

```text
AoS:
  load object 0 -> uses only 1 field, but pulls in many unrelated bytes

SoA:
  load age[0..15] -> every byte is useful for the update
```

That said, SoA is not free.

### The downside of SoA

- code becomes more specialized
- object-level APIs are less natural
- adding or removing fields affects many arrays
- some algorithms become awkward

---

## Hybrid layouts

Many real systems use a hybrid approach.

For example, fields that are always used together stay in a struct, while hot fields used by the tight loop are split out.

```c
typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    vec3 *pos;
    vec3 *vel;
    float *age;
    float *mass;

    char  **name;
    uint32_t *debug_flags;
} particle_store_t;
```

This lets you optimize the hot path without sacrificing all flexibility.

---

## Design checklist for bulk data

When choosing a storage strategy, ask:

1. Do I need stable pointers?
2. Do I care about preserving order?
3. Is deletion frequent?
4. Do I need constant-time lookup by ID?
5. Will algorithms touch all fields or only some fields?
6. Can objects move, or must they stay where they are?
7. Is memory usage more important than simplicity?

There is no universal best answer.

The right choice is usually the one that matches the dominant access pattern of the system.

---

## Conclusion

Bulk data is about engineering the tradeoff between convenience and performance.

The important ideas are:

- contiguous arrays are excellent for iteration
- deletions force a policy decision
- stable references require indirection or non-moving storage
- generation counters make handles safe
- chunked allocation can avoid relocation
- AoS and SoA are different tools for different workloads

The next part of the series builds on this foundation by adding indices, which let us query bulk data efficiently without destroying the locality advantages of arrays.
