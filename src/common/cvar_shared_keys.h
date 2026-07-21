/**
 * @file cvar_shared_keys.h
 * @brief The checked-in CVar classification manifest — the executable form of
 *        ADR 0003 + ADR 0004's classification inventory.
 *
 * RedShipBlueShip runs both games in one process against ONE CVar store backed
 * by ONE config file (`shipofharkinian.json`). Two documents govern that
 * keyspace, and this header is where they become something CI can enforce:
 *
 *   - `docs/adr/0003-settings-namespace.md` — the policy (unify by default,
 *     one key both games read, converge MM onto OoT).
 *   - `docs/enhancement-classification.md` — the per-setting inventory, which
 *     classified at individual-setting level with per-key verification.
 *     **Where the two disagree, the inventory wins**, because it verified each
 *     key's type, range, and implementation rather than its name.
 *
 * `--test cvar-classification` (src/common/tests/test_cvar_classification.c)
 * fails the build when the tree drifts away from the tables below.
 *
 * The three settled premises, restated so nobody has to go find the ADR:
 *
 *   1. Settings are UNIFIED BY DEFAULT. "In general, this is conceptually one
 *      game. If something applying to both makes sense, it should."
 *   2. One CVar, read directly by both ports. No fan-out layer, no per-game
 *      shadow keys, no gCore/gOoT/gMM partition.
 *   3. Where both games mean the same thing under different key names, MM
 *      moves to OoT's key. SoH is the host port owning the config.
 *
 * ============================================================================
 * READ THIS BEFORE "FIXING" A SHARED KEY
 * ============================================================================
 *
 * A key read by both games is NOT a bug by itself. The inventory measured 35
 * exact collisions and found 33 of them CORRECT AND DELIBERATE. The five
 * `gCheats.*` keys below are the ones most likely to be mistaken for a
 * collision defect: they are shared on purpose, and the inventory verified
 * per-key that the two implementations genuinely match. Turning on infinite
 * health is a statement about how the user wants to play RSBS, not about
 * which of the two games happens to be in front. Namespacing them would be a
 * regression, and this lock will fail if you try.
 *
 * ============================================================================
 * AND READ THIS BEFORE CONVERGING A KEY BECAUSE THE NAMES LOOK EQUIVALENT
 * ============================================================================
 *
 * Names are not semantics. `kMustStayDistinct` below holds pairs that read as
 * obvious renames and are not — merging them produces a control that silently
 * does nothing, or drops a setting on one side. The canonical example is text
 * speed: OoT's is a 1-5x integer multiplier, MM's is a boolean, and writing
 * MM's `1` into OoT's key means "1x = no speedup". The toggle would appear in
 * the menu, respond to clicks, persist its value, and have no effect.
 */

#ifndef RSBS_CVAR_SHARED_KEYS_H
#define RSBS_CVAR_SHARED_KEYS_H

/* ==========================================================================
 * Converged keys — canonical OoT spellings.
 *
 * MM used to spell these differently, so the setting silently did not cross
 * the game boundary. The RSBS_CVAR_LEGACY_* spellings are what MM read before
 * this change; they are retired and must not reappear anywhere in games/.
 * ========================================================================== */

/* Audio volume. OoT stores these as INTEGER PERCENT (0-100), not float 0..1 —
 * see the representation note below, which is the correction this work made to
 * ADR 0003 §5.1. */
#define RSBS_CVAR_VOLUME_MASTER "gSettings.Volume.Master"
#define RSBS_CVAR_VOLUME_MAIN_MUSIC "gSettings.Volume.MainMusic"
#define RSBS_CVAR_VOLUME_SUB_MUSIC "gSettings.Volume.SubMusic"
#define RSBS_CVAR_VOLUME_SFX "gSettings.Volume.SFX"
#define RSBS_CVAR_VOLUME_FANFARE "gSettings.Volume.Fanfare"
/* No OoT twin: OoT has no ambience channel. Spelled OoT-style anyway so the
 * family stays consistent and OoT can adopt the key if it ever gains one. */
#define RSBS_CVAR_VOLUME_AMBIENCE "gSettings.Volume.Ambience"

