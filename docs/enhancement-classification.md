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
| `Graphics` | 16 | ✅ all |
| `Restorations` | 10 | ✅ all |
| `Timesavers` | 9 | ✅ all |
| `Cutscenes` | 10 | ✅ all |
| `Saving` | 7 | ✅ all |
| `Player` | 9 | ✅ all |
| `Equipment` | 7 | ✅ all |
| `Camera` | 31 | ✅ all (the aiming and camera inversion / sensitivity keys and the three first-person switches per key since W6-CAM-AIM, §4) |
| `Dialogue` / `Dpad` / `Playback` / `Items` / `A11y` / `Mods` / `Fixes` | 17 | ✅ all |
| **Subtotal** | **130** | |

**NOT classified at individual-setting level** — 55 MM keys across 6 categories:
`Minigames` (18), `DifficultyOptions` (12), `Masks` (10), `Cycle` (8), `Songs` (7). These
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
from SoH's Audio Editor > Audio Options block, and its provider (`Audio/Sfx/`) is carved into `ship__Enh_registrars`:

| MM key | Category | How SoH's Audio Options block drives it |
|---|---|---|
| `gAudioEditor.DisableTatlCallAudio` | converged (§5.3 group E) | retired onto SoH's `gAudioEditor.DisableNaviCallAudio` ("Disable Navi Call Audio"): one row mutes the companion fairy's call in both games |
| `gAudioEditor.LinkVoiceFreqMultiplier.Enable` | translated (`kTranslatedKeys`) | SoH's one float slider `gAudioEditor.LinkVoiceFreqMultiplier` (0.4-2.5); MM's pitch hook is armed when it is not 1.0. LUS stores CVars as dot-nested JSON, so SoH's leaf cannot also be the parent of MM's pair, and a rename is impossible |
| `gAudioEditor.LinkVoiceFreqMultiplier.Scale` | translated (`kTranslatedKeys`) | the same slider's value, read by the same translator (`LinksVoicePitchMultiplier.cpp`); nothing writes MM's two keys |
| `gAudioEditor.MuteCarpenterSfx` | (O) MM-only | its own MM row, "MM Mute Carpenter Sounds", after SoH's mute rows (`RSBS_CVAR_MM_MUTE_CARPENTER_SFX`) |
| `gAudioEditor.ChildGoronCry` | (O) MM-only | its own MM row, "MM Mute Crying Goron Child", after SoH's mute rows (`RSBS_CVAR_MM_MUTE_GORON_CHILD_CRY`) |

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

#### ✅ BUG 2 (RESOLVED #454, 2026-08-02) — `gDeveloperTools.DebugSaveFileMode`: shared key, defaults now aligned

