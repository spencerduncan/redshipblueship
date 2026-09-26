# Lane H3 (wave 6) — tracker and docs hygiene for wave 6

**Branch:** `claude/wave6-tracker-docs-hygiene`

**Task:** Docs and tracker work only. No local build; CI must be green.

1. **The #645 body.**
   - Record wave 5's results:
     - K4's multiplicity: `beat-either` proved; `beat-both` blocked by the cap;
       the 4096-cap experiment took 44.5-50.9 s.
     - K5's O8 table.
     - K8's O6 tooling.
     - W's #583, #681, #643 and #719.
   - Tick O6 and O8, and mark O7 and O10 in flight.
   - Add the wave-6 lanes: K9, K10, K11 (held), K12 and F26.
   - Add the remaining children: GOAL-UI / O11 presentation, retiring the
     pinned pools (with K11), and the second re-pin (with K11).
2. **ADR 0010 dated amendments**, appended only:
   - O5 merged (PR #729), with no golden moved.
   - O6 delivered (PR #734).
   - O8 delivered (PR #725), with trap as a separate class.
   - The multiplicity contract (PR #728), plus the trap correction: "a trap
     never crosses" is a caller convention.
   - The measured numbers for increment 3.
3. **`docs/solver-inventory.md` §6 status.**
   - P7 delivered, and the three places that still read 45 slices annotated.
   - P8 delivered.
   - P10 partial: 26 of 86 re-counted.
   - P12 re-measured.
   - P14 decided (K9).
4. **`docs/known-issues.md`.**
   - Loose mods (#732).
   - The MM autosave interval (#730).
   - The Deku-Stick gate (#729).
   - The Windows `rando` tier (#723).
   - The plentiful wallet wrap (#726), until F26 lands.
   - Stale "fix in flight" entries whose agent trackers closed on 2026-09-11.
5. **This file set.** The `.claude/worker-prompts.md` header and lane table,
   and one card per wave-6 lane.

**Files owned:**
- `docs/solver-inventory.md` (status annotations only)
- `docs/adr/0010-*.md` (amendments only)
- `docs/known-issues.md`
- `.claude/worker-prompts.md`
- `.claude/lanes/*.md`
- Bodies and comments of agent-authored issues only.

**Keywords:** `Refs #645, refs #500`.
