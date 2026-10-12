# Release notes draft: the next prerelease

**Status:** draft for the operator to edit. Nothing here is released.
**Built from:** `main` at `851b1ee9` (2026-10-12, PR [#940](https://github.com/spencerduncan/redshipblueship/pull/940)).
**Covers:** everything since `v0.1.1-prealpha` (2026-07-03). That tag's commit, `eab838c1`, is not an
ancestor of `main`, so "since" is by date: 360 commits on `main` after 2026-07-03. The last 92 of them are
the pull requests of the 2026-09-30 wave ([#802](https://github.com/spencerduncan/redshipblueship/pull/802) to
[#940](https://github.com/spencerduncan/redshipblueship/pull/940)).

Before tagging: `src/common/rsbs_version.h` still reads `v0.1.1-prealpha` (its comment says it "must equal the
git release tag"), and since PR #900 every build's window title reads `v0.1.1-prealpha+<commit>`.

Numbers in parentheses are pull requests. The full list of open problems is
[`known-issues.md`](known-issues.md); the playtest checklist is [`playtest-2026-09.md`](playtest-2026-09.md).

---

## Read this first

- **Saves from earlier builds.** A paired file created before 2026-10-10 is refused by this build: pressing A
  on it in the file select plays the error sound, shows "Refused: Majora's Mask options differ" and keeps you
  on the file list (#895). Erase it and create a new file. Pre-alpha saves are disposable; back up any file
  you care about.
- **Seeds.** A seed string and settings no longer make the world they made on an older build. Generated worlds
  moved in this wave (#840, #874, #895) and before it (#729, #763, #774).
- **Nobody has played this exact build.** Everything below comes from tests, measurements and UI captures. The
  operator played an earlier build on 2026-10-09; its two findings are fixed (#887, #889, #895).

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
  `RedShipBlueShip v0.1.1-prealpha+851b1ee (Morpheus Bravo) (OpenGL)`, and each build writes its own log file
  in `logs/` (#900).

## Known limitations

- macOS is not supported.
- Triforce Hunt is listed as a goal but refused for a paired world. OoT's hints have no pair-level Way of the
  Hero.
- Generation can still fail at file creation ("Not created: try a new seed or other options."); no partial
  file is left.
- At most 64 crossings each way, and netplay grants share those slots.
- Until Majora's Mask has started in a session, the Item Tracker overlay lists MM's items by name instead of
  by icon.
- In narrow windows Settings > General's Boot Sequence dropdown covers its label; Ship of Harkinian draws it
  the same way (#923).
- The config file is still `shipofharkinian.json`.
- Not tested by anyone: cross-game warp songs, networking, DirectX 11, and a crossing after a save-and-reload.

## Open decisions (operator)

- #645: its two open items, and whether to close the epic.
- #497: the SohMenu remainder of ADR 0004.
- #573: cross-game entrance randomization.
- #574: networking / multiworld.
- #460: the rest of netplay increment 1a beyond the hygiene slice (#821).
- #599: MM's regular-save and owl-save flash indexing.
- Closing the trackers #500 and #492 and the QA pass #310.
- Playtests #694 (MM's game-over prompt) and #607 (an OoT item's icon in MM's textbox).
- Replies to the community reports #634, #635 and #636 (fixed by #651, #650 and #648).
- #577: every child has landed; close the epic.
- PR #928's question: should the new-save guard also cover vanilla cross-game sessions?
- #855: which setting the Business Scrub's offer reads (inherited from Ship of Harkinian).

## Run record

The closing smoke at `851b1ee9` (2026-10-12; Windows, OpenGL, ROM archives mounted) passed `redship` 179 of
179, `rando` 43 of 43 (all four golden-world rows), `ui` 2 of 2 with the parity lint, `integration` 14 of 14
and `integration-soak` 1 of 1. The full record is in [`known-issues.md`](known-issues.md).