> **Fixed by #454.** MM's `RegisterDebugMode()` clobbering write into the shared cell is retired and
> MM's read default is aligned to OoT's `1` (`DEBUG_SAVE_INFO_VANILLA_DEBUG`), so the key is genuine
> class (S). The measurement below is the pre-fix state, kept for the record. The MM default is now
> **1**, not 0.

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
| P2 | **Infinite Sword Glitch** | `gCheats.EasyISG` (a **Cheat**) | `gEnhancements.Restorations.TatlISG` (a **Restoration**) | Taxonomy conflict *and* semantics: OoT treats ISG as a cheat to grant; MM treats restoring the Navi-ISG behaviour (via Tatl) as authenticity. Different menu sections, different framing. Needs a maintainer call before any merge. |
| P3 | **Camera invert / sensitivity** | `gSettings.FreeLook.Invert{X,Y}Axis`, `gSettings.Controls.InvertAiming{X,Y}Axis`, `…InvertShieldAiming{X,Y}Axis`, `…InvertZAimingYAxis`, `gSettings.FirstPersonCameraSensitivity.{Enabled,X,Y}` | `gEnhancements.Camera.RightStick.Invert{X,Y}Axis`, `gEnhancements.Camera.FirstPerson.Invert{X,Y}`, `…RightStickInvert{X,Y}`, `…GyroInvert{X,Y}`, `…Sensitivity{X,Y}` | **Different axis decomposition.** OoT splits by *context* (free-look vs aiming vs shield-aiming vs Z-aiming); MM splits by *input device* (right stick vs gyro vs first-person). There is no 1:1 mapping — OoT has no gyro axis, MM has no shield-aim axis. Merging would drop settings on both sides. |
| P4 | **Tunic / transformation colours** | `gCosmetics.Link.{Kokiri,Goron,Zora}Tunic.Value` | `gCosmetics.Link_{Kokiri,Goron,Zora}Tunic.Value` | See §3.3 BUG 1. Intent is shared; the *current keys* are desynced by an OoT migration MM never followed. Converge, but as a fix, not a plain rename. |
| P5 | **Debug save file mode** | `gDeveloperTools.DebugSaveFileMode` (default 1) | `gDeveloperTools.DebugSaveFileMode` (default 1 since #454) | **RESOLVED (#454): now (S).** See §3.3 BUG 2 — MM's clobbering write retired and read default aligned to OoT's 1. |
| P6 | **Ocarina D-pad input** | `gSettings.CustomOcarina.Dpad`, `gSettings.CustomOcarina.RightStick` (Settings > Controls > Ocarina Controls), `gEnhancements.DpadNoDropOcarinaInput`; OoT-only: `gSettings.CustomOcarina.Enabled` (the button-mapping layer) | `gEnhancements.Playback.DpadOcarina`, `gEnhancements.Playback.RightStickOcarina`, `gEnhancements.Playback.NoDropOcarinaInput` | **RESOLVED (#1062): now (S).** This row was first written from key names alone: it listed a `gSettings.OcarinaControl.Dpad` that no OoT code reads. Read at the call sites, the three switches mean the same thing on both sides (bool, default off; the D-pad or the right stick also plays the ocarina notes; a second note is no longer dropped in the frame after the first). MM therefore moved onto SoH's keys (three `kConvergedKeys` rows, three version-9 migration rows) and the held-apart entry left `kMustStayDistinct`. MM's readers were never elided (`GameInteractor_Dpad` and `GameInteractor_RightStickOcarina` are real definitions in `GameExports_SingleExe.cpp`; `code_8019AF00.c` and `z_player.c` read the other key directly); what was missing was a writer, because MM's only widgets sat in the excluded `BenMenu.cpp`. Still OoT-only: the custom button-mapping layer (`CustomOcarina.Enabled` and its per-note bindings), which MM has no equivalent of. `gSettings.DpadInText`, `gSettings.DpadHoldChange` and `gSettings.DPadOnPause` (D-pad navigation of text boxes, shops, file select and the pause screen) have no MM reader at all, so they stay (O) until MM grows the behaviour. |
| P7 | **Shield aim inversion** | `gSettings.Controls.InvertShieldAimingYAxis` (+ X axis) | `gEnhancements.Equipment.InvertShieldY` (Y only) | Different namespace *and* different axis coverage (OoT X+Y, MM Y only). Merging loses OoT's X axis. |

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
| Climb Speed | `gEnhancements.ClimbSpeed` | `gEnhancements.Player.ClimbSpeed` | both numeric |
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
> Since #693 that slider is hosted on the unified menu's **Combo → Majora's Mask** page (MM Enhancements until 2026-09-27)
> (manifest row in `RSBS::kHostedMmEnhancements`), labelled Majora's-Mask-only; the key stays
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
| `Masks` | 10 | Transformation masks, Bunny Hood persistence, Blast Mask |
| `Songs` | 7 | Song of Double Time, Soaring, Oath to Order |
| `Minigames` | 18 | Termina-specific minigames (Doggy Race, Bombers, Honey & Darling…) |
| `DifficultyOptions` | 12 | ⚠️ minus `HyperEnemies` → (S); rest MM-only (Takkuri, Frog Choir, Gibdo trade…) |
| `PlayerActions` | 3 | Arrow cycle, instant recall, remote Bombchu |
| `Restorations` | 10 | ⚠️ minus `PauseBufferWindow` → (S), minus `TatlISG` → (P); rest MM/OoT-behaviour restorations |
| `Modes` (`gModes.*`) | 5 | Play as Kafei, Hyrule Warriors Link, Time Moves When You Move, Mirrored World. **Zero OoT collisions** — clean MM-only block |
| `gFixes.*` | 4 | MM-specific colour/Epona fixes |
| `gWindows.*` | 23 | MM's own window-open namespace (OoT uses `gOpenWindows.*`) |
| `gCollisionViewer.*` | 18 | MM collision viewer |
| `gEventLog.*` | 13 | MM event log — no OoT counterpart |
| `gNotifications.*`, `gDisplayOverlay.*` | 7 | MM overlay/notification system |
| `gRando.*` | 72 | MM randomizer — see ADR 0004 §3 |

### 6.2 OoT-only (category level)

Dungeon/trade/age machinery with no MM counterpart: `gRandoSettings.*` (232),
`gTrackers.*` (78), `gGeneral.*` (21), `gGameplayStats.*`, `gTimeDisplay.*`,
`gRemote.*` (Crowd Control / Sail / Anchor), plus within `gEnhancements.*`:
`RandomizedEnemyList` (62), `ExtraTraps` (11), `OcarinaGame` (5), `MirroredWorld*`,
`TimeTravel`, `BetaQuest*`, and within `gCheats.*`: `SaveStates*`, `EasyPauseBuffer`,
`EasyInputBuffer`, `EasyQPA`, `SpeedModifier.*`, `SuperTunic`, `TimelessEquipment`,
`InfiniteEponaBoost`, `InfiniteNayru`, `NoRedeadFreeze`, `FireproofDekuShield`, and ~20 more.

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
