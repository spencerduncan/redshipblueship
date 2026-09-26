# Lane K10 (wave 6) — persist cross-game placements, print them in the one spoiler, rebuild the coordinator's tables from storage (ADR 0010 O7)

**Branch:** `claude/inc3-crossing-persistence-o7`

**The problem.** Under the single bag, how many items each game hosts for the
other is a fill outcome, not a pool size. It can be hundreds of crossings per
direction. Today's crossing tables are sized by `poolSize*`, and the O7 carve
is about a hundred bytes. Each game's own placements already persist in its
own save (OoT `ItemLocation`, MM `RANDO_SAVE_CHECKS`). What has no home yet is
"which hosts of game A hold an item of game B, and which item".

**Task:**

(1) **Decide the storage with evidence**, under one-game semantics and ADR
0002. The options are:
- a new append-only `.redsave` block, versioned through the #569 choke point;
- a foreign sentinel in each game's own check record, pointing into a compact
  list;
- the O7 carve for the bounds only, with the records elsewhere.

MEASURE the live `reserved[]` yourself, and state each option's byte budget.
The layout must survive creation, the armed MM shadow, freeze/restore across
every switch, a `.redsave` load with MM never booted, and the refuse path.

(2) **Build three paths:**
- the write path from the coordinator's placement tables: an API for K11, with
  no production caller yet;
- the hydrate path, which rebuilds both coordinator tables from storage;
- the read path the host game's give uses. Keep the old pinned-pool lookup
  working until K11 retires it.

(3) **Extend the spoiler.** The `"combo"` section gains a per-host crossing
list in both directions, plus a loader that rebuilds storage from the spoiler.
Lock the round trip byte-identical.

(4) **Locks:**
- round trip through storage and through the spoiler;
- the capacity edge: at the cap it stores, one over is refused, never
  truncated;
- freeze/restore is byte-exact;
- a refused pairing leaves storage empty;
- the format version bump.

(5) **Goldens:** untouched.

**Builds:** yes; both tiers.

**Files owned:**
- `src/common/context.{h,c}`;
- the persistence and hydrate functions of `foreign_items.{h,c}` (NOT the
  pool tables);
- new persistence and hydrate functions in both engine TUs, at function
  granularity;
- the `"combo"` section writer of the spoiler in `ForeignItemsSingleExe.cpp`;
- tests and the shared append-only files.

No production wiring (that is K11).

**Keywords:** `Refs #645, refs #660, refs #569, refs #500`.
