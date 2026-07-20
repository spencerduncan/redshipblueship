ultracode — Execute Phase 3 of RedShipBlueShip: close out Phase 2's debt and land the "Basic Combo Randomizer" MVP.

Read `docs/phase3-roadmap.md` (the phase plan), `CLAUDE.md` (build/test/conventions), and `gh issue view 381` (the Phase-2 review tracker) before planning. `gh` lives at `"C:/Program Files/GitHub CLI/gh.exe"` and is not on PATH.

Phase 2 merged as `c8c47fa6` with fully green CI. That cleared the roadmap's hard blocker; everything downstream is unblocked.

**The roadmap is good but predates a 2026-07-19 review and ODR sweep.** Six findings below were verified against source or the built binary after it was written. Four of them contradict the roadmap or an obvious plan. Trust these over the roadmap where they conflict, but re-verify anything you are about to build on — two claims in this very analysis were overturned by a second look.

---

## Six corrections that reshape the phase

### 1. MM's randomizer is not in the shipping binary

`games/mm/2s2h/Rando/` is a complete string-keyed randomizer — own pools, fill, logic tiers (`NoLogic.cpp` already implements the deferral Lane D wants), a 382-row item table, `Rando::GiveItem`, spoiler JSON. It is built as `2ship_rando` and linked. **The linker discards every object**, because no undefined symbol in the binary references it.

Verified two ways:
- Every non-`Rando/` file referencing `Rando::` is in an excluded TU: `BenPort.cpp` (`games/mm/CMakeLists.txt:202`), `BenGui/` (`:205`), `DeveloperTools/` (`:208`), `SaveManager/` (`:250`), or `Enhancements/**` — and `:335` states `2ship_enh`/`2ship_rando` deliberately do **not** get `WHOLE_ARCHIVE`. Filtering those leaves the empty set.
- String probe of the real `build-cmake/redship.exe`: `"Enable Rando (Randomizes new files upon creation)"` **ABSENT**, while the control `"gEnhancements.Graphics.MotionBlur.Mode"` is **PRESENT**.

