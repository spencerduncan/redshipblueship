# Known issues

**Applies to:** `v0.3.1-prealpha` ("Ripley Bravo"), whose code is `main` at `c1023282` (2026-10-23): `v0.3.0-prealpha` plus the 21 commits merged after its tag (#1111 to #1155, listed under "Release notes: v0.3.1-prealpha" in [`release-notes-next.md`](release-notes-next.md)), among them the Combo Tracker following SoH's Check Tracker window settings (PR [#1154](https://github.com/spencerduncan/redshipblueship/pull/1154)), among them Shuffle Tree Drops (PR [#1125](https://github.com/spencerduncan/redshipblueship/pull/1125)), whose growth of Majora's Mask's save means a paired file from `v0.3.0-prealpha`, or from any `main` build after the tag and before `fabe7d33`, does not carry over ("The `v0.3.0-prealpha` save break" under "Save loss and corruption"), and the Combo Tracker's reachability (PR [#1147](https://github.com/spencerduncan/redshipblueship/pull/1147)), whose growth of the combo record means a file saved from `efe573ec` on does not open in `v0.3.0-prealpha` and a seed package exported by a `main` build before it is refused ("Seed packages from `main` builds before `efe573ec` are refused" under "Randomizer generation"; no released build wrote a seed package). Before that, `v0.3.0-prealpha` ("Ripley Alfa"), whose code is `main` at `30b841c1` (2026-10-22): the 33 commits after `v0.2.0-prealpha`, pull requests #1073 to #1108, all listed in [`release-notes-next.md`](release-notes-next.md), among them Shared Song of Time and Moon Crash Behavior (PR [#1102](https://github.com/spencerduncan/redshipblueship/pull/1102)) and the one randomizer menu of [#1101](https://github.com/spencerduncan/redshipblueship/issues/1101) (PRs [#1104](https://github.com/spencerduncan/redshipblueship/pull/1104) to [#1107](https://github.com/spencerduncan/redshipblueship/pull/1107)), which moved Majora's Mask's randomizer pages and the Cross-Game rules under the Randomizer header (Randomizer → MM Randomizer, MM Locations, MM Tricks and Cross-Game). Older reports and records below keep the page names of their day (Combo → Cross-Game Rules, Combo → MM Randomizer, Combo → MM Tricks). Before that, `v0.2.0-prealpha` ("Trinity Alfa", tagged 2026-10-20 at `1ace3c7b`): `main` at `851b1ee9` (2026-10-12, PR [#940](https://github.com/spencerduncan/redshipblueship/pull/940): the single-bag switch of PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)
plus everything merged after it, the last 92 PRs being the 2026-09-30 wave) plus PRs [#945](https://github.com/spencerduncan/redshipblueship/pull/945) (the first launch also offers Majora's Mask's ROM extraction), [#956](https://github.com/spencerduncan/redshipblueship/pull/956) (the night Happy Mask Shop door) and [#957](https://github.com/spencerduncan/redshipblueship/pull/957) (a crossing refused for a missing archive no longer crashes), with the version bump of PR [#946](https://github.com/spencerduncan/redshipblueship/pull/946) (`ece56551`), then [#958](https://github.com/spencerduncan/redshipblueship/pull/958) (the goldens pin hint text, prices, starting items and entrances; static hints in a fixed order), [#959](https://github.com/spencerduncan/redshipblueship/pull/959) (the creation budget counts fill rounds) and [#960](https://github.com/spencerduncan/redshipblueship/pull/960) (the fingerprint and build beside the seed icons), up to `7e44b8a0` (2026-10-17), then PR [#974](https://github.com/spencerduncan/redshipblueship/pull/974) (2026-10-18: a paired world shuffles the Happy Mask Shop's masks), then the 20 pull requests merged after it from 2026-10-18 to 2026-10-20 up to `ee35d2c6` (PR [#986](https://github.com/spencerduncan/redshipblueship/pull/986) to PR [#1056](https://github.com/spencerduncan/redshipblueship/pull/1056), listed in [`release-notes-next.md`](release-notes-next.md)), and the GitHub Actions builds cut from them; the `v0.1.1-prealpha` tag
(2026-07-03) is older than everything in the first section. What changed since that tag, in player terms:
[`release-notes-next.md`](release-notes-next.md).
**Last updated:** 2026-10-23 (the `v0.3.1-prealpha` cut at `c1023282`: the Combo Tracker's window settings of PR [#1154](https://github.com/spencerduncan/redshipblueship/pull/1154) and the shared window settings it brings under Ocarina of Time); 2026-10-23 (the merges up to `af4d941f`: the gate record corrected, since the integration red of cycle 22 was the operator's game controller reaching the test windows, and the gates at `e3de3ff1` and `af4d941f`; the Combo Tracker's reachability with its save-format and seed-package compatibility breaks; Majora's Mask's tracker rows in the Combo Item Tracker; test processes that ignore game controllers; the open issues refreshed there); 2026-10-23 (the merges after the `v0.3.0-prealpha` tag up to `4fc4fd41`: the `v0.3.0-prealpha` save break, accepted by the operator; the open issues refreshed there; capacity upgrades found in the other game, seed packages, one copy of a shared song, Skip Boss Introductions and Damage Multiplier in Majora's Mask, and the Combo Tracker's settings, list and removed Crossings section); 2026-10-22 (the `v0.3.0-prealpha` cut at `30b841c1`: the pending lines resolved now that PRs #1102 and #1104 to #1107 are on `main`; the open issues refreshed there; the gates on `main` of 2026-10-22); 2026-10-22 (the merges up to `db8d38e8`: the open issues refreshed there; the Shared Items rules and their limitations; the shared settings that are still Ocarina of Time only; [#1055](https://github.com/spencerduncan/redshipblueship/issues/1055) resolved; pending, and marked so: the one randomizer menu of [#1101](https://github.com/spencerduncan/redshipblueship/issues/1101), Majora's Mask's trap types set from the menu, Shared Song of Time, and older paired files with Majora's Mask's two new save fields); 2026-10-20 (the open issues refreshed at `7671196a`; the gates record of 2026-10-20; the cross-game resource cache crash class of [#981](https://github.com/spencerduncan/redshipblueship/issues/981), F10 on DirectX 11, the stale-spoiler refusal of [#1055](https://github.com/spencerduncan/redshipblueship/issues/1055) and Majora's Mask's frozen trap types); 2026-10-18 (the Happy Mask Shop's masks are always in the item pool, PR [#974](https://github.com/spencerduncan/redshipblueship/pull/974), [#973](https://github.com/spencerduncan/redshipblueship/issues/973)); 2026-10-17 (the operator's playtest of `7e44b8a0` and its findings, [#962](https://github.com/spencerduncan/redshipblueship/issues/962) to [#971](https://github.com/spencerduncan/redshipblueship/issues/971)); 2026-10-17 (the creation budget of PR [#959](https://github.com/spencerduncan/redshipblueship/pull/959), the open issues refreshed at `7e44b8a0`, the gates record at `7e44b8a0`); 2026-10-16 (the #645, #599 and #497 lines of the open issues at `851b1ee9` brought up to date; the night Happy Mask Shop door of PR [#956](https://github.com/spencerduncan/redshipblueship/pull/956)); 2026-10-12 (the open issues at `851b1ee9`, the run record, and the sentences the wave made false). **Playtesting this build?** Start with
[`playtest-2026-09.md`](playtest-2026-09.md).

RedShipBlueShip is **pre-alpha**. It boots Ocarina of Time and Majora's Mask from
one executable, round-trips between them through the Happy Mask Shop ↔ Clock Tower
portal, and generates a paired randomizer world in which items cross both ways.
This document is the honest list of what is broken or missing, grouped by what it
does to you as a player rather than by issue number. Every entry links its tracking
issue; entries that have been fixed since the last revision are struck through and
name the pull request, so old reports can be matched to the fix.

If you hit something not on this list, please file it. If you hit something on
this list, the issue link is the place to add detail.

---

## Read this before you play

**A paired file saved by any build before `fabe7d33` (`v0.3.0-prealpha` included) is refused by builds from `fabe7d33` on: start a new file** (see "The `v0.3.0-prealpha` save break" under "Save loss and corruption").

**A seed package exported by a `main` build from `bc93e65e` to `e3de3ff1` is refused by builds from `efe573ec` on: generate the seed again** (`v0.3.0-prealpha` wrote no seed packages; see "Seed packages from `main` builds before `efe573ec` are refused" under "Randomizer generation"). **A file saved by a build from `efe573ec` on does not open in `v0.3.0-prealpha`** (see "A file saved from `efe573ec` on does not open in `v0.3.0-prealpha`" under "Save loss and corruption").

### Cross-game randomization: what ships now, and what does not

The headline feature is real, and since 2026-09-27 it has cross-game logic. Phase 3.1
(tracker [#492](https://github.com/spencerduncan/redshipblueship/issues/492)) shipped; Phase 3.2 (cross-game *logic*, [#500](https://github.com/spencerduncan/redshipblueship/issues/500)) has
**increment 2** (generation once, at file creation; PR [#680](https://github.com/spencerduncan/redshipblueship/pull/680)) and now
**increment 3** in production: **the single-bag fill** (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743), epic
[#645](https://github.com/spencerduncan/redshipblueship/issues/645)). One fill at the creation event places one bag of items across both
games' checks and proves the frozen goal over both games' logic at once. An item that
crosses leaves its home pool. The pieces it composes landed in waves 4-6: the coordinator
and both solver exports (PRs [#701](https://github.com/spencerduncan/redshipblueship/pull/701), [#714](https://github.com/spencerduncan/redshipblueship/pull/714), [#715](https://github.com/spencerduncan/redshipblueship/pull/715), [#717](https://github.com/spencerduncan/redshipblueship/pull/717)),
multiplicity and the surplus / filler / trap rules ([#728](https://github.com/spencerduncan/redshipblueship/pull/728)), the item
classification table ([#725](https://github.com/spencerduncan/redshipblueship/pull/725)), the monotonicity check ([#734](https://github.com/spencerduncan/redshipblueship/pull/734)), durable
crossings and the one spoiler ([#736](https://github.com/spencerduncan/redshipblueship/pull/736)), bag composition ([#738](https://github.com/spencerduncan/redshipblueship/pull/738)), one shared
triforce count ([#740](https://github.com/spencerduncan/redshipblueship/pull/740)) and the shared-quantity trim ([#744](https://github.com/spencerduncan/redshipblueship/pull/744)).

**What ships at `851b1ee9`:**

- **One seed, one paired world, one bag, items crossing in both directions.** Generating an
  OoT randomizer seed also generates the paired Majora's Mask world. OoT items are
  placed on MM checks and MM items on OoT checks, and **a crossed item is no longer also
  in its home pool** (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)). Crossings land on chests (OoT: non-shop
  `EN_BOX` chests; MM: its Tier-A chests) and on shop slots (see the shops entry below,
  [#800](https://github.com/spencerduncan/redshipblueship/issues/800)), at most 64 per
  side. Which items may cross is decided by a rule-defined item class (PR
  [#631](https://github.com/spencerduncan/redshipblueship/pull/631)) and the frozen direction.
- **Hearts and capacity upgrades are counted once for the pair.** Health is one shared
  bar. On the shipped profile the two pools hold 76 Pieces of Heart, 15 Heart Containers
  and 2 Double Defense; the bag keeps 44, 6 and 1 (pieces and containers OoT 14 + 4, MM 30 + 2;
  the Double Defense from either game), and the bar stops at 20 hearts (PR [#744](https://github.com/spencerduncan/redshipblueship/pull/744)). That is 42 trimmed heart and
  double-defense rows. Capacity families whose top tier is the same in both games (magic,
  quiver, bomb bag, and the wallet when OoT has no tycoon's wallet) are trimmed to the
  shared maximum the same way; with them, 51 rows (OoT 22, MM 29) become filler in their
  home game. Families whose top tiers differ keep every copy and cost a known dead pickup:
  the hookshot (OoT 2, MM 1) always. ~~And the ocarina when the frozen rules arm it.~~ The
  ocarina is never armed since [#879](https://github.com/spencerduncan/redshipblueship/issues/879)
  (PR [#889](https://github.com/spencerduncan/redshipblueship/pull/889) retired the effect), so each game keeps its
  own and no ocarina pickup is dead. Stick
  and nut capacity exist only in OoT and are unchanged.
- **Both worlds are generated together, once, at file creation.** Freeze, the single-bag
  fill, each game's own pass over its leftover checks (traps and junk stay in their home
  game), one spoiler with a `combo` section, one atomic identity publish, and the MM shadow
  armed last (PRs [#680](https://github.com/spencerduncan/redshipblueship/pull/680), [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
  Arrival in MM no longer generates anything: it hydrates the frozen shadow or
  refuses. A generation failure fails file creation itself, at file select, with
  no partial identity and no silent vanilla Termina fallback. A paired world's OoT
  spoiler dropped on the file select on its own is refused with a toast; one the file
  select finds by itself at launch is left alone, with no toast (PR [#939](https://github.com/spencerduncan/redshipblueship/pull/939)).
  One generation makes one file: once a file is created from it, "Start Randomizer" greys
  out and reads "This seed was used for file N." (PR [#934](https://github.com/spencerduncan/redshipblueship/pull/934)).
- **Foreign items have their own identity.** An OoT item in Termina shows its real
  name, its OoT icon and its model in MM's get-item cutscene; an MM item in Hyrule shows
  its MM model in OoT's get-item cutscene. The text names the item's game, "(OoT)" in
  Termina and "(MM)" in Hyrule (PRs [#524](https://github.com/spencerduncan/redshipblueship/pull/524),
  [#551](https://github.com/spencerduncan/redshipblueship/pull/551), [#761](https://github.com/spencerduncan/redshipblueship/pull/761),
  [#822](https://github.com/spencerduncan/redshipblueship/pull/822), [#845](https://github.com/spencerduncan/redshipblueship/pull/845),
  [#884](https://github.com/spencerduncan/redshipblueship/pull/884)).
- **The combo is one game, frozen at file creation.** MM's randomizer profile and
  the combo settings (direction, pool sizes, item classes) are frozen into the
  world's identity when the OoT file is created (ADR 0009, ADR 0011; PRs
  [#570](https://github.com/spencerduncan/redshipblueship/pull/570),
  [#628](https://github.com/spencerduncan/redshipblueship/pull/628)). Changing MM
  options or combo settings afterwards does not change the world: loading the paired
  file puts its own values back into the pages, a toast names what it reset, and the
  next crossing agrees with the file (PR
  [#787](https://github.com/spencerduncan/redshipblueship/pull/787); see the resolved
  [#781](https://github.com/spencerduncan/redshipblueship/issues/781) entry under "Save
  loss and corruption" for the cases the file cannot answer). A refused load or
  crossing marks the save slot refused rather than silently overwriting it (PR
  [#568](https://github.com/spencerduncan/redshipblueship/pull/568)). Set MM's
  options *before* creating the file: `Randomizer → MM Randomizer`, `Randomizer → MM
  Locations` and `Randomizer → MM Tricks` (under the Randomizer header since PR
  [#1104](https://github.com/spencerduncan/redshipblueship/pull/1104); Combo pages from
  2026-09-27; before that a pop-out window opened from `Combo → Windows`, and before PR
  [#745](https://github.com/spencerduncan/redshipblueship/pull/745) from
  Randomizer → Cross-Game).
- **The pair is generated with logic set to Glitchless** by default, and the fill proves
  the frozen goal (`beat-both`, Ganon & Majora, by default) over both games' logic.
  It runs behind a deterministic attempt ladder with a budget of 1250 fill rounds per
  attempt and 2500 per creation (about 30 s on the development workstation). The budget
  counts work, not time, so a seed is creatable on every machine or on none (PRs
  [#680](https://github.com/spencerduncan/redshipblueship/pull/680), [#743](https://github.com/spencerduncan/redshipblueship/pull/743), [#959](https://github.com/spencerduncan/redshipblueship/pull/959)). On the development
  workstation 30 real creations took 5.9-13.7 s, each on its first attempt. **A progress
  overlay paints during file creation** ("Placing item n of N in both worlds"; PRs
  [#707](https://github.com/spencerduncan/redshipblueship/pull/707), [#749](https://github.com/spencerduncan/redshipblueship/pull/749), [#582](https://github.com/spencerduncan/redshipblueship/issues/582)): the window stays responsive and menu
  input is suppressed while it paints.
- **The Happy Mask Shop door is never shuffled.** It is the OoT ↔ MM crossing, so
  OoT's own entrance randomizer now leaves both directions of it out of every
  shuffle pool; under interior (or any) entrance shuffle the door you walk
  through is still the door to Termina (PR [#691](https://github.com/spencerduncan/redshipblueship/pull/691), [#661](https://github.com/spencerduncan/redshipblueship/issues/661)).
  Since PR [#956](https://github.com/spencerduncan/redshipblueship/pull/956) ([#948](https://github.com/spencerduncan/redshipblueship/issues/948))
  the door also opens at night while a paired file is open, so child Link can cross at any hour. On a vanilla
  file it still shows its "Apply during the day" sign at night and does not open, as in Ocarina of Time, and
  the other night shops keep their signs. The crossing is child-only: as adult the Market is in ruins and has
  no Happy Mask Shop door.
- **The Happy Mask Shop's masks are always in the item pool** (PR [#974](https://github.com/spencerduncan/redshipblueship/pull/974), [#973](https://github.com/spencerduncan/redshipblueship/issues/973)).
  The shop's door is the crossing, so the shop's room never loads. A paired world is generated with Ocarina of
  Time's "Mask Quest" as "Shuffle", as in OoTMM: all eight masks are in the item pool, and the pause menu's mask
  slot cycles through the masks you have. "Mask Shop Hint" is generated off, because its sign is inside the
  shop. Both rows on the Randomizer settings page are greyed, show the value the world uses, and say why on
  hover. Before this change "Vanilla" (the option's default) and "Completed" (the shipped presets) let the logic
  count masks that only the shop, or under "Completed" the optional Mask Select enhancement the seed cannot see,
  puts in your hands, so the two Deku Theater checks, and Spirit Temple MQ's Skull Mask route, could hold an item
  the seed needed. Following Ship of Harkinian's own "Shuffle", the Deku Theater's Mask of Truth check no longer
  takes one of the "Always" gossip-stone hints, which SoH gives it only under "Vanilla". A file keeps the
  settings it was created with. On a file created earlier with "Completed", the masks can be used only through
  Enhancements > Items > Masks > "Mask Select in Inventory", once Zelda's Letter has been shown to the Kakariko
  gate guard (what the code requires; not played). A file created earlier with "Vanilla" cannot get them.
- **Paired MM worlds can enable individual Majora's Mask logic tricks.** MM now
  has a per-trick vocabulary (86 trick keys, frozen into the world's identity
  like OoT's). **37** of them are bound to a real logic edge, 20 are reserved for an
  Ocarina of Time item and 29 are recorded as not bindable (PRs
  [#686](https://github.com/spencerduncan/redshipblueship/pull/686), [#696](https://github.com/spencerduncan/redshipblueship/pull/696), [#703](https://github.com/spencerduncan/redshipblueship/pull/703), [#713](https://github.com/spencerduncan/redshipblueship/pull/713), [#729](https://github.com/spencerduncan/redshipblueship/pull/729), [#763](https://github.com/spencerduncan/redshipblueship/pull/763); [#578](https://github.com/spencerduncan/redshipblueship/issues/578)). Every trick is off by default.
  **One of them is Deku-Stick combat** (PR [#729](https://github.com/spencerduncan/redshipblueship/pull/729), [#719](https://github.com/spencerduncan/redshipblueship/issues/719)). MM's logic used to assume you
  could fight 17 kinds of enemy with a Deku Stick, even though the matching trick is off by default.
  Those enemies now need `Deku Stick Fighting` enabled, or another weapon. Using the stick as a fire
  source is unchanged. None of the pinned test worlds moved. A
  trick whose edge is not bound draws disabled-with-reason on Randomizer → MM
  Tricks rather than enabled-and-inert ([#697](https://github.com/spencerduncan/redshipblueship/issues/697) closed with PR [#763](https://github.com/spencerduncan/redshipblueship/pull/763)). PR #763 also
  follows OoTMM on two tightenings, so with the tricks off logic kills Twinmold only with
  the Giant's Mask, magic and a sword ("Twinmold with Bow (MM)" admits the Bow), and the
  West Clock Town bank's heart piece needs the Giant's Wallet ("Bank Rewards Require One
  Less Wallet" admits the Adult Wallet).
- ~~**You may opt into one shared Ocarina across both games**~~ RETIRED by
  [#879](https://github.com/spencerduncan/redshipblueship/issues/879) (PR [#889](https://github.com/spencerduncan/redshipblueship/pull/889)): the row is gone, and a world created with it on loads with it ignored. Was: (Combo → Cross-Game
  Rules → "[Both Games] Shared Ocarina") — off by default, frozen at file creation
  like every other combo rule: obtaining an ocarina in
  either game grants it in the other (PR
  [#675](https://github.com/spencerduncan/redshipblueship/pull/675)).
- **Majora's Mask starts a paired world with the Ocarina of Time and the Song of Time
  only.** The Kokiri Sword and the Hero's Shield are in MM's item pool, as in OoTMM (PR
  [#895](https://github.com/spencerduncan/redshipblueship/pull/895)). With Shared Song of Time on (PR
  [#1102](https://github.com/spencerduncan/redshipblueship/pull/1102)), MM starts with the Ocarina alone.
- **Shared Items: one item for both games, rule by rule.** Randomizer → Cross-Game →
  "Shared Items" holds OoTMM's "Shared <Item>" rules in OoTMM's order, each a "[Both Games]"
  checkbox, off by default and frozen at file creation: Shared Fire, Ice and Light Arrows,
  Epona's Song, Song of Storms, Song of Time, Lens of Truth, Goron Mask, Zora Mask, Bunny Hood,
  Keaton Mask, Mask of Truth, Kokiri Sword, Shields, Ocarina Buttons and Bottles (PR [#910](https://github.com/spencerduncan/redshipblueship/pull/910) for the sword;
  PRs #1080 to #1095 and #1102 for the rest, and #1104 and #1105 for the page and section, listed in [`release-notes-next.md`](release-notes-next.md)). With one on, one copy is
  placed for both games and finding it in either game gives it to you in the other at your next crossing. What they
  do not do yet is under "Randomizer generation" below.
- **You choose the goal, and meeting it ends the paired game.** Randomizer → Cross-Game
  → "[Both Games] Goal" offers OoTMM's goals: Ganon & Majora (the default), Any
  Final Boss, Ganon and Majora. A goal that leaves a half unproved says so with a toast
  at creation ("Not proven: Majora's Mask may be unfinishable."; PR [#760](https://github.com/spencerduncan/redshipblueship/pull/760)). The
  ending plays only in the game whose final-boss defeat meets the goal; a final boss
  beaten earlier is recorded and puts you back in its game, in Ganon's Tower or at a new
  cycle in South Clock Town (PR [#769](https://github.com/spencerduncan/redshipblueship/pull/769)). A half's own triforce hunt no longer ends a
  paired world under a boss goal, and OoT's own "Win" mode is generated as "Ganon's
  Boss Key" there (PR [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).
- **Every randomizer file is a paired file.** Every OoT randomizer generation is a
  paired creation, so a single-game OoT randomizer file cannot be created (PR [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).

**What does not ship yet:**

- **The operator played `7e44b8a0` on 2026-10-17** (the release candidate, DirectX 11): a new
  paired file, shops in both games selling crossed items, the Happy Mask Shop crossing both
  ways, play in Termina and the Combo Item Tracker overlay in both games; the first-launch
  extraction flow was checked on 2026-10-16 on PR [#945](https://github.com/spencerduncan/redshipblueship/pull/945)'s build. The findings are listed under
  "Playtest, 2026-10-17, `7e44b8a0`" below; none is a crash. Still not played by anyone: a
  crossing after quitting and relaunching the game, a crossing after a save made during play,
  the cross-game warp songs, "[Both Games] Shared Kokiri Sword" on, MM's game-over prompt
  ([#694](https://github.com/spencerduncan/redshipblueship/issues/694)), OoT's icon in MM's get-item textbox ([#607](https://github.com/spencerduncan/redshipblueship/issues/607)), and OpenGL compared with DirectX 11.
  Everything else above comes from tests, measurements and UI captures. The operator played
  an earlier build on 2026-10-09 (which build was
  not recorded); its two findings are fixed: a crossing toasted an item a second time
  ([#878](https://github.com/spencerduncan/redshipblueship/issues/878), PR [#887](https://github.com/spencerduncan/redshipblueship/pull/887)), and
  entering Termina gave the Kokiri Sword and the Ocarina in both games
  ([#879](https://github.com/spencerduncan/redshipblueship/issues/879), PRs [#889](https://github.com/spencerduncan/redshipblueship/pull/889), [#895](https://github.com/spencerduncan/redshipblueship/pull/895)).
  The playtest is [`playtest-2026-09.md`](playtest-2026-09.md).
- ~~**The in-game crossing views lag Majora's Mask.**~~ **Resolved** ([#799](https://github.com/spencerduncan/redshipblueship/issues/799)).
  Combo > Windows > Toggle Cross-Game Spoiler lists every crossing (the Combo Tracker's Crossings section was
  cut in PR [#1137](https://github.com/spencerduncan/redshipblueship/pull/1137))
  in both directions, by name, with whether each host check was collected. That state
  comes from each game's own save. Before #799, Majora's Mask's was read only from the
  copy written at the last game switch or save, and arriving in Termina empties that
  copy, so while Majora's Mask was played the tracker's Majora's Mask panel read "No data
  yet" and every crossing hosted there read "?" until Majora's Mask's first save or the
  next switch. Now, while Majora's Mask is played, both windows read its running save,
  and a crossing collected there shows as collected at once. From
  Hyrule, Majora's Mask's is as of the last game switch or save (as of file creation
  until Majora's Mask is first entered), which the switch itself writes (#755, #757).
- **Paired-world hints are partial.** OoT's hints have no pair-level Way of the Hero.
  Foolish hints work again: each game calls one of its own areas foolish when none of
  its checks holds a crossing, an item the single bag placed or one the game counts as
  progression (OoTMM's rule without its probabilistic refinement; OoT's own Foolish
  entry, and a "plundering ... is a foolish choice" variant on Majora's Mask's free
  gossip stones, [#575](https://github.com/spencerduncan/redshipblueship/issues/575) H5a).
  OoT's gossip stones may hint an OoT check that holds a Majora's Mask
  item, and name that item with " (MM)", never the Blue Rupee the check holds
  ([#575](https://github.com/spencerduncan/redshipblueship/issues/575) H3b). An OoT item that crossed is
  hinted by the Majora's Mask area that holds it, for example "the Southern Swamp"
  (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743), [#575](https://github.com/spencerduncan/redshipblueship/issues/575) H3a);
  ~~a file created before that still says "Termina".~~ Such a file is refused since
  PR [#895](https://github.com/spencerduncan/redshipblueship/pull/895) (see "Back up your saves").
  Majora's Mask's own hints (gossip stones, and the Oath, remains, Hookshot and gold dust
  hints) name a check's area the way OoTMM does ("in the Southern Swamp", "around Goron
  Village"), not its scene, so a grotto no longer reads "Lone Peak Shrine & Grottos" and
  an enemy drop is never hinted. ~~An MM item that crossed into Hyrule is still hinted as
  "nowhere".~~ **RESOLVED (#575 H3c):** an MM item that crossed onto an Ocarina of Time
  check is named by that check's Hyrule area ("is hidden on Death Mountain Trail"), by
  the static hints and by the gossip stones, which now draw such items after Termina's
  own checks ([#575](https://github.com/spencerduncan/redshipblueship/issues/575) H2, H3c).
- **Triforce Hunt is not yet a paired goal.** The Goal row lists it, but generation
  refuses it, and OoTMM's Triforce Quest is not offered (PRs [#740](https://github.com/spencerduncan/redshipblueship/pull/740), [#760](https://github.com/spencerduncan/redshipblueship/pull/760), [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).
- ~~**Some settings re-seed the world without changing a rule.**~~ RESOLVED: the pool-size
  sliders ([#801](https://github.com/spencerduncan/redshipblueship/issues/801)) and the
  "OoT Classes" / "MM Classes" rows ([#834](https://github.com/spencerduncan/redshipblueship/issues/834))
  are retired, so every Cross-Game Rule left changes a rule (PRs [#817](https://github.com/spencerduncan/redshipblueship/pull/817), [#839](https://github.com/spencerduncan/redshipblueship/pull/839)).
- **Surplus filler can be dropped** when more MM items land in Hyrule than OoT items
  leave; only filler gives way (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Netplay and crossings share one 64-slot shared-item array.** Crossings alone cannot
  fill it, but a netplay peer's grants take slots too, so a pickup can still be refused
  (loudly) while both happen (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Generation can still abort.** A budget stop (1250 fill rounds per
  attempt) fails Generate with a toast ("Not generated: try a new seed or other
  options."); before PR [#1116](https://github.com/spencerduncan/redshipblueship/pull/1116) generation ran at file creation, and the same stop failed the creation
  ("Not created: ..."). The stop depends only on the seed and settings, never on the machine (PR
  [#959](https://github.com/spencerduncan/redshipblueship/pull/959)). No creation in
  the 30-seed sample needed more than two fill batches, which bounds the failure rate
  below about 10%, not at zero. No partial or corrupt file is left behind.
- **One MM randomizer option is drawn disabled on purpose.** "Majora Access: Remains"
  on Randomizer → MM Randomizer is retired ("Option is Retired"; ADR 0010 answer O1). No MM
  game-hook type is registered but undispatched in the single-executable build since PR
  [#673](https://github.com/spencerduncan/redshipblueship/pull/673) closed [#438](https://github.com/spencerduncan/redshipblueship/issues/438), so no
  other row is disabled for a missing hook; an option that is enabled and does nothing
  is a bug worth reporting.
- **MM's enhancement toggles live on Combo → Majora's Mask** (named MM Enhancements until 2026-09-27). The curated MM
  enhancement toggles — the game-over prompt, `BetterSongOfDoubleTime`,
  `SkipSoTCutscenes`, a pointer to the shared `Autosave` checkbox on OoT's
  Enhancements page, and (since PR [#730](https://github.com/spencerduncan/redshipblueship/pull/730), [#693](https://github.com/spencerduncan/redshipblueship/issues/693)) MM's own autosave interval slider
  (`gEnhancements.Saving.AutosaveInterval`, 1–60 minutes, default 5, shown once
  Autosave is on) — are hosted there (PR [#695](https://github.com/spencerduncan/redshipblueship/pull/695), [#682](https://github.com/spencerduncan/redshipblueship/issues/682)). The slider sets
  Majora's Mask's interval only; Ocarina of Time's autosave interval is a fixed
  3 minutes.

**Open problems you may meet while playing** (found in or around the 2026-09 playtest and
checked in the code at `200adaea`; each links the issue that will fix it; at `851b1ee9` all
of them are fixed, the last since PR [#1043](https://github.com/spencerduncan/redshipblueship/pull/1043). Every issue still open is listed
under "Open issues at `af4d941f`" below):

**~~Shops sell only their own game's items~~ — plain shops, Business Scrubs, Tingle, OoT's merchants and the Treasure Chest Game RESOLVED** ([#800](https://github.com/spencerduncan/redshipblueship/issues/800)).
#800's first pass makes plain shops cross-game in both directions. An Ocarina of Time shop
can sell a Majora's Mask item: with OoT's "Shop Shuffle" on, a shelf that shop shuffle
emptied can hold an MM item. The shelf shows the MM item's model (or the mystery item when
Ocarina of Time cannot draw that model yet), the textbox names it at the shelf's price, and
buying it sends the item to Majora's Mask like a cross-game chest. A Majora's Mask shop slot
(the shelves, the Bomb Shop owner's hand item, Gorman's milk and the Milk Bar) can hold an
Ocarina of Time item, shown with its own model and name, sold once for the whole game and
delivered on your next arrival in Hyrule. Two MM shop slots are shuffled in every world (the
Curiosity Shop's special and the Bomb Shop's fourth item), so this can happen even with MM's
"Shuffle Shops" off. #800's second pass adds OoT's Business Scrubs: with OoT's "Scrubs
Shuffle" on, a scrub can sell a Majora's Mask item at its own price, its offer names the item
(unless "Merchant Hint Text" is off or Mysterious Shuffle is on, as for its own items), and buying it sends the item
to Majora's Mask like a cross-game chest. It also adds MM's Tingle: with MM's "Shuffle Tingle
Maps" on, either or both of his two offers can be an Ocarina of Time item at its own price; his
offer names it, he sells it once for the whole game, and it is delivered on your next arrival in
Hyrule. It also adds OoT's merchants (the Magic Bean Salesman, Medigoron, Granny's Shop and the
carpet salesman): with OoT's "Shuffle Merchants" on, a merchant it shuffles can sell a Majora's
Mask item at its own price, its offer names the item (unless "Merchant Hint Text" is off or
Mysterious Shuffle is on), and buying it sends the item to Majora's Mask like a cross-game
chest. It also adds the Treasure Chest Game's prize: the final chest can hold a Majora's Mask
item in any world (no setting needed), the Lens of Truth shows that item's model above the
chest, and opening it sends the item to Majora's Mask like a cross-game chest. The game's other
chests stay its own: Ship of Harkinian never shuffles them. Shop
shuffle, scrub shuffle, merchant shuffle and Tingle map shuffle are off by default (OoT's
"Shop Shuffle", "Scrubs Shuffle" and "Shuffle Merchants", MM's "Shuffle Shops" and "Shuffle
Tingle Maps"). MM's shop stock joins
the shared bag when MM's "Shuffle Shops" is on, so an MM shop item can turn up in an OoT
chest or on an OoT shelf; OoT's shop stock never leaves Hyrule.

**~~The Check Trackers name "Blue Rupee" or "Junk" for a cross-game chest~~ — RESOLVED** ([#796](https://github.com/spencerduncan/redshipblueship/issues/796), PR [#813](https://github.com/spencerduncan/redshipblueship/pull/813)).
Fixed by PR #813: once a cross-game chest is collected, Ocarina of Time's Check Tracker lists it
with the item found and "(MM)", for example "(Bunny Hood (MM))", and the MM Check Tracker with
the item and "(OoT)". OoT's tracker search finds the chest by that name too. The original
report: both trackers named the placeholder the chest holds in its own game's tables, "(Blue
Rupee)" in OoT and "(Junk)" in MM, while the pickup message named the real item.

**~~Do not open Ocarina of Time's Save Editor or Message Viewer while you are in Majora's Mask~~ — RESOLVED** ([#797](https://github.com/spencerduncan/redshipblueship/issues/797)).
Fixed: OoT's Save Editor, Value Viewer, Message Viewer, Gameplay Stats, Check, Entrance and
Item Trackers and Time Splits now draw only while Ocarina of Time is running, and so do
their pages in the menu (Dev Tools > Save Editor and the others). In Termina they stay open
but are not drawn; back in Hyrule they come back as they were. The original report:
~~The two games share one save buffer, and OoT's windows do not yet check which game is
running. In Termina, OoT's Save Editor rewrites health and magic values through OoT's save
layout on every frame it draws, and those bytes are Majora's Mask's save. Opening its page in
the menu (Dev Tools > Save Editor) is enough, even with the window closed. The Message
Viewer's "Display Message" crashes the game there, and clicking the last split in Time
Splits writes into MM's save too. OoT's Item Tracker, Gameplay Stats and Value Viewer show
garbage in Termina, and its Check and Entrance Trackers show "Waiting for file load...".
Use these windows in Hyrule only. MM's own Item and Check Trackers are not affected.~~

**~~Do not use Ocarina of Time's console commands while you are in Majora's Mask~~ — RESOLVED** ([#798](https://github.com/spencerduncan/redshipblueship/issues/798)).
Fixed: every Ocarina of Time console command now runs only while Ocarina of Time is running.
In Termina the console answers that the command is an Ocarina of Time command and changes
nothing. Toggling "Fix Broken Giant's Knife Bug" in Termina, or anywhere outside gameplay,
now only changes the setting. The original report:
~~OoT's console commands `map`, `rupee`, `bottle`, `bItem`, `item`, `give_item` and
`entrance` do the same kind of damage in Termina as its Save Editor (they write MM's save or
crash), and so can toggling OoT's "Fix Broken Giant's Knife Bug" setting there. Use them in
Hyrule only.~~

~~**The Combo Tracker's Majora's Mask panel reads "No data yet" while you play Majora's Mask**~~ — RESOLVED ([#799](https://github.com/spencerduncan/redshipblueship/issues/799)).
The panel read Majora's Mask only from the copy kept while you are in the other game, and
arriving in Termina uses that copy up, so from arrival until your first save in Majora's Mask
or until you left Termina again the panel said "No data yet", the crossings hosted in MM
checks showed a question-mark icon, and their list gave no collected count (seen on screen
before the fix). Now, while you play Majora's Mask, the panel and the crossing list read its
running save, and a collected check shows at once, with no save. In Hyrule it reads the copy,
as of the last game switch or save. (Since PR [#929](https://github.com/spencerduncan/redshipblueship/pull/929)
the panel prints no note saying which of the two it shows.)

**~~The Combo Tracker's Ocarina of Time panel lists more checks than Ocarina of Time's Check Tracker~~ — RESOLVED** ([#1126](https://github.com/spencerduncan/redshipblueship/issues/1126)). The panel listed every location the seed put an item on, excluded locations and unshuffled shop slots included, so its list and "Checks: n / m" did not match the Check Tracker's, and Always Show Gold Skulltulas and Hide Unshuffled Shop Item Checks did nothing there. Now, on a randomizer file, the panel lists what the Check Tracker lists; while you play Majora's Mask it keeps the list from your last time in Hyrule. Still open: a saved Always Show Gold Skulltulas or Hide Unshuffled Shop Item Checks reaches the panel only once Ocarina of Time's Check Tracker window has been shown since launch, because the panel reads both from that tracker, which does not load them with a file.

**~~The Combo Item Tracker lists Majora's Mask's items by name until Majora's Mask has started~~ — RESOLVED** ([#963](https://github.com/spencerduncan/redshipblueship/issues/963), PR [#1043](https://github.com/spencerduncan/redshipblueship/pull/1043); [#458](https://github.com/spencerduncan/redshipblueship/issues/458)).
Fixed by PR #1043 (2026-10-20): the curated archive Ocarina of Time mounts at launch
(`redship-oot.o2r`, rebuilt from your extracted game data when its stamp is old) now carries
Majora's Mask's item icons, so the overlay draws them from the first frame, before Majora's Mask
has run. Not yet looked at by hand. In the same overlay, PR
[#1051](https://github.com/spencerduncan/redshipblueship/pull/1051)
([#962](https://github.com/spencerduncan/redshipblueship/issues/962)) moved the default position
from the HUD's hearts to the left edge just below the hearts and magic bar. A position an earlier
build saved in `imgui.ini` is kept: press Combo > Tracker Settings > Move to Default Position once
(shown while Window Type is Floating). The original report:
~~Majora's Mask's item icons come from its own archive and are loaded the first time Majora's
Mask starts in a session. Until then (in a session that begins in Ocarina of Time, until your
first crossing into Termina) the overlay's Majora's Mask section shows each item's name
instead of its icon, dimmed while you do not hold it, with its count where it has one. The
items are the same ones the icons would show. Once Majora's Mask has started, its icons stay
loaded until you quit. Ocarina of Time's icons load at launch, so its section and the Shared
section always have icons.~~

### Open issues at `af4d941f`

Every issue open at this commit that this project's agents filed, one line each, with what a
player sees. Epics and trackers are listed for what they leave out of this build. (Refreshed
2026-10-23 at `af4d941f`. Closed since the `4fc4fd41` list: [#1129](https://github.com/spencerduncan/redshipblueship/issues/1129) (PR [#1147](https://github.com/spencerduncan/redshipblueship/pull/1147)), [#1140](https://github.com/spencerduncan/redshipblueship/issues/1140) (PR [#1149](https://github.com/spencerduncan/redshipblueship/pull/1149)) and [#1152](https://github.com/spencerduncan/redshipblueship/issues/1152) (PR [#1153](https://github.com/spencerduncan/redshipblueship/pull/1153)). The list at `4fc4fd41` was refreshed 2026-10-23. Closed between the `30b841c1` list and that one: [#964](https://github.com/spencerduncan/redshipblueship/issues/964) (PR [#1113](https://github.com/spencerduncan/redshipblueship/pull/1113)), [#996](https://github.com/spencerduncan/redshipblueship/issues/996) (PR [#1114](https://github.com/spencerduncan/redshipblueship/pull/1114)), [#1085](https://github.com/spencerduncan/redshipblueship/issues/1085) (PR [#1111](https://github.com/spencerduncan/redshipblueship/pull/1111)), [#599](https://github.com/spencerduncan/redshipblueship/issues/599) (PR [#1117](https://github.com/spencerduncan/redshipblueship/pull/1117)), [#1037](https://github.com/spencerduncan/redshipblueship/issues/1037) (PR [#1119](https://github.com/spencerduncan/redshipblueship/pull/1119)) and [#1044](https://github.com/spencerduncan/redshipblueship/issues/1044) (PR [#1116](https://github.com/spencerduncan/redshipblueship/pull/1116)); filed and closed after the tag: [#1120](https://github.com/spencerduncan/redshipblueship/issues/1120) (PR [#1122](https://github.com/spencerduncan/redshipblueship/pull/1122)), [#1121](https://github.com/spencerduncan/redshipblueship/issues/1121) (PR [#1125](https://github.com/spencerduncan/redshipblueship/pull/1125)), [#1123](https://github.com/spencerduncan/redshipblueship/issues/1123) (PR [#1124](https://github.com/spencerduncan/redshipblueship/pull/1124)), [#1126](https://github.com/spencerduncan/redshipblueship/issues/1126) (PR [#1138](https://github.com/spencerduncan/redshipblueship/pull/1138)), [#1128](https://github.com/spencerduncan/redshipblueship/issues/1128) (PR [#1137](https://github.com/spencerduncan/redshipblueship/pull/1137)), [#1135](https://github.com/spencerduncan/redshipblueship/issues/1135) (PR [#1136](https://github.com/spencerduncan/redshipblueship/pull/1136)) and [#1139](https://github.com/spencerduncan/redshipblueship/issues/1139) (PR [#1144](https://github.com/spencerduncan/redshipblueship/pull/1144)). The list at `30b841c1`, the code of `v0.3.0-prealpha`, was refreshed 2026-10-22. Closed between the `db8d38e8` list and that one: [#1069](https://github.com/spencerduncan/redshipblueship/issues/1069) (PR [#1102](https://github.com/spencerduncan/redshipblueship/pull/1102)), [#1072](https://github.com/spencerduncan/redshipblueship/issues/1072) (PR [#1075](https://github.com/spencerduncan/redshipblueship/pull/1075); see [#1070](https://github.com/spencerduncan/redshipblueship/issues/1070) below) and [#1101](https://github.com/spencerduncan/redshipblueship/issues/1101) (PRs [#1104](https://github.com/spencerduncan/redshipblueship/pull/1104) to [#1108](https://github.com/spencerduncan/redshipblueship/pull/1108)). Closed between the `7671196a` and `db8d38e8` lists: [#855](https://github.com/spencerduncan/redshipblueship/issues/855) (PR [#1074](https://github.com/spencerduncan/redshipblueship/pull/1074)), [#1055](https://github.com/spencerduncan/redshipblueship/issues/1055) (PR [#1076](https://github.com/spencerduncan/redshipblueship/pull/1076)), [#1059](https://github.com/spencerduncan/redshipblueship/issues/1059) (PR [#1073](https://github.com/spencerduncan/redshipblueship/pull/1073)), [#1061](https://github.com/spencerduncan/redshipblueship/issues/1061) (PR [#1077](https://github.com/spencerduncan/redshipblueship/pull/1077)), [#1065](https://github.com/spencerduncan/redshipblueship/issues/1065) (PR [#1081](https://github.com/spencerduncan/redshipblueship/pull/1081)), [#1066](https://github.com/spencerduncan/redshipblueship/issues/1066) (PR [#1083](https://github.com/spencerduncan/redshipblueship/pull/1083)), [#1068](https://github.com/spencerduncan/redshipblueship/issues/1068) (PR [#1080](https://github.com/spencerduncan/redshipblueship/pull/1080)), [#1071](https://github.com/spencerduncan/redshipblueship/issues/1071) (PR [#1078](https://github.com/spencerduncan/redshipblueship/pull/1078)), [#1005](https://github.com/spencerduncan/redshipblueship/issues/1005) to [#1009](https://github.com/spencerduncan/redshipblueship/issues/1009) (PR [#1079](https://github.com/spencerduncan/redshipblueship/pull/1079)), [#1018](https://github.com/spencerduncan/redshipblueship/issues/1018) to [#1022](https://github.com/spencerduncan/redshipblueship/issues/1022) (PR [#1082](https://github.com/spencerduncan/redshipblueship/pull/1082)), [#965](https://github.com/spencerduncan/redshipblueship/issues/965) (PR #1091), [#967](https://github.com/spencerduncan/redshipblueship/issues/967) (PR #1096), [#968](https://github.com/spencerduncan/redshipblueship/issues/968) (PR #1088), [#969](https://github.com/spencerduncan/redshipblueship/issues/969) (PR #1095), [#970](https://github.com/spencerduncan/redshipblueship/issues/970) (PR #1094), [#971](https://github.com/spencerduncan/redshipblueship/issues/971) (PR #1097), [#994](https://github.com/spencerduncan/redshipblueship/issues/994) (PR #1098), [#997](https://github.com/spencerduncan/redshipblueship/issues/997) (PR #1090), [#1010](https://github.com/spencerduncan/redshipblueship/issues/1010) to [#1017](https://github.com/spencerduncan/redshipblueship/issues/1017) (PR #1086), [#1023](https://github.com/spencerduncan/redshipblueship/issues/1023) (PR #1089), [#1024](https://github.com/spencerduncan/redshipblueship/issues/1024) to [#1026](https://github.com/spencerduncan/redshipblueship/issues/1026) (PR #1087), [#1030](https://github.com/spencerduncan/redshipblueship/issues/1030) to [#1034](https://github.com/spencerduncan/redshipblueship/issues/1034) (PR #1093), [#1035](https://github.com/spencerduncan/redshipblueship/issues/1035), [#1036](https://github.com/spencerduncan/redshipblueship/issues/1036), [#1038](https://github.com/spencerduncan/redshipblueship/issues/1038), [#1062](https://github.com/spencerduncan/redshipblueship/issues/1062) and [#1064](https://github.com/spencerduncan/redshipblueship/issues/1064) (PR #1099), [#1040](https://github.com/spencerduncan/redshipblueship/issues/1040) (PR #1092), [#1060](https://github.com/spencerduncan/redshipblueship/issues/1060) (PR #1100) and [#1067](https://github.com/spencerduncan/redshipblueship/issues/1067) (PR #1084).)

- No issue: on Ocarina of Time's shop shelves and in its get-item cutscene, the items 2Ship2Harkinian draws with its own code (owl statues, boss and enemy souls, clocks, frogs, the swim ability, Triforce pieces, traps and ocarina buttons) still show the "?" stand-in instead of their model. Buying or getting one works. The stray fairies and the bottled fairy draw since PR [#1113](https://github.com/spencerduncan/redshipblueship/pull/1113) ([#964](https://github.com/spencerduncan/redshipblueship/issues/964), closed), as still figures with no wing flap or colour pulse; the four boss remains, the Moon's Tear and the caught seahorse since PR [#1042](https://github.com/spencerduncan/redshipblueship/pull/1042).
- [#966](https://github.com/spencerduncan/redshipblueship/issues/966): the audit of which settings on the shared pages Majora's Mask does not honour. Every row it filed is fixed on `main`, [#1037](https://github.com/spencerduncan/redshipblueship/issues/1037) (Damage Multiplier) last, by PR [#1119](https://github.com/spencerduncan/redshipblueship/pull/1119). Skip Boss Introductions reaches Majora's Mask since PR [#1124](https://github.com/spencerduncan/redshipblueship/pull/1124); Skip To File Select has no Majora's Mask effect (the title screen is Ocarina of Time's) and no issue of its own; the settings that stay Ocarina of Time only by design are under "Majora's Mask specific".
- [#975](https://github.com/spencerduncan/redshipblueship/issues/975) (epic) and its phases [#976](https://github.com/spencerduncan/redshipblueship/issues/976) to [#980](https://github.com/spencerduncan/redshipblueship/issues/980): cross-game masks as in OoTMM. The five shared masks of P2 ([#978](https://github.com/spencerduncan/redshipblueship/issues/978)) are in with PR #1095, off by default, and P1 ([#976](https://github.com/spencerduncan/redshipblueship/issues/976)) is the groundwork they ride (PR [#1002](https://github.com/spencerduncan/redshipblueship/pull/1002)); both stay open for the operator to close. P3 ([#977](https://github.com/spencerduncan/redshipblueship/issues/977), wearing the other game's masks) has its groundwork since PR [#1112](https://github.com/spencerduncan/redshipblueship/pull/1112) (the worn-mask fields and draw hooks; no world can turn them on, so nothing changes in play) and stays open for the rest. Not built: each game's masks in the other (P4 and P5, [#979](https://github.com/spencerduncan/redshipblueship/issues/979), [#980](https://github.com/spencerduncan/redshipblueship/issues/980)).
- [#1070](https://github.com/spencerduncan/redshipblueship/issues/1070): reported on `v0.2.0-prealpha`: Woodfall Temple does not rise (playing the Sonata of Awakening at the Woodfall platform has no effect). Root cause not settled. The playtester's save and the code on the path were checked and are clean for that file. The lead is Majora's Mask's free-play rule that a note played while L is held cancels songs (L bound to the same input as C-up or C-left would do it); the playtester's controller config (`shipofharkinian.json`) is awaited to confirm it. The ocarina-button view of [#1072](https://github.com/spencerduncan/redshipblueship/issues/1072) is in since PR #1075, so a missing C-up shows in the trackers.
- [#1063](https://github.com/spencerduncan/redshipblueship/issues/1063): reported on `v0.2.0-prealpha`: an Ocarina of Time texture overhaul pack worked for a while, then the game crashed. It cannot be reproduced without the pack's name; the issue lists what a report needs (the pack, the `mods/` listing, the log and any `.dmp`).
- [#993](https://github.com/spencerduncan/redshipblueship/issues/993) (partly fixed): with Enable Available Checks on, the Combo Tracker shows which checks you can reach (SoH's lock glyph, the Available count and Only Show Available Checks per pane) for both games since PR [#1147](https://github.com/spencerduncan/redshipblueship/pull/1147), the issue's option 1; it stays open until the operator picks that option. The game you are not playing shows what was reachable when you left it; it does not yet change as you collect things in the other game ([#1146](https://github.com/spencerduncan/redshipblueship/issues/1146)). Majora's Mask's own Check Tracker window still has no menu row (console: `set gWindows.CheckTracker 1`).
- No issue: the Combo Tracker's Saved Hide never hides a Majora's Mask check. Since PR [#1137](https://github.com/spencerduncan/redshipblueship/pull/1137) the Combo Tracker follows Check Tracker Settings' Hide options, but it gives every obtained Majora's Mask check the Collected state, never Saved, so Saved Hide alone leaves those checks showing, and Collected Hide hides them even when saved. Ocarina of Time's rows follow both options as its own Check Tracker does.
- [#1145](https://github.com/spencerduncan/redshipblueship/issues/1145): with Shared Kokiri Sword on, Ocarina of Time's Kokiri Sword found in Termina still crosses and reaches Ocarina of Time at its next arrival, instead of stepping the shared sword at the pickup as the capacity upgrades do since PR [#1122](https://github.com/spencerduncan/redshipblueship/pull/1122). Nothing is lost; it arrives late.
- [#1127](https://github.com/spencerduncan/redshipblueship/issues/1127) (deferred): the Combo Tracker's Majora's Mask pane groups enemy drops and unconnected grottos differently from Majora's Mask's own Check Tracker.
- [#1146](https://github.com/spencerduncan/redshipblueship/issues/1146) (filed by the #1129 lane): the Combo Tracker's pane for the game you are not playing keeps the reachable set captured when you left that game and does not refresh as you collect things in the other game. Its "Available" counts also count every listed check, where SoH's leave out the checks its Hide options hide, so with Hide options on they can read higher than SoH's Check Tracker.
- [#1151](https://github.com/spencerduncan/redshipblueship/issues/1151) (filed by the #1129 lane): seed packages exported by `main` builds from `fabe7d33` to `e3de3ff1` (a 1004-byte combo record) are refused from `efe573ec` on. The issue's title names `v0.3.0-prealpha`, which wrote no seed packages, and the zero-extend it proposes would not rescue a package from before `fabe7d33`, which is refused one check earlier, on Majora's Mask's half ("Seed packages from `main` builds before `efe573ec` are refused" under "Randomizer generation"). Workaround: generate the seed again.
- No issue (PR [#1149](https://github.com/spencerduncan/redshipblueship/pull/1149)): in the Combo Item Tracker, with Owl Statue shuffle on, the owl cells draw wider than their cell and overlap (the overlay does not clamp Majora's Mask's 46x24 owl face yet; lane IT-3, [#1141](https://github.com/spencerduncan/redshipblueship/issues/1141)). Majora's Mask's small keys show the keys found against the temple's total and light while one is held; SoH's Key Counts display mode is not honoured for them yet ([#1143](https://github.com/spencerduncan/redshipblueship/issues/1143)). At 1280x800 the overlay draws at about 72% of SoH's size, because Majora's Mask's section is now 18 table lines long.
- No issue (PR [#1154](https://github.com/spencerduncan/redshipblueship/pull/1154)): under Ocarina of Time both SoH's Check Tracker and the Combo Tracker draw under the same window settings (Window Type, Enable Dragging, Only Enable While Paused, Display Mode, Background Color, Font Size) until SoH's window retires (lane L5 of the tracker scoping); under Termina only the Combo Tracker draws.
- [#1130](https://github.com/spencerduncan/redshipblueship/issues/1130) to [#1134](https://github.com/spencerduncan/redshipblueship/issues/1134) and [#1141](https://github.com/spencerduncan/redshipblueship/issues/1141) to [#1143](https://github.com/spencerduncan/redshipblueship/issues/1143): planned tracker and menu work. Running: L1b ([#1130](https://github.com/spencerduncan/redshipblueship/issues/1130), the Combo Tracker's window chrome). Queued: L4 ([#1131](https://github.com/spencerduncan/redshipblueship/issues/1131), navigation), L5 ([#1132](https://github.com/spencerduncan/redshipblueship/issues/1132), one Check Tracker for both games), L7 ([#1133](https://github.com/spencerduncan/redshipblueship/issues/1133), retiring the Combo header), IT-3 ([#1141](https://github.com/spencerduncan/redshipblueship/issues/1141), the Item Tracker's settings), IT-2 ([#1142](https://github.com/spencerduncan/redshipblueship/issues/1142), Ocarina of Time's missing Item Tracker rows), IT-4 ([#1143](https://github.com/spencerduncan/redshipblueship/issues/1143), counts and metrics) and L8 ([#1134](https://github.com/spencerduncan/redshipblueship/issues/1134), the docs sweep after L7). Nothing of these in this build yet.

- [#925](https://github.com/spencerduncan/redshipblueship/issues/925): nothing in play. A test row (`AllTests`) failed once on a fresh build tree and then passed; probably the test collision PR [#931](https://github.com/spencerduncan/redshipblueship/pull/931) fixed. Watched. Since PR [#1118](https://github.com/spencerduncan/redshipblueship/pull/1118) every save-writing test row uses its own directory.
- [#824](https://github.com/spencerduncan/redshipblueship/issues/824): nothing in play. The test row `ForeignItemGiveReverse` ended once without a verdict and has not done so again. Watched; PR [#1118](https://github.com/spencerduncan/redshipblueship/pull/1118) removed the shared save directory it most likely collided in.
- [#694](https://github.com/spencerduncan/redshipblueship/issues/694): MM's game-over prompt (Combo > Majora's Mask, off by default) has never been seen in this build; its artwork may be missing. Awaiting a playtest.
- [#607](https://github.com/spencerduncan/redshipblueship/issues/607): an OoT item found in Termina shows its OoT icon in MM's get-item textbox (PR [#761](https://github.com/spencerduncan/redshipblueship/pull/761)); not yet looked at in play.
- [#645](https://github.com/spencerduncan/redshipblueship/issues/645): the single-bag fill is in this build; the epic stays open for two items awaiting the operator: O2, the per-direction crossing-edge expressions (no merged PR records them), and O11, the strictness presentation (the Goal row of PR [#760](https://github.com/spencerduncan/redshipblueship/pull/760) does not record one).
- [#577](https://github.com/spencerduncan/redshipblueship/issues/577): foreign items show their real models in both games; every child has landed, and the epic awaits closing.
- [#564](https://github.com/spencerduncan/redshipblueship/issues/564): the one-game audit. Its remaining rows guard against saves that mix two worlds; a player creating and playing files on one build should not meet them.
- [#573](https://github.com/spencerduncan/redshipblueship/issues/573): no cross-game entrance randomization; the Happy Mask Shop door is always the crossing.
- [#574](https://github.com/spencerduncan/redshipblueship/issues/574): no multiworld over the combo.
- [#460](https://github.com/spencerduncan/redshipblueship/issues/460): a netplay grant for the game you are in would wait for your next crossing; the combo's netplay relay is not built by default (`RSBS_NETPLAY` is OFF), so a player does not meet this.
- [#497](https://github.com/spencerduncan/redshipblueship/issues/497): nothing new in play. PR [#687](https://github.com/spencerduncan/redshipblueship/pull/687) built its capability gating, shared-intent marker and Combo section; only ADR 0004's pairing status, entrance links and hot-swap pages remain, and whether to build them or close the issue is the operator's call.
- [#500](https://github.com/spencerduncan/redshipblueship/issues/500) and [#492](https://github.com/spencerduncan/redshipblueship/issues/492): the Phase 3.2 and 3.1 trackers. The two-way randomizer and the cross-game logic they track are in this build; whether to close them is the operator's call.
- [#310](https://github.com/spencerduncan/redshipblueship/issues/310): the manual QA pass by a person with the ROMs is still to do.

The community reports [#634](https://github.com/spencerduncan/redshipblueship/issues/634),
[#635](https://github.com/spencerduncan/redshipblueship/issues/635) and
[#636](https://github.com/spencerduncan/redshipblueship/issues/636) are open for the operator's
reply; their fixes are in this build (see their entries below).

### Back up your saves. Seriously.

The cross-game save (`.redsave`) format **has been re-versioned** since the last
revision of this document — it is now version 3 (durable crossings, PR
[#736](https://github.com/spencerduncan/redshipblueship/pull/736)), and this build reads versions 1 and 2 too (`src/common/save.h`). The format has grown several times since July as the
combo context gained its identity, commit and settings records. A refused or
corrupt `.redsave` is now quarantined with a reason rather than overwritten
([#533](https://github.com/spencerduncan/redshipblueship/issues/533), PR
[#568](https://github.com/spencerduncan/redshipblueship/pull/568)), and every
durable write goes through one commit point with a generation stamp (PR
[#569](https://github.com/spencerduncan/redshipblueship/pull/569)). None of that
is a promise that the *next* format change will migrate. Treat any progress made on
a pre-alpha build as disposable, and keep copies of files you care about.

**Older paired files that need their Majora's Mask settings restored are refused.** Since
PRs [#1102](https://github.com/spencerduncan/redshipblueship/pull/1102) and
[#1106](https://github.com/spencerduncan/redshipblueship/pull/1106) (2026-10-22, both in
`v0.3.0-prealpha`), the Majora's Mask half of a paired save holds two more fields: Moon
Crash Behavior and the excluded-check list of Randomizer → MM Locations. Each is appended
inside MM's save, so it shifts the fields after it. A paired file saved by an earlier
build, `v0.2.0-prealpha` included, whose Majora's Mask settings were changed at the title
screen, so that the load has to restore them, is refused as unrestorable: the newer layout
reads other bytes where the list now sits. A file whose MM settings were not changed since
it was created loads as before. The release ships this rather than a migration, as the
trick set's identical shift was accepted before it; create a new file for
`v0.3.0-prealpha`.

**Paired files created before PR #895 (2026-10-10) are refused.** PR
[#895](https://github.com/spencerduncan/redshipblueship/pull/895) changed what Majora's
Mask starts a paired world with (the Ocarina of Time and the Song of Time; the sword and
shield went into the pool), and that start is part of the world's identity. An older
file's identity cannot match it, so pressing A on the file in the Ocarina of Time file
select plays the error sound, posts "Refused: Majora's Mask options differ", and keeps
you on the file list. This is read from the code by that PR; no older file was loaded.
Erase the file and create a new one. Since PR
[#911](https://github.com/spencerduncan/redshipblueship/pull/911), a file whose
`.redsave` belongs to another file (a copy put at this slot's path, for example) is
refused the same way, with "Refused: Cross-game record is for another file".

**Paired files created before PR #680 (2026-09-17) are not migrated.** Increment 2
moved the entire paired generation to the file-create seam, and the arrival in Majora's
Mask no longer generates anything: it hydrates the frozen MM half or refuses. Such a file
loads and plays in Ocarina of Time under the Cross-Game Rules it was created with (a
file whose unified save carries the combo record, written since PR #628 on 2026-08-06,
has any changed rule restored at load, as the between-sessions entry below describes;
an older file has no record to compare and is exempt).
What happens at the crossing depends on whether the file entered Majora's Mask before
#680:

- **Never crossed:** it has no frozen MM half. ~~The first crossing into Majora's Mask
  is refused with a toast saying the file has no paired Majora's Mask world: Termina
  stays un-randomized and the slot is latched against writes to the pair for the rest
  of the session.~~ **RESOLVED (#836):** the file is not opened. Pressing A on it in the
  Ocarina of Time file select plays the error sound, posts "Refused: This file has no
  Majora's Mask world", and keeps you on the file list. Nothing is written.
- **Crossed before #680:** it carries the MM half the old arrival generated, and the
  arrival hydrates whatever frozen half it finds. It either plays that old pre-#680 MM
  world, or, if its creation-time MM profile stamp no longer matches this build's MM
  options, the load restores the options from that half when they reproduce the stamp.
  ~~Otherwise it warns at load ("Not restored: Majora's Mask options differ") and the
  crossing is refused by the MM-options check.~~ **RESOLVED (#836):** otherwise the file
  is not opened ("Refused: Majora's Mask options differ" at the file select). Which one a given file meets cannot be
  decided by reading. ~~If that old arrival's generation failed, it reverted to a vanilla
  MM half, which the arrival hydrated under the pairing: Termina played vanilla and the
  vanilla half was saved back (#564 V7).~~ **RESOLVED (#836 PR 2):** a paired file whose
  MM half is vanilla, or is another pair's world (#564 V11), is not opened ("Refused:
  This file has no Majora's Mask world" at the file select), and the arrival refuses such
  a half too.

This project is pre-release — the operator has accepted invalidating existing saves
rather than spending effort on migration. **Create a new file**; there is no recovery
path for the old one.

~~**Paired files created between PR #680 and the switch (PR #743, 2026-09-27) are NOT
refused.** They load and keep playing the world they were created with: the old
overlay world, where a crossed item is also still in its home pool.~~ **SUPERSEDED (PR
#895):** such a file is refused, as above.

~~**Paired files created before PR #772 (2026-09-28) are NOT refused, and are safe to
play.**~~ **SUPERSEDED (PR #895):** such a file is refused, as above. What PR #772 changed
still holds for every file: every arrival in MM gives the session the cross-game slot
number, so the moon-crash reset and the owl save no longer wipe the MM half (PR
[#772](https://github.com/spencerduncan/redshipblueship/pull/772)), and a moon crash
restores the file's last save (PR [#789](https://github.com/spencerduncan/redshipblueship/pull/789); see the #785 entry below). To see whether a
file's MM half exists, open the Combo Tracker: its Majora's Mask panel shows MM's world
under its header, or nothing under the header when there is no half to read (the panel's
explanatory notes were cut by PR [#929](https://github.com/spencerduncan/redshipblueship/pull/929)). (#772 also describes
an `[MM v]` / `[MM _]` file-select marker; only `ComboMenuBar` printed it, and it was never
constructed. PR [#794](https://github.com/spencerduncan/redshipblueship/pull/794) deleted
`ComboMenuBar`, and no screen carries the marker: a paired creation stamps both halves' markers at creation
(#765), so for any file made since then it says nothing about which half was played.)

**A seed string does not make the world it made on an older build.** PR [#763](https://github.com/spencerduncan/redshipblueship/pull/763)'s two
trick tightenings moved generated worlds, and since PR [#774](https://github.com/spencerduncan/redshipblueship/pull/774) the OoT half of every
newly generated world differs from what a build before it made from the same seed string
and settings (OoT's excluded locations now reach the settings fingerprint, in every
environment). A non-default goal is a different world from the same seed (PR [#760](https://github.com/spencerduncan/redshipblueship/pull/760)).
The 2026-09-30 wave moved default worlds three more times: MM shop slots as crossing hosts
(PR [#840](https://github.com/spencerduncan/redshipblueship/pull/840)), the Treasure Chest
Game's prize as a crossing host (PR [#874](https://github.com/spencerduncan/redshipblueship/pull/874))
and MM's paired start (PR [#895](https://github.com/spencerduncan/redshipblueship/pull/895)).
Nothing stored in an existing save is recomputed; files created before PR #895 are
refused for their identity, as above.

**A paired file made by a newer build can be refused by an older one.** Since PR
[#1004](https://github.com/spencerduncan/redshipblueship/pull/1004) ([#1003](https://github.com/spencerduncan/redshipblueship/issues/1003), 2026-10-20) the fingerprint a paired file records names the
generator's version. A build accepts the versions it knows, from before PR #1004 up to its
own, so files created by older builds still load; a file stamped by a newer generator reads
as a fingerprint mismatch ("Refused:" at the file select). Open it in the build that created
it or a newer one. In the same way, since PR [#1002](https://github.com/spencerduncan/redshipblueship/pull/1002) a file made by a later build with one
of the cross-game item rules on is refused by name (`crossItemFlags` on stderr) and is not
quarantined: the file is not damaged, it needs that later build. The fingerprint is a label:
the version moves no placement, and no generated world moved between PR #974 and `7671196a`.

---

## Pre-playtest smoke, 2026-09-27, `d928d67d`

Before anyone plays, the build under test was run through the checks that hosted CI
cannot run, because they need the ROMs. Code at `d928d67d` is the code at `d6c8f270`:
the two commits after it changed documentation only.

**How it was run.** A fresh Windows checkout at `d928d67d`, built from cold (Release,
MSVC). `oot.o2r` and `mm.o2r` were extracted from the ROMs by that build's own
extractor, and `soh.o2r`, `2ship.o2r` and `redship.o2r` were generated at the same commit.
The config was OpenGL only, and windows opened without taking focus. There was no `Save/`
folder from earlier play.

| Check | Result |
|---|---|
| `redship` tier | 138 of 138 passed, none skipped |
| `rando` tier | 40 of 40 passed, none skipped. All four golden-world rows passed with `tests/golden/` untouched, so the generated worlds are the pinned ones |
| `ui` tier (OpenGL, 1280x800, ROM archives mounted) | passed: 105 captures, every one `pass`, none skipped |
| `integration` | 5 of 6 passed. `IntSwitchOoTHmsToMm` timed out at 120 s, which was [#544](https://github.com/spencerduncan/redshipblueship/issues/544) (fixed since; see "CI and quality gates" below). `IntBootOoT`, `IntBootMM`, `IntSwitchMmClockTownSouthToOoT`, `IntArchiveHotswapCycle` (4 arrivals) and `IntGameplayRoundtrip` passed |
| `integration-soak` | `IntGameplayRoundtripSoak` passed: "3 round trip(s), warp, and door transition survived 120 live frames per phase" (70.6 s) |
| Creation time, shipped defaults | 10 real paired creations (`RSBS_CSB_SAMPLE_EVENT=10`: Generate, the creation event with the spoiler written, the arm, `Randomizer_InitSaveFile`). 10 of 10 created on the first attempt with no budget stop. Mean 8.5 s, best 6.2 s, worst 13.0 s (the one seed whose fill needed a second batch), against the 30 s budget. Every world's 50 heart pickups filled the bar with none wasted |
| DX11 menus against OpenGL | See below: no visible difference |

**What the smoke did not cover then, and a row covers now: the first crossing into a
paired Termina with the real game data loaded.** At `d928d67d` no automated row ran the
whole path in one process: create a paired file through the creation event, then walk
into MM with the ROM archives mounted. The integration rows crossed with a vanilla debug
save, and their log said so on every MM arrival:
`[MM] pairing: skipped-because-no-paired-oot-world`. The rows that did run creation and
then an MM arrival in one process (`ComboCreationEvent`, `MMPairSwitchEntry`) ran with the
OTR archive init disabled (`RSBS_DISABLE_OTR_INIT=1`), so no real game data was loaded.

Since PR [#790](https://github.com/spencerduncan/redshipblueship/pull/790) the
`integration` label has a row that does it: `IntPairedFirstCrossing`
(`redship --integration-test int-paired-first-crossing`). In one process, with the ROM
archives mounted and a real OpenGL window, it takes the title to the file select (SoH's
"Boot Sequence: File Select" path), generates the pinned world `RSBSSINGLEBAG1` on the
shipped defaults (checked: it fails if the config sets any OoT, MM or combo setting),
creates file 3 through OoT's own new-file seam (`OoT_Sram_InitSave`,
which runs the production creation event and writes the slot), loads that file back the
way the file select does, and plays it from the Market (0x01D1). The loaded file must carry
the identity the creation recorded before the load (seed, settings digests, crossing counts
and digest). It walks through the
Happy Mask Shop (0x0530) into South Clock Town (0xD800), then back through the Clock
Tower door (0xC010). It fails unless the MM arrival logs both lines §3 of the playtest
guide asks you to look for, the pairing is live in MM, the crossing store is frozen and
not empty, the MM half is the one the creation armed, nothing generated at the arrival,
and no refusal was logged or toasted. The return leg must restore OoT's half, not
regenerate it. The first green run (workstation, Windows, OpenGL, archives extracted
at the branch tip, 55.2 s) logged:

```
[MM] pairing: arrival profile matches the creation-frozen identity (13DE4C35)
[MM] pairing: HYDRATED from the frozen MM half (saveType=rando mmFinalSeed=8DDF1DEE crossingsInHyrule=51 crossingsInTermina=21) — nothing was generated at this arrival
[PFC] return leg PASS: OoT's half restored, not regenerated (file 3, OoT world seed 2852956488, sentinel deaths=777 survived), same identity: ...
```

Hosted CI cannot run it, like every `integration` row: it needs the ROM-derived archives.
It runs locally (see [`ci-gameplay-repro-postmortem.md`](ci-gameplay-repro-postmortem.md) §7)
and in the manual `integration-tests` workflow on a ROM-equipped runner. With
`RSBS_PFC_SKIP_CREATION=1` the row boots the debug save instead, as the older rows do, and
fails on the `skipped-because-no-paired-oot-world` line: that is its red half. The playtest's
first crossing (guide §3, step 3) is still the first time a person walks this path, and it
still comes first. What the row cannot see is what a person sees: it fires the door
transitions directly, and the Happy Mask Shop's Closed Forest gate is not in its path.

**The HYDRATED line's count.** At `d928d67d` every HYDRATED line ended with
`foreignPlacements=0`, even when the file had dozens of crossings. That count was the
retired item table from before the single-bag switch. Since PR
[#790](https://github.com/spencerduncan/redshipblueship/pull/790) the line prints the
crossing store's two counts instead: `crossingsInHyrule` (Majora's Mask items in Ocarina of
Time checks) and `crossingsInTermina` (Ocarina of Time items in Majora's Mask checks). The
Cross-Game Spoiler reads the same store (the Combo Tracker lists no crossings since PR [#1137](https://github.com/spencerduncan/redshipblueship/pull/1137)).

**Smaller notes.**
- The test portal (`--test-entrance`, Mido's House to the Clock Tower) skips Closed
  Forest. On the shipped defaults, the Happy Mask Shop opens only after the Deku Tree.
- No `integration` or `integration-soak` row loaded a `.redsave` at `d928d67d`. Every MM
  arrival in them was vanilla (`skipped-because-no-paired-oot-world`). `IntPairedFirstCrossing`
  (PR [#790](https://github.com/spencerduncan/redshipblueship/pull/790)) now writes and loads one. The `redship` tier's save rows do
  load `.redsave` files and refuse some of them on purpose: a verbose re-run of the tier
  printed 66 `[RsbsSave] slot N REFUSED` lines (20 rows, plus `AllTests` repeating them),
  and every one of those rows passed.

**DirectX 11.** The Windows CI run on `d6c8f270` (run 36369933984) renders the menu
pages, windows and toasts through DirectX 11 without the ROM archives. It lists 98
captures, and 90 of them were rendered. The other 8 are the original SoH pages, which are
skipped without `oot.o2r` (in both runs), so no original SoH page was compared under
DirectX 11. The 90 were compared with an OpenGL render made at `d928d67d` on the same
832x600 profile, also without the ROM archives.
- **Text:** the text drawn is identical in all 90 rendered captures. Each capture's size,
  content rectangle, scroll range and expected text match too.
- **Pixels:** no capture is pixel-identical. Between 0.11% and 1.55% of a capture's pixels
  differ, and all but 282 of those pixels (over all 90 captures) differ by exactly one
  level. Pixels differ by more than 32 levels in only 27 captures: 11 frame-corner pixels
  in each of the 22 MM Tricks captures, one table-line end pixel in each of the 3 MM Mods
  captures, and 2 table-corner pixels each on the Cross-Game Spoiler (crossings) and the
  Combo Tracker (progress, scroll 1). None of them is visible.
- **Pages checked by eye,** side by side and in a difference image: Cross-Game Rules
  (including the Goal tooltip), Majora's Mask, Windows, MM Mods, MM Randomizer (live and
  frozen), MM Tricks (live and frozen), the creation overlay, the Combo Tracker and the
  Cross-Game Spoiler. No difference is visible.
- ~~**Both backends, not a DX11 issue:** at 832x600, the Combo > Majora's Mask page cuts
  off its checkbox labels next to the Autosave note.~~ **Resolved.** SoH's own pages do the
  same at that size, and still do: rule 0 keeps them as shipped. Measured since then (the
  `ui` tier's runtime lint R9, which compares every row's rectangle with its column's clip
  rectangle, OpenGL, ROM archives mounted): SoH's pages overrun 68 rows at 832x600 (23 on
  Enhancements > Quality of Life, among them "Remember Save Location" and "Nighttime GS
  Always Spawn"), 36 at 960x704 and 2 at 1280x800. Of those, 5 at 832x600 and 3 at 960x704
  carry this project's ruled "[Both Games] " marker, which SoH shipped without (Dev Tools >
  General, Settings > General); the other 63 and 33 are SoH's rows as shipped. Ours overran 25 at 832x600 and 10 at
  960x704, not only on the Majora's Mask page: Cross-Game Rules (the pool-size sliders, the
  class headers, Shared Ocarina, all three rows since retired), MM Randomizer (two access comboboxes and seven long
  labels) and the harness's MM Row States probe. The Majora's Mask page now has two columns,
  SoH's count for a mixed page, and the long labels are shorter (the renames are listed in
  the lane's pull request), so no row of ours overruns at any of the three profiles. R9 fails
  the `ui` tier if one does again at the run's profile, and the `UiSnapshotMin` row pins that
  profile to 832x600 in CI (docs/ui-style-guide.md section 1, the width rule).

**Findings.** None apart from #544, so no new issue was filed.

---

## Smoke, 2026-10-03, `82d05d0d`

The closing run of the work merged between `200adaea` and `82d05d0d`: 49 pull requests,
[#802](https://github.com/spencerduncan/redshipblueship/pull/802) to
[#876](https://github.com/spencerduncan/redshipblueship/pull/876). Like the run above, it
covers the checks hosted CI cannot run, because they need the ROMs.

**How it was run.** The workstation's main Windows checkout. Its object directories were
cleared and the tree was rebuilt (Release, MSVC, through sccache) at the merge of the last
pull request's branch ([#872](https://github.com/spencerduncan/redshipblueship/pull/872))
onto `c36ca898`. That merge has the same tree as `82d05d0d` (`d23e4b5c`), the squash commit
that landed right after the run. `soh.o2r`, `2ship.o2r` and the two curated archives
`redship-oot.o2r` and `redship-mm.o2r` were regenerated at that tree. `oot.o2r` and `mm.o2r`
were not re-extracted: they are the ROM extractions earlier builds made. The config was
OpenGL only, and windows opened without taking focus.

| Check | Result |
|---|---|
| `redship` tier | 163 of 163 passed, none skipped (138 in the run above) |
| `rando` tier | 42 of 42 passed, none skipped (40 in the run above). All four golden-world rows passed (`GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps`) with `tests/golden/` as it stands at this commit |
| `ui` tier (OpenGL, ROM archives mounted) | 2 of 2 rows passed. The parity lint passed plain (0 gated hits) and no-grow (neither baseline gained an entry) |
| `integration` | 13 of 13 passed (6 rows in the run above, one of them red): `IntBootOoT`, `IntBootMM`, `IntSwitchOoTHmsToMm`, `IntSwitchMmClockTownSouthToOoT`, `IntArchiveHotswapCycle`, `IntCuratedArchiveBootRegen`, `IntWatchdogWedgeOoTFrame`, `IntWatchdogWedgeHandoff`, `IntWatchdogWedgeMMFrame`, `IntGameplayRoundtrip`, `IntGameplayRoundtripCapture`, `IntPairedFirstCrossing`, `IntPairedFirstCrossingDiverged` |
| `integration-soak` | `IntGameplayRoundtripSoak` passed (71.2 s) |

**Generated worlds moved during this work.** Two pull requests each re-pinned three of the
four golden files (`seed-digest-default`, `seed-digest-profile-v1`, `paired-attempt-digest`):
[#840](https://github.com/spencerduncan/redshipblueship/pull/840) (MM shops host OoT items) and
[#874](https://github.com/spencerduncan/redshipblueship/pull/874) (OoT's treasure chest game
hosts MM items). A seed string and settings that made one world at `200adaea` make a different
one now. Nothing stored in an existing save is recomputed.

**What this run did not cover.**
- Nobody has played this build. The rows drive door transitions and item gives directly; they
  do not see what a person sees.
- DirectX 11 was not compared with OpenGL.
- The creation-time sample (10 real paired creations against the 30 s budget) was not re-run.
- `ForeignItemGiveReverse` passed here. It ended once without a verdict in an earlier gate run
  and has not done so again ([#824](https://github.com/spencerduncan/redshipblueship/issues/824)).

**Findings.** None in this run. Known and open at this commit, each with its own issue: get-item
text does not say which game a crossed item belongs to
([#865](https://github.com/spencerduncan/redshipblueship/issues/865)); a chest that hosts the
other game's item always looks like a major chest
([#862](https://github.com/spencerduncan/redshipblueship/issues/862)); MM's spoiler omits the
price of an OoT item in an MM shop or Tingle slot
([#863](https://github.com/spencerduncan/redshipblueship/issues/863)); on a vanilla paired file,
OoT's Check Tracker can show checks from before a crossing commit after a reload
([#861](https://github.com/spencerduncan/redshipblueship/issues/861)); OoT's threaded save worker
can land an older generation over a crossing commit
([#851](https://github.com/spencerduncan/redshipblueship/issues/851)). All five were fixed
after this run, by PRs [#884](https://github.com/spencerduncan/redshipblueship/pull/884),
[#892](https://github.com/spencerduncan/redshipblueship/pull/892),
[#893](https://github.com/spencerduncan/redshipblueship/pull/893),
[#898](https://github.com/spencerduncan/redshipblueship/pull/898) and
[#896](https://github.com/spencerduncan/redshipblueship/pull/896).

---

## Smoke, 2026-10-12, `851b1ee9`

The closing run of the 2026-09-30 wave: 92 pull requests merged between `200adaea` and
`851b1ee9`, [#802](https://github.com/spencerduncan/redshipblueship/pull/802) to
[#940](https://github.com/spencerduncan/redshipblueship/pull/940), 43 of them after the run
above. Like the runs above, it covers the checks hosted CI cannot run, because they need the
ROMs.

**How it was run.** The workstation's main Windows checkout at `851b1ee9` (the log opens
with `=== merge origin/main onto origin/main flags=uia` and `Already up to date.`), rebuilt
in 2496 build steps (Release). `soh.o2r`, `2ship.o2r` and the two curated archives were
regenerated at 07:16 (`redship-oot.o2r`: "332 curated resource(s)", `redship-mm.o2r`: "190
curated resource(s)"). `oot.o2r` and `mm.o2r` were not re-extracted. The config was OpenGL
only, and windows opened without taking focus. The run ended at 07:30:06 on 2026-10-12.

| Check | Result |
|---|---|
| `redship` tier | 179 of 179 passed ("100% tests passed out of 179"; 163 in the run above). `AllTests` and `ForeignItemGiveReverse` passed |
| `rando` tier | 43 of 43 passed (42 in the run above). All four golden-world rows passed (`GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps`) with `tests/golden/` as it stands at this commit |
| `ui` tier (OpenGL, ROM archives mounted) | 2 of 2 rows passed (`UiSnapshot`, `UiSnapshotMin`). The parity lint passed plain ("OK: 0 gated hit(s), all in the baseline") and no-grow ("neither baseline gained an entry relative to origin/main") |
| `integration` | 14 of 14 passed (13 in the run above): `IntBootOoT`, `IntBootMM`, `IntSwitchOoTHmsToMm`, `IntSwitchMmClockTownSouthToOoT`, `IntArchiveHotswapCycle`, `IntCuratedArchiveBootRegen`, `IntWatchdogWedgeOoTFrame`, `IntWatchdogWedgeHandoff`, `IntWatchdogWedgeMMFrame`, `IntGameplayRoundtrip`, `IntGameplayRoundtripCapture`, `IntPairedFirstCrossing`, `IntPairedFirstCrossingDiverged`, `IntPairedFirstCrossingTwoCycles` |
| `integration-soak` | `IntGameplayRoundtripSoak` passed (70.6 s) |

**Generated worlds moved during this work.** Three pull requests re-pinned golden files:
[#840](https://github.com/spencerduncan/redshipblueship/pull/840) and
[#874](https://github.com/spencerduncan/redshipblueship/pull/874) (named in the run above)
each re-pinned three of the four (`seed-digest-default`, `seed-digest-profile-v1`,
`paired-attempt-digest`), and [#895](https://github.com/spencerduncan/redshipblueship/pull/895)
(MM's paired start) re-pinned all four, `seed-digest-armed-caps` included. A seed string and
settings that made one world at `200adaea` make a different one now, and paired files created
before #895 are refused (see "Back up your saves").

**What this run did not cover.**
- Nobody has played this build. The rows drive door transitions and item gives directly; they
  do not see what a person sees.
- DirectX 11 was not compared with OpenGL.
- The creation-time sample (10 real paired creations against the 30 s budget) was not re-run.
- Not tested by anyone, by a row or by hand: the cross-game warp songs, networking, and a
  crossing made after quitting and relaunching the game, or after a save made during play.
  (A crossing after the file is created, saved and loaded back from disk in the same session
  is tested: `IntPairedFirstCrossing` does exactly that.)

**Findings.** None in this run. The open issues at this commit were listed under "Open issues
at `851b1ee9`" near the top of this document, since refreshed at `7e44b8a0` and `7671196a`.

---

## Gates, 2026-10-17, `7e44b8a0`

The three determinism pull requests merged on top of the version bump (`ece56551`, PR
[#946](https://github.com/spencerduncan/redshipblueship/pull/946)): [#958](https://github.com/spencerduncan/redshipblueship/pull/958) (the goldens pin hint text, prices, starting items and
the entrance table, plus an entrance-shuffle golden; static hints built in a fixed order;
byte-unsigned seed hashing), [#959](https://github.com/spencerduncan/redshipblueship/pull/959) (the creation budget counts fill rounds) and
[#960](https://github.com/spencerduncan/redshipblueship/pull/960) (the paired world's fingerprint and the build beside the seed icons). Like
the runs above, it covers the checks hosted CI cannot run, because they need the ROMs.

**How it was run.** The workstation's main Windows checkout, OpenGL only, windows opened
without taking focus, one tier at a time. Each pull request was gated as a merge of its branch
onto the `main` of that moment, and each merged tree equals its gated tree: #958 at tree
`1fa8507d` (squash commit `865ec10b`), #959 at `fb229270` (`aff54bf7`), #960 at `e524fdfc`,
which is `main`'s tree at `7e44b8a0`.

| Check | #958 (`1fa8507d`) | #959 (`fb229270`) | #960 (`e524fdfc` = `7e44b8a0`) |
|---|---|---|---|
| `redship` tier | 183 of 183 | 183 | 184 of 184 (179 at `851b1ee9`) |
| `rando` tier | 45 of 45 | 48 | 49 of 49 (43 at `851b1ee9`), the golden-world rows among them (five golden files now, with `seed-digest-entrances`) |
| `ui` tier, parity lint | 2 of 2; lint plain and no-grow OK | 2; lint OK | 2 of 2; lint plain and no-grow OK |
| `integration` | 16 of 16 | 16 | 17 of 17 (14 at `851b1ee9`); `IntPairedFileSelectIdentity` is new |
| `integration-soak` | 1 of 1 | 1 | 1 of 1 |

**Creation-time sample, re-run at `7e44b8a0`.** `RSBS_CSB_SAMPLE_EVENT=10 redship --test
combo-single-bag`, host scale 100%: 10 of 10 paired creations succeeded on their first
attempt, each after 258 fill rounds against the 1250-round per-attempt cap. End to end the mean
was 7058 ms, the best 6182 ms and the worst 8550 ms, with no per-attempt budget stop and no
dead heart pickup. This answers the "creation-time sample was not re-run" line of the two runs
above.

**Generated worlds.** No pinned world moved. #958 added its new fields to three golden files and
a fifth golden, `seed-digest-entrances`; no existing golden line changed. A typed seed with
non-ASCII characters makes a different world than before #958 on Windows and x86 Linux (ASCII
and generated seeds are unchanged).

**What this run did not cover.**
- Play by a person. The operator played `7e44b8a0` the same day: see the next entry.
- DirectX 11 was not compared with OpenGL, and F10 does not switch games under DirectX 11
  ([#955](https://github.com/spencerduncan/redshipblueship/issues/955)).
- Networking is untested.

**Findings.** None in this run. The open issues at this commit were listed under "Open issues
at `7e44b8a0`" near the top of this document, since refreshed at `7671196a`.

---

## Playtest, 2026-10-17, `7e44b8a0`

The operator played the release candidate by hand: the `rsbs-windows` artifact of the
`generate-builds` run at `7e44b8a0`, on DirectX 11 (the default for a fresh folder), in a fresh
folder whose `oot.o2r` and `mm.o2r` the first-launch flow had extracted from both ROMs. `main`
was at `66dc8b93`, which adds only documentation to `7e44b8a0`.

**What was played.**
- The first-launch extraction flow, all four scenarios, checked on 2026-10-16 on PR
  [#945](https://github.com/spencerduncan/redshipblueship/pull/945)'s build and recorded on that pull request.
- A new paired file.
- Shops in both games selling crossed items. An ice trap sold under a decoy name worked as
  intended.
- The Happy Mask Shop crossing, both ways.
- Play in Termina.
- The Combo Item Tracker overlay, in both games.

**Findings.** All are filed. None is a crash, and none blocks the tag (the operator's call).
- [#964](https://github.com/spencerduncan/redshipblueship/issues/964): OoT's shop shelves and get-item cutscenes show the "?" stand-in for Majora's Mask
  items whose MM draw is custom (boss remains, stray fairies, the seahorse, the Moon's Tear, a
  bottled fairy). Seen on a Kokiri Shop shelf selling Twinmold's Remains; the purchase worked.
- [#965](https://github.com/spencerduncan/redshipblueship/issues/965): frame interpolation does nothing in Majora's Mask. The shared graphics processor
  interpolates with OoT's recorder while MM records into its own.
- [#966](https://github.com/spencerduncan/redshipblueship/issues/966): an audit of the settings Majora's Mask does not honour in the single executable,
  opened from the operator's "enhancements aren't applying to MM".
- [#962](https://github.com/spencerduncan/redshipblueship/issues/962): the overlay's default position covers the HUD's hearts. Enable Dragging and Window
  Type on Combo > Tracker Settings move it.
- [#963](https://github.com/spencerduncan/redshipblueship/issues/963): the overlay's Majora's Mask section is text until Majora's Mask has started.
- [#967](https://github.com/spencerduncan/redshipblueship/issues/967): shared capacity tiers such as the Bow show per game instead of once in the Shared
  column.
- [#971](https://github.com/spencerduncan/redshipblueship/issues/971): the Shared column reads the pool as of the last save or crossing, not the live game.
- [#968](https://github.com/spencerduncan/redshipblueship/issues/968), [#969](https://github.com/spencerduncan/redshipblueship/issues/969) and [#970](https://github.com/spencerduncan/redshipblueship/issues/970): requests for Shared Magic Arrows, Shared Bunny Hood and Shared
  Bottles rules.

**What this playtest did not cover.** A crossing after quitting and relaunching the game, a
crossing after a save made during play, the cross-game warp songs, "[Both Games] Shared Kokiri
Sword" on, MM's game-over prompt ([#694](https://github.com/spencerduncan/redshipblueship/issues/694)), OoT's icon in MM's get-item textbox ([#607](https://github.com/spencerduncan/redshipblueship/issues/607)),
OpenGL compared with DirectX 11, and networking. On DirectX 11 F10 does not switch games
([#955](https://github.com/spencerduncan/redshipblueship/issues/955)).

---

## Gates, 2026-10-20, `7671196a`

The 21 pull requests merged after PR [#974](https://github.com/spencerduncan/redshipblueship/pull/974) (2026-10-18) up to `7671196a`. Like the runs
above, these gates cover the checks hosted CI cannot run, because they need the ROMs.

**How it was run.** The workstation's main Windows checkout, OpenGL only, the ROM archives
mounted, windows opened without taking focus and, since PR [#1041](https://github.com/spencerduncan/redshipblueship/pull/1041), test processes silent.
Five pull requests were gated alone, each as a merge of its branch onto the `main` of that
moment (the last, [#1058](https://github.com/spencerduncan/redshipblueship/pull/1058), after the batches): [#1000](https://github.com/spencerduncan/redshipblueship/pull/1000), [#1041](https://github.com/spencerduncan/redshipblueship/pull/1041), [#1001](https://github.com/spencerduncan/redshipblueship/pull/1001) and [#1004](https://github.com/spencerduncan/redshipblueship/pull/1004). The rest were gated in batches: each
batch verify-merged several branches onto `main` at once, the pull requests were then
squash-merged one by one, and the merged tree was checked equal to the gated tree. Batch b1
was a verification run only and was not merged as a set. PRs [#986](https://github.com/spencerduncan/redshipblueship/pull/986) and [#989](https://github.com/spencerduncan/redshipblueship/pull/989) (tooling
and documentation) are not in the table.

| Gate | Pull requests | `redship` | `rando` | `ui`, parity lint | `integration` | `integration-soak` |
|---|---|---|---|---|---|---|
| #1000 alone | [#1000](https://github.com/spencerduncan/redshipblueship/pull/1000) (`253b8d49`) | 185 of 185 | 50 of 50 | 2 of 2 | 18 of 18; `IntResourceCacheOwner` is new | 1 of 1 |
| #1041 alone | [#1041](https://github.com/spencerduncan/redshipblueship/pull/1041) (`4453be07`) | 187 | 50 | 2 | 18 | 1 |
| #1001 alone | [#1001](https://github.com/spencerduncan/redshipblueship/pull/1001) (`b9748b5d`) | 187 | 55 (four new goldens and the player-path row) | 2 | 18 | 1 |
| #1004 alone | [#1004](https://github.com/spencerduncan/redshipblueship/pull/1004) (`29e8dba5`) | 188 | 55 | 2 | 18, after a rerun (below) | 1 |
| b2 | [#1049](https://github.com/spencerduncan/redshipblueship/pull/1049), [#1045](https://github.com/spencerduncan/redshipblueship/pull/1045), [#985](https://github.com/spencerduncan/redshipblueship/pull/985), [#1052](https://github.com/spencerduncan/redshipblueship/pull/1052), [#1046](https://github.com/spencerduncan/redshipblueship/pull/1046), [#1042](https://github.com/spencerduncan/redshipblueship/pull/1042), [#1043](https://github.com/spencerduncan/redshipblueship/pull/1043) (up to `0bea298d`) | 195 of 195 | 55 of 55 | 2 of 2; lint plain and no-grow OK | 18 of 18 | 1 of 1 |
| b3 | [#988](https://github.com/spencerduncan/redshipblueship/pull/988), [#1053](https://github.com/spencerduncan/redshipblueship/pull/1053), [#1054](https://github.com/spencerduncan/redshipblueship/pull/1054) (up to `e496f625`) | 196 | 55 | 2 | 19; `IntCrossingRefusedNoOoTArchive` is new | 1 |
| b4 | [#1002](https://github.com/spencerduncan/redshipblueship/pull/1002), [#1048](https://github.com/spencerduncan/redshipblueship/pull/1048), [#1051](https://github.com/spencerduncan/redshipblueship/pull/1051), [#1056](https://github.com/spencerduncan/redshipblueship/pull/1056) (up to `ee35d2c6`) | 200 | 55 | 2 | 19 | 1 |
| #1058 alone | [#1058](https://github.com/spencerduncan/redshipblueship/pull/1058) (`7671196a`) | 201 | 55 | 2 | 19 | 1 |

At `7e44b8a0` the tiers stood at 184, 49, 2, 17 and 1.

**The rerun at #1004.** The first `integration` run failed `IntPairedFirstCrossing`,
`IntPairedFirstCrossingDiverged` and `IntPairedFirstCrossingTwoCycles`: each logged
"[MM] spoiler: REFUSED to write this world's OoT spoiler over './Randomizer/...json': it is
another paired world's spoiler". PR #1004 changed every world's fingerprint, and the build
folder still held spoilers of the same seeds from older runs. With those spoilers deleted the
same built tree passed 18 of 18. The defect is filed as [#1055](https://github.com/spencerduncan/redshipblueship/issues/1055) (see "Randomizer
generation" below): a test-isolation gap, and a trap a player can meet too.

**Generated worlds.** No placement moved. PR #1001 added four golden files (nine in all),
PR #1004 re-pinned only the fingerprint field (`comboSettingsHash`) of the goldens that carry
it, and PR #1049 added two lines (`mmFreeStoneCount`, `mmFreeStoneDigest`) to each
complete-world golden.

**What these gates did not cover.**
- Play by a person: nobody has played `7671196a`. Section N of the playtest guide lists what
  to check by hand.
- DirectX 11: F10 on DirectX 11 ([#955](https://github.com/spencerduncan/redshipblueship/issues/955)) is fixed in the libultraship fork, and no gate runs
  that backend.
- Networking is untested.

**Findings.** [#1055](https://github.com/spencerduncan/redshipblueship/issues/1055), above. The open issues at this commit are listed under "Open issues
at `7671196a`" near the top of this document.

---

## Gates, 2026-10-22, `30b841c1` (`v0.3.0-prealpha`)

The 33 commits after `v0.2.0-prealpha`, pull requests #1073 to #1108, listed in
[`release-notes-next.md`](release-notes-next.md). Like the runs above, these cover the checks hosted CI cannot run,
because they need the ROMs.

**How it was run.** The workstation's main Windows checkout, OpenGL only, the ROM archives mounted, test windows that
take no focus and test processes that play no sound, driven by the pyramid driver. #1073 to #1100 were gated in its
batches: each pull request's `[lock]` commits built alone on `main` with their rows seen red, then one build of the
batch and the `redship`, `rando` and `ui` tiers with the parity lint, then `integration` and `integration-soak`; a
failure was bisected to its branch. #1081 to #1100 reached `main` through the landing PRs #1103 (cycle 13) and #1110
(cycle 16), whose hosted CI rounds were green, and the merged trees were checked equal to the gated ones. #1102 and
#1104 to #1108 were merged once their hosted CI was green. The driver then ran the ROM tiers on `main` itself:

| Cycle | `main` at | `integration` | `integration-soak` |
|---|---|---|---|
| 17 | `db8d38e8` (#1073 to #1100) | 20 of 20 | 1 of 1 |
| 18 | `2a57d1c6` (with #1102, #1104, #1105, #1107, #1108) | 21 of 21; `IntMoonCrashCycle` is new | 1 of 1 |
| 19 | `30b841c1` (with #1106: the code of `v0.3.0-prealpha`) | 21 of 21 | 1 of 1 |

**Generated worlds.** No golden moved: `git diff 1ace3c7b 30b841c1 -- tests/golden/` is empty.

**What these gates did not cover.** Play by a person: nobody has played `30b841c1`; section P of the playtest guide
lists what to try first. The New Cycle moon crash has run only in `IntMoonCrashCycle`. DirectX 11 is not gated, and
networking is untested.

**Findings.** None in these runs. The open issues at this commit were listed under "Open issues at `30b841c1`" near the
top of this document; that list has been refreshed since ("Open issues at `af4d941f`").

## Gates, 2026-10-23, `main` after `v0.3.0-prealpha`

The pull requests merged after the tag, #1111 to #1144, listed under "Next release (draft)" in
[`release-notes-next.md`](release-notes-next.md). Since the replan of 2026-10-22 each was merged once its hosted CI
was green, and the pyramid driver runs the ROM tiers on `main` itself (same workstation, OpenGL, ROM archives
mounted, no focus, no sound):

| Cycle | `main` at | `integration` | `integration-soak` |
|---|---|---|---|
| 20 | `bc93e65e` (with #1111 to #1119) | 16 of 21: five paired-creation rows waited for a log line #1116 renamed (issue #1135) | 1 of 1 |
| 21 | `f416ecee` (with #1122, #1124 and the fix, #1136) | 21 of 21 | 1 of 1 |
| 22 | `fabe7d33` (with #1137, #1138, #1125) | 16 of 21 | 0 of 1 |

**Cycle 22 was red because of a game controller, not the code.** The failing rows were `IntGameplayRoundtrip`,
`IntPairedFirstCrossing`, `IntPairedFirstCrossingDiverged`, `IntPairedFirstCrossingTwoCycles`,
`IntPairedFirstCrossingForeignSpoiler` and `IntGameplayRoundtripSoak`. They stopped on two kinds of message: the
camera stayed put while Link walked ("camera is anchored"), or a warp landed at the wrong entrance (0x0033 instead
of 0x00B1, or 0x0171 instead of 0x01D1 on the return leg). The driver's re-run of those six rows failed five, with
the messages moving between rows. The triage ([#1152](https://github.com/spencerduncan/redshipblueship/issues/1152)) found the operator's
gamepad driving the test windows: libultraship tells SDL to deliver joystick events to a window that has no focus,
and a test window never takes focus, so a moved stick walked Link (the camera assertion failed) and pressed buttons
took doors or loaded a file (warps and boots arrived at the wrong entrance), at a different point each run. The
driver's bisect assumed that the last green tip, `f416ecee`, was still green; it was not under the pad, so the bisect
named the first commit after it, #1137, and opened a revert, PR [#1150](https://github.com/spencerduncan/redshipblueship/pull/1150). The revert
was closed unmerged and #1137 cleared: with the pad idle the same rows passed at `e3de3ff1` (cycle 23, in the next
section: this code plus #1144's curated icons), and since PR [#1153](https://github.com/spencerduncan/redshipblueship/pull/1153) a test process ignores game controllers altogether.
The advice this section first gave, to read an unexpected crossing landing or a camera that stops following Link as
this finding, is withdrawn: on a build of your own, either is a finding to report.

**Generated worlds.** With every new option at its default no world moved: of the golden digests only
`mmPlacementHash` moved, in eight files, because it counts Majora's Mask's 53 new tree checks whether or not they
are shuffled (#1125); `git diff f2f3d64f 4fc4fd41 -- tests/golden/` is that field plus the two `mmReachable*`
fields #1114 retired.

## Gates, 2026-10-23, `af4d941f`

The pull requests merged after cycle 22: the curated tracker icons of PR [#1144](https://github.com/spencerduncan/redshipblueship/pull/1144) and the docs of PR
[#1148](https://github.com/spencerduncan/redshipblueship/pull/1148), then the Combo Tracker's
reachability (PR [#1147](https://github.com/spencerduncan/redshipblueship/pull/1147)), Majora's Mask's rows in the Combo Item Tracker (PR
[#1149](https://github.com/spencerduncan/redshipblueship/pull/1149)) and test processes that ignore game controllers (PR
[#1153](https://github.com/spencerduncan/redshipblueship/pull/1153)). Same workstation and driver as above (OpenGL, ROM archives mounted, no focus,
no sound), on `main` itself:

| Cycle | `main` at | `integration` | `integration-soak` |
|---|---|---|---|
| 23 | `e3de3ff1` (with #1144, #1148; the pad idle) | 21 of 21 | 1 of 1 |
| 24 | `af4d941f` (with #1147, #1149, #1153) | 21 of 21 | 1 of 1 |

Cycle 23 ran the code that was red in cycle 22, plus #1144's curated icons and #1148's docs, and passed every row,
which cleared #1137 (see the section above).
From cycle 24 on, a controller connected to the workstation cannot reach a test process (#1153).

**Generated worlds.** No golden moved: `git diff 4fc4fd41 af4d941f -- tests/golden/` is empty.

**What these gates did not cover.** Play by a person: nobody has played `af4d941f`; section R of the playtest guide
lists what to try. The reachability capture at a real departure and the Combo Item Tracker's new rows in a running
Majora's Mask were exercised by test rows only. DirectX 11 is not gated, and networking is untested.

**Findings.** None in these runs. The open issues at this commit are listed under "Open issues at `af4d941f`" near
the top of this document.

---

## Save loss and corruption

### The `v0.3.0-prealpha` save break: a paired file from any build before `fabe7d33` (`v0.3.0-prealpha` included) is refused (PR [#1125](https://github.com/spencerduncan/redshipblueship/pull/1125), [#1121](https://github.com/spencerduncan/redshipblueship/issues/1121))

**What you see.** On a build from `fabe7d33` (2026-10-23) on, pressing A on a paired randomizer file saved by any build before `fabe7d33` (`v0.3.0-prealpha`, every earlier build, and the `main` builds after the tag that predate PR #1125, with #1111 to #1138) plays the error sound, shows "Refused: This file has no Majora's Mask world" and keeps you on the file list. The file is not opened and nothing is written to it.

**Why.** Shuffle Tree Drops (PR #1125) added 53 checks to Majora's Mask's check table, which sits in the middle of Majora's Mask's half of the save, so every field after it moved, among them the seed by which the file select recognises the pair's Majora's Mask world, and an older file's half now reads as another world. The mask groundwork of PR [#1112](https://github.com/spencerduncan/redshipblueship/pull/1112) appended three more bytes to the same half. The operator accepted this break on 2026-10-23 rather than a migration, as with the earlier save-layout changes of this pre-release.

**What to do.** Create a new paired file on the new build. To finish an old file, keep playing it on `v0.3.0-prealpha`, the build that wrote it; a file saved by a `main` build after the tag and before `fabe7d33` can be finished only on that same build. There is no migration. A file that is not a randomizer file is not refused, but the same shift moves the Majora's Mask fields stored after the save proper; nobody has checked what that does, so start a new file there too.

### A file saved from `efe573ec` on does not open in `v0.3.0-prealpha` (PR [#1147](https://github.com/spencerduncan/redshipblueship/pull/1147))

**What you see.** `v0.3.0-prealpha` refuses a file saved by a build from `efe573ec` (2026-10-23) on as a file made by another build. A file saved by `v0.3.0-prealpha` that the newer build opens at all (the save break above refuses paired ones) loads normally, and its Combo Tracker shows no reachability for the game you are not playing until your next crossing.

**Why.** The Combo Tracker keeps what was reachable in the game you left in the save's combo record. That did not fit the record's old size, so the record grew from 1024 to 2048 bytes and the save format went from version 3 to version 4, as the format's growth rule requires. A build reads only the versions it knows, so `v0.3.0-prealpha` (version 3) refuses version 4; the newer build reads an older file's shorter record as "no reachability captured".

**What to do.** Nothing, unless you go back to `v0.3.0-prealpha`: finish a file on the build family that wrote it, and back up `Save/` before you switch builds either way.

### ~~A changed MM option or Cross-Game Rule breaks the pair for the session~~ — RESOLVED ([#781](https://github.com/spencerduncan/redshipblueship/issues/781); [#564](https://github.com/spencerduncan/redshipblueship/issues/564))

Fixed by the #781 change: **the file's own rules win at load.** The pages are still
editable between sessions, and what they hold there is staging for the next file.
Loading a paired file now compares both its Cross-Game Rules and its Majora's Mask
profile (the same digest the crossing checks) and puts the file's own values back into
the pages:
- a changed Cross-Game Rule (Goal, Crossing Direction, Shared Kokiri Sword; Shared Ocarina until #879 retired it) is
  restored, and a toast reads "Restored from file:" with the rows it reset
  (for example "Restored from file: Goal, Crossing Direction");
- a changed MM option or trick is restored from the file's MM half, and a toast reads
  "Restored for Majora's Mask:" with the rows it reset (for example "Restored for
  Majora's Mask: Starting Hearts +1"; a long trick name is cut with "...", and rows
  that do not fit are counted).

The file loads paired, the slot stays writable, and the next crossing into Majora's Mask
agrees with the file. The cases the file cannot answer stay visible instead of silent:
- ~~an MM identity input the file does not record (the excluded-check list or the
  starting-item block, which no page in this build edits) loads the file paired but
  posts "Not restored: Majora's Mask options differ", and the crossing is refused until
  they match;~~ **RESOLVED (#836):** such a file is not opened ("Refused: Majora's
  Mask options differ" at the file select);
- a Cross-Game record field no page authors (only a file from another build can differ
  there) is refused with "Refused: File made by another build" (a damaged record:
  "Refused: Cross-game record is damaged"). The field names are on stderr. ~~The OoT
  file still opens and plays without its Majora's Mask half, because the load runs after
  OoT has opened the file; the next crossing into Majora's Mask says so ("Not paired:
  Termina stays un-randomized") instead of skipping pairing silently. Nothing is saved
  to the pair that session.~~ **RESOLVED (#836, operator ruling 2026-10-01: "unpaired is
  not an acceptable scenario"):** a refused file is not opened. Pressing A on it in the
  Ocarina of Time file select plays the error sound, posts the refusal toast, and keeps
  you on the file list; Combo > Save Files shows the same words. The same applies to a
  randomizer file whose cross-game record is missing ("Cross-game record is missing":
  a deleted record, or a file made with the file select's Copy, which copies Ocarina of
  Time's file only) and to every damaged record. Nothing is written, renamed or deleted.
  Two ways out: put a good copy of `Save/redship_slot<N>.redsave` back (N = file
  number - 1) and open the file again, or erase the file in the file select. A
  `.refused-*.bak` left by an earlier build is worth renaming back only when its
  refusal was not damage; "File made by another build" opens in that build.

The original report, kept only for matching old logs (historical; it describes the
behaviour before the fix):

> Open, and a trap between sessions. Combo → MM Randomizer and Combo → MM Tricks lock
> only while a creation stamp is resident (`Combo_MMProfileFrozen()` is
> `mmProfileDigest != 0`), and that stamp is zero on a fresh launch and after a return to the
> title screen, so both pages are editable exactly between sessions. Loading a paired file
> compares its Cross-Game Rules field by field against the live ones, but it does **not**
> recompute the MM profile: an edited MM option, trick, excluded
> check or starting item is accepted at load. The next crossing into Majora's Mask recomputes
> the profile, sees the difference and refuses: a 15-second "Cross-game pairing REFUSED"
> toast, an un-randomized Termina, and the slot latched against writes for the session
> (PR [#570](https://github.com/spencerduncan/redshipblueship/pull/570)). The file on disk is untouched.
>
> Cross-Game Rules (including the Goal) unlock between sessions the same way
> (`Combo_ComboSettingsFrozen()` is `comboSettings.formatVersion != 0`). A changed rule *is*
> caught at load, but quietly: `LoadSlot` prints the diverged fields to stderr only and
> latches the slot, and OoT's caller ignores the result, so the OoT file opens and plays
> with nothing saved to the pair. By code reading (not run), the pairing identity is not
> restored either, so the next MM arrival skips pairing and plays an un-randomized Termina
> without a toast. No in-game surface shows the refusal (the `.redsave` file panel lives in
> the never-instantiated `ComboMenuBar`). (2026-09-28: `ComboMenuBar` is deleted; Combo >
> Save Files shows a refused file as "Not paired:" with its reason.)
>
> **Workaround:** once a paired file exists, leave both MM pages and Cross-Game Rules alone;
> if you changed one, set it back exactly or recreate the file. **The fix to come** is the
> same MM-profile compare at load time, plus a load refusal the player can see; neither
> exists yet.

### ~~A flag set in the scene you leave through the portal can be lost~~ — RESOLVED ([#635](https://github.com/spencerduncan/redshipblueship/issues/635), community report; tracked in [#638](https://github.com/spencerduncan/redshipblueship/issues/638), PR [#650](https://github.com/spencerduncan/redshipblueship/pull/650))

Fixed by PR #650 (2026-09-11). Live scene flags are now flushed before every departure freeze, in
both games and on both switch paths. The original report, kept for matching old saves:

Collect the Heart Piece on the Clock Tower, walk straight out through the portal to
Ocarina of Time, come back: the Heart Piece is there again, and can be collected
again. The cross-game departure freezes MM's save **before** the scene-flag flush
that a normal scene transition performs, and that flush never runs — so anything
you picked up or triggered in the scene you left through the portal (collectibles,
chests, switches, in that scene only) is not in the frozen save. Progress in every
*other* scene is safe.

On builds older than PR #650, the workaround was to leave and re-enter the scene through any door
or loading zone before crossing.

### ~~F10 during MM's game-over screen hands over an empty health bar~~ — RESOLVED ([#626](https://github.com/spencerduncan/redshipblueship/issues/626), PR [#650](https://github.com/spencerduncan/redshipblueship/pull/650))

Fixed by PR #650 (2026-09-11). A dead MM health bar is now revived before every departure freeze.
The original report:

Health is a shared resource between the two games, and it is applied as-is on
arrival. Pressing the F10 debug hot-swap while MM's game-over prompt is up freezes
the save with zero health and switches; you arrive in OoT with an empty bar. The
normal game-over exit (choosing not to continue) already revives you on the way out
(PR [#625](https://github.com/spencerduncan/redshipblueship/pull/625)); the F10
route bypasses it.

On builds older than PR #650, the workaround was not to press F10 on the game-over screen.

### ~~F10 during OoT's game-over screen hands MM a one-heart bar~~ — RESOLVED ([#664](https://github.com/spencerduncan/redshipblueship/issues/664), PR [#753](https://github.com/spencerduncan/redshipblueship/pull/753))

Fixed by PR #753 (2026-09-27), the OoT twin of #650's revive. A dead OoT bar is revived before
the freeze to OoT's own continue value: three hearts, or full capacity with "Spawn with Full
Health". If a bottled fairy was spent at the killing blow, both games give the fairy's refill
instead (OoT 20 hearts, MM 10, clamped to capacity). Before the fix MM arrived with one heart,
its arrival floor. A pre-release OoT half already frozen dead stays dead in its blob, and OoT's
arrival floors it at one heart.

### ~~MM's paired half could be wiped by the moon crash or an owl save~~ — RESOLVED ([#765](https://github.com/spencerduncan/redshipblueship/issues/765), PR [#772](https://github.com/spencerduncan/redshipblueship/pull/772))

Fixed by PR #772 (2026-09-28). The creation event left MM's half with slot number 0, a real
flash slot, so the moon-crash reset and the owl save's readback copied an empty save buffer
over the live one (observed in a headless row: day, rupees and inventory wiped). The half is
now authored as MM's own new-file path authors it, and every arrival pins the cross-game slot
number, which also covers older files (see "Back up your saves"). The fix is locked
headlessly and has not been played; the playtest guide's scenario I4 checks it in game.
It fixed the wipe only; the half then stayed on day 4 after a crash, fixed separately
(next entry). To see whether a file's MM half exists, open the Combo Tracker's Majora's
Mask panel: MM's world under its header, versus nothing under the header (its notes were
cut by PR [#929](https://github.com/spencerduncan/redshipblueship/pull/929)).

### ~~A moon crash in a paired MM half leaves the cycle on day 4~~ — RESOLVED ([#785](https://github.com/spencerduncan/redshipblueship/issues/785), PR [#789](https://github.com/spencerduncan/redshipblueship/pull/789))

`Interface_StartMoonCrash` sets day 4 and 06:00 before the crash cutscene, and vanilla
rolls the cycle back when `Sram_ResetSaveFromMoonCrash` reloads the file from flash. A
cross-game session has no flash slot, so that reload was skipped and the half stayed on
day 4 (Final Hours clock, no Dawn of the First Day, no second crash). Now the reload is the
file's **last save**: the last whole `.redsave` commit (an owl save, the autosave, the Song
of Time, a save in OoT, your last crossing between the games, or the file's creation), with
both halves and the cross-game records
restored together. This is what vanilla's reload does and what OoTMM's default
("Last Save") moon crash does. What to expect:

- MM progress since that save is lost, as in vanilla. After an owl save or autosave the
  clock resumes at that save's day and time; the Dawn of the First Day plays only when the
  last save was a Song of Time or the file's creation (a file never saved restarts as
  created). A save taken within about ten in-game minutes of the crash is pulled back to
  05:49 on the final day, OoTMM's grace period, so the moon cannot fall again at once.
- **A crash no longer rolls OoT back** ([#837](https://github.com/spencerduncan/redshipblueship/issues/837)).
  Every crossing into Termina is itself a save of the whole file, so OoT's half in the
  commit the crash restores is OoT as you left it. Without a save in Termina since you
  entered, MM comes back as you last left Termina (or as created, if you never left it).
- **A chest you opened in Termina before you left stays open** ([#837](https://github.com/spencerduncan/redshipblueship/issues/837)).
  Leaving Termina keeps the scene flags (chests, switches, collected items, cleared rooms)
  the way an owl save does, so a crash that restores that crossing's save does not close
  a chest whose item you kept. Before this fix such a chest came back closed and opening it
  again gave its item a second time.
- Shared rupees, health, magic and ammo come back at the shared pool's value as of that
  save, applied to MM as an arrival applies them. They do not come back at MM's own
  balance from its last departure. After a save in OoT these two differ, and without the
  apply MM would have refunded what OoT spent (fixed in the same PR; row
  `mm-moon-crash-pool-applied`).
- A cross-game item that reached MM after the save is not lost: it is delivered again at
  your next arrival in MM.
- Where you wake (traced in source, not played): the crash cutscene ends in the Clock
  Tower interior with the Happy Mask Salesman's scene (the same destination the Skip Moon
  Crash enhancement sets, `ENTRANCE(CLOCK_TOWER_INTERIOR, 3)`), then you walk out into
  South Clock Town. The Dawn of the First Day card needs the restored clock at day 0
  before 06:01, so it does not appear after an owl save or autosave.
- A session with nothing saved at all (a refused slot, a debug boot) has nothing to reload;
  there the clock alone restarts at dawn. Nothing else about the cycle is reset on that
  path: cycle events (`weekEventReg`) and the rest of the half keep their pre-crash
  values, apart from what vanilla's own tail clears (event flags, cycle scene flags from
  the permanent ones, timers).
- **Decision 16, ruled 2026-10-01 (option (b)):** every cross-game crossing is a save
  commit, as OoTMM's `comboGameSwitch` saves at every game switch. This build commits the
  whole file at every crossing, door or F10, in both directions (#837), so the last save
  never predates the crossing and a crash cannot take back unsaved OoT play. A crash
  therefore usually resumes mid-cycle, from the crossing into Termina; OoTMM does the same.
  Resuming mid-cycle after an owl save or autosave (vanilla deletes the owl save and
  reloads the last Song of Time save) follows OoTMM's default too.

Locked headlessly (`mm-creation-new-file`, `mm-creation-new-file-world`,
`mm-moon-crash-never-saved`, `mm-moon-crash-pool-applied`); not played. The playtest guide's scenario I4 checks it in game.

### ~~F10 hot-swap silently rolls back your progress~~ — RESOLVED ([#364](https://github.com/spencerduncan/redshipblueship/issues/364), PR [#400](https://github.com/spencerduncan/redshipblueship/pull/400))

F10 now freezes the departing game's state and sets a return entrance, the same
as the portal switch, and the frozen state is cleared when consumed. The historical
symptom — resuming from the *other* game's save bytes after an F10 switch — is
gone. The game-over case above is fixed too.

### ~~A malformed save permanently deadlocks all saving~~ — RESOLVED ([#370](https://github.com/spencerduncan/redshipblueship/issues/370), PR [#391](https://github.com/spencerduncan/redshipblueship/pull/391))

`saveMtx` is released on the exception path and the file is no longer truncated
before the write that could throw.

### ~~Cross-game save format will change~~ — it did; see "Back up your saves"

The `reserved` padding, the version window and the refused state that entry asked
for all exist now. The advice stands: the format may change again.

---

## Crashes and hangs

### ~~Majora's Mask's shooting gallery, the Southern Swamp route and the staff roll use Ocarina of Time's resources~~ — RESOLVED ([#981](https://github.com/spencerduncan/redshipblueship/issues/981), PR [#1000](https://github.com/spencerduncan/redshipblueship/pull/1000))

Fixed by PR #1000 (2026-10-19). Both games' archives carry some of the same resource paths
(the Octorok's skeleton `objects/object_okuta/gOctorokSkel`, the treasure chest, the get-item
shelf models, the staff roll's text), and the resource cache was keyed by path alone, so the
game that loaded such a path first supplied it to the other for the rest of the session. After
an Octorok in Ocarina of Time (Lake Hylia, for example), Majora's Mask was handed OoT's 37-limb
skeleton for its 15-limb Octoroks, and every frame wrote past the end of their joint tables:
the Clock Town shooting gallery crashed, and so could the Southern Swamp route's Octoroks. MM's
staff roll read OoT's credits table. Now each game loads under its own cache owner, set at every
switch before the arriving game's archives are re-added: a shared path is parsed once per game,
and a game coming back gets its own earlier copies, so nothing is freed or reloaded at a switch.
The libultraship fork gained the owner setter (its PR #9). Locked by `ResourceCacheOwner`
(redship tier) and `IntResourceCacheOwner` (integration tier); not played. To check it by hand
(the playtest guide's N1): in a session started in Ocarina of Time, fight an Octorok at Lake
Hylia, cross through the Happy Mask Shop door, play a round at the Clock Town shooting gallery
(the Octoroks show, and leaving does not crash), visit the Southern Swamp's Octoroks, and if you
can, watch Majora's Mask's staff roll (its text reads correctly). Back in Hyrule the Lake Hylia
Octoroks, chests and shop shelves look as before. The follow-ups, one of which a player can
meet (the Cosmetics Editor used while Majora's Mask runs), are [#1040](https://github.com/spencerduncan/redshipblueship/issues/1040). A sibling, every
switch opening one more copy of every re-added archive ([#987](https://github.com/spencerduncan/redshipblueship/issues/987)), is fixed by PR [#1045](https://github.com/spencerduncan/redshipblueship/pull/1045).

### ~~Opening Network → Anchor in a wide window displaces the game view~~ — RESOLVED ([#634](https://github.com/spencerduncan/redshipblueship/issues/634), community report; tracked in [#640](https://github.com/spencerduncan/redshipblueship/issues/640), PR [#651](https://github.com/spencerduncan/redshipblueship/pull/651))

Fixed by PR #651 (2026-09-11). `soh_port` now links whole-archive, so the page's registrar is kept,
and a menu page with no widgets consumes its own window position. The original report:

On Windows, open the menu, go to `Network → Anchor` while the window is wider than
roughly 800 px (maximized, for example): the page's content column is empty, and
when you close the menu the game view is pushed off to a black rectangle. The page
has no widgets because its menu registrar is dropped by the linker from the
`soh_port` archive, and the empty page still positions the main game window. Not
specific to fullscreen or DirectX 11.

On builds older than PR #651, the workaround was to narrow the window first or avoid the page.

### ~~Every normal exit heap-corrupts on Windows (Fault A)~~ — RESOLVED ([#396](https://github.com/spencerduncan/redshipblueship/issues/396))

**Fixed and operator-confirmed 2026-07-21.** `redship --version` now exits cleanly
with status `0`; the heap corruption on normal exit is gone.

Historical detail, for anyone reading old crash reports: Windows builds used to die
with `STATUS_HEAP_CORRUPTION` (`0xC0000374`, detected inside `ntdll`) during **normal
process exit**, the minimal repro being `redship --version`. The root cause was not
the `/FORCE:MULTIPLE` CRT duplication originally suspected: `OTRExporter/Main.cpp` and
`VersionInfo.cpp` were compiled into **both** the `OTRExporter_OoT` and `OTRExporter_MM`
static libs, so seven-plus global objects were constructed twice and destroyed twice —
the second destructor walking freed heap. The fix (PR
[#413](https://github.com/spencerduncan/redshipblueship/pull/413), submodule pointer
bump) namespaces the exporter globals per variant. A permanent strong-DATA-symbol CI
gate over the two exporter archives (PR
[#430](https://github.com/spencerduncan/redshipblueship/pull/430)) prevents the class
from recurring.

If you ever see `0xC0000374` on exit again, it is a **new** regression, not this one —
start from the exporter-archive symbol gate.

### ~~Crashes in CI/headless mode present as a 180-second hang~~ — RESOLVED ([#388](https://github.com/spencerduncan/redshipblueship/issues/388), PR [#394](https://github.com/spencerduncan/redshipblueship/pull/394))

The crash handler no longer blocks in `SDL_ShowSimpleMessageBox` without a display;
headless crashes fail fast with an exit code.

### ~~Audio subsystem can wedge across a switch~~ — RESOLVED ([#365](https://github.com/spencerduncan/redshipblueship/issues/365) PR [#416](https://github.com/spencerduncan/redshipblueship/pull/416); [#371](https://github.com/spencerduncan/redshipblueship/issues/371) / [#378](https://github.com/spencerduncan/redshipblueship/issues/378) PR [#398](https://github.com/spencerduncan/redshipblueship/pull/398); [#377](https://github.com/spencerduncan/redshipblueship/issues/377) PR [#412](https://github.com/spencerduncan/redshipblueship/pull/412))

OoT's audio buffer fill is guarded on its initialized flag, the sequence maps are
zero-initialized and bounds-checked before registration, and the audio probe observes
the reset handshake instead of bypassing it. If you still get silence or a hang after
a switch with custom music installed, it is a new report.

---

## Majora's Mask specific

MM is the newer half of the combo and still carries more debt than OoT.

### Some shared settings do less, or nothing, in Majora's Mask

Since the merges after `v0.2.0-prealpha` every row the #966 audit filed works in Majora's Mask except these:

- **Ocarina of Time only:** D-pad in text boxes, D-pad hold change, D-pad on
  the pause screen and Customize Ocarina Controls (Majora's Mask has no code for them), Skip Song Cutscenes, Quick
  Boss Deaths and Exclude Glitch-Aiding Cutscenes (no Majora's Mask twin), and Skip To File Select (the title
  screen is Ocarina of Time's). ~~Damage Multiplier~~ has a Majora's Mask row of its own since PR [#1119](https://github.com/spencerduncan/redshipblueship/pull/1119)
  ("MM Damage Multiplier", Enhancements > Difficulty, under Ocarina of Time's; the lists differ: Majora's Mask's
  goes to 16x or 1 Hit KO, Ocarina of Time's to 256x), and ~~Skip Boss Introductions~~ (Majora's Mask's Skip Enemy
  Cutscenes) works in Majora's Mask since PR [#1124](https://github.com/spencerduncan/redshipblueship/pull/1124).
- **Cutscene skips that follow another row in Majora's Mask** (PR [#1124](https://github.com/spencerduncan/redshipblueship/pull/1124)): its song-teacher cutscenes follow
  Skip Story Cutscenes, its boss blue-warp cutscenes follow Skip Boss Introductions, and Tatl's long flashback at
  the tree in southern Termina Field, on the road to the swamp, follows Skip Misc Interactions, as in
  2Ship2Harkinian. Skip First Cycle has no row: the crossing already starts Majora's Mask past its first cycle.
- **Cutscene-skip defaults differ** (PR [#1124](https://github.com/spencerduncan/redshipblueship/pull/1124)): in a randomizer file whose config never touched the
  cutscene-skip checkboxes, Ocarina of Time skips story, entrance and boss-intro cutscenes by default but Majora's
  Mask plays them. Tick the boxes once to apply them to both.
- **Fast Chests** does nothing in a Majora's Mask randomizer file: the chest contents belong to the randomizer, as in
  2Ship2Harkinian (PR [#1086](https://github.com/spencerduncan/redshipblueship/pull/1086)).
- **Permanent Heart Loss** turned off restores lost heart containers in Ocarina of Time only (PR [#1099](https://github.com/spencerduncan/redshipblueship/pull/1099)).
- **Free Look** in Majora's Mask ignores the remote bombchu's camera focus (PR [#1089](https://github.com/spencerduncan/redshipblueship/pull/1089)).
- **Skip Get Item Animations** (Randomizer > General) now decides Majora's Mask's get-item scenes too; Majora's Mask's
  own Skip Item Get Cutscene key is no longer read, and a value set by hand in `shipofharkinian.json` is ignored
  (PR [#1100](https://github.com/spencerduncan/redshipblueship/pull/1100)).

### Dying in MM respawns you at the entrance instead of showing a game-over prompt — this is intentional

MM's vanilla death behaviour (reload at the area entrance with three hearts, no
"Continue?" prompt) is the combo's shipped default; it is not a regression. The
2ship game-over prompt enhancement (`gEnhancements.Kaleido.GameOver`) is off by default.
Since PR [#695](https://github.com/spencerduncan/redshipblueship/pull/695) ([#682](https://github.com/spencerduncan/redshipblueship/issues/682)) you can turn it on from Combo → Majora's Mask.
ADR 0009's death-decline autosave machinery and #626's F10-during-game-over case
only apply once that enhancement is enabled — operator ruling, 2026-09-16
([#653](https://github.com/spencerduncan/redshipblueship/issues/653)).

### ~~First arrival in Clock Town reads 08:00 instead of 06:00~~ — RESOLVED ([#636](https://github.com/spencerduncan/redshipblueship/issues/636), community report; tracked in [#639](https://github.com/spencerduncan/redshipblueship/issues/639), PR [#648](https://github.com/spencerduncan/redshipblueship/pull/648))

Fixed by PR #648 (2026-09-11). A first MM arrival now re-authors the new-file clock, so the dawn
sequence runs. The original report:

The first crossing from OoT into a new MM file lands in South Clock Town at Day 1,
8:00 AM, with no dawn sequence, instead of a new file's 6:00 AM. No time value
leaks from OoT: the 8:00 is MM's own title-screen attract-demo clock, which the
first-entry path never re-authors. You lose two hours of the first day and the
dawn telop; nothing else is wrong with the clock.

### ~~MM enhancements do not initialize in single-executable builds~~ — RESOLVED ([#384](https://github.com/spencerduncan/redshipblueship/issues/384) PR [#408](https://github.com/spencerduncan/redshipblueship/pull/408); [#516](https://github.com/spencerduncan/redshipblueship/issues/516) PRs [#518](https://github.com/spencerduncan/redshipblueship/pull/518), [#520](https://github.com/spencerduncan/redshipblueship/pull/520), [#616](https://github.com/spencerduncan/redshipblueship/pull/616))

The colliding registrar names were prefixed `MM_`, and the registrars that MM's
excluded boot sequence used to run are now called explicitly from the single-exe
boot path, with a link-time audit (`.github/scripts/check-registrar-elision.sh`)
and a runtime lock over the list. MM's enhancement archive still links as a plain
archive by design, so an MM toggle that appears enabled and does nothing is still a
plausible bug — report it.

### ~~MM code binds to OoT implementations in several places~~ — RESOLVED

Actor culling ([#382](https://github.com/spencerduncan/redshipblueship/issues/382),
PR [#402](https://github.com/spencerduncan/redshipblueship/pull/402)), framebuffer
effects ([#386](https://github.com/spencerduncan/redshipblueship/issues/386), PR
[#436](https://github.com/spencerduncan/redshipblueship/pull/436)), the C++ layout
mismatches ([#383](https://github.com/spencerduncan/redshipblueship/issues/383), PR
[#434](https://github.com/spencerduncan/redshipblueship/pull/434)) and the stub
signature drift ([#372](https://github.com/spencerduncan/redshipblueship/issues/372)
PR [#415](https://github.com/spencerduncan/redshipblueship/pull/415);
[#385](https://github.com/spencerduncan/redshipblueship/issues/385) PR
[#401](https://github.com/spencerduncan/redshipblueship/pull/401)) each got MM its own
definition. A symbol-collision baseline and a strong-symbol gate in CI keep the class
from returning.

### ~~Timers are not neutralized on the MM side of a switch~~ — RESOLVED ([#373](https://github.com/spencerduncan/redshipblueship/issues/373), PR [#419](https://github.com/spencerduncan/redshipblueship/pull/419))

### ~~MM text that says the player's name shows blanks in a paired world~~ — RESOLVED ([#773](https://github.com/spencerduncan/redshipblueship/issues/773), PR [#782](https://github.com/spencerduncan/redshipblueship/pull/782))

The paired creation now translates the name typed on OoT's file select into Majora's Mask's
character set (OoTMM's mapping; characters MM cannot draw become a space) and stamps it on MM's
half, so an MM textbox that names Link prints the OoT name. Files created before the fix keep
the all-space name. Not yet looked at in play.

### ~~MM hook dispatch is still partial~~ — RESOLVED ([#438](https://github.com/spencerduncan/redshipblueship/issues/438), PR [#673](https://github.com/spencerduncan/redshipblueship/pull/673))

Fixed by PR #673 (2026-09-17), which wired the last fourteen dormant MM hook types and
the guards their registrants needed. Before it, those hook types had no dispatch point
in the single-exe build, and options that depended on them were shown disabled-with-reason.
The actor-init, actor-draw and open-text hooks (PR
[#512](https://github.com/spencerduncan/redshipblueship/pull/512)), the pause-menu
and file-select hooks (PR [#547](https://github.com/spencerduncan/redshipblueship/pull/547))
and the item/progression trio (PR
[#630](https://github.com/spencerduncan/redshipblueship/pull/630)) now dispatch.

---

## Switching and entrances

### ~~On DirectX 11, F10 does not switch games~~ — RESOLVED, pending a hand test ([#955](https://github.com/spencerduncan/redshipblueship/issues/955))

Windows delivers F10 as a system key (`WM_SYSKEYDOWN`). The DirectX 11 window handled only
ordinary key messages, so F10 went to Windows, which opened its menu loop: the game seemed to
pause until another key was pressed, and no switch happened. DirectX 11 is the default backend
on a fresh Windows install. The libultraship fork's PR #11 (merge commit `2f612109`) now hands
F10 to the game and returns without Windows' default handling; other system keys keep it, so
Alt+F4 still closes the game. The OpenGL window always delivered F10. The fix reached `main` on
2026-10-20 through PR [#1045](https://github.com/spencerduncan/redshipblueship/pull/1045)'s pin (`21b8fa4f`). No gate runs DirectX 11 and nobody has
pressed F10 on it yet: the playtest guide's N2 checks it. Until then, the doors and the OpenGL
backend are the sure ways across.

### ~~MM's clock-shuffle randomizer option was unreachable~~ — RESOLVED ([#678](https://github.com/spencerduncan/redshipblueship/issues/678), PR [#679](https://github.com/spencerduncan/redshipblueship/pull/679))

`RO_CLOCK_SHUFFLE` is now live and reachable on Randomizer → MM Randomizer. The two
`2ship_enh` translation units it depends on (`BetterSongOfDoubleTime.cpp`,
`SkipSoTCutscenes.cpp`) were dropped by plain archive linking because they
register only through a file-scope `RegisterShipInitFunc`; a targeted
`WHOLE_ARCHIVE` carve-out links and guards both, and `check-registrar-elision.sh`
now locks the registrars in. This is not a general `2ship_enh` link change
([#427](https://github.com/spencerduncan/redshipblueship/issues/427) item 3
stays a separate decision).

### ~~Test and default entrance links collide~~ — RESOLVED ([#374](https://github.com/spencerduncan/redshipblueship/issues/374), PR [#397](https://github.com/spencerduncan/redshipblueship/pull/397))

Duplicate entrance-link registrations are rejected instead of silently shadowing.

### ~~An entrance bound is an unchecked literal~~ — RESOLVED ([#380](https://github.com/spencerduncan/redshipblueship/issues/380), PR [#417](https://github.com/spencerduncan/redshipblueship/pull/417))

### ~~Leaving a game mid-session leaves its actors' state behind~~ — RESOLVED ([#666](https://github.com/spencerduncan/redshipblueship/issues/666) PR [#751](https://github.com/spencerduncan/redshipblueship/pull/751); [#750](https://github.com/spencerduncan/redshipblueship/issues/750) PR [#767](https://github.com/spencerduncan/redshipblueship/pull/767))

A departure through the portal or with F10 retires the game's play state without running each
live actor's teardown. Both games now retire that session at departure: every overlay that
still had actors is reset once, and per-actor extension data (such as a rando pot's check
identity) is dropped. Symptoms this fixes, from reading the code: MM's three-day clock actor
killing itself on the next arrival (no night, dawn or moon crash until a scene change), a
skipped "Dawn of the First Day" on a second file, and Romani Ranch's alien defense pointing at
actors from the discarded session; in OoT, a missing Zora diving-game Zora, Spirit and Forest Temple lifts and water, and the Treasure
Chest Shop keeper for the rest of the process. Each departure logs `[OoT] Abandoned session
retired: ...` or `[MM] ...`.

### ~~OoT actor teardown writes to the save are skipped at a departure~~ — RESOLVED ([#770](https://github.com/spencerduncan/redshipblueship/issues/770), PR [#792](https://github.com/spencerduncan/redshipblueship/pull/792); [#807](https://github.com/spencerduncan/redshipblueship/issues/807))

The teardowns PR #767 does not run also write the save. Fixed by PR #792 (2026-09-28): right
before a departure freezes OoT's save, the windmill's Song of Storms flag is cleared and Lake
Hylia's raised water is set again after the Water Temple (rando), as those actors' teardowns do
on any exit, and the lake's river water box is put back. A running room or minigame timer's
`timerState`, Sun's Song state and a magic effect's `magicState` are reset on the next OoT
arrival or file load (read in code, #770). ~~A sun switch lit by a Light Arrow under SoH's
Sunlight Arrows enhancement stays on after a departure, and an F10 between an age change and
its scene reload freezes the old `linkAge`~~ — RESOLVED (#807): the departure now turns that
sun switch off and applies the pending age with its equipment swap, as those teardowns do. State cleared only by destroy
hooks is not retired either: a remote bombchu's camera focus and the entrance-randomizer Epona
state (PRs [#751](https://github.com/spencerduncan/redshipblueship/pull/751), [#767](https://github.com/spencerduncan/redshipblueship/pull/767)).

---

## Randomizer generation

### Seed packages from `main` builds before `efe573ec` are refused ([#1151](https://github.com/spencerduncan/redshipblueship/issues/1151), PR [#1147](https://github.com/spencerduncan/redshipblueship/pull/1147))

**What you see.** On a build from `efe573ec` (2026-10-23) on, Randomizer > General > Seed Package > Load seed package on a package exported by a `main` build from `bc93e65e` (PR [#1116](https://github.com/spencerduncan/redshipblueship/pull/1116), which added seed packages) to `e3de3ff1` shows "Refused: Seed package damaged or incomplete" and stages nothing. `v0.3.0-prealpha` and every earlier release wrote no seed packages, so no released build's package is affected. The log line reads `REFUSED (layout)` and names one of two causes:

- a package from `fabe7d33`, `4fc4fd41` or `e3de3ff1`: "the combo record is 1004 bytes; this build's is 1712";
- a package from `bc93e65e` to `f29a1a82` (before PR [#1125](https://github.com/spencerduncan/redshipblueship/pull/1125)): "MM's half is N bytes; this build's is M", one check before the combo record's.

**Why.** A package carries Majora's Mask's half and the combo record at its writer's sizes, and the loader accepts only this build's sizes. PR #1125 grew Majora's Mask's save through its 53 tree checks, so a package from before `fabe7d33` carries a shorter Majora's Mask half. PR #1147 appended the Combo Tracker's reachability captures to the combo record (1004 to 1712 bytes in memory), so every package from before `efe573ec` carries a shorter combo record. The appended combo bytes are play-time state, never part of the generated world, so a package from `fabe7d33` to `e3de3ff1` could be read with them zeroed; [#1151](https://github.com/spencerduncan/redshipblueship/issues/1151) proposes that. It would not rescue a package from before `fabe7d33`, whose Majora's Mask half is the older layout.

**What to do.** Generate the seed again from the same seed and settings on the new build (with every new option at its default the world is the same). For a package from `fabe7d33`, `4fc4fd41` or `e3de3ff1` you can instead wait for #1151.

### ~~A spoiler from an older build with the same seed name leaves a new world without a spoiler~~ — RESOLVED ([#1055](https://github.com/spencerduncan/redshipblueship/issues/1055), PR [#1076](https://github.com/spencerduncan/redshipblueship/pull/1076))

**Fixed (PR #1076, 2026-10-21):** a paired seed whose spoiler name is already taken by another identity's spoiler (an older build's, or one made with other MM options) is now written beside it as `Randomizer/<icons>-<fingerprint>.json`, and the Randomizer menu's "Spoiler File:" line names it. The integration rows clear their seed's spoilers before creating, so a stale spoiler in the build folder no longer fails them. What it was:

A paired world's spoiler is named from the seed (`Randomizer/<seed icons>.json`), and a creation
never writes over another paired world's spoiler (PR [#937](https://github.com/spencerduncan/redshipblueship/pull/937)): the old file is left as it is,
the new world gets no spoiler, and only stderr says so (`[MM] spoiler: REFUSED to ... it is
another paired world's spoiler`). The world itself is created and plays normally. Since PR
[#1004](https://github.com/spencerduncan/redshipblueship/pull/1004) put the generator's version into the fingerprint, every spoiler a build before
`29e8dba5` wrote counts as another world's, so generating a seed you have played before on an
older build meets this. **Workaround:** before you create the file, delete or move the older
spoiler with the same seed name out of `Randomizer/` (or empty the folder, as the playtest
guide's setup says). The integration rows met the same trap in the #1004 gate (see "Gates,
2026-10-20"). The seed package ([#1044](https://github.com/spencerduncan/redshipblueship/issues/1044), PR [#1116](https://github.com/spencerduncan/redshipblueship/pull/1116)) names its file by seed and fingerprint, which
removes the collision.

### What the Shared Items rules do not do yet ([#1085](https://github.com/spencerduncan/redshipblueship/issues/1085), PRs #1080 to #1095)

Each Shared rule on Randomizer > Cross-Game (under Shared Items) is off by default; these apply only with one on.

- ~~**Two copies of a shared song.**~~ RESOLVED by PR [#1111](https://github.com/spencerduncan/redshipblueship/pull/1111) ([#1085](https://github.com/spencerduncan/redshipblueship/issues/1085)): a shared song is placed
  once whatever Shuffle Songs says. `v0.3.0-prealpha` still places two copies unless Shuffle Songs is Anywhere.
  With Shuffle Songs Off the song's vanilla check (Malon, the Windmill, the Ocarina of Time) now holds a shuffled
  item, but Ocarina of Time's Check Tracker hides song checks under Off, so that check is not listed there.
- **Not every far-game upgrade steps the shared pool at the pickup** (PR [#1122](https://github.com/spencerduncan/redshipblueship/pull/1122)). Since #1122 a capacity
  upgrade found in the other game (bomb bag, quiver, wallet, magic, Double Defense, heart pieces and containers,
  hookshot, and bottles under Shared Bottles) raises your tier at once. Ocarina of Time's Progressive Deku Stick
  and Nut upgrades found in Termina still cross and arrive at Ocarina of Time's next arrival, so Shuffle Deku
  Stick/Nut Bag keeps gating sticks and nuts; so does Ocarina of Time's Kokiri Sword under Shared Kokiri Sword
  ([#1145](https://github.com/spencerduncan/redshipblueship/issues/1145)), and the ocarina, which is never shared.
- **A surplus copy under the plentiful pool.** Ocarina of Time still shuffles a second copy of a shared Lens of Truth,
  magic arrow or mask when its item pool is plentiful.
- **Shields.** With Shared Shields on, a Mirror Shield found in Termina before any Hero's Shield also gives Ocarina of
  Time the Hylian Shield, Ocarina of Time's own Hylian Shield stays in the pool beside the shared one, and the Item
  Tracker lists each game's shield in its own section rather than once under Shared.
- **Bottles.** Shared Bottles shares the bottle count up to Ocarina of Time's four, with each bottle's contents kept
  per game, so Majora's Mask's fifth and sixth bottle slots stay empty. OoTMM's sharedBottles shares six bottles with
  their contents; the difference is deliberate and open to the operator (PR [#1094](https://github.com/spencerduncan/redshipblueship/pull/1094)).
- **Masks.** While Ocarina of Time is running, the tracker's Shared mask rows use Majora's Mask's icons.
- **Song of Time** (PR [#1102](https://github.com/spencerduncan/redshipblueship/pull/1102)). Moon Crash Behavior's New Cycle (forced by Shared Song of Time) does not yet let you
  save on the Clock Tower roof, as OoTMM's does. Forcing New Cycle under the rule is itself a divergence from OoTMM,
  which credits time resets in its logic instead.

### Majora's Mask's trap types are fixed when the file is created ([#1047](https://github.com/spencerduncan/redshipblueship/issues/1047), PR [#1049](https://github.com/spencerduncan/redshipblueship/pull/1049))

Before PR #1049, which kind of trap an MM trap check sprang was rolled at pickup from the live
`gRando.Traps.*` settings and from wherever MM's random stream happened to be, so a reload or a
settings change could spring a different trap from the same check. Now the set of enabled trap
types is written into the save with the world, and each check's type comes from the world's
seed plus the check, so the same check always springs the same trap. A change to the trap types
made before your first trip to Termina is put back when the file loads, like MM options and
tricks, and the toast names the types (for example "Restored for Majora's Mask: Blast Traps").
The Randomizer → MM Randomizer page sets the types: the seven rows after Trap Count, Freeze
Traps to Time Traps, greyed once the world is created (PR [#1107](https://github.com/spencerduncan/redshipblueship/pull/1107)). With no type chosen every
trap is a Freeze Trap, and a file created before PR #1049, whose save has no recorded set,
reads the same way. Locked headlessly; not played.

### ~~OoT's plentiful item pool can wrap the wallet inside the fill's own logic~~ — RESOLVED ([#726](https://github.com/spencerduncan/redshipblueship/issues/726))

OoT's own fill now stops every progressive grant at the item's top tier, as the combo coordinator's
rounds already did. Before the fix, a **Plentiful** pool (one more wallet, bow, slingshot, bomb bag,
strength, scale and magic than balanced) walked the fill's simulated inventory past the top: on a
plentiful + tycoon-wallet seed the wallet read 0 after the fourth copy (observed), and every other
of those items read one tier past its top. `OoTPlentifulProgressive` (rando tier) locks it.

Plentiful worlds can differ from before the fix when the wrap happened mid-fill; balanced, scarce
and minimal worlds, and the shipped default, do not move (the golden rows are unchanged).

---

## Platform and packaging

### macOS is not supported

The macOS CI build is **disabled** (`.github/workflows/generate-builds.yml`,
`build-macos`) due to an unresolved linker issue. No macOS artifacts are produced.
Windows and Linux only.

### ~~MM mods and texture packs never load~~ — RESOLVED ([#670](https://github.com/spencerduncan/redshipblueship/issues/670), PRs [#704](https://github.com/spencerduncan/redshipblueship/pull/704), [#716](https://github.com/spencerduncan/redshipblueship/pull/716))

MM now mounts its asset mods in the single executable. Both games share one
`mods/` folder: the root is OoT's and `mods/mm/` is MM's, each searched
recursively, and a mod under one partition is never registered for the other
game. Layout and precedence are in `docs/MODDING.md`.

**Loose (unpacked) asset folders now mount for both games** (PR [#732](https://github.com/spencerduncan/redshipblueship/pull/732), [#705](https://github.com/spencerduncan/redshipblueship/issues/705)).
Put OoT's loose files in `<mods>/loose` and MM's in `<mods>/mm/loose`. Each game mounts its loose
folder after its packed mods, so a loose file wins over a packed one.

**MM's mods have their own order and enabled set** (PR [#764](https://github.com/spencerduncan/redshipblueship/pull/764), [#706](https://github.com/spencerduncan/redshipblueship/issues/706)): Combo → MM Mods
lists the archives under `mods/mm`, highest priority on top, with arrows to reorder, disable
and enable, and a Rescan button. Changes are saved at once and apply when MM next mounts its
mods: its first start in the session, or after a restart once it has loaded them (the page's
note says which). A scan that cannot see the whole folder leaves the list read-only.

Limits, all described in `docs/MODDING.md`:

- Symlinked subfolders inside `loose/` are not followed.
- Removing a file needs a restart.
- A top-level `version` file must be left out.
- While you are in MM, an OoT loose file is shadowed wherever MM's base archives ship the same path.

### You must supply your own ROMs

Asset extraction requires original Ocarina of Time and Majora's Mask ROMs. No ROMs
or extracted archives are distributed. See `docs/BUILDING.md` and
`docs/mm-archive-setup.md`.

### Config and settings are still Ship-of-Harkinian-shaped

The config file is still named `shipofharkinian.json` and lives in the `soh`
directory (`rsbs/src/main.cpp`). This is deliberate — it keeps existing Ship configs
working. The settings-migration issue
([#34](https://github.com/spencerduncan/redshipblueship/issues/34)) closed with PR
[#461](https://github.com/spencerduncan/redshipblueship/pull/461), which converged
MM's volume and tunic keys onto OoT's and added a config updater; the namespace
decision is ADR 0003 (`docs/adr/0003-settings-namespace.md`). A standalone 2Ship
install's config is not read.

In narrow windows (the ui tier's 832x600 profile, for one),
Settings → General's **Boot Sequence** dropdown is drawn over its own label and
**ImGui Menu Scaling**'s dropdown covers the end of its label. Ship of Harkinian
draws these rows the same way (right-aligned dropdowns as wide as their longest
option, in a column that narrows with the window), so they are kept as shipped
([#899](https://github.com/spencerduncan/redshipblueship/issues/899)).

### Majora's Mask's randomizer pages are under Randomizer ([#1101](https://github.com/spencerduncan/redshipblueship/issues/1101))

Since PRs [#1104](https://github.com/spencerduncan/redshipblueship/pull/1104) to [#1107](https://github.com/spencerduncan/redshipblueship/pull/1107) (2026-10-22, in `v0.3.0-prealpha`), Majora's Mask's
randomizer pages and the pair's rules are pages of the Randomizer header, after Tricks/Glitches: **Combo → MM Randomizer** is **Randomizer → MM Randomizer**, **Combo → MM
Tricks** is **Randomizer → MM Tricks**, **Combo → Cross-Game Rules** is **Randomizer → Cross-Game**, and **Randomizer →
MM Locations** is new. Older reports and the records below use the old names. A menu last left on a moved page opens
on it in its new place. The MM Randomizer page's sections carry Ship of Harkinian's names: "Logic & Conditions",
"Shuffle Options" and "Items" are gone, and an option is under Logic, Win Condition, Area Access, Shuffle Items,
Shuffle Shops & Merchants, Additional Items, Traps, Hints or Starting Items.

**The Randomizer sidebar scrolls in a small window.** It has 16 entries (Ship of Harkinian's 12 and our four), and Ship
of Harkinian's own 12 already scrolled at 832x600. The pull requests estimated from the menu code, without measuring,
that about 10 entries fit at 832x600 and about 14 at 1280x800, so at 1280x800 the last entries scroll too (#1104,
#1106). Nobody has looked at it on screen.

### Some menu entries are stubs

Settings entries backed by unimplemented functionality are grayed out or labeled
where they were caught. Randomizer → MM Randomizer shows no per-row liveness label; its one
disabled row is the retired "Majora Access: Remains". This pass was not exhaustive —
an enabled-looking toggle that does nothing is a plausible bug, and worth reporting.

### The "Fipps" overlay font choice is gone

`Fipps-Regular.otf` shipped with the inherited port trees and asserted "All rights
reserved" with no license grant, so it was deleted (2026-09-21; see
`THIRD_PARTY_NOTICES.md`). It is no longer offered in the overlay font picker. It
was never the default, and a saved overlay-font selection that still names it — or
names anything else that is not loaded — falls back to **Press Start 2P** rather
than leaving a dead entry in the picker.

**Your saved selection is rewritten, once.** The setting is
`gSettings.OverlayFont` in `shipofharkinian.json` (older configs spell it
`gOverlayFont`; the game's own config migration renames it for you). Because the
fallback is a font that IS loaded, the first launch after this change stores
`Press Start 2P` over whatever the key used to say and flushes the config, so a
selection of the removed font is not kept for later. That is deliberate: the
alternative left the overlay drawn in ImGui's built-in face instead of a font this
project ships. No save file is touched — this is a settings key, not save data.

---

## CI and quality gates (for contributors)

The two gates that once **could not fail** — the more dangerous state, because
they read as coverage — are armed:
[#375](https://github.com/spencerduncan/redshipblueship/issues/375)
(`check-symbol-collisions.sh` had no committed baseline) and
[#376](https://github.com/spencerduncan/redshipblueship/issues/376) (orphaned
CTest labels, missing `--no-tests=error`, an unfailable `check-archives`, a
frame-budgeted watchdog; PRs [#409](https://github.com/spencerduncan/redshipblueship/pull/409),
[#437](https://github.com/spencerduncan/redshipblueship/pull/437)). Both are closed.

Duplicate-symbol bugs are only observable on Linux (Windows links with
`/FORCE:MULTIPLE`); `.github/workflows/link-check.yml` gives every push to a
`claude/**` branch a fast Linux link plus the three nm gates
([#387](https://github.com/spencerduncan/redshipblueship/issues/387), PR
[#548](https://github.com/spencerduncan/redshipblueship/pull/548)).

Two rows to know about before you read a red or green as a signal:

- **`build-windows` now runs both test tiers.** It used to run none ([#623](https://github.com/spencerduncan/redshipblueship/issues/623)).
  PR [#642](https://github.com/spencerduncan/redshipblueship/pull/642) added the `redship` tier, and since PR [#723](https://github.com/spencerduncan/redshipblueship/pull/723) ([#709](https://github.com/spencerduncan/redshipblueship/issues/709)) it also runs the whole
  `rando` tier, every golden row included. The `rando` tier adds about a minute.
  The first Windows run after a runner-image or compiler change rebuilds the sccache from zero and
  takes far longer. That is a cold cache, not a regression.
- **`IntSwitchOoTHmsToMm` is a real signal now.** Until
  [#544](https://github.com/spencerduncan/redshipblueship/issues/544) it waited on
  the file-select hook, which needs a Start press the harness never sends, so it
  timed out at 120 s every time without running one assertion. It now injects a
  debug save from the title screen (as `IntGameplayRoundtrip` does), fires the
  Happy Mask Shop entrance after 20 live gameplay frames outside the shop, and
  asserts the switch routes to MM `0xD800`. When an OoT stage before the trigger
  stalls while OoT keeps running frames (the #544 state), the row fails after
  30 s in that stage with a line naming it ("title screen / file select never
  presented", "gameplay never reached", ...) instead of hitting the CTest wall.
  ~~A wedge inside a single frame, or in the OoT-to-MM hand-off and MM half after
  the trigger, still ends at the 120 s CTest timeout: the budget is checked once
  per OoT frame, not from a watchdog thread.~~ RESOLVED
  ([#793](https://github.com/spencerduncan/redshipblueship/issues/793)): a
  wall-clock watchdog thread now ends every `int-*` run that goes 60 s with no
  frame completing, with an `[INT-WATCHDOG] FAIL` line naming the last stage
  (an OoT frame, an MM frame, the hand-off) and each game's state, and exit
  code 3 (`RSBS_INT_WATCHDOG_SECS` changes the budget; 0 disables it, e.g. under a
  debugger). Either way a red run of this row is a regression to read, not
  known noise.
- **A game controller cannot disturb the rows** (PR [#1153](https://github.com/spencerduncan/redshipblueship/pull/1153), [#1152](https://github.com/spencerduncan/redshipblueship/issues/1152)). libultraship lets SDL deliver joystick events to a window without focus, and test windows never take focus, so until #1153 a pad in use while a tier ran walked Link and pressed buttons in the test windows: the integration rows failed with a camera that would not follow, or a warp or boot at the wrong entrance, at a different point each run (the red of cycle 22 under "Gates, 2026-10-23, `main` after `v0.3.0-prealpha`"). Every `--test` and `--integration-test` process now turns those background events off and opens no controller, every CTest row also carries `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=0`, and each `[FOCUS]` line ends `controllerBackground=0 openControllers=0`; any other value fails an integration run. `TestControllersIgnored` locks it. You no longer need to leave the pad alone, or unplug it, while the tiers run.
- **Three rows cross with a paired file:** `IntPairedFirstCrossing` (PR
  [#790](https://github.com/spencerduncan/redshipblueship/pull/790)),
  `IntPairedFirstCrossingDiverged` (changed settings, the load's restore, then a real MM
  boot; PR [#833](https://github.com/spencerduncan/redshipblueship/pull/833)) and
  `IntPairedFirstCrossingTwoCycles` (a second arrival in Termina; PR
  [#891](https://github.com/spencerduncan/redshipblueship/pull/891)). Like every
  `integration` row they need the ROM archives, so hosted CI never runs them; run them locally
  before merging anything that touches creation, the `.redsave` load, or the MM arrival.

Also: ~~clang-format is enforced against an incremental allowlist
(`.github/clang-format-paths.txt`), not the full tree~~ RESOLVED
([#369](https://github.com/spencerduncan/redshipblueship/issues/369)): `src/common/` and
`rsbs/` are enforced whole. The vendored game trees stay on the file-by-file allowlist by
design: a vendored file is never reformatted just to enrol it.

---

## Tracking

- [#492](https://github.com/spencerduncan/redshipblueship/issues/492) — Phase 3.1
  tracker (two-way combo randomizer; closing awaits the operator)
- [#500](https://github.com/spencerduncan/redshipblueship/issues/500) — Phase 3.2
  tracker (cross-game logic and beatability), with its increment epics
  [#644](https://github.com/spencerduncan/redshipblueship/issues/644) (closed) and
  [#645](https://github.com/spencerduncan/redshipblueship/issues/645)
- [#564](https://github.com/spencerduncan/redshipblueship/issues/564) — the one-game
  semantics audit
- [#577](https://github.com/spencerduncan/redshipblueship/issues/577) — cross-game
  models (every child landed)
- Epics closed by the 2026-09-30 wave: [#458](https://github.com/spencerduncan/redshipblueship/issues/458)
  (trackers), [#575](https://github.com/spencerduncan/redshipblueship/issues/575) (hints),
  [#800](https://github.com/spencerduncan/redshipblueship/issues/800) (shops),
  [#836](https://github.com/spencerduncan/redshipblueship/issues/836) (a refused file is never
  opened unpaired), [#837](https://github.com/spencerduncan/redshipblueship/issues/837) (every
  crossing is a save), [#879](https://github.com/spencerduncan/redshipblueship/issues/879)
  (MM's paired start)
- [#310](https://github.com/spencerduncan/redshipblueship/issues/310) — manual
  runtime QA pass (open); findings should be appended to this document rather than
  tracked separately
- Closed trackers, for history: [#321](https://github.com/spencerduncan/redshipblueship/issues/321)
  (pre-alpha v0.1.0 readiness), [#381](https://github.com/spencerduncan/redshipblueship/issues/381)
  (Phase 2 review follow-ups), [#392](https://github.com/spencerduncan/redshipblueship/issues/392)
  (Phase 3.0)
- Full open list: `gh issue list -R spencerduncan/redshipblueship`
