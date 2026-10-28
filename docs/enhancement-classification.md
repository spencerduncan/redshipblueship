# Enhancement / cheat / cosmetic classification inventory

> Companion data for [ADR 0004 — Menu information architecture](adr/0004-menu-information-architecture.md).
> Verified by grep against `origin/main` on 2026-07-21. Symbols-first; line numbers drift.
>
> **This table is the input to two ADRs.** [ADR 0004](adr/0004-menu-information-architecture.md)
> consumes the classification to lay out the menu;
> [ADR 0003](adr/0003-settings-namespace.md) consumes §5 (the convergence list) to generate
> its rename set. A misclassification here becomes a wrong rename downstream, so every row is
> backed by reading the implementation on both sides, not by name-matching alone.
>
> **On the two independent measurements.** ADR 0003 measured the collision set in parallel
> with this document and reports **30** keys where this reports **35 exact + 2 by-design**.
> The gap is method, and both are defensible: ADR 0003 excludes
> `games/oot/soh/config/ConfigMigrators.h` because its `from` fields are legacy spellings OoT
> has already migrated away from, while this document includes it. Including it is what
> surfaced the tunic desync (§3.3 BUG 1) — the key only looks shared *because* it appears in
> the migrator — and ADR 0003 independently arrived at the same fix by a different route
> (its §5.1 Tier-1 rename list). The two documents originally disagreed on only one
> substantive row — `DebugSaveFileMode`, which this document read (P) and ADR 0003 §4.1 read
> (S). #454 has since removed what made it (P) (MM's clobbering write retired, read default
> aligned to OoT's 1), so both now classify it (S); see ADR 0004 §2d for that history.

## 0. Governing principle

> "In general, this is conceptually one game. If something applying to both makes sense, it should."

The default is therefore **(S) shared-intent**. A setting is only pulled out of (S) when the
two implementations genuinely disagree — and §4 lists every case where that happened, with
the reason.

### Settled decisions this inventory assumes

These came from the maintainer and are recorded in ADR 0004; they are not reopened here.

1. **Shared-intent mechanism is ONE shared CVar** read by both games. No UI fan-out to two
   per-game keys. Consequence: **there is no per-game override** — "Infinite Health in MM
   only" is not expressible. Accepted trade, not a defect.
2. **Cheats are class (S).** One control drives both games. The existing
   `gCheats.{InfiniteHealth, InfiniteMagic, MoonJumpOnL, NoClip, EasyFrameAdvance}` key
   sharing is deliberate and correct (verified per-key in §3.2), not a bug list.
3. **Convergence direction is MM → OoT keys**, where a shared-intent setting has different
   keys on each side. "Where we should" is a scope limit: only (S) rows converge. (O) rows
   keep their own keys; (P) rows get *disambiguated*, never merged.

These are the same three premises recorded in ADR 0003 §"Settled inputs"; the two documents
were drafted in parallel from them.

## 1. Classification scheme

| Class | Meaning | Menu consequence |
|---|---|---|
| **(S)** | Shared-intent — exists in both games with equivalent semantics | ONE control, one CVar, drives both games |
| **(P)** | Per-game parallel — exists in both but semantics/units/ranges differ enough that one control would be wrong | Two controls, disambiguated keys |
| **(O)** | Only-in-one — no counterpart in the other game | One control, in that game's subsection |

(S) rows carry a status:

- **MATCH** — both games already read the same key. No action.
- **CONVERGE** — same meaning, different key names. MM renames to the OoT key.

## 2. Coverage — what this document does and does not classify

Being honest about scope, because the rename list is generated from it.

**Fully classified (category level):** all 21 MM `gEnhancements.*` categories, `gCheats.`,
and every MM-only top-level namespace (§6). Complete.

**Fully classified (individual setting level)** — the highest-value categories named in the
brief, plus everything in the collision set:

