# ADR 0009: Combo settings author in CVars and identify by digest; the reverse pool is a second origin-keyed table behind a pre-generation gate

- Status: **Accepted** (2026-07-22)
- For: #498 (combo-level settings), gating #493 (MM -> OoT foreign items); Phase
  3.1 tracker #492, Lane 1
- Depends on:
  - **[ADR 0002](0002-origin-tagged-shared-items.md)** (Accepted) — the origin-tag
    invariant and the `ComboContext` growth contract every carve below obeys.
  - **[ADR 0003](0003-settings-namespace.md)** (Proposed) — CVar key naming; the
    combo keys here are new tier-4 keys, not converged MM keys (ADR 0004 §3).
  - **[ADR 0005](0005-sourced-grant-cursors.md)** (Accepted) — the DO-NOT-BUMP
    prescription and the 672/736 offsets this ADR's budget preserves.
- Amends: nothing. ADR 0002's `ComboForeignPlacement` shape is preserved
  unchanged, which is decision 3's whole point.

## Context

Three decisions belong to #498. All three get made inside #493 whether or not
anyone writes them down, and all three are format-affecting or
determinism-affecting, so they are cheaper to make once than to retrofit:

1. Where combo-level settings (direction, pool size, item class) live.
2. What gates a *reverse* (MM -> OoT) generation pass, given that the pairing
   stamp OoT publishes is written at the very end of `Playthrough_Init`.
3. Whether the cross-game item class is one symmetric pool or two independent,
   origin-keyed pools.

A fourth thing has to be settled alongside them, because five separate work
items are queued to carve from the same 264 bytes: the `reserved[]` budget.

Measured against `claude/lane5-tracker-visibility` on 2026-07-22 by direct read.

---

## Decision 1 — CVars author the settings; a u32 digest carries the identity

**Combo settings are authored in CVars (ADR 0003 tier-4 `gCombo.Rando.*` keys).
The `.redsave` carries a single `uint32_t comboSettingsHash`, not the settings
themselves.**

The two framings in #498 were "per-save carve, so a `.redsave` carries its own
pairing rules" versus "per-install CVars, per ADR 0003's one-store posture".
They are not actually in tension, because they answer different questions:

- The *authoring* question ("where does the player change this?") is settled by
  ADR 0003. A settings surface carved into the save would be a second settings
  store, which is exactly the posture ADR 0003 exists to prevent.
