# Lane K5 (wave 5) — ADR 0010 O8: the single-owner item classification table

**Branch:** `claude/inc3-o8-classification-table`

**Task:** Build ADR 0010 accepted answer O8: one classification per
`SharedItem`, owned by the sanctioned ADR 0002 TU pair
`src/common/shared_items.{h,c}` (which includes no game header), with each
game as the single SOURCE of its own rows through a game-neutral export
consumed at registration. Classes: progression / junk / renewable per O8,
plus **trap** (operator note 2026-09-26: OoT `RG_ICE_TRAP` and MM `RI_TRAP`
are real filler items with per-game disguise machinery) — justify the
fourth class by showing junk and trap need different bag handling (a trap
never crosses this increment). Cover every item either fill can place, not
only the pinned pools; progression per each fill's own advancement
predicate; settings-conditional cases (keysanity off, ADR 0011 criterion 3)
exposed as a predicate over the frozen record, not baked into the static
table. Verify first that `git grep -in classif src/common/shared_items.*`
is empty. Locks (redship tier), each with its red half observed: every
placeable item of both games has exactly one class; traps are never
progression; per-game sources agree with the owner table; a synthetic
double registration is refused. Do not wire the table into the bag (a
later lane, after K4 and K5 merge). Golden rows green, golden files
untouched. Builds; both tiers.

**Files owned:** `src/common/shared_items.{h,c}`; a NEW `classify` export in
each engine TU (function granularity — K4 edits assume/place/pool in the
same TUs); `src/common/tests/test_shared_items*.c`; the shared append-only
files. NOT `combo_logic.*`.

**Keywords:** `Refs #645, refs #500`.
