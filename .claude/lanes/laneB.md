# Lane B — increment-3 prerequisites: #657, #667, #659, #656

**Branch:** `claude/inc3-prereqs-657-667-659-656`

**Task:** Four increment-3 prerequisites from the solver-inventory audit.
(1) **#657** — verify (likely already resolved by PR #680's
`rsbsPairingRequested` call in `playthrough.cpp`) whether
`Combo_ForeignPairingRequested()` is truly wired as the pre-`Fill()` gate;
close by comment if so, finish it if only partial. (2) **#667** — settle
whether `RSBS_COMBO_DIR_OFF` means "no paired world" (current code) or "a
paired world with zero crossings" (ADR 0011's gloss); either flip the code
and its test or amend the ADR gloss, and make every consumer consistent.
(3) **#659** — harden `GetRegionIdFromEntrance`'s first-call cache against
running before every region registrar has completed; probe the real order,
fix at the root. (4) **#656** — gate the reverse (MM-items-in-OoT-checks)
placement pass on reachability, symmetric to the forward pass's
`ComputeReachableCheckSet` gate; lock non-vacuously even if the shipped
default makes it vacuous. A digest re-pin is allowed only if #656's gate
moves placements under the shipped default.

**Files owned:** `src/common/foreign_items.c/.h`,
`src/common/tests/test_combo_settings.c`,
`games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp`,
`games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp` (gate site
only), `games/mm/2s2h/Rando/Logic/Logic.cpp` (entrance cache only),
`games/mm/2s2h/Rando/Foreign.cpp` only if #667 forces a consumer change,
`docs/adr/0011-*.md` (dated amendment only if #667 goes that way).

**Keywords:** `Fixes #659 #656`, plus `Fixes #657` / `Fixes #667` for each
actually resolved in code (or "closed by comment" for a stale premise).
`Refs #645 #644`.
