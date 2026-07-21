# ADR 0005: Sourced item grants are idempotent by per-source cursor, not by content

- Status: **Accepted** (2026-07-21)
- For: #460 (netplay increment 1a), building on the spike merged in #444
  (`docs/netplay-increment-1-spike.md`)
- Depends on: ADR 0002 (origin-tagged `SharedItem`, the ComboContext growth
  contract), Lane A1's stage/record/redeem machinery (#419/#420), Lane C1's
  give path (#428)
- Composes with (does **not** fix): #440 (cross-game state invalidation on
  reset/new game)

This is the contract the 1b transport work builds against. The seam is
`Combo_SubmitSourcedGrant` and friends in `src/common/shared_items.h`; nothing
in this ADR opens a socket, declares `BUILD_REMOTE_CONTROL`, or picks a
transport.

## Context

The `SharedItem` grant machinery (ADR 0002, Lane A1) is correct today only
because every producer is in-process. The netplay spike (#444 §3) identified
four gaps that any remote grant hits on arrival:

1. `Combo_RecordSharedItem` de-dups by content — an un-redeemed
   `(originGame, id)` match merges. Right for a re-fired in-process producer;
   for a network feed it collapses two peers' identical gifts into one item,
   and makes a retransmit indistinguishable from a genuine second gift.
2. `SharedItem` is 4 bytes with size and member offsets static-asserted as
   `.redsave` format. There is no room for a grant/sequence id in the entry.
3. Redemption fires only at the presence-gated switch-arrival points. A grant
   for the game you are already in waits for a switch that may never come.
4. The 64-slot array drops on overflow with only a `stderr` line — silent loss
   of a peer's gift — and, because entries were never cleared, 64 was a
   lifetime cap per save, not a working-set cap.

Constraints: the ADR 0002 growth contract is absolute (carve from
`reserved[]`, append-only, zero == unset, offsets pinned by static_assert,
dual C/C++ views, no untagged cross-game ids); existing `.redsave` files must
keep loading; everything must be lockable ROM-free in the `redship` CTest
tier.

## Decision

### 1. Grant identity is a per-source monotonic cursor, persisted in gComboCtx

A **source** is a producer with its own identity and its own delivery stream —
a network peer, an Archipelago server, a loopback test feed. Each source
numbers its grants with a dense, 1-based sequence. The save persists one
cursor per source:

```c
typedef struct {
    uint32_t sourceKey; // transport-assigned nonzero source identity; 0 = empty slot
    uint32_t lastSeq;   // highest accepted sequence number from this source; 0 = none
} ComboGrantSourceCursor;
// gComboCtx.grantCursors[RSBS_GRANT_SOURCE_CAP /* 8 */]
```

`Combo_SubmitSourcedGrant(sourceKey, seq, originGame, id)` accepts **strictly
in order**: `seq == lastSeq + 1` records the item and advances the cursor;
`seq <= lastSeq` is `RSBS_GRANT_DUPLICATE` (a retransmit — dropped,
idempotent success); `seq > lastSeq + 1` is `RSBS_GRANT_GAP` (predecessors
missing — the source must resend in order; nothing moves). Every non-accepted
outcome leaves `gComboCtx` untouched.

This makes retransmit-vs-second-gift decidable without widening `SharedItem`:
identity lives in the *stream position*, not in the entry. Two peers gifting
the same item are two sources with independent cursors — two accepts, two
entries. One peer resending is the same `(sourceKey, seq)` — one accept, ever,
across save/load, because the cursor rides the `.redsave`.

**Why a cursor and not a per-entry grantId:** (a) the `SharedItem` layout is
pinned format with no spare field, and a parallel seen-`grantId` set is
unbounded state that would itself need serializing; (b) a cursor is O(1)
durable state per source and one comparison per submit; (c) it hosts an
**externally owned cursor directly** — Archipelago's `ReceivedItems` is
server-authoritative with a monotonic `index`; an AP transport maps index `i`
to `seq i + 1`, hands `Combo_GetGrantCursor(key)` to `Sync` as its resume
point, and the spike's resync trap (a full re-delivery being silently eaten by
content de-dup) becomes the *designed* path: the already-seen prefix returns
`DUPLICATE` by cursor, the novel tail records. A grantId-set design cannot
host a foreign cursor; this one is a superset of it.

Strict in-order acceptance is deliberate: it is what makes received order
well-defined (§4), turns message loss into a visible `GAP` instead of silent
reordering, and matches both planned source shapes (AP's index is dense;
a P2P feed numbers per-sender). A source that cannot replay in order is not a
source; it is a bug surfaced early.

`RSBS_GRANT_SOURCE_CAP` is 8: an AP server is *one* source regardless of room
size, P2P co-op is one per peer. Exhaustion is explicit
(`RSBS_GRANT_NO_SOURCE_SLOT`), never silent. Growing the table later is a
legal reserved[] carve.

### 2. The two producer classes never share an idempotency domain

