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
- **Check that you have the right build.** Open the menu (Esc), then Settings > General; the About column shows `Commit:`. It must be `0c8807b` or a later `main` commit. Anything older is a pre-switch build and tests none of this.
- Artifacts are kept 90 days. You supply both ROMs; the first launch extracts them ([`BUILDING.md`](BUILDING.md), [`mm-archive-setup.md`](mm-archive-setup.md)).

## 2. Fresh-file setup

1. **Back up, then start clean.** Copy your `Save/` folder aside. Everything below lives in the game's working folder (`SHIP_HOME` on Linux when it is set): `Save/`, `Randomizer/` (spoilers) and `logs/`.
2. **Use a new paired file.** Do not use an old one:
   - Paired files from before PR [#680](https://github.com/spencerduncan/redshipblueship/pull/680) (2026-09-17) are refused on load.
   - Paired files from #680 up to the switch are **not** refused. They load and play the world they were created with, which is the old overlay world (crossed items still in their home pool). Nothing in this playtest applies to them.
   - A pre-switch paired world cannot be re-created from its seed: the same seed now makes a different world.
   - A paired world's OoT spoiler loaded on its own at file select is refused with a toast (it would be an unfinishable solo OoT world). That refusal is expected.
3. **Set everything first.** Open the menu (Esc) and set Combo > Cross-Game Rules, Combo > MM Randomizer and Combo > MM Tricks, and OoT's own Randomizer settings. All of it freezes when you press Generate in step 4.
4. **Generate the seed.** Randomizer > General. For a seed you choose, tick "Manual seed entry" and type it in the Seed box; leave the box empty for a random one. Press **Generate Randomizer** (enabled only on file select, with no file loaded). The spoiler JSON is written to `Randomizer/<seed hash>.json`; the "Spoiler File:" line names it.
5. **Create the file.** On file select, pick an empty slot, choose the **Randomizer** quest, then **Start Randomizer** (grayed out until a seed exists), enter a name and confirm. The paired world is filled now, behind the creation overlay (A1).
6. **Write down** the seed, the build commit, and every non-default setting. You need them for any report.

## 3. Scenarios

Run them in order; later groups assume a created world. "Expect" is what the merged PRs state. Anything else is a finding.

### A. The single-bag fill ([#743](https://github.com/spencerduncan/redshipblueship/pull/743), [#744](https://github.com/spencerduncan/redshipblueship/pull/744), [#749](https://github.com/spencerduncan/redshipblueship/pull/749))

1. **Create a paired world with the shipped defaults; time it.** Expect a dimmed modal, "Creating Your Paired World", with a progress bar and "Placing item n of N in both worlds (round r, batch b)". On the development workstation, 30 real creations took 5.9-13.7 s (mean 7.2 s), all created on the first attempt. Record your time and your CPU. A creation that fails shows the toast "Not created: try a new seed or Majora's Mask options." and leaves no file. Report every failure with its seed.
2. **Read the crossings in the spoiler JSON,** under `combo.crossingStore`: `ootItemsInMM` lists OoT items on Termina checks, `mmItemsInOoT` MM items on Hyrule checks. Each row has `hostCheckName` and `itemName`, and the item names are display names ("Lens of Truth"). On the pinned golden worlds, 27-29 OoT items crossed into Termina and 52-58 MM items into Hyrule; each side hosts at most 64. At a Hyrule host, the OoT half's `locations` entry shows the stand-in "Blue Rupee"; the crossing row is what the chest must give. Then open Combo > Windows > Toggle Cross-Game Spoiler and the Combo Tracker: both list the same crossings in both directions, with check and item names, and each check name led by a collected or open box read from each game's save (Majora's Mask's as of the last switch or save, or as of file creation before you first enter Termina) ([#755](https://github.com/spencerduncan/redshipblueship/issues/755)). A crossing in the JSON that the windows do not list, or the reverse, is a finding.
3. **Walk into Majora's Mask and back** through the Happy Mask Shop door and the Clock Tower. Expect no refusal on arrival and the same world each time.
4. **Pick up a crossed item in each direction.** Pick a chest from A2's lists: first an OoT item on a Termina chest, then an MM item on a Hyrule chest. Crossings are hosted only on chests. Read the textbox and the toast: each should name the item. Expect the item in your inventory when you are next in its home game. If `mmItemsInOoT` lists an MM Heart Container, Double Defense or Bombchus, pick that one up too: whether MM's give path delivers those three was never checked ([#738](https://github.com/spencerduncan/redshipblueship/pull/738)).
5. **Count the heart items in the spoiler JSON.** Health is one shared bar, so on the shipped defaults the two games' 76 Pieces of Heart, 15 Heart Containers and 2 Double Defense are trimmed to one set for the pair ([#744](https://github.com/spencerduncan/redshipblueship/pull/744)). Count across three places: the OoT half's `locations` ("Piece of Heart", "Piece of Heart (WINNER)", "Heart Container", "Double Defense"), `combo.mm.checks` (`RI_HEART_PIECE`, `RI_HEART_CONTAINER`, `RI_DOUBLE_DEFENSE`, and the same OoT names with " (Ocarina of Time)"), and `combo.crossingStore.mmItemsInOoT` ("Heart Piece", "Heart Container", "Double Defense"). Expect exactly **44 pieces, 6 containers and 1 Double Defense**: 14 pieces and 4 containers of OoT's, 30 and 2 of MM's, and the Double Defense from either game. Three worlds from #743's creation rows counted exactly that. Any other total on the shipped defaults is a finding. (The container count follows the larger starting health: 5 at four hearts, 7 at two.)
6. **Count hearts through a full clear,** if you get that far. The bar must stop at **20 hearts**, and no heart pickup should be dead (#744's walk: 50 heart pickups, 0 dead).
7. **One dead hookshot pickup is expected.** OoT's pool has 2 progressive hookshots and MM's has 1, and the games top out at different tiers, so the family is kept whole: of the three pickups, exactly one does nothing, in any order. A second dead one is a finding.
8. **Finish the pair.** The frozen goal is `beat-both`, the only goal a production world can have today. A world the fill created should be finishable without reading the spoiler.

### B. Tricks ([#696](https://github.com/spencerduncan/redshipblueship/pull/696), [#713](https://github.com/spencerduncan/redshipblueship/pull/713), [#729](https://github.com/spencerduncan/redshipblueship/pull/729))

1. Open **Combo > MM Tricks**. Every trick is off by default. A trick with no logic binding yet draws disabled with a reason ([#697](https://github.com/spencerduncan/redshipblueship/issues/697)).
2. **A keg-gated site: the Lone Peak Shrine boulder chest** (`RC_LONE_PEAK_SHRINE_BOULDER_CHEST` in the spoiler's `combo.mm.checks`). With "Use Powder Kegs as Explosives" **off**, logic opens that boulder only with Bombs, Bombchus or the Blast Mask. So that chest must never hold your first explosive (`RI_PROGRESSIVE_BOMB_BAG`, `RI_BOMBCHU*`, `RI_MASK_BLAST`) when no other one is reachable first. More generally, a trick-off world must never leave a Powder Keg as your only way through a boulder, hidden grotto or breakable wall; if it does, report the site.
3. **The world moves.** Create a second world from the same manual seed with the trick **on**. The placements should differ (the trick set is frozen into the identity), and a keg may now be required. Do the same with "Deku Stick Fighting".

### C. Goals and the Cross-Game Rules page ([#740](https://github.com/spencerduncan/redshipblueship/pull/740), [#742](https://github.com/spencerduncan/redshipblueship/pull/742))

1. Before you press Generate Randomizer, Combo > Cross-Game Rules is editable: direction, the two pool-size sliders, item classes, Shared Ocarina, and "Reset Combo Rules" (asks first).
2. Once you press Generate Randomizer the record is frozen: the rows are disabled and hovering one shows SoH's "This setting is disabled because:" tooltip with "Already Decided". Changing a rule must never change the loaded world.
3. Goals: no goal selector ships yet. Every production world is `beat-both`; a paired triforce hunt is refused at generation ([#740](https://github.com/spencerduncan/redshipblueship/pull/740)). Surfacing the frozen goal is in flight.

### D. Menus ([#745](https://github.com/spencerduncan/redshipblueship/pull/745), [#746](https://github.com/spencerduncan/redshipblueship/pull/746), [#748](https://github.com/spencerduncan/redshipblueship/pull/748))

1. The Combo sidebar lists **Cross-Game Rules**, **Windows**, **Majora's Mask**, **MM Randomizer** and **MM Tricks** (the last three together, in that order; **MM Mods** also appears). A saved selection of the old names "Cross-Game Windows" or "MM Enhancements" should carry over. Randomizer > Cross-Game shows only a "Moved to Combo" note.
2. **Combo > Majora's Mask:** the toggles, and MM's autosave interval slider (1-60 min, default 5; visible once Autosave is on). Check whether a non-default interval changes when MM's owl autosave fires. This has never been checked in play.
3. **Combo > MM Randomizer and Combo > MM Tricks** (pages since 2026-09-27; they were a pop-out window). See "MM options as pages" below.
4. **Capability rows:** a row that is unavailable keeps its own name and explains itself only in the disabled tooltip (for example "No Paired World Yet"). A gray note sits above the group. No tooltip should show an issue number.

### E. Game over

1. **MM death** reloads you at the area entrance with three hearts and no "Continue?" prompt. This is intentional ([#653](https://github.com/spencerduncan/redshipblueship/issues/653)).
2. **MM's game-over prompt** (Combo > Majora's Mask): turn it on, die, and confirm the kaleido art draws correctly. This artwork has never been observed in this build ([#694](https://github.com/spencerduncan/redshipblueship/issues/694)).
3. **OoT F10 from the game-over screen:** die in OoT and press F10 on the game-over screen. Until [#664](https://github.com/spencerduncan/redshipblueship/issues/664) lands (PR [#753](https://github.com/spencerduncan/redshipblueship/pull/753), open at writing), expect MM to arrive with **one heart**, the arrival floor. After it lands, expect **three hearts**, or full capacity with the FullHealthSpawn enhancement on.

### F. Mods ([#732](https://github.com/spencerduncan/redshipblueship/pull/732), [`MODDING.md`](MODDING.md))

1. Put a loose texture in `<mods>/loose` (OoT) and one in `<mods>/mm/loose` (MM). Confirm each shows in its own game only. No loose texture has been checked on screen yet. If you can, also try a loose custom asset with a `.meta` file; nothing covers that path.
2. MM has no mod menu until [#706](https://github.com/spencerduncan/redshipblueship/issues/706) lands. When it does, check enable/disable and reorder for MM mods.

### G. Shared Ocarina ([#675](https://github.com/spencerduncan/redshipblueship/pull/675))

Turn Shared Ocarina on before creation. Get an ocarina in OoT, cross, and confirm MM has one. Then do the reverse on a second file.

### H. MM options as pages (ADR 0004's 2026-09-27 host amendment)

MM's randomizer options and tricks moved out of the pop-out window into two Combo pages. Nothing about what they do changed. Check the move:

1. **Where each option lives.** Combo > **MM Randomizer** has two columns. Column 1: a gray note ("No paired world yet. ..."), while Ocarina of Time is running a second gray note ("Majora's Mask is suspended; these options stay editable."), two orange warnings, then **Logic & Conditions** (Logic, Dungeon Access, Trials Access, the four Moon/Majora access counts, and the retired "Majora Access: Remains", disabled with "Option is Retired") and **Shuffle Options** (cows, owls, freestanding items, pots, snowballs, boss remains, Gold Skulltula tokens and their minimum, stray fairies, frogs, shops, Tingle maps, crates, barrels, grass). Column 2: **Items** (Plentiful Items, traps and their count, boss and enemy souls, enemy drops, ocarina buttons, swim, Triforce Hunt and its two counts, Shuffle Time, Time Progression, Final Hours Start Time shown as HH:MM), **Starting Items** (hearts, consumables, wallet, maps and compasses), **Hints** (six hint toggles), and **Reset MM Randomizer**, which asks first. Combo > **MM Tricks** is shaped like Randomizer > Tricks/Glitches: a filter, Disable All and Enable All, and a Disabled/Enabled table by area. Move a trick with its arrow. A trick the logic does not support yet stays in Disabled with a greyed arrow; hovering its name says why.
2. **Every option is there once.** The page should hold the same options the window did (47 rows). Search the menu for "Shuffle Cows": you should see both OoT's and MM's rows, and ticking one must not tick the other.
3. **What froze.** Set a few options and tricks, then press Generate Randomizer. Every row on both pages goes grey; hovering one shows "This setting is disabled because:" and "- Already Decided"; both pages' notes read "Already decided when this world was created. ..."; Reset, the arrows, Disable All and Enable All are greyed. The area headers still open and close. Return to the title screen: the pages are editable again.
4. **What the Windows page still offers.** Combo > Windows holds only live-play tools: Toggle Cross-Game Spoiler, Toggle Combo Tracker, Toggle MM Item Tracker, Popout MM Item Tracker Settings, Toggle MM Check Tracker and Popout MM Check Tracker Settings. There is no "Toggle MM Randomizer Options" any more. A config that last had the options window open keeps an unused `gCombo.Windows.MMOptions` entry; it does nothing.
5. **A second world.** With a trick turned on from Combo > MM Tricks, create a world, note the placements, then create another from the same manual seed with the trick off. They should differ, as they did from the window.

## 4. What to report, and how

- **One issue per finding** at [`spencerduncan/redshipblueship`](https://github.com/spencerduncan/redshipblueship/issues), labelled `agent-tracking`. Title it with the symptom.
- **Body:** the build commit (Settings > General), the OS and GPU backend, the seed, every non-default setting (Cross-Game Rules, MM options, tricks, OoT rando settings), exact steps from file creation, and expected vs. actual. Add the scenario number (for example "A4").
- **Attach:** the spoiler JSON, a screenshot, and for crashes or refusals the `.log` file from the `logs/` folder in the game's working folder (a crash dump is written into that log). For a save problem, a copy of the `.redsave` and the slot's save files.
- Timings (A1) are useful even when they pass. Post them as one line each: seed, seconds, CPU.

## 5. Known gaps (from the "Unverified" / "Not verified" sections of the merged PRs #723-#749)

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
- **Plentiful, scarce, minimal and trap profiles** have not been played. Plentiful OoT seeds changed with [#739](https://github.com/spencerduncan/redshipblueship/pull/739); scarce and minimal were not even generated there.
- **Clock shuffle:** a clock-shuffle world's MM crawl takes an unmeasured path, and no golden pins it ([#729](https://github.com/spencerduncan/redshipblueship/pull/729)).
- **Crossed MM hearts, Double Defense and Bombchus** may cross since [#738](https://github.com/spencerduncan/redshipblueship/pull/738); their in-game give was never checked (A4).
- **Hookshot:** one dead pickup by design (A7).
- **Loose mods:** no loose texture was checked on screen, and a custom asset with a `.meta` file is uncovered ([#732](https://github.com/spencerduncan/redshipblueship/pull/732)).
- **Small windows:** the Cross-Game Rules page and the MM options (then a pane, now Combo > MM Randomizer) were not rendered at 960x704 or below 800 px wide, where the columns collapse ([#742](https://github.com/spencerduncan/redshipblueship/pull/742), [#748](https://github.com/spencerduncan/redshipblueship/pull/748)). Try one.
- **In-game crossing views:** the Cross-Game Spoiler window and the Combo Tracker read the crossing store since #755; they were not opened in play. Majora's Mask's collected state shows as of the last switch or save.
- **Renderers:** only OpenGL was captured locally. DX11 and llvmpipe renders come from CI, and macOS is not built.
