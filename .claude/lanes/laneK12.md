# Lane K12 (wave 6): one shared triforce piece count across both worlds (ADR 0010 O10)

**Branch:** `claude/inc3-o10-shared-triforce-count`

## Task

**One shared count.** Keep one combo-level triforce piece count as a
MONOTONIC shared resource:

- Collecting a piece in either game increments it.
- Once the files are paired, neither game's own counter is authoritative.
- The substrate is each port's existing piece machinery: OoT
  `RSK_TRIFORCE_HUNT_PIECES_TOTAL` / `_REQUIRED` in `randomizerTypes.h`, and MM
  `RO_TRIFORCE_PIECES_*` in `Types.h`.
- The shared-resource tiers are in `src/common/shared_resources.*`. Mind the
  discipline pin: only lower-harvest-after-full-apply distinguishes a
  MONOTONIC resource from a CONSUMABLE one.

**Frozen at creation.** `required` and `total` are frozen at the combo level,
and so is the split of pieces between the two pools. State the rule you use.
Under a paired triforce hunt, each game's own piece settings are inputs, not
truths.

ADR 0011's 12-byte record is append-only. A new value needs a dated amendment
and a format note.

**Win trigger.** It fires in whichever game reaches `required`. Reuse each
port's existing way of ending a hunt.

**Coordinator.** `goalReached` for `RSBS_COMBO_GOAL_TRIFORCE_HUNT` reads the
shared count through an engine-neutral query, not a game id.

## Locks

- **Cross-game sum:** collect k pieces in OoT and m in MM; after a switch, both
  games read k+m.
- **The freeze:** a divergent required/total is refused.
- **The win trigger** fires in each game.
- **The goal predicate** is checked over stub engines.

## Goldens

Untouched, unless the shipped default is a triforce hunt. It is not; verify
that.

## Scope

**Builds:** yes; run both tiers.

**Files owned:**

- the triforce carrier in `src/common/shared_resources.*`;
- a new triforce function in each engine TU, at function granularity (K9 owns
  bag and classify, K10 owns persistence);
- each port's hunt-win seam;
- the goal predicate;
- tests;
- the shared append-only files.

**PR:** "One shared triforce piece count across both worlds (ADR 0010 O10)".

**Keywords:** `Refs #645, refs #500`.