#define RSBS_CVAR_LEGACY_VOLUME_MASTER "gSettings.Audio.MasterVolume"
#define RSBS_CVAR_LEGACY_VOLUME_MAIN_MUSIC "gSettings.Audio.MainMusicVolume"
#define RSBS_CVAR_LEGACY_VOLUME_SUB_MUSIC "gSettings.Audio.SubMusicVolume"
#define RSBS_CVAR_LEGACY_VOLUME_SFX "gSettings.Audio.SoundEffectsVolume"
#define RSBS_CVAR_LEGACY_VOLUME_FANFARE "gSettings.Audio.FanfareVolume"
#define RSBS_CVAR_LEGACY_VOLUME_AMBIENCE "gSettings.Audio.AmbienceVolume"

/* Character colour — issue #453, inventory §3.3 BUG 1 / §4 row P4.
 *
 * Not a collision: a STALE-KEY DESYNC. OoT reads the dot form and its own v3
 * migrator actively renames the underscore form away, so the key MM reads is
 * one OoT has already migrated out of existence. After any OoT config
 * migration runs, MM's tunic colours read a key nothing writes and fall back
 * to hardcoded defaults permanently, with no UI to correct it. MM applies
 * these to the Deku/Goron/Zora forms where OoT applies them to the tunics:
 * different garment, same user intent. Converge as a FIX, not a plain rename. */
#define RSBS_CVAR_COLOR_KOKIRI_TUNIC "gCosmetics.Link.KokiriTunic.Value"
#define RSBS_CVAR_COLOR_GORON_TUNIC "gCosmetics.Link.GoronTunic.Value"
#define RSBS_CVAR_COLOR_ZORA_TUNIC "gCosmetics.Link.ZoraTunic.Value"

#define RSBS_CVAR_LEGACY_COLOR_KOKIRI_TUNIC "gCosmetics.Link_KokiriTunic.Value"
#define RSBS_CVAR_LEGACY_COLOR_GORON_TUNIC "gCosmetics.Link_GoronTunic.Value"
#define RSBS_CVAR_LEGACY_COLOR_ZORA_TUNIC "gCosmetics.Link_ZoraTunic.Value"

/* ==========================================================================
 * OoT's defaults for the converged volume family.
 *
 * These are the values OoT's menu and read sites already use
 * (SohMenuSettings.cpp, audio_playback.c, audioMgr.c). MM adopts them: a
 * converged setting has to have ONE default too, or a fresh config still
 * sounds different in the two games.
 * ========================================================================== */
#define RSBS_VOLUME_DEFAULT_MASTER 40
#define RSBS_VOLUME_DEFAULT_OTHER 100

/* Percent -> linear scale, the expression OoT's own read sites use. */
#define RSBS_VOLUME_SCALE(percent) ((float)(percent) / 100.0f)

#ifdef __cplusplus

#include <cstddef>

