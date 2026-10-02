# Known issues

**Applies to:** `main` at `200adaea` (2026-09-28, PR [#794](https://github.com/spencerduncan/redshipblueship/pull/794): the single-bag switch of PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)
plus the 35 PRs merged after it) and the GitHub Actions builds cut from it; the `v0.1.1-prealpha` tag
(2026-07-03) is older than everything in the first section.
**Last updated:** 2026-09-30 (the open cross-game findings, then a corrections pass). **Playtesting this build?** Start with
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

**What ships at `200adaea`:**

- **One seed, one paired world, one bag, items crossing in both directions.** Generating an
  OoT randomizer seed also generates the paired Majora's Mask world. OoT items are
  placed on MM checks and MM items on OoT checks, and **a crossed item is no longer also
  in its home pool** (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)). Crossings land only on chests (OoT: non-shop
  `EN_BOX` chests outside the treasure-chest game; MM: its Tier-A chests), at most 64 per
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
  the hookshot (OoT 2, MM 1) always, and the ocarina when the frozen rules arm it. Stick
  and nut capacity exist only in OoT and are unchanged.
- **Both worlds are generated together, once, at file creation.** Freeze, the single-bag
  fill, each game's own pass over its leftover checks (traps and junk stay in their home
  game), one spoiler with a `combo` section, one atomic identity publish, and the MM shadow
  armed last (PRs [#680](https://github.com/spencerduncan/redshipblueship/pull/680), [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
  Arrival in MM no longer generates anything: it hydrates the frozen shadow or
  refuses. A generation failure fails file creation itself, at file select, with
  no partial identity and no silent vanilla Termina fallback. A paired world's OoT
  spoiler loaded on its own at file select is refused with a toast.
- **Foreign items have their own identity.** An OoT item in Termina shows its real
  name and a coloured pickup toast; an MM item in Hyrule is presented as a native
  pickup (PRs [#524](https://github.com/spencerduncan/redshipblueship/pull/524),
  [#551](https://github.com/spencerduncan/redshipblueship/pull/551)).
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
  options *before* creating the file: `Combo → MM Randomizer` and `Combo → MM
  Tricks` (menu pages since 2026-09-27; before that a pop-out window opened from
  `Combo → Windows`, and before PR
  [#745](https://github.com/spencerduncan/redshipblueship/pull/745) from
  Randomizer → Cross-Game).
- **The pair is generated with logic set to Glitchless** by default, and the fill proves
  the frozen goal (`beat-both`, Ganon & Majora, by default) over both games' logic.
  It runs behind a deterministic attempt ladder with a host-calibrated budget (~30 s
  floor, ~90 s ceiling; PRs [#680](https://github.com/spencerduncan/redshipblueship/pull/680), [#743](https://github.com/spencerduncan/redshipblueship/pull/743)). On the development
  workstation 30 real creations took 5.9-13.7 s, each on its first attempt. **A progress
  overlay paints during file creation** ("Placing item n of N in both worlds"; PRs
  [#707](https://github.com/spencerduncan/redshipblueship/pull/707), [#749](https://github.com/spencerduncan/redshipblueship/pull/749), [#582](https://github.com/spencerduncan/redshipblueship/issues/582)): the window stays responsive and menu
  input is suppressed while it paints.
- **The Happy Mask Shop door is never shuffled.** It is the OoT ↔ MM crossing, so
  OoT's own entrance randomizer now leaves both directions of it out of every
  shuffle pool; under interior (or any) entrance shuffle the door you walk
  through is still the door to Termina (PR [#691](https://github.com/spencerduncan/redshipblueship/pull/691), [#661](https://github.com/spencerduncan/redshipblueship/issues/661)).
- **Paired MM worlds can enable individual Majora's Mask logic tricks.** MM now
  has a per-trick vocabulary (86 trick keys, frozen into the world's identity
  like OoT's). **37** of them are bound to a real logic edge, 20 are reserved for an
  Ocarina of Time item and 29 are recorded as not bindable (PRs
  [#686](https://github.com/spencerduncan/redshipblueship/pull/686), [#696](https://github.com/spencerduncan/redshipblueship/pull/696), [#703](https://github.com/spencerduncan/redshipblueship/pull/703), [#713](https://github.com/spencerduncan/redshipblueship/pull/713), [#729](https://github.com/spencerduncan/redshipblueship/pull/729), [#763](https://github.com/spencerduncan/redshipblueship/pull/763); [#578](https://github.com/spencerduncan/redshipblueship/issues/578)). Every trick is off by default.
  **One of them is Deku-Stick combat** (PR [#729](https://github.com/spencerduncan/redshipblueship/pull/729), [#719](https://github.com/spencerduncan/redshipblueship/issues/719)). MM's logic used to assume you
  could fight 17 kinds of enemy with a Deku Stick, even though the matching trick is off by default.
  Those enemies now need `Deku Stick Fighting` enabled, or another weapon. Using the stick as a fire
  source is unchanged. None of the pinned test worlds moved. A
  trick whose edge is not bound draws disabled-with-reason on Combo → MM
  Tricks rather than enabled-and-inert ([#697](https://github.com/spencerduncan/redshipblueship/issues/697) closed with PR [#763](https://github.com/spencerduncan/redshipblueship/pull/763)). PR #763 also
  follows OoTMM on two tightenings, so with the tricks off logic kills Twinmold only with
  the Giant's Mask, magic and a sword ("Twinmold with Bow (MM)" admits the Bow), and the
  West Clock Town bank's heart piece needs the Giant's Wallet ("Bank Rewards Require One
  Less Wallet" admits the Adult Wallet).
- **You may opt into one shared Ocarina across both games** (Combo → Cross-Game
  Rules → "[Both Games] Shared Ocarina") — off by default, frozen at file creation
  like every other combo rule: obtaining an ocarina in
  either game grants it in the other (PR
  [#675](https://github.com/spencerduncan/redshipblueship/pull/675)).
- **You choose the goal, and meeting it ends the paired game.** Combo → Cross-Game
  Rules → "[Both Games] Goal" offers OoTMM's goals: Ganon & Majora (the default), Any
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

- **Nothing of the switch has been played yet.** Every claim above comes from tests,
  measurements and UI captures. The playtest is
  [`playtest-2026-09.md`](playtest-2026-09.md).
- ~~**The in-game crossing views lag Majora's Mask.**~~ **Resolved** ([#799](https://github.com/spencerduncan/redshipblueship/issues/799)).
  Combo > Windows > Toggle Cross-Game Spoiler and the Combo Tracker list every crossing
  in both directions, by name, with whether each host check was collected. That state
  comes from each game's own save. Before #799, Majora's Mask's was read only from the
  copy written at the last game switch or save, and arriving in Termina empties that
  copy, so while Majora's Mask was played the tracker's Majora's Mask panel read "No data
  yet" and every crossing hosted there read "?" until Majora's Mask's first save or the
  next switch. Now, while Majora's Mask is played, both windows read its running save
  ("Updated live"), and a crossing collected there shows as collected at once. From
  Hyrule, Majora's Mask's is as of the last game switch or save (as of file creation
  until Majora's Mask is first entered), which the switch itself writes (#755, #757).
- **Paired-world hints are partial.** OoT's hints have no pair-level Way of the Hero
  or barren analysis. Crossing hosts are never hinted, and an OoT item that crossed is
  hinted as "Termina" (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Triforce Hunt is not yet a paired goal.** The Goal row lists it, but generation
  refuses it, and OoTMM's Triforce Quest is not offered (PRs [#740](https://github.com/spencerduncan/redshipblueship/pull/740), [#760](https://github.com/spencerduncan/redshipblueship/pull/760), [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).
- ~~**Some settings re-seed the world without changing a rule.**~~ RESOLVED: the pool-size
  sliders ([#801](https://github.com/spencerduncan/redshipblueship/issues/801)) and the
  "OoT Classes" / "MM Classes" rows ([#834](https://github.com/spencerduncan/redshipblueship/issues/834))
  are retired, so every Cross-Game Rule left changes a rule (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Surplus filler can be dropped** when more MM items land in Hyrule than OoT items
  leave; only filler gives way (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Netplay and crossings share one 64-slot shared-item array.** Crossings alone cannot
  fill it, but a netplay peer's grants take slots too, so a pickup can still be refused
  (loudly) while both happen (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Generation can still abort**, at file creation. A wall-clock stop fails the creation
  with a toast ("Not created: try a new seed or Majora's Mask options."). No creation in
  the 30-seed sample needed more than two fill batches, which bounds the failure rate
  below about 10%, not at zero. No partial or corrupt file is left behind.
- **One MM randomizer option is drawn disabled on purpose.** "Majora Access: Remains"
  on Combo → MM Randomizer is retired ("Option is Retired"; ADR 0010 answer O1). No MM
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
checked in the code at `200adaea`; each links the issue that will fix it):

**~~Shops sell only their own game's items~~ — plain shops and Business Scrubs RESOLVED; merchants, the Treasure Chest Game and Tingle pending** ([#800](https://github.com/spencerduncan/redshipblueship/issues/800)).
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
to Majora's Mask like a cross-game chest. Still pending: OoT's merchants and Treasure Chest
Game, and MM's Tingle, hold only their own game's items. Shop shuffle and scrub shuffle are off
by default (OoT's "Shop Shuffle" and "Scrubs Shuffle", MM's "Shuffle Shops"). MM's shop stock joins
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
running save: the panel says "Updated live." and a collected check shows at once, with no
save. In Hyrule it reads the copy, as of the last game switch or save.

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

**Paired files created before PR #680 (2026-09-17) are not migrated.** Increment 2
moved the entire paired generation to the file-create seam, and the arrival in Majora's
Mask no longer generates anything: it hydrates the frozen MM half or refuses. Such a file
loads and plays in Ocarina of Time under the Cross-Game Rules it was created with (a
file whose unified save carries the combo record, written since PR #628 on 2026-08-06,
has any changed rule restored at load, as the between-sessions entry below describes;
an older file has no record to compare and is exempt).
What happens at the crossing depends on whether the file entered Majora's Mask before
#680:

- **Never crossed:** it has no frozen MM half. The **first crossing into Majora's Mask**
  is refused with a toast saying the file has no paired Majora's Mask world, rather than
  silently re-generating the world: Termina stays un-randomized and the slot is latched
  against writes to the pair for the rest of the session.
- **Crossed before #680:** it carries the MM half the old arrival generated, and the
  arrival hydrates whatever frozen half it finds. It either plays that old pre-#680 MM
  world, or, if its creation-time MM profile stamp no longer matches this build's MM
  options, the load restores the options from that half when they reproduce the stamp,
  and otherwise warns at load ("Not restored: Majora's Mask options differ") and the
  crossing is refused by the MM-options check. Which one a given file meets cannot be
  decided by reading.

This project is pre-release — the operator has accepted invalidating existing saves
rather than spending effort on migration. **Create a new file**; there is no recovery
path for the old one.

**Paired files created between PR #680 and the switch (PR #743, 2026-09-27) are NOT
refused.** They load and keep playing the world they were created with: the old
overlay world, where a crossed item is also still in its home pool. They show nothing
of the single-bag fill, and the same seed now generates a different world. Create a new
file to play the current build.

**Paired files created before PR #772 (2026-09-28) are NOT refused, and are safe to
play.** Every arrival in MM now gives the session the cross-game slot number, so the
moon-crash reset and the owl save no longer wipe their MM half (PR [#772](https://github.com/spencerduncan/redshipblueship/pull/772)), and a moon
crash restores the file's last save (PR [#789](https://github.com/spencerduncan/redshipblueship/pull/789); see the #785 entry below). What stays wrong on such a
file is display only: the Combo Tracker's Majora's Mask panel reads "No data yet". A new
file's panel reads "As of file creation." until MM is first entered. (#772 also describes
an `[MM v]` / `[MM _]` file-select marker; only `ComboMenuBar` printed it, and it was never
constructed. PR [#794](https://github.com/spencerduncan/redshipblueship/pull/794) deleted
`ComboMenuBar`, and no screen carries the marker: a paired creation stamps both halves' markers at creation
(#765), so for any file made since then it says nothing about which half was played.)

**A seed string does not make the world it made on an older build.** PR [#763](https://github.com/spencerduncan/redshipblueship/pull/763)'s two
trick tightenings moved generated worlds, and since PR [#774](https://github.com/spencerduncan/redshipblueship/pull/774) the OoT half of every
newly generated world differs from what a build before it made from the same seed string
and settings (OoT's excluded locations now reach the settings fingerprint, in every
environment). A non-default goal is a different world from the same seed (PR [#760](https://github.com/spencerduncan/redshipblueship/pull/760)).
Nothing stored in an existing save is recomputed, so existing files are unaffected.

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
Combo Tracker and the Cross-Game Spoiler read the same store.

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
  class headers, Shared Ocarina), MM Randomizer (two access comboboxes and seven long
  labels) and the harness's MM Row States probe. The Majora's Mask page now has two columns,
  SoH's count for a mixed page, and the long labels are shorter (the renames are listed in
  the lane's pull request), so no row of ours overruns at any of the three profiles. R9 fails
  the `ui` tier if one does again at the run's profile, and the `UiSnapshotMin` row pins that
  profile to 832x600 in CI (docs/ui-style-guide.md section 1, the width rule).

**Findings.** None apart from #544, so no new issue was filed.

---

## Save loss and corruption

### ~~A changed MM option or Cross-Game Rule breaks the pair for the session~~ — RESOLVED ([#781](https://github.com/spencerduncan/redshipblueship/issues/781); [#564](https://github.com/spencerduncan/redshipblueship/issues/564))

Fixed by the #781 change: **the file's own rules win at load.** The pages are still
editable between sessions, and what they hold there is staging for the next file.
Loading a paired file now compares both its Cross-Game Rules and its Majora's Mask
profile (the same digest the crossing checks) and puts the file's own values back into
the pages:
- a changed Cross-Game Rule (Goal, Crossing Direction, Shared Ocarina) is
  restored, and a toast reads "Restored from file:" with the rows it reset
  (for example "Restored from file: Goal, Crossing Direction");
- a changed MM option or trick is restored from the file's MM half, and a toast reads
  "Restored for Majora's Mask:" with the rows it reset (for example "Restored for
  Majora's Mask: Starting Hearts +1"; a long trick name is cut with "...", and rows
  that do not fit are counted).

The file loads paired, the slot stays writable, and the next crossing into Majora's Mask
agrees with the file. The cases the file cannot answer stay visible instead of silent:
- an MM identity input the file does not record (the excluded-check list or the
  starting-item block, which no page in this build edits) loads the file paired but
  posts "Not restored: Majora's Mask options differ", and the crossing is refused until
  they match;
- a Cross-Game record field no page authors (only a file from another build can differ
  there) refuses the load with "Not paired: File made by another build" (a damaged
  record: "Not paired: Cross-game record is damaged"). The field names are on stderr.
  The OoT file still opens and plays without its Majora's Mask half, because the load
  runs after OoT has opened the file; the next crossing into Majora's Mask says so
  ("Not paired: Termina stays un-randomized") instead of skipping pairing
  silently. Nothing is saved to the pair that session. This residual is reachable only
  from another build's file or a damaged one (ADR 0011, 2026-09-28 amendment).

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
Mask panel ("As of file creation." or a later freshness note, versus "No data yet").

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

### ~~MM's clock-shuffle randomizer option was unreachable~~ — RESOLVED ([#678](https://github.com/spencerduncan/redshipblueship/issues/678), PR [#679](https://github.com/spencerduncan/redshipblueship/pull/679))

`RO_CLOCK_SHUFFLE` is now live and reachable on Combo → MM Randomizer. The two
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

### ~~OoT actor teardown writes to the save are skipped at a departure~~ — RESOLVED ([#770](https://github.com/spencerduncan/redshipblueship/issues/770), PR [#792](https://github.com/spencerduncan/redshipblueship/pull/792)); two residues in [#807](https://github.com/spencerduncan/redshipblueship/issues/807)

The teardowns PR #767 does not run also write the save. Fixed by PR #792 (2026-09-28): right
before a departure freezes OoT's save, the windmill's Song of Storms flag is cleared and Lake
Hylia's raised water is set again after the Water Temple (rando), as those actors' teardowns do
on any exit, and the lake's river water box is put back. A running room or minigame timer's
`timerState`, Sun's Song state and a magic effect's `magicState` are reset on the next OoT
arrival or file load (read in code, #770). Still open in #807: a sun switch lit by a Light
Arrow under SoH's Sunlight Arrows enhancement stays on after a departure, and an F10 between
an age change and its scene reload freezes the old `linkAge`. State cleared only by destroy
hooks is not retired either: a remote bombchu's camera focus and the entrance-randomizer Epona
state (PRs [#751](https://github.com/spencerduncan/redshipblueship/pull/751), [#767](https://github.com/spencerduncan/redshipblueship/pull/767)).

---

## Randomizer generation

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

### Some menu entries are stubs

Settings entries backed by unimplemented functionality are grayed out or labeled
where they were caught. Combo → MM Randomizer shows no per-row liveness label; its one
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
- **`IntPairedFirstCrossing` is the only row that crosses with a paired file** (PR
  [#790](https://github.com/spencerduncan/redshipblueship/pull/790)). Like every
  `integration` row it needs the ROM archives, so hosted CI never runs it; run it locally
  before merging anything that touches creation, the `.redsave` load, or the MM arrival.

Also: clang-format is enforced against an incremental allowlist
(`.github/clang-format-paths.txt`), not the full tree
([#369](https://github.com/spencerduncan/redshipblueship/issues/369), open).

---

## Tracking

- [#492](https://github.com/spencerduncan/redshipblueship/issues/492) — Phase 3.1
  tracker (two-way combo randomizer; closing)
- [#500](https://github.com/spencerduncan/redshipblueship/issues/500) — Phase 3.2
  tracker (cross-game logic and beatability), with its increment epics
  [#644](https://github.com/spencerduncan/redshipblueship/issues/644) and
  [#645](https://github.com/spencerduncan/redshipblueship/issues/645)
- [#310](https://github.com/spencerduncan/redshipblueship/issues/310) — manual
  runtime QA pass (open); findings should be appended to this document rather than
  tracked separately
- Closed trackers, for history: [#321](https://github.com/spencerduncan/redshipblueship/issues/321)
  (pre-alpha v0.1.0 readiness), [#381](https://github.com/spencerduncan/redshipblueship/issues/381)
  (Phase 2 review follow-ups), [#392](https://github.com/spencerduncan/redshipblueship/issues/392)
  (Phase 3.0)
- Full open list: `gh issue list -R spencerduncan/redshipblueship`
