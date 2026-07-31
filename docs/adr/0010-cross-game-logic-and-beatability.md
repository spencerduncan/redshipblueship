# ADR 0010: Cross-game logic and goal-parametric beatability — one bag, one linked fixpoint, frozen at creation

- Status: **Proposed** (2026-07-31)
- For: #500 (Phase 3.2 tracker — cross-game logic and beatability, Lane D
  promoted); shaped throughout by #564 (one-game alignment audit)
- Depends on:
  - **[ADR 0002](0002-origin-tagged-shared-items.md)** (Accepted) — the
    origin-tag invariant (no raw game-local id crosses a game boundary outside
    a `SharedItem`) and the `ComboContext` growth contract. Every boundary
    object below obeys it.
  - **[ADR 0004](0004-menu-information-architecture.md)** (Accepted; §6/§4.1a
    amended 2026-07-30 for one-game semantics) — presentation states for
    frozen identity keys. This ADR, if accepted, supersedes one §4.1a
    consequence (the paired `RO_LOGIC` default) — see increment 1.
  - **[ADR 0009](0009-combo-settings-and-reverse-pool.md)** (Accepted;
    decisions 1/2 amended 2026-07-30) — combo settings authoring, the frozen
    profile record, `comboSettingsHash` (claim 2, reserved), the three-tense
    pairing predicates. The GOAL setting and both trick sets defined here fold
    into that identity machinery; no new mechanism is invented.
  - **#564's target creation-event contract** (steps 0-10) — the event this
    ADR's guarantees are evaluated inside.
  - Phase-1 substrate: **landed** — #568 (REFUSED as a first-class slot state,
    quarantine + latch + surface, for #533) and #569 (one game-thread commit
    choke point + monotonic commit generation in both artifacts, for #537);
    **in flight** — #570 (creation-time MM profile freeze + arrival
    compare-and-refuse, for #498/#564 phase 2 step 9).
- Sources: the #500 design corpus (the OoTMM beatability research and the
  merged-generation feasibility probe, both 2026-07-30) and five operator
  rulings quoted verbatim in the Context. OoTMM citations are anchored at
  OoTMM commit `669aaf5` as read by the corpus; redship citations are
  re-verified at `origin/main` = `aafee46b`.

Everything in the **Decisions** below is proposed as final for Phase 3.2.
The **Open questions** table at the end is the part awaiting operator
acceptance; nothing in it blocks increment 1.

## Context

### Where 3.1 left the combo

