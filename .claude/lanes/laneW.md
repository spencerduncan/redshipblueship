# Lane W (wave 5) — the world-moving bundle, one golden re-pin per change

**Branch:** `claude/world-moving-bundle-583-681-643-719`

**Task:** Four changes that deliberately move generated worlds, each as its
own code + lock commit followed by its own re-pin commit ("Re-pin goldens
after <item>: <fields that moved, why>"), ordered so each golden diff shows
ONE cause. Read `docs/determinism-goldens.md` "Re-pinning" first and use
`regen-golden-digests` (plus `regen-golden-digests-rom-mounted` when the
paired golden moves). If an item does not move a world, say so and do not
re-pin for it. (1) **#583**: a foreign-placement shortfall drops a
uniformly random, identity-seeded subset (shuffle-then-truncate over the
eligible reachable hosts), not the tail; keep #680's creation-time
shortfall surface accurate; lock that drops differ across seeds and are
stable within one. (2) **#681**: ADR 0011 criterion 3 becomes
profile-conditional per D3.5 over PR #680's values surface; lock one
narrowing profile and one that does not narrow. (3) **#643**: pin MM's time
slices at 46 by adding `NIGHT2_AM_05_30` everywhere the tree enumerates
slices; every O5 consumer agrees. (4) **#719**: gate MM Glitchless's
Deku-stick combat disjunct on `MMRT_DEKU_STICK_FIGHTING` (default off),
citing PR #686's "sync and gate" keg/GBT precedent; red/green lock as in
#686. Update the ADR answer rows (O5 adopted; criterion-3 narrowing
delivered) by dated amendment. PR body: one section per item with its
golden diff summarized. Pre-release: world and save changes are accepted,
and every one is stated. Builds; both tiers.

**Files owned:** the files each item names; `tests/golden/*`; ADR 0010 and
0011 amendment paragraphs (append only); the shared append-only files. NOT
`combo_logic.*` or the engine TUs.

**Keywords:** `Fixes #583, fixes #681, fixes #643, fixes #719`, `Refs #645, refs #578`.
