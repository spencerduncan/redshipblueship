# Lane A1 — #578 part 1: MM per-trick vocabulary substrate

**Branch:** `claude/578-mm-tricks-part1`

**Task:** Build the substrate for MM's per-trick vocabulary (ADR 0010 O9):
an `MMRT_*` enum plus a SoH-shaped row table (key, display name, tooltip,
area, tag set, `reserved` flag) for OoTMM's 84 MM trick keys (~62 live, 22
reserved-and-inert naming an OoT-side item, plus a new
`MM_GBT_BOSS_KEY_ICE`). Store trick enablement as frozen, per-file rando
state; expose one `MM_TRICK(...)` predicate for region conditions. Fold the
trick set into `ProfileIdentityString` so `mmProfileDigest` covers it. Gate
the Powder Keg disjunct (finding a) and the Great Bay Temple boss-key edge
(finding b, `MM_GBT_BOSS_KEY_ICE`, default off) behind the new predicate.
Add a Tricks section to the MM options pane. A digest re-pin is expected and
allowed here (gating the keg changes reachability) — own commit, before/after
digests, count of checks affected.

**Files owned:** `games/mm/2s2h/Rando/Logic/Logic.h` (`CAN_USE_EXPLOSIVE`
macro only), `games/mm/2s2h/Rando/Logic/Regions/GreatBayTemple.cpp` (line 120
only), new `games/mm/2s2h/Rando/StaticData/Tricks.h/.cpp`, the MM rando
save-options storage TU, `games/mm/2s2h/Rando/Foreign.cpp`
(`ProfileIdentityString` only), `games/mm/2s2h/Rando/OptionsUiSingleExe.cpp`
(new section), `games/mm/2s2h/mm_rando_options_test.cpp`, new tests.

**Keywords:** `Refs #578 #570 #500 #645` (does not close #578; part 1 of 3).