| MM category | MM keys | Covered |
|---|---:|---|
| `gCheats.` | 14 | ✅ all |
| `Graphics` | 16 | ✅ all, but per key only since W6-GFX (#1237): the eight keys MM still reads under its own spelling (Clock Type, 24 Hours Clock, the three Motion Blur keys, the two Scene Geometry keys, and Authentic Logo, INTENTIONAL) in §6.1 W6-GFX; Bow Reticle, Disable Black Bars and Enemy Health Bars in §5.3; Actor Culling and Actor Draw Distance in §3.2. The earlier "✅ all" named few of them |
| `Restorations` | 10 | ✅ all; the eight behaviour restorations per key since W6-FIXES (#1237, §6.1 W6-FIXES), N64 Weird Frames and Pause Buffer Window in §5.3 |
| `Timesavers` | 9 | ✅ all |
| `Cutscenes` | 10 | ✅ all |
| `Saving` | 7 | ✅ all |
| `Player` | 9 | ✅ all, and only since W6-PLAYER and W6-MASKS-SONGS (#1236): Instant Putaway (§5.3), Climb Speed ((P), §4 W6-PLAYER), six (O) keys (§6.1 W6-PLAYER) and Underwater Ocarina ((O), §6.1, hosted with the Ocarina rows by W6-MASKS-SONGS). The earlier "✅ all" predated any per-key row for the seven |
| `Equipment` | 7 | ✅ all since W6-PLAYER (#1236): Item Unequip and Magic Arrow Equip Speed (§5.3), Invert Shield Y ((P) P7, §4), Bombchu Drops ((P), §4 W6-PLAYER) and three (O) keys (§6.1 W6-PLAYER). The earlier "✅ all" predated any per-key row for the last four |
| `Camera` | 31 | ✅ all (the aiming and camera inversion / sensitivity keys and the three first-person switches per key since W6-CAM-AIM, §4; the gyro axes, the free-look pitch limits and the debug camera per key since W6-CAM-GYRO, §4) |
| `Dialogue` / `Dpad` / `Playback` / `Items` / `A11y` / `Mods` / `Fixes` | 17 | ✅ all; `Fixes` (and the `gFixes.*` namespace) per key only since W6-FIXES (#1237, §6.1 W6-FIXES): the earlier "✅ all" was a category-level claim |
| **Subtotal** | **130** | |

**NOT classified at individual-setting level** — 55 MM keys across 6 categories:
`Minigames` (18; per key in §6.1 since W6-MINIGAMES), `DifficultyOptions` (12), `Masks` (9; per key in §6.1 since W6-MASKS-SONGS), `Cycle` (8), `Songs` (7; per key in §6.1 and §5 since W6-MASKS-SONGS and L7). These
are classified **(O) MM-only at the category level** on the strength of their contents —
Bombers' hide-and-seek, Deku flower launches, three-day-cycle resets, Song of Double Time,
transformation masks have no OoT counterpart by construction. The two rows inside them that
*do* have OoT counterparts were pulled out and classified individually
(`DifficultyOptions.HyperEnemies` → (S), `Cutscenes.*` → (S), see §5). Anyone extending this
work should start there and confirm the remaining 53 really are (O).

Also not covered: OoT's 1570 `gCosmetics.*` keys (overwhelmingly auto-generated per-actor
colour entries with no MM counterpart — MM has 3, all in the collision set and all
classified), OoT's 232 `gRandoSettings.*` and MM's 72 `gRando.*` (randomizer settings are
ADR 0004 §3's Randomizer section, a separate storage-model problem), and both games'
tracker-UI keys.

## 3. The collision set — keys BOTH games already read

35 exact key collisions, computed by extracting every CVar key literal from `games/mm/2s2h/**`
and every macro-expanded + literal key from `games/oot/soh/**`, then intersecting. Plus 2
by-design shared keys that the intersection misses because OoT reaches them through a CMake
cache variable rather than a prefix macro.

Every one is classified below. **Under decision (2), the cheat block is deliberate-and-correct,
not a bug list.** The genuine divergences are the 2 rows marked ❌.

### 3.1 Deliberate and correct — port-level (19 keys)

Shared because there is physically one of the thing. See ADR 0004 §2 tier 1.

| Key(s) | Why correct |
|---|---|
| `gSettings.Menu.{ActiveHeader, DevToolsSidebarSection, EnhancementsSidebarSection, Popout, PoppedHeight, PoppedPos.x, PoppedPos.y, PoppedWidth, SearchAutofocus, SettingsSidebarSection, SidebarSearch, Theme}` (12) | One menu shell on one `Gui`. Shared state is the *point*. ⚠️ See caveat below. |
| `gOpenWindows.{Console, GfxDebugger, Stats}` (3) | One LUS window each, on the shared `Gui`. |
| `gSettings.CursorVisibility`, `gSettings.DisableChanges` (2) | One SDL window, one input stack. |
| `gSettings.LowResMode`, `gSettings.SimulatedInputLag` (2) | Defined **once** in `CMake/lus-cvars.cmake` and compiled into both games. Shared *by construction* — MM's menu references them via the same `CVAR_LOW_RES_MODE` / `CVAR_SIMULATED_INPUT_LAG` macros OoT uses. |

⚠️ **Caveat on the `gSettings.Menu.*SidebarSection` keys (3 of the 12).** These store the
selected sidebar **by display-name string**. OoT's Enhancements sidebars are
`Quality of Life`, `Skips & Speed-ups`, …; MM's are `Camera`, `Gameplay`, `Items/Songs`, ….
The name sets barely overlap, and MM additionally defines `gSettings.Menu.RandoSidebarSection`,
which OoT does not. Sharing is correct *only* under ADR 0004's single-shell decision (one
SohMenu, one set of sidebar names). It would silently break if MM's menu were ever revived
as a second shell — which is one more reason ADR 0004 rejects that option.

### 3.2 Deliberate and correct — shared-intent gameplay (14 keys)

Per decision (2), cheats are (S). Verified per-key that the semantics genuinely match:

| Key | OoT impl | MM impl | Verdict |
|---|---|---|---|
| `gCheats.InfiniteHealth` | `Enhancements/Cheats/Infinite/Health.cpp` | `Enhancements/Cheats/Infinite.cpp` | ✅ both bool, "always full hearts" |
| `gCheats.InfiniteMagic` | `Enhancements/Cheats/Infinite/Magic.cpp` | `Enhancements/Cheats/Infinite.cpp` | ✅ both bool, "always full magic" |
| `gCheats.NoClip` | `SohMenuEnhancements.cpp:1747` | `GameInteractor.cpp` / `BenPort.cpp` | ✅ both bool, phase through collision |
| `gCheats.MoonJumpOnL` | `Enhancements/Cheats/MoonJump.cpp` | `Enhancements/Cheats/MoonJump.cpp` | ✅ both bool, **same L binding** |
| `gCheats.EasyFrameAdvance` | `Enhancements/Cheats/EasyFrameAdvance.cpp` | `Enhancements/Cheats/EasyFrameAdvance.cpp` | ✅ both bool, hold START on unpause |
| `gAudioEditor.LowHpAlarm` | mute low-HP beep | `Audio/Sfx/MuteLowHpAlarm.cpp` | ✅ both bool |
| `gAudioEditor.EnemyBGMDisable` | disable enemy proximity music | `Audio/Sfx/DisableEnemyProximityMusic.cpp` | ✅ both bool |
| `gAudioEditor.SeqNameNotification` + `…Duration` (2) | sequence-name toast | `Audio/AudioHook.cpp` | ✅ bool + int seconds |
| `gEnhancements.Graphics.ActorCullingAccountsForWidescreen` | `ShipUtils.cpp:41` | `ShipUtils.cpp:246` | ✅ both bool, structurally identical code. **CORRECTED by #966 (#1028): not shared.** Only OoT's uncalled `ShipUtils.cpp` copy read this spelling; SoH's menu and every live OoT reader use `gEnhancements.WidescreenActorCulling`, and MM now reads that key (a `kConvergedKeys` row) |
| `gEnhancements.Graphics.IncreaseActorDrawDistance` | `ShipUtils.cpp:30` | `ShipUtils.cpp:235` | ✅ both `s32 multiplier`, default 1, slider 1–5. **CORRECTED by #966 (#1027): not shared.** Same as the row above; the key SoH writes is `gEnhancements.DisableDrawDistance` |
| `gDeveloperTools.DebugEnabled` | unlocks OoT debug features | unlocks MM debug features | ✅ same intent |
| `gDeveloperTools.FrameAdvanceTick` | frame-advance tick | frame-advance tick | ✅ same intent |
| `gDeveloperTools.LogLevel` | one spdlog logger | one spdlog logger | ✅ port-level |

**The Audio Editor's other MM keys (#1239).** Not shared spellings, so not rows of the table above; each is now driven
from SoH's Audio Editor, by its Audio Options block (providers in `Audio/Sfx/`, carved into `ship__Enh_registrars`) or
by its Majora's Mask tab (W6-AUDIO-SEQ: the port of 2Ship's editor write paths, `Audio/AudioEditorSingleExe.cpp`):

| MM key | Category | How SoH's Audio Editor drives it |
|---|---|---|
| `gAudioEditor.DisableTatlCallAudio` | converged (§5.3 group E) | retired onto SoH's `gAudioEditor.DisableNaviCallAudio` ("Disable Navi Call Audio"): one row mutes the companion fairy's call in both games |
| `gAudioEditor.LinkVoiceFreqMultiplier.Enable` | translated (`kTranslatedKeys`) | SoH's one float slider `gAudioEditor.LinkVoiceFreqMultiplier` (0.4-2.5); MM's pitch hook is armed when it is not 1.0. LUS stores CVars as dot-nested JSON, so SoH's leaf cannot also be the parent of MM's pair, and a rename is impossible |
| `gAudioEditor.LinkVoiceFreqMultiplier.Scale` | translated (`kTranslatedKeys`) | the same slider's value, read by the same translator (`LinksVoicePitchMultiplier.cpp`); nothing writes MM's two keys |
| `gAudioEditor.MuteCarpenterSfx` | (O) MM-only | its own MM row, "MM Mute Carpenter Sounds", after SoH's mute rows (`RSBS_CVAR_MM_MUTE_CARPENTER_SFX`) |
| `gAudioEditor.ChildGoronCry` | (O) MM-only | its own MM row, "MM Mute Crying Goron Child", after SoH's mute rows (`RSBS_CVAR_MM_MUTE_GORON_CHILD_CRY`) |
| `gAudioEditor.ReplacedSequences.<sfxKey>.value` / `.locked` | (O) MM-only family, **moved** to `gAudioEditor.MM.ReplacedSequences.` (`RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX`) | the Majora's Mask tab's seven sequence tabs (Background Music, Fanfares, Events, Battle Music, Ocarina, Instrument, Songs), laid out like SoH's. Not shared: 11 sfxKeys name a sequence in both games (`NA_BGM_BOSS`, `NA_BGM_ENEMY`, `NA_BGM_FILE_SELECT`, `NA_BGM_GAME_OVER`, `NA_BGM_HORSE`, `NA_BGM_HORSE_GOAL`, `NA_BGM_INGO`, `NA_BGM_MINI_BOSS`, `NA_BGM_OWL`, `NA_BGM_SHOP`, `NA_BGM_TIMED_MINI_GAME`), so under SoH's spelling either game's tab would set the other's music. MM's collection composes the family; MM's audio core reads it through `MM_AudioEditor_GetReplacementSeq`. MM's Instrument tab lists nothing: 2Ship's sequence table has no instrument rows |
| `gAudioEditor.Excluded.<sfxKey>` | (O) MM-only family, **moved** to `gAudioEditor.MM.Excluded.` (`RSBS_CVAR_MM_AUDIO_EXCLUDED_PREFIX`) | the Majora's Mask tab's Audio Shuffle Pool Management, with a Songs filter 2Ship lacked |
| `gAudioEditor.RandomizeAllOnNewScene` | translated (`kTranslatedKeys`) | SoH's combobox `gAudioEditor.RandomizeAudioGenModes` ("Automatically Randomize All Music and Sound Effects"): On New Scene arms MM's randomize on each new scene (`VB_PLAY_TRANSITION_CS`). Nothing writes MM's bool |
| `gAudioEditor.RandomizeAllOnRandoGen` | translated (`kTranslatedKeys`) | the same combobox: On Rando Gen Only arms MM's randomize when MM's randomizer file is created (`OnRandoSeedGeneration`). Nothing writes MM's bool |
| MM's Randomize / Reset / Lock / Unlock All Groups | no key | four MM rows at the top of the Majora's Mask tab ("MM Randomize All Groups", ...). "All Groups" is the seven tabbed groups: 2Ship's own list left out Songs (its Songs tab worked, its "All Groups" skipped it), so Songs is in; it listed Sound Effects and Voices, which have no replaceable MM row, so they are out |
| MM's Sound Effects and Voices tabs | **not ported** | compiled out in 2Ship itself (`#if 0`, `AudioEditor.cpp:533-541`); nothing to drive |
| `gAudioEditor.Playing` (MM's preview) | not used | MM's preview state is kept in memory by the port, so SoH's preview key stays SoH's; MM's previews play only while Majora's Mask runs |
| `gWindows.AudioEditor` (#1237) | resolved, no key | 2Ship's popout opens no window in the single exe: SoH's one Audio Editor (Enhancements > Audio Editor) holds the Majora's Mask tab (ADR 0003 §5.2, tier 2 `gWindows.*` -> `gOpenWindows.*`) |

**The Cosmetics Editor's MM keys (W6-COS-A, #1239, #1237).** MM's colours are `gCosmetic.<element>.Color` beside
`.Changed` (one family, `RSBS_CVAR_MM_COSMETIC_PREFIX`). 2Ship's Cosmetic Editor stays excluded; SoH's one Cosmetics
Editor drives MM through MM's colour lookup, `CosmeticEditor_GetChangedColor` (`games/mm/2s2h/CosmeticGfxSingleExe.cpp`,
ported from 2Ship's reader), which reads the CVars live in either game. Hosting decision (upstream-backport audit M14,
#1239 option b): an element with an SoH twin reads the twin's `.Changed` / `.Value` (one colour for both games, a
`kTranslatedKeys` pair, never a rename: the value children differ); an element with none is an MM row in SoH's editor.
The classification lock's colour-family leg (3c-3) holds every element of the lookup's table to one or the other.

| MM key (`gCosmetic.` + …, unless spelled in full) | Category | How SoH's Cosmetics Editor drives it |
|---|---|---|
| `Buttons.B` | translated (`kTranslatedKeys`) | HUD tab, SoH's B Button (`gCosmetics.HUD.BButton`) |
| `Buttons.A` | translated | HUD tab, SoH's A Button (`gCosmetics.HUD.AButton`) |
| `Buttons.CLeft`, `Buttons.CDown`, `Buttons.CRight` | translated | HUD tab, SoH's C Left / C Down / C Right Button over SoH's C Buttons (two rows each), the order OoT's HUD reads them in |
| `Buttons.DPad` | translated | HUD tab, SoH's Dpad (`gCosmetics.HUD.Dpad`) |
| `Buttons.Start` | translated | HUD tab, SoH's Start Button (`gCosmetics.HUD.StartButton`) |
| `HUD.SmallKey` | translated | HUD tab, SoH's Key Count (Advanced Mode), the small-key icon's colour in both games |
| `HUD.Minimap` | translated | HUD tab, SoH's Minimap (`gCosmetics.HUD.Minimap`) |
| `HUD.RupeeIcon` | (O) MM-only | its own MM row, "Rupee Icon", in a Majora's Mask block after SoH's HUD group (`CosmeticsEditorMm.cpp`) |
| `Hearts.Normal` | translated (W6-COS-B) | Link & Items tab, SoH's Hearts (`gCosmetics.Consumable.Hearts`) |
| `Hearts.Border` | translated | Link & Items tab, SoH's Heart Border (Advanced Mode) |
| `Hearts.DD` | translated | Link & Items tab, SoH's DD Hearts |
| `Hearts.DDBorder` | translated | Link & Items tab, SoH's DD Heart Border (Advanced Mode) |
| `Hearts.Beating` | (O) MM-only | its own MM row, "Beating Hearts", in a Majora's Mask block after SoH's Consumables group (OoT's beating heart takes the Hearts colour) |
| `Hearts.DDBeating` | (O) MM-only | its own MM row, "DD Beating Hearts", in the same block |
| `Magic.Normal` | translated | Link & Items tab, SoH's Magic (`gCosmetics.Consumable.Magic`) |
| `Magic.Active` | translated | Link & Items tab, SoH's Magic Active (Advanced Mode), the meter while magic drains |
| `Magic.Border` | translated | Link & Items tab, SoH's Magic Border (Advanced Mode) |
| `Magic.Chateau` | (O) MM-only | its own MM row, "Chateau Magic", in the Consumables block; not SoH's Infinite Magic (`gCosmetics.Consumable_MagicInfinite`), a cheat's colour where Chateau Romani's is MM's own |
| `Player.HumanTunic` | translated | Link & Items tab, SoH's Kokiri Tunic (`RSBS_CVAR_COLOR_KOKIRI_TUNIC`, the canonical key BUG 1 converged onto): a model-colour patch of Human Link, not a draw-site colour |
| `Player.HumanHair` | translated | Link & Items tab, SoH's Hair (`gCosmetics.Link.Hair`), likewise a patch |
| `Player.DekuTunic`, `Player.DekuHair`, `Player.GoronTunic`, `Player.ZoraTunic`, `Player.KafeiHair` | (O) MM-only | MM rows "Deku Form Tunic", "Deku Form Hair", "Goron Form Tunic", "Zora Form Tunic", "Kafei Hair" in a Majora's Mask block after SoH's Link group. MM's Goron and Zora Tunic colour Link's transformation forms, not OoT's tunics, so they are no twin of SoH's Goron / Zora Tunic. The patches (2Ship's registrars, ported into `CosmeticGfxSingleExe.cpp`) patch MM's resources only while MM owns the resource cache (#981/#1040), and a change made while OoT runs is applied on MM's next frame |
| `Effects.SpinSlashCharge`, `Effects.SpinSlashBurst` | translated | Effects tab, SoH's Spin Attack Level 1 Primary (OoT draws its charge and its burst from it; Level 1 Secondary sets it too) |
| `Effects.GreatSpinCharge`, `Effects.GreatSpinBurst` | translated | Effects tab, SoH's Spin Attack Level 2 Primary (likewise) |
| `Menus.FileWindow`, `Menus.FilePlates` | INTENTIONAL, no row | they colour MM's file select, which paired play never shows (the file select is OoT's; see `gEnhancements.Saving.FileSlot3` below) |
| `gWindows.CosmeticEditor` | INTENTIONAL, no row | 2Ship's editor popout resolves to SoH's one Cosmetics Editor (`gOpenWindows.CosmeticsEditor`, ADR 0003 §5.2's near-miss); no second window |

### 3.3 ❌ Accidental divergence — the actual bug list (2 rows, 4 keys)

These are the only collision-set entries where the two games disagree. Both are filed.

#### ❌ BUG 1 — `gCosmetics.Link_{Kokiri,Goron,Zora}Tunic.Value` (3 keys): stale-key desync

This is not really a collision — it is worse, and a naive collision scan mis-reports it.

- **OoT reads `gCosmetics.Link.KokiriTunic.Value`** (dot separator) —
  `OTRGlobals.cpp:2390-2395`, `CosmeticsEditor.cpp:217-218`.
- **OoT's config migrator actively renames the underscore form away**:
  `ConfigMigrators.h:482` — `{ Rename, "gCosmetics.Link_GoronTunic.Value", "gCosmetics.Link.GoronTunic.Value" }`
  (and the same for `.Rainbow`, `.Locked`, `.Changed` at lines 642, 818, 981).
- **MM still reads the pre-migration underscore form** — `BenPort.cpp:1846-1856`,
  `CVarGetColor24("gCosmetics.Link_KokiriTunic.Value", …)`.

So the key MM reads is one OoT has already migrated *out of existence*. After any OoT config
migration runs, MM's tunic colours read a key nothing writes and silently fall back to
hardcoded defaults — permanently, with no UI to correct it. The shared appearance in a grep
is a ghost of a key neither game will agree on.

Currently inert only because `BenPort.cpp` is excluded from the single-exe link. It arms the
moment MM's cosmetic surface is un-elided. **Classification: (S) shared-intent, CONVERGE →
OoT's `gCosmetics.Link.{Kokiri,Goron,Zora}Tunic.Value`.**

W6-COS-B (#1239) un-elided the first of that surface onto the canonical key: Human Link's model tunic colour
(`gCosmetic.Player.HumanTunic`) reads `gCosmetics.Link.KokiriTunic.Value` through MM's colour lookup, a
`kTranslatedKeys` pair, so SoH's Kokiri Tunic row colours both games. MM's Goron and Zora Tunic colour its
transformation forms and stay MM rows.

#### ✅ BUG 2 (RESOLVED #454, 2026-08-02) — `gDeveloperTools.DebugSaveFileMode`: shared key, defaults now aligned

> **Fixed by #454.** MM's `RegisterDebugMode()` clobbering write into the shared cell is retired and
> MM's read default is aligned to OoT's `1` (`DEBUG_SAVE_INFO_VANILLA_DEBUG`), so the key is genuine
> class (S). The measurement below is the pre-fix state, kept for the record. The MM default is now
> **1**, not 0.

> **Live in Majora's Mask since W6-DEV (#1238).** Until then the key was shared on paper only: MM's
> reader, `RegisterDebugSaveCreate` in `DeveloperTools/DeveloperTools.cpp`, was out of the single-exe
> link, so SoH's Debug Save File Mode row (`[Both Games]`) drove OoT alone. The TU is now linked
> (`ship__Enh_registrars`, W6-DEV block) and the debug save applies on MM's own new-save path only: its
> OnSaveInit body stands down while a pairing is live, because OnSaveInit also runs when the paired
> creation event authors the world's MM half, and a debug save there would replace the generated world.

| | OoT | MM |
|---|---|---|
| Default | **1** (`.DefaultIndex(1)`, `SaveManager.cpp:975` reads default `1`) | **1** since #454 (`DEBUG_SAVE_INFO_VANILLA_DEBUG`, `DeveloperTools.cpp:26`); was `0` (`DEBUG_SAVE_INFO_NONE`) |
| Value 0 | "Off — normal savefile" | `DEBUG_SAVE_INFO_NONE` — empty save |
| Value 1 | "Vanilla debug save" | `DEBUG_SAVE_INFO_VANILLA_DEBUG` |
| Value 2 | "Maxed — all items & upgrades" | `DEBUG_SAVE_INFO_COMPLETE` — 100% save |

The *enum ordering happens to align*, but the **defaults disagree**: unset, OoT behaves as
"Vanilla" and MM as "Empty". The first game to write the key silently changes the other
game's debug-save behaviour away from its own default. Low blast radius (debug-only, gated
behind `DebugEnabled`) but it is a genuine accidental divergence on a shared key.

**Classification: (S) — RESOLVED by #454.** The original reading here was (P): the key already
matched so nothing renamed, but the divergent default had to be reconciled, which is a behaviour
decision. #454 made that decision — MM's default was aligned to OoT's `1` and MM's clobbering
write retired — so the key is now genuine (S). The pre-fix (P) framing is kept below for the record.

> ⚠️ **ADR 0003 §4.1 always classified this (S)** — "value spaces align… only the unwritten
> fallback differs." This document originally took the more conservative (P) line, and #454 then
> decided it in favour of (S), aligning MM's default to OoT's 1 and retiring MM's clobbering
> write. Recorded rather than silently overwritten so the route to the shared conclusion is legible.

## 4. (P) — per-game parallel: same idea, incompatible realisation

Seven rows, two of them since resolved (P5 by #454, P6 by #1062). Each says WHY one control would be wrong. **These must not be merged by the
rename pass.**

| # | Setting | OoT key | MM key | Why (P) |
|---|---|---|---|---|
| P1 | **Text speed** | `gEnhancements.TextSpeed` (int slider, Min 1 Max 5, `%dx`) | `gEnhancements.Dialogue.FastText` (bool) | **Units are incompatible.** OoT is a 1–5× multiplier; MM is on/off. Writing MM's `1` into OoT's key means "1× = no speedup" — the toggle would silently do nothing. MM's FastText *also bundles* hold-B-to-advance, which OoT exposes as a **separate** CVar `gEnhancements.SkipText`. One control cannot express both decompositions. |
| P2 | **Infinite Sword Glitch** | `gCheats.EasyISG` (a **Cheat**) | `gEnhancements.Restorations.TatlISG` (a **Restoration**) | Taxonomy conflict *and* semantics: OoT treats ISG as a cheat to grant; MM treats restoring the Navi-ISG behaviour (via Tatl) as authenticity. Different menu sections, different framing. **Ruled (ADR 0004): keep both keys distinct** (`kMustStayDistinct`, bound through `RSBS_CVAR_MM_RESTORATIONS_TATL_ISG`); MM's own row, "Tatl ISG (restoration)", is hosted on Enhancements > Fixes > Glitch Restorations and `Restorations/TatlISG.cpp` is linked (W6-FIXES, #1237). |
| P3 | **Camera invert / sensitivity** | `gSettings.FreeLook.Invert{X,Y}Axis`, `gSettings.Controls.InvertAiming{X,Y}Axis`, `…InvertShieldAiming{X,Y}Axis`, `…InvertZAimingYAxis`, `gSettings.FirstPersonCameraSensitivity.{Enabled,X,Y}` | `gEnhancements.Camera.RightStick.Invert{X,Y}Axis`, `gEnhancements.Camera.FirstPerson.Invert{X,Y}`, `…RightStickInvert{X,Y}`, `…GyroInvert{X,Y}`, `…Sensitivity{X,Y}` | **Different axis decomposition.** OoT splits by *context* (free-look vs aiming vs shield-aiming vs Z-aiming); MM splits by *input device* (right stick vs gyro vs first-person). There is no 1:1 mapping — OoT has no gyro axis, MM has no shield-aim axis. Merging would drop settings on both sides. |
| P4 | **Tunic / transformation colours** | `gCosmetics.Link.{Kokiri,Goron,Zora}Tunic.Value` | `gCosmetics.Link_{Kokiri,Goron,Zora}Tunic.Value` | See §3.3 BUG 1. Intent is shared; the *current keys* are desynced by an OoT migration MM never followed. Converge, but as a fix, not a plain rename. |
| P5 | **Debug save file mode** | `gDeveloperTools.DebugSaveFileMode` (default 1) | `gDeveloperTools.DebugSaveFileMode` (default 1 since #454) | **RESOLVED (#454): now (S).** See §3.3 BUG 2 — MM's clobbering write retired and read default aligned to OoT's 1. |
| P6 | **Ocarina D-pad input** | `gSettings.CustomOcarina.Dpad`, `gSettings.CustomOcarina.RightStick` (Settings > Controls > Ocarina Controls), `gEnhancements.DpadNoDropOcarinaInput`; OoT-only: `gSettings.CustomOcarina.Enabled` (the button-mapping layer) | `gEnhancements.Playback.DpadOcarina`, `gEnhancements.Playback.RightStickOcarina`, `gEnhancements.Playback.NoDropOcarinaInput` | **RESOLVED (#1062): now (S).** This row was first written from key names alone: it listed a `gSettings.OcarinaControl.Dpad` that no OoT code reads. Read at the call sites, the three switches mean the same thing on both sides (bool, default off; the D-pad or the right stick also plays the ocarina notes; a second note is no longer dropped in the frame after the first). MM therefore moved onto SoH's keys (three `kConvergedKeys` rows, three version-9 migration rows) and the held-apart entry left `kMustStayDistinct`. MM's readers were never elided (`GameInteractor_Dpad` and `GameInteractor_RightStickOcarina` are real definitions in `GameExports_SingleExe.cpp`; `code_8019AF00.c` and `z_player.c` read the other key directly); what was missing was a writer, because MM's only widgets sat in the excluded `BenMenu.cpp`. Still OoT-only: the custom button-mapping layer (`CustomOcarina.Enabled` and its per-note bindings), which MM has no equivalent of. `gSettings.DpadInText`, `gSettings.DpadHoldChange` and `gSettings.DPadOnPause` (D-pad navigation of text boxes, shops, file select and the pause screen) have no MM reader at all, so they stay (O) until MM grows the behaviour. |
| P7 | **Shield aim inversion** | `gSettings.Controls.InvertShieldAimingYAxis` (+ X axis) | `gEnhancements.Equipment.InvertShieldY` (Y only) | Different namespace *and* different axis coverage (OoT X+Y, MM Y only). Merging loses OoT's X axis. |

**W6-TIMESAVERS (#1236, #1240) — MM's Dialogue, Timesavers and Saving keys.** All fifteen are (O) MM-only except Fast
Text, which is P1 above and stays held apart. MM read each of them, but its only widgets sat in the excluded
`BenMenu.cpp`, and the TUs behind the hooked ones were not linked. Twelve now have a hosted row, bound through its
`RSBS_CVAR_MM_*` define (`SohMenuHostedMmTimesavers.cpp`), with their TUs in `ship__Enh_registrars` (and
`Fixes/BgmReplay.cpp`, which keeps MM's main BGM player valid under Faster Scene Transitions and Pause Menu Save). Three
are INTENTIONAL: they cannot act in the paired game. The Timesavers family keeps MM's lowercase-s spelling (§5.1).

| MM key | Row (page > section) | Call |
|---|---|---|
| `gEnhancements.Dialogue.FastText` | MM Fast Text (Skips & Speed-ups > Text, after Text Speed) | (P) P1: MM's own row beside SoH's Text Speed |
| `gEnhancements.Dialogue.FastBankSelection` | Fast Bank Selection (Skips & Speed-ups > Majora's Mask) | (O) |
| `gEnhancements.Dialogue.AutoBombersCode` | Auto Bombers' Code (likewise) | (O) |
| `gEnhancements.Timesavers.SwampBoatSpeed` | Swamp Boat Timesaver (likewise) | (O) |
| `gEnhancements.Timesavers.GalleryTwofer` | Both Gallery Rewards (likewise) | (O); in a randomizer file the second reward is the Perfect Score check's item, given once. 2Ship's label, Shooting Gallery Both Rewards, overran its column at 832x600 (R9) |
| `gEnhancements.Timesavers.MarineLabHP` | Fast Marine Lab Fish (likewise) | (O); in a randomizer file the fish still gives its check's item once |
| `gEnhancements.Timesavers.DampeDiggingSkip` | Fast Dampe Flame Digging (likewise) | (O) |
| `gEnhancements.Timesavers.FasterSceneTransitions` | Faster Scene Transitions (likewise) | (O) |
| `gEnhancements.Timesavers.PowderKegCertification` | Skip Powder Keg Certification (likewise) | (O) |
| `gEnhancements.Timesavers.SkipBalladOfWindfish` | Skip Ballad of Windfish (likewise) | (O) |
| `gEnhancements.Timesavers.AutoBankDeposit` | Auto Bank Deposit (likewise) | (O) |
| `gEnhancements.Saving.PauseSave` | Pause Menu Save (Quality of Life > Saving, in a Majora's Mask block after SoH's rows) | (O); the save is a paired `.redsave` commit, not a flash write |
| `gEnhancements.Saving.FileSlot3` | none | INTENTIONAL: it shapes MM's file select, which paired play never shows (the file select is OoT's) |
| `gEnhancements.Saving.PersistentOwlSaves` | none | INTENTIONAL: it only stops the owl save's flash delete on continue, and a paired MM session has no flash slot (`func_80147314` returns at the 0xFF fileNum), so the owl save already persists |
| `gEnhancements.Saving.DisableSaveDelay` | none | INTENTIONAL: it only shortens the wait on a flash write, and a paired MM session never starts one |

**W6-CAM-AIM (#1236) — P3 and P7 per key, held apart and given rows.** MM read all sixteen keys below
(`GameInteractor_InvertControl` in `GameExports_SingleExe.cpp`, `z_player.c`, `FreeLook.cpp`) but its only widgets sat
in the excluded `BenMenu.cpp`, so nothing in the single exe wrote them, and three of them MM reads inverted by
default. Each key now has an MM-labelled row beside its SoH twin in SoH's Bindings window, Camera Controls panel
(`SohInputEditorWindow.cpp`, `DrawMmCameraRows`), bound through its `RSBS_CVAR_MM_*` define; each row declares the
default MM reads, and the sliders take 2Ship's ranges. The three first-person switches the §2 Camera claim covered only
at category level were checked for convergence and each failed on gating, so they are held apart too (three
`kMustStayDistinct` rows beside P3). The classification lock's (3f-5) holds every row.

| MM key | MM default | Row (after its SoH twin) | Call |
|---|---|---|---|
| `gEnhancements.Camera.FirstPerson.RightStickEnabled` | off | MM Right Stick Aiming (Aiming/First-Person Camera) | (P) gating: SoH's `gSettings.Controls.RightStickAim` also feeds Z-target weapon aiming and uses SoH's aiming axes; MM's gates its own right-stick axes and move-while-aiming |
| `gEnhancements.Camera.FirstPerson.MoveInFirstPerson` | off | MM Allow moving while in first-person mode (shown while MM Right Stick Aiming is on) | (P) gating: SoH's `gSettings.MoveInFirstPerson` is read on its own; MM's needs its right-stick switch and never moves Link on Epona, the swamp boat or in the archery games |
| `gEnhancements.Camera.FirstPerson.InvertX` | off | MM Invert Aiming X Axis | (P) P3 |
| `gEnhancements.Camera.FirstPerson.InvertY` | **on (inverted)** | MM Invert Aiming Y Axis | (P) P3 |
| `gEnhancements.Camera.FirstPerson.DisableFirstPersonAutoCenterView` | off | MM Disable Auto-Centering in First-Person View | (P) gating: MM never auto-centers while its Gyro Aiming is on (and 2Ship disables the row then); SoH's `gSettings.DisableFirstPersonAutoCenterView` has no such gate |
| `gEnhancements.Camera.FirstPerson.Sensitivity{X,Y}` | 100% | MM Aiming/First-Person Horizontal / Vertical Sensitivity (1-200%) | (P) P3: MM's applies to the left stick only, SoH's to every aiming input behind its own enable |
| `gEnhancements.Camera.FirstPerson.RightStickInvertX` | off | MM Invert Right Stick Aiming X Axis (shown while MM Right Stick Aiming is on) | (P) P3: SoH has no separate right-stick axis |
| `gEnhancements.Camera.FirstPerson.RightStickInvertY` | **on (inverted)** | MM Invert Right Stick Aiming Y Axis (likewise) | (P) P3 |
| `gEnhancements.Camera.FirstPerson.RightStickSensitivity{X,Y}` | 100% | MM Right Stick Aiming Horizontal / Vertical Sensitivity (1-200%, likewise) | (P) P3 |
| `gEnhancements.Equipment.InvertShieldY` | off | MM Invert Shield Aiming Y Axis | (P) P7 |
| `gEnhancements.Camera.RightStick.InvertXAxis` | off | MM Invert Camera X Axis (Third-Person Camera) | (P) P3 |
| `gEnhancements.Camera.RightStick.InvertYAxis` | **on (inverted)** | MM Invert Camera Y Axis | (P) P3 |
| `gEnhancements.Camera.RightStick.CameraSensitivity.{X,Y}` | 100% | MM Third-Person Horizontal / Vertical Sensitivity (1-500%) | (P) P3 |

**W6-CAM-GYRO (#1235, #1236, #1240) — gyro aiming, pitch limits, debug camera, Remote Bombchu and the speed modifiers.**
MM read every key below, but its only widgets sat in the excluded `BenMenu.cpp` and `BenInputEditorWindow.cpp`, and
three providers (`Camera/DebugCam.cpp`, `Equipment/RemoteBombchu.cpp`, `Player/LinkSpeedModifier.cpp`) were out of the
link. They are now carved into `ship__Enh_registrars`, and each key has an MM-labelled row bound through its
`RSBS_CVAR_MM_*` define: the camera and speed rows in SoH's Bindings window (`SohInputEditorWindow.cpp`,
`DrawMmCameraRows`), Remote Bombchu Control on Enhancements > Items > Explosives (`SohMenuHostedMmCamGyro.cpp`). Free
Look stands aside while the debug camera is on, and is unchanged while it is off. The registrar-coverage lock's
W6-CAM-GYRO phase attributes each TU by registry content; the classification lock's leg 3f-2c holds each binding.
`gSettings.SpeedModifier.*` was not inventoried before: it is MM's (MM-labelled rows after SoH's under Modifier
Buttons), held apart from SoH's `gSettings.WalkModifier.*` by a `kMustStayDistinct` row.

| MM key | MM default | Row (where) | Call |
|---|---|---|---|
| `gEnhancements.Camera.FirstPerson.GyroEnabled` | off | MM Gyro Aiming (Camera Controls, after MM's aiming rows) | (P) P3: SoH has no gyro aiming |
| `gEnhancements.Camera.FirstPerson.GyroInvert{X,Y}` | off | MM Invert Gyro Aiming X / Y Axis (shown while MM Gyro Aiming is on) | (P) P3 |
| `gEnhancements.Camera.FirstPerson.GyroSensitivity{X,Y}` | 100% | MM Gyro Aiming Horizontal / Vertical Sensitivity (1-200%, likewise) | (P) P3 |
| `gEnhancements.Camera.FreeLook.{MaxPitch,MinPitch}` | 72° / -49° | MM Max / Min Camera Height Angle (Camera Controls, after SoH's free-look rows) | (O) SoH's free look has no pitch limit; the pair stays ordered as in 2Ship |
| `gEnhancements.Camera.DebugCam.Enable` | off | MM Debug Camera (likewise) | (O) MM-only; listens to port 1 (its port key has no row and is not read in the single exe) |
| `gEnhancements.Camera.DebugCam.{6DOF,CameraSpeed}` | off / 50% | MM Enable Roll (6° of Freedom), MM Debug Camera Speed (10-300%, shown while MM Debug Camera is on) | (O) MM-only |
| `gEnhancements.PlayerActions.RemoteBombchu` | off | Remote Bombchu Control (Items > Explosives, after SoH's Prevent Bombchus Forcing First-Person) | (O) nearest relative only |
| `gSettings.SpeedModifier.{Enable,Toggle}` | off | MM Enable speed modifiers, MM Toggle modifier instead of holding (Modifier Buttons, after SoH's) | (P) held apart (`kMustStayDistinct`): SoH's walk modifier has one enable and a 0-500% range |
| `gSettings.SpeedModifier.{WalkEnable,SwimEnable}` | off | MM Enable Walk / Swim Speed Modifier (shown while MM's enable is on) | (P) likewise: SoH has no separate walk and swim enables |
| `gSettings.SpeedModifier.{WalkMapping1,WalkMapping2}` | 100% | MM Walk Modifier 1 / 2 (0-1500%, likewise) | (P) likewise |
| `gSettings.SpeedModifier.{SwimMapping1,SwimMapping2}` | 100% | MM Swim Modifier 1 / 2 (0-875%, likewise) | (P) likewise |

**W6-PLAYER (#1236) — Climb Speed and Bombchu Drops, held apart and given rows.** Two of W6-PLAYER's twelve keys have
an SoH twin whose rule differs, so each stays in `kMustStayDistinct` and gets an MM-labelled row right after its SoH
twin (`SohMenuHostedMmPlayer.cpp`), bound through its `RSBS_CVAR_MM_*` define. Their TUs are linked with the other
ten, which are (O) and listed in §6.1 W6-PLAYER; the coverage notes there hold for all twelve.

| MM key | MM default | Row (page > section) | Call |
|---|---|---|---|
| `gEnhancements.Player.ClimbSpeed` | 1x | MM Vine/Ladder Climb Speed (Skips & Speed-ups > Animations, after SoH's) | (P) held apart (`kMustStayDistinct`): SoH adds +0-12, MM multiplies 1-5x; §5.3 group A's "both numeric" was wrong |
| `gEnhancements.Equipment.ChuDrops` | off | MM Enable Bombchu Drops (Difficulty > Drops, after SoH's) | (P) held apart (`kMustStayDistinct`). SoH's `gEnhancements.EnableBombchuDrops` weighs what Link carries (no Bombchu drops without Bombchus, then the scarcer ammo first), and an OoT seed can force it on. MM's row converts Bomb drops to Bombchu drops once you have Bombchus: in the single exe a Bomb drop converts, at an even chance, only while `INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU` already holds, which is SoH's first clause (review H1 on #1246). The reason: the Bombchu count is a shared PROGRESSION quantity (`src/common/shared_items.h`). Under 2Ship's unguarded rule the pickup would set the Bombchu item in Termina (`z_parameter.c` Item_Give), the next harvest would carry the count into the pool, and `OoT_ApplySharedAmmoCount` would grant Hyrule the Bombchu item and open its Bombchu shops before the seed placed them (`OoT_SharedAmmoGrantBlocked` stops that only when the seed's Bombchu Bag option is on). MM's rule still never weighs the scarcer ammo, so the keys are not converged: same bool and JSON shape, different rule. MMRegistrarCoverage's FAIL 22 holds the rule. Its inline read of `gEnhancements.NewDrops` is SoH's own converged key (#1031). **2Ship import (W6-IMPORT, §6.3): an MM-only key.** Imported as MM's own key, never onto `gEnhancements.EnableBombchuDrops` |

## 5. (S) CONVERGE — the rename list for the settings-namespace ADR

**This section is the direct input to the namespace ADR's rename set.** Direction is
MM → OoT per decision (3). Each row is verified as genuinely shared-intent: same type, same
range, equivalent implementation.

### 5.1 The systematic cause

The single reason most of these differ: **the two games use different namespace *depth*
conventions.**

- **MM is fully category-namespaced.** 171 `gEnhancements.*` keys across 21 second-level
  segments; essentially every key is `gEnhancements.<Category>.<Setting>`.
- **OoT is predominantly flat.** Of 327 `gEnhancements.*` keys, the large majority are
  `gEnhancements.<Setting>` with no category segment. Only a handful of families group
  (`RandomizedEnemyList` 62, `TimeSavers` 16, `ExtraTraps` 11, `OcarinaGame` 5, `Graphics` 2).

So a setting present in both very often has **the identical leaf name** and differs only by
MM's category prefix. Those are the safest rows in the table.

⚠️ **Casing hazard for the ADR:** MM uses `gEnhancements.Timesavers.*` (lowercase `s`, 9
keys); OoT uses `gEnhancements.TimeSavers.*` (capital `S`, 16 keys). They do **not** collide,
but they are one keystroke apart in a case-sensitive store. Any rename touching this family
must be explicit about which spelling wins.

⚠️ **Consequence of "unify around OoT keys":** since (O) rows keep their own keys, MM's
category-namespaced MM-only keys stay as they are. `shipofharkinian.json` will therefore be
permanently **mixed-convention** — flat for shared settings, category-nested for MM-only
ones. That is an accepted consequence of the scope limit, not an oversight.

### 5.2 Rows that already MATCH (no action)

The 33 keys in §3.1 + §3.2. Listed there; not repeated.

### 5.3 Rows that must CONVERGE (26)

**Group A — identical leaf, MM adds a category prefix (12).** Highest confidence; mechanical.

| Setting | OoT key (target) | MM key (rename from) | Verified |
|---|---|---|---|
| Fast Chests | `gEnhancements.FastChests` | `gEnhancements.Timesavers.FastChests` | both bool |
| Bow Reticle | `gEnhancements.BowReticle` | `gEnhancements.Graphics.BowReticle` | both bool |
| Disable Black Bars | `gEnhancements.DisableBlackBars` | `gEnhancements.Graphics.DisableBlackBars` | both bool |
| Instant Putaway | `gEnhancements.InstantPutaway` | `gEnhancements.Player.InstantPutaway` | both bool |
| Climb Speed | `gEnhancements.ClimbSpeed` | `gEnhancements.Player.ClimbSpeed` | ~~both numeric~~ **held apart (P)**: OoT adds +0-12, MM multiplies 1-5x (`kMustStayDistinct`); MM row hosted, see §4 W6-PLAYER |
| Item Unequip | `gEnhancements.ItemUnequip` | `gEnhancements.Equipment.ItemUnequip` | both bool |
| Autosave | `gEnhancements.Autosave` | `gEnhancements.Saving.Autosave` | both bool — see caveat |
| Remember Save Location | `gEnhancements.RememberSaveLocation` | `gEnhancements.Saving.RememberSaveLocation` | both bool ⚠️ see #442 |
| D-pad Equips | `gEnhancements.DpadEquips` | `gEnhancements.Dpad.DpadEquips` | both bool |
| Pause Buffer Window | `gEnhancements.PauseBufferWindow` | `gEnhancements.Restorations.PauseBufferWindow` | both int frames |
| Hyper Enemies | `gEnhancements.HyperEnemies` | `gEnhancements.DifficultyOptions.HyperEnemies` | both bool |
| Alternate Assets Hotkey | `gSettings.Mods.AlternateAssetsHotkey` | `gEnhancements.Mods.AlternateAssetsHotkey` | ⚠️ **different top-level namespace** (`gSettings` vs `gEnhancements`) |

> **Autosave caveat (do not over-merge).** The *enable* flag is genuinely (S) — both bool,
> both "save periodically". But OoT's interval is **hardcoded to 3 minutes**
> (`THREE_MINUTES_IN_UNIX`, `Enhancements/QoL/Autosave.cpp`) with no CVar, while MM exposes
> `gEnhancements.Saving.AutosaveInterval` (int slider 1–60, default 5). The interval is
> therefore **(O) MM-only** and must NOT be swept into the rename. One shared checkbox
> driving a 3-minute OoT save and a 5-minute MM save is the accepted behaviour; the interval
> slider stays in the MM subsection.
> Since #693 that slider is hosted in the unified menu (manifest row in `RSBS::kHostedMmEnhancements`); since #1133
> it sits on Enhancements > Quality of Life directly after SoH's Autosave (the **Combo → Majora's Mask** page that held
> it retired with the Combo header), labelled "MM Autosave Interval" and Majora's-Mask-only; the key stays
> in `RSBS::kMustStayDistinct`. A `static_assert` in `src/common/cvar_shared_keys.h` fails
> the build if the key is dropped from that table, and the `CvarClassification` source scan
> (step 3c-2) fails if `games/oot` ever names it, spelled out or as
> `CVAR_ENHANCEMENT("Saving.AutosaveInterval")`, which is how the two intervals would
> converge with the table untouched.

**Group B — wording drift, same meaning (7).** Confirmed by reading both implementations.

| Setting | OoT key (target) | MM key (rename from) | Note |
|---|---|---|---|
| Climb everything | `gCheats.ClimbEverything` | `gCheats.ClimbAnywhere` | "Everything" vs "Anywhere" |
| Hookshot everything | `gCheats.HookshotEverything` | `gCheats.HookshotAnywhere` | same |
| Infinite money | `gCheats.InfiniteMoney` | `gCheats.InfiniteRupees` | same concept, different noun |
| Infinite ammo | `gCheats.InfiniteAmmo` | `gCheats.InfiniteConsumables` | ⚠️ MM's "consumables" scope may be wider than OoT's "ammo" — confirm before renaming. PARKED (`kMustStayDistinct`); until it is answered MM keeps its own row, "MM Consumables", right after SoH's Ammo on Cheats > Infinite... (W6-CYCLE-CHEATS, #1240; `RSBS_CVAR_MM_CHEATS_INFINITE_CONSUMABLES`). MM's reader `Cheats/Infinite.cpp` was already linked (#1005-#1009); the row was MISSING. |
| Unrestricted items | `gCheats.NoRestrictItems` | `gCheats.UnrestrictedItems` | ⚠️ OoT = ignore age restrictions; MM = all forms use all items. Same *intent* (drop item gating), different gate. Confirm. PARKED (`kMustStayDistinct`); until it is answered MM keeps its own row, "MM Unrestricted Items", right after SoH's on Cheats > Items (W6-CYCLE-CHEATS, #1240), and `Cheats/UnrestrictedItems.cpp` is linked. |
| Enemy health bar | `gEnhancements.EnemyHealthBar` | `gEnhancements.Graphics.EnemyHealthBars` | **singular vs plural** leaf |
| Title cards | `gEnhancements.TimeSavers.DisableTitleCard` | `gEnhancements.Cutscenes.HideTitleCards` | "Disable…Card" vs "Hide…Cards" |

**Group C — cutscene skip family (4).** OoT groups under `TimeSavers.SkipCutscene.*`; MM
under `Cutscenes.Skip*Cutscenes`. Same behaviour, both bool.

| Setting | OoT key (target) | MM key (rename from) |
|---|---|---|
| Skip entrance cutscenes | `gEnhancements.TimeSavers.SkipCutscene.Entrances` | `gEnhancements.Cutscenes.SkipEntranceCutscenes` |
| Skip intro | `gEnhancements.TimeSavers.SkipCutscene.Intro` | `gEnhancements.Cutscenes.SkipIntroSequence` |
| Skip one-point cutscenes | `gEnhancements.TimeSavers.SkipCutscene.OnePoint` | `gEnhancements.Cutscenes.SkipOnePointCutscenes` |
| Skip story cutscenes | `gEnhancements.TimeSavers.SkipCutscene.Story` | `gEnhancements.Cutscenes.SkipStoryCutscenes` |
| Skip boss introductions (#1123, version 9) | `gEnhancements.TimeSavers.SkipCutscene.BossIntro` | `gEnhancements.Cutscenes.SkipEnemyCutscenes` |

Remaining members of each family are (O): OoT-only `SkipCutscene.{GlitchAiding, LearnSong,
QuickBossDeaths}` (MM's song-teacher skips key on Story, as upstream 2S2H does); MM-only
`Cutscenes.{SkipFirstCycle, SkipToFileSelect}`, neither of which can act in the paired game (the
cross-game arrival is MM's intro event and already grants SkipFirstCycle's set, #654/#1123; the
title screen is OoT's). `Cutscenes.SkipMiscInteractions` is no longer MM-only: since #1046 MM reads
SoH's `gEnhancements.TimeSavers.SkipMiscInteractions` ("Skip Misc Interactions"), and #1093 linked
its providers.

`Cutscenes.SkipGetItemCutscenes` is NOT MM-only (#1060): its twin is SoH's
`gRandoEnhancements.TimeSavers.SkipGetItemAnimation` ("Skip Get Item Animations", Randomizer
page), outside the `gEnhancements` namespace. The value spaces differ (SoH Off / Junk Items /
All Items, MM Never / Junk Items Only / Everything But Major / Always), so it is a
**translated** pair (`RSBS::kTranslatedKeys`), not a rename: every MM reader calls
`Rando::StaticData::SkipGetItemCutsceneTier()`, which reads SoH's key and maps Off → Never,
Junk Items → Junk Items Only, All Items → Always. The story-cutscene gives (masks, songs, the
Oath to Order, the Giants' Chamber), which queue only outside rando, ask
`StoryGiveShowsGetItemCutscene()` instead and keep their hold unless SoH says All Items: SoH
itself always shows outside rando, so its default Junk Items must not strip them. MM's own key
is left to its elided widget.

**Group D — free-look (3).** OoT files free-look under `gSettings.`, MM under
`gEnhancements.Camera.` — a top-level namespace disagreement.

| Setting | OoT key (target) | MM key (rename from) | Note |
|---|---|---|---|
| Free look enable | `gSettings.FreeLook.Enabled` | `gEnhancements.Camera.FreeLook.Enable` | **"Enabled" vs "Enable"** — one character |
| Max camera distance | `gSettings.FreeLook.MaxCameraDistance` | `gEnhancements.Camera.FreeLook.MaxCameraDistance` | identical leaf |
| Transition speed | `gSettings.FreeLook.TransitionSpeed` | `gEnhancements.Camera.FreeLook.TransitionSpeed` | identical leaf |

**#1037 — Damage Multiplier, held apart and hosted.** `gEnhancements.DamageMult` (OoT) and
`gEnhancements.DifficultyOptions.DamageMultiplier` (MM) are the same idea (a left-shift of the damage taken) and the
values 0-4 agree (1x-16x), but OoT's list runs 0-8 (to 256x) and MM's is 0-4 plus 10 (1 Hit KO), so each menu would
show a blank for the other's value. Ruled: the keys stay distinct (`kMustStayDistinct`, no longer PARKED) and MM's own
row, with MM's own list, is hosted beside SoH's on Enhancements > Difficulty ("MM Damage Multiplier"). MM's provider
(`DifficultyOptions/DamageMultiplier.cpp`) was elided from the single exe and is carved into `ship__Enh_registrars`.
The #966 audit table's row for it reads: not mapped, hosted separately, fixed by #1037.

**#1235 — Ambience Volume, hosted on Settings > Audio.** `gSettings.Volume.Ambience` is the fifth key of the volume
family (ADR 0003 §5.1: one integer-percent slider per channel, spelled OoT-style, `RSBS_CVAR_VOLUME_*`). It is (O)
MM-only in effect: OoT has no ambience sequence player, so only MM's `MM_AudioMgr_Init` reads it. It keeps the family
spelling so OoT could adopt it, and was MISSING-ROW: nothing in the single exe wrote it (2Ship's own slider is in the
excluded `BenMenu.cpp`). Its row, "Ambience Volume", now sits right after SoH's Sound Effects Volume, SoH's int slider
(0-100, default 100), its tooltip ending "Majora's Mask only." The same change makes all five volume sliders reach
MM live: MM read them only in `MM_AudioMgr_Init`, once per bring-up, so a slider moved after MM's first init applied
in MM only after a restart; the slider callbacks now also call `MM_Volume_ReapplyPortScales`
(`src/common/mm_volume_bridge.h`).

**#1133 — the curated rows (#682, #693) hosted beside their twins.** The Combo header's Majora's Mask page retired
(ruling D5). Each row it held now sits on SoH's own Enhancements pages through the hosted-row seam, binds an
`RSBS_CVAR_MM_*` define (classification leg 3f-2c) and takes its label, tooltip, widget and liveness from its
`RSBS::kHostedMmEnhancements` entry. No key is renamed.

| Row | Key | Host | Placement | Form |
|---|---|---|---|---|
| MM Autosave Interval | `gEnhancements.Saving.AutosaveInterval` (`RSBS_CVAR_MM_AUTOSAVE_INTERVAL`) | Enhancements > Quality of Life > Saving | directly after SoH's Autosave, hidden while Autosave is off | MM's own row beside the twin (the #1037 form): OoT's interval is a fixed 3 minutes, the key stays in `kMustStayDistinct` |
| Autosave | `gEnhancements.Autosave` (converged) | Enhancements > Quality of Life > Saving | SoH's own row | one row for both games (#1119's form); its tooltip says so; the page's pointer to it is gone |
| Skip Time Song Cutscenes | `gEnhancements.Songs.SkipSoTCutscenes` (`RSBS_CVAR_MM_SKIP_SOT_CUTSCENES`) | Enhancements > Skips & Speed-ups > Cutscenes | directly after SoH's Skip Song Cutscenes, its nearest relative | (O) MM-only row, plain label |
| Game Over Prompt | `gEnhancements.Kaleido.GameOver` (`RSBS_CVAR_MM_KALEIDO_GAME_OVER`) | Enhancements > Quality of Life | the "Majora's Mask" block at the end of Time of Day, after #1245's Time Cycle rows | (O) MM-only, no twin |
| Better Double Time | `gEnhancements.Songs.BetterSongOfDoubleTime` (`RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME`) | Enhancements > Quality of Life | the same block, after Game Over Prompt | (O) MM-only, no twin |

The two rows with no twin share one block on Quality of Life, the page where SoH keeps how play flows around saving
and the time of day: the Game Over Prompt is a save prompt on death, and Better Double Time picks the hour the Song of
Double Time skips to. MM Mods (Rescan Mods Folder and the MM mod list) is a Settings page of its own, directly after
SoH's Mod Menu page, which is labelled OoT Mods (the operator's ruling of 2026-10-27: "Have two rows, OoT Mods and MM
Mods instead of just Mods"); it binds no key (`Combo_MMModSet_*`, `RSBS_CVAR_MM_ENABLED_MODS` / `_DISABLED_MODS` are
written by the model).

**Hosted-row convention (#1133 ruling D5, #1233).** Every MM row goes on the SoH page holding its twin or nearest
relative, directly after that row, through a project-added function SoH's page calls in one line; a row with no twin
goes under a "Majora's Mask" block on the most applicable SoH page. Rows are added through the hosted-row seam
(`games/oot/soh/SohGui/HostedMm/SohMenuHostedMm.h`), which records each one so the UI snapshot captures, hovers and
lints it and the menu lock finds it under its section, bound to its key. A row beside its twin is labelled "MM ..." and
every hosted row's tooltip ends "Majora's Mask only."; the "[Both Games]" labels stay. An MM-row key is an
`RSBS_CVAR_MM_*` define bound by exactly one shared-menu row and read under `games/mm` (the classification lock's leg
3f-2c).

MM-only within free-look: `MinPitch`, `MaxPitch` → (O). OoT-only:
`FreeLook.CameraSensitivity.{X,Y}` → see P3, do not merge.

**Group E — the #966 key-name gaps (rows added after the audit).** MM's reader sat on a
differently spelled twin of the key SoH's widget writes, so the control moved a key nothing in
MM consulted. Each row was read at both call sites and agrees on type (bool), range and
default (off).

| Setting | OoT key (target) | MM key (rename from) | Issue | Note |
|---|---|---|---|---|
| Skip Magic Arrow Equip Animation | `gEnhancements.SkipArrowAnimation` | `gEnhancements.Equipment.MagicArrowEquipSpeed` | #1035 | The key name says "speed" and the issue asked whether the value spaces differ; they do not: MM's reader is `CVarGetInteger(..., 0)` truthiness on a checkbox, exactly SoH's. |
| Permanent Heart Loss | `gEnhancements.PermanentHeartLoss` | `gEnhancements.DifficultyOptions.PermanentHeartLoss` | #1036 | The two tooltips disagreed about turning the option off. Settled: OoT restores lost containers (it keeps a heart-container count in its save stats), MM does not (it has no such count), so the shared tooltip now says so. |
| N64 Weird Frames | `gEnhancements.N64WeirdFrames` | `gEnhancements.Restorations.N64WeirdFrames` | #1038 | Default is off on both sides since #1046 (2Ship defaulted it on). |
| D-pad ocarina | `gSettings.CustomOcarina.Dpad` | `gEnhancements.Playback.DpadOcarina` | #1062 | Was P6 (held apart); see §4. |
| Right-stick ocarina | `gSettings.CustomOcarina.RightStick` | `gEnhancements.Playback.RightStickOcarina` | #1062 | Was P6. |
| Prevent dropped ocarina inputs | `gEnhancements.DpadNoDropOcarinaInput` | `gEnhancements.Playback.NoDropOcarinaInput` | #1062 | Was P6. |
| Containers Match Contents | `gEnhancements.ChestSizeAndTextureMatchContents` | `gRando.CSMC` | #1064 | The #966 audit table listed this OOT-ONLY with no MM reader; that was wrong, the reader is `gRando.CSMC` (chests, pots, grass, crates, barrels). `ChestSizeDependsStoneOfAgony` stays OoT-only (MM has no Stone of Agony). |
| Disable Navi Call Audio | `gAudioEditor.DisableNaviCallAudio` | `gAudioEditor.DisableTatlCallAudio` | #1239 | Both bool, default off, and both skip the companion fairy's call sound (MM's `VB_PLAY_TATL_CALL_AUDIO`). SoH's row is left as SoH wrote it and now drives both games. |

## 6. (O) — only-in-one, by category

### 6.1 MM-only categories (complete category-level classification)

| MM category | Keys | Why (O) |
|---|---:|---|
| `Cycle.DoNotResetBottleContent` | 1 | (O). Row "Do Not Reset Bottle Contents", Quality of Life > Time of Day, Majora's Mask block (W6-CYCLE-CHEATS, #1236); `Cycle/EndOfCycle.cpp` linked. Bottle contents never cross (the shared bottle COUNT, #970, is untouched) |
| `Cycle.DoNotResetRazorSword` | 1 | (O). Row "Do Not Reset Razor Sword", same block. Forced on for rando (`CVAR \|\| IS_RANDO`): EndOfCycle.cpp's own hook, which retired #1224's inline port in `Rando/MiscBehavior/OnCycleSave.cpp` |
| `Cycle.DoNotResetTimeSpeed` | 1 | (O). Row "Do Not Reset Time Speed", same block |
| `Cycle.DoNotResetRupees` | 1 | INTENTIONAL, no row. The combo keeps the SHARED rupees through every cycle reset in every world (`MM_RegisterSharedResourceCycleHooks`, #525), so the switch would do nothing; EndOfCycle.cpp holds its restore off in the single exe so a hand-set key cannot restore a second, unsettled count over the combo's |
| `Cycle.DoNotResetConsumables` | 1 | INTENTIONAL, no row. The five shared ammo counts survive every reset (#525), and the Powder Keg and the Magic Beans follow the world's FROZEN Keep Items on Cycle Reset (#1175, `gComboCtx.mmKeepItemsReset`); a live per-player switch may not overrule a setting frozen at file creation (one-game semantics). Held off in EndOfCycle.cpp |
| `Cycle.KeepExpressMail` | 1 | (O). Row "Keep Express Mail", same block; `Cycle/KeepExpressMail.cpp` linked (play-state guard added) |
| `Cycle.StopOceansideSpiderHouseSquatter` | 1 | (O). Row "Stop Oceanside Spider House Squatter", same block; forced on for rando (`CVAR \|\| IS_RANDO`, #1221), `Cycle/OceansideSpiderHouseSquatter.cpp` linked and #1224's port retired |
| `Cycle.OceansideWalletAnyDay` | 1 | (O). Row "Oceanside Wallet Any Day", same block; forced on for rando (#1221), `Cycle/OceansideWalletAnyDay.cpp` linked and #1224's port retired |
| `gCheats.{LongerFlowerGlide, ElegyAnywhere, TempleTimeStop, UnbreakableRazorSword}` | 4 | (O) MM-only cheats. Rows "Longer Deku Flower Glide", "Elegy of Emptiness Anywhere", "Stop Time in Dungeons" and "Unbreakable Razor Sword" under a Majora's Mask block at the end of Enhancements > Cheats (W6-CYCLE-CHEATS, #1240); their four TUs linked. Unbreakable Razor Sword is forced on for rando (#1215) |
| `Masks.EquipWhileSwimming` | 1 | (O). Row "Equippable While Swimming", Items > Masks, Majora's Mask block (W6-MASKS-SONGS, #1236); `Masks/EquipWhileSwimming.cpp` linked (play-state guard added). Keys on MM item actions, so an Ocarina of Time mask MM hosts (worn through its shared slot's C button, #977) goes on in water too |
| `Masks.BlastMaskKeg` | 1 | (O). Row "Blast Mask Has Powder Keg Force", same block; `Masks/BlastMaskKeg.cpp` linked (play-state guard added) |
| `Masks.FastTransformation` | 1 | (O). Row "Fast Transformation", same block; `Masks/FastTransformation.cpp` linked (play-state guard added). A worn foreign mask still comes off on transforming: `Rando/ForeignMaskMM.cpp` applies MM's human-form rule every player update |
| `Masks.FierceDeitysAnywhere` | 1 | (O). Row "Fierce Deity's Mask Anywhere", same block; `Masks/FierceDeityAnywhere.cpp` linked (play-state guards on its four play-state readers) |
| `Masks.PersistentBunnyHood.Enabled` | 1 | (O). Row "Persistent Bunny Hood", same block; `Masks/PersistentMasks.cpp` linked (its #438 guards were already in place, three more added). Keys on the Bunny Hood alone, which shares no pause slot with a foreign mask; a Bunny Hood found in Hyrule is MM's own inventory item through the pool |
| `Masks.PersistentBunnyHood.State` | 1 | INTERNAL, no row: the toggle `PersistentMasks.cpp` itself writes from A on the mask menu and clears when the switch is off or the hood is not owned |
| `Masks.NoBlastMaskCooldown` | 1 | (O). Row "No Blast Mask Cooldown", same block; `Masks/NoBlastMaskCooldown.cpp` linked |
| `Masks.GoronRollingIgnoresMagic` | 1 | (O). Row "Goron Rolling Ignores Magic", same block; `Masks/GoronRollingIgnoresMagic.cpp` linked (play-state guard added) |
| `Masks.GoronRollingFastSpikes` | 1 | (O). Row "Goron Rolling Fast Spikes", same block; `Masks/GoronRollingFastSpikes.cpp` linked (play-state guard added) |
| `Songs.PauseOwlWarp` | 1 | (O). Row "Pause Owl Warp", Items > Ocarina, Majora's Mask block (W6-MASKS-SONGS, #1240). `Songs/PauseOwlWarp.cpp` was already linked through the z_kaleido accessor; the row was MISSING |
| `Songs.EnableSunsSong` | 1 | (O). Row "Enable Sun's Song", same block; `Songs/EnableSunsSong.cpp` linked. Makes the song playable without its quest bit, which is not a shared song (only Epona's Song and the Song of Storms are, #1066), and its notes stay behind the shared ocarina buttons |
| `Songs.ZoraEggCount` | 1 | (O). Slider "Zora Eggs for Bossa Nova" (1-7, default 7), same block; `Songs/ZoraEggCount.cpp` linked |
| `Songs.FasterSongPlayback` | 1 | (O). Row "Faster Song Playback", same block; `Songs/FasterSongPlayback.cpp` linked (play-state guards added). SoH's `gEnhancements.FastOcarinaPlayback` is its nearest relative, not a twin (SoH skips the replay of the notes; MM cuts the wait after the song and the Elegy statues' fade) |
| `Songs.SkipSoaringCutscene` | 1 | (O). Row "Skip Soaring Cutscene", same block; `Songs/SkipSoaringCutscene.cpp` linked (play-state guard added). #1133 L7 may move it to Skips & Speed-ups > Cutscenes later |
| `Songs.{BetterSongOfDoubleTime, SkipSoTCutscenes}` | 2 | (O). Rows hosted by L7 (#1133): see the #1133 hosted-rows table in §5.3 |
| `Player.UnderwaterOcarina` | 1 | (O). Row "Underwater Ocarina", Items > Ocarina, Majora's Mask block (W6-MASKS-SONGS, #1236); `Player/UnderwaterOcarina.cpp` linked (play-state guard added) |
| `Minigames.TownArcheryScore` | 1 | (O). Slider "Town Archery Score" (1-50, default 50), Minigames > Shooting Gallery, after SoH's gallery rows (W6-MINIGAMES, #1236); `Minigames/Archery.cpp` linked. Nearest relative only: SoH's gallery options are Hyrule's. Default kept at MM's 50, which `Timesavers/GalleryTwofer.cpp` also reads. Every W6-MINIGAMES TU only moves a win condition: the reward stays the vanilla actor's, which the rando hooks take (`mm_minigame_offer_hooks_test.cpp` runs its rows with every minigame row on) |
| `Minigames.SwampArcheryScore` | 1 | (O). Slider "Swamp Archery Score" (1000-2180, default 2180), same section; `Archery.cpp`. Default kept at 2180 for `GalleryTwofer.cpp` |
| `Minigames.MarkShootingGalleryOctoroks` | 1 | (O). Row "Mark Gallery Octoroks", same section (2Ship keeps it under Accessibility); `Minigames/MarkShootingGalleryOctoroks.cpp` linked (play-state guard added) |
| `Minigames.BombersHideAndSeek` | 1 | (O). Slider "Bombers Hide-and-Seek" (1-5, default 5), Minigames, Majora's Mask block at the end of the page; `Timesavers/BombersHideAndSeek.cpp` linked (play-state guard added) |
| `Minigames.SwordsmanSchoolScore` | 1 | (O). Slider "Swordsman School" (1-30, default 30), same block; `Minigames/SwordsmanSchool.cpp` linked |
| `Minigames.HoneyAndDarlingDay1` | 1 | (O). Slider "Honey & Darling Day 1" (1-8, default 8), same block; `Minigames/HoneyAndDarling.cpp` (one registrar on its three keys) linked |
| `Minigames.HoneyAndDarlingDay2` | 1 | (O). Slider "Honey & Darling Day 2" (1-8, default 8), same block; `HoneyAndDarling.cpp` |
| `Minigames.HoneyAndDarlingDay3` | 1 | (O). Slider "Honey & Darling Day 3" (1-16, default 16), same block; `HoneyAndDarling.cpp` |
| `Minigames.RomaniTargetPractice` | 1 | (O). Slider "Romani Practice" (1-10, default 10), same block; `Minigames/RomaniTargetPractice.cpp` linked |
| `Minigames.AlwaysWinDoggyRace` | 1 | (O). Combobox "Always Win Doggy Race" (Off, With Mask of Truth, Always), same block; `Minigames/AlwaysWinDoggyRace.cpp` linked |
| `Minigames.CuccoShackCuccoCount` | 1 | (O). Slider "Cucco Shack Count" (1-10, default 10), same block; `Minigames/CuccoShackCuccoCount.cpp` linked |
| `Minigames.SkipHorseRace` | 1 | (O). Row "Skip Gorman Horse Race", same block; `Minigames/SkipHorseRace.cpp` linked |
| `Minigames.BeaverRaceRingsCollected` | 1 | (O). Slider "Beaver Race Rings" (1-20, default 20), same block; `Minigames/BeaverRace.cpp` linked (play-state guard added) |
| `Minigames.SkipLittleBeaver` | 1 | (O). Row "Skip Little Beaver Races", same block; `BeaverRace.cpp` |
| `DifficultyOptions.GoronRace` | 1 | (O). Combobox "Goron Race" (Vanilla, Balanced, Skip), same block (2Ship keeps it with the minigames); `DifficultyOptions/GoronRace.cpp` linked |
| `Minigames.BoatArcheryScore` | 1 | (O). Slider "Boat Archery Score" (1-50, default 20), same block; `Archery.cpp`. Default kept at 20, which `Timesavers/SwampBoatSkip.cpp` also reads |
| `Minigames.BoatArcheryHealth` | 1 | (O). Slider "Koume's Health" (1-30, default 10; disabled while "Invincible Koume" is on), same block; `Archery.cpp` |
| `Minigames.BoatArcheryInvincible` | 1 | (O). Row "Invincible Koume", same block; `Archery.cpp` |
| `Minigames.TreasureChestShopShowFullMaze` | 1 | (O). Row "Treasure Shop Full Maze", same block; `Minigames/TreasureChestShopFullMaze.cpp` linked |
| `DifficultyOptions` | 12 | ⚠️ minus `HyperEnemies` → (S); rest MM-only (Takkuri, Frog Choir, Gibdo trade…). Eight classified per key since W6-MODES-DIFF (#1236, below): seven rows and Delete File on Death (INTENTIONAL, no row) |
| `PlayerActions` | 3 | Arrow cycle (MM now reads SoH's `gEnhancements.BowArrowCycle`), instant recall (classified per key: Instant Fin Boomerangs Recall, W6-PLAYER below), remote Bombchu (classified per key: Remote Bombchu Control, §4 W6-CAM-GYRO) |
| `Restorations` | 10 | ⚠️ minus `PauseBufferWindow` and `N64WeirdFrames` → (S), minus `TatlISG` → (P); the other seven are MM/OoT-behaviour restorations, classified per key in W6-FIXES below |
| `Modes` (`gModes.*`) | 5 | Play as Kafei, Hyrule Warriors Link, Time Moves When You Move, Mirrored World. **Zero OoT collisions**, but not a clean MM-only block: since W6-MODES-DIFF (#1236, below) three are (O) rows and Mirrored World's `.State` is translated from SoH's `gEnhancements.MirroredWorldMode` (`kTranslatedKeys`), with `.Mode` retired |
| `gFixes.*` | 3 | MM-specific colour/Epona fixes, classified per key in W6-FIXES below. 2Ship v1's `gFixes.FixButtonEnvColor` is a rename MM's code never followed (`z_parameter.c` reads `gFixes.FixAmmoCountEnvColor`), so it is not counted |
| `gWindows.*` | 23 | MM's own window-open namespace (OoT uses `gOpenWindows.*`) |
| `gWindows.HudEditor` | 1 | (G), no row and no second window (W6-HUD-A, #1237). 2Ship's HUD Editor popout resolves to SoH's "Popout Cosmetics Editor Window": MM's HUD placements are in the Cosmetics Editor's HUD Placement tab, and `BenGui/HudEditor.cpp` stays unlinked |
| `gHudEditor.B.{Mode, Position.X, Position.Y, Scale}` | 4 | (O) MM-only (W6-HUD-A, #1239). Header "MM B Button position" in the Majora's Mask section of SoH's Cosmetics Editor, HUD Placement tab, after SoH's placements: SoH's position radio buttons (Original position = Vanilla, Anchor to the left / right = Movable left / right, No anchors = Movable center, Hidden), its Up/Down and Left/Right sliders and MM's scale. Read by `mm_hudeditor_stubs.c`, 2Ship's reader ported. Composed from `RSBS_CVAR_MM_HUD_EDITOR_PREFIX` (leg 3d-3); not SoH's `gCosmetics.HUD.*` placements, whose positions and anchors mean OoT's HUD layout |
| `gHudEditor.A.*` | 4 | (O), likewise: "MM A Button position" |
| `gHudEditor.Start.*` | 4 | (O), likewise: "MM Start Button position" |
| `gHudEditor.CUp.*` | 4 | (O), likewise: "MM C Button Up position" (Tatl's button) |
| `gHudEditor.CDown.*` | 4 | (O), likewise: "MM C Button Down position" |
| `gHudEditor.CLeft.*` | 4 | (O), likewise: "MM C Button Left position" |
| `gHudEditor.CRight.*` | 4 | (O), likewise: "MM C Button Right position" |
| `gHudEditor.DPad.*` | 4 | (O), likewise: "MM DPad items position" |
| `gHudEditor.Minimap.*` | 4 | (O), likewise: "MM Minimaps position" |
| `gHudEditor.Hearts.*` | 4 | (O), likewise (W6-HUD-B, #1239): "MM Hearts count position"; drawn by `z_lifemeter.c` |
| `gHudEditor.Magic.*` | 4 | (O), likewise (W6-HUD-B): "MM Magic Meter position"; `z_parameter.c`'s magic meter |
| `gHudEditor.Keys.*` | 4 | (O), likewise (W6-HUD-B): "MM Small Keys counter position" |
| `gHudEditor.Rupees.*` | 4 | (O), likewise (W6-HUD-B): "MM Rupee counter position" |
| `gHudEditor.Skulltulas.*` | 4 | (O), likewise (W6-HUD-B): "MM Skulltula counter position" (MM's token counter in the Spider Houses) |
| `gHudEditor.Carrots.*` | 4 | (O), likewise (W6-HUD-B): "MM Carrots position" (Epona's horse carrots) |
| `gHudEditor.Timers.*` | 4 | (O), likewise (W6-HUD-B): "MM Timers position", every timer but the moon-crash one, moved from its own start position |
| `gHudEditor.SkullKidTimer.*` | 4 | (O), likewise (W6-HUD-B): "MM Skull Kid Timer position" (2Ship's "Timer - Skull Kid"), the moon-crash countdown |
| `gHudEditor.Minigames.*` | 4 | (O), likewise (W6-HUD-B): "MM Minigame counter position" (the minigame score: its icon, such as archery's target or the fisherman's torch, and its digits) |
| `gHudEditor.Clock.*` | 4 | (O), likewise (W6-HUD-B): "MM Three Day Clock position". All three clock styles honour it: the original clock (`z_parameter.c`), the MM3D clock (`Graphics/3DSClock.cpp`) and the text clock (`Graphics/TextBasedClock.cpp`, both linked by W6-GFX) each draw as `HUD_EDITOR_ELEMENT_CLOCK` |
| HUD Editor preset | 0 | (O), no key: the section's "HUD Preset" row (Vanilla (4:3) / Hidden / Widescreen) writes every element's Mode (all nineteen since W6-HUD-B) and clears its position and scale, as 2Ship's preset did |
| `gColors.<element>.{Color, Changed}` | 0 rows | INTENTIONAL, no row: 2Ship's HUD editor colour keys are read only by its window, never by a draw, so a row would write a key nothing in MM reads. MM's HUD colours are W6-COS-A's `gCosmetic.*` |
| `gCollisionViewer.*` | 18 | MM collision viewer |
| `gEventLog.*` | 13 | MM event log — no OoT counterpart |
| `gNotifications.*` | 4 | MM notification system |
| `gDisplayOverlay.*` | 3 | (O) MM's In-Game Timer overlay; rows on Enhancements > Timers since W6-TIMERS, see the table below |
| `gRando.*` | 72 | MM randomizer — see ADR 0004 §3 |

**W6-TIMERS (#1235, #1237, #1239) — MM's In-Game Timer and Time Splits, linked and hosted.** 2Ship created its three
timer windows in the excluded `BenGui.cpp` and drew their rows in the excluded `BenMenu.cpp` and in the Time Splits
popout, so none of these keys could be written or shown. `Trackers/DisplayOverlay.cpp` and the three `Trackers/TimeSplits/`
TUs are now carved into `ship__Enh_registrars`; `TimersGuiSingleExe.cpp` registers the In-Game Timer and the splits list
beside MM's trackers, drawn only while MM runs; and SoH's own Time Splits window hosts 2Ship's popout body while MM runs,
so the popout resolves to that one surface. SoH's Additional Timers and Time Splits read OoT's save only, so no key has
a twin. Each scalar key is bound through its `RSBS_CVAR_MM_*` define (classification leg 3f-2c); the registrar-coverage
lock's W6-TIMERS phase attributes `TimeSplitsActions.cpp` by registry content, and mm-trackers-gui leg 8 holds the
windows' registration and gate.

| MM key | MM default | Row or surface (page > section) | Call |
|---|---|---|---|
| `gDisplayOverlay.Mode` | Off | In-Game Timer (Enhancements > Timers, Majora's Mask block): Off, Real-Time, In-Game Time | (O) MM-only. The mode alone shows the timer; 2Ship's `gWindows.DisplayOverlay` is not used |
| `gDisplayOverlay.Background` | off | Hide In-Game Timer Background (likewise) | (O) MM-only |
| `gDisplayOverlay.Scale` | 1.0 | In-Game Timer Scale (likewise), 1x to 5x | (O) MM-only |
| `gSettings.TimeSplits.Enable` | off | Enable Time Splits (Enhancements > Time Splits, Majora's Mask block) | (O) MM-only; arms `TimeSplitsActions.cpp`'s six hooks and shows the splits list |
| `gSettings.TimeSplits.ShowHeaders` | off | Show Split Headers (likewise) | (O) MM-only |
| `gSettings.TimeSplits.Opacity` | off | Hide Split List Background (likewise) | (O) MM-only; despite its name a switch |
| `gSettings.TimeSplits.Highlight` | off | Highlight Active Split (likewise) | (O) MM-only |
| `gSettings.TimeSplits.Follow` | off | Follow Active Split (likewise) | (O) MM-only |
| `gSettings.TimeSplits.Compare` | off | Compare Splits (likewise); the list compared against is chosen in the Time Splits window | (O) MM-only |
| `gWindows.Timesplits` | off | The splits list window ("MM Time Splits"), registered beside MM's trackers | (O) MM-only, no row: as upstream, the list draws while Enable Time Splits is on, whatever this key says |
| `gWindows.Timesplits.Settings` | off | Resolves to SoH's Time Splits window (Popout Time Splits Window), which hosts 2Ship's list management and Split Entries tabs while MM runs | INTENTIONAL, no row: one Time Splits surface, not a second window |

**W6-PLAYER (#1236) — MM's Player, Equipment and PlayerActions rows, linked and hosted.** MM read twelve Player,
Equipment and PlayerActions keys: the ten (O) keys below and the two held-apart (P) keys, Climb Speed and Bombchu Drops,
which are in §4 W6-PLAYER. Each provider TU had been dropped from the link (a plain `2ship_enh` archive keeps no TU
whose only anchor is a `RegisterShipInitFunc`) and the only widgets sat in the excluded `BenMenu.cpp`, so a key could be
neither written nor acted on. Each provider is now carved into `ship__Enh_registrars` and each key has a row on an SoH
Enhancements page, added through the hosted-row seam and bound through its `RSBS_CVAR_MM_*` define. The
registrar-coverage lock's W6-PLAYER phase attributes each TU by registry content; the classification lock's leg 3f-2c
holds each binding.

| MM key | MM default | Row (page > section) | Call |
|---|---|---|---|
| `gEnhancements.Player.ManualJump` | off | Manual Jump (Quality of Life > Controls, Majora's Mask block) | (O) MM-only |
| `gEnhancements.Player.PreventDiveOverWater` | off | Prevent Diving Over Water (likewise) | (O) MM-only |
| `gEnhancements.Player.InfiniteDekuHopping` | off | Infinite Deku Hopping (likewise) | (O) MM-only: Deku Link |
| `gEnhancements.Player.FasterPushAndPull` | off | Faster Push/Pull (Animations, after SoH's Block Pushing Speed) | (O) nearest relative only: one switch over every pushed and pulled object (gear clamps, blocks, ice blocks), not SoH's push-speed step |
| `gEnhancements.Player.FastFlowerLaunch` | off | Fast Deku Flower Launch (Animations, Majora's Mask block) | (O) MM-only: Deku Link |
| `gEnhancements.Player.FierceDeityPutaway` | off | Fierce Deity Putaway (likewise) | (O) MM-only; acts on the held sword only, never on a mask the combo owns |
| `gEnhancements.Equipment.TwoHandedSwordSpinAttack` | off | Two-Handed Sword Spin Attack (Items > Equipment, Majora's Mask block) | (O) MM-only |
| `gEnhancements.Equipment.GreatFairySwordBButton` | off | Great Fairy Sword B-Button Attack (likewise) | (O) MM-only; acts on the B button's item only, never on a mask the combo owns |
| `gEnhancements.Equipment.BetterPictoMessage` | off | Better Picto Message (likewise) | (O) MM-only |
| `gEnhancements.PlayerActions.InstantRecall` | off | Instant Fin Boomerangs Recall (Items > Boomerang, after SoH's Instant Boomerang Recall) | (O) nearest relative only: Zora Link's fins on B, not the Boomerang on its item button |

**W6-DEV (#1238, #1240) — MM's developer switches, linked and hosted.** 2Ship draws these in its own Dev Tools menu
(the excluded `BenMenu.cpp`), and their TUs (`DeveloperTools/DeveloperTools.cpp`, `ObjectDependency.cpp`,
`WarpPoint.cpp`) were out of the link, so in the single exe the keys could be neither written nor acted on. The TUs are
now carved into `ship__Enh_registrars` and each key has a row on SoH's Dev Tools > General through the hosted-row seam,
bound through its `RSBS_CVAR_MM_*` define. The registrar-coverage lock's W6-DEV phase attributes each TU by registry
content; the classification lock's leg 3f-2c holds each binding. `gDeveloperTools.*` falls outside this document's
individual-setting scope (§2), so these rows are listed here rather than in a category of their own.

| MM key | MM default | Row (page > section) | Call |
|---|---|---|---|
| `gDeveloperTools.BetterMapSelect.Enabled` | off | MM Better Debug Warp Screen (Dev Tools > General > Warping, after SoH's Better Debug Warp Screen) | (O) nearest relative only: MM's map select is its own screen (`BetterMapSelect.c`, already linked), and the row was MISSING; SoH's key (default on) stays SoH's |
| `gDeveloperTools.PreventActorUpdate` | off | Prevent Actor Update (Dev Tools > General, Majora's Mask block) | (O) MM-only; acts only under Debug Mode, so the row is disabled while it is off |
| `gDeveloperTools.PreventActorDraw` | off | Prevent Actor Draw (likewise) | (O) MM-only; likewise |
| `gDeveloperTools.PreventActorInit` | off | Prevent Actor Init (likewise) | (O) MM-only; likewise. Link is never prevented (2Ship's SActorNotPlayer filter, moved into the hook body) |
| `gDeveloperTools.DisableObjectDependency` | off | Disable Object Dependency (likewise) | (O) MM-only; MM's Debug Mode switch clears it, so the row is disabled while Debug Mode is off |
| `gDeveloperTools.WarpPoint.*` (7 keys) | unset | MM Set Warp Point / MM Warp to Warp Point / MM Clear Warp Point (Dev Tools > General > Warping, after SoH's Warp Points) | (O) MM-only. SoH's Warp Points widget stores OoT entrances in its own `WarpPoints` config block and warps OoT's play state, so it is left OoT's and MM gets rows of its own: they set, warp to and clear 2Ship's single point through `src/common/mm_dev_tools_bridge.h`, only while MM is in gameplay. `BootToWarpPoint` has no row and no reader: 2Ship's boot hook is fenced out of the single exe, since it would author a new save over the world MM was booting |

SoH's own rows cover two more MM behaviours with no MM row: Frame Advance, Advance 1 and Advance (Hold) now bind the
RUNNING game's `frameAdvCtx` (SohMenu.cpp's `RunningFrameAdvanceEnabled`; `gDeveloperTools.FrameAdvanceTick` was
already shared and MM honours it in `z_pause.c`), and Debug Save File Mode drives MM through the one shared key (§3.3
BUG 2). MM's `RegisterDebugMode`, linked with the rest, clears the four switches and MM's frame advance when Debug Mode
goes off, as in 2Ship, clearing rather than storing the keys.

**W6-MODES-DIFF (#1236, #1240) — MM's Modes, Difficulty and 'Other' keys, linked and given rows or a ruling.** None of
2Ship's Modes reached MM: Mirrored World was read (`gModes.MirroredWorld.State`, some thirty read sites) but only the
excluded `BenMenu.cpp` wrote it, and the other three Modes TUs were not linked. The same held for 2Ship's Difficulty
'Combat' and 'Other' rows. Twelve provider TUs and one new translator (`Modes/MirroredWorld.cpp`) are carved into
`ship__Enh_registrars`, each hook body that reads the play state is guarded, and each key has one row below or a
ruling. The registrar-coverage lock's W6-MODES-DIFF phase attributes each TU by registry content; the classification
lock's leg 3f-2c holds each binding and leg 3f-3b the Mirrored World translation; the `MMMirroredWorld` row drives it.

| MM key | MM default | Row (page > section) | Call |
|---|---|---|---|
| `gModes.MirroredWorld.State` | off | none: SoH's Mirrored World (Extra Modes) | translated (`kTranslatedKeys`). SoH's nine-mode `gEnhancements.MirroredWorldMode` is the one control; `Modes/MirroredWorld.cpp` sets `.State` when it changes and on each MM scene change. Off and Always map exactly; Random and Random (Seeded) per MM scene (seeded by the MM scene and the file: rando seed, else creation stamp); Dungeons and Dungeons (Vanilla) mirror MM's four temples and their lairs; Dungeons (MQ) never mirrors in MM (no Master Quest: the one mode with no MM meaning); the Dungeons Random modes add the coin. It is the only MM writer of `.State` |
| `gModes.MirroredWorld.Mode` | off | none | RETIRED. 2Ship's on/off checkbox; nothing in the single exe reads or writes it (leg 3f-3b), and SoH's row replaces it |
| `gModes.PlayAsKafei` | off | Play as Kafei (Extra Modes, Majora's Mask block) | (O) MM-only. Swaps MM's cached `gLinkHumanSkel` for Kafei's only from MM's own scene-destroy hook, or at MM's boot with no scene, so it edits MM's cache owner's copy (#981) and never a copy drawn by Ocarina of Time; it takes effect at the next MM scene change, and turning it off puts Link back at the next one |
| `gModes.HyruleWarriorsStyledLink` | off | Hyrule Warriors Styled Link (likewise) | (O) MM-only. Draws MM's own mask display lists from MM's player-limb hook, under MM's cache owner |
| `gModes.TimeMovesWhenYouMove` | off | Time Moves When You Move (likewise) | (O) MM-only |
| `gEnhancements.DifficultyOptions.JinxedTimer` | 60 | Jinxed Timer (Difficulty, Majora's Mask block after Enemies) | (O) MM-only |
| `gEnhancements.DifficultyOptions.LowerBankRewardThresholds` | off | Lower Bank Reward Thresholds (likewise) | (O) MM-only. This row owns the key; W6-TIMESAVERS' `AutoBankDeposit.cpp` also reads it (default off) |
| `gEnhancements.DifficultyOptions.DisableTakkuriSteal` | off | Disable Takkuri Steal (likewise) | (O) MM-only |
| `gEnhancements.DifficultyOptions.DekuGuardSearchBalls` | Night Only | Deku Guard Search Balls (likewise) | (O) MM-only |
| `gEnhancements.DifficultyOptions.GibdoTradeSequence` | Vanilla | Gibdo Trade Sequence Options (likewise) | (O) MM-only |
| `gEnhancements.DifficultyOptions.HiddenGrottosVisibility` | Off | Hidden Grottos Visibility (likewise) | (O) MM-only |
| `gEnhancements.DifficultyOptions.FrogChoirCount` | 5 | Frog Choir Count (likewise) | (O) MM-only |
| `gEnhancements.DifficultyOptions.DeleteFileOnDeath` | off | none | INTENTIONAL, no row. A paired file is one game, so a death may delete only the whole paired save; 2Ship's `DeleteFileOnDeath.cpp` deletes MM's own slot through MM's SaveManager, which the single exe does not compile, so the TU stays unlinked. SoH's own Delete File on Death acts on Ocarina of Time deaths only |
| `gEnhancements.Minigames.CremiaHugs` | Vanilla | Milk Run Reward Options (Difficulty, Majora's Mask block) | (O) MM-only; from 2Ship's 'Other' block, no nearer SoH relative |
| `gEnhancements.Items.AmmoBuyback` | Vanilla | Ammo Buyback Options (likewise) | (O) MM-only; from 2Ship's 'Other' block. The ammo sold is the shared count, so a sale in Termina spends Hyrule's too: one pool, one game |

**W6-GFX (#1237, #1240) — MM's clock, motion blur and scene-geometry rows, linked and hosted.** MM read eight Graphics
keys under its own spelling, all (O) MM-only, and two motion blur values that are registers, not keys. The only widgets
sat in the excluded `BenMenu.cpp`, and the two clock-style TUs (`Graphics/3DSClock.cpp`, `Graphics/TextBasedClock.cpp`)
were dropped from the link. Both TUs are now carved into `ship__Enh_registrars` (play-state guards added), and every
key but one has a row on Enhancements > Graphics through the hosted-row seam, bound through its `RSBS_CVAR_MM_*`
define. The registrar-coverage lock's W6-GFX phase attributes both clock TUs by registry content, the classification
lock's leg 3f-2c holds each binding, and MenuCapabilityGating's leg 8 holds the two register rows disabled while MM has
no play state.

| MM key | MM default | Row (page > section) | Call |
|---|---|---|---|
| `gEnhancements.Graphics.ClockType` | 0 (original) | Clock Type (Graphics, UI column, Majora's Mask block) | (O) MM-only: the three-day clock, as the original, the MM3D clock or a text-only clock. Its two TUs were UNLINKED; both draw through the HUD-editor stand-ins (`mm_hudeditor_stubs.c`) until W6-HUD-A |
| `gEnhancements.Graphics.24HoursClock` | off | 24 Hours Clock (likewise) | (O) MM-only; read inline by `z_parameter.c` and both clock TUs |
| `gEnhancements.Graphics.MotionBlur.Mode` | 0 (Dynamic) | Motion Blur Mode (Graphics, Misc. column, Majora's Mask block) | (O) MM-only; `MotionBlur.cpp` (linked from `z_play.c`) |
| `gEnhancements.Graphics.MotionBlur.Interpolate` | off | Interpolate Motion Blur (likewise; disabled while the mode is Always Off) | (O) MM-only; read by `z_play.c` |
| `gEnhancements.Graphics.MotionBlur.Strength` | 180 | Motion Blur Strength (likewise; disabled unless the mode is Always On) | (O) MM-only |
| (register `R_MOTION_BLUR_ENABLED`, no key) | set by MM's scenes | Dynamic Motion Blur (likewise) | (O) MM-only and not persisted: reached through `src/common/mm_graphics_bridge.h`, live only while MM runs a play state, otherwise disabled with the reason "Majora's Mask Not Running" and a gray note (ADR 0004's CAPABILITY state, #497) |
| (register `R_MOTION_BLUR_ALPHA`, no key) | set by MM's scenes | Dynamic Motion Blur Strength (likewise) | (O) likewise, and disabled while Dynamic Motion Blur is off |
| `gEnhancements.Graphics.FixSceneGeometrySeams` | off | Fix Scene Geometry Seams (likewise) | (O) MM-only. Read by MM's authentic patches, applied once in MM's asset phase; a change now reapplies them through `MM_GfxPatches_Reapply`, at once while MM runs and otherwise at MM's next resume (never under OoT's resource cache owner, #1000). SoH's Fix Out of Bounds Textures row (the shared `gEnhancements.FixTexturesOOB`) reapplies MM's patches the same way |
| `gEnhancements.Graphics.DisableSceneGeometryDistanceCheck` | off | Disable Scene Geometry Distance Check (likewise) | (O) MM-only; reapplied as the row above |
| `gEnhancements.Graphics.AuthenticLogo` | off | none | INTENTIONAL, no row. It changes only the console logo screen, which draws only on a cold `--game mm` boot: `z_title.c` skips it on every crossing into MM, and unpaired play is not a supported mode |

**W6-FIXES (#1237, #1236) — MM's Fixes and Restorations, linked and hosted.** MM read fifteen Fixes, `gFixes.*` and
Restorations keys and one Camera key (Fix Targeting Camera Snap), but its only widgets sat in the excluded `BenMenu.cpp`,
so nothing in the single exe wrote them, and eleven of their TUs were dropped from the link (a plain `2ship_enh`
archive keeps no TU whose only anchor is a `RegisterShipInitFunc`). The eleven are now carved into
`ship__Enh_registrars`, and fifteen keys have a row on Enhancements > Fixes, at the end of the SoH section holding
their nearest relatives, added through the hosted-row seam (`SohMenuHostedMmFixes.cpp`) and bound through its
`RSBS_CVAR_MM_*` define. The registrar-coverage lock's W6-FIXES phase attributes each TU by registry content (JP
Grottos with every leg under its one key); the classification lock's leg 3f-2c holds each binding. None has an SoH twin
that could share its key.

| MM key | MM default | Row (Fixes > section) | Call |
|---|---|---|---|
| `gEnhancements.Camera.FixTargettingCameraSnap` | off | Fix Targeting Camera Snap (Camera Fixes) | (O) MM-only; row only, `z_player.c` reads it |
| `gFixes.FixEponaStealingSword` | off | Fix Epona Stealing Sword (Gameplay Fixes) | (O) MM-only; `Fixes/FixEponaStealingSword.cpp` linked (play-state guard added) |
| `gEnhancements.Fixes.FierceDeityZTargetMovement` | off | Fix Fierce Deity Z-Target Movement (Gameplay Fixes) | (O) MM-only; `Fixes/FierceDeityZTargetMovement.cpp` linked (guard added) |
| `gEnhancements.Fixes.ConsoleCrashes` | on | none | INTENTIONAL: the single exe always keeps the crash fixes on. `src/common/mm_stubs.c`'s handler returns "not reset" and every call site takes its safe fallback; 2Ship's off mode is a soft reset through the console's "reset" command, which in the single exe is SoH's and resets Ocarina of Time, not the Majora's Mask state the crash came from |
| `gFixes.FixAmmoCountEnvColor` | off | Fix Ammo Count Color (Graphical Fixes) | (O) MM-only; row only, `z_parameter.c` reads it. The key keeps this spelling: 2Ship v1's rename to `gFixes.FixButtonEnvColor` (`ConfigUpdaters.cpp`) was never followed by MM's code, so the row never binds the renamed key. The 2Ship import (§6.3) carries it under this spelling and no longer replays the v1 rename, which moved the value onto a key nothing reads |
| `gFixes.FixIkanaGreatFairyFountainColor` | off | Fix Ikana Great Fairy Fountain Color (Graphical Fixes) | (O) MM-only; row only, `z_kankyo.c` reads it |
| `gEnhancements.Fixes.ControlCharacters` | off | Fix Text Control Characters (Graphical Fixes) | (O) MM-only; row only, `z_message_nes.c` reads it |
| `gEnhancements.Fixes.CompletedHeartContainerAudio` | off | Fix Completed Heart Container Audio (Audio Fixes) | (O) MM-only; `Fixes/CompletedHeartContainerAudio.cpp` linked |
| `gEnhancements.Restorations.PowerCrouchStab` | 0 (Patched) | MM Power Crouch Stab (Item-related Fixes, after SoH's Remove Power Crouch Stab, always shown) | (O) never mapped: SoH's `gEnhancements.CrouchStabFix` REMOVES the glitch and is a bool behind Fix Megaton Hammer Crouch Stab; MM's RESTORES it, with three values (Patched, Unpatched JP, Unpatched OoT). `Restorations/PowerCrouchStab.cpp` linked |
| `gEnhancements.Restorations.TatlISG` | off | Tatl ISG (restoration) (Glitch Restorations) | (P) P2, held apart from SoH's `gCheats.EasyISG` (§4); `Restorations/TatlISG.cpp` linked |
| `gEnhancements.Restorations.ConstantFlipsHops` | off | Constant Distance Backflips and Sidehops (Misc Restorations) | (O) restores OoT behaviour in MM; `Restorations/FlipHopVariable.cpp` linked (guard added) |
| `gEnhancements.Restorations.SideRoll` | off | Side Rolls (Misc Restorations) | (O) likewise; `Restorations/SideRoll.cpp` linked |
| `gEnhancements.Restorations.OoTFasterSwim` | off | Faster Swim (Misc Restorations) | (O) likewise; `Restorations/OoTFasterSwim.cpp` linked (guard added) |
| `gEnhancements.Restorations.WoodfallMountainAppearance` | off | Woodfall Mountain Appearance (Misc Restorations) | (O) MM-only; `Restorations/WoodfallMountainAppearance.cpp` linked |
| `gEnhancements.Restorations.JPGrottos` | off | JP Deku Palace Grottos (Misc Restorations) | (O) MM-only; `Restorations/JPGrottos.cpp` linked whole: its AfterRoomSceneCommands leg is the only writer of the flag its four ShouldActorInit legs wait on, so linking the deleting legs alone would leave Deku Palace with no grottos (guards added to the four legs) |
| `gEnhancements.Restorations.BonkCollision` | off | Bonk Collision (Misc Restorations) | (O) restores OoT behaviour in MM; `Restorations/BonkCollision.cpp` linked |

### 6.2 OoT-only (category level)

Dungeon/trade/age machinery with no MM counterpart: `gRandoSettings.*` (232),
`gTrackers.*` (78), `gGeneral.*` (21), `gGameplayStats.*`, `gTimeDisplay.*`,
`gRemote.*` (Crowd Control / Sail / Anchor), plus within `gEnhancements.*`:
`RandomizedEnemyList` (62), `ExtraTraps` (11), `OcarinaGame` (5), `MirroredWorld` (the per-scene state OoT acts on;
`MirroredWorldMode` drives both games since W6-MODES-DIFF, §6.1),
`TimeTravel`, `BetaQuest*`, and within `gCheats.*`: `SaveStates*`, `EasyPauseBuffer`,
`EasyInputBuffer`, `EasyQPA`, `SpeedModifier.*`, `SuperTunic`, `TimelessEquipment`,
`InfiniteEponaBoost`, `InfiniteNayru`, `NoRedeadFreeze`, `FireproofDekuShield`, and ~20 more.

### 6.3 The 2Ship config import (W6-IMPORT, #1240)

The one-time import of a standalone 2Ship config (`games/oot/soh/config/TwoShipImport.cpp`) used to walk only
`kConvergedKeys`, so a returning 2Ship player lost every MM-only setting. It now also adopts, as-is and under ADR 0003
§6.3's conflict rule (a value already set in the combined config wins), every key on the compile-time list
`RSBS_TWOSHIP_IMPORT_KEYS` in `src/common/cvar_shared_keys.h`. Fixed at compile time because the import runs at boot
before the menu exists. CvarClassification leg (2b) and the TwoShipImportMmKeys row hold both lists.

| Keys | Import |
|---|---|
| Every MM-only key in this document with a row bound through an `RSBS_CVAR_MM_*` define (§4 W6-CAM-AIM, W6-CAM-GYRO, W6-PLAYER; §6.1 W6-TIMESAVERS through W6-DEV; the L7 hosted rows) | Imported under MM's own spelling. MM's Damage Multiplier keeps 2Ship's value list (never mapped onto `gEnhancements.DamageMult`); Bombchu Drops stays MM's own key |
| `gSettings.Volume.Ambience` | Imported (no OoT twin); 2Ship's `gSettings.Audio.AmbienceVolume` still converges first |
| `gCheats.{InfiniteHealth, InfiniteMagic, EasyFrameAdvance, NoClip, MoonJumpOnL}`, `gEnhancements.HookshotableReticle`, `gDeveloperTools.DebugEnabled`, `gSettings.Menu.Theme` | Imported: one spelling read by both games (§3), so the imported value drives both, as the shared row does |
| `kConvergedKeys` legacy spellings (§5.3, the #966 rows, the fairy's call) | Imported onto SoH's key through the existing walk, never listed twice |
| `gFixes.FixAmmoCountEnvColor` | Imported under its live spelling; 2Ship's v1 rename to `gFixes.FixButtonEnvColor` is no longer replayed, and that key is held back |
| `gModes.MirroredWorld.State` / `.Mode` | Not imported: SoH's Mirrored World is the one control (translated, W6-MODES-DIFF) |
| `gEnhancements.Cutscenes.SkipGetItemCutscenes` | Not imported: translated from SoH's tiers (#1060) with no reverse translation; MM's 2 and 3 have no SoH value |
| `gAudioEditor.LinkVoiceFreqMultiplier.{Enable, Scale}`, `gAudioEditor.RandomizeAllOn{NewScene, RandoGen}` | Not imported: translated from SoH's rows (#1239); the pitch pair would turn SoH's float leaf into a JSON parent |
| INTENTIONAL keys with no row: Skip First Cycle, Skip to File Select, 3rd Save File Slot, Persistent Owl Saves, Disable Save Delay, Do Not Reset Rupees / Consumables, Delete File on Death, Authentic Logo, Fix Console Crashes, `gWindows.Timesplits.Settings`; the internal `Masks.PersistentBunnyHood.State` | Not imported: no row could change an imported value, and none acts in paired play |
| Families `gAudioEditor.ReplacedSequences.*`, `gAudioEditor.Excluded.*`, `gHudEditor.*`, `gCosmetic.*` (its translated colours included), `gColors.*` | Not imported; a family import is a follow-up. A raw sequence copy would land MM's choices on SoH's own keys for the eleven sfxKeys both games name (W6-AUDIO-SEQ) |

## 7. Counts

Scoped to the enhancement/cheat/cosmetic surface classified at **individual-setting level**
(MM's 185 `gEnhancements.*` + `gCheats.*` keys, and their OoT counterparts):

| Class | Rows | Notes |
|---|---:|---|
| **(S) shared-intent** | **60** | 34 already MATCH + 26 need CONVERGE (DebugSaveFileMode joined MATCH via #454) |
| — of which port-level MATCH | 19 | §3.1 |
| — of which gameplay/tooling MATCH | 15 | §3.2 (incl. DebugSaveFileMode since #454) |
| — of which CONVERGE (→ rename list) | 26 | §5.3 |
| **(P) per-game parallel** | **7** | §4 |
| **(O) only-in-one (MM side)** | **~146** | of MM's 185 in-scope keys |
| **(O) only-in-one (OoT side)** | **~328** | of OoT's 367 in-scope keys |

**Collision set:** 35 exact + 2 by-design = **37 shared keys**.

- 34 deliberate-and-correct — (S) (19 port-level, 15 shared-intent gameplay/tooling — the
  latter now including `DebugSaveFileMode`, reconciled to (S) by #454)
- **1 accidental-divergence bug** — the tunic stale-key desync (3 keys). Filed; see §3.3.
  (`DebugSaveFileMode`'s default disagreement was the second bug here; #454 resolved it —
  MM's clobbering write retired and its read default aligned to OoT's 1 — so it is now (S).)

**Bug list for filing:** 1 open issue (the tunic desync), not the 5-cheat block a naive scan
reports; `DebugSaveFileMode` was the other and is resolved by #454. Under decision (2) the cheat
sharing is correct by design.

## 8. Hazards surfaced in passing

Not classification rows, but they affect the rename pass and the menu work. Recorded so they
are not rediscovered.

1. **`"gPlaceholderBool"`** — a namespace-less dummy CVar shared by 3 disabled Rando hint
   checkboxes (`games/mm/2s2h/Rando/Menu.cpp:971,976,978`). Will collide with anything that
   ever uses the same string. Should be namespaced or removed.
2. **MM consumes `CVAR_PREFIX_AUDIO`** — a compile definition only ever *defined* in
   `CMake/soh-cvars.cmake` — by re-declaring `CVAR_AUDIO(var)` locally in two TUs
   (`Audio/AudioCollection.cpp:229`, `Audio/AudioEditor.cpp:58`) rather than including a
   shared header. Any rename in the SoH cvar config silently moves MM's audio keys too.
3. **Orphan CVars with no widget** — `gFixes.FixButtonEnvColor`,
   `gEnhancements.Restorations.N64WeirdFrames`, `gSettings.DisableMenuShortcutNotify`. These
   are settings with no way to set them; either surface or retire. (`N64WeirdFrames` is no
   longer an orphan: #1038 moved MM's reader onto OoT's `gEnhancements.N64WeirdFrames`, whose
   widget is SoH's Fixes > N64 Weird Frames checkbox, and linked the provider.)
4. **High fan-out single toggles** — 25 TUs in `Cutscenes/MiscInteractions/` all read the one
   key `gEnhancements.Cutscenes.SkipMiscInteractions` (SoH's `TimeSavers.SkipMiscInteractions` since #1046); 27 TUs in `Cutscenes/StoryCutscenes/`
   all read `…SkipStoryCutscenes`. Relevant to capability gating (ADR 0004 §4): the control
   is only as live as the *least* linked of its 25 TUs.
5. **MM's directory taxonomy ≠ MM's menu taxonomy.** `Modes/`, `Player/`, `Equipment/`,
   `Cycle/`, `Saving/`, `Items/`, `Accessibility/` all collapse into one menu sidebar called
   "Gameplay". An IA that keys off directory names will not match what users saw in 2Ship.
