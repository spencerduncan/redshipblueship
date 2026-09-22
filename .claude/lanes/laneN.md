# Lane N — license follow-up: attribution name, remove Fipps, ship required texts, elect CC0/MIT wherever offered

**Branch:** `claude/license-elections-and-fipps-removal`

**Task:** Read, on `origin/main`: `LICENSE`, `THIRD_PARTY_NOTICES.md` in full
(especially "Summary", "Fonts", "Unresolved license status", "Open items for
the operator"), `docs/CREDITS.md`, and the README's license line. Operator
rulings (2026-09-21, verbatim): use "Penelope Duncan, or just leave my name
off of it all together"; ask where Fipps is even coming from; "use cc0/MIT
wherever there's an option to do that". Implement: use **Penelope Duncan**;
**remove Fipps** (vendored, unused by default, the only "All rights
reserved" asset — both `.otf` copies and both `LoadFont("Fipps", ...)`
sites in `OTRGlobals.cpp` and `BenPort.cpp`, plus a fallback so a saved
`gOverlayFont` naming an unloaded font resolves to "Press Start 2P" before
`SetCurrentFont`); ship the OFL 1.1 text beside both games' custom fonts and
2Ship2Harkinian's CC0-1.0 `LICENSE` verbatim into `games/mm/LICENSE`; elect
MIT/CC0 for every component whose upstream genuinely offers that option,
recorded in a new "Elections" table with evidence (component, options
offered, election, the upstream file + sentence); update "Unresolved license
status" (Fipps resolved by removal; Ship of Harkinian stays unresolved — no
election possible, no license published upstream at all); one line in
`docs/known-issues.md` about the Fipps overlay-font choice falling back.
**No upstream reports** — read-only fetches from upstream are fine, filing
or proposing anything to them is not.

**Files owned:** `LICENSE`, `THIRD_PARTY_NOTICES.md`, `docs/CREDITS.md`,
`docs/known-issues.md`, the two `LoadFont("Fipps", ...)` call sites, both
games' `assets/custom/fonts/` directories, `games/mm/LICENSE` (new),
shared append-only files. Not `src/common/` beyond the three shared test
files, not rando/logic code, not the libultraship/ZAPDTR/OTRExporter
submodules.

**Keywords:** `Refs #578`.
