# The combo-logic coordinator: what increment 3's core decided

Companion to `docs/solver-inventory.md` (the O4 audit) and ADR 0010 (whose
2026-09-20 amendment records the operator's COMPOSITION ruling). This note is
the decision record for the coordinator core itself — `src/common/combo_logic.h`
/ `.c` and `src/common/tests/test_combo_logic.c` — so that the two follow-on
lanes implementing the real engines, and anyone reading the header later, can
tell a decision from an accident.

The header carries the *contract*. This note carries the *choices*, the three
places the audit's §4.1 surface turned out to be incomplete, and the list of
what is deliberately not here.

## 1. The engine surface is a registered vtable, not fixed `extern "C"` symbols

The audit sketched the surface as two sets of fixed C symbols
(`OoT_Logic_Expand`, `MM_Logic_Expand`, …). The shipped surface is a
`ComboLogicEngine` vtable registered per `GameId`, and the substitution is the
whole reason:

- A ROM-free, display-free test must be able to drive the coordinator over
  engines it authors. Fixed symbols cannot be substituted, because the real
  engines' TUs link into the *same* binary as the tests (single executable), so a
  test providing its own `OoT_Logic_Expand` is a duplicate symbol.
- The alternative — a test that drove the *real* engines — needs both ROMs, both
  graphs built, a display for MM's crawl (audit §6.3 records that a windowless
  MM crawl is not known to work), and a generated world. That dependency chain
  is exactly what has kept every pair-level beatability assertion in this
  project vacuous, and ADR 0010's increment-3 text is explicit that the
  pair-level lock must come **with removal** or it is theatre. A removal lock
  needs to author two worlds that differ by one host, which is cheap over stubs
  and expensive-to-impossible over real generation.
- Two smaller gains: a build with only one engine's TU linked resolves cleanly
  (the missing origin simply has no engine, the property
  `Combo_RegisterForeignItemPool` already exists for), and MM's snapshot/restore
  pair is genuinely *optional*, which a fixed-symbol surface can only express as
  a stub that lies.

Cost: one indirect call per query, against a query that runs a whole
reachability search. Not measurable.

Registration refuses, rather than accepting and crashing later: a `GameId` that
is not a real game, an `abiVersion` mismatch, any NULL required entry point, and
a `snapshot` without its `restore` (or the reverse). `NULL` un-registers, so a
test restores the registry instead of leaving process-global state behind.

## 2. The bracket semantics

`src/common/combo_logic.h` is the NORMATIVE statement of the call order and of
the ownership rule below — an engine implementation reads the header, not this
note. What follows is the same rule with the reasons attached, and it is kept in
step with the header on purpose: increment 3 shipped a weaker version here than
in the header and the follow-up (#717) corrected it.

Per round, for both sides, in this order:

```
if (S.snapshot) S.snapshot()      // BEFORE beginQuery
S.beginQuery()
... assume the set, alternate to a fixpoint, read every fact ...
if (S snapshotted OK) { S.restore(); re-apply all of P[S] through S.place() }
if (S.beginQuery was CALLED)  S.endQuery()
```

- **Snapshot before `beginQuery`**, because the restore has to undo whatever
  `beginQuery` did to the live save as well.
- **One pair brackets the whole round**, not each call — audit amendment 1, and
  the reason is that every MM query writes the live save, the "read-only" crawl
  included.
- **The re-apply runs only for a side that restored.** A side with no snapshot
  lost nothing, and the re-apply is one `place` per placement per round while the
  fill runs a round per bag item, so the distinction is a real cost, not
  tidiness.
- **It re-applies the whole table for that side**, not only what the round
  placed. In the fill's own shape those are the same set (the fill places
  *between* rounds, never inside the bracket); re-applying everything is the
  reading that stays correct if a later consumer does place inside one, and it
  makes the invariant statable without reference to timing: *after every round, a
  restoring engine's state contains every placement the coordinator has decided.*
  That is why `place` must be idempotent for the same `(host, item)`.
- **`endQuery` is last**, after the restore and the re-apply, because it is where
  OoT re-points `Logic::mSaveContext` at the live save (audit §4.4/§1.8) — on a
  round that opened, the engine must still be in query shape while the re-apply
  runs.

### The ownership rule, when a round is abandoned part-opened