- **In-process producers** (Lane C1's give path, the staged/commit path) keep
  `Combo_RecordSharedItem` and its content de-dup unchanged — a re-fired local
  event is the same event.
- **Sourced producers** go through `Combo_SubmitSourcedGrant` only, which
  appends **without** content de-dup.

Entries recorded by the sourced path carry a new flag bit,
`RSBS_SHARED_ITEM_SOURCED` (0x02), and content de-dup **skips** flagged
entries. Without this, a peer's pending gift of item X would swallow a genuine
local pickup of X (the merge would eat the local event). Zero == unset holds:
every legacy entry was in-process, which is exactly what a zero bit says.

### 3. Overflow: reclaim redeemed entries; refuse loudly; backpressure sources

When the array is full, recording first **reclaims the oldest REDEEMED
entry**: the array compacts (preserving relative order — occupied entries
always form a prefix, so slot order stays acceptance order) and the freed tail
slot takes the new record. A redeemed entry is an informational record of a
completed crossing; an un-redeemed entry is an undelivered item. Only the
former is evictable; the latter is **never dropped**.

> **Amendment to A1's contract:** shared_items' previous "entries are never
> cleared; they remain as the durable record of the crossing" weakens to
> "…until capacity pressure reclaims redeemed entries, oldest first". Nothing
> in the tree reads redeemed entries as a history (verified: consumers filter
> them out; tests use them transiently), and single-use never depended on the
> entry's continued presence — an evicted entry cannot re-award because
> re-recording it requires a fresh producer event by definition. Slot indices
> returned by record APIs are therefore transient; callers must not store
> them (no caller does).

If every entry is un-redeemed, the record is **refused loudly**:

- `gComboCtx.sharedItemOverflowCount` (new, serialized, zero == unset)
  increments — a durable, tracker-surfaceable signal that capacity refused
  work; it survives save/load and resets only with the world.
- For a **sourced** grant the refusal is `RSBS_GRANT_RETRY_FULL` and the
  cursor does **not** advance: the grant remains owed by the source, and its
  later retransmit of the same seq is *accepted*, not treated as a duplicate.
  Backpressure, not loss.
- For an **in-process** record the refusal (return -1) *is* a lost item — the
  pickup already happened in-game. The counter is what makes that loss
  diagnosable instead of a `stderr` line nobody sees. With reclamation, this
  requires 64 simultaneously *undelivered* items, which the working set can
  no longer reach by mere accumulation.

This is the honest middle the increment asked for: the game never blocks, no
peer's gift is silently lost (sourced grants are retried by contract), and the
one genuinely lossy corner (in-process producer against a fully-undelivered
array) is durable-signaled and CI-locked.

### 4. Redemption safe points — "next switch only" generalizes to "next safe point"

`Combo_RedeemSharedItemsForGame` is the **single** redemption entry and is
safe wherever all of the following hold for the target game: it is the active
game, on the game thread; a save is loaded and normal gameplay has been
reached (so the award callback's give machinery is valid); the caller passes
that game's real award callback. `RSBS_SHARED_ITEM_REDEEMED` makes redemption
idempotent under any interleaving of safe points, and awards fire in slot
order == acceptance order (**received-order redemption is a contract**: the
give paths resolve progressives against the live save, so order changes what
the player gets; reclamation preserves it).

Safe points, precisely:

1. The two presence-gated startup-entrance consumption points
   (`OoT_ConsumeSharedItems`, `MM_ConsumeSharedItems`) — wired today,
   unchanged.
2. A gameplay-gated frame tick (`OnGameFrameUpdate`-class, gated on "save
   loaded and in normal gameplay") — **defined here, wired by 1b** together
   with the first producer that can target the active game mid-session.
   Deliberately not wired now: every in-process producer records for the game
   being *left*, so today a tick would have nothing to redeem, and its gate
   ("normal gameplay") is game-side state that only an in-game path exercises
   — dead code with ROM-only risk. The model side — that redemption needs no
   switch machinery whatsoever — is what this increment locks in CI.

Plain-load semantics are unchanged for everything wired today and the header
now states the general rule: un-redeemed entries persist and are awarded at
the next safe point. Redeem-at-load remains forbidden for the same reason as
before — the give machinery is not yet valid.

### 5. Save-format delta rides the ADR 0002 growth contract; no version bump

Carved from the FRONT of `reserved[332]`, which shrinks to `reserved[264]`:

| Field | Offset | Size |
|---|---|---|
| `grantCursors[8]` | 672 | 64 |
| `sharedItemOverflowCount` | 736 | 4 |

Offsets pinned by the `context.h` static-assert chain (contiguous with
`foreignPlacements`); zero == unset for every member ("no source has ever
delivered; no refusal has ever happened" — exactly what a zero-extended
pre-netplay record must mean). `sizeof(ComboContext)` stays 1004 ≤ the fixed
1024-byte Tier-1 record, so `RSBS_SAVE_VERSION` stays 2 and
`COMBO_CONTEXT_VERSION` stays 1 (nothing old needs distinguishing, only
extending). Migration is locked by a crafted pre-carve record test in the
`test_save_roundtrip.c` style (`Test_GrantPersistence` part b).

### 6. Composition with #440 (not a fix for it)

Cursors live in the same Tier-1 record as the item array **on purpose**:
`ComboContext_Init` — the invalidation primitive #440's fix will invoke on
reset/new-game — retires items, cursors, and the overflow count in one memset.
That atomicity is load-bearing in both directions: a surviving cursor without
its items would refuse re-delivery into the world that lost them
(`DUPLICATE`); surviving items without their cursor would re-accept a dead
room's stream. After invalidation, a dead-room retransmit is a `GAP` (new
sources must start at seq 1), never a silent acceptance — locked in
`Test_GrantPersistence` part c. #440 itself (making reset/new-game actually
*call* the invalidation) stays with its own fix; when it lands, remotely
sourced grants are covered by the same call this ADR's tests already pin.

### 7. The seam a transport writes against

```c
ComboGrantResult Combo_SubmitSourcedGrant(uint32_t sourceKey, uint32_t seq,
                                          GameId originGame, uint16_t id);
uint32_t Combo_GetGrantCursor(uint32_t sourceKey); // resume point: deliver from cursor+1
int      Combo_CountGrantSources(void);
uint32_t Combo_GetSharedItemOverflowCount(void);
```

Contract highlights for 1b:

- **Game thread only.** A transport receiving off-thread marshals to the game
  thread before submitting. (The vendored Anchor client's
  network-thread-mutation hazard — spike §6 — must not be reproduced against
  this seam.)
- `sourceKey` is a nonzero identity *within the current world/session*; the
  transport derives it (e.g. hash of room id + peer slot; an AP server is one
  source). Cross-session collisions are handled by invalidation (§6), not by
  key hygiene.
- `seq` is dense and 1-based per source. On reconnect or after a `.redsave`
  load, resume from `Combo_GetGrantCursor(key) + 1`; on `GAP`, resync/resend
  in order; on `RETRY_FULL`, re-offer later — delivery is the source's
  obligation until `ACCEPTED` or `DUPLICATE`.
- Delivery of a grant to the *player* still happens via
  `Combo_RedeemSharedItemsForGame` at a safe point (§4); the transport never
  awards anything itself.

## Rationale

1. **Idempotency must be decidable at the API, not probable at the wire.**
   Retransmits are a fact of any transport; second gifts are a fact of
   multiworld. Any design that cannot tell them apart locally is wrong under
   exactly the load netplay creates.
2. **Own as little identity as possible.** The cursor stores one u32 per
   source and no per-grant identity, which is why an external
   server-authoritative cursor (AP) can *be* the identity. Designs that stamp
   every entry force a mapping layer under every transport forever.
3. **Losing data loudly beats both losing it silently and blocking.** The
   durable counter + non-advancing cursor is the smallest mechanism where
   every loss is either prevented (sourced) or signaled (in-process).
4. **The wrong producer path should be structurally awkward.** Sourced
   producers get their own entry point and their entries are flagged; content
   de-dup cannot reach them even if someone routes wrongly later.

## Consequences

- `sizeof(ComboContext)` stays 1004; `reserved` headroom drops 332 → 264.
- New CI locks (all ROM-free, registered as CTest rows): `GrantIdempotency`,
  `GrantRedeemNoSwitch`, `GrantOverflow`, `GrantPersistence`.
- `shared_items.h` is the seam's documentation of record; its file header now
  defines the producer classes, safe points, capacity policy, and threading
  rule stated here.
- The spike's §8 loopback-pair row ("two in-process client objects over an
  in-memory channel") becomes 1b's first test: it needs a transport object,
  which is exactly the piece this ADR deliberately does not build. Everything
  semantic beneath it is locked here.
- A1's "never cleared" note is amended per §3; ADR 0002 is otherwise
  untouched and its growth contract fully honored.

## Alternatives considered

- **Widen `SharedItem` with a grantId/sender/seq.** Rejected: the 4-byte
  layout with pinned offsets is shipped `.redsave` format (ADR 0002); a
  parallel wide array duplicates the store and still needs the cursor logic
  for external sources.
- **A persisted seen-`grantId` set.** Rejected: unbounded, needs its own
  eviction policy (which reintroduces the duplicate window it exists to
  close), and cannot host a server-authoritative cursor.
- **RAM-only cursors.** Rejected: a save/reload re-opens the duplicate
  window — the spike called this trap out explicitly for the AP resync case.
- **Out-of-order acceptance (advance cursor past gaps, or track a bitmap).**
  Rejected: silently reorders progressive-item resolution, turns message loss
  into undetectable item loss (jump) or unbounded state (bitmap), and no
  planned source needs it.
- **Growing `RSBS_SHARED_ITEM_CAP` instead of reclaiming.** Rejected as the
  *primary* fix: any fixed cap without reclamation is a lifetime cap per
  save; reclamation makes the cap a working-set bound. Raising the cap later
  remains a legal, compatible carve if the working set ever demands it.
- **Wiring the redemption tick now.** Rejected for this increment: no
  producer can target the active game yet, so the wiring would be untestable
  dead code in the ROM-gated tier; the safe-point contract it must obey is
  defined and locked instead.