namespace RSBS {

/** One converged setting: the retired MM spelling and the canonical OoT one. */
struct ConvergedKey {
    const char* legacy;    ///< MM's pre-convergence spelling. Retired.
    const char* canonical; ///< OoT's spelling. The single source of truth.
    /// True when the two sides also disagreed about REPRESENTATION, not just
    /// spelling — OoT stores integer percent, MM stored float 0..1. libultraship
    /// returns the DEFAULT on a CVar type mismatch, so a plain CVarCopy across
    /// one of these is a type-punned no-op: the version-7 updater must convert
    /// rather than rename, and MM's read sites must read integers.
    bool scaledPercent;
};

/**
 * The convergence set landed by this work. The single table the version-7
 * config updater and the 2Ship importer both consume, so the two cannot drift.
 *
 * SCOPE NOTE — this is not the whole convergence backlog. ADR 0003 §5.1 scoped
 * "tier 1" to these 9 keys; ADR 0004's inventory §5.3 independently found 26
 * more convergence rows. The two sets are very nearly DISJOINT, not nested:
 * the inventory classified MM's `gEnhancements.*`/`gCheats.*` categories and
 * the 35-key collision set, while the audio-volume family below is in neither
 * (MM and OoT spell it differently, so it is not a collision, and it is not an
 * enhancement). They intersect only on the three tunic keys. The inventory's
 * 26 rows are tracked separately and are NOT attempted here — four of them are
 * parked on unanswered taxonomy questions (see kMustStayDistinct).
 */
inline constexpr ConvergedKey kConvergedKeys[] = {
    { RSBS_CVAR_LEGACY_VOLUME_MASTER, RSBS_CVAR_VOLUME_MASTER, true },
    { RSBS_CVAR_LEGACY_VOLUME_MAIN_MUSIC, RSBS_CVAR_VOLUME_MAIN_MUSIC, true },
    { RSBS_CVAR_LEGACY_VOLUME_SUB_MUSIC, RSBS_CVAR_VOLUME_SUB_MUSIC, true },
    { RSBS_CVAR_LEGACY_VOLUME_SFX, RSBS_CVAR_VOLUME_SFX, true },
    { RSBS_CVAR_LEGACY_VOLUME_FANFARE, RSBS_CVAR_VOLUME_FANFARE, true },
    { RSBS_CVAR_LEGACY_VOLUME_AMBIENCE, RSBS_CVAR_VOLUME_AMBIENCE, true },
    { RSBS_CVAR_LEGACY_COLOR_KOKIRI_TUNIC, RSBS_CVAR_COLOR_KOKIRI_TUNIC, false },
    { RSBS_CVAR_LEGACY_COLOR_GORON_TUNIC, RSBS_CVAR_COLOR_GORON_TUNIC, false },
    { RSBS_CVAR_LEGACY_COLOR_ZORA_TUNIC, RSBS_CVAR_COLOR_ZORA_TUNIC, false },
};

/**
 * Key prefixes a converged family owns. Once a family converges, a NEW sibling
 * appearing on MM's abandoned prefix is the same defect all over again, so the
 * lock rejects the whole prefix rather than only the literals we happened to
 * fix.
 */
inline constexpr const char* kRetiredKeyPrefixes[] = {
    "gSettings.Audio.",
    "gCosmetics.Link_",
};

/**
 * A pair the lock must keep APART.
 *
 * These look like rename candidates and are not. The test asserts MM's key is
 * still read somewhere under `games/mm/`; converging it makes the literal
 * vanish and turns this table into a red build that names the row and the
 * reason. That is the whole point — the failure mode being prevented
 * (a control that persists a value and has no effect) is invisible at runtime.
 */
struct DistinctPair {
    const char* row;    ///< Inventory row id, for the failure message.
    const char* mmKey;  ///< MM's key. Must remain present in games/mm/.
    const char* ootKey; ///< OoT's counterpart, for context in the message.
    const char* why;    ///< Why merging is wrong. Printed on failure.
};

/**
 * Inventory §4 rows (P1-P7, minus the ones with no single representative key)
 * plus §5.3's four PARKED rows, which are real convergence candidates blocked
 * on maintainer answers rather than settled non-merges.
 */
inline constexpr DistinctPair kMustStayDistinct[] = {
    { "P1 text speed", "gEnhancements.Dialogue.FastText", "gEnhancements.TextSpeed",
      "Units are incompatible: OoT is a 1-5x integer multiplier, MM is a boolean. Merging writes 1 = "
      "'no speedup', so the control silently does nothing. MM's FastText also bundles hold-B-to-advance, "
      "which OoT exposes as the SEPARATE key gEnhancements.SkipText — one control cannot express both "
      "decompositions." },
    { "P2 infinite sword glitch", "gEnhancements.Restorations.TatlISG", "gCheats.EasyISG",
      "Taxonomy conflict: OoT treats ISG as a cheat to grant, MM treats restoring Navi-ISG via Tatl as "
      "authenticity. Different menu sections, different framing. Needs a maintainer call." },
    { "P3 camera axis decomposition", "gEnhancements.Camera.FirstPerson.InvertX",
      "gSettings.Controls.InvertAimingXAxis",
      "Different axis decomposition: OoT splits by context (free-look / aiming / shield / Z-aim), MM by "
      "input device (right stick / gyro / first-person). No 1:1 mapping — OoT has no gyro axis, MM has no "
      "shield-aim axis. Merging drops settings on both sides." },
    { "P6 ocarina d-pad input", "gEnhancements.Playback.DpadOcarina", "gSettings.OcarinaControl.Dpad",
      "Split across different top-level namespaces with different member sets: MM adds right-stick "
      "ocarina, OoT adds a custom-mapping layer. Structurally parallel, not identical." },
    { "P7 shield aim inversion", "gEnhancements.Equipment.InvertShieldY",
      "gSettings.Controls.InvertShieldAimingYAxis",
      "Different namespace AND different axis coverage: OoT has X+Y, MM has Y only. Merging loses OoT's "
      "X axis." },
    { "Autosave interval (inventory §5.3 group A caveat)", "gEnhancements.Saving.AutosaveInterval",
      "(none — OoT hardcodes THREE_MINUTES_IN_UNIX)",
      "The Autosave ENABLE flag is genuinely shared-intent, but OoT's interval is hardcoded with no CVar. "
      "The interval is MM-only and must NOT ride along with an Autosave rename." },
    { "PARKED: infinite ammo scope", "gCheats.InfiniteConsumables", "gCheats.InfiniteAmmo",
      "MM's 'consumables' scope may be wider than OoT's 'ammo'. Pending a maintainer answer; do not "
      "converge until it lands." },
    { "PARKED: item restriction gate", "gCheats.UnrestrictedItems", "gCheats.NoRestrictItems",
      "Same intent (drop item gating) but DIFFERENT GATE: OoT ignores age restrictions, MM lets all forms "
      "use all items. Pending a maintainer answer; do not converge until it lands." },
};

/**
 * Casing hazard, inventory §5.1. MM uses `gEnhancements.Timesavers.*`
 * (lowercase s, 9 keys), OoT uses `gEnhancements.TimeSavers.*` (capital S, 16
 * keys). They do NOT collide, but they are one keystroke apart in a
 * case-sensitive store. Any rename touching this family must be explicit about
 * which spelling wins; until that is decided, MM's spelling stays put and the
 * lock asserts it.
 */
inline constexpr const char* kParkedCasingPrefixMM = "gEnhancements.Timesavers.";

/**
 * Inventory §3.1 + §3.2: shared spelling, shared meaning, KEEP AS IS. Zero
 * migration. This list exists so that "these are deliberate" is a thing the
 * build asserts rather than a thing a comment claims.
 *
 * `gInputViewer.` is a shared macro PREFIX — CVAR_INPUT_VIEWER(var) is defined
 * identically in both trees, so its ~60 keys collide by construction and count
 * as one entry.
 */
inline constexpr const char* kSharedIntentKeys[] = {
    // Cheats. Deliberate, and verified per-key by the inventory. See the
    // banner at the top of this file.
    "gCheats.EasyFrameAdvance",
    "gCheats.InfiniteHealth",
    "gCheats.InfiniteMagic",
    "gCheats.MoonJumpOnL",
    "gCheats.NoClip",
    // Developer tooling. NOTE: gDeveloperTools.DebugSaveFileMode is
    // deliberately ABSENT — see kDisputedClassificationKeys.
    "gDeveloperTools.DebugEnabled",
    "gDeveloperTools.FrameAdvanceTick",
    "gDeveloperTools.LogLevel",
    // Graphics enhancements that describe the renderer, not the game.
    "gEnhancements.Graphics.IncreaseActorDrawDistance",
    "gEnhancements.Graphics.ActorCullingAccountsForWidescreen",
    // Audio editor behaviour.
    "gAudioEditor.EnemyBGMDisable",
    "gAudioEditor.LowHpAlarm",
    "gAudioEditor.SeqNameNotification",
    "gAudioEditor.SeqNameNotificationDuration",
    // Port-level: one SDL window, one input stack, one menu shell, one Gui.
    "gSettings.CursorVisibility",
    "gSettings.DisableChanges",
    "gSettings.Menu.Theme",
    "gSettings.Menu.Popout",
    "gSettings.Menu.PoppedWidth",
    "gSettings.Menu.PoppedHeight",
    "gSettings.Menu.PoppedPos.x",
    "gSettings.Menu.PoppedPos.y",
    "gSettings.Menu.SearchAutofocus",
    "gSettings.Menu.SidebarSearch",
    // Shared macro prefix, identical in both trees.
    "gInputViewer.",
};

/**
 * The five cheat keys, called out separately so the lock can assert they are
 * still read by MM under the SHARED spelling. If a future change namespaces
 * MM's cheats, this is the assertion that names the mistake.
 */
inline constexpr const char* kDeliberateSharedCheatKeys[] = {
    "gCheats.EasyFrameAdvance", "gCheats.InfiniteHealth", "gCheats.InfiniteMagic",
    "gCheats.MoonJumpOnL",      "gCheats.NoClip",
};

/**
 * Keys the two governing documents classify DIFFERENTLY. Recorded, not
 * reconciled, so neither document silently overrides the other — and asserted
 * on by nothing, because asserting either reading would prejudge the issue.
 *
 * `gDeveloperTools.DebugSaveFileMode`: ADR 0003 §4.1 calls it (S) — "value
 * spaces align, only the unwritten fallback differs, acceptable". The
 * inventory §3.3 BUG 2 takes the more conservative (P) line: the DEFAULTS
 * disagree (OoT 1 = "vanilla debug save", MM 0 = "empty save"), so the first
 * game to write the key silently changes the other's debug-save behaviour away
 * from its own default. No rename either way, so the disagreement is narrow
 * and safe. **Issue #454 decides it.** Do not converge, split, or re-default
 * this key as a side effect of other work.
 */
inline constexpr const char* kDisputedClassificationKeys[] = {
    "gDeveloperTools.DebugSaveFileMode",
};

/**
 * Menu-index keys: shared spelling, and the value names or indexes into a
 * menu. **Issue #451 decides these.**
 *
 * ADR 0003 §4.2 classifies them (P) — OoT's SohMenu and MM's BenMenu have
 * different section sets, so one game's persisted index selects an unrelated
 * or out-of-range entry in the other. ADR 0004 §3.1 argues the opposite for
 * the same keys: under its single-shell decision there is only ONE menu, so
 * sharing is correct — with an explicit caveat that this holds *only* while MM
 * has no second shell.
 *
 * The lock does not pick a winner. It asserts the one thing true under BOTH
 * readings: these keys are never swept into the convergence set. They are not
 * renamed, not merged with a differently-spelled MM key, and not touched as a
 * side effect of convergence work.
 */
inline constexpr const char* kMenuIndexKeys[] = {
    "gSettings.Menu.ActiveHeader",
    "gSettings.Menu.SettingsSidebarSection",
    "gSettings.Menu.EnhancementsSidebarSection",
    "gSettings.Menu.DevToolsSidebarSection",
};

inline constexpr std::size_t kConvergedKeyCount = sizeof(kConvergedKeys) / sizeof(kConvergedKeys[0]);
inline constexpr std::size_t kRetiredKeyPrefixCount = sizeof(kRetiredKeyPrefixes) / sizeof(kRetiredKeyPrefixes[0]);
inline constexpr std::size_t kMustStayDistinctCount = sizeof(kMustStayDistinct) / sizeof(kMustStayDistinct[0]);
inline constexpr std::size_t kSharedIntentKeyCount = sizeof(kSharedIntentKeys) / sizeof(kSharedIntentKeys[0]);
inline constexpr std::size_t kDeliberateSharedCheatKeyCount =
    sizeof(kDeliberateSharedCheatKeys) / sizeof(kDeliberateSharedCheatKeys[0]);
inline constexpr std::size_t kDisputedClassificationKeyCount =
    sizeof(kDisputedClassificationKeys) / sizeof(kDisputedClassificationKeys[0]);
inline constexpr std::size_t kMenuIndexKeyCount = sizeof(kMenuIndexKeys) / sizeof(kMenuIndexKeys[0]);

// ADR 0003 §5.1 summarises tier 1 as "8 renames across 9 literal sites"; its
// own sub-counts say 6 audio + 3 colour. Both describe this 9-entry table: 8 of
// the 9 land on a key OoT already reads, and the 9th (Volume.Ambience) is an
// adoption rather than a rename because OoT has no ambience channel to collide
// with. Nine keys either way.
static_assert(kConvergedKeyCount == 9, "tier 1 converges 9 keys (8 onto an existing OoT key + Ambience)");

// ADR 0003 Appendix B measured 26 class-(S) collisions. This table carries 25:
// DebugSaveFileMode was pulled out into kDisputedClassificationKeys because the
// inventory reclassified it (P) and #454 has not settled it. Pinning the count
// makes a silent drop a compile error rather than a quietly weaker lock.
static_assert(kSharedIntentKeyCount == 25,
              "ADR 0003 Appendix B's 26 class-(S) keys, less DebugSaveFileMode (disputed, #454)");
static_assert(kMenuIndexKeyCount == 4, "four menu-index keys — #451");

} // namespace RSBS

#endif // __cplusplus

#endif // RSBS_CVAR_SHARED_KEYS_H
