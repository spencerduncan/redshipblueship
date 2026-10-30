# Release notes: v0.4.1-alpha ("Deckard Bravo")

**Status:** released. The release is cut as the tag `v0.4.1-alpha` on the commit that merges this text (the version
bump, the release workflow's title and these notes), whose code is `main` at `4b68e319` and whose docs are that commit.
**Version:** `v0.4.1-alpha`, codename "Deckard Bravo" (`src/common/rsbs_version.h`). Under [`VERSIONING.md`](VERSIONING.md) the number is a `z`
bump: bugfix collection, stability improvements, and gameplay refinements without a new `oot.otr` (the archive version,
`project(Ship VERSION 9.1.1)`, is unchanged), so it keeps the film character "Deckard" and advances the NATO phonetic code word
from Alfa to Bravo. A build's window title reads `RedShipBlueShip v0.4.1-alpha+<commit> (Deckard Bravo)`, and `redship --version`
prints the same first line. Saves written by this version remain version 5 and seeds come from generator 2.
**Last release:** `v0.4.0-alpha` ("Deckard Alfa"), tagged 2026-10-28 at `37768b04` (code tip `347ba755`). Its notes
follow this section.
**Merged up to:** `main` at `4b68e319` (2026-10-29) for the code, 22 commits after the tag: pull requests #1285 to
#1320.
**Covers:** everything merged after `37768b04`. Numbers in parentheses are pull requests; issues are written "issue
#N".
**Get it:** the [release page](https://github.com/spencerduncan/redshipblueship/releases/tag/v0.4.1-alpha) carries
`redshipblueship-v0.4.1-alpha-windows.zip` and `redshipblueship-v0.4.1-alpha-linux.AppImage` once the tag's
`generate-builds` run finishes.

---

## Since v0.4.0-alpha

### Read this first

- **This is the second alpha release.** It addresses stability, pause-save persistence, cross-game mod handling, and
  tracker usability discovered following the `v0.4.0-alpha` milestone.
- **Majora's Mask pause-save keeps its entrance and respawn across reloads** (#1317, issues #1298, #1302). Backported
  2Ship2Harkinian's pause-save entrance and respawn persistence (2Ship commits `16ef59bae`, `96611c3b3`). When saving
  via the pause menu in Termina and reloading, Link now reliably returns to the exact entrance and respawn state
  where the save occurred, rather than defaulting to generic daytime scene spawns.
- **Menu Reset and Ctrl+R reset Majora's Mask while running** (#1315, issue #1274). The menu header's red Reset button
  and the Ctrl+R shortcut now dispatch to Majora's Mask's reset handler (`MM_gGameState`) when Termina is active,
  resolving the issue where resets were ignored outside Hyrule.
- **Cross-game mod eviction, actor overlay bounds, and UAF prevention** (#1311, #1319, #1320, issue #1308). Switching
  between Hyrule and Termina now evicts departing game mod archives from the active `ArchiveManager` to prevent
  conflicting textures, models, and scripts. `FileAltExists` and `UnloadOriginalWhenAltExists` verify that alt assets
  belong to currently mounted archives before attempting alt-replacement or eviction, preventing use-after-free
  crashes. `MM_Actor_Delete` validates actor overlay entries against table bounds with a per-actor canary, and
  `EnOkuta` and `SkelAnime` enforce limb count bounds against model definitions.
- **Moon crash New Cycle trade item crash fix** (#1307). Fixed a crash during New Cycle moon crashes with Skip Story
  Cutscenes enabled when a trade item or bottled drink was assigned to a C or D-pad button.
- **Moon crash respawn lands in South Clock Town** (#1288, issue #1278). After the moon falls, a paired file now
  consistently respawns at South Clock Town spawn 0 (`0xD800`, the Song of Time and crossing spawn), avoiding an
  unintended jump into the Happy Mask Salesman's Clock Tower interior cutscene (`0xC030`).
- **Paired-world creation per-attempt budget rises to 60 s** (#1285). The per-attempt generation floor was raised from
  30 s to 60 s (2500 fill rounds per attempt, 5000 per creation event; overlay ceiling 180 s), giving complex seeds
  ample headroom to complete generation without spurious timeouts.
- **Tycoon's Wallet holds 999 rupees in Majora's Mask** (#1297). Termina now recognizes Ocarina of Time's Tycoon's
  Wallet, displaying a full 3-digit counter (up to 999) with a purple rupee icon.
- **Full Wallets fills shared wallet when receiving a far game wallet** (#1304). Under Full Wallets, finding the other
  game's wallet now properly tops off the shared wallet to capacity.
- **Foreign item graphics in shops and get-item cutscenes** (#1295, #1310, issue #1299). Ocarina of Time shops and
  get-item cutscenes now display Majora's Mask clock items using the Clock Tower clock model instead of a placeholder '?'
  stand-in (#1295). Termina shops and get-item cutscenes display Ocarina of Time boss souls as tinted flames (#1310).

### Randomizer and logic

- **Hookshot Ladders trick backported** (#1318). Backported `RT_HOOKSHOT_LADDERS` and the logic helpers
  `CanClimbLadder` and `CanClimbHighLadder` from upstream Ship of Harkinian into Ocarina of Time logic.
- **Shuffle Climb ladder re-gating** (#1291). With Shuffle Climb enabled, OoT logic and the Check Tracker now enforce
  Climb requirements on ladders re-gated in upstream SoH.
- **Scene-dependent item usability logic** (#1290). OoT logic now checks item usability within specific scenes, such as
  requiring weapons permitted indoors before considering house pots reachable under Shuffle Grab.
- **Shuffled fairy check behavior** (#1292). Shuffled fairies in OoT now behave strictly as item checks without
  providing an unintended health or magic refill upon collection; vanilla fairies continue to heal and refill.
- **Gossip Stone and Sun's Song fairy double-spawns prevented** (#1312, issue #1296). Playing an Ocarina song at a
  Gossip Stone or Sun's Song at a fairy spot while its shuffled fairy is already active no longer spawns a duplicate.
- **Astral Observatory access logic** (#1314, issue #1306). MM logic now strictly requires the Bombers' Code or the
  Backflip Over Bomber Guard trick to enter the Astral Observatory from East Clock Town.

### Trackers and UI

- **Check Tracker improvements** (#1293, #1309). Skipped checks now sink to the bottom following SoH's sorting order,
  and the far pane correctly marks checks skipped across both games (#1309). Added a "Scroll to Current Area" button
  and a dedicated visibility toggle for "Only Show Current Area" (#1293).
- **Tatl ISG title casing and Audio Editor MM scroll** (#1316, issue #1276). Title-cased "Tatl ISG (Restoration)" and
  preserved scroll captures in the Audio Editor's MM pane.

### Build and Diagnostics

- **Release map files** (#1313, issue #1303). CI builds now emit `redship.map` as build artifacts and attach them to
  releases to assist in diagnostic and crash dump analysis.

---

# Release notes: v0.4.0-alpha ("Deckard Alfa")

**Status:** released. The release is cut as the tag `v0.4.0-alpha` on the commit that merges this text (the version
bump, the release workflow's title and these notes), whose code is `main` at `347ba755` and whose docs are `main` at
`53a49d42` plus that commit. The next release's draft starts above this section when something merges after the tag.
**Version:** `v0.4.0-alpha`, codename "Deckard Alfa" (`src/common/rsbs_version.h`), the first alpha: the stage word
(`RSBS_VERSION_STAGE`) moves from `prealpha` to `alpha`. Under [`VERSIONING.md`](VERSIONING.md) the number is a `y`
bump: major features and fixes without a new `oot.otr` (the archive version, `project(Ship VERSION 9.1.1)`, is
unchanged), so it takes a new film character and the code word starts again at Alfa. A build's window title reads
`RedShipBlueShip v0.4.0-alpha+<commit> (Deckard Alfa)`, and `redship --version` prints the same first line. Saves
written by this version are version 5 and seeds come from generator 2 (Read this first).
**Last release:** `v0.3.1-prealpha` ("Ripley Bravo"), tagged 2026-10-23 at `038ae33d` (code tip `c1023282`). Its notes
follow this section.
**Merged up to:** `main` at `347ba755` (2026-10-27) for the code, 67 commits after the tag: pull requests #1174 to
#1271 (#1174, #1179, #1183, #1193, #1208, #1214 and #1241 are docs; the numbers in that range that are not listed
below are issues); then the docs of #1273 (`53a49d42`) and the release commit.
**Covers:** everything merged after `038ae33d`. Numbers in parentheses are pull requests; issues are written "issue
#N". The operator played one path of it on the `347ba755` build on 2026-10-27, the Mido's House debug seed through
the crossing to Woodfall Temple, which rose at the first attempt (Read this first); nobody has played the rest. Every
change below is locked by test rows except the warp-song prompt's hint
(#1181; Mido's and the boss door's hints have generation coverage in a paired world since #1207, not a lock row), the
Nearly No Logic cow rule (#1178), the capital P's width (#1198) and the debug-print font path (#1206), the last two
locked by source checks in the Python tests, and the Mido's House debug seed (#1265, #1268: a fixture, made again);
#1174, #1179, #1183, #1193, #1208, #1214 and #1241 are docs. The Majora's Mask settings rows (#1242 to #1271) are
locked for their page, their key and the linking of their code; what each setting does in play is 2Ship2Harkinian's
code, run by no test row.
**Get it:** the [release page](https://github.com/spencerduncan/redshipblueship/releases/tag/v0.4.0-alpha) carries
`redshipblueship-v0.4.0-alpha-windows.zip` and `redshipblueship-v0.4.0-alpha-linux.AppImage` once the tag's
`generate-builds` run finishes. What to test first is section W of [`playtest-2026-09.md`](playtest-2026-09.md).

---

## Since v0.3.1-prealpha

### Read this first

- **This is the first alpha.** The paired game is playable end to end through Woodfall: on 2026-10-27 the operator
  played the Mido's House debug seed on the `347ba755` build, opened the Mido's House chests, crossed at the Happy Mask
  Shop, received the Sonata of Awakening and the Deku Mask at the Clock Tower, and raised Woodfall Temple as Deku Link
  at the first attempt (issue #1070, below). Every menu row of both games has a home: Majora's Mask's settings sit
  beside their Ship of Harkinian twins, apart from the few that cannot act in paired play (listed below), and the
  Combo header is gone (#1242 to #1271, #1248). Saves are version 5 and
  seeds come from generator 2 (#1264, next items). Alpha does not mean finished: expect bugs and missing features, and
  a release can still break saves or seeds; each break is named here first. The open issues are in
  [`known-issues.md`](known-issues.md).
- **Woodfall Temple rises (issue #1070).** The operator's retest above closes the issue's retest condition. No code
  changed for it: its two reports came from the position box of Majora's Mask's own trigger (Deku Link must stand on
  the Deku Scrub symbol) and from the D-pad ocarina of `v0.2.0-prealpha`, fixed by #1099 in `v0.3.0-prealpha` (issue
  #1062).
- **Rupees, hearts, magic and ammo earned or spent between loading a paired file and the first save or crossing are
  no longer lost or refunded, and a file saved from Termina shows the right wallet, bags and hearts in Hyrule at
  once** (#1186, issue #1163). On `v0.3.1-prealpha` nothing reconciled Ocarina of Time with the shared pool at a
  load: what you earned in Hyrule before your first save or crossing was dropped at that crossing or save (and what
  you spent came back), and a file saved in Termina showed Ocarina of Time's older wallet, bags and hearts until the
  first crossing. Loading a paired file now applies the shared pool to Ocarina of Time as the file loads. It
  also fixes the load half of issue #1159: a Piece of Heart found in Termina counts toward Hyrule's next heart
  straight after a load. Files from `v0.3.1-prealpha` load as before; this fix did not change the save format (#1264
  did, next item).
- **Saves written by this build are version 5, and older builds refuse them** (#1264). The cross-game outbox grew a
  new block in the `.redsave` file (Cross-game outbox, below). Files from `v0.3.1-prealpha` and every earlier
  version-4 file load unchanged and are written as version 5 at their next save; from then on `v0.3.1-prealpha` and
  older builds refuse that file, because its version is outside their window. Back up `Save/` before you try this
  build if you may go back.
- **Seed packages from every earlier build are refused: generate the seed again** (#1264). Lifting the 64-crossing
  limit per side moves every world that reached it, so the seed generator's version is now 2: the same seed shows a
  different fingerprint at file select, and a seed package written by a generator-1 build is refused with "Seed
  package from another generator". The Mido's House debug seed was made again for generator 2 (#1268). The
  settings fingerprint (`comboSettingsHash`) moved in all eight golden digests (the generator label only), and
  `seed-digest-shopsanity`'s Majora's Mask placements, hints and free stones moved, because that world reached
  exactly 64 crossings into Hyrule; its crossings did not move.
- **Seeds.** The same seed and settings no longer make the same world as `v0.3.1-prealpha`. Two Majora's Mask logic
  gates (#1178) put Epona's Song and the Circus Leader's Mask, checks in every paired world, in a later sphere, so
  the single bag's crossings move; and gossip stones that read "No Hint" now read gag lines (#1181), which changes
  the hint text after the first such stone. Eight golden digests re-pinned (#1178: placement, crossing and hint
  fields; #1181: `ootHintDigest` only); `seed-digest-armed-caps` and the settings fingerprint (`settingsHash`,
  `comboSettings`) did not move. A file keeps the world it was created with.
- **Files created before #1181 keep their "No Hint" stones**, because hints are saved with the file. Create a new
  file to get gag lines.
- **A file in progress that has freed exactly one of the Double Cell and Steep Slope carpenters in the Thieves'
  Hideout loses an item after updating** (#1205, issue #1191). The two carpenters' keys now award their own cells'
  checks (Ocarina of Time randomizer, below); before, each awarded the other's. A save made before this fix that freed
  exactly one of the two cells shows that cell collected without its item, and gets the other cell's item twice:
  the remaining guard gives again the item you already received, and the freed cell's own item is lost for that file.
  Free the second cell before updating, or start a new file. Saves that freed neither or both cells are unaffected.
  A save does not record which build resolved the key, so no migration can repair it; breaking saves this way is
  accepted in the pre-alpha period.
- **Custom player models no longer crash the game at the first draw** (#1197, issue #1190). Since the January symbol
  namespacing, a skeleton mod whose limbs use Fast64's `gEmptyDL` marker for a limb with no mesh (the Heroine of
  Time Malon pack, for example) crashed where Link is first drawn: on the title screen with Alt Assets on, or when
  child Link first appears. Such a limb has no display list again. On an older build, disable the pack or turn Alt
  Assets off.
- **The Combo Tracker is now the Check Tracker, and the Combo Item Tracker the Item Tracker; SoH's own windows of
  those names are retired** (#1192, Trackers below). Open them from Randomizer > Check Tracker and Randomizer > Item
  Tracker.
- **The Combo header is gone** (#1248, Menus below): every Majora's Mask setting sits beside its Ship of Harkinian
  twin, Settings has OoT Mods and MM Mods, and Save Files is under Dev Tools. A menu last left on a Combo page opens
  on that page's new home. Older sections of these notes name the pages of their day: Combo > Windows is Randomizer >
  Check Tracker (and Randomizer > Item Tracker), Combo > Tracker Settings is Randomizer > Item Tracker, Combo >
  Majora's Mask is Enhancements > Quality of Life and Skips & Speed-ups, Combo > MM Mods is Settings > MM Mods,
  Combo > Save Files is Dev Tools > Save Files, and Combo > Cross-Game Rules, MM Randomizer and MM Tricks are
  Randomizer > Cross-Game, MM Randomizer and MM Tricks.
- **Majora's Mask's settings are in the shared menu** (#1242 to #1271, issue #1233). About 270 of
  2Ship2Harkinian's settings rows had no row in the shared menu, or no linked code, or both; they now sit on the
  Ship of Harkinian page that holds their twin, apart from a few that cannot act in paired play (listed below).
  Majora's Mask's first-person, right-stick aiming and free-camera Y axes start inverted, as Majora's Mask
  plays, and can at last be turned off (Settings > Controls > Popout Bindings Window > Camera Controls). The
  one-time import of 2Ship2Harkinian's settings runs only on the first boot of an install: see "Majora's Mask
  settings in the shared menu", below.

### Majora's Mask settings in the shared menu

The 2026-10-27 audit of 2Ship2Harkinian's menu (issue #1233, sections issues #1235 to #1240) found about 270 settings
rows a player could not reach in the single executable, or whose code was not linked, or both. Twenty lanes added
them through one seam for Majora's Mask rows hosted on Ship of Harkinian's pages (#1244): each row sits on the page
that holds its twin, or in a "Majora's Mask" block on the page of its nearest relatives, its code is linked, and its
tooltip ends "Majora's Mask only." A row whose name matches the SoH row beside it carries an "MM" label. Where one
setting already meant the same thing in both games, the one SoH row now drives both. The leftovers are tracked in
issue #1272.

What you will notice first:

- **The camera is no longer stuck inverted in Termina.** Majora's Mask's aiming, right-stick aiming, shield and
  camera inversion and sensitivity have their own rows in the Bindings window (Settings > Controls > Popout Bindings
  Window > Link (P1) > Camera Controls), beside SoH's. Majora's Mask's first-person, right-stick aiming and free-camera
  Y axes still start inverted, as Majora's Mask plays, and can now be turned off (#1243).
- **Disable Navi Call Audio also mutes Tatl's call**, and Link's Voice Pitch Multiplier changes Link's voice in
  Termina (Enhancements > Audio Editor > Audio Options); two new rows mute the Clock Town carpenters and the crying
  Goron child (#1242).
- **Every volume slider changes Majora's Mask at once**, instead of after a restart, and Settings > Audio has an
  Ambience Volume slider for Majora's Mask's wind, birdsong and night sounds, right after Sound Effects Volume (#1244).
- **MM Fast Text** has its own row under Text Speed (Enhancements > Skips & Speed-ups > Text): text draws fast and
  holding B advances it (#1247).
- **SoH's Mirrored World mirrors Termina** (Enhancements > Extra Modes): Always, Random and the Dungeons modes mirror
  Majora's Mask per scene; Dungeons (MQ) leaves it unmirrored, since Majora's Mask has no Master Quest (#1256).
- **The Cosmetics Editor reaches Majora's Mask**: its HUD can be moved, scaled and hidden, and its buttons, hearts,
  magic, Link's forms, arrows and trails take SoH's colours (below).
- **Coming from 2Ship2Harkinian?** The one-time import of 2Ship2Harkinian's settings now brings over every Majora's
  Mask setting that has a row (#1270), below.

By page:

- **Settings > Controls, Bindings window.** Camera Controls: the inversion and sensitivity rows above (#1243); MM Gyro
  Aiming and its four axis rows, MM Max / Min Camera Height Angle for the free camera, and MM Debug Camera, which takes
  over from Free Look while it is on (#1251). Modifier Buttons: Majora's Mask's walk and swim speed modifiers (hold or
  toggle), kept apart from SoH's (#1251).
- **Enhancements > Quality of Life.** Under Autosave, MM Autosave Interval (shown while Autosave is on; Autosave
  itself applies to both games) (#1248). A "Majora's Mask" block at the end of Time of Day: Do Not Reset Bottle
  Contents, Do Not Reset Razor Sword, Do Not Reset Time Speed, Keep Express Mail, Stop Oceanside Spider House Squatter
  and Oceanside Wallet Any Day (#1245), then Game Over Prompt and Better Double Time (#1248). Under Saving, Pause
  Menu Save, which saves the paired file (#1247). Under Controls, Manual Jump, Prevent Diving Over Water and Infinite
  Deku Hopping (#1246). Do Not Reset Rupees and Do Not Reset Consumables have no row: the shared rupees and ammo
  survive every Song of Time, and the Powder Keg and Magic Beans follow the file's Keep Items on Cycle Reset (#1245).
- **Enhancements > Skips & Speed-ups.** MM Fast Text (above); Skip Time Song Cutscenes right after Skip Song
  Cutscenes (#1248); a "Majora's Mask" block with Fast Bank Selection, Auto Bombers' Code and eight time savers (the
  swamp boat, Both Gallery Rewards, the lab fish, Dampe, Faster Scene Transitions, the Powder Keg certification, the
  Ballad of the Windfish and the automatic bank deposit) (#1247, #1262); under Animations, MM Vine/Ladder Climb Speed,
  Faster Push/Pull, Fast Deku Flower Launch and Fierce Deity Putaway (#1246).
- **Enhancements > Items.** Equipment: Two-Handed Sword Spin Attack, Great Fairy Sword B-Button Attack and Better Picto
  Message; Boomerang: Instant Fin Boomerangs Recall (#1246); Explosives: Remote Bombchu Control (#1251); Masks: eight
  rows (Equippable While Swimming, Blast Mask Has Powder Keg Force, Fast Transformation, Fierce Deity's Mask Anywhere,
  Persistent Bunny Hood, No Blast Mask Cooldown, Goron Rolling Ignores Magic, Goron Rolling Fast Spikes); Ocarina: six
  (Pause Owl Warp, Enable Sun's Song, Zora Eggs for Bossa Nova, Faster Song Playback, Skip Soaring Cutscene and
  Underwater Ocarina) (#1253).
- **Enhancements > Difficulty.** Under Drops, MM Enable Bombchu Drops turns bomb drops into Bombchu drops only once
  you hold Bombchus, so it never hands out Bombchus the seed has not given you in either game; SoH's own Enable
  Bombchu Drops does not change Majora's Mask (#1246). A "Majora's Mask" block: Jinxed Timer, Lower Bank Reward
  Thresholds, Disable Takkuri Steal, Deku Guard Search Balls, Gibdo Trade Sequence, Hidden Grottos Visibility, Frog
  Choir Count, Milk Run Reward Options and Ammo Buyback (#1256).
- **Enhancements > Extra Modes.** Mirrored World (above); Play as Kafei, Hyrule Warriors Styled Link and Time Moves
  When You Move (#1256).
- **Enhancements > Minigames.** Town and Swamp Archery Score and Mark Gallery Octoroks under Shooting Gallery, and a
  "Majora's Mask" block for the Bombers' hide-and-seek, Swordsman School, Honey & Darling, Romani's practice, the
  Doggy, Gorman, beaver and Goron races, the Cucco Shack, boat archery, Koume and the Treasure Chest Shop (#1255). On a
  randomizer file a shortcut win hands the check's item once, never a vanilla reward in its place.
- **Enhancements > Graphics.** Clock Type (MM3D Style or Text Only) and 24 Hours Clock in the UI column; Motion Blur
  Mode, Interpolate Motion Blur, Motion Blur Strength and Dynamic Motion Blur with its strength (Dynamic Motion Blur
  is live only while Majora's Mask runs); Fix Scene Geometry Seams and Disable Scene Geometry Distance Check; the
  scene-geometry and Fix Out of Bounds Textures switches take effect in Majora's Mask without a restart (#1254).
- **Enhancements > Fixes.** Each Majora's Mask row at the end of the SoH section that holds its relatives: Fix Epona
  Stealing Sword, Fix Fierce Deity Z-Target Movement, MM Power Crouch Stab, Fix Targeting Camera Snap, Fix Ammo Count
  Color, Fix Ikana Great Fairy Fountain Color, Fix Text Control Characters, Fix Completed Heart Container Audio, Tatl
  ISG (restoration), Constant Distance Backflips and Sidehops, Side Rolls, Faster Swim, Woodfall Mountain Appearance,
  JP Deku Palace Grottos and Bonk Collision (#1252).
- **Enhancements > Cheats.** MM Consumables under Infinite..., MM Unrestricted Items beside SoH's, and a "Majora's
  Mask" block with Longer Deku Flower Glide, Elegy of Emptiness Anywhere, Stop Time in Dungeons and Unbreakable Razor
  Sword (#1245).
- **Enhancements > Timers and Time Splits.** Majora's Mask's In-Game Timer is back, with its mode (Real-Time or
  In-Game Time), background and scale; on a paired file it measures Majora's Mask only. Time Splits has its six
  options, and the Time Splits window shows 2Ship2Harkinian's list management and split entries while Majora's Mask
  runs (#1258).
- **Enhancements > Audio Editor.** The Tatl call, voice pitch and mutes (above, #1242), and a new "Majora's Mask" tab
  that replaces, locks, shuffles and resets Majora's Mask's background music, fanfares, events, battle music, ocarina,
  instrument and song sequences, with its own shuffle pool; Automatically Randomize All Music and Sound Effects
  reaches Majora's Mask on a new scene or at randomizer generation (#1261).
- **Enhancements > Cosmetics Editor, HUD Placement.** A "Majora's Mask" section moves, scales and hides the B, A,
  Start and C buttons, the D-pad items, the minimap (#1257), the three-day clock in all three styles, hearts, magic
  meter, timers, carrots, minigame score and the rupee, key and skulltula counters (#1266); its HUD Preset (Vanilla,
  Hidden, Widescreen) covers them all.
- **Enhancements > Cosmetics Editor, colours.** SoH's A, B, C, D-Pad and Start buttons, small-key icon, minimap and
  spin attack rows colour Majora's Mask too, and the HUD tab has a Rupee Icon row (#1259). Hearts, Heart Border, DD
  Hearts and Magic (with its Active and Border rows) colour Majora's Mask's hearts and meter, and Kokiri Tunic and
  Hair colour human Link in Termina; Beating Hearts, DD Beating Hearts, Chateau Magic and the Deku, Goron and Zora
  forms and Kafei get rows of their own (#1269). The Arrow Effects rows colour Majora's Mask's fire, ice and light
  arrows, SoH's Kokiri Sword, Deku Stick and Boomerang trails colour Majora's Mask's, and a "Majora's Mask" block on
  the Effects tab has the Razor, Gilded, Great Fairy and Fierce Deity sword trails and the Deku spin, Zora punch and
  Zora kick trails (#1271).
- **Dev Tools > General.** Frame Advance, Advance 1 and Advance (Hold) step whichever game is running, Termina
  included. Under Warping, MM Better Debug Warp Screen (Majora's Mask's map select), MM Set Warp Point, MM Warp to
  Warp Point and MM Clear Warp Point; a "Majora's Mask" block has the Prevent Actor Update, Prevent Actor Draw,
  Prevent Actor Init and Disable Object Dependency switches, which turning Debug Mode off clears;
  Debug Save File Mode applies to Majora's Mask's own new saves, never to a paired world's (#1260).
- **The 2Ship2Harkinian settings import** (#1270). On the first boot of an install, the importer reads a standalone
  `2ship2harkinian.json` and now brings over every Majora's Mask setting that has a row (the camera inversion and
  sensitivity, gyro, Fast Text, motion blur, the cheats, the time savers and more), not just the settings both games
  share; a setting already changed in this build keeps its value. It runs once per install: an install that has
  already booted has the stamp `"TwoShipImport": {"Completed": true}` in `shipofharkinian.json`, and gets the wider
  import only after that entry is removed. Custom colours saved under 2Ship2Harkinian's `gCosmetic.Hearts`,
  `gCosmetic.Magic` and `gCosmetic.Player.Human` keys are not carried (issue #1272); set them again in the Cosmetics
  Editor.

Settings with no row on purpose: Fix Console Crashes (Majora's Mask's console-crash fixes are always on, #1252),
Authentic Logo (its console logo shows only on an unpaired `--game mm` boot, #1254), 3rd Save File Slot, Persistent
Owl Saves and Disable Save Delay (none can act in paired play, #1247), Do Not Reset Rupees and Do Not Reset
Consumables (above), and 2Ship2Harkinian's About panel (issue #1272).

### Menus: the Combo header is gone

- **Every Majora's Mask row sits beside its Ship of Harkinian twin** (#1248, issue #1133): the menu's headers are
  Settings, Enhancements, Randomizer, Network and Dev Tools, with no Combo header. Combo > Majora's Mask's rows went
  to Enhancements > Quality of Life (MM Autosave Interval, Game Over Prompt, Better Double Time) and Skips &
  Speed-ups (Skip Time Song Cutscenes).
- **Settings has OoT Mods and MM Mods**, side by side in the sidebar: OoT Mods is Ship of Harkinian's Mod Menu,
  renamed, and MM Mods is Majora's Mask's mod list, formerly Combo > MM Mods, with the same order arrows and disable.
  **Save Files** is the last page of Dev Tools.
- **A menu last left on a Combo page reopens on that page's new home**, and one last left on Settings > Mod Menu
  reopens on OoT Mods. The menu search finds the moved rows on their new pages.

### Shared items

- **Buying a bomb bag in Termina fills your shared bombchus at once**, and so does Ocarina of Time's bomb bag found
  on a Majora's Mask check; before, Termina showed no bombchus until the next arrival (nothing was lost). Hyrule
  names Majora's Mask's chu packs by count: "You found 1 Bombchu (MM)!", and "5 Bombchus" or "10 Bombchus". Termina
  holds bombchus only under a bomb bag, by design (OoTMM's Majora's Mask default); the count waits in the shared pool
  until then (#1184, issue #1160).

### Cross-game items

- **Majora's Mask's songs draw as notes on Hyrule's shelves and in its get-item cutscene**, in 2Ship2Harkinian's
  colour for each song (pink for the Song of Healing, red for the Goron Lullaby, purple for the Oath to Order),
  instead of the "?" stand-in; the Sun's Song, Song of Time, Song of Storms and Epona's Song show Ocarina of Time's
  own note in Ocarina of Time's colour (#1185, issue #1167).
- **Ocarina of Time's Gerudo Mask and the six medallions draw correctly in Termina**: each host now draws another
  game's model under that game's own setup list, where Majora's Mask used its own, different entry and smeared the
  texture (#1185, issue #1171).
- **Majora's Mask's masks and stray fairies in Hyrule's chests draw Ship of Harkinian's mask and fairy chests** when
  Containers Match Contents is on, where they drew the gold major chest (#1213, issue #1172). Ocarina of Time's own
  masks keep the major chest, as OoTMM grades them.
- **Ocarina of Time's Piece of Heart, Heart Container and Double Defense get the heart chest again** (#1213; SoH
  `614ec084f`); the Treasure Chest Game's heart stays a lesser chest. A Majora's Mask chest holding one of them shows
  Majora's Mask's heart chest.

### Cross-game outbox

- **The cross-game outbox holds up to 2048 pickups, and a seed can place any number of the other game's items on a
  side** (#1264, issue #1222; the operator's ruling of 2026-10-27). The outbox, which carries the other game's items
  you pick up until your next crossing, had 64 fixed slots, and the single bag never put more than 64 of one game's
  items in the other game's world. The 64 slots are kept and a new block of the save carries the rest; the fill's
  64-per-side cap is gone, so seeds mix the two games' items as OoTMM's do. The save format and the seed generator's
  version moved with it (Read this first). A moon-crash reset restores the overflow with the fixed slots (#1267).
- **A full outbox holds a pickup instead of losing it** (#1250, issue #1222). Before, a check in Termina that held a
  Hyrule item was marked delivered, and a shop slot sold with its price charged, even when the outbox had no room,
  and the item was lost. Now the item is held with an "Outbox full: cross to Hyrule to send it" toast ("cross to
  Termina" in Hyrule) and given after your next crossing; a Majora's Mask shop or Tingle refuses the sale and charges
  nothing, and counted packs and capacity upgrades wait for the crossing too. Since #1264 this needs 2048 un-redeemed
  pickups in one game.

### Keys and reticles

- **Termina's dungeon keys draw 2Ship2Harkinian's dungeon key models**: a small-key and a boss-key body shared by the
  four temples, each with its temple's emblem in the temple's colours (#1213, issue #1172; 2S2H `5d96fd84f`,
  `f56c7939e`). The boss key no longer draws its translucent gem, as in 2Ship2Harkinian. The models are in
  `2ship.o2r`: the GitHub Actions builds generate it and carry them; a locally staged tree needs the
  `Generate2ShipOtr` target run again, and until then the keys keep their tinted vanilla model. A Majora's Mask key
  drawn in Hyrule keeps its current look.
- **The Hookshot reticle turns green on a hookshot surface in Termina too** when Enhancements > Items > Hookshot >
  Targetable Hookshot Reticle is on, in the Cosmetics Editor's Hookshotable Reticle and Non-Hookshotable Reticle
  colours, as in Hyrule: the one option drives both games (#1212, issue #1173). It never turns green on an enemy.
- In Hyrule the **Bow, Slingshot and Boomerang reticles no longer turn green on hookshot targets** with that option
  on; only the Hookshot and Longshot reticles do (#1212; SoH `6340ad3d5`).

### Majora's Mask options

- **Keep Items on Cycle Reset** (OoTMM's option, off by default; #1194, issue #1175): a checkbox on Randomizer > MM
  Randomizer, right after Moon Crash Behavior. With it on, the Song of Time, the new day after the credits and a New
  Cycle moon crash keep your Powder Keg and Magic Beans; with it off the Song of Time takes them, as in vanilla
  Majora's Mask and in OoTMM. Rupees and the shared ammo are kept in every world, and the pictograph is still lost.
  It is frozen when the file is created, and loading the file puts its value back on the page. Like Moon Crash
  Behavior it is part of the world's identity, so turning it on changes the placements a seed makes; left off, no
  seed moves. Issue #1175's keg "with its slot but no keg" was this reset: a keg found in Hyrule arrives as one keg,
  and the three-day reset emptied it.
- **Shuffle Time no longer hands out hours the file does not own** (#1230, issue #1229). When no owned half-day is
  left, the moon falls at the end of the last owned one, as in OoTMM: a file without Time (Night 3) no longer jumps
  from 6:00 PM on Day 3 to 12:00 AM and gets six "Final Hours" with the Clock Tower open. The **Final Hours Start
  Time** row is retired: it is drawn disabled with "Option is Retired" and has no effect. Under Moon Crash Behavior
  "Last Save", a save taken shortly before the last owned half-day ends no longer crashes the moon again right after
  the restore. The Song of Double Time's prompt names "the Moon's Fall" when nothing owned is ahead, and the
  Scarecrow's dance then leaves the clock where it was: the scene fades and reloads with no message, which is
  intended. Logic never counted those hours, so placements do not move.

### Hints

- **Gossip stones' junk hints read gag lines again.** Under Balanced (the default) about one stone in eight read
  "No Hint", and under Useless all of them (#1181, issue #1162; SoH `2f262f492`).
- Three more Ship of Harkinian hint fixes came with it (#1181): under 1 to 3 Ganon's Trials the trial hints name
  required trials (all six were hinted, taking four stones), and an item hint naming several places keeps every
  name (SoH `af485bf64`); Mido's Hint and Boss Door Hints now show their text when on (SoH `983d04d36`); and with
  warp songs shuffled and Warp Song Hints off, playing a warp song asks about "a mysterious place" instead of naming
  its vanilla destination (SoH `0dcd52e5c`; the prompt only, the warp is unchanged).
- **A shuffled dungeon map's text box names its dungeon again, with the map icon, and the Great Deku Tree Compass
  shows the compass text instead of the map's** (#1207, issue #1187): the halves of SoH `983d04d36` that #1181 left
  out. With them, Ship of Harkinian's message text replacement no longer loops forever when a replacement contains
  the text it replaces, so an item text that puts its icon on every page can no longer hang the game when it runs to
  a second page (ported from SoH `983d04d36`, with the search resuming after the inserted text). Mido's hint and the
  boss-door hints now have generation coverage in a paired world, including a boss key the single bag put in
  Termina.

### Logic

- **Ocarina of Time** (Ship of Harkinian's location logic): under Shuffle Jabber Nuts, Song from Malon and the four
  Thieves' Hideout carpenters need the Hylian jabber nut; under Shuffle Grab, Talon's chickens need the Power
  Bracelet; and the Haunted Wasteland carpet salesman needs both the nut and his price, where a missing pair of
  brackets let any jumpslash reach him (#1178, issues #1168, #1182; SoH `c906c71f7`, `63597ec63`, `1413ceb33`,
  `0702b11b9`).
- **Majora's Mask** (2Ship2Harkinian's region logic): Romani Ranch's six cows, Epona's Song and the alien drop need
  the Milk Road boulder broken (the Goron Mask and a Powder Keg), and the Circus Leader's Mask needs Romani's Mask.
  Nearly No Logic never leaves Epona's Song on a cow (an unpaired Majora's Mask file only) (#1178; 2S2H `9b699dee1`,
  `cc5d87143`, `b754231c3`).

### Ocarina of Time randomizer

- **The Double Cell and Steep Slope carpenters' keys award their own cells' checks**; each awarded the other's
  (#1205, issue #1191; SoH `2bc50f605`). A file in progress that freed only one of the two: Read this first.
- **Under Shuffle Master Sword, a respawn out of the Ganon's Tower collapse or Ganon's arena no longer gives the
  Master Sword the seed placed elsewhere** (#1223, issue #1218): dying and picking Continue, letting the escape timer
  run out, or voiding out. A sword you already found is still put back on B, and with Shuffle Master Sword off, or on
  a vanilla file, the respawn gives the sword as before. Inherited from Ship of Harkinian.
- **Under Shuffle Jabber Nuts the Deku Theater leader gives the seed's item and no vanilla capacity upgrade** (#1226,
  issue #1217). Before, the randomizer's theater skip turned itself off under the option and the vanilla judge gave a
  Deku Stick or Deku Nut capacity upgrade on top of the theater check, which under Deku Stick Bag or Deku Nut Bag
  shuffle handed over the bag the seed placed elsewhere. A vanilla file still gets the upgrades.

### Majora's Mask randomizer

- **The seed's Razor Sword stays the Razor Sword** (#1224, issue #1215): the Song of Time and breaking after about
  100 hits no longer turn it back into the Kokiri Sword. Four 2Ship2Harkinian gates that the randomizer forces on
  were left out of the single executable's link; their randomizer halves are in now.
- **The Oceanside Spider House reward can be collected on any day, and passing Mikau's grave first no longer moves
  its owner away from the entrance** (#1224, issue #1221).
- **Leaving the Deku King's chamber after the monkey check no longer teaches the vanilla Sonata of Awakening**
  (#1225, issue #1203), so Woodfall Temple rises only once you have the Sonata the seed placed. The monkey still
  resets your instrument as you leave. The shared-item redeem lines (`[SharedItem] redeemed`, `[MM] shared-item
  redeem`, `[MM] foreign give (deferred to gameplay)`) now reach the log file.
- **Minigame rewards no longer give a second item for one check** (#1227, issues #1216, #1219, #1220). With the
  quiver maxed, a new Town gallery record or a Swamp "Perfect!" no longer adds a Purple or Red Rupee to the
  high-score check, nor does the first beaver race from the second cycle on. The Doggy Racetrack no longer adds an
  unplaced Red Rupee to its first win, and a later win in the same cycle gives the vanilla Red Rupee instead of the
  check again, as in OoTMM. A second Deku Playground win gives the vanilla Purple Rupee. A lab fish fed again after
  re-entering the Marine Research Lab gives nothing (its vanilla reward would be a heart piece), and targeting Flat's
  tomb again after re-entering the room no longer gives the Song of Storms check a second time.
- **The Hags' potion shop no longer waits forever on its free blue potion** when its first shelf hosts an Ocarina of
  Time item already delivered (#1250, issue #1228): on a later cycle that shelf shows its own line and "can't buy
  right now", never the free-potion offer, and the shop keeps working.

### Trackers

- **The Combo Tracker names a shop item once you have seen it, and adds its price once you have checked it**, as
  SoH's Check Tracker does: "(item - price)"; under Mysterious Shuffle a seen shuffled slot stays unnamed (#1180,
  issue #1166).
- **With Show Logic on** (Check Tracker Settings), hovering a check in either game's pane of the Combo Tracker shows
  its logic as a tooltip (#1180, issue #1161).
- Two Ship of Harkinian Check Tracker fixes came with it: an identified check that sells nothing (the Skulltula
  House rewards) no longer prints "- 0", and with Shuffle Merchants on All But Beans the bean salesman is no longer
  listed (#1180; SoH `2ba368e7e`, `6bf5ec514`).
- **The Combo Tracker scrolls to the area you are in** each time you enter a new one, in the game you are playing,
  and each game's list has its own Only Show Current Area toggle. Two new settings set them: Auto Scroll To Current
  Area (on by default) and Only Show Current Area (off by default) (#1188, issue #1131); since #1192 they are on
  Randomizer > Check Tracker.
- **The Check Tracker and Item Tracker replace Ship of Harkinian's** (#1192, issue #1132). The Combo Tracker is now
  named the Check Tracker and the Combo Item Tracker the Item Tracker, and SoH's own Toggle Check Tracker and Toggle
  Item Tracker buttons, on Randomizer > Check Tracker and Randomizer > Item Tracker, open them; SoH's single-game
  windows no longer draw. Randomizer > Check Tracker also carries Auto Scroll To Current Area, Only Show Current Area
  and the Cross-Game Spoiler's toggle. Combo > Windows is gone, and a menu last left on it opens on Randomizer > Check
  Tracker. (Combo > Tracker Settings kept the Item Tracker overlay's rows until #1202, below.) Searching the menu for
  one of SoH's six Check Tracker settings (Background Color, Window Type, Vanilla/MQ Dungeon Spoilers, Hide Unshuffled
  Shop Item Checks, Always Show Gold Skulltulas, Show Logic) still lands on SoH's Randomizer > Check Tracker page,
  which is now the unified tracker's page.
- **The Check Tracker draws its rows as SoH's does** (#1192): no status square before a check's name (the name's
  colour shows the status), the line under a pane's header only while a header control shows, and a search box that
  matches item names but no longer prices.
- **The Item Tracker follows SoH's Item Tracker Settings** (#1202, issue #1141). Its settings live on Randomizer >
  Item Tracker as one set: SoH's Item Tracker Settings (Window Type, Enable Dragging, Only Enable While Paused, the
  Combo Button Hold display mode, Background Color, Icon size and each section's display) govern it, and below them
  "Item Tracker Sections" (Ocarina of Time, Majora's Mask, Shared) and "Majora's Mask Groups" hide a column or one of
  Majora's Mask's groups. Combo > Tracker Settings is gone; a window type and icon size saved there carry over once,
  and a menu last left on it opens on Randomizer > Item Tracker. Owl faces fit their cells.
- **The Item Tracker honours SoH's count settings** in both games' sections (#1249, issue #1143): Ammo/Capacity
  Tracking, Align count to left side, Key Count Tracking, Triforce Piece Count, Hookshot Identifier, Icon margins and
  Text size. Both sections now default to SoH's "Current Capacity", so a held bow reads its quiver ("40", where it
  read "35/40"), and the Bombs, Deku Sticks and Deku Nuts show their bag's size; "Current Ammo" in Item Tracker
  Settings brings the counts back. Bombchus always show their count.
- **The Item Tracker shows more of Ocarina of Time** (#1263, issues #1142, #1158): its dungeon items (small keys,
  boss keys, maps and compasses) in SoH's compact layout, and, when the seed shuffles them, its boss and bean souls,
  jabber nuts, ocarina buttons, overworld keys, the fishing pole and Greg, each shown by SoH's own Item Tracker
  section setting (hidden under SoH's defaults; set a section to Main Window to see it). A small key's cell reads
  collected/max, or in-hand/collected/max under Key Count Tracking, with the Thieves' Hideout's count under Fast
  carpenters and a Master Quest dungeon's own count.
- **The Item Tracker's Shared column shows the heart pieces toward your next heart** (#1263, issue #1159): four pieces
  from either game make one shared heart. Issue #1159's report of five pieces to a heart stays open until a playtest
  confirms the display; no path in the code needs a fifth.

### Fixes

- Majora's Mask's Shooting Gallery time saver reads "Both Gallery Rewards", so it fits its column at the smallest
  window size (832x600) (#1262).
- With Debug Mode on, a Majora's Mask scene change to different music stops the old main music player first, as in
  2Ship2Harkinian, and the area music comes back after a mini-boss fight with Faster Scene Transitions on (#1247).
- Verdict, no code change: Woodfall Temple rises at the first attempt in the operator's 2026-10-27 play of the
  `347ba755` build (issue #1070, Read this first); the position box and the `v0.2.0-prealpha` D-pad ocarina (issue
  #1062, #1099) were the two causes.

- A file in `presets/` that is not a preset (a stray `index.html` from a recursive download, `Thumbs.db`,
  `.DS_Store`, a `.json.tmp` from an interrupted save) no longer closes the game about 110 ms after launch: it is
  skipped with one log line, and a `.json` that fails to parse is logged and skipped (#1211, issue #1210; SoH
  `5875ed880`, `7cdf69b20`). A preset renamed with an upper-case extension (`Foo.JSON`) is skipped too: rename it to
  `.json`.
- Toggling Alt Assets while in Termina no longer writes past Majora's Mask actors' animation state (#1177, issue
  #1176); this may have been the cause of issue #1063's delayed crash with an Ocarina of Time texture overhaul pack.
- Custom skeleton mods made with Fast64 draw again (#1197, issue #1190; Read this first).
- A capital P has its pixel width in the text auto-wrap of Ship of Harkinian's own messages (hints, shop and item
  texts) again, so a line with names such as "Pocket Egg" or "Prelude of Light" no longer runs past the text box's
  edge; the January namespacing had renamed the width table's "P" key (#1198, issue #1190).
- A re-added `.otr` mod no longer leaks its file handle (#1199, issue #1190; libultraship fork): every mod archive is
  mounted again after the curated archives, at boot and on every game switch, and each re-add of an `.otr` kept the
  replaced copy's file open. A release build's crash trace now prints each frame's module offset, which resolves
  against `redship.map`, where it printed meaningless symbol names.
- The spoiler's cross-game crossing rows (`combo.crossingStore`) give each item two names: `itemName`, the game's own
  name, and `displayName`, the name the tracker shows; a Majora's Mask single Bombchu reads "Bombchu" and
  "1 Bombchu" (#1201, issue #1200). A spoiler without `displayName` loads as before.
- A texture pack's replacement debug-print font (`textures/font/sGfxPrintFontData`) is picked up in Ocarina of Time
  again (#1206, issue #1195): the January namespacing had prefixed that resource path too. Nothing changes without
  such a pack. It was the last lookup key the namespacing damaged (Mods under Known limitations).
- The log no longer fills with "File[255] - does not exist" lines (about 113,000 in one session, one per frame with
  Autosave on), and a foreign-item give that waits, for a scrub's text box for example, logs one attempt line and
  then one line with its retry count instead of one line per frame (#1209, issue #1204). Items are given as before.

### For contributors

- `docs/upstream-issue-xref-2026-10-26.md`: the open issues cross-referenced against upstream Ship of Harkinian,
  2Ship2Harkinian and libultraship fixes (#1174), with three corrections from the 2026-10-26 triage (#1183).
- `docs/upstream-backport-audit-2026-10.md`: the bundled backport audit of the three upstreams since our import
  bases (#1179).
- `tools/namespace_symbols.py` rewrites code only, never string literals, char literals or comments, and has a
  read-only `--audit-literals` mode that lists the literals naming a game-prefixed symbol (#1206). A pytest invariant
  fails on any key-shaped literal (no whitespace) that names one.
- `--test debug-seed-forced-placements` makes a paired seed with items pinned to checks (`RSBS_FORCE_PLACEMENTS`),
  for reproducing reports (#1234). Its first use is the Mido's House debug seed in
  `docs/playtest-fixtures/midos-house-sonata-deku/`: the Sonata of Awakening, the Deku Mask and an Empty Bottle in
  Mido's House chests with Closed Forest off, a quick way to the Woodfall platform for issue #1070 (#1234, #1265),
  made again for generator 2 (#1268).
- `MMOcarinaDpadNotes` drives the D-pad through Majora's Mask's real ocarina note path, guarding the D-pad ocarina
  fix of #1099 (#1232).
- `SaveSharedItemOverflowResetRestore` proves the moon-crash reset restores the outbox's overflow with its fixed
  slots (#1267).
- Majora's Mask rows on Ship of Harkinian's pages go through one seam (`AddHostedMmWidget` / `AddHostedMmBlock`) that
  the UI snapshot captures, hovers and lints, with generic menu, classification and registrar locks (#1244); the
  hosted rows' label fit at 832x600 is measured against the widest SoH row on the same page (#1248).

### Known limitations added since v0.3.1-prealpha

- **Hints:** a file created before #1181 keeps its "No Hint" stones (above). The follow-ups of issue #1187 are in
  (#1207, above); the map and compass boxes were checked through their text builder and item table, not on screen.
- **Logic (#1178):** logic names the right cell for the Double Cell and Steep Slope carpenters, and since #1205 the
  game does too (Read this first for a file in progress). The game stays more permissive than logic in two
  places (the carpenter cutscene skip without the nut, Talon's game without the bracelet), so no seed becomes
  unbeatable. Link's House Cow shows available once everything needed to win Malon's race is held, as SoH's own
  tracker shows it: its logic assumes the race can be won (issue #1170, by design).
- **Bombchus (#1184):** Termina holds none without a bomb bag (by design, above). A Majora's Mask chu pack found in
  Termina with no Majora's Mask bag still becomes junk, and 2Ship2Harkinian's own Majora's Mask bomb-bag give can
  add chus to the shared count; both are follow-ups (issue #1061's root-cause comment).
- **Combo Tracker (#1180, #1188):** Majora's Mask's pane names only obtained checks (2Ship2Harkinian's tracker has no
  seen state or prices to mirror). A game's pane shows no logic tooltip until that game has booted once in the
  session. The current area the tracker keeps for the game you are not playing outlives the file: load another file
  in Hyrule and Majora's Mask's pane, with Only Show Current Area on, filters to the old file's last scene until you
  play Termina again (issue #1189).
- **Check Tracker and Item Tracker (#1192):** window positions saved for the old Combo Tracker and Combo Item Tracker
  are not carried (the windows take SoH's own windows' saved positions, if any), nor whether they were open: a
  tracker opened through Combo > Windows starts closed, and one opened through SoH's own toggle opens as the unified
  window. An SoH preset's Check Tracker position is applied to SoH's retired window, not the unified one. Some of
  SoH's Check Tracker settings govern nothing yet (Vanilla/MQ Dungeon Spoilers, for example). The menu search for one
  of SoH's tracker settings still lands on SoH's own page. SoH's Item Tracker Settings govern the Item Tracker since
  #1202 and its counts since #1249; SoH's notes, separate section windows, Circle and Horizontal display and Total
  Checks are not honoured.
- **Item Tracker (#1263):** with both on, Greg and the Fishing Pole each start a line of their own in the Ocarina of
  Time column, where SoH's main window puts them on one line, and a section set to SoH's Misc window shows in the
  overlay's one window. Majora's Mask's Dungeon and Boss Souls groups follow its own Majora's Mask Groups rows, not
  SoH's section settings. Nobody has seen the new Ocarina of Time groups on screen.
- **Keep Items on Cycle Reset (#1194):** logic does not read it, as OoTMM's does not, and the spoiler does not name
  it. OoTMM's separate Keep Bottles on Cycle Reset is not built.
- **Mods (#1197, #1198, #1206):** the January namespacing renamed string literals as well as symbols. The class is
  closed: the tool is fixed, its sweep found one more lookup key (the debug-print font path, above), and every
  damaged key is restored and locked by the Python invariant; the prefixed literals left are console, crash-screen
  and debug messages. Issue #1063's crash with a texture pack that
  "worked for a while" is still open: the skeleton crash was a different defect. Same-named archives in different mod
  folders never both mount (issue #1196, inherited from Ship of Harkinian).
- **Majora's Mask randomizer:** the monkey's vanilla Sonata (issue #1203) and the Hags' free potion (issue #1228)
  are fixed (#1225, #1250, above). Under Shuffle Time the time-left text still counts every owned half-day from the
  start of the cycle (cosmetic, no issue).
- **Cross-game outbox (#1250, #1264):** the lost pickup of issue #1222 is fixed (above). An Ocarina of Time pickup
  held for a full outbox at a quit or a file load, before a crossing, keeps its check collected and is not given;
  with 2048 slots this needs 2048 un-redeemed pickups. In Hyrule a shop purchase held that way is charged at the
  purchase and given after the next crossing. Nobody has carried more than 64 pickups across in play, nor opened a
  version-4 file on this build by hand (playtest guide, W14).
- **Ocarina of Time randomizer:** the Deku Theater's capacity upgrade under Shuffle Jabber Nuts (issue #1217) is fixed
  (#1226, above).
- **Majora's Mask settings (#1242 to #1271):** the rows landed (above); issue #1272 tracks what their reviews left
  over: in Termina the last full heart keeps its default red until the Majora's Mask-only Beating Hearts row is set
  (SoH's Hearts colour does not cover it, as it does in Hyrule); the magic-arrow hit flash stays vanilla in Termina;
  the import's once-per-install stamp and its uncarried colours (above); Faster Song Playback sits in the Majora's
  Mask Ocarina block rather than beside SoH's Fast Ocarina Playback; and 2Ship2Harkinian's About panel. A rainbow
  colour in the Cosmetics Editor holds still in Majora's Mask, and Randomize All, Reset All and Lock All skip the
  Majora's Mask rows (#1259). A recoloured Majora's Mask sword or form trail draws its inner edge opaque, where
  Ocarina of Time's keeps its fade (#1271). Finishing, skipping or removing the last Majora's Mask time split marks
  the file complete, as in 2Ship2Harkinian: Majora's Mask's playtime and its In-Game Timer then stop for that file
  (#1258). Ammo Buyback sells the shared ammo, so a sale in Termina lowers Hyrule's count too (one pool, #1256). Time
  Moves When You Move may save a stopped clock if you save while standing still (suspected, inherited from
  2Ship2Harkinian; playtest guide, W6). Nobody has played any of the rows; the playtest guide's section W lists one
  step per group.
- **Networking:** Ship of Harkinian's Network > Anchor page is present in release builds but unsupported on a paired
  file: it knows nothing of the pair, and a session is unsound until issue #574 (multiworld over the combo; issue
  #1272 asks for the page to be gated meanwhile).
- **Save editing:** there is no in-app way to edit a Termina save (issue #1231); Ship of Harkinian's Save Editor
  draws nothing while Termina runs and must never be used on Majora's Mask.
- **Keys and reticles (#1212, #1213):** a customised Non-Hookshotable Reticle colour applies to Hyrule's Bow reticle
  but not to Termina's, which stays red (a follow-up). Nobody has seen the Termina reticle, the mask and fairy chests
  or the key models on screen.
- **Woodfall Temple (issue #1070, resolved by the 2026-10-27 retest):** with Dungeon Access at its default (Transformation &
  Song), Majora's Mask's own trigger raises the temple only when Deku Link plays the Sonata standing on the Deku
  Scrub symbol on the floor between the pink flower and the owl statue; played on the flower, or where the Song of
  Soaring sets you down, the song gives its full announcement and nothing happens. A short chime as Deku Link takes
  out the pipes confirms the spot. A Sonata that crossed from Hyrule works the same as one learned in Termina
  (playtest guide, section U). The 2026-10-21 report, where no song with C-up was recognized, came from the D-pad
  ocarina on `v0.2.0-prealpha`, which played no note in Termina until `v0.3.0-prealpha` (issue #1062, #1099). The
  operator's retest on the `347ba755` build raised the temple at the first attempt (Read this first; playtest guide,
  U1).
- **Reset in Majora's Mask (issue #1274):** the menu header's Reset button and Ctrl+R do nothing while Majora's Mask
  runs: both send the console command `reset`, which is Ship of Harkinian's and refused outside Ocarina of Time
  (issue #798), and Majora's Mask's own reset is not linked. Cross back to Hyrule and reset there, or quit.

### Run record

The pull requests were merged once their hosted CI was green, and the pyramid driver runs the ROM tiers on `main`
itself. On `main`, `integration` passed 21 of 21 at `7c1aecf9` (cycle 25; `integration-soak` 1 of 1), 21 of 21 at
`095c8b83` (cycle 27, with #1177 to #1183; `integration-soak` 1 of 1) and 21 of 21 at `51bcfa4c` (cycle 28, with
#1178, #1184, #1185, #1186, #1188 and #1193; `integration-soak` 1 of 1). Cycle 29, at `0be75d87` (with #1192, #1197,
#1199 and #1201), was the first to run the unit tiers on `main` as well: `redship` 244 of 244, `rando` 77 of 77,
`integration` 21 of 21, `integration-soak` 1 of 1. The gate on `main` runs the unit tiers since issue #1200: `ComboCrossingViewsWorld` was
red in `main`'s hosted CI from `d94ab061` (#1184's chu-pack names on #1178's placements) until #1201, and a gate of
the ROM tiers alone did not see it. Cycle 30, at `38a5138c` (with #1194, #1198, #1206, #1207 and #1209), and cycle
31, at `722da4f8` (with #1211, #1212, #1213 and #1223), passed every tier: `redship` 248 of 248 and 252 of 252, `rando`
78 of 78 and 79 of 79, `integration` 21 of 21 and `integration-soak` 1 of 1 both times. Cycles 32 to 37
passed every tier too, at `3772e70d` (with #1205, #1224, #1225, #1227 and #1230), `c9fd36de`, `7ec19332`, `9ceea4bc`,
`11b2212b` and `791f26d3`: every `redship` row (260, 262, 263, 265, 267 and 271), every `rando` row (79, 80, 80, 80, 80
and 81), `integration` 21 of 21 and `integration-soak` 1 of 1 each time. Cycle 38, at `347ba755` (with #1268, #1259,
#1260, #1267, #1270, #1269 and #1271), passed every tier as well: `redship` 273 of 273, `rando` 81 of 81, `integration`
21 of 21, `integration-soak` 1 of 1. The record is under "Gates, 2026-10-26/27" in [`known-issues.md`](known-issues.md).
The release commit changes only the version string, the release workflow's title and docs, so cycle 38 is the code
record for `v0.4.0-alpha`. Beyond the operator's Woodfall run of 2026-10-27, nobody has played it.

---

# Release notes: v0.3.1-prealpha ("Ripley Bravo")

**Status:** released. The release is cut as the tag `v0.3.1-prealpha` on the commit that merges this text (the version
bump and these notes), whose code is `main` at `c1023282`. The next release's draft starts above this section when
something merges after the tag.
**Version:** `v0.3.1-prealpha`, codename "Ripley Bravo" (`src/common/rsbs_version.h`). Under [`VERSIONING.md`](VERSIONING.md)
that is a `z` bump: the film character stays and the code word moves from Alfa to Bravo. The archive version
(`project(Ship VERSION 9.1.1)`) is unchanged, but a paired file from `v0.3.0-prealpha` does not carry over (Read this
first). A build's window title reads `RedShipBlueShip v0.3.1-prealpha+<commit> (Ripley Bravo)`, and `redship --version`
prints the same first line.
**Last release:** `v0.3.0-prealpha` ("Ripley Alfa"), tagged 2026-10-22 at `f2f3d64f` (code tip `30b841c1`). Its notes
follow this section.
**Merged up to:** `main` at `c1023282` (2026-10-23), 21 commits after the tag: pull requests #1111 to #1155 (#1115 is
the previous release itself and #1148 and #1155 are docs for these notes; the numbers between them that are not listed
are issues or the revert #1150, closed unmerged).
**Covers:** everything merged after `f2f3d64f`. Numbers in parentheses are pull requests; issues are written "issue #N".
Nobody has played any of it: every change below is locked by test rows.

---

## Since v0.3.0-prealpha

### Read this first

- **Start a new paired file: files from `v0.3.0-prealpha` do not carry over.** Shuffle Tree Drops (#1125) added 53
  checks to Majora's Mask's check table, which sits in the middle of Majora's Mask's half of the save, so every field
  after it moved, the world's seed among them. A paired randomizer file saved by any build before `fabe7d33`
  (`v0.3.0-prealpha` included, and the `main` builds after the tag with #1111 to #1138) no longer reads as its own
  Majora's Mask world, and the file select refuses it ("Refused: This file has no Majora's Mask world"). The operator
  accepted this break before release (2026-10-23); there is no migration. Keep the build that wrote an old file if
  you want to finish it. See "The `v0.3.0-prealpha` save break" under "Save
  loss and corruption" in [`known-issues.md`](known-issues.md).
- **Files saved by this version do not open in `v0.3.0-prealpha`.** From `efe573ec` the save format is version 4 (the
  combo record grew from 1024 to 2048 bytes to keep the Combo Tracker's reachability for the game you are not playing,
  #1147), and `v0.3.0-prealpha` refuses a version 4 file as made by another build. A `v0.3.0-prealpha` file that
  this version opens at all (the save break above refuses paired ones) loads, and shows no reachability for the other
  game until you next cross.
- **Generate now makes the whole paired world** and writes it to a seed package in `Randomizer/` (next section). The
  progress overlay shows when you press Generate; creating the file is instant.
- **Seeds.** With every new option at its default (Shuffle Tree Drops off), no world moved after the tag: the same seed
  and settings make the same world with the same fingerprint (`comboSettingsHash`, `mmFinalSeed` and every placement
  field of the golden digests are unchanged). Eight golden digests re-pinned one field, `mmPlacementHash`, because that
  hash counts every Majora's Mask check, the 53 tree checks included, whether or not they are shuffled (#1125). With
  Shuffle Tree Drops on, or a shared song rule on while Shuffle Songs is not Anywhere (#1111), the world differs.

### Seed packages (issue #1044)

- **Generate builds the whole paired world** (both games' placements, the crossings and the spoiler) and writes it to
  `Randomizer/<icons>-<fingerprint>.rsbs-seed.zip`. The "Creating Your Paired World" overlay now shows at Generate. A
  generation that runs out of budget fails Generate with "Not generated: try a new seed or other options."
- **File creation uses the package and generates nothing**, so it is instant, and settings changed after Generate no
  longer change the world.
- **Randomizer > General > Seed Package:** a list of the packages in `Randomizer/`, "Load seed package" (stages one
  for the next file) and "Export seed package" (writes the staged one under a name you type, to share). A line names
  what is staged. A package from another generator version is refused ("Refused: Seed package from another
  generator") and nothing is staged (#1116).

### Majora's Mask randomizer

- **Shuffle Tree Drops** (Randomizer > MM Randomizer > Shuffle Items, after Shuffle Grass Drops; off by default),
  ported from 2Ship2Harkinian's Tree Shuffle: bonking into each of 53 trees that shake is a check, and trees that do
  not shake have no drops. The Gorman Track trees are not included. A tree check only ever holds a Majora's Mask item
  (#1125, issue #1121).

### Cross-Game Rules and shared items

- **A capacity upgrade found in the other game raises your tier at the pickup.** Majora's Mask's Progressive Bomb Bag
  on an Ocarina of Time check, or Ocarina of Time's Progressive Wallet in Termina, now raises the finder's tier at
  once, as a bombchu pack already did; the other game reads it from the shared pool at its next arrival. This covers
  the bomb bag, quiver, wallet, magic, Double Defense, Pieces of Heart and Heart Containers, the hookshot, and bottles
  under Shared Bottles. It used to arrive only after a round trip through the other game (#1122, issue #1120).
- **Shared Epona's Song, Shared Song of Storms and Shared Song of Time place one copy** whatever Ocarina of Time's
  Shuffle Songs says; the spot Ocarina of Time's own copy would have taken holds another item. With Shuffle Songs Off,
  the song's vanilla check (Malon, the Windmill, the Ocarina of Time) holds whatever the fill puts there (#1111, issue
  #1085).
- Groundwork for wearing the other game's masks (issue #977): no change in play until the mask items land (#1112).

### Majora's Mask now honours more of the shared settings

- **Skip Boss Introductions** (Enhancements > Skips & Speed-ups) now works in Majora's Mask: boss and miniboss intro
  cutscenes and the boss blue-warp cutscenes are skipped (#1124, issue #1123).
- **Damage Multiplier:** a new "MM Damage Multiplier" row on Enhancements > Difficulty, under Ocarina of Time's, with
  Majora's Mask's own values (Vanilla 1x to Foolish 16x, or 1 Hit KO). The two rows stay separate because the two
  games' value lists differ (#1119, issue #1037).

### Cross-game items

- Ocarina of Time's shop shelves and get-item cutscene show Majora's Mask's stray fairies (in their area's colour) and
  the bottled fairy instead of the "?" stand-in. The fairy is a still figure (#1113, issue #964).

### Trackers

- **The Combo Tracker follows Check Tracker Settings:** its Hide options, Filter Empty Areas and header toggles (Show
  Hidden Items, Expand/Collapse Buttons, Search Input, Check Totals). Each game pane has its own Show Hidden Items. The
  list is always drawn (there is no "Checks" node to open), and "Checks: n / m" counts skipped checks as checked, as
  both native trackers do. Its Crossings section is gone (the Cross-Game Spoiler still lists crossings), so Toggle
  Combo Tracker is no longer disabled in race mode (#1137, issue #1128).
- **The Combo Tracker's Ocarina of Time pane lists what Ocarina of Time's Check Tracker lists:** excluded and
  unshuffled checks are out, and Always Show Gold Skulltulas and Hide Unshuffled Shop Item Checks apply. While you
  play Majora's Mask it keeps the list from your last time in Hyrule (#1138, issue #1126).
- The Combo Item Tracker draws Majora's Mask's ocarina-button icons in Hyrule before Majora's Mask has started, where
  it drew the item's name as clipped text before (#1144).
- **The Combo Tracker shows which checks you can reach** in both games when Check Tracker Settings > Enable Available
  Checks is on: SoH's lock or open lock before each check, "N Available / Checks: x / y" on the totals
  line, "(available / checked / total)" on each area, and an Only Show Available Checks box in each game's pane (each
  pane its own). The game you are not playing shows what was reachable when you left it, kept in the save; it does
  not yet change as you collect things in the other game (issue #1146). Majora's Mask's pane refreshes every 20
  frames, as its own Check Tracker does (#1147, issue #1129; option 1 of issue #993).
- **The Combo Item Tracker's Majora's Mask section shows Majora's Mask's tracker rows** after Quest: a Heart Pieces
  count, Tokens (Swamp and Ocean Gold Skulltula Tokens against the file's minimum, 30 on a vanilla file), Stray
  Fairies (Clock Town, and each temple's count against the file's minimum, 15 on a vanilla file) and Dungeon (one
  column per temple: boss key, small keys found against the temple's total, lit while one is held, map and compass).
  When the file's options shuffle them, Boss Souls (no Majora under Triforce Hunt), Enemy Souls (one n/47 count),
  Owl Statues, Time (the six half-days) and Misc (Ability to Swim) follow the Ocarina Buttons. They draw as icons in
  Hyrule before Majora's Mask has started, from the curated icons of #1144 (#1149, issue #1140). At 1280x800 the
  overlay now draws at about 72% of SoH's size to fit Majora's Mask's longer section.
- **The Combo Tracker follows SoH's Check Tracker window settings** (Combo > Tracker Settings): Window Type (Floating
  overlay or Window), Enable Dragging, Only Enable While Paused, Display Mode (Always or Combo Button Hold), Background
  Color and Font Size. As Floating it has no title bar or close button, as SoH's overlays; as Window it keeps both
  (#1154, issue #1130).

### Fixes

- Majora's Mask's pause-menu and game-over saves on file 2 or 3 address that file's own save pages instead of file
  1's backup or file 2's save (inherited code, issue #599). In this executable Majora's Mask's saves go to the
  `.redsave`, so nothing changes in play (#1117).

### For contributors

- Test processes ignore game controllers: every `--test` and `--integration-test` process keeps SDL's joystick
  background events off and opens no controller, and each `[FOCUS]` line ends `controllerBackground=0
  openControllers=0` (any other value fails an integration run). A pad in use while the tiers run no longer moves
  Link in the unfocused test windows (#1153, issue #1152; row `TestControllersIgnored`).
- The integration rows accept the single-bag line that Generate's completion prints, and the integration stderr tee
  keeps it; five paired-creation rows had been red since #1116 (#1136, issue #1135).
- Save-writing test rows each use their own directory, so rows that run at the same time no longer share save files
  (#1118, refs issues #824, #925).
- The paired determinism digest no longer carries `mmReachableCount` / `mmReachableHash`, which generation does not
  use (#1114, issue #996).

### Known limitations added since v0.3.0-prealpha

- **The save break above** (#1125): a paired file from any build before `fabe7d33` (`v0.3.0-prealpha` included) is
  refused. Start a new file.
- **Seed packages from `main` builds before `efe573ec` are refused** (#1147). No released build wrote a seed package
  (they are new in this release, #1116), so a player coming from `v0.3.0-prealpha` has none. A package exported by a
  `main` build from `bc93e65e` to `e3de3ff1` is refused with "Refused: Seed package damaged or incomplete". From
  `fabe7d33`, `4fc4fd41` or `e3de3ff1` the log reads "the combo record is 1004 bytes; this build's is 1712", and issue
  #1151 would accept those packages; from `bc93e65e` to `f29a1a82` (before #1125) it reads "MM's half is N bytes; this
  build's is M", which #1151 does not change. Generate the seed again. See "Seed packages from `main` builds before
  `efe573ec` are refused" under "Randomizer generation" in [`known-issues.md`](known-issues.md).
- Ocarina of Time's Progressive Deku Stick and Nut upgrades found in Termina still cross and arrive at Ocarina of
  Time's next arrival, so Shuffle Deku Stick/Nut Bag keeps gating sticks and nuts. The ocarina still crosses as an
  ordinary item (#1122).
- With Shared Kokiri Sword on, Ocarina of Time's Kokiri Sword found in Termina still crosses and arrives at Ocarina
  of Time's next arrival, instead of stepping the shared sword at the pickup (issue #1145).
- With a shared song rule on and Shuffle Songs Off, the song's vanilla check holds a shuffled item, but Ocarina of
  Time's Check Tracker hides song checks under Off, so it is not listed there (#1111).
- Skip Song Cutscenes, Quick Boss Deaths and Exclude Glitch-Aiding Cutscenes are Ocarina of Time only. Majora's
  Mask's song-teacher cutscenes follow Skip Story Cutscenes, and its boss blue-warp cutscenes follow Skip Boss
  Introductions. Skip First Cycle has no row: the crossing already starts Majora's Mask past its first cycle. Tatl's
  long flashback at the tree in southern Termina Field, on the road to the swamp, follows Skip Misc Interactions, as
  in 2Ship2Harkinian (#1124).
- In a randomizer file whose config never touched the cutscene-skip checkboxes, Ocarina of Time skips story, entrance
  and boss-intro cutscenes by default but Majora's Mask plays them; tick the boxes once to apply them to both
  (#1124).
- The Combo Tracker's Saved Hide never hides a Majora's Mask check: every obtained Majora's Mask check counts as
  Collected, never Saved, so Collected Hide hides them even when saved (#1137).
- A saved Always Show Gold Skulltulas or Hide Unshuffled Shop Item Checks reaches the Combo Tracker's Ocarina of Time
  pane only once Ocarina of Time's Check Tracker window has been shown since launch (#1138).
- The Combo Tracker's Majora's Mask pane groups enemy drops and unconnected grottos differently from Majora's Mask's
  own Check Tracker (issue #1127, deferred).
- **Reachability in the Combo Tracker** (#1147): the pane of the game you are not playing keeps what was reachable
  when you left it and does not follow what you collect in the other game (issue #1146). Its "Available" counts can
  read higher than SoH's Check Tracker with Hide options on, because they count every listed check where SoH leaves
  out the checks its Hide options hide (issue #1146).
- **Combo Item Tracker, Majora's Mask rows** (#1149): with Owl Statue shuffle on, the owl cells draw wider than their
  cell and overlap their neighbours (the overlay does not yet clamp Majora's Mask's 46x24 owl face; lane IT-3, issue
  #1141). Majora's Mask's small keys show the keys found against the temple's total; SoH's Key Counts display mode
  is not honoured for them yet (issue #1143). The Heart Pieces row reads Majora's Mask's last snapshot of the shared
  pieces, so a piece found in Hyrule shows in the Shared section at once and in Majora's Mask's row only after the
  next crossing.
- Under Ocarina of Time both SoH's Check Tracker and the Combo Tracker draw under the same window settings, until
  SoH's window retires (lane L5 of the tracker scoping) (#1154).
- A stray fairy on an Ocarina of Time shelf or in its get-item cutscene is a still figure (no wing flap or colour
  pulse). The other items 2Ship2Harkinian draws with its own code (owl statues, souls, clocks, frogs, the swim ability,
  Triforce pieces, traps and ocarina buttons) still show "?" there (#1113).

### Open decisions (operator), added since v0.3.0-prealpha

- #1124: whether Majora's Mask's song-teacher cutscenes should follow Skip Song Cutscenes instead of Skip Story
  Cutscenes, and whether its boss blue-warp cutscenes should follow Skip Story Cutscenes (as Ocarina of Time's do)
  instead of Skip Boss Introductions. Both keep 2Ship2Harkinian's grouping for now.
- Issue #1123: whether Tatl's flashback on the road to the swamp should also be skipped under Skip Story Cutscenes.
  It follows Skip Misc Interactions only, as in 2Ship2Harkinian.
- Issue #993: its option 1, reachability in the Combo Tracker, is built (#1147); the issue stays open until the
  operator picks that option. Majora's Mask's own Check Tracker still has no menu row.
- #1149: whether Majora's Mask's Heart Pieces row stays (it shows a shared quantity a second time, from Majora's
  Mask's snapshot) or is dropped or read from the shared pool.

### Run record

The pull requests were merged once their hosted CI was green, and the pyramid driver runs the ROM tiers on
`main` itself (the replan of 2026-10-22). On `main`, `integration` passed 16 of 21 at `bc93e65e` (cycle 20: five
paired-creation rows waited for the log line #1116 renamed, issue #1135), 21 of 21 at `f416ecee` (cycle 21, with
#1136's fix; `integration-soak` 1 of 1), 16 of 21 at `fabe7d33` (cycle 22; `integration-soak` 0 of 1), 21 of 21 at
`e3de3ff1` (cycle 23; `integration-soak` 1 of 1) and 21 of 21 at `af4d941f` (cycle 24, with #1147, #1149 and #1153;
`integration-soak` 1 of 1). The cycle 22 red was not in the code: the operator's game controller was driving the
test windows. libultraship lets SDL deliver joystick events to a window without focus, so a moved stick walked Link
(the camera assertion failed) and pressed buttons took doors or loaded a file (warps and boots arrived at the wrong
entrance), with the failure point moving between runs. The driver's bisect assumed the last green tip was still
green, so it blamed #1137; its revert (#1150) was closed unmerged and #1137 cleared. Cycle 23 ran with the pad idle,
and since #1153 (issue #1152) a test process ignores controllers altogether. The full record is in the two
"Gates, 2026-10-23" sections of [`known-issues.md`](known-issues.md). Nobody has played any of it.

The release commit itself (`c1023282` plus the version bump and these notes) was gated on the workstation (Windows,
ROM archives mounted): `redship` 240 of 240, `rando` 74 of 74, `integration` 21 of 21. Two capture rows,
`IntGameplayRoundtripCapture` and `IntPairedFileSelectIdentity`, first failed because the build tree's
`shipofharkinian.json` had fallen back to libultraship's Windows default, DirectX 11, where no frame capture is
written; on the OpenGL backend the test setup requires, both passed.

---

# Release notes: v0.3.0-prealpha ("Ripley Alfa")

**Status:** released. The release is cut as the tag `v0.3.0-prealpha` on the commit that merges this text (the version
bump and these notes), whose code is `main` at `30b841c1`. The next release's draft starts above this section when
something merges after the tag.
**Version:** `v0.3.0-prealpha`, codename "Ripley Alfa" (`src/common/rsbs_version.h`). Under [`VERSIONING.md`](VERSIONING.md)
that is a `y` bump: major features and fixes without a new `oot.otr` (the archive version, `project(Ship VERSION
9.1.1)`, is unchanged), so it takes a new film character and the code word starts again at Alfa. A build's window
title reads `RedShipBlueShip v0.3.0-prealpha+<commit> (Ripley Alfa)`, and `redship --version` prints the same first
line.
**Last release:** `v0.2.0-prealpha` ("Trinity Alfa"), tagged 2026-10-20 at `1ace3c7b` (code tip `7671196a`). Its notes
are the release's own text and this file as it stood at the tag
([`release-notes-next.md` at `v0.2.0-prealpha`](https://github.com/spencerduncan/redshipblueship/blob/v0.2.0-prealpha/docs/release-notes-next.md)).
They are kept below, unchanged, under "v0.2.0-prealpha, as released".
**Merged up to:** `main` at `30b841c1` (2026-10-22), 33 commits after the tag: pull requests #1073 to #1108 (#1085 is an
issue; #1103 and #1110 were the pyramid driver's landing PRs, and #1109 was a stale landing PR closed unmerged). #1081 to
#1100 landed through the two landing PRs, so each of their commits on `main` still carries its own PR number.
**Covers:** everything merged after `1ace3c7b`. Numbers in parentheses are pull requests; issues are written "issue #N".
Nobody has played any of it: every change below is locked by test rows and, for the menu, by the UI harness.
**Get it:** the [release page](https://github.com/spencerduncan/redshipblueship/releases/tag/v0.3.0-prealpha) carries
`redshipblueship-v0.3.0-prealpha-windows.zip` and `redshipblueship-v0.3.0-prealpha-linux.AppImage` once the tag's
`generate-builds` run finishes. What to test first is section P of [`playtest-2026-09.md`](playtest-2026-09.md).

---

## Since v0.2.0-prealpha

### Read this first

- **Create a new paired file for this release.** The Majora's Mask half of a paired save gained two fields after the
  tag: Moon Crash Behavior (#1102) and the excluded-check list of Randomizer > MM Locations (#1106). Each is appended
  inside Majora's Mask's save, so it shifts the fields after it. A paired file saved by an earlier build,
  `v0.2.0-prealpha` included, loads as before while its Majora's Mask settings still match the ones it was created
  with. If they were changed at the title screen, so that the load has to restore them, the file is refused as
  unrestorable ("Refused: ..." at the file select). Pre-alpha saves are disposable; back up any file you care about.
- **Seeds.** With every new rule and option at its default, no golden world moved after the tag
  (`git diff 1ace3c7b 30b841c1 -- tests/golden/` is empty). A rule or option that is on, or a check moved to Forced
  Junk, changes the world and its fingerprint.
- **The randomizer pages moved.** Majora's Mask's randomizer pages and the Cross-Game rules are now under the
  Randomizer header (next section). A menu last left on one of the old Combo pages opens on it in its new place.

### One randomizer menu for both games (issue #1101)

The operator's ruling of 2026-10-21 (option C): one randomizer menu, as OoTMM has one generator, with every Ship of
Harkinian page left as it ships.

- **Majora's Mask's randomizer pages and the Cross-Game rules are pages of the Randomizer header**, after
  Tricks/Glitches and before Plandomizer, in this order: MM Randomizer, MM Locations, MM Tricks, Cross-Game. The
  "Cross-Game" page that used to say "Moved to Combo" is now the rules page itself. The Combo header keeps Windows,
  Majora's Mask, MM Mods, Save Files and Tracker Settings. A menu last left on one of the moved pages opens on it in
  its new place. Ship of Harkinian's own Randomizer pages, rows and wording are unchanged (#1104).
- **The same words as Ship of Harkinian and OoTMM.** Majora's Mask's randomizer options use SoH's section names
  (Logic, Win Condition, Area Access, Shuffle Items, Shuffle Shops & Merchants, Additional Items, Traps, Hints,
  Starting Items), and the Cross-Game page groups every shared-item rule under Shared Items, in OoTMM's order. Our
  randomizer pages share one status sentence ("These apply to the next world you generate. Loading a paired file
  shows its own."), and with a paired file loaded each says it was already decided and shows the fingerprint the
  file select prints beside the seed icons. MM Tricks' count of supported tricks is a gray note of its own (#1105).
- **Randomizer > MM Locations (new):** Majora's Mask's excluded checks, an Included / Forced Junk table drawn like
  SoH's Locations page, between MM Randomizer and MM Tricks. A check moved to Forced Junk holds junk in the next
  world. The list is part of the world: it is greyed once the world is created (and under Vanilla logic, which never
  reads it), and loading a paired file restores its own list, with a toast naming "Forced Junk Checks" (#1106).
- **Majora's Mask's trap types are rows:** Freeze, Blast, Shock, Jinx, Wallet, Like Like and Time Traps, under Traps
  on Randomizer > MM Randomizer, after Shuffle Traps and Trap Count. They used to be console-only. They are frozen
  with the world like every other MM option, and Reset MM Randomizer clears them; with none ticked, every trap is a
  Freeze Trap (#1107).
- The release notes, known issues and playtest guide were brought up to the merges after the tag (#1108).

### Cross-Game Rules: OoTMM's Shared Items

Each is a "[Both Games]" checkbox on Randomizer > Cross-Game under Shared Items (#1104, #1105), off by default, named
after OoTMM's option. With a rule on, one copy of the item is placed for both games, both games' logic counts it, and finding it in
either game gives it to you in the other at your next crossing. The rules freeze when the world is created, and
loading a paired file puts its own rules back. `v0.2.0-prealpha` refuses a file created with any of them on, and says
so by name.

- **Shared Shields** (OoTMM's sharedShields): Ocarina of Time's Hylian Shield and Majora's Mask's Hero's Shield are one
  item, and the two Mirror Shields are one item. In Majora's Mask the Mirror Shield replaces the Hero's Shield. Shops
  still sell each game's own shield (#1080).
- **Shared Lens of Truth** (sharedLens) (#1081).
- **Shared Epona's Song** and **Shared Song of Storms** (sharedSongEpona, sharedSongStorms) (#1083).
- **Shared Ocarina Buttons** (sharedOcarinaButtons): with both games' ocarina-button shuffles on, each button found in
  one game is yours in the other on your next arrival, and the Item Tracker shows the five buttons once, in the
  Shared group (#1084).
- **Shared Fire Arrows**, **Shared Ice Arrows** and **Shared Light Arrows** (sharedMagicArrowFire, Ice, Light)
  (#1088).
- **Shared Bottles:** the bottle count is shared up to Ocarina of Time's four, and each bottle's contents stay per
  game. Four bottle-slot items are placed across both games (Ruto's Letter is one of them while Zora's Fountain is
  closed). This differs from OoTMM's sharedBottles, which shares six bottles with their contents (#1094).
- **Shared Goron Mask**, **Shared Zora Mask**, **Shared Bunny Hood**, **Shared Keaton Mask** and **Shared Mask of
  Truth** (sharedMask*). With Shared Bunny Hood on, the Bunny Hood also gives Majora's Mask's speed boost in Hyrule,
  whatever Enhancements > Bunny Hood Effect says (#1095).
- **Shared Song of Time** (sharedSongTime): the Song of Time is one song for both games, Majora's Mask starts a paired
  world with the Ocarina alone, and its moon crash always starts a new cycle that keeps your progress (#1102,
  issue #1069).
- **New Majora's Mask option, Moon Crash Behavior** (OoTMM's moonCrash), on Randomizer > MM Randomizer under Logic:
  Last Save (the default: the crash restores the file's last save, as before) or New Cycle (a new cycle starts at the
  Dawn of the First Day and you keep your items and progress, as with the Song of Time). It is frozen with the world
  like every MM option, and Shared Song of Time forces New Cycle whatever it says. Forcing it is a divergence from
  OoTMM, which reasons about time resets in its logic instead (#1102).

### Majora's Mask now honours more of the shared settings

These rows were drawn for both games but did nothing in Majora's Mask, because MM's code for them was not linked into
the executable or did not read the shared key. Each now takes effect there too (audit issue #966).

- **Cheats:** Infinite Health, Magic and Money, Climb Everything, Hookshot Everything, Moon Jump on L and Easy Frame
  Advancing (#1079).
- **Skips & Speed-ups:** Disable Title Card, Skip Intro, Skip Entrance Cutscenes, Skip One Point Cutscenes and Skip
  Story Cutscenes (#1082).
- **Enhancements:** Fast Chests, Enemy Health Bars, Aiming Reticle for the Bow/Slingshot, Disable Black Bar
  Letterboxes, Instant Putaway, Unequip C-Items on Re-press (with D-pad Equips' page-switch prevention), Pause Buffer
  Input Window and Hyper Enemies (#1086).
- **Audio Editor:** Mute Low HP Alarm, Disable Enemy Proximity Music and Display Sequence Name in Notifications
  (#1087).
- **Free Look**, Camera Distance and Camera Transition Speed (#1089).
- **Frame interpolation** (Settings > Graphics) now smooths Majora's Mask as well as Ocarina of Time (#1091).
- **Disable Screen Flash for Finishing Blow**, Enable 3D Dropped Items/Projectiles, Skip Misc Interactions, Skip
  Playing Scarecrow's Song and Arrow Cycle (#1093).
- **Skip Magic Arrow Equip Animation**, Permanent Heart Loss, N64 Weird Frames, the ocarina D-pad, right-stick and
  dropped-input options, and Containers Match Contents. Turning Permanent Heart Loss off restores lost containers in
  Ocarina of Time only (#1099).
- **Skip Get Item Animations** (Randomizer > General) also decides Majora's Mask's get-item scenes: Off shows every
  one, Junk Items skips junk and repeated pickups, All Items skips them all. Story-cutscene gives on non-randomizer
  files keep their hold unless the setting is All Items. Majora's Mask's own Skip Item Get Cutscene key is no longer
  read; a value set by hand in `shipofharkinian.json` is ignored (#1100).

### Randomizer

- Majora's Mask's song-teacher checks (the Deku King's chamber monkey, Igos du Ikana, Romani, the Soaring engraving,
  Flat's tomb and the Goron Lullaby checks) give the item the seed placed there instead of teaching the vanilla song
  (#1078).
- Bombchu packs found in the other game add to the shared bombchu count the moment they are picked up; Majora's Mask
  no longer loses them to its bomb-bag cap (#1077).
- Business Scrubs' offer text follows Scrub Hint Text instead of Merchant Hint Text, as the setting's description and
  the Check Tracker do. Merchant Hint Text still governs the other merchants. This differs from Ship of Harkinian,
  which reads Merchant Hint Text for scrubs (#1074, issue #855).
- Under the combo Triforce Hunt, Ocarina of Time's own Triforce Hunt set to "Ganon's Boss Key" is generated as "Win",
  because the combo hunt's pieces end the game (#1090; a paired triforce-hunt world is still refused at Generate).
- A spoiler kept from an older build no longer stops a new paired file of the same seed from getting its spoiler: the
  new one is saved beside it, named with the world's fingerprint (`Randomizer/<icons>-<fingerprint>.json`) (#1076,
  issue #1055).

### Trackers

- The Combo Item Tracker shows Majora's Mask's five ocarina buttons when the file shuffles them, so a missing C-up is
  visible (#1075). With Shared Ocarina Buttons on they are shown once, in the Shared group (#1084).
- The Combo Item Tracker shows the pair's Bow, Bomb Bag, Deku Stick and Deku Nut capacity, and the shared Kokiri Sword
  when that rule is on, once in the Shared column; each game's own cell for it is faded (#1096).
- The Shared column follows the game you are playing, so a hookshot or wallet found in Hyrule shows there at once and
  the rupees match the HUD (#1097).

### Fixes

- Changing a colour in the Cosmetics Editor while Majora's Mask is running no longer patches Majora's Mask's models,
  and Ocarina of Time shows the new colour from its first frames after the next crossing (#1092, issue #1040).

### For contributors

- A release tag pushed as an annotated tag checks out in CI (#1073, issue #1059).
- Test rows drive both check trackers' reachability: Ocarina of Time's availability across an arrival, Majora's
  Mask's in-logic set after an apply, and the fill against the tracker (#1098, issue #994).

### Known limitations added since v0.2.0-prealpha

- With Shared Epona's Song, Shared Song of Storms or Shared Song of Time on and Ocarina of Time's Shuffle Songs at
  Song Locations (the default), Dungeon Rewards or Off, the world holds two copies of that song: Ocarina of Time's own
  and the shared one. Either teaches both games. Set Shuffle Songs to Anywhere for a single copy (issue #1085; a fix
  that places one copy whatever Shuffle Songs says is in review, PR #1111, and is not in this release).
- Under the plentiful item pool, Ocarina of Time still shuffles a surplus second copy of a shared Lens, arrow or mask.
- With Shared Shields on, a Mirror Shield found in Termina before any Hero's Shield also gives Ocarina of Time the
  Hylian Shield; Ocarina of Time's own Hylian Shield stays in the item pool beside the shared one; and the Item
  Tracker lists each game's shield in its own section rather than once under Shared.
- While Ocarina of Time is running, the tracker's Shared mask rows use Majora's Mask's icons.
- With Shared Bottles on, Majora's Mask's fifth and sixth bottle slots stay empty.
- Moon Crash Behavior's New Cycle (and the moon crash under Shared Song of Time) does not yet let you save on the
  Clock Tower roof, as OoTMM's does (#1102).
- Fast Chests does nothing in a Majora's Mask randomizer file: the chest contents belong to the randomizer.
- D-pad in text boxes, D-pad hold change, D-pad on the pause screen and Customize Ocarina Controls are Ocarina of
  Time only. Damage Multiplier is still Ocarina of Time only (issue #1037). Skip Enemy Cutscenes and Skip To File
  Select still do nothing in Majora's Mask.
- Majora's Mask's free look ignores the remote bombchu's camera focus.
- **Older paired files** (#1102, #1106): the Majora's Mask half of a paired save gained two fields (Moon Crash
  Behavior and the excluded-check list), and each shifts the fields after it. A paired file saved by an earlier
  build, `v0.2.0-prealpha` included, that needs its Majora's Mask settings restored at load (because they were
  changed at the title screen) is refused as unrestorable. A file whose settings were not changed loads as before.
  Create a new file for this release (see Read this first).
- The Randomizer sidebar has 16 entries (Ship of Harkinian's 12 and our MM Randomizer, MM Locations, MM Tricks and
  Cross-Game), so it scrolls in a small window; Ship of Harkinian's own 12 already scrolled at 832x600. The pull
  requests estimated this from the menu code (about 10 entries fit at 832x600, about 14 at 1280x800); nobody has
  measured it on screen (#1104, #1106).
- New reports, open: Woodfall Temple does not rise after the Sonata of Awakening (issue #1070: the playtester's save
  and the code are clean; the lead is Majora's Mask's rule that holding L while playing cancels songs in free play,
  and the playtester's controller config is awaited), and an Ocarina of Time texture overhaul pack crashed the game
  after a while (issue #1063: the pack's name is needed to reproduce it).

### Open decisions (operator), added since v0.2.0-prealpha

- #1106: the older-paired-file invalidation above ships in this release, as the trick set's did before it; whether a
  later build migrates such files is open.
- #1094: Shared Bottles shares the count, capped at Ocarina of Time's four, with per-game contents; OoTMM shares six
  bottles with their contents. Kept as a deliberate divergence, open to the operator.
- Issue #1085: a shared song places two copies under the default Shuffle Songs; the fix in review (PR #1111) makes it
  one copy.

### Run record

#1073 to #1100 were gated on the workstation (Windows, OpenGL, ROM archives mounted) by the pyramid driver: a pull
request's `[lock]` commits were built alone on `main` and their rows seen red, then the batch was built once and the
`redship`, `rando` and `ui` tiers and the parity lint run on it, then `integration` and `integration-soak`. #1081 to
#1100 were merged through the landing PRs #1103 and #1110, whose hosted CI rounds were green. #1102 and #1104 to #1108
were merged once their hosted CI was green, and the driver then ran the ROM tiers on `main`. On `main` itself,
`integration` passed 20 of 20 and `integration-soak` 1 of 1 at `db8d38e8` (cycle 17), then 21 of 21 and 1 of 1 at
`2a57d1c6` (cycle 18, with `IntMoonCrashCycle` of #1102) and at `30b841c1`, the code of this release (cycle 19), all
on 2026-10-22. Nobody has played the release; the full records are in [`known-issues.md`](known-issues.md).

---

# v0.2.0-prealpha ("Trinity Alfa"), as released

**Status:** released 2026-10-20 as the tag `v0.2.0-prealpha` at `1ace3c7b`. The text below is the draft as it was
published, kept for the links that point into it; it is not updated.
**Version:** `v0.2.0-prealpha`, codename "Trinity Alfa" (`src/common/rsbs_version.h`, PR
[#946](https://github.com/spencerduncan/redshipblueship/pull/946)). Under [`VERSIONING.md`](VERSIONING.md) that is a
`y` bump: major features and fixes without a new `oot.otr` (the archive version, `project(Ship VERSION 9.1.1)`, is
unchanged).
**Built from:** `main` at `7e44b8a0` (2026-10-17, PR [#960](https://github.com/spencerduncan/redshipblueship/pull/960);
which commit the tag takes, this one or a later one with the merges listed under "Since the last refresh", is the
operator's call):
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
**Since the last refresh (2026-10-18 to 2026-10-20):** 22 more pull requests merged, up to `main` at `7671196a`
(2026-10-20, PR [#1058](https://github.com/spencerduncan/redshipblueship/pull/1058)); no tag has been cut yet, so they
are in this draft. The player-facing ones are listed under their headings below with links. Test and tooling only:
PR [#986](https://github.com/spencerduncan/redshipblueship/pull/986) (the wave toolkit reads CTest 4.x summaries),
PR [#989](https://github.com/spencerduncan/redshipblueship/pull/989) (the tracker reachability audit report,
`docs/tracker-reachability-audit-2026-10-18.md`), PR [#1041](https://github.com/spencerduncan/redshipblueship/pull/1041)
(test processes play no sound), PR [#988](https://github.com/spencerduncan/redshipblueship/pull/988) (an integration row
for Majora's Mask's side of a refused crossing, [#954](https://github.com/spencerduncan/redshipblueship/issues/954)) and
PR [#1054](https://github.com/spencerduncan/redshipblueship/pull/1054) (the crossing pin locked in OoT's one-way entrance
pools, [#998](https://github.com/spencerduncan/redshipblueship/issues/998)). No generated world moved after #974: the
fingerprint at file select changed once (PR [#1004](https://github.com/spencerduncan/redshipblueship/pull/1004)), the
placements did not.
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
  (#960). Since PR [#1004](https://github.com/spencerduncan/redshipblueship/pull/1004) that fingerprint also names
  the generator's version, so a build that generates worlds differently shows a different fingerprint for the same
  seed and settings. Builds from before it show another fingerprint for the same world: compare fingerprints only
  between builds at or after `29e8dba5`. A paired file made by a newer generator is refused by an older build; open
  it in the build that made it or a newer one.
- **The first launch now also asks for your Majora's Mask ROM.** When no `mm.o2r` is found, the first-launch
  setup that generates `oot.o2r` now also offers to generate `mm.o2r` from your Majora's Mask ROM, with the same
  popups and progress dialog. If you already have `oot.o2r` but no `mm.o2r`, this is a new question on your next launch;
  "Not now" starts Ocarina of Time alone, as before, and the question returns at the next launch until
  `mm.o2r` exists (#945).
- **The operator played this build on 2026-10-17** (`7e44b8a0`, on DirectX 11): a new paired file, shops in both
  games selling crossed items, the Happy Mask Shop crossing both ways, play in Termina and the Combo Item Tracker
  overlay in both games. The first-launch extraction was checked on 2026-10-16 (#945). The findings are filed
  (issues #962 to #971), none is a crash, and the ones a player sees are under Known limitations. Not played yet:
  a crossing after quitting and relaunching the game or after a save made during play,
  "[Both Games] Shared Kokiri Sword" on, MM's game-over prompt, OoT's icon in MM's get-item textbox, and OpenGL
  compared with DirectX 11. (Corrected 2026-10-27: this list also named "the cross-game warp songs", which no code
  implements; issue #1169 scopes them.) An earlier build's two findings (2026-10-09) are fixed (#887, #889, #895).
  Of the
  2026-10-17 findings, the overlay's position and its Majora's Mask icons are fixed
  ([#962](https://github.com/spencerduncan/redshipblueship/issues/962),
  [#963](https://github.com/spencerduncan/redshipblueship/issues/963)), the boss remains and the Moon's Tear show
  their models in Hyrule ([#964](https://github.com/spencerduncan/redshipblueship/issues/964), partly), and F10 on
  DirectX 11 is fixed in code ([#955](https://github.com/spencerduncan/redshipblueship/issues/955)); nobody has
  played those fixes yet.

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
  shipped presets) let the logic count masks only the shop (or, under "Completed", the optional Mask Select
  enhancement) puts in your hands, so the two Deku Theater checks could hold an item the seed needed. As in Ship
  of Harkinian's "Shuffle", the Deku Theater's Mask of Truth check no longer takes an "Always" gossip-stone hint
  (#974).
- **Majora's Mask's start in a paired world is the Ocarina of Time and the Song of Time.** The Kokiri Sword and
  the Hero's Shield are in MM's item pool, as in OoTMM (#895). A new rule, "[Both Games] Shared Kokiri Sword"
  (off by default), makes the two games' Kokiri Swords one item (#910).
- **Majora's Mask's trap types are part of the world.** The set of trap types is fixed when the file is created,
  and which type a check springs comes from the world's seed and the check, so the same check always springs the
  same trap. A change made before the first trip to Termina is put back when the file loads, as MM options and
  tricks are. Files created before this change spring Freeze traps only, which is what the single executable's
  pages could set anyway (PR [#1049](https://github.com/spencerduncan/redshipblueship/pull/1049),
  [#1047](https://github.com/spencerduncan/redshipblueship/issues/1047)).
- **Groundwork for shared and cross-game masks.** The file now records the cross-game item rules, every one off,
  so nothing changes in play yet; a file made by a later build with one of them on is refused by name
  (`crossItemFlags`) (PR [#1002](https://github.com/spencerduncan/redshipblueship/pull/1002); the masks epic
  [#975](https://github.com/spencerduncan/redshipblueship/issues/975) and
  [#976](https://github.com/spencerduncan/redshipblueship/issues/976) stay open).
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
- **Majora's Mask's boss remains and the Moon's Tear show their real models** on Ocarina of Time's shop shelves
  and in its get-item cutscene, instead of the "?" stand-in. Stray fairies, the other items 2Ship2Harkinian draws
  with its own code (owl statues, boss and enemy souls, clocks, frogs, the swim ability, Triforce pieces, traps
  and ocarina buttons) and the bottled fairy still show "?" (PR [#1042](https://github.com/spencerduncan/redshipblueship/pull/1042);
  [#964](https://github.com/spencerduncan/redshipblueship/issues/964) stays open for the stray fairies).
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
- Majora's Mask's free gossip stones say the same hint on every platform: a stone's seed no longer depends on how
  the compiler converts a negative number (PR [#1049](https://github.com/spencerduncan/redshipblueship/pull/1049),
  [#1047](https://github.com/spencerduncan/redshipblueship/issues/1047)). Two things still vary from play to play by
  design: an MM junk check shows a cycling junk item and gives the one showing when you collect it, and a bought
  gossip-stone hint names a randomly chosen unobtained check each time.

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
- Four more goldens pin a world with excluded locations and tricks in both games, OoT shop shuffle with drawn
  prices, MM's Nearly No Logic and the combo direction FORWARD, and a new row checks the player's own Generate path
  (worker thread, full start-up, the real file-creation event) against the default golden (PR
  [#1001](https://github.com/spencerduncan/redshipblueship/pull/1001),
  [#982](https://github.com/spencerduncan/redshipblueship/issues/982)). The goldens also pin what each MM free
  gossip stone says (PR [#1049](https://github.com/spencerduncan/redshipblueship/pull/1049)).
- **The fingerprint at file select names the generator's version.** A build that changes how worlds are made
  shows a different fingerprint for the same seed and settings, so two players can tell whether their builds make
  the same world; the version is a label and moves no placement. Files created by older builds still load. CI now
  compares the digests the Linux and Windows builds produce, field by field (PR
  [#1004](https://github.com/spencerduncan/redshipblueship/pull/1004),
  [#1003](https://github.com/spencerduncan/redshipblueship/issues/1003)).

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
- **The Item Tracker overlay draws Majora's Mask's icons from the start**, before Majora's Mask has run in the
  session, from the curated archive built at launch (PR [#1043](https://github.com/spencerduncan/redshipblueship/pull/1043),
  [#963](https://github.com/spencerduncan/redshipblueship/issues/963)).
- **The Item Tracker overlay starts below the HUD's hearts and magic bar**, at the left edge, instead of on top of
  them. Combo > Tracker Settings has a Move to Default Position button, and the Combo > Windows toggle's tooltip says
  how to drag the overlay. A position saved in `imgui.ini` by an earlier build is kept until you press Move to
  Default Position (PR [#1051](https://github.com/spencerduncan/redshipblueship/pull/1051),
  [#962](https://github.com/spencerduncan/redshipblueship/issues/962)).
- **Check Tracker: Available Checks update when you return to Hyrule from Termina,** at every crossing, door or
  F10: items redeemed and hearts or wallet applied from Majora's Mask count at once (PR
  [#1052](https://github.com/spencerduncan/redshipblueship/pull/1052),
  [#990](https://github.com/spencerduncan/redshipblueship/issues/990)).
- Check Tracker: a shop price range set with the higher number first no longer hides shop, scrub and merchant
  checks you can afford (PR [#1053](https://github.com/spencerduncan/redshipblueship/pull/1053),
  [#999](https://github.com/spencerduncan/redshipblueship/issues/999); inherited from Ship of Harkinian).
- Majora's Mask's own Check Tracker no longer rewrites the randomizer's event flags in the live save while it
  works out what is in logic, so what MM saves no longer depends on whether that window is open (PR
  [#1056](https://github.com/spencerduncan/redshipblueship/pull/1056),
  [#995](https://github.com/spencerduncan/redshipblueship/issues/995); inherited from 2Ship2Harkinian).
- With OoT's "Shuffle Child's Wallet" on, a wallet found in Termina counts as OoT's wallet, so the Check
  Tracker no longer shows about 115 price-gated checks as available while rupees are pinned at 0, and the
  shared rupee pool is no longer drained on every visit to Hyrule (PR [#1058](https://github.com/spencerduncan/redshipblueship/pull/1058),
  [#991](https://github.com/spencerduncan/redshipblueship/issues/991), [#992](https://github.com/spencerduncan/redshipblueship/issues/992)). While Ocarina of Time has no wallet at all, pooled rupees
  show as 0 there and stay in the pool until a wallet is found.
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
- **F10 switches games on DirectX 11.** Windows delivers F10 as a system key, and the DirectX 11 window passed it
  to Windows, which opened its menu loop and stalled the game. The window now hands F10 to the game, as the OpenGL
  window always did; Alt+F4 still closes the game. The fix is in the libultraship fork (its PR #11), and it reached
  `main` with PR [#1045](https://github.com/spencerduncan/redshipblueship/pull/1045)'s pin
  ([#955](https://github.com/spencerduncan/redshipblueship/issues/955)). Not yet tried by hand.
- Switching games no longer piles up another open copy of every archive it re-mounts (file handles and file lists);
  a mod still overrides the base game after every switch (PR
  [#1045](https://github.com/spencerduncan/redshipblueship/pull/1045),
  [#987](https://github.com/spencerduncan/redshipblueship/issues/987)).
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
- **Three of Ship of Harkinian's settings now apply in Majora's Mask too:** Enhancements > Graphics > Increase
  Actor Draw Distance and Widescreen Actor Culling, and Enhancements > Fixes > Fix Out of Bounds Textures (which
  reaches Majora's Mask when it next starts: set it before your first crossing, or relaunch). Eight more rows now
  write the key Majora's Mask reads, but MM's code for them is not linked into the executable yet, so they still do
  nothing there (see Known limitations). Damage Multiplier stays Ocarina of Time only until its two value ranges
  are reconciled (PR [#1046](https://github.com/spencerduncan/redshipblueship/pull/1046);
  [#1027](https://github.com/spencerduncan/redshipblueship/issues/1027),
  [#1028](https://github.com/spencerduncan/redshipblueship/issues/1028),
  [#1029](https://github.com/spencerduncan/redshipblueship/issues/1029); audit
  [#966](https://github.com/spencerduncan/redshipblueship/issues/966)).
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
- **Each game is served its own copy of the resource paths both archives carry.** Before, the game that loaded a
  shared path first supplied it to the other for the rest of the session: after Ocarina of Time had loaded an
  Octorok, Majora's Mask's Clock Town shooting gallery and the Southern Swamp route crashed, and MM's staff roll read
  OoT's credits table; MM's chests, bottles and shop models could show OoT's copies (PR
  [#1000](https://github.com/spencerduncan/redshipblueship/pull/1000),
  [#981](https://github.com/spencerduncan/redshipblueship/issues/981)).
- Ocarina of Time's item-age and item-action tables now cover every item id, so Ship of Harkinian's Roc's Feather
  no longer writes or reads past their ends (PRs [#985](https://github.com/spencerduncan/redshipblueship/pull/985)
  and [#1048](https://github.com/spencerduncan/redshipblueship/pull/1048);
  [#983](https://github.com/spencerduncan/redshipblueship/issues/983),
  [#984](https://github.com/spencerduncan/redshipblueship/issues/984)).

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
- At most 64 crossings each way, and netplay grants share those slots.
- In narrow windows Settings > General's Boot Sequence dropdown covers its label; Ship of Harkinian draws it
  the same way (#923).
- The config file is still `shipofharkinian.json`.
- Majora's Mask's stray fairies, the other items 2Ship2Harkinian draws with its own code (owl statues, boss and
  enemy souls, clocks, frogs, the swim ability, Triforce pieces, traps and ocarina buttons) and the bottled fairy,
  which no item reaches, still show as the "?" stand-in on Ocarina of Time's shop shelves and in its get-item
  cutscene; buying or getting one works. The boss remains and the Moon's Tear show their models since PR [#1042](https://github.com/spencerduncan/redshipblueship/pull/1042)
  (issue [#964](https://github.com/spencerduncan/redshipblueship/issues/964)).
- Frame interpolation (Settings > Graphics) smooths Ocarina of Time but not Majora's Mask, which stays at 20
  frames a second (issue #965).
- Some settings on the shared pages do nothing in Majora's Mask, because MM's code for them is not linked into the
  executable: among them the cheats, Fast Chests, Enemy Health Bars, the cutscene skips, Free Look, three audio
  options ([#1005](https://github.com/spencerduncan/redshipblueship/issues/1005) to
  [#1026](https://github.com/spencerduncan/redshipblueship/issues/1026)), and eight rows whose key Majora's Mask now
  reads, such as Permanent Heart Loss and 3D Dropped Items
  ([#1030](https://github.com/spencerduncan/redshipblueship/issues/1030) to
  [#1036](https://github.com/spencerduncan/redshipblueship/issues/1036),
  [#1038](https://github.com/spencerduncan/redshipblueship/issues/1038)). Damage Multiplier is Ocarina of Time only
  ([#1037](https://github.com/spencerduncan/redshipblueship/issues/1037)). The audit is
  [#966](https://github.com/spencerduncan/redshipblueship/issues/966).
- A spoiler left in `Randomizer/` by a build before `29e8dba5` for the same seed belongs to another fingerprint, so a
  new paired world of that seed gets no spoiler (stderr says `[MM] spoiler: REFUSED ...`; the world plays normally).
  Move or delete the older spoiler with the same seed name before you create the file
  ([#1055](https://github.com/spencerduncan/redshipblueship/issues/1055)).
- The Item Tracker overlay's Shared column updates at a save or a crossing, not live (issue #971).
- Not tested by anyone: networking, OpenGL compared with DirectX 11, and a crossing after
  quitting and relaunching the game or after a save made during play. (Corrected 2026-10-27: this line also named
  "cross-game warp songs", which no code implements; issue #1169 scopes them.)

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
- [#1037](https://github.com/spencerduncan/redshipblueship/issues/1037): which value range wins when Damage Multiplier
  becomes one key for both games (OoT's 0-8 with 8 the one-hit KO, MM's 0-4 plus 10, or a union list).
- [#993](https://github.com/spencerduncan/redshipblueship/issues/993): whether the Combo Tracker shows which checks
  you can reach, and whether Majora's Mask's own Check Tracker gets a menu row back (from the tracker reachability
  audit, PR [#989](https://github.com/spencerduncan/redshipblueship/pull/989)).

## Run record

The closing smoke at `851b1ee9` (2026-10-12; Windows, OpenGL, ROM archives mounted) passed `redship` 179 of
179, `rando` 43 of 43 (all four golden-world rows), `ui` 2 of 2 with the parity lint, `integration` 14 of 14
and `integration-soak` 1 of 1. The gates of the three determinism pull requests (#958, #959, #960; 2026-10-17,
same setup) ended at `7e44b8a0`, the code of this release, with `redship` 184 of 184, `rando` 49 of 49 (the golden-world
rows among them, over five golden files now), `ui` 2 of 2 with the parity lint, `integration` 17 of 17 and `integration-soak` 1 of
1. There, 10 of 10 sample paired creations succeeded on the first attempt (258 fill rounds each against the
1250-round cap; 6.2-8.6 s, mean 7.1 s). The operator then played the build on DirectX 11 (see Read this first); OpenGL and DirectX 11 were not compared.
The merges of 2026-10-18 to 2026-10-20 were gated the same way, singly (#1000, #1041, #1001, #1004, #1058) and in three
merged batches, each merged tree checked equal to its gated tree; the last gate, PR #1058 at `7671196a`, passed `redship`
201 of 201, `rando` 55 of 55, `ui` 2 of 2, `integration` 19 of 19 and `integration-soak` 1 of 1. Nobody has played
them yet. The full records are in [`known-issues.md`](known-issues.md).