A round can fail while the two sides are still being opened: `snapshot` refuses,
or `beginQuery` refuses. The teardown then runs over a bracket that is only half
there, and exactly one rule decides each half. **It is per CALL, not per
success**, and both gates are the ones in the pseudo-code above:

| half | called iff | why not the other reading |
|---|---|---|
| `restore` | `snapshot` was called **and returned nonzero** | Gating on the mere presence of the `restore` pointer would restore out of a blob that was never captured, writing stale bytes over live state. |
| `endQuery` | `beginQuery` **was called at all**, whatever it returned | The engine's detach lives in `beginQuery` and its only documented inverse lives in `endQuery` (OoT's `Logic::mSaveContext`). A `beginQuery` that detaches and then refuses would otherwise leave the engine coupled to a simulated save for the rest of the process — audit §1.8's hazard, reached through the contract's own refusal path. That was increment 3's behaviour. |

A side whose `snapshot` refused is therefore **never asked to begin**, and gets
neither `endQuery` nor `restore`. Sides opened *before* the refusing one are torn
down in full.

**What an engine must tolerate**, which is the part an engine author needs and the
reason this is a contract rather than a behaviour — two different teardowns,
depending on whether the side snapshotted:

- *No snapshot pair*: one `endQuery` on a round that never opened. A no-op there,
  not an unwind of state the engine never set.
- *A snapshot pair whose `snapshot` succeeded before `beginQuery` refused*:
  `restore`, **then a full re-apply of that side's whole placement table through
  `place`**, then `endQuery` — all three on an engine whose `beginQuery` returned
  zero. So `restore` and `place` must both work outside an open query bracket, and
  `place` must still be idempotent there. MM is the side that snapshots, so MM is
  the side that meets this.

Both configurations are locked in `test_combo_logic.c`'s
`ComboLogicContractEdges` row (scenarios 3 and 4).

## 3. Four things the audit's surface did not have

These are the decisions a reviewer should look at hardest, because each one is
an addition to §4.1 rather than an implementation of it.

### 3.1 The arrival gate

MM's region graph is rooted unconditionally in its own dialect (audit §4.5:
`CONNECTION(RR_MAX, true)` on South Clock Town makes the cycle reset free), so
MM's engine cannot know that Termina is *entered* through OoT's Happy Mask Shop.
The coordinator therefore applies the gate the engine cannot: **while
`OoT.crossingOpen()` is false, MM's hosts are not candidates and MM's
`goalReached()` does not count.**

Without it, a world whose crossing never opens would host progression in a
Termina the player can never enter, and `beat-either` would report that half as
proved. In practice OoT's crossing opens at sphere zero under default settings —
which is precisely why this has to be a rule and not an observation: the one
configuration where it matters is the one nobody tests by accident.

The exchange gates are the same reasoning stated as travel:

| Item | Host | Reaches its own engine when |
|---|---|---|
| MM-origin | an OoT check | that check is reached **and** OoT's crossing is open (the player must be able to get to MM to spend it) |
| OoT-origin | an MM check | that check is reached **and** OoT's crossing is open (to get there) **and** MM's crossing is open (to get back) |

### 3.2 `clearPlacements` is part of the surface

The fill's dead-end recovery is OoT's own discipline: roll the whole batch back
and retry with a different deterministic order. A roll-back that cleared only the
*coordinator's* tables would leave the previous attempt's placements inside both
engines; the next attempt would see those hosts as assigned, run out of
candidates, and report a dead end that belongs to the retry rather than to the
world. One bad shuffle would become a permanently failing fill, and the symptom
would be a plausible-looking "this seed cannot be filled".

So `Combo_Logic_ResetPlacements()` calls each registered engine's
`clearPlacements`, and that call is required, not optional. A test row asserts
that a world which fails ten times fails ten times *for the same reason* rather
than dead-ending on its own leftovers.

### 3.3 The `all-reachable` rung's obligation is over placed hosts

