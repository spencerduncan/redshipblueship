# Playtest, September 2026: the single-bag build

**Build under test:** `main` at or after `0c8807b3` (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743), 2026-09-27), the first build where a paired world is filled as **one** world: an item that crosses games leaves its home pool, and one fill proves the pair can be finished.
Trackers: [#645](https://github.com/spencerduncan/redshipblueship/issues/645) (the single-bag fill), [#310](https://github.com/spencerduncan/redshipblueship/issues/310) (manual QA). Also read [`known-issues.md`](known-issues.md).

## 1. Get the build

No release has been cut since `v0.1.1-prealpha` (July), which is far older than this build. Use the CI build from `main`.

- **Click path:** open the [latest `main` builds on nightly.link](https://nightly.link/spencerduncan/redshipblueship/workflows/generate-builds/main). Take `rsbs-windows.zip` (an unzipped Windows package inside) or `rsbs-linux.zip` (`rsbs.appimage` and `readme.txt`). nightly.link serves the newest **successful** `generate-builds` run on `main`, so it lags a fresh merge by the ~20 minutes a run takes.
- **Command line:** find the run, then download one artifact:
  ```
  gh run list -R spencerduncan/redshipblueship --workflow generate-builds --branch main --status success --limit 3
  gh run download <run-id> -R spencerduncan/redshipblueship -n rsbs-windows   # or -n rsbs-linux
  ```
- **Check that you have the right build.** Settings > General shows the build's commit. It must be `0c8807b` or a later `main` commit. Anything older is a pre-switch build and tests none of this.
- Artifacts are kept 90 days. You supply both ROMs; the first launch extracts them ([`BUILDING.md`](BUILDING.md), [`mm-archive-setup.md`](mm-archive-setup.md)).

## 2. Fresh-file setup

1. **Back up, then start clean.** Copy your `Save/` folder aside.
2. **Create a new paired file.** Do not use an old one:
   - Paired files from before PR [#680](https://github.com/spencerduncan/redshipblueship/pull/680) (2026-09-17) are refused on load.
   - Paired files from #680 up to the switch are **not** refused. They load and play the world they were created with, which is the old overlay world (crossed items still in their home pool). Nothing in this playtest applies to them.
   - A pre-switch paired world cannot be re-created from its seed: the same seed now makes a different world.
   - A paired world's OoT spoiler loaded on its own at file select is refused with a toast (it would be an unfinishable solo OoT world). That refusal is expected.
3. **Before you create the file,** set everything you want to test: Combo > Cross-Game Rules, Combo > Windows > Toggle MM Randomizer Options (including its Tricks section), and OoT's own Randomizer settings. All of it freezes at creation.
4. **Write down** the seed, the build commit, and every non-default setting. You need them for any report.

## 3. Scenarios

Run them in order; later groups assume a created world. "Expect" is what the merged PRs state. Anything else is a finding.

### A. The single-bag fill ([#743](https://github.com/spencerduncan/redshipblueship/pull/743), [#744](https://github.com/spencerduncan/redshipblueship/pull/744), [#749](https://github.com/spencerduncan/redshipblueship/pull/749))

1. **Create a paired world with the shipped defaults; time it.** Expect a dimmed modal, "Creating Your Paired World", with a progress bar and "Placing item n of N in both worlds (round r, batch b)". On the development workstation, 30 real creations took 5.9-13.7 s (mean 7.2 s), all created on the first attempt. Record your time and your CPU. A creation that fails shows the toast "Not created: try a new seed or Majora's Mask options." and leaves no file. Report every failure with its seed.
2. **Open the spoiler's combo section:** Combo > Windows > Toggle Cross-Game Spoiler. Expect crossed items listed with their display names ("Lens of Truth", not enum names). On the pinned test seeds, 27-58 items crossed each way. Each side hosts at most 64.
3. **Walk into Majora's Mask and back** through the Happy Mask Shop door and the Clock Tower. Expect no refusal on arrival and the same world each time.
4. **Pick up a crossed item in each direction.** First an OoT item on a Termina chest, then an MM item on a Hyrule chest. Crossings are hosted only on chests. Read the textbox and the toast: each should name the item. Expect the item in your inventory when you are next in its home game.
5. **Find a trimmed heart host.** Both games together hold 100 heart items. The bag keeps 50 (44 Pieces of Heart and 6 Heart Containers) plus 1 Double Defense, and the 51 trimmed rows become junk in their home game. In the spoiler, find a vanilla Piece of Heart location that now lists a junk item, go there, and confirm it gives junk.
6. **Count hearts through a full clear,** if you get that far. The bar must stop at **20 hearts**, and no heart pickup should be dead. The creation-time walk measured 50 pickups, 0 dead.
7. **Finish the pair.** The frozen goal is `beat-both`, the only goal a production world can have today. A world the fill created should be finishable without reading the spoiler.

### B. Tricks ([#696](https://github.com/spencerduncan/redshipblueship/pull/696), [#713](https://github.com/spencerduncan/redshipblueship/pull/713), [#729](https://github.com/spencerduncan/redshipblueship/pull/729))

1. Open the MM Randomizer Options' **Tricks** section. Every trick is off by default. A trick with no logic binding yet draws disabled with a reason ([#697](https://github.com/spencerduncan/redshipblueship/issues/697)).
2. **A keg-gated edge.** With "Use Powder Kegs as Explosives" **off**, the spoiler should never require a Powder Keg for a boulder, hidden grotto or breakable wall. Create a second world from the same seed with it **on**: the world moves (the trick set is frozen into the identity), and a keg may now be required. Same test for "Deku Stick Fighting" (17 enemy kinds).

### C. Goals and the Cross-Game Rules page ([#740](https://github.com/spencerduncan/redshipblueship/pull/740), [#742](https://github.com/spencerduncan/redshipblueship/pull/742))

1. Before creation, Combo > Cross-Game Rules is editable: direction, the two pool-size sliders, item classes, Shared Ocarina, and "Reset Combo Rules" (asks first).
2. After creation, with the file loaded, the rows are disabled and hovering one shows SoH's "This setting is disabled because:" tooltip with "Already Decided". Changing a rule must never change the loaded world.
3. Goals: no goal selector ships yet. Every production world is `beat-both`; a paired triforce hunt is refused at generation ([#740](https://github.com/spencerduncan/redshipblueship/pull/740)). Surfacing the frozen goal is in flight.

### D. Menus ([#745](https://github.com/spencerduncan/redshipblueship/pull/745), [#746](https://github.com/spencerduncan/redshipblueship/pull/746), [#748](https://github.com/spencerduncan/redshipblueship/pull/748))

1. The Combo sidebar lists **Cross-Game Rules**, **Windows** and **Majora's Mask** (a saved selection of the old names "Cross-Game Windows" or "MM Enhancements" should carry over). Randomizer > Cross-Game shows only a "Moved to Combo" note.
2. **Combo > Majora's Mask:** the toggles, and MM's autosave interval slider (1-60 min, default 5; visible once Autosave is on). Check whether a non-default interval changes when MM's owl autosave fires. This has never been checked in play.
3. **MM Randomizer Options pane:** click through every section, and use its close button.
4. **Capability rows:** a row that is unavailable keeps its own name and explains itself only in the disabled tooltip (for example "No Paired World Yet"). A gray note sits above the group. No tooltip should show an issue number.

### E. Game over

1. **MM death** respawns you at the room entrance with no "Continue?" prompt. This is intentional ([#653](https://github.com/spencerduncan/redshipblueship/issues/653)).
2. **MM's game-over prompt** (Combo > Majora's Mask): turn it on, die, and confirm the kaleido art draws correctly. This artwork has never been observed in this build ([#694](https://github.com/spencerduncan/redshipblueship/issues/694)).
3. **OoT F10 revive:** once [#664](https://github.com/spencerduncan/redshipblueship/issues/664) lands (PR [#753](https://github.com/spencerduncan/redshipblueship/pull/753), open at writing), press F10 on OoT's game-over screen. MM should receive a revived bar, as MM already does in the other direction. Until then, expect the bar to arrive empty.

### F. Mods ([#732](https://github.com/spencerduncan/redshipblueship/pull/732), [`MODDING.md`](MODDING.md))

1. Put a loose texture in `<mods>/loose` (OoT) and one in `<mods>/mm/loose` (MM). Confirm each shows in its own game only. No loose texture has been checked on screen yet.
2. MM has no mod menu until [#706](https://github.com/spencerduncan/redshipblueship/issues/706) lands. When it does, check enable/disable and reorder for MM mods.

### G. Shared Ocarina ([#675](https://github.com/spencerduncan/redshipblueship/pull/675))

Turn Shared Ocarina on before creation. Get an ocarina in OoT, cross, and confirm MM has one. Then do the reverse on a second file.

## 4. What to report, and how

- **One issue per finding** at [`spencerduncan/redshipblueship`](https://github.com/spencerduncan/redshipblueship/issues), labelled `agent-tracking`. Title it with the symptom.
- **Body:** the build commit (Settings > General), the OS and GPU backend, the seed, every non-default setting (Cross-Game Rules, MM options, tricks, OoT rando settings), exact steps from file creation, and expected vs. actual. Add the scenario number (for example "A5").
- **Attach:** the spoiler JSON, a screenshot, and for crashes or refusals the `.log` file from the `logs/` folder in the game's working folder (a crash dump is written into that log). For a save problem, a copy of the `.redsave` and the slot's save files.
- Timings (A1) are useful even when they pass. Post them as one line each: seed, seconds, CPU.

## 5. Known gaps (from the merged PRs' "unverified" sections)

- **Never played:** everything in PRs [#736](https://github.com/spencerduncan/redshipblueship/pull/736)-[#749](https://github.com/spencerduncan/redshipblueship/pull/749) was verified by tests and harness captures only. That includes crossing pickups through the store, both toasts in real file-select play, the dimmed overlay over the live game image, and the renamed pages with a real saved config.
- **Creation budget:** on the workstation, no creation needed more than two fill batches. That bounds a budget failure below about 10%, not at zero ([#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Hints:** paired-world hints have no pair-level Way of the Hero or barren analysis. Crossing hosts are never hinted, and an OoT item that crossed is hinted as "Termina".
- **Surplus filler can be dropped** when more MM items land in Hyrule than OoT items leave. Only filler gives way.
- **Settings that re-seed without changing a rule:** changing a pool-size slider, or any item class other than Progression, gives a different world from the same seed.
- **Netplay and crossings share one 64-slot array.** A peer filling it while you collect crossings can make a pickup refuse (loudly).
- **Proof is conservative for shared quantities** (hearts, capacity upgrades). It could fail to prove on a profile nobody measured ([#744](https://github.com/spencerduncan/redshipblueship/pull/744)).
- **MM's vanilla-shop bomb bag** is still bought as a vanilla check. Buying it after the shared bag is full is one dead purchase.
- **Capability-row crossings** (souls and similar, when armed) have no in-game pickup text check. An armed-souls MM half can hit the fill's 30 s abort ([#729](https://github.com/spencerduncan/redshipblueship/pull/729)).
- **MM's own "Cross-game pairing REFUSED" toasts** are long and can run off-screen ([#749](https://github.com/spencerduncan/redshipblueship/pull/749) findings).
- **Plentiful and trap profiles** have not been played. Plentiful OoT seeds changed with [#739](https://github.com/spencerduncan/redshipblueship/pull/739).
- **Renderers:** only OpenGL was captured locally. DX11 and llvmpipe renders come from CI, and macOS is not built.
