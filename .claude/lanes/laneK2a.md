# Lane K2a — increment 3: OoT's solver export for the combo-logic coordinator

**Branch:** `claude/inc3-oot-logic-export`

**Task:** Implement lane K1's merged `src/common/combo_logic.h` contract
over OoT's real solver. **Nothing you write may run in a production
path** — you register an engine, nobody calls the coordinator yet. Prove no
generated world changed with a real comparison against a main binary. New TU
beside `ForeignItemsSingleExe.cpp` (guard `RSBS_SINGLE_EXECUTABLE`).
Begin-query = the detach (`Logic::Reset(true)`, access reset, starting
inventory) — must genuinely reset the `logic` singleton's simulated
inventory (a bare `ReachabilitySearch` inherits residue from the previous
search, per the w4 preamble). Assume-own-item = `Item::ApplyEffect` into the
detached save. Expand = one `ReachabilitySearch` over all locations,
returning whether anything new opened. Crossing-open = `RR_MARKET_MASK_SHOP`
child access (the pair is pinned out of the entrance shuffle, PR #691).
Goal-reached = `CheckBeatable`'s condition. Place = `PlaceItemInLocation` for
OoT-origin, junk cover + table entry for MM-origin. Enforce the audit's
re-attach rule (§4.4): after a query, `Logic::mSaveContext` points where it
did before. Register at init. Read the MERGED lane K1 PR and
`docs/solver-inventory.md` §1, §4.1, §4.2, §4.4, §4.5 and ADR 0002 first; if
K1's PR is not on `origin/main`, STOP and report `blocked`.

**Locks (rando tier, real graph):** reachability monotone under assume;
begin/end leaves `gSaveContext`, `Context`, and the `logic` singleton
byte-for-byte where they were; two identical queries agree, and a third run
after an unrelated search still agrees (the residue bug's direct lock);
crossing-open false for an adult-only start, true for the default child
start.

**Files owned:** the new TU, its test, `games/oot/CMakeLists.txt` source
list if needed, shared append-only files.

**Keywords:** `Refs #645, refs #500`.
