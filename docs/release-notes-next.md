# Release notes draft: v0.2.0-prealpha ("Trinity Alfa")

**Status:** draft for the operator to edit. Nothing here is released.
**Version:** `v0.2.0-prealpha`, codename "Trinity Alfa" (`src/common/rsbs_version.h`, PR
[#946](https://github.com/spencerduncan/redshipblueship/pull/946)). Under [`VERSIONING.md`](VERSIONING.md) that is a
`y` bump: major features and fixes without a new `oot.otr` (the archive version, `project(Ship VERSION 9.1.1)`, is
unchanged).
**Built from:** `main` at `7e44b8a0` (2026-10-17, PR [#960](https://github.com/spencerduncan/redshipblueship/pull/960)):
`851b1ee9` (2026-10-12, PR [#940](https://github.com/spencerduncan/redshipblueship/pull/940)) plus the code merged
after it: PR [#945](https://github.com/spencerduncan/redshipblueship/pull/945) (the first launch also offers Majora's
Mask's ROM extraction), PR [#956](https://github.com/spencerduncan/redshipblueship/pull/956) (the Happy Mask Shop door
opens at night), PR [#957](https://github.com/spencerduncan/redshipblueship/pull/957) (a crossing refused for a
missing archive no longer crashes), the version bump of PR [#946](https://github.com/spencerduncan/redshipblueship/pull/946)
(`ece56551`), PR [#958](https://github.com/spencerduncan/redshipblueship/pull/958) (the goldens pin more; static hints
in a fixed order), PR [#959](https://github.com/spencerduncan/redshipblueship/pull/959) (the creation budget counts
fill rounds) and PR [#960](https://github.com/spencerduncan/redshipblueship/pull/960) (the fingerprint and build
beside the seed icons). Everything else merged after `851b1ee9` is documentation or agent tooling (PR #947). After
the operator's playtest of `7e44b8a0`, PR [#974](https://github.com/spencerduncan/redshipblueship/pull/974) (2026-10-18)
makes a paired world shuffle the Happy Mask Shop's masks and drop the Mask Shop Hint, and moves generated worlds. The
version is unchanged: `v0.2.0-prealpha`, "Trinity Alfa".
**Covers:** everything since `v0.1.1-prealpha` (2026-07-03). That tag's commit, `eab838c1`, is not an
ancestor of `main`, so "since" is by date: 360 commits on `main` after 2026-07-03. The last 92 of them are
the pull requests of the 2026-09-30 wave ([#802](https://github.com/spencerduncan/redshipblueship/pull/802) to
[#940](https://github.com/spencerduncan/redshipblueship/pull/940)).

Before tagging: the tag must be `v0.2.0-prealpha`, the `RSBS_VERSION_STRING` that `src/common/rsbs_version.h` now
builds (its comment says it "must equal the git release tag"); the `create-release` job refuses any other tag. Since
PR #900 a build's window title adds its commit: `v0.2.0-prealpha+<commit> (Trinity Alfa)` from the bump on,
`v0.1.1-prealpha+<commit> (Morpheus Bravo)` before it.

Numbers in parentheses are pull requests. The full list of open problems is
[`known-issues.md`](known-issues.md); the playtest checklist is [`playtest-2026-09.md`](playtest-2026-09.md).

---

## Read this first

- **Saves from earlier builds.** A paired file created before 2026-10-10 is refused by this build: pressing A
  on it in the file select plays the error sound, shows "Refused: Majora's Mask options differ" and keeps you
  on the file list (#895). Erase it and create a new file. Pre-alpha saves are disposable; back up any file
  you care about.
- **Seeds.** A seed string and settings no longer make the world they made on an older build. Generated worlds
  moved in this wave (#840, #874, #895, #974; #958 for a typed seed with non-ASCII characters) and before it (#729,
  #763, #774). Sharing a seed means sharing the same seed string, OoT settings, MM options, Cross-Game Rules, extracted
  ROM type (vanilla or Master Quest) and build. The file select now prints the paired world's fingerprint and the
  build's short commit beside the five seed icons; the icons alone cover only the seed string and OoT's settings
  (#960).
- **The first launch now also asks for your Majora's Mask ROM.** When no `mm.o2r` is found, the first-launch
  setup that generates `oot.o2r` now also offers to generate `mm.o2r` from your Majora's Mask ROM, with the same
  popups and progress dialog. If you already have `oot.o2r` but no `mm.o2r`, this is a new question on your next launch;
  "Not now" starts Ocarina of Time alone, as before, and the question returns at the next launch until
  `mm.o2r` exists (#945).
- **The operator played this build on 2026-10-17** (`7e44b8a0`, on DirectX 11): a new paired file, shops in both
  games selling crossed items, the Happy Mask Shop crossing both ways, play in Termina and the Combo Item Tracker
  overlay in both games. The first-launch extraction was checked on 2026-10-16 (#945). The findings are filed
  (issues #962 to #971), none is a crash, and the ones a player sees are under Known limitations. Not played yet:
  a crossing after quitting and relaunching the game or after a save made during play, the cross-game warp songs,
  "[Both Games] Shared Kokiri Sword" on, MM's game-over prompt, OoT's icon in MM's get-item textbox, and OpenGL
  compared with DirectX 11. An earlier build's two findings (2026-10-09) are fixed (#887, #889, #895).

## The combo randomizer

- **One seed makes one paired world.** Generating an Ocarina of Time randomizer seed also generates the
  Majora's Mask world, and every randomizer file is a paired file (#680, #775). Both halves are generated
  together, once, when you create the file; arriving in Termina never generates anything. A failed generation
  fails the file creation itself, with a toast, and leaves no half-made file (#680, #743).
- **One bag of items for both games.** Items cross both ways, and an item that crosses leaves its home game's
  pool. One fill places the bag across both games' checks and proves the chosen goal can be reached with both
  games' logic, Glitchless by default (#581, #725, #728, #738, #743). A progress overlay shows while the world
  is filled ("Placing item n of N in both worlds"; #707, #749).
- **Shared resources.** Rupees, hearts, magic, ammo and the Hookshot's tier are shared between the games (#540,
  #554, #555, #556). Health is one bar that stops at 20 hearts, so the pair holds 44 Pieces of Heart, 6 Heart
  Containers and 1 Double Defense, and a capacity upgrade with the same top tier in both games is counted once
  (#744). One shared Triforce piece count (#740).
- **Goals.** Combo > Cross-Game Rules > Goal offers OoTMM's goals: Ganon & Majora (the default), Any Final
  Boss, Ganon, Majora. Meeting the goal ends the paired game; a final boss beaten earlier is recorded and puts
  you back in its game (#760, #769, #775).
- **Majora's Mask tricks.** 86 MM trick keys, 37 of them bound to a real logic edge, all off by default. Deku
  Stick combat now needs its trick (#686, #696, #703, #713, #729, #763).
- **The Happy Mask Shop door is never shuffled** by OoT's entrance randomizer: it is always the door to
  Termina (#691).
- **The Happy Mask Shop's masks are always in the item pool,** as in OoTMM. Because its door leads to Termina,
  the shop's room never loads, so a paired world is generated with "Mask Quest" as "Shuffle" (all eight masks
  in the pool; the pause menu's mask slot cycles through the masks you have) and "Mask Shop Hint" off, whatever the
  settings say. Both rows are greyed with the reason. Before this, "Vanilla" (the default) and "Completed" (the
  shipped presets) let the logic count masks nobody could get, so the two Deku Theater checks could hold an
  item the seed needed (#974).
- **Majora's Mask's start in a paired world is the Ocarina of Time and the Song of Time.** The Kokiri Sword and
  the Hero's Shield are in MM's item pool, as in OoTMM (#895). A new rule, "[Both Games] Shared Kokiri Sword"
  (off by default), makes the two games' Kokiri Swords one item (#910).
- **Retired rules.** "Shared Ocarina" is gone; a world created with it on loads with it ignored (#889). The
  "Max OoT/MM Items" sliders and the "OoT Classes" / "MM Classes" rows are gone: they changed the seed without
  changing a rule (#817, #839). Combo > Cross-Game Rules is now Goal, Crossing Direction, Shared Kokiri Sword
  and Reset, in one column (#907).

## Cross-game items and models

- **Foreign items keep their identity.** An Ocarina of Time item found in Termina shows its own name, its OoT
  icon in MM's textbox and its 3D model in MM's get-item cutscene. A Majora's Mask item found in Hyrule shows
  its MM model in OoT's get-item cutscene and is given on your next arrival in Termina (#551, #761, #822, #829,
  #845). An OoT song found in Termina shows a tinted note (#853).
- **The text says which game an item belongs to:** "You found the Song of Time (OoT)!" in Termina, "You found
  the Postman's Hat (MM)!" in Hyrule (#884).
- **One announcement per item.** An item is presented when you pick it up; crossing into its game no longer
  toasts it a second time (#887).
- **Chest appearance.** With "Chest Size & Texture Matches Contents" on, a chest holding the other game's item
  is dressed by that item's type, as OoTMM does, not always as a major chest. OoT has no fairy or mask chest,
  so an MM stray fairy or mask shows as major there (#892).
- **The models come from your own ROMs.** The two curated archives (`redship-oot.o2r`, `redship-mm.o2r`) are
  built on your machine from your extracted game data at launch, and the result is written to `logs/*.log`
  (#810, #827, #867, #905).

## Shops

- **Shops sell the other game's items, in both directions.** With OoT's "Shop Shuffle" on, an OoT shop shelf
  can hold an MM item (#831). An MM shop slot can hold an OoT item, sold once for the whole game (#840). Also
  OoT's Business Scrubs with "Scrubs Shuffle" (#854), MM's Tingle with "Shuffle Tingle Maps" (#860), OoT's
  merchants with "Shuffle Merchants" (#870), and the Treasure Chest Game's prize in every world (#874).
  Shelves show the item's model (#858), and MM's spoiler lists such a slot's price (#893).

## Hints

- **One shared area vocabulary** for both games' hints (#908).
- MM's hints name a check's area with OoTMM's wording, "in the Southern Swamp", not its scene, so a grotto no
  longer reads "Lone Peak Shrine & Grottos" (#915).
- OoT's hints name the exact MM area an OoT item crossed to, not "Termina" (#916).
- MM's hints name the Hyrule area of an MM item placed in Hyrule, and MM's stones can hint it (#920).
- Gossip stones in both games can hint a check that holds the other game's item, naming that item (#814, #922).
- Foolish hints work in paired worlds again, in both games (#924).

## Seeds and determinism

- **The same seed and settings make the same world, and the same hints, on every machine.** Static hints are
  built in a fixed order, so the same seed reads the same hint text on Windows and Linux. Before, a world where a
  hinted item sat on a location spanning several hint areas could read different static-hint text on the two
  (#958).
- **File creation's budget counts the fill's work instead of time** (1250 fill rounds per attempt, 2500 per
  creation), so the same seed and settings are created, or refused, on every machine. Before, a slow or busy
  machine, or Linux (where the old clock measured CPU time), could refuse a seed another machine created (#959).
- **A typed seed that contains non-ASCII characters** (accents, emoji) now generates a different world than it
  did in earlier builds on Windows and x86 Linux, and the same world on every platform. ASCII seeds and generated
  seeds are unchanged (#958).
- The Combo Tracker, the Cross-Game Spoiler and the Combo settings pane print the paired seed as the seed string
  you typed (or the ten digits a blank seed field generated), which you can type back in, instead of a number
  (#960).
- The determinism goldens now pin hint text, shop prices, starting items and the entrance table for both games,
  and a new golden pins a world with entrances shuffled (#958).

## Trackers

- **One Check Tracker for both games.** Combo > Windows > Toggle Combo Tracker shows both games' checks: live
  for the game you are playing, as of the last game switch or save for the other (#553, #766, #811). Its check
  lists group by area with Ship of Harkinian's colours, search and found items, and the live game's checks can
  be marked skipped (#842, #864).
- **One Item Tracker overlay for both games.** Combo > Windows > Toggle Combo Item Tracker draws each game's
  own icon grid and the shared pool (#868, #875, #876). Its settings are on Combo > Tracker Settings (#890).
- The native check trackers name a crossed item, for example "(Bunny Hood (MM))", not the Blue Rupee or Junk
  the chest holds (#813).
- Combo > Windows keeps three buttons: Cross-Game Spoiler, Combo Tracker and Combo Item Tracker (#894). An MM
  tracker that an old config left open closes from its right-click menu (#917).
- The trackers no longer carry explanatory captions (#929).
- Ocarina of Time's Save Editor, Message Viewer and its other save windows, and its console commands, do
  nothing while you are in Majora's Mask, instead of writing MM's save or crashing (#812, #823).

## Saves and crossings

- **Every crossing saves the file.** Walking through the Happy Mask Shop or Clock Tower door, or pressing F10,
  saves the whole file, as OoTMM does at every game switch (#846). Closing the game after a crossing no longer
  loses it.
- **The Happy Mask Shop door opens at night in a paired world,** so the crossing into Termina works at any hour as
  child Link. On a vanilla file it still shows its "Apply during the day" sign, and the other night shops stay
  closed (#956). The crossing is child-only: as adult the Market is in ruins and has no Happy Mask Shop door.
- **A crossing into a game that cannot start is refused, and the game you are in keeps running.** If its archive
  is missing (for example after "Not now" to the Majora's Mask extraction, or with no `2ship.o2r`), the door leaves
  Link just outside it (the Market in Hyrule, South Clock Town in Termina), F10 does nothing, and a message says
  why. Before, such a refusal crashed the game (#957).
- After such a reload Link wakes at a safe place (#871), OoT's collected checks stay collected (#856, #898),
  and an older save can never land over a newer one (#896).
- **Moon crash.** A moon crash in a paired file restores the file's last save instead of leaving you on day 4,
  and no longer rolls Hyrule back; a Termina chest you opened before leaving stays open (#772, #789, #866).
- **What you did in the scene you leave through the door is kept** (#650), and a dead health bar is revived
  before an F10 crossing (#650, #753).
- **The file's own rules win.** Loading a paired file puts its own Cross-Game Rules, MM options and tricks
  back into the menu, and a toast names what it reset (#787).
- **A paired file that cannot be played paired is never opened.** The file select refuses it with one toast
  that starts "Refused:" and keeps you on the file list; Combo > Save Files shows the same words (#848, #869,
  #872). This covers a file with a missing or damaged cross-game record, a vanilla MM half or another pair's,
  and a record that belongs to another file (#911).
- One generation makes one file: after a file is created, "Start Randomizer" greys out until you generate a
  new seed (#934). One spoiler per paired world, and no world writes over another's (#937). A paired spoiler
  is no longer loaded, with a toast, at every launch (#939).
- An MM save that cannot be written says so: "Save failed: ..." (#921). MM cannot write a new save over an
  open paired file (#928).
- MM's half carries the name you typed in OoT (#782). The first arrival in Clock Town starts at 06:00 with the
  dawn sequence (#648).
- Combo > Save Files shows each slot's state (#794).

## Menus and settings

- **A Combo section in the menu:** Cross-Game Rules, Windows, Majora's Mask, MM Randomizer, MM Tricks, MM Mods,
  Save Files and Tracker Settings (#672, #687, #695, #764, #771, #794, #890). Settings are pages; windows are
  for tools you use while playing (#771). Our pages use Ship of Harkinian's own widgets (#742, #745, #746,
  #748, #752, #759).
- **Majora's Mask enhancements** on Combo > Majora's Mask: the game-over prompt, Better Song of Double Time,
  skipping the Song of Time cutscenes, and MM's autosave interval (#695, #730). MM's clock-shuffle option
  works (#679).
- **MM mods and texture packs load** from `mods/mm/`, in their own order on Combo > MM Mods; loose folders
  load for both games (#704, #716, #732, #764).
- Toasts fit the default 640x480 window (#904, #912).
- Console commands no longer end the game on input they cannot parse or when run at the wrong time, and
  `gen_rando` can no longer regenerate the world of a file you are playing (#847, #914, #932, #933, #938).

## Fixes since v0.1.1-prealpha

- Windows builds no longer corrupt the heap on every exit (#413).
- Crashes on the way back from Majora's Mask, at MM boot and in MM's first room (#345, #352, #356, #357).
- Audio no longer wedges across a game switch (#398, #412, #416).
- Network > Anchor no longer pushes the game view off screen in a wide window (#651).
- Seeds with song shuffle and with stock settings generate (#339, #340); gossip stones no longer read "No
  Item" after a crossing (#481); the Fire Temple boss key hint names the right key (#359).
- A Plentiful item pool no longer wraps the wallet inside the fill (#739).
- Leaving a game no longer leaves its actors' state behind, and the save writes those actors make on exit are
  applied (#751, #767, #792, #852).

## For testers

- The window title and `redship --version` name the commit the build was made from, for example
  `RedShipBlueShip v0.2.0-prealpha+<commit> (Trinity Alfa) (OpenGL)`, and each build writes its own log file
  in `logs/` (#900).

## Known limitations

- macOS is not supported.
- Triforce Hunt is listed as a goal but refused for a paired world. OoT's hints have no pair-level Way of the
  Hero.
- Generation can still fail at file creation ("Not created: try a new seed or other options."); no partial
  file is left. Whether a seed and settings fail is the same on every machine (#959).
- On DirectX 11, the default backend on a fresh Windows install, F10 does not switch games: Windows takes it as
  the menu key and the game seems to pause until another key is pressed. Cross through the doors, or switch to
  OpenGL (issue #955).
- At most 64 crossings each way, and netplay grants share those slots.
- Until Majora's Mask has started in a session, the Item Tracker overlay lists MM's items by name instead of
  by icon (issue #963).
- In narrow windows Settings > General's Boot Sequence dropdown covers its label; Ship of Harkinian draws it
  the same way (#923).
- The config file is still `shipofharkinian.json`.
- A Majora's Mask item whose model MM draws with custom code (the boss remains, stray fairies, the seahorse,
  the Moon's Tear, a bottled fairy) shows as the "?" stand-in on Ocarina of Time's shop shelves and in its get-item
  cutscene; buying or getting it works (issue #964).
- Frame interpolation (Settings > Graphics) smooths Ocarina of Time but not Majora's Mask, which stays at 20
  frames a second (issue #965).
- The Item Tracker overlay's Shared column updates at a save or a crossing, not live (issue #971), and the
  overlay's default position covers the hearts until you move it: turn on Enable Dragging, or set Window Type to
  Window, on Combo > Tracker Settings (issue #962).
- Not tested by anyone: cross-game warp songs, networking, OpenGL compared with DirectX 11, and a crossing after
  quitting and relaunching the game or after a save made during play.

## Open decisions (operator)

- #645: its two open items, O2 (the per-direction crossing-edge expressions, which no merged PR records) and
  O11 (the strictness presentation, which the Goal row of #760 does not record), and whether to close the
  epic.
- #497: build or close. PR #687 delivered its steps 3, 5 and 6 (capability gating, the shared-intent marker
  and the Combo section) and the `SetMenu`-count invariant; all that remains is ADR 0004 §4's pairing status,
  entrance links and hot-swap pages, which the ADR still lists as unbuilt.
- #573: cross-game entrance randomization.
- #574: networking / multiworld.
- #460: the rest of netplay increment 1a beyond the hygiene slice (#821).
- #599: close or keep. MM's regular-save and owl-save flash indexing has no effect in the single executable:
  MM's flash write ends in a no-op (`z_sram_NES.c` -> `sys_flashrom.c` -> `mm_save_manager_stubs.c`), and MM's
  saves are committed to the unified `.redsave` instead.
- Closing the trackers #500 and #492 and the QA pass #310.
- Playtests #694 (MM's game-over prompt) and #607 (an OoT item's icon in MM's textbox).
- Replies to the community reports #634, #635 and #636 (fixed by #651, #650 and #648).
- #577: every child has landed; close the epic.
- PR #928's question: should the new-save guard also cover vanilla cross-game sessions?
- #855: which setting the Business Scrub's offer reads (inherited from Ship of Harkinian).

## Run record

The closing smoke at `851b1ee9` (2026-10-12; Windows, OpenGL, ROM archives mounted) passed `redship` 179 of
179, `rando` 43 of 43 (all four golden-world rows), `ui` 2 of 2 with the parity lint, `integration` 14 of 14
and `integration-soak` 1 of 1. The gates of the three determinism pull requests (#958, #959, #960; 2026-10-17,
same setup) ended at `7e44b8a0`, the code of this release, with `redship` 184 of 184, `rando` 49 of 49 (the golden-world
rows among them, over five golden files now), `ui` 2 of 2 with the parity lint, `integration` 17 of 17 and `integration-soak` 1 of
1. There, 10 of 10 sample paired creations succeeded on the first attempt (258 fill rounds each against the
1250-round cap; 6.2-8.6 s, mean 7.1 s). The operator then played the build on DirectX 11 (see Read this first); OpenGL and DirectX 11 were not compared. The full
records are in [`known-issues.md`](known-issues.md).
