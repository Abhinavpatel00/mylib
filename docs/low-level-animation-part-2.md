# Low-Level Animation, Part 2 — Streaming Layout for Cache-Friendly Playback

This part delivers on the promise to pack compressed animation data so evaluation touches as few cache lines as possible.

## Recap

Each track (one channel of a bone: position or rotation) becomes a sequence of curve points `(t_i, A_i)` after fitting. Different tracks have different timestamps.

```
Track A: (t0,A0) (t1,A1) ...
Track B: (s0,B0) (s1,B1) ...
```

To evaluate at time *t* you need the bracketing points in each track.

## Naïve Layout Hurts Cache

Sorting by track then time leads to at least one cache miss per track per frame. With ~200 tracks per character, that’s unacceptable.

## Organize Around “Hot” Data

At any time *t* only the current segment per track is needed.

```
Active array per playing clip:
[track0: t_i,A_i | t_i+1,A_i+1]
[track1: s_j,B_j | s_j+1,B_j+1]
...
```

Evaluation reuses this active array until the earliest next-knot boundary.

Benefits:
- Evaluation = single contiguous block.
- Fetching new points becomes a separate, infrequent step.

## Streaming the Future Points

We know exactly *when* each future point will be needed: `t_{i+1}` for track A, `s_{j+1}` for track B, etc. Put all points from all tracks into one linear stream sorted by “needed-at time”.

```
Animation stream (by needed time):
[(time=t1) A1] [(time=s1) B1] [(time=t2) A2] ...
```

Playback keeps a single pointer into this stream:

1. Advance time.
2. While `stream.time <= now`, copy that point into the active array for its track.
3. Evaluate curves using only the active array.

ASCII flow:

```
stream_ptr -> [t1,A1][s1,B1][t2,A2]...
             ^ copy when time passed
active -> [track A seg][track B seg]...
```

Cache wins:
- Fetch = pointer bump (sequential).
- Eval = contiguous active array.
- Total accesses: ~2 cache lines.

## Jumping in Time

Linear streaming forbids random access. Add sparse **jump frames**:

```
JumpFrame {
  time;
  active_state_snapshot;
  stream_offset;
}
```

To seek: pick nearest jump frame ≤ target, restore active state, set stream pointer, then fast-forward.

Tradeoff: more jump frames → more memory, faster seeks.

## Compression Bonus

Streaming order enables gzip/delta compression and direct disk streaming (sequential reads). SPU/DMA friendly.

## Edge Cases / Limitations

- Only supports forward playback between jump frames.
- Variable-size payloads per track require careful alignment/padding in the stream.
- Needs per-track metadata to map stream entries back to active slots.

## Key Insights

- Treat “what do I need *now*?” separately from “what will I need later?”.
- One active array + one linear stream collapses random access into two sequential buffers.
- Jump frames give controlled random access without sacrificing streaming.