ADR 0010 §1.2 keeps "every location reachable" as each half's own existing
strictness axis (`RSK_ALL_LOCATIONS_REACHABLE`, MM's analogue), composing with
any GOAL. The pair-level obligation the combo rung adds on top is therefore
narrower and precise: **every host the single-bag fill placed on must be reached
in the final round.** Nothing the bag distributed may land where the player
cannot stand.

Widening it to every shuffled host in both worlds would duplicate a per-half
setting that already exists. (`allEmptyHosts`, added in §3.4 below, could now
enumerate the wider set, so the narrow reading is a decision rather than a limit
of the surface.)

### 3.4 `allEmptyHosts` is part of the surface, and the `none` rung is why

Audit §4.1's surface has one host enumerator, `ReachedEmptyHosts`. Audit §4.3
then says that under `RSBS_COMBO_RUNG_NONE` "the round is skipped and hosts are
drawn from all empties (the operator's *bag → randomly distribute* base mode as
the same code path, D5)" — which the single enumerator cannot express. So the
vtable carries a second, required call: **every host the engine does not already
hold, reached or not, callable outside any query bracket.**

Without it, `none` would be reachability-gated, and that is not a cosmetic
difference:

- It could **refuse a world it must accept.** With no reached, unassigned host on
  either side the fill dead-ends and the attempt ladder exhausts into
  `ERR_NO_CANDIDATE` — but "randomly distribute to each check" cannot dead-end
  while any check is free. A rung defined by the absence of a logic obligation
  would be failing for a logic reason.
- It would **pay for the round it discards.** A reachability round per bag item
  is the expensive half of the fill, against the #582 creation budget, computing
  a filter this rung is required not to apply.

The arrival gate (§3.1) is likewise not applied under `none`: it is a
reachability rule, and a rung that asserts nothing about reachability must not
gate on one — doing so would quietly empty half the world under the one rung that
promises to fill all of it.

A test row pins both sides over the one world where the two host sources
disagree (its only free host is unreached and never offered): `none` places
there, and `beatable` on the identical world refuses with `ERR_NO_CANDIDATE`.

## 4. The fill

One bag, both games' hosts, OoT's assumed fill lifted one level (audit §4.3):
shuffle with the private RNG; for each item run a round with every *other*
unplaced bag item assumed; take the union of both sides' candidate hosts; pick
one uniformly; place it on its owning side. Empty candidate set → roll the batch
back and re-shuffle.

Three points worth stating because a reviewer could reasonably expect otherwise:

- **One uniform draw over the union**, never "pick a side, then a host". Weighting
  by side would bias toward the smaller game, and under `beat-either` a side
  preference *is* the XOR bias ADR 0010 §1.2 forbids.
- **The GOAL check is the loop's exit condition**, evaluated by a final round with
  nothing assumed — never a check bolted on after the fill. `beatable` requires
  the GOAL expression; `all-reachable` requires it plus §3.3; the two differ in
  that block and nowhere else.
- **`none` is the same distribution over a different host source**, not the same
  path with the proof switched off. It runs **no round at all** and draws from
  `allEmptyHosts` (§3.4), so `rounds` is 0 and there is exactly one attempt —
  nothing there can dead-end for a logic reason, so no re-shuffle could help. It
  reports `proofSkipped`, so "not proven" cannot be misread as "proven". What the
  three rungs genuinely share is the bag, the private RNG, the uniform union
  draw and the two tables: one function, `ComboLogicDrawAndPlace`, does the
  drawing and placing for all of them, so they cannot drift apart on the half
  that decides the distribution.
- **A refusal taken before any attempt describes the call, not the tables.** A
  bad request, an unpinned rung, an unsupported GOAL or a missing engine returns
  with `attempts == 0`, `placed == 0` and `placementDigest == 0`, having called
  no engine and touched no table. Reading the tables there would report whatever
  an earlier fill left in them dressed up as a property of the refusal — and a
  lock on "a refused fill places nothing" would then be measuring the fixture.
- **The RNG is private and injected.** `seed` comes from the frozen identity; a
  query must consume no game RNG, because a query runs a variable number of times
  per fill and a coordinator drawing from a game's stream would make the world a
  function of the search's shape rather than of the identity.

### 4.1 Three capacities, three different quantities

The header sizes three caps separately, and conflating any two of them is a real
defect rather than a tidiness question (increment 3 conflated the first and the
third; #717 split them):

| cap | bounds | today | sized against |
|---|---|---|---|
| `RSBS_COMBO_LOGIC_BAG_CAP` (512) | bag items in one fill | low hundreds | both games' last general advancement pass (audit amendment 2) |
| `RSBS_COMBO_LOGIC_PLACEMENT_CAP` (1024) | placements on **one host game** | ≤ the bag | the bag — it bounds what one side can *receive* |
| `RSBS_COMBO_LOGIC_HOST_CAP` (4096) | hosts one engine may **offer in one enumeration** (`reachedEmptyHosts` / `allEmptyHosts`) | OoT 2528, MM 2258 | **a game's check id-space**, not the bag |

The third is the one the collector's scratch buffer must be sized by. An engine
answering "every shuffled check I do not consider assigned" hands back its whole
pool, so a scratch sized by the placement cap would have met `ERR_CAPACITY` on the
**first bag item of the first fill** — and reported "this seed cannot be filled"
for a reason with nothing to do with the world. A `#error` in the header keeps the
host cap at least the placement cap, because every placed host was first an
offered host.

Above the cap the behaviour is to **refuse, never truncate**: a truncated
candidate list silently narrows the world to a prefix of one engine's table, and
no determinism row could see that. The refusal names the constant to raise.

### 4.2 `Combo_Logic_RunFill` resets both placement tables at every attempt

Every attempt — on both fill paths, the `none` rung included — begins with
`Combo_Logic_ResetPlacements()`, which clears both coordinator tables *and* calls
each registered engine's `clearPlacements`. So both tables and both engines are
empty before the first item is placed.

Stated here because `Combo_Logic_Place` invites a spoiler-load caller to rebuild
both tables from a spoiler's foreign section, and `RunFill`'s post-conditions read
as "the fill only ever adds". **There is no keep-existing-placements mode**, and a
fill is not a continuation of an authored partial world: anything authored before
a fill is discarded, not extended. Adding such a mode would change
`ComboLogicFillRequest`, which increment 3 and its follow-up deliberately did not
do.

`RSBS_COMBO_GOAL_TRIFORCE_HUNT` **refuses** with
`RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL`. Answer O10 rules one shared piece count
across both worlds, that carrier does not exist, and per-half composition is what
O10 rejected — so evaluating it as anything would ship a world whose stated goal
is not the goal that was proved.

## 5. Termination, and the two watchdogs

The argument is written at the loop: each engine's expansion is monotone on its
own finite join-semilattice, the exchanged facts are monotone (nothing in a round
is withdrawn — `assumeOwnItem` only adds, `place` grants nothing, no placement is
made inside the bracket), so the composite is monotone on a finite product
lattice and reaches a least fixed point.

The iteration bound is **not** that argument; it is a watchdog on its *premise*.
Two detectors, because they catch different lies:

| Lie | Detector | Status |
|---|---|---|
| a monotone observable falls (candidate count, crossing flag) | compared every iteration | `ERR_NON_MONOTONE`, at the iteration it happens |
| `expand` reports change forever with nothing growing | the bound | `ERR_NO_FIXPOINT`, at `RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS` |

Neither is worked around. Assumed fill and each game's trailing junk `FastFill`
are unsound without monotonicity (ADR 0010 §2.3), so a violated premise fails the
fill rather than producing a world nobody can characterise. This is the runtime
half of answer O6's three mechanisms; the static probe and the CI grow-check over
each port's lambda dialect remain owed.

## 6. Deliberately absent

Not oversights. Each has an owner.

| Absent | Owner |
|---|---|
| Any production wiring — no shipping TU registers an engine, no creation seam calls the fill | the two engine lanes, then a wiring lane |
| The O7 boundary carve; the placement tables are RAM, not `gComboCtx.reserved[108]` | epic #645 item 2 (a carve taken here would freeze a layout before the decision that sizes it) |
| The O8 single-owner classification table; `ComboLogicBagItem.itemClass` is carried from the caller and recorded on the placement, and the coordinator filters nothing by it | epic #645 item 5. Narrowing belongs to the pool DRAW (`Combo_ForeignPoolDrawFor`), which runs before the bag exists: the six membership criteria run FIRST and the bitset selects among survivors (ADR 0011 decision 3.1) — an ordering this file may not weaken |
| The O10 shared triforce piece count | epic #645 item 5; until then triforce-hunt refuses |
| One spoiler artifact for the pair | #564 V23 / audit P11 |
| The attempt ladder (re-rolling the seed) | the layer above; the coordinator's retries are batch roll-backs within one seed |
| OoT's restricted-pool passes and both games' junk `FastFill`s | per-game, audit amendment 2 |
| The crossing edge's Day-1 re-stamp and reset-time guard (answer O2) | authored inside the engines behind `crossingOpen`; the coordinator reads the resulting region fact |
| The two §6.3 measurements — one linked round's cost against the #582 budget, and whether an assumed fill over MM's forward-authored graph converges | epic #645's first two work items. `Combo_Logic_RunRound` is public partly so they can be made |

## 7. What the locks prove, and what they do not

Four `redship` rows, all over two synthetic bitmask stub engines.

Proved: the registration refusals; the GOAL truth table including
`beat-either`'s OR never narrowing to an XOR and triforce-hunt refusing; that a
paired fill with one engine refuses instead of half-filling; that a refusal taken
before any attempt reports on the call and leaves full tables untouched;
**the coordinator's half of ADR 0002 routing** — the `SharedItem` reaching
`place` still carries its foreign origin (the coordinator routes by origin and
never rewrites it) and the id never reaches its own engine's `assumeOwnItem`;
both premise watchdogs, bounded; one round terminating in a handful of
alternations and being independent of the order facts arrived in; crossing
exchange in **both** directions inside one world; the arrival gate; **the
pair-level goal with removal** (an OoT-origin item hosted only in the MM stub
makes `beat-both` provable; deleting that one host flips it unprovable, and the
same world under `beat-either` is a legitimate world with one unbeatable half);
the re-apply invariant against a restore that returns the engine's table empty;
that an engine refusing a host it had itself offered aborts the fill and leaves
no row in the coordinator's table claiming that host; same-seed determinism
**with** a different-seed sensitivity control; **what `none` means** — on the one
world where the two host sources disagree it places into the unreached host while
`beatable` refuses the same world with `ERR_NO_CANDIDATE`, and it opens no query
bracket on either engine; `beat-either` placing byte-identically to `beat-both`
where both halves prove, and not starving the permitted unbeatable half's checks;
and `all-reachable` refusing a world `beatable` accepts.

The fourth row (`ComboLogicContractEdges`, #717) is the contract's EDGES, which
the first three could not reach because their worlds are six hosts per side and
their stubs never refuse: an engine offering 2300 hosts is accepted on both the
`none` and a proving rung and one past `RSBS_COMBO_LOGIC_HOST_CAP` is refused
rather than truncated; **both** part-opened teardowns of §2's ownership rule — a
side that refused `beginQuery` without a snapshot pair gets `endQuery` and no
`restore`, and one that refused it *after* a successful `snapshot` gets `restore`,
a full re-apply of its whole table, and `endQuery`, with a harsh restore proving
the re-apply actually ran; a refused `snapshot` getting neither; and `RunFill`
discarding an authored partial world from both tables *and* both engines (§4.2).

**Three** of that row's six scenarios have **no red half on `origin/main`**, and
they are locked as contract rather than as regressions: the enumeration cap's
*refusal* (increment 3 refused above its own smaller cap too, so it never
truncated either — only a hypothetical truncating coordinator makes that half
red), the refused `snapshot` (main already gated `restore` on the snapshot's
return), and `RunFill`'s reset (already true, just undocumented). The three that do
have one are the cap's *acceptance* and the two failed-bracket teardowns.

Asserted of the stub rather than of the coordinator, and therefore **not** proved
here: the junk cover. `ClPlace` makes a foreign item into a local junk item
because that is what a real engine must do; no change to `combo_logic.c` can flip
that assertion, and substituting the cover is the two engine lanes' obligation.
It is asserted anyway, visibly, so that the stub's modelling of the contract is
on the page instead of implied.

Not proved, and not claimed: anything about either real engine. The stubs answer
the coordinator's questions correctly by construction; whether OoT's
`Logic::Reset(true)` + `ReachabilitySearch` and MM's snapshot/give/crawl actually
satisfy this contract is each engine lane's own lock to write, and the header is
written so that those lanes cannot guess wrong about what is being asked of them.
No generated world is touched by any of this, because nothing is wired.
