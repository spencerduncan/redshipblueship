# ADR 0009: Combo settings author in CVars and identify by digest; the reverse pool is a second origin-keyed table behind a pre-generation gate

- Status: **Accepted** (2026-07-22); **decisions 1 and 2 amended 2026-07-30**
  under the one-game-semantics ruling
- For: #498 (combo-level settings), gating #493 (MM -> OoT foreign items); Phase
  3.1 tracker #492, Lane 1
- Amended: **2026-07-30, #564** (the one-game ruling is recorded on
  [#500](https://github.com/spencerduncan/redshipblueship/issues/500#issuecomment-5126492334))
  — decision 1 (a frozen record on the save side is world identity, not a second
  settings store; the digest becomes a cross-check stamped before the act it
  validates) and decision 2 (identity is the post-condition of the WHOLE
  creation, and the MM option profile freezes ahead of OoT's `Fill()`). Decision
  3, the reverse-pool rules and the `reserved[]` budget table are unchanged;
  claim 3's *rationale* is restated where it leaned on decision 1. Each
  amendment sits at the end of its decision, with the original reasoning kept
  above it so the decision trail stays legible.
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

> **Amended 2026-07-30 (#564) — read the amendment block at the end of this
> section before acting on the reasoning below, which is kept for the trail.**

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

### Amendment to decision 1 — one game, one identity, frozen at creation

> **2026-07-30, operator ruling on #500, alignment plan #564.** *"Freezing at
> creation is the correct semantics for sure. Keep that idea in mind with all
> designs. This is one game from a semantic standpoint."* Decision 1 is amended,
> not withdrawn: the CVar authoring surface survives with a boundary, and the
> digest survives with a different job.
>
> **Survives — CVars author, and there is still no second settings store.** ADR
> 0003's one-store posture is about *authoring*, and nothing here weakens it. It
> gains a boundary: CVars author the one game's settings **only up to the
> creation event**. After creation, no CVar describes a created world, and no
> gameplay path may read one to decide world behaviour.
>
> **Changes — a frozen record on the save side is world identity, not a store.**
> The argument above ("a settings surface carved into the save would be a second
> settings store") holds only while something *authors* there. Nothing does: such
> a record is written once, by the creation event, and is read-only for the life
> of the file — precisely the status the REVERSE placement table
> `foreignPlacementsOoT` already has (claim 1 of the budget below; #534's KEEP
> snapshot exists because "nothing re-places the reverse table after
> generation", `context.cpp`), and nobody calls that table a second item
> database. Take the analogy from the reverse table specifically and not from
> the forward `foreignPlacements`: the forward table is re-authored by MM at its
> own `OnFileCreate` on each arrival, which is the deferred-generation behaviour
> this amendment retires — it is the counter-example, not the precedent.
> Rejecting a profile record as a "second store" rejected the wrong thing.
> *Where* the record
> lives stays a budget question, answered under claim 3 below: MM's own
> `RANDO_SAVE_OPTIONS` in Tier-3 already IS a frozen per-save option record, so
> the one game gets the storage for free and the digest stays a 4-byte
> cross-check rather than becoming the storage.
>
> **Changes — the digest is a cross-check, and it is stamped BEFORE the act it
> validates.** Today `mmProfileDigest` has one writer, zero comparators, and is
> produced *by* the resolution it should be validating (`Foreign.cpp:149`; every
> consumer is display-only). Under one-game semantics it is stamped by the
> creation event before either half generates, and every arrival and every load
> recomputes it from the frozen record and compares. A mismatch is **corruption
> to refuse, never a divergence to honour** — the ruling's second binding
> consequence — so the comparison is semantics, not defensive plumbing.
>
> **Changes — "detects but cannot repair … that is accepted."** Detection stops
> being merely sufficient and becomes mandatory; and repair stops being
> impossible wherever the frozen record exists, because a record can say what the
> original rules were and a digest never could. The un-repairable case shrinks to
> a save whose record is itself gone — which is the refusal path, not a
> degraded-play path.
>
> **Reinterpretation that must be written down loudly — `mmProfileDigest == 0`
> changes meaning.** It reads today as "the MM half has not been generated yet",
> and is rendered to the player as the *normal* state of a file still in OoT
> (`ComboMmOptionsWindow.cpp:80-85`). Once identity freezes at creation, zero
> means **identity not frozen**, a state no created combo file may be in. Any
> reader carried across unchanged will report corruption as normal.
>
> **Coverage — the identity term must span the generator's whole input set.**
> `gRando.ExcludedChecks`, the StartingItems block, spoiler policy, the RESOLVED
> `RO_LOGIC` default (today decided at arrival by probing CVar *existence*,
> `Foreign.cpp:125`), and every trap-type key classified as world identity all
> shape the MM world and all sit outside `MMOptionsString()`. A digest narrower
> than the input set is vacuous — same seed, same digest, different world — so a
> guard built on it would pass while the thing it guards diverges. OoT's own
> settings hash already folds excludes and tricks; that is the scope to copy
> (#564 V4).
>
> **Sequencing.** This amendment lands before #498 carves any tier-4 key. No
> identity-mismatch refusal may ship before #533's quarantine and armed-session
> latch: while a refused file is indistinguishable from an absent one and the
> next save overwrites it, every new detection converts into data loss (#564
> V19).

## Decision 2 — an explicit pre-generation gate; the pairing stamp does NOT move

**Add a pre-generation opt-in read before `Fill()`. Leave the
`sourceIsRando`/`sharedRandoSeed`/`sharedRandoSettingsHash` stamp exactly where
it is, at the end of `Playthrough_Init`.**

> **Amended 2026-07-30 (#564) — restated, not reversed. The amendment block at
> the end of this section widens the post-condition and adds one ordering
> constraint; everything below it still holds.**

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

### Amendment to decision 2 — the post-condition is the whole creation

> **2026-07-30, #564.** The core of this decision is unchanged and still
> load-bearing: the stamp is a post-condition, and hoisting it above `Fill()`
> would publish an identity for a world that was never generated. Three things
> change around it.
>
> **The post-condition widens from OoT's fill to the WHOLE creation.** As
> written, atomicity is scoped to one game's fill: the stamp says "OoT generated
> successfully", and MM's half is authored later, at first arrival. One game
> means one identity from one creation event, so identity is the post-condition
> of *everything* the creation does — both option profiles frozen, both fills,
> both crossing passes, pair validation, one spoiler — published atomically or
> not at all (the ordered form is #564's target creation-event contract, steps
> 1-8). "OoT's fill succeeded" is no longer a sufficient pre-condition for
> publishing a paired identity, because a paired identity now claims things about
> a Termina that must already exist.
>
> **New ordering constraint: the MM option profile freezes BEFORE OoT's
> `Fill()`.** Freezing an *input* early is not the hoist this decision forbids,
> and holding those two acts apart is what lets both rules stand at once — the
> freeze snapshots what the player chose, the stamp publishes what generation
> produced. The constraint is not tidiness: reverse-pool criterion 3
> (`ForeignItemsSingleExe.cpp:139-157`) excludes the enemy/boss souls, the
> ocarina buttons, `RI_ABILITY_SWIM` and the clock items solely because OoT's
> placement pass "CANNOT read MM's option profile" — true only while the profile
> resolves after that pass runs. With one frozen surface read by both directions,
> criterion 3 becomes a profile-conditional predicate ("admissible when the
> frozen profile arms its give") rather than a timing accident (#564 V24). It is
> narrowed, not deleted: the promise it protects — never advertise a crossing in
> OoT that Termina silently never delivers — is unaffected by the ruling.
>
> **The self-correcting consequence narrows to one attempt.** "A reverse pass can
> run and place items into a world whose fill then fails … harmless and
> self-correcting" holds *within* a creation attempt and no longer beyond it,
> because there is no later generation to pick up the pieces: arrival becomes
> hydrate-or-refuse, with no generation path reachable from it. A creation that
> cannot produce both halves must therefore fail the creation, visibly, at the
> file select where the player made the decision — never revert to a vanilla
> Termina under an active pairing identity (`OnFileCreate.cpp:325`), which is a
> divergence honoured forever (#564 V7).
>
> **A third tense joins the two predicates.** `Requested` (future) and `Active`
> (past) do not cover the new question the option surfaces have to ask: *is this
> world's identity already frozen?* (present). #564 V5 prescribes the
> creation-stamped `mmProfileDigest != 0` as that fact, read from `src/common`
> and never from a `gSaveContext` (ADR 0008 rule 5, and the pane's
> game-agnosticism tripwire). The shape, names to be settled by the implementing
> lane:
>
> ```
> Combo_ForeignPairingRequested()  -- pre-condition, read before Fill()
> <frozen predicate>               -- identity stamped; gates every authoring surface
> Combo_ForeignPairingActive()     -- post-condition, read at give time
> ```
>
> Reviewers must not collapse the three. They are three tenses of one noun, and
> the same "simplify these into one" instinct that decision 2 was written against
> applies unchanged to the third.

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
slots behind it, silently dropping every resource past the eighth. Seventeen of
the twenty are occupied (seven from v1 plus magic, nine from ammo, one for the
shared hookshot); the block was sized for the whole class in one carve, because
a third block would cost another literal offset, another span in every
accessor, and another amendment here. The hookshot proved that out immediately:
it landed in a spare slot with no format change at all.

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

**Claim 3's rationale restated (2026-07-30, #564).** The 4-byte carve is
unchanged and the table still holds; the *reason* a wider Tier-1 record was
rejected is not the one given above, which decision 1's amendment retires. Two
reasons replace it, and both are arithmetic rather than posture:

- **It does not fit.** After claims 1, 3, 5 and 6, `reserved[]` is 132 bytes;
  claim 2 (4 B) and claim 4 (64 B) land it exactly on the 64-byte floor. A
  47-option Tier-1 record fits in none of that, and could not be `u8` anyway —
  `RO_CLOCK_TERMINAL_TIME` ranges to 359 (`OptionsUiSingleExe.cpp:339-342`).
- **It is already stored, one tier down.** MM's `RANDO_SAVE_OPTIONS` lives in
  the MM SaveContext the `.redsave` carries as Tier-3, is written once per file,
  and is what every gameplay path already reads. The frozen record decision 1's
  amendment calls for therefore costs zero new bytes; the digest stays a
  cross-check over it, which is exactly the job the amendment gives it.

Nothing here re-opens the floor or the table. It records that "a save-side record
would be a second settings store" must not be re-used as an argument against
freezing identity — it is the sentence #564 struck.

Two constraints on anyone spending from this:

- **Keep `reserved[]` at 64 bytes or more.** `Test_SaveComboRecordFixed`'s
  scribble loop (`test_save_roundtrip.c:512-538`) iterates `sizeof(reserved)`; at
  zero it degenerates to zero iterations and passes vacuously, retiring the only
  test that proves headroom round-trips at all.
- ~~**Claim 3's size is a placeholder.** Lane 4 must confirm its digest size
  before carving, not after. 4 B assumes a u32 digest by analogy with
  `sharedRandoSettingsHash`; a wider profile record changes the table and this
  ADR should be amended rather than the floor quietly eaten.~~ **Paid** — see
  "Claim 3, settled" above (4 B, carved at offset 788) and, for why a wider
  Tier-1 record is not the answer even under the freeze, "Claim 3's rationale
  restated". Struck 2026-07-30 (#564) because the amendment two paragraphs
  above re-opens the wider-record question and this bullet reads as though it
  were still unanswered.

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

Added by the 2026-07-30 amendments (#564):

- **The identity terms this ADR names are creation-time state, so
  `Context_InvalidateSessionState`'s KEEP policy must carry every one of them.**
  A term stamped at or before file-create that KEEP drops is wiped by the
  creation itself, which is a hole under any freeze design, not a preference
  (#564 V9; the policy's contract lives at `src/common/context.h`).
- **`mmProfileDigest` acquires comparators.** Every arrival and every load
  recomputes and compares it; the display-only consumers stay, but a
  display-only digest is no longer the whole of its use, and "digest 0 = not
  generated yet" must be re-read as "identity not frozen" wherever it is
  rendered.
- **A creation-failure surface is owed on the OoT side.** Decision 2's amendment
  makes "publish or fail" whole-creation, which leaves the silent
  vanilla-Termina revert with nowhere to live; the refusal surface is shared
  with #533's refused-load surface rather than invented per producer (#564's
  one-authority persistence model, REFUSAL).
