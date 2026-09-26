# Lane K9 (wave 6) — compose the bag from the O8 table (progression + surplus only), raise the caps, re-measure

**Branch:** `claude/inc3-bag-composition-o8`

**Task:** Five parts. Parts 1-2 build the bag and settle two classification
questions; part 3 raises the caps; part 4 re-measures; part 5 is the golden
check.

(1) Build the bag from the O8 table, with a coordinator-side builder that each
engine feeds through its pool export:
- every copy of a `progression`-class item from both games' pools enters as a
  required row;
- copies beyond what the frozen settings' non-plentiful pool would hold enter
  as `RSBS_COMBO_BAG_SURPLUS` rows. Derive them from how each port's plentiful
  setting adds copies (OoT `item_pool.cpp` `RO_ITEM_POOL_PLENTIFUL`, MM
  `RO_PLENTIFUL_ITEMS`), not from a guess;
- `renewable`, `junk` and `trap` rows never enter the bag; they are counted per
  game for the leftover pass;
- OoT's restricted-pass items (dungeon items, songs, shops, rewards under their
  own settings) stay out, as audit §4.6 reads P14, with the setting-conditional
  cases taken from the frozen record.

State the rule table in the header.

(2) Settle #731 and #733:
- #731: one class per cross-game shared quantity (bombchus, double defense,
  hearts). The shared-resource tiers already treat each as ONE quantity, so
  the two sources must not disagree.
- #733: MM heart pieces gate two checks through `CHECK_MAX_HP(4)`. Either make
  them progression or model max-HP as a derived quantity. Decide with evidence
  and lock it.

(3) Raise the caps (#727): `RSBS_COMBO_LOGIC_BAG_CAP` and
`RSBS_COMBO_LOGIC_PLACEMENT_CAP` go to the measured worst case (both games'
progression plus surplus under plentiful), with headroom. Restate the
contract-edge leg, keep the static asserts coherent, and state the memory
cost.

(4) Re-measure on the shipped profile and on a plentiful profile for each
game:
- `beat-both` and `beat-either` provability over 3 seeds;
- roll-backs and rounds per placed item;
- per-attempt wall time against the #582 30 s floor and 90 s ceiling;
- surplus, drop and leftover counts, and the real-engine surplus and trap
  behaviour.

Post ONE comment on #645. If `beat-both` still fails, name the next mechanism.

(5) Leave the goldens untouched; nothing here runs in production. Name the
four golden rows to show it.

**Builds:** yes; run both tiers.

**Files owned:**
- `src/common/combo_logic.{h,c}` and `src/common/shared_items.{h,c}`;
- the bag-building and classify functions of both engine TUs, at function
  granularity (K10 adds persistence functions and K12 a triforce carrier to
  the same TUs);
- the measure, bag-model and multiplicity tests;
- the shared append-only files.

NOT `fill.cpp`, `OnFileCreate.cpp` or the creation event (K11).

**Keywords:** `Fixes #727, fixes #731, fixes #733`, `Refs #645, refs #582`.
