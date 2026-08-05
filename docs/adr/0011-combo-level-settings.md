# ADR 0011: Combo settings are a frozen 12-byte record plus the claim-2 digest; direction stays two tables and becomes a gate; item classes become a rule

- Status: **Proposed**
- For: **#498** (combo-level settings: direction, pool size, item classes). Feeds
  **#493** Lane C2 and **#495** (rule-defined item class); Phase 3.1 tracker #492,
  Lane 1.
- Depends on:
  - **[ADR 0002](0002-origin-tagged-shared-items.md)** (Accepted) — the origin-tag
    invariant, the one-TU rule for `RG_*`/`RI_*`, and the `ComboContext` growth
    contract every carve below obeys.
  - **[ADR 0003](0003-settings-namespace.md)** (Accepted) — one CVar store; the
    combo keys here are new tier-4 keys (ADR 0004 §3), not converged MM keys.
  - **[ADR 0004](0004-menu-information-architecture.md)** (Accepted) — §3 tier 4,
    and **§6 state 4** (frozen-at-creation: read-only, reason-labelled, value shown
    *from the save*, enforcement on the writers and not the widget).
  - **[ADR 0005](0005-sourced-grant-cursors.md)** (Accepted) — the DO-NOT-BUMP
    prescription and the literal-offset assert discipline.
  - **[ADR 0007](0007-grant-relay-netplay.md)** (Accepted) — **§2's codec
    discipline**: multi-byte integers little-endian, written a byte at a time
    rather than by casting a packed struct. Decision 1.4 adopts it verbatim for
    `canonical(comboSettings)`.
  - **[ADR 0009](0009-combo-settings-and-reverse-pool.md)** (Accepted; decisions
    1/2 amended 2026-07-30 #564; byte budget amended 2026-08-04 #584; **decision 4
    added 2026-08-04 and decision 4a 2026-08-05, #589/#531**) — the posture this
    ADR implements: CVars author up to creation, a frozen record is identity and
    not a second store, the three-tense predicates, two origin-keyed pools,
    `(originGame, name)` as the key, and **claim 2 (`comboSettingsHash`, 4 B) as
    the only outstanding claim against `reserved[124]`**. Decision 4/4a
    (whole-file commit; newest whole commit wins) is **orthogonal to this ADR**:
    Tier-1 is committed as one unit regardless of which half's save-and-quit
    triggered the commit, so `comboSettings` and `comboSettingsHash` need no
    special handling under it — they ride the same `ComboContext` write every
    other Tier-1 field rides, and they are frozen-at-creation terms that no
    commit ever mutates.
  - **[ADR 0010](0010-cross-game-logic-and-beatability.md)** (Accepted) — the GOAL
    setting and the logic rung are declared combo-level tier-4 terms that "fold
    into `comboSettingsHash` … and into the frozen record #570 stamps. **No new
    carve**". Decision 1 below is where that promise is either honoured or
    corrected; see the stale-premise note.
- Amends:
  - **ADR 0009's byte-budget table** — adds claim 10, the settings record.
  - **ADR 0009 decision 3's fourth rejection ground** — the version-bump cost,
    re-weighed against the landed #533/#568 refusal surface. Decisions 1-3 of ADR
    0009 are otherwise reaffirmed, one of them on new grounds.
  - **ADR 0010 decision 1's growth-contract clause** — "stored value 0 means
    **unset** (a pre-3.2 legacy record)", written on GOAL's own zero value on the
    assumption that GOAL would be a standalone carved field with its own zero-tag.
    **Superseded in mechanism, preserved in effect**: unset-ness is now a property
    of `ComboSettingsRecord.formatVersion`, not of `goal`'s own value, and
    `goal == 0` inside a formatted record is corruption rather than a legacy state
    (decision 1.3). The clause's *effect* — a legacy record makes no beatability
    claim and is never silently promoted to `beat-both` — holds unchanged, carried
    by `formatVersion == 0`. Nothing else in ADR 0010 decision 1 is touched.
- Verification: every anchor below **re-read at `origin/main` = `91bc133b`** on
  2026-08-05. #498's KNOWN section, #493's KNOWN section and #495's Known section
  are each stale in load-bearing ways; the corrections are recorded in Context
  rather than silently absorbed, because all three issues are still open and will
  be read again.

---

## Context

### What #498 says is missing, and what is actually missing

#498's body was written before four merges. Re-derived at source:

| #498's claim | At `91bc133b` |
|---|---|
| "The pool is four literal entries … `ForeignItemsSingleExe.cpp:58-67`" | **True, moved.** `kForeignPoolV1[]` is four rows at `games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp:107-115`; `kForeignPoolCount` is `sizeof`-derived at `:117` and `static_assert`ed `<= RSBS_FOREIGN_PLACEMENT_CAP` at `:118`. The pool's *membership reasoning* is now written out (criterion 6, `:71-96`) but is enforced by hand. |
| "Direction is structural, not a flag … cannot gain a `hostGame` byte in place" | **The premise is true; the conclusion is spent.** The record is still 6 bytes with offsets asserted as format (`src/common/context.h:335-339`, not `:233-242` — that range is now `RSBS_GRANT_SOURCE_CAP`'s comment). But the **second table already landed**: `ComboForeignPlacement foreignPlacementsOoT[8]` at `.redsave` offset **740** (`context.h:560`, pinned at `:782`), with four mirror accessors sharing one body (`src/common/foreign_items.c:250-284`) and one comment that states the rule: "**the DIRECTION IS THE ACCESSOR**" (`foreign_items.c:151-171`). ADR 0009 claim 1 is carved. |
| "The gate … `Combo_ForeignPairingActive()` is `sourceIsRando && sharedRandoSettingsHash != 0` (`foreign_items.c:18-20`)" | **True, verbatim, unchanged.** |
| "The world identity carries OoT's settings only" | **False now.** `mmProfileDigest` (`context.h:601`, offset 788) is stamped by the creation event at `3drando/playthrough.cpp`, folds `gRando.ExcludedChecks` and `GetStartingItemsFromConfig()` (`Rando/Foreign.cpp:183-230`), is in the invalidation KEEP set (`context.cpp:343`/`:399`), and is compared-and-refused at MM arrival. |
| "The name→item inverse … walks exactly one pool and keys on display name alone" | **False now.** `src/common/foreign_items.c:31-149` is an origin-indexed pool registry; `Combo_GetForeignItemByNameFor(origin, name, …)` is the inverse, and `"Lens of Truth"` is a *deliberate* cross-pool collision asserted in `test_foreign_items.c`. ADR 0009 decision 3 is implemented. |
| Work step 2, "fix the stale growth-contract comment at `context.h:161-167`" | **Done** — `RSBS_FOREIGN_PLACEMENT_CAP` now carries the full DO-NOT-BUMP treatment plus the second-block prescription (`context.h:187-215`). |
| Work step 5, "give the pool/name surface an origin dimension" | **Done** (the registry above). |
| Needs-investigation, "whether the MM award callback exists at all — it does not" | **False now.** `MM_AwardSharedItem` (`GameExports_SingleExe.cpp:2275`) calls the real `MM_ForeignItem_Give`; MM's give is deferred to the first live-`PlayState` frame rather than allowlisted (`games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp:17-82`). `kForeignPoolMMV1` is ~116 rows at `:235`. |

**What is genuinely missing is exactly one thing, and it is the thing this ADR is for:**
`comboSettingsHash` has **zero source hits** — it exists only in ADR prose. No
`gCombo.Rando.*` key exists (the only `gCombo.` keys in the tree are three
`gCombo.Windows.*` visibility toggles). There is no direction field, no pool-size
field, no item-class selector, in any store. Every cross-game parameter is still a
compile-time constant or an unconditional consequence of `Combo_ForeignPairingActive()`.

So the reverse direction **works** and cannot be **turned off, sized, or scoped** —
and the paired identity that refuses divergence covers OoT's settings and MM's
profile but says nothing about the rules that govern the crossing itself.

### The storage argument ADR 0009 settled for MM does not transfer

ADR 0009 decision 1's amendment resolved "digest vs record" for the MM half by
finding that the record already existed: "**It is already stored, one tier down** —
MM's `RANDO_SAVE_OPTIONS` lives in the MM SaveContext the `.redsave` carries as
Tier-3 … The frozen record decision 1's amendment calls for therefore costs zero
new bytes; the digest stays a cross-check over it."

**There is no such tier for combo-level terms.** Direction, pool size, item class,
GOAL and rung belong to neither game's save by construction — that is what
"combo-level" means. ADR 0010 D1 nonetheless promised these fold "into the frozen
record #570 stamps … **No new carve**"; #570's record is `RANDO_SAVE_OPTIONS`, MM's
own options, which cannot hold a term about the pair. That promise is a
stale premise inside an Accepted ADR, and decision 1 below is where it gets paid
rather than inherited.

### The version-bump cost has dropped since ADR 0009 decision 3 priced it

ADR 0009 decision 3 rejected reshaping `ComboForeignPlacement` to
`{hostGame, checkId, item}` on four grounds, the fourth being "`RSBS_SAVE_VERSION`
past 2 with a stated forward-incompatibility — a 2048-byte Tier-1 fails
`h.comboSize > kComboSize` on every already-shipped binary". Re-measured:

- `RSBS_SAVE_VERSION` is **2**, `RSBS_SAVE_VERSION_MIN` is **1** (`src/common/save.h:48`,
  `:54`), and the load test is a **window, not an equality** (`save.cpp:379-385`):
  "Bumping `RSBS_SAVE_VERSION` against an equality test is precisely how a format
  change orphans every existing save; older versions inside the window are
  prefix-compatible and load."
- Backward: a v2 record read by a v3 build zero-extends through the Tier-1 prefix
  load — the growth contract, already exercised by six landed carves.
- Forward: a v3 record read by a v2 build takes `RSBS_REFUSE_VERSION`
  (`save.cpp:384`), and since **#568** that is a first-class `RSBS_SLOT_REFUSED`
  state with quarantine, an armed-session write latch (`save.cpp:234`, `:604-611`)
  and its own file-select rendering (`ComboMenuBar.cpp:358-440`). An old binary
  meeting a new save now refuses loudly and destroys nothing; when ADR 0009 was
  written it would have converted into data loss (#564 V19).

The bump is therefore **near-free where it is genuinely needed**. That is a reason
to stop treating it as an unpayable cost — not a reason to spend it here. Nothing
in this ADR needs it (decision 1's carve is a front-of-`reserved[]` carve at an
unchanged record size), and decision 2 explains why the cheaper price does *not*
re-open the table reshape.

---

## Decision 1 — Combo settings are a 12-byte frozen record plus the 4-byte claim-2 digest, carved once

**CVars author, up to the creation event and no further (ADR 0009 decision 1,
unchanged). The `.redsave` gains two contiguous fields carved from the front of
`reserved[124]`: `uint32_t comboSettingsHash` at offset 880 — claim 2, spent
exactly as reserved — and `ComboSettingsRecord comboSettings` (12 B) at offset
884. `reserved[]` becomes `reserved[108]`. No version bump, no record-size raise.**

### 1.1 Why a record and not the digest alone

ADR 0009 decision 1's amendment already made the general ruling: "a frozen record
on the save side is world identity, not a store", and "repair stops being
impossible wherever the frozen record exists, because a record can say what the
original rules were and a digest never could." Two shipped constraints turn that
from a preference into a requirement for these particular terms:

1. **ADR 0004 §6 state 4 is unimplementable over a digest.** "Where a frozen key's
   value is shown at all, show the value from the save rather than the CVar — after
   creation the two may legitimately differ, and the save is the one the world was
   built from." A hash cannot be shown. The MM pane satisfies this by reading
   `Combo_MMProfileSummary` over `gComboCtx` (`combo_mm_options_view.c:181-197`); a
   combo pane has nothing equivalent to read unless the values are stored.
2. **The refusal path needs a reason string.** #533/#568 refusal is
   loud-and-explainable by design. "Your combo rules do not match this save" with no
   ability to name *which* rule is the un-repairable case ADR 0009 accepted only
   because it had no alternative — and it has one here, for twelve bytes.
   **This capability is only real if something builds it**, so increment 1 carries
   an explicit field-diff task (`Combo_ComboSettingsDivergence`) that feeds the
   refusal notification text, and a test lock that asserts the refusal names the
   diverged field rather than merely refusing. Without that task the twelve bytes
   buy display alone and justification 2 would be a promise the ADR does not keep.

### 1.2 The shape

```c
typedef struct {
    uint8_t  formatVersion; // 0 = record ABSENT (legacy / never frozen). Nonzero = every field below is authoritative.
    uint8_t  direction;     // RSBS_COMBO_DIR_* ; OFF is a NONZERO enumerator (see 1.3)
    uint8_t  poolSizeOoT;   // max OoT-origin placements into MM checks, 1..RSBS_FOREIGN_PLACEMENT_CAP
    uint8_t  poolSizeMM;    // max MM-origin placements into OoT checks, 1..RSBS_FOREIGN_PLACEMENT_CAP
    uint16_t itemClassOoT;  // RSBS_ITEMCLASS_* bitset over the OoT pool (decision 3)
    uint16_t itemClassMM;   // RSBS_ITEMCLASS_* bitset over the MM pool (decision 3)
    uint8_t  goal;          // RSBS_COMBO_GOAL_* (ADR 0010 D1); illegal to be 0 inside a formatted record
    uint8_t  logicRung;     // RSBS_COMBO_RUNG_* (ADR 0010 D5); likewise
    uint8_t  spare0;        // growth under formatVersion, not under zero-means-unset
    uint8_t  spare1;
} ComboSettingsRecord;      // sizeof == 12, alignment 2, no padding
```

- **`formatVersion` is the occupancy tag, and it is what makes the other ten bytes
  usable.** This is the `ComboSharedResource` device (`context.h:371-396`: "0 rupees
  is a legal player state, so a bare `uint16_t sharedRupees` cannot distinguish a
  pre-#525 record from a broke player"), applied to a struct instead of a slot. The
  growth contract's "zero means unset" is satisfied **at the block**, which frees
  every field inside a formatted record to use zero legitimately.
- **`formatVersion` is a version, not a bool, precisely so `spare0`/`spare1` are
  safe.** A field added later is authoritative only at `formatVersion >= N`. This is
  ADR 0002 §4's rule for `COMBO_CONTEXT_VERSION` — "bump it when old content must be
  *distinguished*, not merely extended" — scoped to one block.
- **Layout is `.redsave` format.** `sizeof == 12` plus member offsets, plus the two
  literal block offsets **880** and **884**, all `RSBS_CTX_STATIC_ASSERT`ed, in the
  same style and for the same reason as 672/736/740/788/792/824/872/876
  (`context.h:750-862`): an assert phrased in terms of a capacity constant follows a
  bump instead of catching it, which is the silent break #490 describes.

### 1.2.1 Every value space here is PINNED and APPEND-ONLY

This is the load-bearing half of the shape, and it is stated in the ADR rather than
left to the implementer because the default C idiom (bare sequential `enum`) gets it
wrong silently.

> **Rule.** Every enumerator and every bit position below is assigned an explicit
> numeric literal and is **append-only**. To remove a value, **retire it in place** —
> keep the literal, mark it dead, never renumber. This is exactly the
> `RSBS_SHARED_RES_*` discipline (`context.h:388-415`, explicit `= 0`, `= 1`, `= 2`,
> …) and exactly the reason MM's own `Options.cpp:38` retires a dead row rather than
> deleting it: "deleting it renumbers the 44 enumerators after it and
> `RANDO_SAVE_OPTIONS` is indexed by that number in every already-written MM rando
> save."

Two things break if this is not held. First, a **local** save corruption with no
netplay involved: a contributor inserting a value mid-list silently reassigns the
meaning of `direction`/`goal`/`logicRung`/`itemClass*` in every already-written
`formatVersion >= 1` record, and a newer binary reads an old world's rules as
different rules. Second, `comboSettingsHash` stops being usable as a cross-peer
comparison term the moment #574's identity handshake exchanges it (#574 item 4 names
`comboSettingsHash` directly): two peers on builds either side of a renumbering
compute different hashes for identical settings — or, if two renumberings cancel,
identical hashes for *different* settings, which is the failure mode a digest exists
to prevent.

```c
// Direction. Pinned, append-only, retire-never-renumber.
// 0 is unreachable inside a formatted record (see 1.3).
#define RSBS_COMBO_DIR_OFF     1u // paired world, zero crossings (a real, chooseable world)
#define RSBS_COMBO_DIR_FORWARD 2u // OoT-origin items into MM checks only
#define RSBS_COMBO_DIR_REVERSE 3u // MM-origin items into OoT checks only
#define RSBS_COMBO_DIR_BOTH    4u // today's shipped behaviour; the O2 default

// GOAL. Values track ADR 0010 D1's table; ADR 0010 owns the value LIST,
// this ADR owns their ENCODING. Pinned, append-only.
#define RSBS_COMBO_GOAL_BEAT_BOTH     1u
#define RSBS_COMBO_GOAL_BEAT_EITHER   2u
#define RSBS_COMBO_GOAL_TRIFORCE_HUNT 3u

// Logic rung. Values track ADR 0010 §2.2's ladder; same ownership split.
// The trick set T is a PARAMETER of the rung and is NOT encoded here — it is
// each half's own authored settings (ADR 0010 §1.2), reached through the
// half-digests this record's hash folds (1.4).
#define RSBS_COMBO_RUNG_NONE          1u // base: no proof; the spoiler carries the burden
#define RSBS_COMBO_RUNG_BEATABLE      2u // GOAL provable under the frozen trick set
#define RSBS_COMBO_RUNG_ALL_REACHABLE 3u // GOAL provable and every location reachable

// Item classes (decision 3). Pinned BIT POSITIONS, append-only: allocate the
// next free bit, never re-point an allocated one. Bits 0x0100..0x8000 are
// unallocated and MUST read as 0 in a formatVersion == 1 record.
#define RSBS_ITEMCLASS_PROGRESSION    0x0001u // progressive/major items
#define RSBS_ITEMCLASS_SONGS          0x0002u
#define RSBS_ITEMCLASS_MASKS          0x0004u
#define RSBS_ITEMCLASS_DUNGEON_ITEMS  0x0008u // small/boss keys, maps, compasses
#define RSBS_ITEMCLASS_DUNGEON_REWARD 0x0010u // medallions, stones, remains
#define RSBS_ITEMCLASS_SIDEQUEST      0x0020u // non-progression sidequest rewards
```

A class bit is only ever a **filter over candidates that already passed the six
criteria** of decision 3.1. No bit can widen the pool past them — in particular no
bit can readmit a criterion-6 shared resource (#525 wallet/heart/magic/ammo/hookshot),
because the criteria run first and the bitset selects among survivors. Increment 3
may append bits; it may not weaken that ordering.

### 1.3 Zero traps, stated because both are silent

- **`direction == 0` must not mean "no crossings".** A legacy record zero-extends to
  all zeros; if 0 meant OFF, every pre-3.1 paired save would silently lose its
  crossings on the first load by a new build. `RSBS_COMBO_DIR_OFF` is `1u`; 0 is
  reachable only when `formatVersion == 0`, where the whole block is absent.
- **`goal == 0` inside a formatted record is illegal, not "unset".** ADR 0010 D1
  says "stored value 0 means **unset** … A legacy record makes no beatability claim".
  That effect is satisfied here by `formatVersion == 0`, which is strictly more
  precise: a created file always carries a resolved GOAL, and a zero in a formatted
  record is corruption, not a legacy state. **This relocates where ADR 0010 D1's
  growth contract lives** — see the Amends bullet, which records the supersession
  explicitly so the two Accepted documents do not assert incompatible
  zero-semantics for the same field without a cross-reference. `logicRung == 0`
  reads identically.

### 1.4 How claim 2 is spent

`comboSettingsHash` is **the whole pair's fingerprint**, not a checksum of the
twelve bytes beside it:

```
comboSettingsHash = Hash( canonical(comboSettings) ‖ ":" ‖ sharedRandoSettingsHash ‖ ":" ‖ mmProfileDigest )
```

- **`canonical(comboSettings)` is defined, not left to the compiler.** It is the
  twelve bytes of the record encoded **field-by-field in declaration order**, each
  `uint16_t` little-endian, **written a byte at a time — never a struct memcpy and
  never a cast of a packed struct**. This is ADR 0007 §2's codec discipline
  ("All multi-byte integers are little-endian, and the codec reads and writes them a
  byte at a time rather than casting a packed struct, so a big-endian host emits
  identical bytes"), adopted here for the same reason it was adopted there: the
  digest must be a property of the *values*, not of the host's layout or endianness.
  The `sizeof == 12` / member-offset asserts pin the **storage** format; this
  encoding pins the **digest input**; on every supported host they are the same
  twelve bytes in the same order, and the byte-at-a-time codec makes that true by
  construction rather than by luck.
  - This also keeps `comboSettingsHash` in family with the two terms it folds,
    which are both encoded-first rather than struct-hashed:
    `sharedRandoSettingsHash` hashes a `std::string` built from `GetOptionText()`
    per option (`3drando/playthrough.cpp:77`), and `mmProfileDigest` hashes
    `ProfileIdentityString()` over `MMOptionsString()`, whose comment states the
    load-bearing property outright — "std::map iteration gives a stable option
    order" (`Foreign.cpp:94-99`). A raw-struct hash beside two string-first digests
    would be the one term in the fingerprint whose bytes depend on the toolchain.
  - Pinned by a **golden-vector test** at increment 1, the way `test_netplay_relay.c`
    pins the wire format: a fixed `ComboSettingsRecord` maps to a fixed byte string
    and a fixed digest, so a layout or endianness change is a red test rather than a
    silently different world identity.
- Folding both half-digests is what makes the term non-vacuous. ADR 0009 decision
  1's amendment states the rule: "A digest narrower than the input set is vacuous —
  same seed, same digest, different world — so a guard built on it would pass while
  the thing it guards diverges."
- **Order is a constraint, not a convention.** `comboSettingsHash` is computed last,
  after both half-digests are stamped and after the record is frozen. Anything else
  hashes a term that has not been decided yet.
- **Zero displaces**, exactly as `DigestFromIdentity` does (`Foreign.cpp:242-249`):
  a real identity hashing to 0 would read as "not frozen" and become an undetectable
  mismatch. One collision in 2^32 against a certain false negative.
- **`MixPairedFinalSeed()` MUST NOT be widened with it.** Folding identity terms into
  the *seed derivation* re-derives `finalSeed` for every already-generated paired
  world; `SeedDeterminism` and `MMPairedAttemptDeterminism` pin exactly that. The
  digest answers "were these worlds generated under the same rules"; the seed
  answers "which world does this seed derive". Two questions, two computations —
  the same separation `ResolvePairedProfile` already documents at `Foreign.cpp:263-267`.

### 1.5 What a version bump would cover, so nobody re-derives it hopefully

Recorded because §"the version-bump cost has dropped" will otherwise be read as
permission:

- **Not needed for this carve.** A front-of-`reserved[]` carve leaves
  `RSBS_COMBO_CONTEXT_RECORD_SIZE` at 1024 and every prior offset fixed. Two builds
  either side of it interoperate: the older reads the block as all-zero, which is
  correctly "not frozen".
- **Needed only when the Tier-1 record size itself must rise** — ADR 0010 increment
  3's boundary carve is the realistic claimant, and a placement-cap raise is the
  other. When that day comes: raise `RSBS_COMBO_CONTEXT_RECORD_SIZE` and
  `RSBS_SAVE_VERSION` together, keep `RSBS_SAVE_VERSION_MIN` at 1, and accept that
  older binaries refuse the newer file through `RSBS_REFUSE_VERSION` into the #533
  surface. That refusal is now quarantine-and-latch, not loss.
- **A bump is not a migration.** `save.cpp`'s Tier-1 parse is still a flat `memcpy`
  with no hook. Every carve stays append-only regardless of version.

## Decision 2 — Direction stays two tables; the setting is a generation-time gate, not a table shape

**Reaffirm ADR 0009 decision 3's two-table shape. `ComboForeignPlacement` is not
reshaped, now or later. The direction setting gates *which placement passes run*,
reading the frozen record — never a CVar, and never `Combo_ForeignPairingActive()`.**

### 2.1 Both options, re-weighed with what shipped

| | Two tables (shipped) | Record v2 `{hostGame, checkId, item}` |
|---|---|---|
| Bytes | 48 B already carved at offset 740 | 8 B × 16 = 128 B; the 96 shipped bytes at 740-787 become dead-in-place |
| Format cost | none | a format generation + `RSBS_SAVE_VERSION` 3 + a migration of **live** data |
| Migration hook | n/a | still does not exist (`save.cpp` Tier-1 is one flat `memcpy`) |
| ADR 0002 | amendment not needed | amendment needed |
| Accessors | 4 mirrors, **already fused to one body** (`foreign_items.c:178-248`) | one family |
| Determinism | `foreignPlacementsOoT` is already folded into `Rando_HeadlessSeedDeterminismDigest` (`3drando/menu.cpp`, the #510 fold) | re-pin |
| Aliasing safety | **structural** — see 2.2 | a discipline a scan can forget |

The version-bump ground weakened; the other three held; and a fifth arrived that did
not exist when ADR 0009 priced this: **the second table is shipped**. Reshaping now
migrates real `.redsave` bytes and two shipped accessor families, to buy an
elegance the accessor split already delivers.

### 2.2 The decisive argument is ADR 0002's own, and it now runs the other way

ADR 0002's mechanism is "make the wrong thing unrepresentable at compile time." A
`hostGame` byte inside one array is a *runtime* discriminator: an OoT
`RandomizerCheck` and an MM `RandoCheckId` are unrelated enumerations that "collide
freely as raw u16s" (`context.h:555-559`), and a scan that forgets to compare
`hostGame` false-positives across directions and silently writes a wrong-origin row —
the #356 class, in the table ADR 0002 exists to protect.

Two tables make forgetting impossible: **there is no accessor that takes both**, so
the direction is chosen at the call site by which function you name. That is a
stronger enforcement than the tagged struct, in the same spirit. The reshape is
rejected on ADR 0002 grounds, not merely on cost — and that ground does not expire
when bytes get cheaper.

### 2.3 What the direction setting actually gates

Today both passes run unconditionally under an active pairing:

- forward, `Rando::Foreign::PlaceForeignItems` (`games/mm/2s2h/Rando/Foreign.cpp:482`),
  from MM's `OnFileCreate`;
- reverse, `OoT_PlaceForeignItems` (`ForeignItemsSingleExe.cpp:300`), from
  `Playthrough_Init` immediately after the identity stamp, with delivery wired at
  `hook_handlers.cpp:383` on the flag-driven RC-queue path.

The setting is read **from the frozen record**, by both passes, and each pass
becomes a no-op (returning 0, the existing "normal solo" code) when its direction is
not armed. Three consequences:

- **`RSBS_COMBO_DIR_OFF` is a real value**, and its world is a paired world with no
  crossings — not an unpaired world. `Combo_ForeignPairingActive()` keeps its
  meaning untouched.
- **The pre-condition predicate ADR 0009 decision 2 designed is still owed.**
  `Combo_ForeignPairingRequested()` has zero source hits. It is what the *freeze
  step* reads (from the tier-4 CVars, before `Fill()`); after the freeze, nothing
  reads a CVar. This ADR does not redesign it; it names it as decision 4's input.
- **The three tenses stay three.** ADR 0009 decision 2's amendment warns reviewers
  not to collapse them. This ADR adds the present-tense predicate for *these* keys:
  `Combo_ComboSettingsFrozen()` ≡ `gComboCtx.comboSettings.formatVersion != 0`, the
  exact twin of `Combo_MMProfileFrozen()` (`combo_mm_options_view.c:174`).

### 2.4 What this hands #493 Lane C2

- **"Second placement carve"** — done (offset 740). Lane C2 inherits it.
- **"OoT sentinel item id"** (`RG_RSBS_FOREIGN_SLOT_*`) — **superseded, not owed.**
  Zero source hits, and the reverse leg delivers without one because the RC-queue
  path knows the check id at give time. #493 step 8's sentinel design was written
  against the actor path; do not resurrect it, and do not append enumerators to
  `RandomizerGet` for this — that renumbering hazard buys nothing now.
- **"Pre-Fill pairing gate"** — genuinely owed, and it is decision 4's freeze step.

## Decision 3 — The item class is a rule with no seed term; the class *bitset* is the setting

**Both pinned tables are replaced by machine-evaluated membership predicates, each
living in its own pool TU (ADR 0002's one-TU rule, unchanged). The criteria are
named and numbered in one game-header-free place. The derived pool is a pure
function of (that game's static item table, the frozen `itemClass*` bitset) — with
**no seed term**. `ComboForeignItemDef`, the registry, and every consumer signature
are unchanged.**

### 3.1 The rule already exists; it is enforced by hand

`games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp:128-234` states six criteria and
then hand-transcribes ~116 rows that satisfy them. `kForeignPoolV1`'s comment says
"CRITERION 6 APPLIES HERE TOO" (`ForeignItemsSingleExe.cpp:71`) and then
hand-transcribes four. The criteria are:

1. real item, not a sentinel (`RI_UNKNOWN`/`RI_NONE`/`RI_JUNK` out);
2. not junk-class (what a host degrades to when the table is absent);
3. give is **unconditionally effectful** — not armed by an option;
4. give fires no global world event (`RI_TRIFORCE_PIECE`'s completion cascade);
5. reward, not punishment (`RI_TRAP`);
6. not a shared cross-game resource (#525 — wallet/heart/magic/ammo/hookshot).

Promoting these to code is the whole of #495's substance. It is not a rewrite: the
give side already resolves any id generically on both halves.

### 3.2 The class carries no seed — and #495's central lock must be re-aimed

#495 step 2 asks for "a LOCAL xorshift seeded from `(sharedRandoSeed,
sharedRandoSettingsHash, ":foreign-class-v2")`" selecting *the pool*, and its lock
asserts "a different `sharedRandoSeed` must produce a different pool — otherwise the
'rule' is a constant wearing a rule's clothes." That prescription was correct when
the OoT pool (4) was smaller than the cap (8), so pool ≡ what gets placed. **The MM
side has since proved the other shape**, and it is better:

- The pool is 116 rows against a cap of 8, and **both** passes already draw a
  per-seed subset — the reverse pass draws pool entry *and* host without replacement
  from an identity-seeded stream (`ForeignItemsSingleExe.cpp:353-381`), the forward
  pass draws hosts likewise (`Foreign.cpp:560-570`).
- Seed variety therefore **already exists, at placement time**. A seed term in the
  class would be a second randomisation of the same axis.
- And it would break the name inverse. `Combo_GetForeignItemByNameFor` is the
  spoiler-**LOAD** path (`foreign_items.c:125-145`), and it runs in processes that
  never generated. A seed-varying pool makes the inverse partial: a name that was in
  the pool at generation is absent at load, and the loader either refuses a valid
  spoiler or the pool must be re-derived with the right seed first. Both were listed
  as viable in #495; neither is, once the placement draw already supplies variety.

> **Correction to #495's primary lock**, recorded here in the same spirit as ADR
> 0009 decision 3's correction to #493's: the assertion "a different
> `sharedRandoSeed` must produce a different pool" must **not** be written — it
> would be satisfiable only by a design that makes the spoiler-load inverse partial.
> Re-aim it at the observable that matters: **a different seed must produce
> different placements** (the `SeedDeterminism` / `MMRandoGen` folds already carry
> this), and **a different `itemClass*` bitset must produce a different derived
> pool**. A lock that cannot pass gets "fixed" by weakening it, which is why the
> correction lives in an ADR and not a review comment.

### 3.3 The class is a bitset, and it is the setting

`itemClassOoT` / `itemClassMM` are `RSBS_ITEMCLASS_*` bitsets (decision 1.2.1's
pinned bit table: progression, songs, masks, dungeon items, dungeon rewards,
sidequest), each bit's membership decided by the same six criteria plus that bit's
own predicate. Bitset rather than an ordinal "breadth" rung because it composes,
extends without renumbering, and reads correctly under the block tag: inside a
formatted record, `itemClass* == 0` is a legitimate "no classes armed for this
direction", which is the same world as that direction being off — and is therefore
not a hidden second OFF, because the direction byte says so first.

Bit positions are pinned and append-only per 1.2.1; unallocated bits must read as 0
in a `formatVersion == 1` record, so a future build can distinguish "class not armed"
from "class did not exist yet" by the record's own version.

### 3.4 Where the criteria live, given the one-TU rule

The predicates must name `RG_*`/`RI_*`, so they cannot leave their TUs. The
*criteria* can and must:

- **`src/common/foreign_items.h`** owns the criterion identifiers, the
  `RSBS_ITEMCLASS_*` bit table, and their meanings (game-header-free, exactly as ADR
  0010 answer O8 places the shared-item classification table in the sanctioned
  `shared_items.h`/`.c` pair — "never a per-game duplicate that can disagree with
  itself").
- Each pool TU implements them against its own enum and reports, per rejected
  candidate, **which criterion rejected it**. That is what makes the lock
  non-vacuous: without it, the only observable for "the rule ran" is a table that
  happens to look right.

### 3.5 Criterion 3's narrowing is designed but **not yet unlocked** — do not ship it early

ADR 0009 decision 2's amendment prescribes that criterion 3 "becomes a
profile-conditional predicate ('admissible when the frozen profile arms its give')
rather than a timing accident" once the MM profile freezes before OoT's `Fill()`.
Measured at `91bc133b`, that unlock is **not available**:

- `MM_Rando_ComputeProfileStamp` (`Foreign.cpp:663-667`) is deliberately
  **side-effect-free** — "no side effects on any save, CVar, or `gComboCtx`". It
  returns a digest and publishes **no option values**.
- `RANDO_SAVE_OPTIONS` is written only by `ResolvePairedProfile`
  (`Foreign.cpp:248-252`), which runs at MM's generation, after OoT's pass.
- And the creation stamp itself still sits **after** `Fill()` and after the spoiler
  write in `Playthrough_Init`, alongside the seed/settings stamps.

So OoT's placement pass can read the *digest* and nothing else. Criterion 3 stays
blanket until ADR 0010 increment 2 moves the freeze ahead of `Fill()` **and** a
values-publishing `src/common` surface exists. Narrowing it earlier would
re-introduce exactly the promise criterion 3 protects: a crossing advertised in OoT
that Termina silently never delivers.

## Decision 4 — Combo settings are world identity: frozen at creation, compared at every arrival and load, refused on divergence

**Reuse the shipped freeze/compare/refuse machinery end to end. Invent nothing.**

One-game semantics are binding: every combo-level decision freezes at file creation,
and arrival-time divergence is corruption, not choice.

### 4.1 The mechanism, mapped onto what exists

| Concern | MM profile (shipped) | Combo settings (this ADR) |
|---|---|---|
| Authoring | tier-3 CVars, up to creation | tier-4 `gCombo.Rando.*`, up to creation |
| Frozen store | `RANDO_SAVE_OPTIONS` (Tier-3) | `comboSettings` (Tier-1, decision 1) |
| Identity term | `mmProfileDigest` (offset 788) | `comboSettingsHash` (offset 880) |
| Frozen predicate | `Combo_MMProfileFrozen()` — digest ≠ 0 | `Combo_ComboSettingsFrozen()` — `formatVersion` ≠ 0 |
| Write gate | on the writers (`combo_mm_options_view.c:120`), not the widget | same, same file |
| Compare-and-refuse | MM arrival → `RsbsSave_RefuseSlotIdentity` + notification | same surface, same call |
| Refusal detail | pass/fail on one digest | **field-level**: `Combo_ComboSettingsDivergence` names what diverged (1.1 justification 2) |
| Transitional writer | `ResolvePairedProfile` freezes a legacy pair at its first crossing | same shape, same call site (4.4) |
| Invalidation | in the KEEP set (`context.cpp:343`/`:399`) | **must** be in KEEP — see 4.3 |
| Rendering | `Combo_MMProfileSummary` over `gComboCtx` only | a twin accessor over `gComboCtx` only |
| Durable commit | rides Tier-1's whole-file commit (ADR 0009 D4/4a) | identical; frozen terms no commit mutates |

The freeze order inside the creation event, stated because decision 1.4 depends on
it: freeze the combo record (from `Combo_ForeignPairingRequested()` and the tier-4
CVars) → stamp `sharedRandoSettingsHash` → stamp `mmProfileDigest` → **then** compute
`comboSettingsHash` over all three.

### 4.2 `formatVersion == 0` changes meaning, loudly

The same reinterpretation #564 V8 forced onto `mmProfileDigest == 0`, restated
before anyone builds a reader:

- On a **legacy** (pre-this-ADR) file, 0 means "no combo settings were ever frozen" —
  correct, exempt from comparison, and repaired by the transitional writer in 4.4.
- On a **post-this-ADR created** file, 0 means **identity not frozen**, a state no
  created combo file may be in. Any surface that renders 0 as a benign default will
  report corruption as normal.

### 4.3 The KEEP set is not optional

ADR 0009's 2026-07-30 consequence: "A term stamped at or before file-create that
KEEP drops is wiped by the creation itself, which is a hole under any freeze design."
Both new fields are authored **by the creation event**, so both join
`Context_InvalidateSessionState`'s KEEP set alongside `mmProfileDigest` and
`foreignPlacementsOoT` (`context.cpp:343`, `:363`, `:399`, `:403`) — and explicitly
**not** alongside `mmPairedAttempt`, which is authored by generation rather than
creation and is correctly dropped (`context.h:677-681`). Getting this backwards is
silent in exactly one direction.

### 4.4 The legacy pair, and why refusing it is wrong

`ResolvePairedProfile` already carries the precedent (`Foreign.cpp:288-297`): a
pre-freeze pair reads digest 0 and "the first crossing is where its profile freezes —
the one transitional writer besides the creation event." Combo settings take the
same shape: a paired file with `formatVersion == 0` freezes the *shipped defaults*
(decision 2.3's "reproduce today's world") at its first crossing, and is compared
normally thereafter. Refusing legacy pairs instead would orphan every `.redsave`
written before this ADR to gain nothing — they were all generated under one set of
rules, which is exactly what the defaults record.

**This is a write, and increment 1 schedules it as one.** "Compared normally
thereafter" is only true if something actually flips `formatVersion` 0 → 1 and fills
the defaults; without that step every legacy file stays at 0, permanently exempt from
comparison, and 4.4 would describe a behaviour the ADR never builds. The writer lives
at the same arrival gate that already carries `ResolvePairedProfile`'s transitional
stamp for `mmProfileDigest`, and it recomputes `comboSettingsHash` in the order of
4.1 immediately after.

---

## Operator questions

The ADR 0010 pattern: each row carries the options and a recommended answer. Rows
marked **blocking** must be answered before increment 1 carves bytes.

| # | Question | Options | Recommended answer |
|---|---|---|---|
| **O1** | **Record or digest.** ADR 0009 claim 2 reserved 4 B and ADR 0010 D1 promised "no new carve". Decision 1 spends 16 B total. | (a) digest only, 4 B, accept that frozen values can never be shown or repaired; (b) `comboSettingsHash` 4 B + `ComboSettingsRecord` 12 B, `reserved[108]` | **(b), blocking.** ADR 0004 §6 state 4 requires showing the frozen value from the save, and combo terms have no per-half tier to live in the way MM's did. The second justification — a refusal that can name *which* rule diverged — is now a scheduled increment-1 task (`Combo_ComboSettingsDivergence`) with its own test lock, not an implied benefit. 12 B against a 108-byte remainder and a 64-byte floor is the cheapest way to make an Accepted requirement implementable. ADR 0010 D1's "no new carve" is corrected, not overruled — it assumed a record that cannot hold pair-level terms. |
| **O2** | **Shipped default direction** for a newly created combo file. | (a) both directions (today's unconditional behaviour); (b) forward only; (c) OFF | **(a).** A default that differs from shipped behaviour silently changes every new world at the moment the setting lands, and the change is invisible in a diff. Match ADR 0010 answer O11's posture: the shipped default is what already ships. |
| **O3** | **Does the item class carry a seed term?** #495 step 2 says yes; decision 3.2 says no. | (a) rule only, variety from the placement draw; (b) rule + per-seed subset | **(a), blocking** (it determines whether the name inverse is total). (b) makes `Combo_GetForeignItemByNameFor` partial on the spoiler-**load** path, which runs in processes that never generated. #495's lock is re-aimed accordingly. |
| **O4** | **Pool-size semantics.** | (a) two per-direction counts, each 1..`RSBS_FOREIGN_PLACEMENT_CAP` (8); (b) one combo-level count split across directions | **(a).** The two tables are separate 8-slot carves; one shared number would have to be re-derived per direction anyway, and a count that can exceed a table's capacity is a setting that lies. Neither option may bump the cap in place (`context.h:187-215`). |
| **O5** | **Legacy paired saves** (`formatVersion == 0`). | (a) freeze shipped defaults at first crossing (the `ResolvePairedProfile` precedent); (b) refuse | **(a).** (b) orphans every already-written paired `.redsave` to detect a divergence that cannot have happened — there was only ever one rule set. The transitional writer is an increment-1 task (4.4), not a consequence. |
| **O6** | **Digest scope.** | (a) `comboSettingsHash` covers the 12-byte record only; (b) it folds the record **and** `sharedRandoSettingsHash` **and** `mmProfileDigest` | **(b).** ADR 0009 decision 1's amendment: a digest narrower than the generator's input set is vacuous, and a guard built on it passes while the thing it guards diverges. A whole-pair fingerprint is what "two peers agree on seed and hash and still get different rules" actually needs. Its input encoding is pinned byte-for-byte per decision 1.4 (ADR 0007 §2's codec discipline) so the fingerprint is a property of the values, not of the host. |
| **O7** | **Item-class selector shape.** | (a) `uint16_t` bitset of named classes per direction; (b) an ordinal breadth rung (narrow/standard/wide) | **(a).** Composable, extends without renumbering, and reads correctly under the `formatVersion` tag. An ordinal collapses independent axes and forces a renumber the first time a class is inserted rather than appended. Bit positions are pinned and append-only per decision 1.2.1. |
| **O8** | **When criterion 3 narrows** to profile-conditional (ADR 0009 D2 amendment / #564 V24). | (a) with this ADR; (b) gated on ADR 0010 increment 2's freeze-before-`Fill()` **plus** a values-publishing `src/common` surface | **(b).** Verified: `MM_Rando_ComputeProfileStamp` returns a digest and publishes no option values, and the creation stamp still sits after `Fill()`. Narrowing now would advertise crossings in OoT that Termina never delivers — the exact promise criterion 3 protects. |

---

## Increments

Ordered so format lands before consumers pin it, and so no increment ships a
behaviour change it cannot lock.

### Increment 0 — bookkeeping (no format change; start immediately)

- Fix the now-stale note at `src/common/context.h:700-708`, which still warns that
  "outstanding claims 2 and 4 (4 + 64) would leave 56, UNDER the 64-byte floor". ADR
  0009's 2026-08-04 amendment retired claim 4 as already spent by PR #473; the live
  arithmetic is `124 − 4 = 120`. Leaving the warning in place invites the next
  carver to raise the record size to solve a problem that no longer exists.
- Record on #498, #493 and #495 which of their steps already landed (the Context
  table above), so the next pass does not re-derive it a fourth time.

### Increment 1 — the carve, the freeze, the gate, the identity fold (**no world changes**)

- Carve `comboSettingsHash` (880) and `comboSettings` (884); `reserved[108]`;
  literal-offset asserts on both, plus the `sizeof == 12` and member-offset asserts.
- The pinned value tables of decision 1.2.1 in `src/common/foreign_items.h`
  (`RSBS_COMBO_DIR_*`, `RSBS_COMBO_GOAL_*`, `RSBS_COMBO_RUNG_*`,
  `RSBS_ITEMCLASS_*`), each with an explicit literal and the
  retire-never-renumber comment.
- `Combo_ComboSettingsFrozen()` + a `Combo_ComboSettingsSummary()` twin of
  `Combo_MMProfileSummary`, in `src/common`, reading `gComboCtx` and nothing else
  (ADR 0008 rule 5).
- **`Combo_ComboSettingsDivergence()`** — the field-level diff between the loaded
  record and the session's resolved settings, returning which fields differ, so the
  #533/#568 refusal notification can name the diverged rule instead of saying only
  that something diverged. This is the capability decision 1.1 justification 2
  claims; it ships here or the justification is withdrawn.
- **`canonical()` and the golden-vector test** — the byte-at-a-time little-endian
  encoder of decision 1.4, plus a fixed-record → fixed-bytes → fixed-digest lock in
  the `redship` tier, modelled on `test_netplay_relay.c`'s wire-format vectors.
- `Combo_ForeignPairingRequested()` — the pre-condition predicate ADR 0009 decision 2
  designed and nobody built.
- Freeze at the creation event in the order of 4.1; both fields into the KEEP set.
- **The transitional writer for legacy paired files** (4.4): at the arrival gate that
  already carries `ResolvePairedProfile`'s transitional stamp, a paired file with
  `formatVersion == 0` gets the shipped defaults written and `comboSettingsHash`
  recomputed, after which it compares normally.
- Compare at every arrival and load; refuse through the #533/#568 surface.
- Fold `comboSettingsHash` into `Rando_HeadlessSeedDeterminismDigest`
  (`3drando/menu.cpp`) — and **not** into `MixPairedFinalSeed()`.
- Both passes read direction/pool size **from the record**. Defaults reproduce
  today's world exactly, so `SeedDeterminism` re-pins on the digest term alone and
  `MMRandoGen` should not move at all.

### Increment 2 — the tier-4 keys and the pane

- `gCombo.Rando.Direction`, `.PoolSize.OoT`, `.PoolSize.MM`, `.ItemClass.OoT`,
  `.ItemClass.MM` (ADR 0003 naming; new tier-4 keys, not converged MM keys).
- The pane renders **ADR 0004 §6 state 4** post-creation: read-only, reason string
  "already decided" and not the capability reason, values read from the summary
  accessor. Enforcement stays on the writers.
- Per ADR 0004 §6's scope note, every new key is classified identity-vs-preference
  at introduction; all five above are identity.

### Increment 3 — rule-defined classes (#495), both halves

- Criterion identifiers in `src/common/foreign_items.h`; per-game evaluators in the
  two pool TUs; derived tables into stable file-static arenas (`const char*` name
  pointers must stay pointer-stable — `test_foreign_items.c` asserts pointer
  identity against `Combo_GetForeignItemName`).
- Both pinned tables retire. `kForeignPoolV1`'s `static_assert(count <= CAP)`
  becomes a runtime clamp against the frozen `poolSize*` plus a compile-time bound on
  the maximum, matching what MM's broad pool already does.
- Class bits are evaluated **after** the six criteria, never around them (1.2.1).
- No seed term (O3).

### Increment 4 — the direction setting becomes real

Each pass no-ops when its direction is unarmed. Deliberately last: it is the only
increment that can change a generated world, and it should land on top of a frozen,
compared, rendered setting rather than under one.

**Not in scope**, and named so it is not absorbed: criterion 3's narrowing (O8, owned
by ADR 0010 increment 2), the placement-cap raise, and increment 3's boundary carve.

---

## Consequences

### Byte spend

| Claim | Field | Size | Offset | Status |
|---|---|---|---|---|
| 2 (ADR 0009) | `comboSettingsHash` | 4 B | 880 | **spent by this ADR, exactly as reserved** |
| **10 (new)** | `comboSettings` (`ComboSettingsRecord`) | 12 B | 884 | **new claim, this ADR** |
| — | `reserved[]` | | 896 | **124 → 108** |

`124 − 4 − 12 = 108`, clear of the 64-byte floor by 44 bytes, and ADR 0009's floor
constraint holds unchanged (`Test_SaveComboRecordFixed`'s scribble loop iterates
`sizeof(reserved)` and passes vacuously at zero). `sizeof(ComboContext)` stays 1004
against the 1024 budget. `RSBS_SAVE_VERSION`, `RSBS_COMBO_CONTEXT_RECORD_SIZE` and
`RSBS_COMBO_CONTEXT_PRECARVE_SIZE` are all unmoved.

**ADR 0009's budget table gains claim 10 and its arithmetic is republished here**;
after this ADR the outstanding-claim list against `reserved[]` is empty, and the next
carver starts from 108 with the append-only second-block rule in force.

### Test locks

Extending the rows that already drive the real chain, per #498's Lock:

- **`ForeignItemGive`** / **`ForeignItemGiveReverse`** (`CMake/SingleExecutable.cmake:381-382`,
  `redship` label, ROM-free): placement count follows the configured pool size —
  asserted against `min(poolSize, RSBS_FOREIGN_PLACEMENT_CAP, candidates)`, **not**
  against the raw setting, or the assertion is red for a correct implementation the
  first time hosts are scarce. Each armed class bit yields only members of that
  class. Every entry is origin-tagged. `formatVersion == 0` ⇒ shipped defaults.
- **Canonical-encoding golden vector**: a fixed `ComboSettingsRecord` produces a
  fixed byte string and a fixed `comboSettingsHash`. This is what makes decision
  1.4's endianness rule enforceable rather than aspirational, and it is the lock
  #574's handshake will inherit.
- **Enumerator pinning**: a compile-time assert per value that each
  `RSBS_COMBO_DIR_*` / `RSBS_COMBO_GOAL_*` / `RSBS_COMBO_RUNG_*` / `RSBS_ITEMCLASS_*`
  literal equals its published number, so a renumbering is a red build rather than a
  silently re-read save. The same shape `RSBS_SHARED_RES_*` already carries.
- **The name round trip stays as ADR 0009 decision 3 corrected it**: `(origin, name)`
  uniqueness within and across pools, plus "a colliding bare name resolves
  differently under each origin". #498's Lock still asks for "no cross-pool
  display-name collision" — that assertion **must not be written**; `"Lens of Truth"`
  collides deliberately (`foreign_items.h`, `ForeignItemsSingleExe.cpp:82-89`) and
  the natural "fix" is to degrade a spoiler name to dodge a lookup bug.
- **`SeedDeterminism`** (`:987`): same settings ⇒ identical digest; any changed combo
  setting ⇒ different digest. This is the assertion #498 calls unassertable today,
  and it becomes expressible the moment decision 1 lands.
- **Freeze/refuse coverage**, mirroring what `mmProfileDigest` already has: a
  post-creation tier-4 write is rejected at the writer (not merely greyed); an
  arrival under a divergent record refuses through #533 rather than generating; the
  record survives file-create invalidation (KEEP) — the same shape
  `test_session_invalidation.c` already drives for `foreignPlacementsOoT`.
- **Refusal names the field**: an arrival whose `direction` alone diverges produces a
  refusal message naming `direction`, not a generic mismatch. Without this row the
  twelve bytes are display-only and decision 1.1's second justification is unbacked.
- **Legacy transitional write**: a `formatVersion == 0` paired file, taken through
  one crossing, comes back at `formatVersion == 1` with the shipped defaults and a
  nonzero `comboSettingsHash`, and a second crossing compares rather than re-freezes.
- **Criterion attribution** (increment 3): every excluded candidate names the
  criterion that excluded it. Without this the class rule has no observable and the
  lock degenerates into "the table looks right".

### Determinism re-pins

One, at increment 1, on the `comboSettingsHash` fold into
`Rando_HeadlessSeedDeterminismDigest`. `MMRandoGen`, `MMPairedAttemptGen` and
`MMPairedAttemptDeterminism` should be untouched — if any of them moves, something
folded into `MixPairedFinalSeed()` that decision 1.4 forbids, and that is the signal
to look for it rather than to re-pin.

### Standing costs

- **The record is a second thing to keep in sync with the CVars up to the freeze
  line.** Mitigated the way `Foreign.cpp` already mitigates it: one string builder /
  one resolver shared by the freeze and by any preview, so the identity term and the
  generation input cannot drift.
- **Two settings surfaces exist during a session** — CVars before creation, the record
  after. That is the one-game ruling's price, and ADR 0004 §6 state 4 is where the
  player meets it.
- **`spare0`/`spare1` will be spent, and they are the record's only headroom.** A
  third field beyond them needs a second block from `reserved[108]`, never a widen in
  place — the same prescription `RSBS_SHARED_RESOURCE_EXT_CAP` followed literally.
- **The pinned value tables are a permanent maintenance obligation.** Every future
  direction, goal, rung or class arrives as the next free literal or bit, and a dead
  one is retired in place. This is a cost the ADR accepts deliberately: the
  alternative is a save format whose meaning depends on source order.

---

## Compatibility

- **Old save, new build.** A pre-carve `.redsave` loads as a byte prefix and
  zero-extends: `formatVersion == 0`, `comboSettingsHash == 0`. Read as "identity not
  frozen", exempt from comparison, defaults frozen at the first crossing (O5). No
  migration, no version bump.
- **New save, old build.** `h.version` is unchanged at 2 and `h.comboSize` is
  unchanged at 1024, so an older binary loads the file normally and reads the two new
  fields as part of `reserved[]` — i.e. ignores them. The pair still validates on the
  terms that build knows. This is the growth contract working, and it is the reason
  the record is a carve and not a record-size raise.
- **Whole-file commit (ADR 0009 D4/4a) needs no accommodation.** Tier-1 is written as
  one unit at the #569 choke point regardless of which half triggered the commit, and
  both new fields are frozen-at-creation terms no commit mutates. "Newest whole commit
  wins" therefore never picks between two versions of a combo record — there is only
  ever one, stamped before the first commit.
- **Spoilers.** No spoiler format change is required by this ADR. If the combo record
  is later written into the spoiler as provenance, it inherits ADR 0009's rule for
  the foreign section: an entry missing the field is **refused, never guessed**.
- **Pool widening (increment 3) is not a format change.** The pool is a source-side
  class; the `.redsave` stores placements, not pools. A wider class changes which
  items a *new* world can host and changes nothing about an existing one.
- **The name inverse stays total** across the class change, which is what keeps
  already-written spoilers loadable — the direct consequence of O3's answer, and the
  reason it is marked blocking.

## Alternatives considered

- **Digest only (4 B), per ADR 0009 claim 2 as literally written.** Rejected: leaves
  ADR 0004 §6 state 4 unimplementable for these keys and leaves refusal unable to
  name what diverged. ADR 0009's own amendment retired the "a save-side record is a
  second settings store" argument; what remains is arithmetic, and 12 bytes fits.
- **Reshaping `ComboForeignPlacement` to `{hostGame, checkId, item}` now that a
  version bump is cheap.** Rejected on ADR 0002 grounds (decision 2.2): a runtime
  discriminator inside one array is exactly the aliasing surface the origin tag
  exists to eliminate, and the two-table split makes the error unrepresentable. The
  cheaper bump changes the price, not the argument — and it now also costs a
  migration of shipped bytes at offset 740.
- **A seed-varying item class (#495 step 2 as written).** Rejected: duplicates
  variety the placement draw already provides, and makes the spoiler-load inverse
  partial in processes that never generated.
- **Hashing the record as a raw struct (`memcpy` of the 12 bytes).** Rejected in
  decision 1.4: it makes the world's identity a property of the toolchain's layout
  and the host's endianness, in a fingerprint whose other two terms are both
  string-encoded specifically to avoid that, and in the exact term #574 wants to
  exchange between peers. ADR 0007 §2 already paid for this lesson once.
- **Bare sequential `enum`s for the new value spaces.** Rejected in decision 1.2.1:
  it makes a mid-list insertion — the ordinary C idiom — silently reinterpret every
  already-written record, which is the failure `RSBS_SHARED_RES_*` and MM's
  `Options.cpp:38` retirement both exist to prevent.
- **Storing combo settings in MM's `RANDO_SAVE_OPTIONS`** alongside the frozen MM
  profile. Rejected: they are not MM's options. Tier-3 is per-game by definition, and
  a combo term living in one half's save is a term the other half cannot read without
  crossing the ADR 0002 boundary to get it.
- **Per-install CVars as the post-creation authority** (the pre-#564 reading of ADR
  0003). Rejected by the one-game ruling, and this ADR does not re-litigate it:
  after the freeze line, no CVar describes a created world and no gameplay path may
  read one to decide world behaviour.

---

## Review notes

Two adversarial reviews were run against the draft — a one-game-semantics lens
(CONDITIONAL PASS: 2 MAJOR, 2 MINOR) and a forward-compatibility / netplay lens
(FAIL: 1 BLOCKER, 1 MAJOR). All six findings were independently re-derived against
`origin/main` = `91bc133b` before disposition. Dispositions:

| Finding | Lens | Severity | Disposition |
|---|---|---|---|
| ADR 0010 D1's growth-contract clause on `goal == 0` silently conflicts with `formatVersion` as the sole occupancy tag | one-game | MAJOR | **Accepted.** Added to the Amends list as an explicit supersession, and cross-referenced from decision 1.3. Refined on one point — see below. |
| O1's "blocking" byte spend partly rests on a refusal reason-string capability no increment builds | one-game | MAJOR | **Accepted via the reviewer's fix (a), not fix (b)** — see the recorded partial rejection below. `Combo_ComboSettingsDivergence()` is now an increment-1 task with its own test lock, and O1's recommended answer says so. |
| Decision 4.4's transitional writer for legacy paired files has no increment task | one-game | MINOR | **Accepted.** Added to increment 1, to 4.4 itself, to the 4.1 mapping table, and to the test locks. |
| Verification anchor stale; ADR 0009 decision 4/4a unacknowledged | one-game | MINOR | **Accepted.** Anchor bumped to `91bc133b` / 2026-08-05; the ADR 0009 dependency bullet now names decisions 4 and 4a and states their orthogonality, and Compatibility carries the same statement where a reader would look for it. |
| No pinned, append-only value space for `direction` / `goal` / `logicRung` / `itemClass*` | forward-compat | BLOCKER | **Accepted.** New decision 1.2.1 assigns explicit literals to every enumerator and bit position, states the retire-never-renumber rule against the `RSBS_SHARED_RES_*` and `Options.cpp:38` precedents, and adds a compile-time pinning lock plus an increment-1 task. |
| `canonical(comboSettings)` undefined; departs from the codebase's digest-encoding discipline | forward-compat | MAJOR | **Accepted.** Decision 1.4 now defines it as field-by-field, declaration-order, little-endian, byte-at-a-time encoding per ADR 0007 §2, notes that both folded half-digests are string-encoded for the same reason, and pins it with a golden-vector test in increment 1. |

**Recorded rejections.**

1. **Rejected: the one-game lens's alternative fix for finding 2 — "strike
   justification #2 and let O1 rest on justification #1 alone."** The reviewer
   offered this as an equally acceptable smallest fix and judged justification 1
   sufficient on its own. It is sufficient *to carry O1*, but striking justification
   2 would leave the ADR silent about a capability the refusal surface already
   promises: #533/#568 was built to be loud **and explainable**, and a combo-identity
   refusal that cannot name the diverged rule is the un-repairable case ADR 0009
   accepted only for want of an alternative. Withdrawing the justification would
   preserve the ADR's internal consistency at the cost of shipping the same
   un-nameable refusal the record exists to retire. Scheduling the work is the
   cheaper honesty, so fix (a) was taken and fix (b) is declined on the record.

2. **Partially rejected: the one-game lens's proposed wording for finding 1 —
   "superseded" as a flat verb.** ADR 0010 D1's clause is superseded in *mechanism*
   (unset-ness moves from `goal`'s own zero to `formatVersion`) but preserved in
   *effect* (a legacy record still makes no beatability claim and is still never
   silently promoted to `beat-both`). Recording a flat supersession would suggest the
   protection itself lapsed, which is the opposite of what decision 1.3 does. The
   Amends bullet therefore states both halves. The substance of the finding — that
   the conflict must be named rather than absorbed — is accepted in full.

No finding was rejected outright.
