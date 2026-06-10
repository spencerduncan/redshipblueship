# Worker loop goals — Wave 2 (updated 2026-06-10)

Lanes 1–4 are complete (record below). Active lanes: 5 and 6 (cloud). Each active
section is a complete, self-contained loop goal for one code worker: run it as a `/loop` goal
(self-paced) or as a plain dispatch prompt. Lanes 5 and 6 may run concurrently — their
pre-flight checks enforce mutual exclusion on contended files.

Supersedes the Wave 1 prompts (issues #154–160, all closed — see git history of this file).

## Completed lanes

- **Lane 1** — PR #249 (scrolling texture interpolation) merged → main `7563640b6a`; #234
  closed (2026-06-10).
- **Lane 2** — PR #247 (Custom Messages → Hooks) merged → main `5ecce8f5b6`; #228, #253,
  #254–#258 closed; gate-clear comment posted on #235 (2026-06-10).
- **Lane 3** — PR #299 (MM tracker architecture sync) merged → main `a4eb32fdfa`; #297 closed
  (2026-06-10). The #238 PR-A revival (individual tracker improvement ports) is now unblocked
  and awaits a future lane.
- **Lane 4** — admin closeout done locally (2026-06-10): Phase 0/1 milestones closed; worktrees
  pruned ~130 → 20 (dirty/ahead/protected entries kept); all 16 audited merged branches
  deleted; no remote branches deleted — stale-remote candidates (e.g.
  `claude/fix-randomizer-compilation-6LcCr`) remain listed for maintainer sign-off.

## Common rules (all lanes)

- Repo: `spencerduncan/redshipblueship`. Default branch `main`. All merges are **squash merges**.
- **You are expected to open PRs, merge them once green, and close issues for your lane.**
  Pushes, PRs, and merge-on-green are automated by default in this repo (see CLAUDE.md
  conventions) — closure is the goal. If your environment's GitHub credentials cannot merge,
  stop at "CI green + ready to merge" and report that state instead.
- CI is the authoritative gate: `clang-format`, `clang-tidy`, `generate-builds` (Linux + Windows),
  and `integration-tests-linux` (runs `ctest --label-regex "^redship$"` — BootOoT/BootMM/Roundtrip
  live there). Full wall clock ≈ 25–40 min; Windows is the slow job.
- Local builds (if your env supports them): `git submodule update --init`, `cmake -B build -S .`,
  `cmake --build build --parallel`. Needs 8 GB+ RAM. Prebuilt-deps image:
  `ghcr.io/spencerduncan/redshipblueship-build:latest`. When in doubt, trust CI over local.
- **Always `git fetch` before touching any branch** — stale local copies of work branches exist
  and have caused confusion before.
- Loop protocol: each iteration = (1) fetch + assess state against the closure criteria,
  (2) take the smallest next step, (3) push, (4) wait for CI (`gh pr checks <n> --watch` or poll
  ~5 min), (5) on red, diagnose and fix — max 3 attempts per distinct failure, then stop and
  report, (6) on green, advance. Exit the loop only when every closure criterion is true.
- Post a one-line progress comment on your lane's driving issue at each milestone so other lanes
  and humans can see state. Report outcomes faithfully — failed checks get quoted, not summarized
  away.
- Never force-push over commits you didn't write without fetching and reading them first.

---

## Lane 5 — shuffle feature ports + 9.2.x rando absorption (epic #235, serial)

**Goal:** Land the five remaining SOH shuffle features (#289–#293) one at a time as individual
PRs, absorb the SOH 9.2.x rando/hint batch, then close epic #235.

**Closure criteria:**

- [ ] #289 Shuffle Speak merged (own PR, squash, CI green) and issue closed with merge SHA
- [ ] #290 Shuffle Climb merged and closed, same standard
- [ ] #291 Shuffle Grab merged and closed, same standard
- [ ] #292 Shuffle Open Chest merged and closed, same standard
- [ ] #293 Pot CMC merged and closed, same standard
- [ ] SOH 9.2.x rando/hint batch absorbed in 1–3 thematic PRs, all merged green (upstream SOH
  PRs #6540, #6557, #6565, #6608, #6641, #6647, #6648, #6661 — all merged upstream)
- [ ] Epic #235 closed with a summary comment listing every merge SHA and what each PR shipped

**State as of 2026-06-10:** Gate is OPEN — PR #247 merged as `5ecce8f5b6`, PR #249 as
`7563640b6a` (both merged 2026-06-10 UTC); gate-clear comment posted on #235 at
2026-06-10T16:32:15Z. 6 of the original 11 features are already done (PR #248, merged as
`aa2bbe4b05` / already in-tree — see #235 body); the remaining 5 are split into sub-issues
#289–#293, all OPEN, none of their symbols in `randomizerTypes.h` on main. Zero open PRs as of
2026-06-10 ~18:00Z (Lane 3's #299 merged), so the serialization gate is clear — start now.
#235 is this lane's driving issue for milestone comments. Upstream SOH commits (verified):
Speak `e2db315ff` (SOH PR #5538), Climb `596b714fa` (#5182), Grab `18b00e7bc` (#5719),
Open Chest `04df48944` (#5946), Pot CMC `f52b653cf` (#6167).

**Serialization pre-flight (run at the top of EVERY iteration, before starting a new feature
or batch — these are hard gates, not suggestions):**

1. `git fetch origin`. Work only from fresh `origin/main`.
2. `gh pr list -R spencerduncan/redshipblueship --state open --json number,headRefName` — for
   each open PR OTHER THAN your own single in-flight Lane 5 PR, list its files with
   `gh api repos/spencerduncan/redshipblueship/pulls/<n>/files --paginate --jq '.[].filename'`
   (never `gh pr view --json files` — it silently caps at 100 files, and on asset-heavy
   shuffle PRs the alphabetically-first asset paths fill the window and hide the source files)
   and grep for `Enhancements/randomizer/item_list.cpp`,
   `Enhancements/randomizer/randomizer.cpp`, or
   `Enhancements/randomizer/3drando/item_pool.cpp`. If any such PR touches any of these, do
   not start a NEW feature — wait and re-check next iteration. Your own open Lane 5 PR is
   exempt: keep iterating on it, fixing CI, and merging it regardless of this gate.
3. Confirm you have at most ONE Lane 5 PR open. Never two features in flight.
4. Confirm the previous feature's PR is merged AND its issue closed before branching the next.

**Steps (read each sub-issue body first — it has the exact spec and upstream SHA):**

1. Features in strict order: #289 → #290 → #291 → #292 → #293. For each: branch
   `claude/shuffle-<feature>-<issue#>` off fresh `origin/main` (e.g. `claude/shuffle-speak-289`,
   `claude/shuffle-climb-290`, `claude/shuffle-grab-291`, `claude/shuffle-openchest-292`,
   `claude/shuffle-potcmc-293`).
2. Port the upstream SOH commit (SHA above) from HarbourMasters/Shipwright. Path mapping:
   upstream repo subdir `soh/` → `games/oot/`, i.e. `soh/soh/**` → `games/oot/soh/**`,
   `soh/src/**` → `games/oot/src/**`, `soh/assets/**` → `games/oot/assets/**` (the asset bulk
   of each diff lands under `games/oot/assets/custom/`), `soh/include/**` →
   `games/oot/include/**`. Both `Enhancements/randomizer/item_list.cpp` and
   `Enhancements/randomizer/3drando/item_pool.cpp` exist in RSBS and are touched by these
   ports — map each upstream file to its identically named RSBS counterpart; never fold
   `item_pool.cpp` hunks into `item_list.cpp`. Expected touch points:
   `games/oot/soh/Enhancements/randomizer/` (item_list.cpp, 3drando/item_pool.cpp,
   randomizerTypes.h, logic, location_access), `games/oot/soh/SohGui/` (menu options),
   `games/oot/src/overlays/actors/ovl_player_actor/z_player.c`. Exception: Pot CMC
   (`f52b653cf`) touches only ShufflePots.cpp + ~192 asset files, none of the conflict files.
3. Each feature is L/XL with ~150–200 files, mostly assets flowing through the o2r pipeline.
   The CI job `generate-rsbs-otr` (in both generate-builds and integration-tests workflows)
   rebuilds the archive; its cache key hashes `games/**/*.c`, `games/**/*.h`, and
   `games/**/*.xml` (plus OTRExporter/ZAPDTR sources), so source and asset changes alike
   force a full regeneration — expect the first CI run on each PR to be slow, not hung.
4. Per-feature acceptance (from #235 — verify via CI, no local build assumed): new shuffle
   option appears in the randomizer menu; seeds generate cleanly with the option enabled
   (alone AND combined with existing shuffles); logic validates with no unreachable items;
   existing shuffles still work; in-world placement looks correct for new check types.
5. Open the PR referencing the sub-issue and the upstream SHA. Re-run the pre-flight
   contention check immediately before merging (parallel lanes can open PRs mid-run), then
   squash-merge on green; if main moved since the PR last went green, update the branch and
   wait for fresh green first. After merge, confirm main's post-merge CI is green, close the
   sub-issue with the merge SHA, post a one-line milestone comment on #235. Only then return
   to the pre-flight and start the next feature.
6. After all five are closed: absorb the 9.2.x batch as 1–3 thematic PRs, same pre-flight and
   same one-at-a-time rule; branch as `claude/92x-<topic>-235` (no sub-issues exist — all
   evidence and milestone comments go on #235). Suggested grouping — hints: #6540 (refactor
   hints), #6565 (Ganon's Tower & Mido hint logic), #6648 (gossip stone check fix);
   checks/trackers: #6557 (bean merchant tracker), #6641 (merchant/scrub checks), #6661
   (carpenter item logic); world/audio: #6608 (ending audio shuffle), #6647 (Ganon's castle
   blue warp). Skip anything already present on main — check via
   `git log origin/main --grep '<upstream PR#>'` plus `git grep` for a distinctive symbol
   from each upstream diff, and note skips in the PR body.
7. Close #235 with the summary comment (all merge SHAs, features + batch).

**Safety rails:**

- Never bundle two features in one PR — bundling was explicitly rejected as unreviewable in
  the #235 re-scope (2026-06-09).
- Never stack a feature branch on a previous feature branch; always branch from `origin/main`
  after the prior merge.
- If a port conflicts heavily or an upstream commit doesn't apply against the RSBS tree,
  follow the Common-rules failure budget, then push the branch and report on #235 — do not
  force a broken adaptation through.
- Do not close #235 while any of #289–#293 or the 9.2.x batch is unfinished.

---

## Lane 6 — Phase 2 closeout (epic #202, milestone 3)

**Goal:** Close every open Phase 2 issue except #34 — #233, #231, #232, #288, #211, #286, #212, in that order — then close epic #202 and the Phase 2 milestone. Driving issue for progress comments: #202. Lane branch prefix: `claude/lane6-issue<NNN>` — this prefix is the lane's identity for the pre-flight PR check.

**Closure criteria:**
- [ ] #233 closed — `sohFast3dWindow = nullptr;` in `DeinitOTR()` merged (SOH `79d6f54be`)
- [ ] #231 closed — `DetectOTRVersion("soh.o2r", false)` at OTRGlobals.cpp:345 merged (SOH `35039565d`)
- [ ] #232 closed — RunExtract follow-ups dispositioned: #6215 re-check + `96c4fef05` (#6386) + `99c1f23d5` (#6412) + `adb1e46ba` (#6501). For any commit found already covered by #250/#241, a disposition note on #232 naming the pre-applied hunks satisfies that item
- [ ] #288 closed — MM ConfigUpdaters (2S2H #1703) merged; close comment explicitly defers the gCore/gOoT/gMM namespace migration to #34
- [ ] #211 closed — SOH #6656 + applicable #6636 subset merged; disposition table posted mirroring #226's format
- [ ] #286 closed — every `rand()` call site in `games/` + `src/common` converted EXCEPT the Switch/WiiU seed fallback at `games/oot/soh/ShipUtils.cpp:122` (identical line exists on Shipwright develop — upstream parity; document as intentional in the close comment). Re-grep first; 32 convertible sites / 16 files as of 2026-06-10 (33/17 counting the exempt line)
- [ ] #212 closed — verification comment maps every checklist item → CI run / commit / manual-QA note
- [ ] #202 closed with note: #235 + #289–#293 continue under Phase 3
- [ ] Milestone 3 closed via `gh api -X PATCH repos/spencerduncan/redshipblueship/milestones/3 -f state=closed` — only after #34 is out of it
- [ ] #34 untouched: ZERO comments authored on #34, no commits, no state/label/milestone changes by this lane. The re-milestone request lives on #202; its `#34` mention leaves only a timeline cross-reference

**State as of 2026-06-10 (main tip `a4eb32fdfa`, post-PR #299):**
- Phase 2 milestone (number 3) open set is exactly {#202, #211, #212, #231, #232, #233, #286, #288, #34}. #235/#289–#293 already re-milestoned to Phase 3.
- PR #250 (`566900772f`, SOH #5892 ImGui extraction flow) is the event that unblocked #231/#232/#233. PR #247 merged as `5ecce8f5b6`, #249 as `7563640b6a`, #299 as `a4eb32fdfa` (all 2026-06-10). #299 rewrote `games/mm/2s2h/ShipUtils.{cpp,h}` and touched UIWidgets.cpp/ConvertItem.cpp — all #286/#288 facts below re-verified post-merge.
- #231 is NOT verify-and-close: the #5892 helpers exist on main (`portArchivePath` :289, `sohArchiveVersionMatch` :290, detection block :344–348) but line 345 still has the pre-fix `DetectOTRVersion(portArchivePath, false)`. The 2026-06-01 triage comment on #231 claiming verify-only is wrong about the fix itself.
- #233 has nothing to salvage: no remote branch matches; local `fix/issue-233-segfault-quit` is an empty diff at stale base `3a32bfb439`. Start fresh.
- `claude/appimage-fix-232` is already-merged content (PR #241 / `3775c368d2`, closed #183) — do not resume it.
- #34 is design-blocked on the gCore/gOoT/gMM config-namespace decision (no `redship.json` exists; `rsbs/src/main.cpp:270–281` deliberately pins `shipofharkinian.json`; only TODO stubs in `src/common/ComboMenuBar.cpp:16–34,159–175`). EXCLUDED from this lane — flag, don't decide.

**Pre-flight (every iteration, after the Common-rules fetch):**
1. `gh pr list -R spencerduncan/redshipblueship --state open --json number,headRefName`
   - A PR is this lane's iff `headRefName` starts with `claude/lane6-`. If one exists → drive it to merge first; never two lane PRs open at once. Never drive or merge a PR whose head doesn't match the prefix (other lanes use `claude/*` too — e.g. `claude/mm-tracker-sync-297` was open under the same convention).
   - File-contention gate before opening ANY PR: list this PR's planned files, then for every other open PR run `gh pr diff <num> --name-only` and block on any non-empty intersection until that PR merges, then rebase. Do NOT use `--json files` for this — it truncates at 100 entries (PR #149: changedFiles=698, files returns 100) and lane-scale PRs here exceed that.
   - Known contention surfaces: `games/oot/soh/OTRGlobals.cpp` (#233/#231/#232/#211 vs the shuffle lane #289–#293); ShufflePots.cpp (#211's #6636 subset vs open shuffle issue #293 "Pot CMC"); MM-lane files for #288/#286 — BenPort.cpp, UIWidgets.cpp, ConvertItem.cpp, `games/mm/2s2h/ShipUtils.{cpp,h}` (PR #299 contended on all of these before it merged 2026-06-10).
2. `gh issue view <next> -R spencerduncan/redshipblueship --json state` — if the next step's issue is already CLOSED, confirm the specific fix CONTENT is on `origin/main` before advancing — content-level, not file-touch-level (OTRGlobals.cpp churns in most lane PRs): `git show origin/main:games/oot/soh/OTRGlobals.cpp | grep -n 'sohFast3dWindow = nullptr'` (#233), same with `grep -n 'DetectOTRVersion("soh.o2r"'` (#231), `git log origin/main -S'<distinctive token>'` per upstream commit for #232 (all four, individually). If a fix is on main but the issue is open, close it with that evidence (SHA + file:line) instead of opening a redundant PR.
3. The #211 branch must be rebased onto fresh `origin/main` immediately before its PR is opened — it ends the OTRGlobals chain and is the likeliest conflict target.

**Steps (one PR per issue, squash-merge-on-green; read each issue body first — it has the exact test criteria):**
1. Post one comment on #202 (NOT on #34): recommend re-milestoning #34 to Phase 3 since it is blocked on the config-namespace design decision and milestone 3 cannot close around it. The `#34` mention cross-references its timeline without authoring on it. Check #202's existing comments first and skip if an equivalent request is already posted — the loop re-enters this step every iteration. Maintainer call. Do this first for lead time.
2. **#233** — port SOH `79d6f54be`: add `sohFast3dWindow = nullptr;` immediately after `SohGui::Destroy();` (:1624) inside `DeinitOTR()` (`games/oot/soh/OTRGlobals.cpp:1606`; var declared :286). Issue criteria include MM quit paths and cross-game-switch-then-quit; Valgrind/window-close items are manual — this PR closes on CI green + code-port evidence, with manual residue rolled into step 8's #212 carve-out.
3. **#231** — port SOH `35039565d`: OTRGlobals.cpp:345 `DetectOTRVersion(portArchivePath, false)` → `DetectOTRVersion("soh.o2r", false)`. Rationale: `DetectOTRVersion` (:1477) calls `LocateFileAcrossAppDirs(fileName, appShortName)` itself, so it needs a bare filename, not a resolved path. Verify the issue's 3 criteria (version match passes; mismatch → "soh.o2r is outdated"; missing path graceful).
4. **#232** — in `RunExtract` (OTRGlobals.cpp:425): re-check ALL FOUR upstream commits' deltas against the post-#250 flow, not just #6215 — PR #241 already ported #6215's Extract.cpp ROM-search half, and #6501's `argv[i]` hunk is pre-applied on main. Port what remains in upstream order: `96c4fef05` (#6386, detect extraction task crash) → `99c1f23d5` (#6412, move `CheckAndCreateModFolder()` earlier) → `adb1e46ba` (#6501, extractor args handling). Fully-covered commits go in the #232 disposition note (closure criterion accepts this). AppImage-hardware criteria are manual — same CI-green + step-8 carve-out policy as #233.
5. **#288** — port 2S2H #1703 (+79/−1): new `games/mm/2s2h/config/ConfigUpdaters.{cpp,h}` + register `Ben::ConfigVersion1Updater` in MM's `InitOTR` (`games/mm/2s2h/BenPort.cpp:728`) per upstream's BenPort.cpp hunk. In-repo pattern: `games/oot/soh/config/ConfigUpdaters.{cpp,h}` (landed via PR #243). MM tree has zero ConfigVersion files today. Single-exe: MM_ symbol prefixing; check `src/common/mm_stubs.c`; the shared Ship::Context means the updater runs against `shipofharkinian.json` (`rsbs/src/main.cpp:279–280` pins it). The issue body's "reconcile with #34's migration design" sentence is answered in the close comment by explicitly deferring the namespace migration to #34 — do not wait on #34.
6. **#211 residue** — port SOH #6656 (stdint.h include hygiene, 48 headers; grep-verified absent from our OTRGlobals.h) + the applicable subset of SOH #6636 "Clean OTRGlobals 2" (72 files upstream). Governing rule: derive the present/absent split FRESH from #6636's full file listing (`gh pr diff 6636 -R HarbourMasters/Shipwright --name-only`) against our main — port hunks ONLY for files present. Known-present examples (not exhaustive): ShuffleGrass.cpp, SeedContext.cpp, RocsFeather.cpp, ShufflePots.cpp, ShuffleBeehives.cpp, ShuffleCows.cpp, ShuffleCrates.cpp, ShuffleFairies.cpp, ShuffleFreestanding.cpp, ShuffleTrees.cpp. Known-absent examples (not exhaustive): ShuffleSpeak.cpp, ShuffleIcicles.cpp, ShuffleWonderItems.cpp, particle_cmc.h, ShuffleBeggar.cpp, ShuffleRedIce.cpp, ShuffleRocks.cpp, ShuffleSigns.cpp. Hand ALL absent-file hunks to the #289–#293 lane via one comment on #235. Re-assess `b65c1c831` (SOH #6385 unused-includes) for what survives post-#247/#250 churn. Close #211 with a disposition table mirroring #226's.
7. **#286** — strictly after #211 (upstream merge order #6636 → #6553; they overlap on AudioEditor.cpp and SohMenuRandomizer.cpp). Re-grep first (`git grep -n '\brand()' -- games/ src/common`); as of 2026-06-10 post-#299: 33 sites / 17 files, 32/16 convertible. Per-tree targets:
   - OoT C++ sites (incl. Network/Sail/Sail.cpp ×10, Enhancements/mods.cpp ×3) → `ShipUtils::Random` (`games/oot/soh/ShipUtils.h:33`, impl ShipUtils.cpp:136 — helper exists, no new OoT helper work).
   - OoT C sites — `z_en_bom.c:107`, `z_en_vali.c:252` — CANNOT call ShipUtils::Random (C++-only: `#ifdef __cplusplus` namespace, default args, no C linkage). Use `Rand_ZeroOne()` per upstream #6553's own z_en_bom.c hunk; z_en_vali.c is RSBS-divergent (not in #6553) — apply the same pattern.
   - MM sites (BenGui/CosmeticEditor.cpp ×3 :346–348, BenGui/UIWidgets.cpp:1324, Audio/AudioEditor.cpp:345, ChuDrops.cpp:78, EnGirlA.cpp:301, ConvertItem.cpp:91, Traps.cpp :44/:136/:138) → MM's own `extern "C" s32 Ship_Random(s32, s32)` (`games/mm/2s2h/ShipUtils.h:51`, PCG impl ShipUtils.cpp:321) — NOT the OoT helper, which MM files cannot include.
   - LEAVE `games/oot/soh/ShipUtils.cpp:122` unconverted — it is the `__SWITCH__`/`__WIIU__` seeding fallback inside the RNG helper itself (converting it is circular; identical line on Shipwright develop, untouched by #6553).
   Upstream #6553's EnemyRandomizer.cpp / RandomizedEnemySizes.cpp hunks may not apply (no `rand()` there in our tree). Issue scope includes MM + `src/common` (extra vs upstream; currently zero `rand()` outside `games/`). Does not touch OTRGlobals.cpp.
8. **#212** — post the final verification comment mapping every checklist item → evidence:
   - CI tier: the redship-labeled BootOoT/BootMM/Roundtrip suite runs unconditionally (`ctest --label-regex "^redship$"` at `generate-builds.yml:307` and `integration-tests.yml:141`; tests registered with `LABELS "redship"` at `CMake/SingleExecutable.cmake:191–217`).
   - The 2026-04-20 comment remains ACCURATE about the int-* tier: IntBootOoT/IntBootMM etc. (`SingleExecutable.cmake:226–239`, `LABELS "integration"`) stay archive-gated (`integration-tests.yml:143–160` archive check; `:163`/`:173` `if: has_oot/has_mm`) and skip in hosted CI where ROMs cannot exist. Do NOT call that comment stale — map int-*-covered rows (real-ROM "boots and plays") to manual QA, not to CI evidence.
   - Add the freshly merged #231/#232/#233 commits. Carve genuinely-manual residue (save create/load, rando seed gen, valgrind, OoT↔MM runtime feel, AppImage hardware smoke, plus per-issue manual items deferred from steps 2–4) into a follow-up issue or explicit manual-QA note. Close #212.
9. **#202 + milestone** — verify closure concretely: `gh issue view <n> --json state` for each of #211/#212/#231/#232/#233/#286/#288 must return CLOSED (#202 has no GitHub sub-issues; its body checklist's only still-open items are #235 + #289–#293 under Phase 3). Close the epic with the Phase 3 re-scope note (#235/#289–#293). Then milestone 3: if #34 still sits in it, do NOT close — post one follow-up on #202 re-raising the step-1 request (never on #34), then stop and report "all Phase 2 issues closed except #34; milestone blocked on #34 placement". Otherwise PATCH it closed (command in closure criteria).

**Safety rails:**
- **Never work #34.** No code, no state/label/milestone change, zero comments authored on #34. All #34 discussion happens on #202.
- OTRGlobals.cpp PRs strictly serialized: #233 → #231 → #232 → #211. The lane is serial overall — one open PR at a time even for non-overlapping work (#288, #286).
- Never close milestone 3 while any issue (including #34) is open in it — lane policy, not a platform constraint (GitHub permits closing milestones with open issues); never close #202 while #211/#212/#231/#232/#233/#286/#288 are open.
- Do not resume `fix/issue-233-segfault-quit` (empty) or `claude/appimage-fix-232` (merged) — fetch first per Common rules, then branch fresh from `origin/main`.
- Push rebased lane branches with `--force-with-lease` only; never force-push any branch this lane did not create; never push to main.
- Line numbers above were verified against `a4eb32fdfa` (main tip 2026-06-10, post-#249/#299 — both touched cited files); if main has moved, locate by symbol (`DeinitOTR`, `RunExtract`, `DetectOTRVersion`, `Ship_Random`), not by line.
