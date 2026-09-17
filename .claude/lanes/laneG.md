# Lane G — #682: host MM enhancement toggles in the unified menu

**Branch:** `claude/682-mm-enhancement-toggles` (starts only after lane D merges)

**Task:** Host a curated allowlist of four MM enhancement CVars —
`Kaleido.GameOver` (#653), `BetterSongOfDoubleTime`, `SkipSoTCutscenes`,
`Autosave` — in lane D's tier-4 Combo section. For each, verify all three of
ADR 0004 §5's liveness legs (TU links, registrar runs, dispatch point
exists); carve out the TU the #679 `WHOLE_ARCHIVE` way if it is elided but
small and safely guardable, otherwise draw the row disabled-with-reason. Lock
that each LIVE toggle's TU links and its registrar ran. No digest re-pin
expected (enhancement toggles are preference-class, not identity). Do not
widen the allowlist or flip `2ship_enh` to a general `WHOLE_ARCHIVE`.

**Files owned:** `games/mm/CMakeLists.txt` (carve-outs), provider TUs under
`games/mm/2s2h/Enhancements/` (guards only),
`.github/scripts/check-registrar-elision.sh`,
`games/mm/2s2h/mm_registrar_coverage_test.cpp`, lane D's MM sub-section file
or extension point, `src/common/cvar_shared_keys.h` (append), new tests.

**Keywords:** `Fixes #682`, `Refs #497 #679 #673 #427`.
