# Lane L — root LICENSE (MIT, if appropriate) and THIRD_PARTY_NOTICES

**Branch:** `claude/root-license-and-third-party-notices`

**Task:** No local build. Operator ruling: "add an MIT license if that's
appropriate" — the appropriateness finding comes first. Inventory every
vendored component (SoH, 2Ship, the three MIT submodules, every third-party
library actually vendored under `games/*/`, `src/`, `libultraship/extern`,
plus OoTMM as the planned MIT source of MM trick names for #578, and
mm-rando as an explicit GPL-3.0 NON-source) with path, license, copyright,
upstream URL, and whether the license text is in-tree. MIT for
RedShipBlueShip's own code is appropriate only if every vendored component is
permissive and nothing GPL/LGPL/AGPL is compiled in — if copyleft code is
found, stop and report `blocked` rather than adding a license. If clean: add
root `LICENSE` (MIT) and `THIRD_PARTY_NOTICES.md` (one section per
component), and point `docs/CREDITS.md` / the README at both. No per-file
headers on vendored code (that is lane A1's narrower per-file-attribution
task on OoTMM-derived names, not this lane's).

**Files owned:** `LICENSE`, `THIRD_PARTY_NOTICES.md`, `docs/CREDITS.md`, the
README's license line.

**Keywords:** `Refs #578`.
