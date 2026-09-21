# Lane D — #497: the SohMenu remainder of ADR 0004

**Branch:** `claude/497-sohmenu-remainder`

**Task:** Operator ruling "go ahead" on all of #497's remaining scope: step 3
(capability-gating infrastructure so a row can declare a needed capability
and draw disabled-with-reason when absent), step 5 (the shared-intent marker
and ADR 0004 §6's four presentation states: live / frozen-read-only /
partial-with-reason / not-available), and step 6 (a real tier-4 Combo section
hosting the Cross-Game rows, moved off their interim host in
`SohMenuRandomizer.cpp`, with `combo_settings_view.*` as its content source
and a documented extension point for lane G's MM enhancement rows). Also lock
the SetMenu-count invariant #497 names, and record the go-ahead as a dated
ADR 0004 amendment.

**Files owned:** `games/oot/soh/SohGui/SohMenu.cpp/.h`,
`SohMenuRandomizer.cpp` (interim rows only), new `SohMenuCombo.cpp`,
`src/common/combo_settings_view.*` (function granularity, only if a new
accessor is needed — state it), `docs/adr/0004-*.md` (amendment), new tests.

**Keywords:** `Fixes #497` only if all three steps and the invariant landed;
otherwise `Refs #497` with a precise list of what remains. `Refs #682 #438`.