Consequences: any Lane C plan that edits `StaticData/Items.cpp` / `itemPool` / `GiveItem.cpp` would compile, pass CI, and **do nothing**. `#384` (registrar collisions) therefore **does** gate Lane C, since getting `2ship_rando` into the link means either `WHOLE_ARCHIVE` (needs #384 cleared) or an explicit `MM_Rando_Init()` from `GameExports_SingleExe.cpp`. And the `rando` CTest label is three invocations of one **OoT** test (`CMake/SingleExecutable.cmake:278/291/292`) — there is zero MM-side rando coverage because there is no MM-side rando.

**Wave 0 spike, before Lane C is scoped:** what is the smallest change that makes `2ship_rando` reachable, and what does it drag in?

### 2. The cross-game seed does not propagate in single-exe

The roadmap's §1 table says seed propagation and the `sourceIsRando` handshake are "wired both directions," citing `OTRGlobals.cpp:2756-2766` and `BenPort.cpp:2153-2226`. Both cited sites are in the **dead legacy path**:

- `OTRGlobals.cpp:2866` is inside `OoT_FreezeState(ComboContext*)` (starts `:2850`)
- `BenPort.cpp:2167/:2222` are inside `MM_InitFirstEntrySaveContext` / `MM_FreezeState(ComboContext*)` — and `BenPort.cpp` is excluded (`:202`)
- `src/common/switch.cpp:171` states these are "legacy … only compiled in non-single-exe builds"; their only caller `Context_ProcessSwitch` has no callers

The **live** path is `Combo_FreezeState(gameId, returnEntrance, saveCtx, size)` → `Context_FreezeState` → `gFrozenStates.FreezeState(...)`, carrying a `SaveContext` blob and nothing else. No `ComboContext`, so no seed, no `sourceIsRando`, no `sharedItems`, no `sharedFlags`.

So Lane A is larger than scoped: **the `ComboContext` channel must cross the switch in the live path at all** before any item work means anything. Lane B's "seed propagation already exists; only generation-side determinism is missing" is false as written.

### 3. `ComboContext` has no serialization headroom — this bites the moment Lane A lands

`src/common/save.cpp:31` sets `kComboSize = sizeof(ComboContext)`, and `DeserializeHeader` (`:158-160`) does `if (h.comboSize != kComboSize) return false;`, commented *"ComboContext has no capacity headroom."* `Load()` returning false leaves `gComboCtx` untouched **and shows the user nothing.**

The roadmap's own #1 risk mitigation — tag shared items with origin game — widens `sharedItems` and changes `sizeof(ComboContext)`. **Every existing `.redsave` then silently stops loading**, and again on every later shape change.

Fix before Lane A's first commit and before the `v0.1.0-prealpha` tag (i.e. before the operator holds saves they care about): add reserved padding, convert the tier-1 check from exact-match-refuse to the same size-field-driven zero-extend the game tiers already use (`save.cpp:167-170`), bump `RSBS_SAVE_VERSION`. An afternoon now versus a migration path plus an operator sitting later.

Note the **write** path is genuinely good — atomic temp-plus-rename (`:110-133`), CRC32 over payload, explicit refusal to write a half-empty file. Roadmap §7's "Fault A may corrupt saves" is wrong; the real Fault A exposure is the `OnExitGame` write not being *reached*, which is loss-of-latest-state, not corruption.

### 4. `#370` does gate Lane A — settled from source

The `.redsave` write executes **inside** the mutex #370 poisons: `SaveManager.cpp:1264` locks `saveMtx`, `:1353` fires `ExecuteHooks<OnSaveFile>` — where the handler registered at `:151` calls `RsbsSave_Save()` at `:164` — and `:1355` unlocks. The coupling is through hook dispatch, not through an include, so "the two SaveManagers are separable" is technically true and analytically irrelevant.

Severity is narrower than it first looks (the leak needs a malformed-UTF8 OoT JSON meta save, on the randomizer branch at `:624`), but the fix is a `lock_guard` plus temp-rename — under an hour. Land it in Wave 0.

**Forward-looking trap worth documenting before Lane A is written:** `saveMtx` is a plain `std::mutex` (`SaveManager.h:200`), so any Lane A code hanging off `OnSaveFile` that re-enters SaveManager self-deadlocks.

### 5. #387's "fast link-only job" cannot exist — fix ccache instead

Measured against a real run: **31.42 of build-linux's 32.7 minutes is compilation.** Stripping asset download, tests, and packaging saves ~1.1 minutes. The advertised ~5-minute link-only job is not achievable.

The actual lever is `generate-builds.yml:227` — `save: ${{ github.ref_name == github.event.repository.default_branch }}` restores ccache on feature branches but **never saves** it, so a long-lived `claude/**` branch re-misses the same objects on every push. Fix the policy; drop the link-only job idea.

(#387's *diagnosis* stands — this class is invisible on the operator's Windows-only box because Windows links `/FORCE:MULTIPLE`. Only the proposed remedy was wrong.)

### 6. Four required items no plan covered

- **The known-issues doc does not exist** (`docs/` has no `known-issues*`), and it is a hard `#321` gate alongside #310. Any plan that tags `v0.1.0-prealpha` after one operator sitting is describing a tag that cannot be cut. Write it **before** the sitting so the operator's findings append to it. Agent-doable, no ROMs.
- **`.claude/worker-prompts.md` is stale and actively misleading.** Line 1 says "Wave 3"; the Rev 7 body still claims "eleven commits awaiting push (gh auth still absent)" — corrected on 2026-07-18, still wrong today. This is the file agents read to learn what wave they are in. Minimum fix: replace the body with a pointer to the tracker epic. Ten minutes.
- **File the Phase 3 tracker epic** with lanes as sub-issues, shaped like #381 (categorized, with the *why* on each item). Without it every agent invents its own wave numbering.
- **`gComboCtx.saveSlot` is dead plumbing too** — declared `context.h:103`, set to `-1` at `context.cpp:210`, otherwise referenced only in `src/common/tests/`. Same status as `sharedItems`/`sharedFlags` (both confirmed zero non-test references). Decide its fate during Lane A rather than carrying it.

---

## Execution shape

### Wave 0 — unblock, in parallel (no lane touches another's files)

| Lane | Work | Files |
|---|---|---|
| 0a | **#388** headless crash handler | `CrashHandler.cpp` |
| 0b | **#370** `lock_guard` + temp-rename | `games/oot/soh/SaveManager.cpp` (sole owner) |
| 0c | **ccache policy** (replaces #387's link job) | `.github/workflows/generate-builds.yml` |
| 0d | **Spike:** smallest change making `2ship_rando` reachable | read-only, report |
| 0e | Known-issues doc; worker-prompts.md pointer; tracker epic; #34/#177 out of milestone 4 | docs + `gh` |

Do **#388 first or concurrently** — it is a throughput multiplier, not a correctness item. Every wave below is agent-driven and CI-only-observable, and right now a crash presents as a 180s timeout with no exit code, signal, or stack. It already cost a full Linux bisect this week.

### Wave 1 — save/switch integrity (gates Lane A)

`ComboContext` headroom + versioned migration **solo** (exclusive ownership of `src/common/context.h`, `context.cpp`, `save.cpp`, `unified_save.c`), then in parallel: **#364** (`rsbs/src/main.cpp`, `src/common/switch.cpp`), **#374** (`src/common/entrance.h`), **#371 + #378 together** (both edit `audio_load.c` — one agent, not two).

### Wave 2 — MM binding correctness (parallel with Lane A; gates Lane C)

**#382**, **#383 GameInteractor**, **#385**. Note #382 and #383 both need `games/mm/CMakeLists.txt` — give one sole ownership and have the other rebase, or land the filter change as one preparatory commit both branch from. A detailed #383 handoff prompt already exists; its key finding is that the `S2H` namespace pattern **does not** transfer to GameInteractor (sharing is architectural per `CMakeLists.txt:224` "use OoT's"), so an `extern "C"` shim is the likely shape.

### Wave 3 — the MVP

**Lane A0** (ComboContext shape) is the serial section — keep it to about a day. **Lane A1** then fans out to two agents on the genuinely disjoint `games/oot/soh/GameExports_SingleExe.cpp` and `games/mm/2s2h/GameExports_SingleExe.cpp`. Then B, then C.

**Item-id representation — decide as an ADR merged before Lane A's first commit, not during.** It is a serialization-format decision, since it sets `sizeof(ComboContext)`. Do **not** bit-pack a 12-bit id plus 4-bit game tag into the existing `uint16_t`: a packed representation makes a raw integer read *almost* work, which is exactly how the #356 entrance-id leak behaved. Use an explicit tagged struct so a raw read fails **at compile time**. Size the array generously once (64+ entries) — the expensive part is re-versioning the format, not the bytes.

### Deferred out of 3.0

#386, #383's UIWidgets portion, #376 items 4-6, Lane D (logic), and **#369** — a whole-tree reformat conflicts with *every* open branch simultaneously, so run it in a quiet window between waves or slip it to 3.1 at near-zero cost, since `49444e30`/`b802120f` already established incremental enforcement.

---

## The governing constraint

**Validation bandwidth, not engineering throughput.** Hosted CI is ROM-free by construction; the `int-*` tier is `workflow_dispatch`-only and never runs on a PR. Exactly one human, with real ROMs, on a Windows-only box (no Docker, no WSL), can confirm gameplay behavior. `docs/ci-gameplay-repro-postmortem.md` exists because an entire crash class shipped through fully green CI.

Therefore: **every feature ships with a ROM-free lock in the `redship` CTest label, or it is not done.** Batch operator verdicts — design waves so one sitting covers maximum surface. Do not create lanes each needing their own hands-on session.

Second constraint: each PR triggers ~32 min Linux + ~46 min Windows CI. Eight simultaneous PRs is eight concurrent builds; #177 (Windows CI time) is open. Stagger if a wave is wide.

## Rules

- Work branches `claude/<description>`; separate PRs per lane; squash-merge only on fully green CI.
- Never merge red or partial CI. If blocked or uncertain, push the branch and report.
- Do **not** use `-Wl,--allow-multiple-definition` or otherwise weaken the Linux link to match Windows. Linux `ld` is the project's only working ODR gate: #375's tripwire cannot fail (baseline never committed) **and** would not catch the COMDAT class anyway, since `nm`-based intersection skips weak/COMDAT symbols. There is a comment in `src/common/mm_stubs.c` explaining this.
- If a fix is riskier than the bug, or an item proves latent rather than active, **say so and stop.** Several findings in this analysis changed shape under scrutiny, including the one a judge had already ranked first. A precise diagnosis with no patch is a good outcome.
- Verify load-bearing claims against source or the built binary before building on them. The single most consequential fact here — that MM's randomizer is absent from the binary — was asserted by one agent, accepted by a judge, and only confirmed by probing `redship.exe` directly.