- The *identity* question ("can two peers agree on seed and hash and still get
  different cross-game rules?") does not require storing the settings. It
  requires storing something that **differs whenever the rules differ**. A
  digest does that in 4 bytes; the settings themselves would cost tens and would
  have to be re-versioned every time an option is added.

So the world identity gains one term and the format gains one field. This
mirrors what `sharedRandoSettingsHash` already does for OoT's own settings
(`playthrough.cpp:71`) — an established, already-shipped pattern rather than a
new mechanism.

**Consequence, stated plainly:** a `.redsave` moved to an install with different
combo CVars will *detect* the mismatch (the digest disagrees) but cannot *repair*
it — it cannot tell the player what the original rules were. That is accepted.
Detection is the property that matters; ADR 0002's whole argument is that a
silently-wrong pairing is worse than a loudly-refused one. If un-repairable
mismatch later proves painful, the settings can be carved additively at that
point under the growth contract, and the digest becomes a checksum over them.

**Consequence for the digest:** `Rando_HeadlessSeedDeterminismDigest`
(`3drando/menu.cpp:508`) must fold `comboSettingsHash`, or "changing a combo
setting changes the derived world" is unassertable. That is #498's lock, not
this ADR's.

## Decision 2 — an explicit pre-generation gate; the pairing stamp does NOT move

**Add a pre-generation opt-in read before `Fill()`. Leave the
`sourceIsRando`/`sharedRandoSeed`/`sharedRandoSettingsHash` stamp exactly where
it is, at the end of `Playthrough_Init`.**

The tempting move — hoist the stamp above `Fill()`, since `rsbsSettingsHash` is
already in hand at `playthrough.cpp:71` and `seed` is a parameter — is
mechanically trivial and semantically wrong.

The stamp is a **post-condition**, and its current position is what makes it one.
`Playthrough_Init` reaches line 116 only on a fully successful fill *and* a
successful spoiler write; `Fill()` can return `< 0` and take an early return at
`:82-84`. Hoisting the stamp publishes a paired-world identity for a world that
was never generated: `Combo_ForeignPairingActive()` would then answer "yes,
paired" for a failed generation, and MM would key its own world off a seed and
settings profile that produced nothing. That converts a clean generation failure
into a silently mispaired pair of worlds — the failure class ADR 0002 is written
against.

It also perturbs the determinism digest for no gain: the digest observes when
the Lane B carrier goes live, so moving the stamp changes pinned digests and
costs a re-pin, purely to reuse a field for a purpose it does not have.

The gate a reverse pass actually needs is a different predicate with a different
tense. `Combo_ForeignPairingActive()` means *"a paired world exists"* (past).
A generation pass needs *"a paired world is being asked for"* (future). Those are
two predicates, and conflating them is the reason the hoist looked necessary:

```
Combo_ForeignPairingActive()     -- post-condition, unchanged, read at give time
Combo_ForeignPairingRequested()  -- pre-condition, new, read before Fill()
```

`Combo_ForeignPairingRequested()` is derived from the decision-1 CVar opt-in and
does not read `gComboCtx`'s stamp at all, so it is answerable before generation
by construction and is immune to the ordering hazard entirely.

**Consequence:** a reverse pass can run and place items into a world whose fill
then fails. That is harmless and self-correcting — the placement table lives in
`gComboCtx`, and `Context_InvalidateSessionState` / `ComboContext_Init` wipe it
(`context.cpp:208-231`, `:273-310`); a failed generation never reaches the stamp,
so `Combo_ForeignPairingActive()` stays false and nothing consumes the table.

## Decision 3 — two independent origin-keyed pools; lookup keys on (origin, name)

**Two pools, each defined in the single TU where its enum is in scope. The
name -> item inverse gains an origin dimension. `ComboForeignPlacement` is not
reshaped.**

A single symmetric "cross-game item class v1" is cleaner for a spoiler renderer,
and it is not available, for a concrete reason rather than a stylistic one:

- `kForeignPoolV1` carries the literal display name `"Bomb Bag"`
  (`ForeignItemsSingleExe.cpp:61`).
- MM's `RI_BOMB_BAG_20` row is also literally `"Bomb Bag"`
  (`StaticData/Items.cpp:37`).

`Combo_GetForeignItemByName` (`foreign_items.c:22-44`) is the **spoiler-load
inverse**. Keyed on display name alone across a merged pool, it resolves
`"Bomb Bag"` to whichever entry it scans first and writes a **silently wrong
origin tag** into the placement table — the #356 aliasing class that ADR 0002
exists to prevent, arriving through the one path that reconstructs state from
untrusted text on disk.

The name collision is therefore not a hypothetical to be avoided by careful
naming. It exists today, in the first two pools anyone would write, between two
items that genuinely have the same name in both games. Any rule of the form
"keep display names globally unique" asks two independently-evolving item tables
to coordinate forever, and fails silently on the first violation.

So the uniqueness invariant is stated per-origin, and the origin travels with the
name everywhere the name travels:

> **(originGame, name) is unique.** Bare `name` is not a key. The spoiler's
> foreign section records the origin alongside the name, and the load-side
> inverse takes an origin argument.

**Correction to #493's secondary lock.** #493 asks for an assertion of "no
cross-pool name collision". That assertion is **not satisfiable** and must not be
written: `"Bomb Bag"` collides in the very first MM pool anyone writes, so the
assertion would go red on arrival and the natural fix — renaming one game's item
away from its real name — degrades the spoiler to avoid a lookup bug rather than
fixing the lookup. The correct assertion is `(origin, name)` uniqueness within
and across pools, plus a test that a colliding bare name resolves differently
under each origin. Recorded here because a lock that cannot pass gets "fixed" by
weakening it.

**Why not reshape `ComboForeignPlacement` to `{hostGame, checkId, item}`.** It is
the more elegant table, and it costs an ADR 0002 amendment, a format generation,
a migration hook that does not exist (`save.cpp:262-263` is a flat `memcpy`), and
`RSBS_SAVE_VERSION` past 2 with a stated forward-incompatibility — a 2048-byte
Tier-1 fails `h.comboSize > kComboSize` on every already-shipped binary. A second
parallel table keyed by OoT `RandomizerCheck` costs 48 bytes and no version bump.
The accepted cost of the parallel carve is stated explicitly below, because it is
real and it is the thing most likely to rot.

**Accepted cost of two tables.** Five accessors are duplicated, and *every* clear
/ invalidate / serialize / spoiler site must retire both or they desynchronize
(the #440 class). One mitigation is already in place and load-bearing:
invalidation is not per-field. `Context_InvalidateSessionState` retires session
state by calling `ComboContext_Init()`, which `memset`s the whole struct
(`context.cpp:212`, `:299`), so **any** field carved from `reserved[]` is retired
automatically. The sites that genuinely need pairing are the explicit
`Combo_ClearForeignPlacements` callers, the spoiler write/read pair, and the
determinism digest.

---

## The `reserved[264]` byte budget

`reserved[]` begins at offset 740 and there are 20 further bytes of record slack
(`sizeof(ComboContext)` 1004 vs. `RSBS_COMBO_CONTEXT_RECORD_SIZE` 1024), so
Tier-1 growth capacity is **284 bytes**. Six work items are queued against it.
Publishing the allocation once beats five sequential re-versions:

| # | Claimant | Size | Status |
|---|---|---|---|
| 1 | `foreignPlacementsOoT[8]` (#493 step 2, Lane 1) | 48 B | **carved by this arc** |
| 2 | `comboSettingsHash` (#498 decision 1, Lane 1) | 4 B | reserved |
| 3 | MM option-profile digest (#497 step 7 / #499 step 4, Lane 4) | 4 B | **carved by Lane 4; size CONFIRMED at 4 B** |
| 4 | Netplay grant fields (#460, ADR 0005/0007) | 64 B | reserved |
| 5 | `sharedResources[8]` (#525, shared rupees + hearts + magic) | 32 B | **carved by #525** |
| 6 | `sharedResourcesExt[12]` (#525 optional tier, shared ammo) | 48 B | **carved by #525** |
| 7 | Unallocated floor | 84 B | must not drop below 64 |

Total reserved-against: 152 B of 284. After claims 1, 3, 5 and 6 land, `reserved[]`
is 132 bytes and 152 B of capacity remain.

**Claim 5, settled (#525, 2026-07-29; amended for the optional tier).** Shared
cross-game resources are eight kind-tagged 4-byte slots —
`ComboSharedResource sharedResources[8]` at `.redsave` byte offset **792**,
immediately after `mmProfileDigest`. Seven of the eight are occupied: five by
v1 (rupees, wallet tier, health quarters, current health, double defense) and
two by shared magic (meter level + current magic, kinds 6 and 7), which is why
the claim is sized at the array rather than at any moment's resource count. The
ammo upgrades — the queued remainder of the class — needed nine more kinds and
did NOT fit in the one remaining slot; see claim 6.

**Claim 6, settled (#525 optional tier, 2026-07-29).** Shared ammo arrived
exactly as claim 5's prescription requires: a SECOND block,
`ComboSharedResource sharedResourcesExt[12]` at `.redsave` byte offset **824**,
carved from the front of `reserved[]` and pinned by its own literal-offset
static_assert. `RSBS_SHARED_RESOURCE_CAP` was **not** bumped — a widen in place
would have moved this very block, and every field carved after it, off the
offset every shipped save stored it at. `reserved[180]` becomes `reserved[132]`.

The two blocks are ONE LOGICAL ARRAY of twenty slots. `shared_resources.c`
addresses them through a single `SlotAt(logical)` resolver, and every scan — the
duplicate lookup, the first-free claim, the occupancy count — spans both in one
pass. That is not tidiness: a scan stopping at the first block's boundary would
either split one kind across two slots or report the array full with twelve free
slots behind it, silently dropping every resource past the eighth. Sixteen of
the twenty are occupied (seven from v1 plus magic, nine from ammo); the block
was sized for the whole class in one carve, because a third block would cost
another literal offset, another span in every accessor, and another amendment
here.

Twelve is the ceiling the floor below permits rather than a preference: 48 B
leaves `reserved[132]`, which still clears the 64-byte floor once the
outstanding claims 2 and 4 (4 B + 64 B) land. Sixteen slots would have breached
it.

The tag is load-bearing, not bookkeeping. This ADR's growth contract is "zero
means unset", and **0 rupees is a legal player state** — so a bare `uint16_t
sharedRupees` cannot distinguish a pre-#525 record from a broke player, and would
either resurrect a stale balance or discard a real zero. Occupancy therefore
rides `kind != RSBS_SHARED_RES_NONE`, exactly as `SharedItem`'s rides
`originGame != GAME_NONE`. Do not "simplify" the slots back into scalars.

**Claim 3, settled (Lane 4, 2026-07-23).** The digest is a single `uint32_t
mmProfileDigest` at `.redsave` byte offset 788, computed by
`Rando::Foreign::ResolvePairedProfile()` over the same option string
`MixPairedFinalSeed()` already hashes. 4 B was the placeholder and 4 B is the
answer, so the table above is confirmed rather than amended. A wider *record* of
the profile was rejected for decision 1's reason: the settings are authored in
CVars, and storing them would be a second settings store that has to be
re-versioned every time an option is added.

Two constraints on anyone spending from this:

- **Keep `reserved[]` at 64 bytes or more.** `Test_SaveComboRecordFixed`'s
  scribble loop (`test_save_roundtrip.c:512-538`) iterates `sizeof(reserved)`; at
  zero it degenerates to zero iterations and passes vacuously, retiring the only
  test that proves headroom round-trips at all.
- **Claim 3's size is a placeholder.** Lane 4 must confirm its digest size before
  carving, not after. 4 B assumes a u32 digest by analogy with
  `sharedRandoSettingsHash`; a wider profile record changes the table and this
  ADR should be amended rather than the floor quietly eaten.

Nothing here raises `RSBS_SAVE_VERSION`, `RSBS_COMBO_CONTEXT_RECORD_SIZE`, or
`RSBS_COMBO_CONTEXT_PRECARVE_SIZE`. Every claim is a front-of-`reserved[]` carve
under the ADR 0002 growth contract: prior offsets fixed, zero means unset, a
zero-extended legacy record reads every new field as absent.

The capacity that does **not** exist, so nobody re-derives it hopefully: raising
`RSBS_FOREIGN_PLACEMENT_CAP` in place is a build error as of #490, and a
symmetric ~55-entry carve in both directions would consume the entire budget and
leave nothing for claims 2-4.

## Consequences

- `Combo_ForeignPairingActive()` keeps its meaning and its position; a second,
  cheaper predicate appears next to it. Reviewers must not "simplify" the two
  back into one — decision 2 is the reason they are separate.
- The spoiler format gains an origin field in its foreign section. A spoiler
  written before that field exists loads with an absent origin; the loader must
  refuse such an entry rather than guess, which is decision 3's invariant applied
  to the one path that reads untrusted text.
- `.redsave` format is unchanged in version terms. Two `.redsave` files written
  by builds either side of the `foreignPlacementsOoT` carve interoperate: the
  older one reads the new block as all-unset, which is correct.
- ~~Lane 4 owes a digest size against claim 3 before it carves.~~ Paid: 4 B,
  carved as `ComboContext.mmProfileDigest` at offset 788 (see the budget table).
