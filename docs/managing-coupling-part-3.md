# Managing Coupling, Part 3 — Duck Typing in C++ Without Losing Performance

Some systems must operate on objects whose exact shape varies (particles with optional fields, networked game objects with arbitrary properties). Classic C++ inheritance couples layouts and forces heap allocation; we need a more data-oriented form of “duck typing.”

## Three Levels of Flexibility

1) **Exact typing (C style)** — only ducks are ducks. Fast, but inflexible.
2) **Interface typing (classic OOP)** — if it *claims* to be a duck (inherits an interface). More flexible, but forces all participants to share a base type (coupling).
3) **Duck typing** — if it *quacks* like a duck. We care about fields present, not static type.

Dynamic languages default to (3); we can emulate it in C++ with POD layouts instead of vtables.

## Problems with Virtual Inheritance for “Open” Data

- Requires pointers/heap, hurting layout and SIMD/SPU friendliness.
- Non-POD; awkward to serialize, move, DMA.
- RTTI/dynamic_cast adds overhead and coupling to type hierarchies.

## A Data-Oriented Duck-Typed Object

Represent an object as: `[type-tag][raw data bytes...]`.

```
+------------+-------------------+
| type enum  | payload (bytes)   |
+------------+-------------------+
```

Examples of type tags: BOOL, INT, FLOAT, VEC3, QUAT, STRING, ARRAY, DICT.

Access pattern:

```cpp
uint32_t t = *reinterpret_cast<uint32_t*>(o);
if (t == FLOAT_TYPE) {
    float v = *reinterpret_cast<float*>(o + 4);
}
```

Copying, sending over network, or DMA’ing is a memcpy of the block.

## Dictionaries (Open Structs)

A dictionary stores key/type pairs followed by data:

```
+--------+------+------+-----+
| count  | k0,t0| k1,t1| ... |
+--------+------+------+-----+
| data0 data1 data2 ...      |
```

Keys can be 32-bit hashes; collisions are negligible for small structs and can be resolved manually.

### Sharing a Type Description

For many objects of the same “shape,” split type metadata from data:

```
TypeDesc:
  fields: [key, type, offset]...

Data block for each instance:
  [values laid out per offsets]
```

Now an array of such objects matches the footprint of an array of C structs—cache-friendly with zero virtual overhead.

ASCII analogy to vertex buffers:

```
TypeDesc ~ vertex declaration
Data     ~ vertex buffer
```

## When to Use This Pattern

- Particle systems with varying per-effect attributes.
- Network sync of gameplay objects as key/value blobs.
- Serialization of small flexible configs.

## Limitations and Safeguards

- Manual casting requires discipline; misuse usually crashes fast (acceptable in engine code).
- Keys must match between producer/consumer; define them centrally to avoid drift.
- Best for *small* structs; large nested data may be better served by higher-level serializers.

## Extending Further

- You can add function-pointer fields to approach scripting-language objects.
- For SOA layouts, store per-field arrays and keep the same TypeDesc (exercise for the reader).

## Key Insights

- Duck typing in C++ is possible with POD layouts and type tags; no need for base classes.
- Data-oriented approach keeps objects movable (memcpy, DMA, file IO) and cache-friendly.
- The design mirrors GPU vertex data: flexible, declarative types with tightly packed data.
- Minimizing coupling means avoiding imposed base classes; let data shapes be described, not inherited.