Phase 3.0/3.1 shipped a deliberate bargain: **free-form placement plus a
spoiler log**, with the spoiler carrying the burden logic would otherwise
carry (`docs/phase3-roadmap.md`, Lane D: "not placing MM's Bow behind an OoT
check that requires MM's Bow is the hardest problem in the phase"). At
`aafee46b`:

- Both directions of the crossing are **post-fill overlays of duplicates**.
  Nothing is removed from either origin pool, so the crossing is logically
  inert: pair beatability factors exactly into "OoT beatable" (guaranteed by
  its own fill) AND "MM beatable" (unguaranteed), and any pair-level
  beatability assertion written today passes vacuously.
- OoT has real machinery: a static region graph with 4-state age/time
  reachability (`games/oot/soh/Enhancements/randomizer/location_access.h`),
  assumed fill, `CheckBeatable` (`3drando/fill.cpp:589-604`),
  `IsBeatableWithout` (`fill.cpp:300-308`), sphere playthrough, a headless
  harness, and CI-pinned determinism digests.
- MM has a real region graph (`games/mm/2s2h/Rando/Logic/Logic.cpp:10`,
  populated by ShipInit registrars; traversal `FindReachableRegions` at
  `Logic.cpp:92-136`; a 45-slice time model, `Logic.h:20+`) but **no
  beatability predicate anywhere in `games/mm`**, a Glitchless solver that
  is a forward fill mutating the live `gSaveContext`
  (`GlitchlessLogic.cpp:22, :57, :258`) under a 10s wall-clock abort
  (`:64`), and a paired world that defaults to **Nearly No Logic** — a
  shuffle plus a scene blacklist, zero reachability
  (`Rando/Logic/NearlyNoLogic.cpp:12-89`; default resolved at
  `Rando/Foreign.cpp:126-128` by CVar-existence probe).
- A failed paired generation still reverts to a **silent vanilla Termina**
  (`Rando/MiscBehavior/OnFileCreate.cpp:317-329`) — the divergence class the
  one-game ruling names corruption.

### The substrate that landed under this ADR's feet

The #564 alignment plan's phase 0/1 is in: REFUSED is a first-class slot
state with quarantine and an armed-session latch (#568, for #533), and every
durable write marshals through one game-thread commit choke point stamping a
monotonic commit generation into both artifacts (#569, for #537). #570 (in
flight) freezes the MM option profile into the pairing identity at creation
and makes arrival compare-and-refuse. **Refusal no longer converts to data
loss**, which is what makes every guarantee below shippable: this ADR's
failure mode everywhere is "refuse loudly through the #533 surface", and
that surface now exists.

### The operator rulings this ADR encodes

Recorded verbatim because the decisions below are their design consequences,
not proposals.

**One game (2026-07-29, on #500):**

> "freezing at creation is the correct semantics for sure. keep that idea in
> mind with all designs. this is *one game* from a semantic standpoint."

Binding consequences (recorded on #500 and #564): one identity fixed at one
creation event; arrival-time divergence is corruption to detect and refuse,
never a choice to honor; merged generation at OoT file-create is the natural
implementation; combo-level settings are the one game's settings.

**Beatability is goal-parametric (2026-07-31):**

> "beatability is defined by the game type and other rules. mm lets you pick
> how many remains are required to get to majora. that many is beating the
> game if beating majora is part of the win condition. maybe the setting is
> beat either game. maybe its triforce hunt."

**One bag, linked at sphere zero (2026-07-31):**

> "needing to cross games to make progress in either game is expected
> behavior. consider the base behavior of this for the original users of
> this: speedrunner types. for someone who knows how to break this game,
> there are very few unwinnable situations. the base algorithm would be 'put
> every item from both games into a bag, then randomly distribute to each
> check'. you can use sphere expansion or something to include beatability.
> in that sense, you can think of each game as its own sphere expansion that
> are both linked at sphere zero (assuming you can easily get to castle town
> in your oot which is easy with settings)."

**Tricks are part of the proof (2026-07-31):**

> "both games have a graph of tricks/glitches that the user can choose as a
> valid path for the randomizer to include in beatability."

**Reuse the solvers (2026-07-31):**

> "each game already has its own solver of this problem."

with the refinement:

> "you *can* merge them in code if that results in an easier to maintain end
> product; just know what's there."

---

## Decision 1 — The beatability contract is goal-parametric

**"Beatable" is not a fixed predicate. It is: the combo's GOAL expression is
provable by the deterministic reachability fixpoint, under the world's frozen
rules — and the GOAL is a combo-level setting, frozen at creation, part of
the pairing identity.**

### 1.1 The GOAL setting

A new combo-level (ADR 0003 tier-4, #498-owned) setting — working name
`gCombo.Rando.Goal` — with an extensible value set, at minimum:

| Value | Goal expression |
|---|---|
| `beat-both` (default) | `OOT_GOAL && MM_GOAL` |
| `beat-either` | `OOT_GOAL \|\| MM_GOAL` |
| `triforce-hunt` | combo-level piece requirement met (accounting: open question O10) |

This is OoTMM's own shape — its `goal` setting defaults to `'both'` and is
evaluated as one boolean over the merged event set
(`packages/core/src/settings/data.ts:76-89`; `isGoalReached`,
`pathfind.ts:712-739`: `events.has('OOT_GANON') && events.has('MM_MAJORA')`;
OoTMM @ `669aaf5`, per the #500 corpus). OoTMM never had a "pair" concept
separate from "the world"; neither does the one game.

Identity mechanics, all existing machinery (ADR 0009):

- The resolved GOAL value and its parameters **fold into
  `comboSettingsHash`** (ADR 0009 claim 2, 4 B, reserved) and into the frozen
  record #570 stamps. No new carve.
- Frozen at **creation-event step 1** (#564 contract): nothing may be read
  from a CVar after the freeze line, the GOAL included.
- Arrival or load divergence from the frozen GOAL is **corruption to refuse**
  through the landed #533/#568 machinery — same treatment as any identity
  term, per the one-game ruling and #570's compare-and-refuse.
- Growth contract: stored value 0 means **unset** (a pre-3.2 legacy record).
  A legacy record makes no beatability claim; it is displayed as such, never
  silently promoted to `beat-both`.

### 1.2 The goal's parameters are each half's own authored settings

The ruling's operative sentence — "mm lets you pick how many remains are
required to get to majora. that many is beating the game" — means the goal
expression does not invent new knobs. Each half's existing, authored access
settings ARE the goal parameters, and the corpus's open question "what does
MM-beatable even mean" is thereby settled in shape:

**`MM_GOAL`** = Majora's Lair reached and Majora defeated, provable through
MM's own authored gate chain, with MM's own authored parameters:

- Moon access: `CAN_PLAY_SONG(OATH) && MeetsMoonRequirements()` on the
  `THE_MOON` exit (`Rando/Logic/Regions/Central.cpp:83`), where
  `MeetsMoonRequirements()` is
  `RemainsCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MOON_REMAINS_COUNT] &&
  MoonMaskCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MOON_MASKS_COUNT]`
  (`Rando/Logic/Logic.h:409-412`; `RO_ACCESS_MOON_REMAINS_COUNT` defaults to
  4, `StaticData/Options.cpp:38`).
- Lair access:
  `RemainsCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_REMAINS_COUNT] &&
  MoonMaskCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_MASKS_COUNT]` on
  the `MAJORAS_LAIR` entrance (`Rando/Logic/Regions/Moon.cpp:115`;
  `RO_ACCESS_MAJORA_REMAINS_COUNT` defaults to 0, `Options.cpp:22`).
- MM's own triforce machinery exists for the hunt variant:
  `RO_SHUFFLE_TRIFORCE_PIECES`, `RO_TRIFORCE_PIECES_MAX`,
  `RO_TRIFORCE_PIECES_REQUIRED` (`Options.cpp:70, :75-76`).

**`OOT_GOAL`** = per OoT's own goal settings:

- `RSK_TRIFORCE_HUNT` ("Triforce Hunt": Off / Win / Ganon's Boss Key,
  `settings.cpp:418`) with `RSK_TRIFORCE_HUNT_PIECES_TOTAL` /
  `RSK_TRIFORCE_HUNT_PIECES_REQUIRED` (`settings.cpp:433, :441`). Off means
  Ganon defeated.
- Ganon's-path gates as parameters: `RSK_RAINBOW_BRIDGE` (Vanilla / Always
  open / Stones / Medallions / Dungeon rewards / Dungeons / Tokens / Greg,
  `settings.cpp:190`) and its count settings, `RSK_GANONS_TRIALS` /
  `RSK_TRIAL_COUNT` (`settings.cpp:274, :284`), `RSK_GANONS_BOSS_KEY`.

Both halves' goal parameters are already inside the frozen identity: OoT's
settings hash folds its own options, and #570's widened `mmProfileDigest`
covers MM's (per ADR 0009 decision 1's amendment, a digest narrower than the
generator's input set is vacuous). The GOAL value composes them; it does not
duplicate them.

**Consequence, stated plainly:** beatability under `beat-either` promises
exactly the goal expression and nothing else. If only one half's goal is
provable, the other half may contain unreachable required items — that is
the contract working, not failing. A player who wants both halves finishable
selects `beat-both`. Per-half strictness beyond the goal ("all locations
reachable") remains each half's own existing axis
(`RSK_ALL_LOCATIONS_REACHABLE`; OoTMM's `allLocations` analogue) and
composes with any GOAL.

## Decision 2 — One bag: items leave origin pools, and cross-game progression is base behavior

**The destination fill is a single combined bag over both games' items,
distributed across both games' check sets. Needing to cross games to make
progress in either game is expected base behavior, not an advanced tier.**
This settles the corpus's first open question — the one the tracker said
"gates everything after" — in the removed-from-origin direction.

Terms, defined precisely because the two regimes get confused:

- **Duplicate overlay** (today, and increments 1-2, explicitly
  *transitional*): a crossed item is an extra copy placed post-fill in the
  other game; the origin world keeps its own copy and stays self-beatable;
  pair beatability factors per-game; every pair-level beatability assertion
  is vacuous.
- **Removed-from-origin** (the decided destination, increment 3): the union
  of both games' item pools is one bag; one fill assigns bag items to the
  union of both games' shuffled checks; an item may exist only in the other
  game; each half is in general NOT self-beatable and the goal proof is
  genuinely pair-level. Origin tags travel with every placement (ADR 0002);
  the fill's output is origin-tagged end to end.

### 2.1 The reachability model: two sphere expansions linked at sphere zero

One fixpoint over the linked graph. Each game is its own sphere expansion
over its own region graph; the two are joined through the crossing edges
(Happy Mask Shop ↔ Clock Tower, `src/common/entrance.h`). Under settings
that leave the crossing reachable from the start — OoT child start reaches
Castle Town's Happy Mask Shop cheaply — the link participates from **sphere
zero**, which is the operator's framing.

The nuance to hold onto: **the crossing is an edge with a requirement
expression, like any other edge.** If settings gate it (a closed Deku Tree
path, an entrance requirement, a time-of-day constraint), the link enters
the expansion at whatever sphere its requirement is first met, and the
fixpoint handles that with no special case. There is no "phase where the
games connect"; there is an edge whose guard becomes true at some sphere,
possibly zero. OoTMM's precedent for the guard's content: it auto-ANDs
`can_reset_time` onto every entrance into MM except the designated game
link, and re-stamps Day 1 into the MM time mask on every OoT→MM edge
(`entrances.ts:210-212`; `pathfind.ts:596-599`; OoTMM @ `669aaf5`).
Redship's exact requirement expression for each direction of
Happy Mask Shop ↔ Clock Tower is **open question O2** — the corpus's
"crossing-entrance safety predicate" question, now concretized as "author
the crossing edge's requirement expression".

### 2.2 The logic ladder: no-logic base, provable rungs on top

The audience inverts the ladder. The **base mode is single-bag no-logic**:
put every item from both games into a bag, distribute randomly across both
games' checks, spoiler log carries the burden. Its target user is the
operator's "speedrunner types" — players who can break both games, for whom
"there are very few unwinnable situations", and for whom an unwinnable seed
is an accepted cost of the mode, exactly as each game's existing No Logic
already is (`RO_LOGIC_NO_LOGIC`; OoTMM's `logic: 'none'` bypasses the
pathfinder entirely and may produce unbeatable seeds by design,
`pathfind.ts:242-256`).

Beatability modes **layer on top** via deterministic sphere expansion, per
Decision 1: a rung's guarantee is "the GOAL expression is provable under
this rung's rules". The rung set, frozen at creation like everything else:

| Rung | Guarantee | Who it is for |
|---|---|---|
| `none` (base) | no proof; spoiler carries the burden | players who can break both games |
| `beatable(tricks = T)` | GOAL provable with trick set T enabled | everyone else, tuned by T (Decision 3) |
| `all-reachable(tricks = T)` | GOAL provable and every location reachable | completionists; composes per-half strictness axes |

A maximal-tricks `beatable` config approximates the speedrunner while
*keeping the proof* — see Decision 3.4. The shipped default rung is open
question O11. This combo ladder is distinct from the **attempt ladder**
(deterministic seed re-roll on fill dead-end, increment 1); the two are
named separately everywhere below because conflating them produced the
silent-vanilla-revert bug class.

### 2.3 What the guarantee is, formally

The right formalism — established by the corpus against the reference
implementation and adopted here — is a **least fixed point of a monotone
operator over a finite join-semilattice**, the Datalog / points-to-analysis
class, not plain graph reachability. The dependency `area → location → item
→ guard → area` is cyclic regardless of exit topology, so there is no
a-priori DAG; the DAG (sphere decomposition) is the **stratification of the
least fixed point**, an output (`getSpheres`, `solve.ts:563-580`;
`makeSpheresRaw`, `analysis.ts:66-105`; OoTMM @ `669aaf5`).

The reference's load-bearing mechanics, all adopted as disciplines here:

- **The guarantee is the fill's exit condition, never a check bolted after
  it.** `LogicPassSolver.run()` loops "pathfind over the partial placement;
  if goal reached, stop; else place exactly one more required item"
  (`solve.ts:405-438`) — the loop can only terminate in a state where the
  goal is provable. The placement primitive is **assumed (reverse) fill**
  (`randomAssumed`, `solve.ts:1189-1217`); the trailing `fillAll()` dumps
  the rest with no logic at all (`solve.ts:1248-1257`), sound **only**
  because the reachability operator is monotone in items.
- **Monotonicity is protected by a negation ban**: runtime `!` over
  `has()`/`event()` throws at expression-build time (`expr/builder.ts:61-74`).
  Redship's conditions are C++ lambdas, so the ban becomes a review rule
  plus an enforcement mechanism (open question O6) — but the invariant
  itself is not optional; assumed fill and the trailing no-logic dump are
  unsound without it.
- **Exclusive world states are per-path constraint accumulation, not state
  queries**: constraint bitsets with contradiction detection
  (`compile.ts:34-45, :251-277`) model MM's mutually-exclusive region states
  and "which game's world-state does this path assume" without duplicating
  graphs.
- **The join must be a real join.** OoTMM's `mergeAreaData` unions times and
  intersects constraint flags, and iteration re-explores on a covering test
  (`pathfind.ts:155-170, :348-351`). Redship MM's `FindReachableRegions`
  guards on first visit and **overwrites** `regionTimeStates`
  (`Logic.cpp:92-136`) — a region first reached with a poor time set is
  never re-explored when a better one appears. That is a correctness bug
  class in MM's crawl **today** and is fixed as part of increment 1's
  factoring, before any cross-game fact is computed from that crawl.
- **Failure handling is deterministic restart** (`retry()`,
  `solve.ts:339-359`, `attemptsMax = 100`), which is the attempt ladder's
  precedent.
- **Resource exhaustion is designed out, not counted**: the
  `renewable`/`license`/source-event triad plus a placement restriction
  forcing critical renewables into renewable locations (`locations.ts:87-115`;
  `solve.ts:1204-1206`). This connects directly to redship's shared-resource
  discipline (#525's kind-tagged slots, the #540/#554/#555 harvest/apply
  gates): "very few unwinnable" is the *base mode's* bargain for players who
  can break the games; the **beatable rungs' fill must respect
  renewable-vs-consumable discipline**, because consumable-funded paths are
  exactly what a non-speedrunner cannot re-earn. The ladder's stricter rungs
  exist for them.

**Never a stochastic post-check.** The reference has no softlock hunter; its
two stochastic passes are hint refinement (`monteCarloZigZag`,
`analysis-foolish.ts:54-172` — Monte-Carlo sampling over a deterministic
oracle, output feeds hint quality only) and an entrance-shuffle fillability
stress test (`validate()` → `forwardFill`, run only when ER changed the
world). Redship builds neither for beatability; a stochastic pass may only
ever be layered on top of the deterministic core for hint quality (future
hints epic), never as the guarantee.

## Decision 3 — Tricks and glitches parametrize the proof

> "both games have a graph of tricks/glitches that the user can choose as a
> valid path for the randomizer to include in beatability."

### 3.1 What exists at source

**OoT already has the full system.** SoH carries a per-trick option table:
`RandomizerTrick` (`RT_*`, `randomizerTypes.h:4188+`),
`TrickOption::LogicTrick` rows built by the `OPT_TRICK` macro
(`settings.cpp:113`), surfaced as the "Logical Tricks" option group
(`settings.cpp:2200`), read by logic through
`ctx->GetTrickOption(RandomizerTrick)` (`settings.cpp:2889-2890`). A trick
gates a logic edge by widening its guard disjunctively — 425 conditions
under `location_access/` consult a trick, e.g. Deku Tree B1:
`logic->IsAdult || ctx->GetTrickOption(RT_DEKU_B1_SKIP) || ...`
(`location_access/dungeons/deku_tree.cpp:118`, likewise `:106`). Enabled
tricks are already folded into OoT's settings hash (the scope #564 V4 told
the MM digest to copy).

**MM does not have it yet.** 2ship's logic dialect has no per-trick option:
the only coarseness knob is `RO_LOGIC` itself, and the region graph carries
the seams as comments — "TODO: Trick for doing without the Bunny Hood"
(`Rando/Logic/Regions/Central.cpp:365`), "if someone wants to make it a
trick later feel free" (`MilkRoad.cpp:29`), and more. Until MM grows a
per-trick vocabulary (open question O9), its trick dimension collapses to
its `RO_LOGIC` mode; the model below is written so that is a degenerate
case, not a special one.

### 3.2 In the unified model, the trick set parametrizes edge requirements

The enabled trick set `T` is an input of the reachability operator: the
sphere-expansion fixpoint runs over the graph **with the chosen tricks'
guard-widenings satisfied**. Beatability is therefore always
"provable under the selected tricks" — `beatable(tricks = T)` — and the
trick set joins the GOAL expression as the second axis of the beatability
parameter space. This is not new machinery in either engine: it is exactly
what `GetTrickOption` in a guard already does; the unified statement just
names it as a proof parameter.

### 3.3 Trick selections are frozen combo identity

Per one-game semantics, **both games' trick selections are combo-identity
terms**: frozen at creation (contract step 1), folded into the pairing
digest (OoT's hash already folds its tricks; MM's per #570's widened
digest scope when its vocabulary exists), rendered post-creation in ADR 0004
§6 state 4 (frozen, read-only, with reason). A post-creation trick toggle is
divergence to refuse — the proof was computed under `T`, and a world played
under `T' ≠ T` is not the world that was proved. Across the game boundary,
trick sets travel **only as opaque identity terms / digest input**: no
game-local trick enum crosses raw (ADR 0002; the coordinator layer sees "a
digest over MM's trick selections", never an `RT_*` or an MM trick id).

### 3.4 The bridge to the base audience

A maximal-tricks `beatable` configuration is the formal approximation of the
player who can break both games — near-base-mode freedom of routing, but the
proof still holds. The ladder therefore reads, from loosest to strictest:
`none` (no proof, the bag alone) → `beatable(T = maximal)` (speedrunner
routes, proved) → `beatable(T = ∅)` (conservative Glitchless) →
`all-reachable`. The trick axis is what makes the ladder continuous rather
than a cliff between "anything goes" and "granny logic".

## Decision 4 — Informed reuse: the solvers stay authoritative; composition is the default posture

> "each game already has its own solver of this problem." /
> "you *can* merge them in code if that results in an easier to maintain end
> product; just know what's there."

**The non-negotiable is informed reuse.** Both ports already own a
battle-tested solver: SoH's 3drando logic/fill (region table +
`ReachabilitySearch`/assumed fill/`CheckBeatable`/spheres,
`3drando/fill.cpp`) and 2ship's Rando logic (registrar-built region map +
`FindReachableRegions` + the Glitchless forward fill,
`Rando/Logic/`). Whoever implements increment 3 must first know both in
depth: a **solver-inventory audit** — graph representation, trick gating,
fill algorithm, evaluation entry points, save/state coupling — is an
explicit first step of the increment-3 epic, before any line of the combo
pass is written.

**Default shape: composition by coordinated alternation.** Realize the
unified fixpoint as the standard fixpoint-of-two-monotone-operators
composition: each game's own solver runs its own reachability expansion
against its own graph; between rounds the coordinator exchanges **only
boundary state** — items obtained from checks the other side granted,
crossing-edge availability, shared-resource state — and iterates until
neither side unlocks anything new. Convergence follows from both operators
being monotone on the same finite lattice (Decision 2.3's disciplines are
what make MM's operator honestly monotone). The boundary exchange is
origin-tagged `SharedItem`-shaped data throughout, so ADR 0002 holds by
construction: each solver names only its own ids. Upstream diffs in both
trees stay small; the new code is a **thin coordination layer in
`src/common`**. The single-bag fill composes the same way: the combo pass
draws from the union bag but delegates per-check placement legality and
per-round reachability queries to the owning game's solver.

**Code-level unification is permitted, on maintainability grounds.** If the
audit concludes that merging the two solvers into one engine yields an
easier-to-maintain end product, that choice is legitimate. The trade-offs,
stated so the choice is made with eyes open:

| | Composition (default) | Unification |
|---|---|---|
| Upstream diffs | small, both trees | large port; permanent divergence cost against SoH/2ship |
| ADR 0002 | clean by construction (boundary = SharedItem-shaped) | needs explicit care; a unified engine is one namespace by temptation |
| Logic dialects | two, forever (C++ lambda styles differ) | one dialect, one engine to maintain |
| MM's live-save mutation | contained behind the query seam (memcpy swap discipline, `GlitchlessLogic.cpp:22/:258`) | forced to fix it properly (detached simulated save, as OoT's `Logic::mSaveContext`) |
| Determinism digests | re-pin once (boundary observable) | re-pin everything |

**The contract is binding regardless of shape**: single bag, unified
fixpoint semantics, sphere-zero-linked expansion, goal expression as the
fill's exit condition, trick parametrization, identity frozen at creation.
The implementation question this leaves the increment-3 epic is "what does
the thin coordination layer look like" (or, if unification wins the audit,
"what does the merged engine's boundary with ADR 0002 look like") — open
question O4 — not "is there a combo-level guarantee".

## Decision 5 — Three increments

Restructured under the pool ruling: the destination is increment 3;
increments 1 and 2 are **transitional** (crossings remain duplicate
overlays) and are what make 3 reachable.

### Increment 1 — per-game guarantees under the pair (DECIDED; can start immediately)

While crossings are duplicates, pair beatability factors per-game, so the
smallest honest step closes the open half — MM's — and hardens the seams:

1. **Paired-MM logic default flips Nearly No Logic → Glitchless** at its
   single resolution point (`Rando/Foreign.cpp:126-128`). An explicit player
   choice of a no-logic mode is still honored (existence-probe semantics
   unchanged) — choosing away the proof is legitimate and is recorded in the
   frozen identity like everything else. This supersedes, on acceptance, ADR
   0004 §4.1a's "no raised profile ships" consequence for this one default;
   the dead-end-rate concern recorded there is answered by (2), not ignored.
2. **The deterministic attempt ladder replaces the silent vanilla revert.**
   On a fill dead-end: `seed_n = Hash(master ‖ ":glitchless-attempt-" ‖ n)`,
   attempt index recorded in the save and the spoiler so the world stays a
   pure function of identity (spec details: open question O3). Terminal
   failure **refuses** through the #533/#568 surface — at arrival while
   generation still runs there, moving to file select with increment 2 —
   never `SAVETYPE_VANILLA` (`OnFileCreate.cpp:317-329` loses its catch-all
   job).
3. **Reachability-gated foreign hosts**: factor the check-tracker crawl
   (`Rando/CheckTracker/CheckTracker.cpp:240-305`) into `Rando::Logic` — 
   adopting the join-and-recheck discipline while in there, fixing the
   `regionTimeStates` overwrite (Decision 2.3) — and intersect
   `PlaceForeignItems`' candidate list with the reachable-check set. Narrows
   a list; cannot dead-end a fill.
4. **CI locks over pinned seeds**: extend the headless MMRandoGen rows —
   paired generation under the ladder succeeds for N pinned master seeds
   (red today: nothing retries); every `gComboCtx.foreignPlacements` entry
   is in the reachable set (red today: the xorshift picks over all junk
   holders, including Moon/temple-interior checks).

Honest limit, stated so nobody writes the vacuous lock: increment 1 makes
"OoT beatable ∧ MM beatable" true by construction, which under duplicates
equals `beat-both` — but a *pair-level* beatability assertion is still
theatre until items leave origin pools. Pair locks land with increment 3,
paired with removal (per the tracker's lock guidance).

### Increment 2 — merged generation, FULL delivery (DECIDED)

The #500 probe's delivery **option 1**, which #564 ruled is the one that
satisfies one-game semantics (the hybrid is acceptable only as a transition
and is "option 1 with the bytes thrown away"):

- The whole creation runs at the OoT file-create seam
  (`games/oot/src/code/z_sram.c`, after `Context_InvalidateSessionOnNewGame`
  and before `Save_SaveFile` — `z_sram.c:306` at HEAD), executing #564's
  creation-event contract steps 1-9: freeze (profiles, GOAL, tricks, seeds)
  **before OoT's `Fill()`** (ADR 0009 D2's amendment), both fills, both
  crossing passes, validation + attempt ladder, one spoiler, atomic identity
  publish, author-and-arm the MM shadow
  (`Context_UpdateShadowCopy(GAME_MM, ...)` +
  `Context_ArmShadowAsFrozen(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0)`).
- **Arrival becomes hydrate-or-refuse** with zero generation capability —
  #570's compare-and-refuse, completed by deleting the arrival-time
  generation dispatch.
- **`sRandoInitDone` splits core/asset two-phase**
  (`games/mm/2s2h/GameExports_SingleExe.cpp:1404-1512`): creation-time
  generation must init MM's rando core without latching away the real MM
  boot's asset work (GfxPatcher, tracker icons, asset-gated DLs). The
  once-only-init class from the resume contract, in reverse.
- **Generation failure fails the creation, at file select**, on the #533
  surface, wholly — no partial identity, no vanilla Termina. This is where
  the attempt ladder's terminal case lands permanently.
- Feasibility is settled, not hoped: the rando-determinism CI row already
  runs OoT generation plus the full paired MM generation in one process with
  MM never booted and no MM archives mounted
  (`src/common/test_runner.cpp` → `MM_Rando_HeadlessForeignDigest`).
- Cost acknowledged: SeedDeterminism / MMRandoGen / HeadlessForeignDigest
  digests re-pin.

Increment 2 is the **prerequisite of increment 3**: a single-bag fill is a
single generation event by definition — both worlds' placements must be
decided at one seam before either spoiler exists.

### Increment 3 — the single-bag combo fill (DECIDED scope; the phase's destination)

Items leave origin pools. One fill, at the creation event, draws from the
union bag and places across both games' shuffled check sets, per Decisions
2-4:

- **Needs (from the substrate):** the pre-Fill pairing gate
  `Combo_ForeignPairingRequested()` (ADR 0009 decision 2 — designed, still
  unimplemented; #493's named gap); the ADR-0002-clean boundary language
  (origin-tagged `SharedItem` + host check id + game-neutral bounds — exact
  scalars and carve budget: open question O7, against `reserved[132]` under
  the append-only second-block rule); the negation-ban and constraint-bitset
  disciplines in force in both graphs (Decision 2.3, enforcement: O6); MM's
  crawl join-fix landed (increment 1.3); the solver-inventory audit and the
  composition-vs-unification choice (Decision 4, O4).
- **The fill's exit condition is the GOAL expression provable** under the
  frozen rung and trick set (Decisions 1-3), via assumed fill against the
  linked fixpoint; the attempt ladder is the failure policy; the trailing
  no-logic dump is legal only under the monotonicity disciplines.
- **The base rung ships here too**: single-bag `none` is this same fill with
  the proof obligation off — the operator's "bag → randomly distribute"
  algorithm — so the speedrunner base mode and the proved rungs are one
  code path with the exit condition parametrized, not two fills.
- Pair-level beatability locks land **here**, paired with removal: generate
  a paired world with an OoT item hosted only in MM; assert the goal
  provable; assert removing the MM host flips it unprovable. Both halves
  required — without removal the lock is theatre.

## Decision table

### Decided by this ADR (operator-ruled or corpus-settled; not up for re-litigation in the epics)

| # | Decision |
|---|---|
| D1 | Beatability is **goal-parametric**: a combo-level GOAL setting (`beat-both` default, `beat-either`, `triforce-hunt`; extensible), frozen at creation, folded into `comboSettingsHash` + the frozen record, divergence refused via #533/#568/#570 machinery |
| D2 | Each half's authored access settings ARE the goal parameters (MM: `RO_ACCESS_MOON_REMAINS_COUNT` / `RO_ACCESS_MAJORA_REMAINS_COUNT` / masks counts / triforce rows; OoT: `RSK_TRIFORCE_HUNT*`, `RSK_RAINBOW_BRIDGE*`, `RSK_GANONS_TRIALS`, `RSK_GANONS_BOSS_KEY`) |
| D3 | **Items leave origin pools**: single combined bag over both games, cross-game progression is expected base behavior; duplicates in increments 1-2 are transitional |
| D4 | Reachability is **one deterministic fixpoint over the linked graph** — two sphere expansions joined at sphere zero; the crossing is an ordinary requirement edge; the guarantee is the fill's **exit condition**, never a stochastic post-check; no softlock hunter is ever built |
| D5 | The **logic ladder** is base-first: `none` (single-bag free-form, speedrunner bargain) with provable rungs layered on top (`beatable(tricks)`, `all-reachable`) |
| D6 | **Trick sets parametrize the proof** and are frozen combo identity, crossing the boundary only as opaque digest input (ADR 0002); OoT's `RT_*`/`GetTrickOption` system is the model |
| D7 | **Informed reuse**: solver-inventory audit first; composition (alternating fixpoint, thin `src/common` coordinator, SharedItem-shaped boundary) is the default posture; code-level unification permitted on demonstrated maintainability grounds; the contract binds either way |
| D8 | Increment 1 (paired default Glitchless + attempt ladder + reachability-gated hosts + pinned-seed CI locks) and increment 2 (merged generation, FULL delivery, creation-event contract, `sRandoInitDone` split, fail-at-file-select) as specified; increment 2 precedes 3 |
| D9 | Beatable rungs respect **renewable-vs-consumable discipline** (#525/#540/#554/#555's shared-resource machinery; OoTMM's renewable/license triad as precedent); exhaustion is designed out, not counted |
| D10 | **License mechanics if porting OoTMM material**: MIT of record (root LICENSE, 1093 bytes verbatim MIT) despite the two `"license": "ISC"` package.json fields (near-certain scaffolding leftovers — discrepancy recorded here so nobody rediscovers it); ported files carry the copyright line; a repo `THIRD_PARTY_NOTICES` accompanies any port; an upstream issue asks OoTMM to fix the fields; algorithms *reimplemented from reading* are not a port, world-data YAML/CSV taken wholesale is |
| D11 | Scope exclusions for 3.2: **entrance randomization** and **networking/multiworld** are OUT; each exclusion becomes its own epic (below) |

### Open questions (for operator acceptance; none blocks increment 1)

Headed by the four the rulings left sharpest, then the remainder of the
corpus's list, updated for what is now settled. Settled and struck from the
corpus's ten: pool removal (→ D3), "what does MM-beatable mean" (→ D1/D2),
profile-freeze timing (one-game ruling), bidirectionality (a single bag is
inherently bidirectional), license mechanics (→ D10).

| # | Question | What it decides |
|---|---|---|
| O1 | **MM-beatability parameters at the combo surface**: which of MM's goal parameters are surfaced/validated per GOAL value; disposition of the dead `RO_ACCESS_MAJORA_REMAINS` row (`Options.cpp:36`, no consumer — implement or retire); whether `beat-either` permits a half whose own parameters make it unbeatable by choice | GOAL UI + validation at creation |
| O2 | **The crossing edge's requirement expression**, per direction, for Happy Mask Shop ↔ Clock Tower: time-slice re-stamp on entry to MM (OoTMM re-stamps Day 1 and ANDs `can_reset_time`; redship must pick its equivalent), return-trip requirement, interaction with arrival hydrate-or-refuse | The one edge both expansions share |
| O3 | **Attempt-ladder spec**: hash recipe and separator, max attempts, where the attempt index is recorded in save + spoiler, terminal-failure UX copy on the #533 surface | Determinism + failure surface |
| O4 | **The combo-fill implementation shape** (after the Decision 4 audit): composition's coordinator contract (each engine's exported query surface, snapshot/restore around MM's mutating queries, which TU owns the boundary under ADR 0002's one-sanctioned-TU rule) — or unification, if the audit makes that case | Increment 3's engineering core |
| O5 | **Canonical MM time-slice list**: redship has 45 (`Logic.h:20+`), OoTMM has 46 (missing `NIGHT2_AM_05_30`); adopt the u64 representation, add the slice or justify its absence, pin the list where both the crawl and any data port read it | Lattice height; digest stability |
| O6 | **Monotonicity enforcement mechanism** for lambda logic: review rule + grep/clang-tidy probe + a CI check that adds items and asserts the reachable set never shrinks — which combination, and where it runs | Soundness of assumed fill |
| O7 | **Boundary/constraint carrier scalars and carve budget**: exactly which game-neutral fields cross (SharedItem + host check id + earliness bound?), sized against `reserved[132]` under the append-only second-block rule and the 64-byte floor | `.redsave` format |
| O8 | **Shared-item classification authority**: one owner per shared item's junk/progression/renewable classification (OoTMM's `SHARED_BOMBCHU` two-settings wart is the cautionary tale) | Bag construction |
| O9 | **MM per-trick vocabulary**: does MM grow an `RT_*`-equivalent option table (the graph's TODO seams name the first candidates) before increment 3, or does its trick dimension ship RO_LOGIC-coarse at first and refine later | Trick axis symmetry |
| O10 | **Combo triforce-hunt accounting**: one shared piece count across both worlds (a shared-resource-style carrier) vs per-half counts ANDed/ORed; both engines' existing piece machinery is the substrate | GOAL value semantics |
| O11 | **Shipped default rung** of the logic ladder (`beatable(T = ∅)` is the conservative candidate; base `none` is the operator's named audience) — and whether the default GOAL stays `beat-both` | First-run experience |

## Non-goals → future epics

Excluded or deferred by this ADR. Per the operator's instruction, **each
line becomes its own epic issue** filed against the tracker after this ADR
is accepted (listed here; deliberately not filed by this ADR):

1. **Epic: cross-game entrance randomization.** Operator-excluded from 3.2.
   `src/common/entrance.h` was designed for generalization; the epic
   inherits O2's crossing-edge expression and D4's linked-fixpoint
   convention, and adds OoTMM's ER-fillability lesson (the `forwardFill`
   stress test exists because ER can be reachability-satisfiable but not
   incrementally fillable).
2. **Epic: networking / multiworld over the combo.** Operator-excluded from
   3.2. Builds on ADR 0006/0007 and #460's per-session identity handshake;
   a grant is acceptable only when sender identity == receiver creation
   identity (#564 V15).
3. **Epic: cross-game asset rendering.** **Permanently blocked** as things
   stand — 151 object-namespace collisions plus lazy archive mounting
   (`docs/resource-namespace-audit.md`); the shipped answer is native
   model-less presentation in both directions. The epic exists to hold the
   "unless the namespace is reworked" line so nobody rediscovers the
   blocker mid-increment; it is not scheduled work.
4. **Epic: hints v2 (cross-game hints).** MM area taxonomy (#500 work item
   4, including the dead `StaticData::RandoStaticRegion` decoy), foreign-host
   gossip-stone hints (today structurally impossible: the junk filter
   excludes every host), OoT-side cross-game hint type + serialization,
   check-tracker surfacing of foreign hosts, and — strictly on top of the
   deterministic core — OoTMM-style probabilistic foolish analysis as hint
   refinement.
5. **Epic: OoTMM world-data port.** The long-horizon unification target
   (Decision 4's "A only ever via the data port" corpus verdict survives as
   this epic): ~100 world files / ~1320 areas / ~6045 pool rows under D10's
   license mechanics. Only meaningful if the Decision 4 audit chooses
   unification, or if hand-maintaining two logic dialects proves the larger
   cost.

## Consequences

**Good:**

- The combo gets a real win condition with a real guarantee, parametrized
  the way both communities already think (goal + tricks + strictness), and
  the guarantee is constructive — the fill cannot terminate un-provable on
  a proved rung.
- Increment 1 is immediately startable, entirely inside MM's existing
  seams, and closes the one genuinely open beatability hole (the MM half)
  while the identity substrate (#570) finishes landing.
- The composition posture keeps both upstream diffs small and makes ADR
  0002 hold by construction; the audit requirement makes the alternative an
  informed choice rather than a rewrite instinct.
- Every failure path terminates on the already-landed #533/#568 REFUSED
  surface — no new failure UX is invented, and refusal no longer risks data
  loss (#569).

**Costs, accepted with eyes open:**

- **Freeze-at-creation UX**: after file-create, GOAL, tricks, logic rung and
  every goal parameter are read-only (ADR 0004 §6 state 4). The player who
  wants to "loosen logic mid-run" is refused; that is the one-game ruling's
  price, and the pane copy must say "already decided", not "not available".
- **Creation-time compute budget**: the whole proof (and the attempt
  ladder's retries) runs inside the creation event at file select. OoTMM
  ships thousands of whole-world re-solves as normal user options, so the
  fixpoint itself is cheap — but MM's 10s Glitchless wall-clock abort
  (`GlitchlessLogic.cpp:64`) is unmeasured under creation flow and must be
  measured, not assumed, before increment 2 ships. Re-roll time is a
  budgeted product cost.
- **Determinism re-pins, twice**: increment 2 re-pins the
  SeedDeterminism/MMRandoGen/HeadlessForeignDigest rows; increment 3 re-pins
  everything again when the fill unifies. Batched deliberately; never
  silently.
- **Increment 3's blast radius is the largest in the phase**: both fills'
  ordering, the spoiler identity (one artifact, both worlds — #564 V23),
  boundary carve bytes against a 132-byte budget with a 64-byte floor, and
  the CI tier cost (MM logic needs registrars + a live save; the `rando`
  label runs under a display and is skipped by `--test all`). Budget for
  slower feedback.
- **`beat-either` is exactly what it says**: the un-required half may be
  unfinishable. Documented at the setting, not discovered in a bug report.
- **Two logic dialects persist** under the composition default — the same
  reachability fact is written as a C++ lambda two different ways, forever,
  unless the audit chooses unification and pays its port cost instead.

**Risks:**

- MM's Glitchless solver mutates the live `gSaveContext`
  (`GlitchlessLogic.cpp:22/:258`); every coordinator round must run the
  memcpy swap discipline or corrupt the session. This is the single biggest
  structural obstacle the audit must size (and the strongest standing
  argument unification will make for itself).
- The vacuity trap: pair-level beatability locks written before items leave
  origin pools pass today and keep passing after a broken change. Locks
  land with removal (increment 3), never before.
- `RandoHintCrossGame` still sounds like cross-game hint plumbing and still
  is not (it is the #441 arrival-wipe regression lock); the dead
  `StaticData::RandoStaticRegion` still reads like the area taxonomy Lane D
  wants and still has zero references. Both traps are inherited by every
  epic above and are re-stated here because they will outlive this ADR's
  authors' attention.
