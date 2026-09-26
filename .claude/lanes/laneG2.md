# Lane G2 (wave 5) — #693: host MM's autosave interval

**Branch:** `claude/693-autosave-interval-host`

**Task:** Add MM's autosave interval (`gEnhancements.Saving.AutosaveInterval`,
read by `SavingEnhancements.cpp`, 5-minute default) as a row on the
Combo → MM Enhancements page PR #695 built, with the three-part liveness
evidence that page's manifest requires; bump `kHostedMmEnhancementCount`
(static_asserted at 4) and its lock. OoT hardcodes its own interval and the
key is in `RSBS::kMustStayDistinct`: keep the two intervals distinct and
say why they must not converge. Lock the row's state. Golden rows green,
golden files untouched. Builds; both tiers.

**Files owned:** the MM Enhancements page
(`games/oot/soh/SohGui/SohMenuComboMmEnhancements.cpp`), its manifest in
`src/common/cvar_shared_keys.h`, the enhancement-toggle locks, the shared
append-only files.

**Keywords:** `Fixes #693`, `Refs #682`.
