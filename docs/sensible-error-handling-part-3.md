# Sensible Error Handling, Part 3 — Warnings That Get Fixed

Warnings are situations that are suspicious but not outright errors: questionable data, expensive content, deprecated interfaces. Left unmanaged, they turn into scroll-by noise; handled well, they guide teams to fix risks before they ship.

## Types of Warnings

- **Performance warnings**: potentially slow (textures without MIPs, 300 MB audio, 1,000,000-particle effects).
- **Suspicion warnings**: likely unintentional (duplicate node names, empty font, unit below terrain).
- **Deprecation warnings**: old patterns we intend to eliminate (legacy naming conventions, deprecated script APIs).

## When Warnings Matter

Two moments really count:

1. While editing a specific asset — show all warnings for that asset in the tool.
2. During focused reviews (performance, memory, content quality) — browse warnings by category.

## Strategy 1: Turn Warnings into Errors

Errors get fixed; warnings linger. Whenever feasible, promote a warning to a hard error. Examples:

- Duplicate node names → error.
- Asset driven simultaneously by animation and physics → error.

### Limitation

Legacy data may block immediate promotion; that’s a deprecation problem, not a reason to keep the warning forever.

## Strategy 2: Eliminating Deprecation Warnings

Goal: eradicate legacy usage and delete backward-compatibility code paths.

1) **Write conversion scripts**

- Convert legacy data formats to the new one (JSON-friendly pipelines help).
- Even partial automation (98%) can slash manual cleanup from weeks to hours.

2) **Script overrides for deprecated APIs**

- Reimplement deprecated engine calls in script using the new API:

```lua
function AudioWorld.set_listener(pos)
    AudioWorld.set_listeners({pos})
end
```

- Lets gameplay code migrate at its own pace without engine support for the old API.

3) **Doomsday clock**

- Visible warning with a hard cutoff date: “This warning becomes an error on 2026-05-01.”
- Requires producer buy-in; deadline enforces cleanup.

4) **Surrender gracefully**

- If legacy content is too costly to fix, prevent *new* bad data:

```json
bad_name_is_error = true
```

- Tools set the flag for new assets; compiler treats flagged assets’ issues as errors, legacy stays warning-level.

## Strategy 3: Design Tools to Prevent Warnings

Warnings signal tool UX gaps. Use them to improve authoring flows:

- Display overdraw meters in particle editor if overdraw warnings are common.
- Enforce unique-name generation in scene editors to avoid duplicates by construction.

## Strategy 4: Surface Warnings Where They’re Useful

- Tools should show a warning badge/count for the currently edited object; optionally require acknowledgment before save/export.
- Runtime warnings are secondary safety nets, not the primary channel.

## Strategy 5: Build a Warning Review Tool

Aggregate warnings by category and asset for periodic audits:

- “Potentially expensive particle systems” → list systems with > N particles, sorted.
- “Possibly invisible units” → list units below ground.
- Allow producers to mark warnings as accepted; hash (object, message) to hide acknowledged cases.

## Key Insights

- Warnings are actionable debt; unbounded accumulation renders them useless.
- Promote to errors whenever possible; for deprecation, migrate content or set deadlines.
- Show warnings at the point of authoring; reviews catch systemic issues.
- Use tooling and UX to prevent common warning classes from arising at all.
