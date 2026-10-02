# ADR 0002: Cross-game items are origin-tagged structs; the dead ComboContext fields retire in place

- Status: **Accepted** (2026-07-20)
- For: #392 (Phase 3.0 tracker), Lane A serial section (A0)
- Depends on: the ComboContext serialization headroom shipped in #399
  (`RSBS_COMBO_CONTEXT_RECORD_SIZE`, the growth contract in
  `src/common/context.h`)

Everything below is **final** for Phase 3: Lanes A1 (producers/consumers),
B (unified seed), and C (foreign items) build on these decisions without
re-litigating them.

## Context

Phase 3's MVP moves items between two randomizers that were never designed to
meet: OoT items are `RandomizerGet` (`RG_*`,
`games/oot/soh/Enhancements/randomizer/randomizerTypes.h`) and MM items are
`RandoItemId` (`RI_*`, `games/mm/2s2h/Rando/Types.h`). Both are dense enums
from 0 that fit comfortably in a `uint16_t` — which is exactly the danger: a
raw `uint16_t` holding an `RG_*` value is indistinguishable from one holding an
`RI_*` value, and an id from one game interpreted in the other is the #356
entrance-id crash class all over again, this time written into save files.

The carrier for cross-game state is `ComboContext` (`gComboCtx`,
`src/common/context.h`), a process-global shared by both games in the single
exe and serialized as the fixed 1024-byte Tier-1 record of every `.redsave`.
Its growth contract (established with #399): new fields are carved from
`reserved[]` or appended — never inserted — and **zero must be a valid "unset"
for every new field**, because a shorter legacy record is loaded as a byte
prefix and zero-extended.

`ComboContext` also carries dead weight whose fate had to be decided before
Lane A wires anything: `sharedItems[32]` (`uint16_t`, never wired),
`sharedFlags[64]` (`uint32_t`, never wired), `saveSlot` (never assigned), and
`sourceIsRando`/`sharedRandoSeed` (serialized, produced/consumed only by dead
or excluded code paths). All of their offsets are shipped: bytes at those
positions exist in every `.redsave` written since the format landed.

`context.h` is compiled as both C and C++ (game C TUs like `z_play.c` on one
side, `save.cpp`/`GameExports_SingleExe.cpp` on the other), so whatever type
system enforces the tagging has to work identically in both languages.

## Decision

### 1. The shared item representation: a plain tagged struct, no bit-packing

```c
typedef struct {
    uint8_t originGame;  // GameId that owns the id-space; GAME_NONE (0) = empty slot
    uint8_t flags;       // RSBS_SHARED_ITEM_* bits; 0 = default (present, not redeemed)
    uint16_t id;         // RG_* if originGame==GAME_OOT, RI_* if GAME_MM; 0 when empty
} SharedItem;
```

- **4 bytes, fixed layout**, locked by static asserts in both language views
  (`sizeof == 4`, member offsets 0/1/2). These bytes are `.redsave` format.
- **No bit-packing.** A game tag packed into the high bits of a `uint16_t`
  makes a raw integer read *almost* work — the precise failure mode of the
  #356 entrance-id leak. As a struct, assigning a raw integer into a
  `SharedItem` is ill-formed in **both** C and C++ (neither language converts
  arithmetic types to struct types). The C++ view additionally carries
  `static_assert(!std::is_convertible<int, SharedItem>)` /
  `!std::is_assignable<SharedItem&, int>` probes so nobody can later add a
  converting constructor; the C view needs no probe because C has no
  user-defined conversions at all.
- **No `enum class`, no constructors** — the type must be identical in the C
  view. A plain struct is the whole mechanism.
- **Zero means unset**, member by member: `originGame == GAME_NONE` marks an
  empty slot, `flags == 0` is the default state, `id == 0` in both id-spaces is
  the respective "none" enumerator (`RG_NONE`, `RI_UNKNOWN`). A zero-extended
  legacy record therefore reads as "no shared items", which is correct.
- **`flags` bit 0 is `RSBS_SHARED_ITEM_REDEEMED`**: set by the origin game's
  consumer when it actually awards the item, so a round trip can never award
  an entry twice (Lane C's redemption flow). Remaining bits are reserved;
  any future bit must keep 0 == unset.
- **Storage: `SharedItem sharedItemsTagged[64]`** (`RSBS_SHARED_ITEM_CAP`),
  carved from the **front** of `reserved[640]`, which shrinks to
  `reserved[384]`. Every previously shipped field keeps its offset; the array
  begins exactly where `reserved` used to (offset 364, pinned — see below).
  64 entries is deliberately generous, sized once; the MVP needs a handful.
- **No count field.** A slot is occupied iff `originGame != GAME_NONE`. A
  count would be a second source of truth that a zero-extended legacy record
  could contradict.

### 2. The dead fields: retire in place, never delete

Offsets are shipped format; deletion or retyping would break the byte-prefix
property every existing `.redsave` relies on, and renaming would break two
test TUs outside A0's ownership (`test_roundtrip_integrity.c`,
`test_shared_state_roundtrip.c`). So:

- **`sharedItems[32]` — RETIRED IN PLACE.** Never wired, and its untagged
  `uint16_t` shape is the aliasing hazard this ADR exists to prevent. The
  bytes stay; nothing may ever wire it. `sharedItemsTagged` is its
  replacement.
- **`sharedFlags[64]` — KEPT.** Origin-neutral event bits have no id-space
  aliasing problem; the shape is fine. Which bit means what is assigned by
  Lane A1 and later; until something writes a bit, every word stays zero.
- **`saveSlot` — RETIRED IN PLACE.** Dead plumbing; `ComboContext_Init` keeps
  stamping -1 (shipped behavior, harmless), nothing may wire it.

### 3. `sourceIsRando` / `sharedRandoSeed` are Lane B's carrier — explicitly

Yes: the brief's default is confirmed. Both fields exist, serialize in every
`.redsave`, and are dead (their only writers are the legacy `OoT_FreezeState`
— compiled, zero callers — and MM's excluded `BenPort.cpp`). **Lane B revives
them as-is**: written in the live path at OoT generation time (not at freeze
time), read by MM's consumption path when Lane C makes it reachable. No new
seed fields; no shape change. If B needs a settings digest beyond the seed, it
carves that from `reserved[384]` under the same growth contract.

> **Note added by #598 (record kept, fact corrected).** The parenthetical above
> is wrong on one detail and stale on another. `OoT_FreezeState` was never
> *compiled*: its guard was `#ifdef SINGLE_EXECUTABLE_BUILD`, a CMake option
> name that is never turned into a compile definition (the single-exe define is
> `RSBS_SINGLE_EXECUTABLE`). And it no longer exists — #598 deleted it, so that
> a future reader grepping for a freeze-time writer of `sharedRandoSeed` finds
> no precedent to imitate. The decision itself stands unchanged: the ONLY
> sanctioned writer of the identity stamps is the creation event
> (`3drando/playthrough.cpp`), which is what "written in the live path at OoT
> generation time (not at freeze time)" already said.

### 4. Version knobs: neither moves

- `RSBS_SAVE_VERSION` stays **2**. It bumps only with
  `RSBS_COMBO_CONTEXT_RECORD_SIZE`, which is unchanged (the struct grew from
  1004 bytes toward the same 1024-byte budget; the on-disk record length did
  not move).
- `COMBO_CONTEXT_VERSION` (inner content semantics, `context.cpp`) stays
  **1**. The carve introduces no semantic that zero-as-unset does not already
  carry; bump it only when old content must be *distinguished*, not merely
  extended.

### 5. The legacy-record test pin

`Test_SaveComboLegacyRecord` used to derive its "legacy" Tier-1 length as
`offsetof(ComboContext, reserved)`. The carve moves that offset forward past
the new array — the test would stay green while silently redefining "legacy"
to include the carved fields, no longer exercising the true shipped prefix.
The pre-carve length is now a pinned constant,
`RSBS_COMBO_CONTEXT_PRECARVE_SIZE` (**364**), tied to the live layout by
static asserts (`offsetof(ComboContext, sharedItemsTagged) == 364`, and the
tagged array contiguous with the remaining headroom). Any layout drift —
compiler surprise or accidental field insertion — is a build break, not a
silently weakened test.

## Rationale

1. **Make the wrong thing unrepresentable at compile time.** The failure mode
   this phase fears most — an id from one game read in the other — compiles
   clean under any integer encoding and corrupts saves months later. The
   struct makes the untagged read a compile error in both languages, which is
   the only enforcement layer that runs everywhere the type goes.
2. **Zero-as-unset falls out of the shape.** `GAME_NONE == 0` was already the
   sentinel game id; making it the occupancy marker means the growth
   contract's zero-extension rule needs no special case.
3. **Retire-in-place is the only move that respects shipped bytes.** The
   prefix-load scheme means offsets are API. Reuse of `sharedItems`' bytes for
   a differently-shaped field was considered and rejected — a legacy record
   would alias old u16 ids into the new interpretation.
4. **Reviving the seed fields costs nothing and unblocks B now.** They are
   already the right shape (u32 seed + bool), already serialized, already
   round-trip-locked by tests.

## Consequences

- `sizeof(ComboContext)` stays 1004 (≤ 1024 budget); ~20 bytes of record slack
  plus `reserved[384]` remain for A1/B/C carves.
- New coverage: `SaveTaggedItems` CTest (byte-exact round-trip of the tagged
  array, unset slots stay unset) and the extended `SaveComboLegacyRecord`
  (pre-carve record loads with every tagged slot reading unset).
- Housekeeping landed with this ADR, as `context.h`'s owner: the dangling
  legacy declarations `Context_ProcessSwitch` / `Context_IsSwitchInProgress`
  are deleted (their implementations left `switch.cpp` earlier; see its header
  comment), the stale `context.cpp` note claiming switch.cpp is
  non-single-exe-only is corrected, and `docs/known-issues.md`'s dead-plumbing
  entry now reflects this ADR.
- Lane A1 wires producers (both suspend paths + the entrance-switch hook) and
  consumers (both startup-entrance consumption points) against
  `sharedItemsTagged` only. Lane C's give path stores foreign items here with
  `originGame` set to the item's owner and redeems via
  `RSBS_SHARED_ITEM_REDEEMED`. Raw `RG_*`/`RI_*` integers must not cross a
  game boundary outside a `SharedItem`.

## Alternatives considered

- **Bit-packed `uint16_t` (tag in high bits).** Rejected: a packed id *almost*
  works as a raw integer — the #356 failure mode — and 16 bits minus tag bits
  leaves the id-space cramped for no benefit.
- **C++ wrapper type (`enum class` id + explicit constructor).** Rejected: the
  type must exist identically in the C view of `context.h`; a C++-only device
  enforces nothing in the C TUs that touch `gComboCtx`.
- **Widening `sharedItems` in place to the tagged shape.** Rejected: same
  offset, different element layout — a legacy record's u16 ids would be
  reinterpreted as tag/flag bytes. Retire-in-place plus a fresh carve keeps
  old bytes meaning what they always meant.
- **A unified cross-game item namespace (single enum spanning both games).**
  Rejected for 3.0: requires a hand-maintained mapping table that drifts
  against both upstream enums, and the origin tag still ends up encoded — just
  implicitly, in ranges. The tag might as well be explicit bytes.
- **Deleting the dead fields and bumping `RSBS_SAVE_VERSION`.** Rejected: it
  orphans every existing `.redsave` (or demands a real migration path) to save
  68 bytes that the 1024-byte fixed record doesn't even give back.

## Amendment 2026-09-30 — tracker rows: origin-tagged, status projected, no raw passthrough (#458)

**Why an amendment.** The unified trackers (#458) show both games' checks and items in one common-owned
window, so rows computed from each game's data cross into `src/common` for display. The Consequences rule
above ("raw `RG_*`/`RI_*` integers must not cross a game boundary outside a `SharedItem`") was written for
items that move between games; a tracker row does not move, it is drawn. This amendment says how the rule
applies to rows. The operator took the #458 recommendation on 2026-09-30: amend before the item view
(#458 U1) merges; raw passthrough stays deferred; rows are origin-tagged, never a merged id space.
Nothing decided above changes.

**1. Rows are origin-tagged by their source, never merged.** Each game registers its own tracker adapter
from a TU where its layout and enums are in scope, and every read takes the `GameId`: the game argument is
the tag. A row's game-local id (`ComboTrackerCheckRow.checkId`, an MM `RandoCheckId` or an OoT
`RandomizerCheck`) is meaningful only in its own game's panel. It is never compared across games, stored
across games, or used as a key outside that panel. An item row carries no game-local item id at all
(no `ITEM_*`, `RG_*` or `RI_*`): what the row shows (group, name, icon key, have, count, max) is resolved by
the adapter in its own TU. An icon key is an opaque string the adapter produced (OoT keys its icons by
enum name, MM by resource path); common code hands it to the texture loader and never parses it. There is
no common id space for checks or items and no table mapping one game's ids to the other's: the unified
namespace rejected under "Alternatives considered" is rejected for checks and `ITEM_*` too. The shared
resources (`Combo_GetSharedResource`, `src/common/shared_resources.h`) are origin-neutral by construction,
keyed by a resource kind rather than a game's id, so a group of them is a third source with its own
freshness label, not a merge of the two games' rows.

**2. Status crosses as a projection that each game defines.** The common check-status vocabulary is
`ComboTrackerCheckRow`'s `shuffled`, `obtained` and `skipped` (`src/common/combo_tracker_view.h`). The two
adapters have different shapes, and each game defines its projection differently:

- **OoT** registers an accessor vtable (`ComboOoTTrackerOps`) and maps its native status onto the
  projection in its own TU. It projects its one ordered `RandomizerCheckStatus`: `RCSHOW_COLLECTED` and
  `RCSHOW_SAVED` are `obtained`, the heap skip flag is `skipped`, and `RCSHOW_UNCHECKED` and the UI-only
  states (`RCSHOW_SEEN`, `RCSHOW_IDENTIFIED`, `RCSHOW_SCUMMED`) are not `obtained` and have no common value
  of their own.
- **MM** registers an offset descriptor (`ComboMMTrackerDesc`) and runs no code of its own on the read
  path. Its projection is the three one-byte `RandoSaveCheck` flags `shuffled`, `obtained` and `skipped`,
  at offsets the MM TU registers and pins with `static_assert`s
  (`games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp`). The view reads each byte as zero or non-zero and
  interprets nothing else. MM's `eligible` and `cycleObtained` have no registered offset, so they are not
  projected.

The projection is lossy on purpose. It may gain a value when a consumer needs one (the Check Tracker's
colours, #458 U4): the value is defined in common terms, and a game with no analogue never produces it.
OoT derives it in its own TU, behind a new vtable member. MM supplies it through a new descriptor member
that the MM TU fills: an offset to a flag that already means the common value, or a callback, defined in
the MM TU, that derives it. Common code never derives a new value by interpreting MM's bytes on its own.

*As built by #458 U4 (2026-10-01).* The Check Tracker's colours needed a status, and three things landed
that the paragraph above does not describe; this note is the binding text for them.

- **The vocabulary.** `ComboTrackerCheckRow.status` holds a `ComboTrackerCheckStatus`, six values in SoH's
  check-tracker terms, the vocabulary its colour settings are written in: `UNCHECKED`, `SEEN`, `SCUMMED`,
  `SKIPPED`, `COLLECTED`, `SAVED`. So `RCSHOW_SEEN` / `RCSHOW_IDENTIFIED` (as `SEEN`), `RCSHOW_SCUMMED` and
  `RCSHOW_SAVED` now have common values of their own, for this field only; `obtained` stays exactly
  `COLLECTED` or `SAVED`, so the three-flag projection above is unchanged. It is still a projection, not raw
  passthrough (point 3): no common type carries `RandomizerCheckStatus`, and two OoT states share `SEEN`.
- **OoT fills it through the existing `checkAt`**, not a new vtable member: `checkAt` already fills the
  whole row in OoT's TU (`TrackerAdapterSingleExe.cpp`, `ProjectStatus`, with SoH's own precedence), and
  the view zeroes the row before calling it, so a field an adapter leaves alone reads as "none". The same
  member fills the row's area, short name and found item.
- **MM's status is mapped in common code from the flags already projected** (`Combo_TrackerCheckAt`):
  `obtained` is `COLLECTED`, else `skipped` is `SKIPPED`, else `UNCHECKED`. No new MM byte is read or
  interpreted for it; it re-labels two values the descriptor already locates, and MM never produces
  `SEEN`, `SCUMMED` or `SAVED`. MM's area name and found item come through the new descriptor callbacks
  `areaName` and `placedItemName`, defined in the MM TU, as the paragraph above describes.
An item row's `have`, `count` and `max` are the same kind of projection, derived from each game's save
layout inside that game's TU.

**3. Raw passthrough is deferred.** No common type carries a game's raw status enumerator, and common code
interprets no game's status bits beyond the projection above: MM's descriptor locates only the flags that
are the projection, never a raw status field to be decoded. A consumer that needs a raw state reads it in
that game's own TU, as the native trackers do, or proposes a new projected value under point 2. Adding raw
passthrough later is a further amendment to this ADR, not an implementation detail, because every consumer
and every future game would inherit the shape.

**4. Freshness stays a field.** Every summary, and every group of rows, carries a `ComboTrackerFreshness`
(`LIVE`, `STALE`, `UNAVAILABLE`) that the view sets, never the adapter. Which source may be read as live is
ADR 0008 rule 5 and its amendments.

**Locks:** `ComboTrackerView` (the check adapters, their projection and freshness, with MM's rows read at
the offsets MM registers). The item view's lock arrives with the item view (#458 U1).
