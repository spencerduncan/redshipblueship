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
// W6-AUDIO-SFX (#1239): MM's two sound mutes with no SoH twin, drawn as MM rows in SoH's Audio Editor (Audio Options).
#define RSBS_CVAR_MM_MUTE_CARPENTER_SFX "gAudioEditor.MuteCarpenterSfx"
#define RSBS_CVAR_MM_MUTE_GORON_CHILD_CRY "gAudioEditor.ChildGoronCry"

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

/* ==========================================================================
 * Tier-4 combo-level keys (ADR 0004 §3 tier 4; ADR 0011 increment 2).
 *
 * The `gCombo.` namespace is the combo's own. Keys here describe the PAIR,
 * belong to neither game's save by construction, and have no upstream
 * counterpart: ADR 0003 names them "new tier-4 keys, not converged MM keys",
 * so none of them is a ConvergedKey row and none has a legacy spelling.
 *
 * The three `gCombo.Rando.*` keys are WORLD IDENTITY (ADR 0004 §6 state 4):
 * they author a ComboSettingsRecord up to the creation event and no further.
 * Combo_ResolveComboSettings (foreign_items.h) is the ONE reader that turns
 * them into the record, through combo_settings_view.h, which also holds the
 * ONE writer surface — and that surface refuses once
 * Combo_ComboSettingsFrozen() is true. Values are the pinned RSBS_COMBO_DIR_*
 * / RSBS_COMBO_GOAL_* / 0-or-1 spaces; an
 * out-of-space value in the store resolves to the shipped default with a logged
 * reason, never to a new enumerator.
 *
 * The classification itself lives in RSBS::kComboKeys below, and the
 * cvar-classification lock scans the whole tree for `gCombo.` literals: a new
 * key in this namespace that is not classified there is a red build naming
 * the key and the file, because an unclassified key defaults to identity and
 * guessing "preference" for an identity key is the failure ADR 0004 §6 state
 * 4 exists to prevent.
 * ========================================================================== */
#define RSBS_CVAR_COMBO_RANDO_DIRECTION "gCombo.Rando.Direction"
/* #801 retired the two pool-size keys (gCombo.Rando.PoolSize.OoT / .MM, the
 * "Max OoT Items" / "Max MM Items" rows): under the single bag no rule reads a
 * pool size, and a moved slider only re-seeded the world. The record fields
 * stay (format and identity); every new world writes the shipped default.
 * #834 retired the two item-class keys (gCombo.Rando.ItemClass.OoT / .MM, the
 * "OoT Classes" / "MM Classes" rows) for the same reason: under the single bag
 * every bag row is progression, so no rule reads the other class bits and the
 * PROGRESSION bit only repeats the direction. The record fields stay too.
 * #879 retired #668's shared-ocarina key (gCombo.Rando.SharedOcarina, the
 * "[Both Games] Shared Ocarina" row): MM starts every paired world with its
 * ocarina, so the rule only handed OoT a free one. Its comboFlags bit stays
 * reserved in the record; nothing sets it and nothing reads it. */
/* ADR 0010 D1's GOAL (its working name, kept): one of the pinned
 * RSBS_COMBO_GOAL_* enumerators, DEFAULT beat-both (1), so a player who leaves
 * it alone resolves to the same twelve record bytes as before it existed. */
#define RSBS_CVAR_COMBO_RANDO_GOAL "gCombo.Rando.Goal"
/* #886: one Kokiri Sword across both games. Boolean (0/1), DEFAULT 0, so every
 * world created before it existed, and every world whose player leaves it alone,
 * resolves to the same twelve record bytes and the same comboSettingsHash. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_KOKIRI_SWORD "gCombo.Rando.SharedKokiriSword"
/* #1068: one Hylian/Hero's Shield and one Mirror Shield across both games
 * (OoTMM sharedShields). Boolean (0/1), DEFAULT 0: a BIT of the item rules
 * record, so with it off the record stays all clear and every fingerprint holds. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_SHIELDS "gCombo.Rando.SharedShields"
/* #1065: one Lens of Truth across both games (OoTMM sharedLens). Boolean (0/1),
 * DEFAULT 0. A bit of the sibling ComboItemRulesRecord's crossItemFlags
 * (RSBS_XITEM_SHARED_LENS), folded into comboSettingsHash only when on, so a
 * player who leaves it alone keeps every world's identity. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_LENS "gCombo.Rando.SharedLens"
/* #1066: one Epona's Song and one Song of Storms across both games (OoTMM
 * sharedSongEpona, sharedSongStorms). Boolean (0/1), DEFAULT 0 each. Bits of the
 * same crossItemFlags (RSBS_XITEM_SHARED_SONG_EPONA / _STORMS), folded into
 * comboSettingsHash only when on. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_SONG_EPONA "gCombo.Rando.SharedSongEpona"
#define RSBS_CVAR_COMBO_RANDO_SHARED_SONG_STORMS "gCombo.Rando.SharedSongStorms"
/* #1067: each game's five ocarina buttons are one set for the pair (OoTMM
 * sharedOcarinaButtons). Boolean (0/1), DEFAULT 0: a BIT of the item rules
 * record, so with it off the record stays all clear and every fingerprint holds. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_OCARINA_BUTTONS "gCombo.Rando.SharedOcarinaButtons"
/* #1069: one Song of Time across both games (OoTMM sharedSongTime). Boolean
 * (0/1), DEFAULT 0. A bit of the same crossItemFlags (RSBS_XITEM_SHARED_SONG_TIME),
 * folded into comboSettingsHash only when on. On, MM's paired start drops the
 * song and MM's moon crash is a New Cycle. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_SONG_TIME "gCombo.Rando.SharedSongTime"
/* #968: one Fire, one Ice and one Light Arrows across both games (OoTMM
 * sharedMagicArrowFire, sharedMagicArrowIce, sharedMagicArrowLight). Boolean
 * (0/1), DEFAULT 0 each. Bits of the same crossItemFlags
 * (RSBS_XITEM_SHARED_ARROW_FIRE / _ICE / _LIGHT, reserved by #976), folded into
 * comboSettingsHash only when on. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_FIRE "gCombo.Rando.SharedArrowFire"
#define RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_ICE "gCombo.Rando.SharedArrowIce"
#define RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_LIGHT "gCombo.Rando.SharedArrowLight"
/* #970: the bottle COUNT is one quantity for the pair up to OoT's four slots, the
 * contents stay each game's own. A deliberate divergence from OoTMM's sharedBottles
 * (six bottles, contents and refills shared, needs extraBottlesOot's six OoT
 * slots): see PR #1094. Boolean (0/1), DEFAULT 0:
 * a BIT of the item rules record, so with it off the record stays all clear and
 * every fingerprint holds. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES "gCombo.Rando.SharedBottles"
/* #978 (epic #975 P2): one Goron Mask, Zora Mask, Bunny Hood, Keaton Mask and
 * Mask of Truth across both games (OoTMM sharedMaskGoron, sharedMaskZora,
 * sharedMaskBunny, sharedMaskKeaton, sharedMaskTruth). Boolean (0/1), DEFAULT 0
 * each. Bits of the same crossItemFlags (RSBS_XITEM_SHARED_GORON ... _TRUTH,
 * reserved by #976), folded into comboSettingsHash only when on. #969's Bunny
 * Hood speed in Hyrule rides the Bunny Hood's bit. */
#define RSBS_CVAR_COMBO_RANDO_SHARED_MASK_GORON "gCombo.Rando.SharedMaskGoron"
#define RSBS_CVAR_COMBO_RANDO_SHARED_MASK_ZORA "gCombo.Rando.SharedMaskZora"
#define RSBS_CVAR_COMBO_RANDO_SHARED_MASK_BUNNY "gCombo.Rando.SharedMaskBunny"
#define RSBS_CVAR_COMBO_RANDO_SHARED_MASK_KEATON "gCombo.Rando.SharedMaskKeaton"
#define RSBS_CVAR_COMBO_RANDO_SHARED_MASK_TRUTH "gCombo.Rando.SharedMaskTruth"

/* The common-owned windows' visibility toggles (ADR 0008). PREFERENCE keys:
 * no identity role, live forever. Spelled here so the tier-4 manifest carries
 * the whole namespace and not just its identity half; the two older windows'
 * headers still spell their own literal, which the lock verifies against this
 * table. The MM randomizer options window's key (gCombo.Windows.MMOptions)
 * left with that window on 2026-09-27, when the options became Combo menu
 * pages: nothing reads it, so it is no longer classified or spelled. A config
 * that still holds it keeps an inert entry. */
#define RSBS_CVAR_COMBO_WINDOW_SPOILER "gCombo.Windows.Spoiler"
/* #1132: the Check Tracker and Item Tracker windows answer SoH's own window keys
 * (gOpenWindows.CheckTracker / .ItemTracker) since they replace SoH's, so
 * nothing reads this key or RSBS_CVAR_COMBO_WINDOW_ITEM_TRACKER below any more.
 * Both stay classified, so a config that holds them keeps a classified, inert
 * entry. */
#define RSBS_CVAR_COMBO_WINDOW_TRACKER "gCombo.Windows.Tracker"
#define RSBS_CVAR_COMBO_WINDOW_COMBO_SETTINGS "gCombo.Windows.ComboSettings"
/* #458 U0: the unified Item Tracker overlay's visibility (one overlay for both
 * games, live for the active one and the frozen snapshot for the other). The
 * window lands with #458 U2 and reads this define, as the Combo Settings pane
 * reads its own; the key is spelled and classified first so the overlay cannot
 * arrive with an unclassified key. */
#define RSBS_CVAR_COMBO_WINDOW_ITEM_TRACKER "gCombo.Windows.ItemTracker"
/* #458 U2: the unified Item Tracker overlay's own settings, the ones SoH's Item
 * Tracker has no key for: since #1141 (operator ruling IT-D6) the overlay reads
 * SoH's gTrackers.ItemTracker.* keys for everything else (Window Type, dragging,
 * paused-only, background colour, icon size, ...; ComboReadItemTrackerKeys), and
 * these are rows appended to SoH's Randomizer > Item Tracker page
 * (SohMenuComboTrackerSettings.cpp). The five twins this block held until then
 * are RSBS::kComboRetiredKeys, carried into SoH's keys once. PREFERENCE keys.
 *   Opacity         float 0.1..1, the whole overlay's alpha
 *   Section.*       0/1 each, default 1: which sections the overlay draws */
#define RSBS_CVAR_COMBO_ITEMS_OPACITY "gCombo.Tracker.Items.Opacity"
#define RSBS_CVAR_COMBO_ITEMS_SECTION_OOT "gCombo.Tracker.Items.Section.OoT"
#define RSBS_CVAR_COMBO_ITEMS_SECTION_MM "gCombo.Tracker.Items.Section.MM"
#define RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED "gCombo.Tracker.Items.Section.Shared"
/* #1141: one key per Majora's Mask group the overlay can hide, 0/1, default 1,
 * named after MM's own group (the MM item adapter's group string without its
 * spaces; Combo_ItemGroupVisibilityKey maps them). Not under Section.MM, which
 * holds a value of its own and so cannot also be a block of keys. PREFERENCE. */
#define RSBS_CVAR_COMBO_ITEMS_MM_TOKENS "gCombo.Tracker.Items.MM.Tokens"
#define RSBS_CVAR_COMBO_ITEMS_MM_STRAY_FAIRIES "gCombo.Tracker.Items.MM.StrayFairies"
#define RSBS_CVAR_COMBO_ITEMS_MM_DUNGEON "gCombo.Tracker.Items.MM.Dungeon"
#define RSBS_CVAR_COMBO_ITEMS_MM_BOSS_SOULS "gCombo.Tracker.Items.MM.BossSouls"
#define RSBS_CVAR_COMBO_ITEMS_MM_ENEMY_SOULS "gCombo.Tracker.Items.MM.EnemySouls"
#define RSBS_CVAR_COMBO_ITEMS_MM_OWL_STATUES "gCombo.Tracker.Items.MM.OwlStatues"
#define RSBS_CVAR_COMBO_ITEMS_MM_TIME "gCombo.Tracker.Items.MM.Time"
#define RSBS_CVAR_COMBO_ITEMS_MM_MISC "gCombo.Tracker.Items.MM.Misc"
/* #1131: the Combo Tracker's own keys, the two settings SoH's Check Tracker has
 * no key for (MM's tracker has both: gRando.CheckTracker.ScrollToCurrentScene
 * and .ShowCurrentScene), one each for both panes (ruling D3), written by the
 * Combo > Tracker Settings page under its Check Tracker Settings separator.
 * PREFERENCE keys.
 *   AutoScrollToCurrentArea  0/1, default 1: the active game's pane scrolls to
 *                            the player's area when it changes (SoH's behaviour)
 *   OnlyShowCurrentArea      0/1, default 0: seeds each pane's own Only Show
 *                            Current Area toggle (ruling D12) when it changes */
#define RSBS_CVAR_COMBO_CHECKS_AUTO_SCROLL "gCombo.Tracker.Checks.AutoScrollToCurrentArea"
#define RSBS_CVAR_COMBO_CHECKS_ONLY_CURRENT_AREA "gCombo.Tracker.Checks.OnlyShowCurrentArea"

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>

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
 * The convergence set. The single table the config version updaters and the
 * 2Ship importer all consume, so they cannot drift.
 *
 * SCOPE — this table now carries BOTH waves of convergence:
 *
 *   - Tier 1 (#34 / PR #461): the 6 audio-volume keys and the 3 tunic-colour
 *     keys. The volume rows carry scaledPercent=true because OoT stores integer
 *     percent while MM stored a float scale; those need a value transform, not
 *     just a rename (see the representation note above). Migrated by
 *     ConfigVersion7Updater.
 *   - The inventory §5.3 remainder (#462): 23 of its 26 rows. All pure renames —
 *     each was re-verified against origin/main to agree on TYPE, UNITS, RANGE
 *     and DEFAULT on both sides before it was treated as mechanical, because a
 *     key-only swap across a representation gap is a silent no-op, not a fix
 *     (the trap the volume rows hit). Migrated by ConfigVersion8Updater.
 *
 * The 3 unlanded §5.3 rows did NOT pass that verification and are held out:
 * ClimbSpeed (OoT additive "+N" 0-12 vs MM multiplier 1-5×, incompatible units
 * and neutral value) plus the two parked cheat pairs (scope/gate undecided) —
 * all three now sit in kMustStayDistinct so a future plain rename fails CI.
 */
inline constexpr ConvergedKey kConvergedKeys[] = {
    { RSBS_CVAR_LEGACY_VOLUME_MASTER, RSBS_CVAR_VOLUME_MASTER, true },
    { RSBS_CVAR_LEGACY_VOLUME_MAIN_MUSIC, RSBS_CVAR_VOLUME_MAIN_MUSIC, true },
    { RSBS_CVAR_LEGACY_VOLUME_SUB_MUSIC, RSBS_CVAR_VOLUME_SUB_MUSIC, true },
    { RSBS_CVAR_LEGACY_VOLUME_SFX, RSBS_CVAR_VOLUME_SFX, true },
    { RSBS_CVAR_LEGACY_VOLUME_FANFARE, RSBS_CVAR_VOLUME_FANFARE, true },
    { RSBS_CVAR_LEGACY_VOLUME_AMBIENCE, RSBS_CVAR_VOLUME_AMBIENCE, true },
    // #1239 (beside the other audio rows; migrated by ConfigVersion9Updater): the companion fairy's call. SoH's
    // "Disable Navi Call Audio" (Audio Editor > Audio Options) and MM's "Disable Tatl Call Audio" are both bool,
    // default off, and both skip the fairy's call sound (OoT z_parameter.c, MM VB_PLAY_TATL_CALL_AUDIO at
    // z_parameter.c:5094), so SoH's one row drives both games.
    { "gAudioEditor.DisableTatlCallAudio", "gAudioEditor.DisableNaviCallAudio", false },
    { RSBS_CVAR_LEGACY_COLOR_KOKIRI_TUNIC, RSBS_CVAR_COLOR_KOKIRI_TUNIC, false },
    { RSBS_CVAR_LEGACY_COLOR_GORON_TUNIC, RSBS_CVAR_COLOR_GORON_TUNIC, false },
    { RSBS_CVAR_LEGACY_COLOR_ZORA_TUNIC, RSBS_CVAR_COLOR_ZORA_TUNIC, false },

    // ---- Inventory §5.3 remainder (#462), ConfigVersion8Updater. ----
    // All pure renames (scaledPercent=false): representation re-verified matching
    // on both sides. MM's read sites and OoT's already agree on int/bool type and
    // default, so version8Migrations copies the stored value unchanged.

    // Group A — identical leaf, MM adds a category prefix. (ClimbSpeed is the
    // 12th Group A row and is deliberately absent: it is in kMustStayDistinct
    // because OoT reads it as an additive "+0..12" offset and MM as a "1..5x"
    // multiplier — same name, incompatible units.)
    { "gEnhancements.Timesavers.FastChests", "gEnhancements.FastChests", false },
    { "gEnhancements.Graphics.BowReticle", "gEnhancements.BowReticle", false },
    { "gEnhancements.Graphics.DisableBlackBars", "gEnhancements.DisableBlackBars", false },
    { "gEnhancements.Player.InstantPutaway", "gEnhancements.InstantPutaway", false },
    { "gEnhancements.Equipment.ItemUnequip", "gEnhancements.ItemUnequip", false },
    // The Autosave ENABLE flag converges; the MM-only interval slider
    // (gEnhancements.Saving.AutosaveInterval) does NOT ride along — it is in
    // kMustStayDistinct because OoT hardcodes its interval with no CVar.
    { "gEnhancements.Saving.Autosave", "gEnhancements.Autosave", false },
    { "gEnhancements.Saving.RememberSaveLocation", "gEnhancements.RememberSaveLocation", false },
    { "gEnhancements.Dpad.DpadEquips", "gEnhancements.DpadEquips", false },
    { "gEnhancements.Restorations.PauseBufferWindow", "gEnhancements.PauseBufferWindow", false },
    { "gEnhancements.DifficultyOptions.HyperEnemies", "gEnhancements.HyperEnemies", false },
    // Top-level namespace change too: MM gEnhancements.Mods -> OoT gSettings.Mods.
    { "gEnhancements.Mods.AlternateAssetsHotkey", "gSettings.Mods.AlternateAssetsHotkey", false },

    // Group B — wording drift, same meaning. The two parked cheat pairs
    // (InfiniteConsumables/InfiniteAmmo, UnrestrictedItems/NoRestrictItems) are
    // NOT here — their scope/gate is undecided, so they stay in kMustStayDistinct.
    { "gCheats.ClimbAnywhere", "gCheats.ClimbEverything", false },
    { "gCheats.HookshotAnywhere", "gCheats.HookshotEverything", false },
    { "gCheats.InfiniteRupees", "gCheats.InfiniteMoney", false },
    { "gEnhancements.Graphics.EnemyHealthBars", "gEnhancements.EnemyHealthBar", false },
    { "gEnhancements.Cutscenes.HideTitleCards", "gEnhancements.TimeSavers.DisableTitleCard", false },

    // Group C — cutscene-skip family. OoT groups under TimeSavers.SkipCutscene.*,
    // MM under Cutscenes.Skip*Cutscenes. Both bool. (OoT defaults these to
    // IS_RANDO and MM to 0; that per-reader default difference is inherent to a
    // shared key and pre-dates convergence — it is not a type mismatch.)
    // SkipEnemyCutscenes joined in #1123 (migrated by ConfigVersion9Updater, not 8):
    // MM's one checkbox covers the boss intros SoH's "Skip Boss Introductions"
    // names plus MM's boss-warp and miniboss cutscenes, all under that one key.
    // LearnSong, QuickBossDeaths and GlitchAiding have no MM twin; MM's song-teacher
    // skips key on Story, as upstream 2S2H does.
    { "gEnhancements.Cutscenes.SkipEntranceCutscenes", "gEnhancements.TimeSavers.SkipCutscene.Entrances", false },
    { "gEnhancements.Cutscenes.SkipIntroSequence", "gEnhancements.TimeSavers.SkipCutscene.Intro", false },
    { "gEnhancements.Cutscenes.SkipOnePointCutscenes", "gEnhancements.TimeSavers.SkipCutscene.OnePoint", false },
    { "gEnhancements.Cutscenes.SkipStoryCutscenes", "gEnhancements.TimeSavers.SkipCutscene.Story", false },
    { "gEnhancements.Cutscenes.SkipEnemyCutscenes", "gEnhancements.TimeSavers.SkipCutscene.BossIntro", false },

    // Group D — free-look. OoT files it under gSettings.FreeLook, MM under
    // gEnhancements.Camera.FreeLook. Enable is bool; MaxCameraDistance and
    // TransitionSpeed are int sliders read with CVarGetInteger on BOTH sides
    // (TransitionSpeed default 25 on both), so no representation gap.
    { "gEnhancements.Camera.FreeLook.Enable", "gSettings.FreeLook.Enabled", false },
    { "gEnhancements.Camera.FreeLook.MaxCameraDistance", "gSettings.FreeLook.MaxCameraDistance", false },
    { "gEnhancements.Camera.FreeLook.TransitionSpeed", "gSettings.FreeLook.TransitionSpeed", false },

    // ---- The #966 key-name gaps (issues #1027-#1038). ----
    // The audit of which settings MM does not honour found rows where SoH's menu
    // writes one key and MM reads a differently spelled twin with NO row here, so
    // the control moved a key nothing in MM consulted. Each row below was
    // re-verified at its read sites to agree on TYPE and RANGE on both sides
    // (the draw-distance multiplier is an int 1..5, default 1; every other row is
    // a 0/1 toggle, default 0), which makes it a pure rename: ConfigVersion9Updater
    // copies a stored MM-spelled value across and test_cvar_classification pins
    // every row (the "#966 key-gap rows" block). MM's readers moved to SoH's key
    // in the same change, BenMenu.cpp's excluded widgets included.
    //
    // Mapping is not the same as live. The first three rows (#1027-#1029) have
    // readers that are LINKED (the actor-cull TUs and AuthenticGfxPatches.cpp).
    // The other eight providers sat in the plain 2ship_enh archive and were
    // elided from the single exe; mapping came first so the day a provider
    // linked it already read the one shared key, and #1030-#1036 and #1038 have
    // since carved them into the WHOLE_ARCHIVE'd registrars list (games/mm/
    // CMakeLists.txt), which test_cvar_classification pins row by row.
    // Defaults: N64WeirdFrames (#1038) is the one row whose MM reader default
    // differed (2Ship: on, to match hardware); the reader now takes OoT's off, so
    // the one checkbox cannot show a state MM is not in. SkipMiscInteractions
    // keeps the per-reader default difference the cutscene-skip family already
    // documents above (OoT IS_RANDO, MM 0).
    { "gEnhancements.Graphics.IncreaseActorDrawDistance", "gEnhancements.DisableDrawDistance", false },
    { "gEnhancements.Graphics.ActorCullingAccountsForWidescreen", "gEnhancements.WidescreenActorCulling", false },
    { "gEnhancements.Fixes.FixTexturesOOB", "gEnhancements.FixTexturesOOB", false },
    { "gEnhancements.A11y.NoScreenFlashForEnemyKill", "gSettings.A11yNoScreenFlashForFinishingBlow", false },
    { "gEnhancements.Graphics.3DItemDrops", "gEnhancements.NewDrops", false },
    { "gEnhancements.Cutscenes.SkipMiscInteractions", "gEnhancements.TimeSavers.SkipMiscInteractions", false },
    { "gEnhancements.Playback.SkipScarecrowSong", "gEnhancements.InstantScarecrow", false },
    { "gEnhancements.PlayerActions.ArrowCycle", "gEnhancements.BowArrowCycle", false },
    { "gEnhancements.Equipment.MagicArrowEquipSpeed", "gEnhancements.SkipArrowAnimation", false },
    { "gEnhancements.DifficultyOptions.PermanentHeartLoss", "gEnhancements.PermanentHeartLoss", false },
    { "gEnhancements.Restorations.N64WeirdFrames", "gEnhancements.N64WeirdFrames", false },
    // #1062: the ocarina D-pad and right-stick switches and "Prevent Dropped Ocarina Inputs". MM kept them
    // under Playback.*; SoH writes gSettings.CustomOcarina.Dpad and gSettings.CustomOcarina.RightStick (Settings >
    // Controls > Ocarina Controls) and gEnhancements.DpadNoDropOcarinaInput.
    // All bool, default off, read per frame by GameInteractor_Dpad,
    // GameInteractor_RightStickOcarina and the two ocarina input loops, so a plain rename. The held-apart P6 pair that
    // stood in kMustStayDistinct is gone: both games have all three switches, and the one genuinely OoT-only piece (the
    // custom button-mapping layer, gSettings.CustomOcarina.Enabled) has no MM key to pair.
    { "gEnhancements.Playback.DpadOcarina", "gSettings.CustomOcarina.Dpad", false },
    { "gEnhancements.Playback.RightStickOcarina", "gSettings.CustomOcarina.RightStick", false },
    { "gEnhancements.Playback.NoDropOcarinaInput", "gEnhancements.DpadNoDropOcarinaInput", false },
    // #1064: SoH's Containers Match Contents. MM's chest/pot/grass/crate/barrel readers sat on gRando.CSMC (its only
    // widget, in the excluded Rando/Menu.cpp, never drew). Both bool, default off. ChestSizeDependsStoneOfAgony stays
    // OoT-only: MM has no Stone of Agony.
    { "gRando.CSMC", "gEnhancements.ChestSizeAndTextureMatchContents", false },
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

/*
 * W6 define slots (#1233). Each later W6 lane puts its RSBS_CVAR_MM_* defines on the lines directly after its own
 * slot comment and nowhere else, so lanes that run side by side never edit the same lines. A scalar key gets a plain
 * define, bound by exactly one shared-menu row and read under games/mm (the classification lock's leg 3f-2c); a key
 * family MM composes by concatenation gets one define whose name ends in _PREFIX, locked by its lane's own leg.
 */
// W6-TIMESAVERS (#1236, #1240): MM's text, time-saver and pause-save keys, each written by one hosted row on SoH's
// Enhancements pages (SohMenuHostedMmTimesavers.cpp). Fast Text stays held apart from SoH's Text Speed (P1 below).
// MM spells the Timesavers family with a lowercase s (kParkedCasingPrefixMM); these defines keep that spelling.
#define RSBS_CVAR_MM_DIALOGUE_FAST_TEXT "gEnhancements.Dialogue.FastText"
#define RSBS_CVAR_MM_DIALOGUE_FAST_BANK_SELECTION "gEnhancements.Dialogue.FastBankSelection"
#define RSBS_CVAR_MM_DIALOGUE_AUTO_BOMBERS_CODE "gEnhancements.Dialogue.AutoBombersCode"
#define RSBS_CVAR_MM_TIMESAVERS_SWAMP_BOAT_SPEED "gEnhancements.Timesavers.SwampBoatSpeed"
#define RSBS_CVAR_MM_TIMESAVERS_GALLERY_TWOFER "gEnhancements.Timesavers.GalleryTwofer"
#define RSBS_CVAR_MM_TIMESAVERS_MARINE_LAB_HP "gEnhancements.Timesavers.MarineLabHP"
#define RSBS_CVAR_MM_TIMESAVERS_DAMPE_DIGGING_SKIP "gEnhancements.Timesavers.DampeDiggingSkip"
#define RSBS_CVAR_MM_TIMESAVERS_FASTER_SCENE_TRANSITIONS "gEnhancements.Timesavers.FasterSceneTransitions"
#define RSBS_CVAR_MM_TIMESAVERS_POWDER_KEG_CERTIFICATION "gEnhancements.Timesavers.PowderKegCertification"
#define RSBS_CVAR_MM_TIMESAVERS_SKIP_BALLAD_OF_WINDFISH "gEnhancements.Timesavers.SkipBalladOfWindfish"
#define RSBS_CVAR_MM_TIMESAVERS_AUTO_BANK_DEPOSIT "gEnhancements.Timesavers.AutoBankDeposit"
#define RSBS_CVAR_MM_SAVING_PAUSE_SAVE "gEnhancements.Saving.PauseSave"
// W6-CYCLE-CHEATS
// #1236, #1240: MM's cheats and Time Cycle options, each bound by one hosted row on SoH's Cheats or
// Quality of Life page. The first two are the PARKED pairs below (kept apart, MM rows beside SoH's); the rest have no
// OoT twin. DoNotResetRupees / DoNotResetConsumables have no row on purpose (the combo's cycle hooks and the frozen
// Keep Items on Cycle Reset decide them), so they have no define either.
#define RSBS_CVAR_MM_CHEATS_INFINITE_CONSUMABLES "gCheats.InfiniteConsumables"
#define RSBS_CVAR_MM_CHEATS_UNRESTRICTED_ITEMS "gCheats.UnrestrictedItems"
#define RSBS_CVAR_MM_CHEATS_LONGER_FLOWER_GLIDE "gCheats.LongerFlowerGlide"
#define RSBS_CVAR_MM_CHEATS_ELEGY_ANYWHERE "gCheats.ElegyAnywhere"
#define RSBS_CVAR_MM_CHEATS_TEMPLE_TIME_STOP "gCheats.TempleTimeStop"
#define RSBS_CVAR_MM_CHEATS_UNBREAKABLE_RAZOR_SWORD "gCheats.UnbreakableRazorSword"
#define RSBS_CVAR_MM_CYCLE_DO_NOT_RESET_BOTTLE_CONTENT "gEnhancements.Cycle.DoNotResetBottleContent"
#define RSBS_CVAR_MM_CYCLE_DO_NOT_RESET_RAZOR_SWORD "gEnhancements.Cycle.DoNotResetRazorSword"
#define RSBS_CVAR_MM_CYCLE_DO_NOT_RESET_TIME_SPEED "gEnhancements.Cycle.DoNotResetTimeSpeed"
#define RSBS_CVAR_MM_CYCLE_KEEP_EXPRESS_MAIL "gEnhancements.Cycle.KeepExpressMail"
#define RSBS_CVAR_MM_CYCLE_STOP_OCEANSIDE_SPIDER_HOUSE_SQUATTER "gEnhancements.Cycle.StopOceansideSpiderHouseSquatter"
#define RSBS_CVAR_MM_CYCLE_OCEANSIDE_WALLET_ANY_DAY "gEnhancements.Cycle.OceansideWalletAnyDay"
// W6-PLAYER
// W6-PLAYER (#1236): MM's Player, Equipment and PlayerActions rows, hosted on SoH's Enhancements pages
// (SohGui/HostedMm/SohMenuHostedMmPlayer.cpp). Climb speed and Bombchu drops are held apart (kMustStayDistinct).
#define RSBS_CVAR_MM_PLAYER_MANUAL_JUMP "gEnhancements.Player.ManualJump"
#define RSBS_CVAR_MM_PLAYER_PREVENT_DIVE_OVER_WATER "gEnhancements.Player.PreventDiveOverWater"
#define RSBS_CVAR_MM_PLAYER_INFINITE_DEKU_HOPPING "gEnhancements.Player.InfiniteDekuHopping"
#define RSBS_CVAR_MM_PLAYER_CLIMB_SPEED "gEnhancements.Player.ClimbSpeed"
#define RSBS_CVAR_MM_PLAYER_FASTER_PUSH_AND_PULL "gEnhancements.Player.FasterPushAndPull"
#define RSBS_CVAR_MM_PLAYER_FAST_FLOWER_LAUNCH "gEnhancements.Player.FastFlowerLaunch"
#define RSBS_CVAR_MM_PLAYER_FIERCE_DEITY_PUTAWAY "gEnhancements.Player.FierceDeityPutaway"
#define RSBS_CVAR_MM_EQUIPMENT_TWO_HANDED_SWORD_SPIN_ATTACK "gEnhancements.Equipment.TwoHandedSwordSpinAttack"
#define RSBS_CVAR_MM_EQUIPMENT_GREAT_FAIRY_SWORD_B_BUTTON "gEnhancements.Equipment.GreatFairySwordBButton"
#define RSBS_CVAR_MM_EQUIPMENT_BETTER_PICTO_MESSAGE "gEnhancements.Equipment.BetterPictoMessage"
#define RSBS_CVAR_MM_PLAYER_ACTIONS_INSTANT_RECALL "gEnhancements.PlayerActions.InstantRecall"
#define RSBS_CVAR_MM_EQUIPMENT_CHU_DROPS "gEnhancements.Equipment.ChuDrops"
// W6-MASKS-SONGS
// W6-MASKS-SONGS (#1236, #1240): MM's Masks and Ocarina rows, hosted in two "Majora's Mask" blocks on SoH's
// Enhancements > Items (SohGui/HostedMm/SohMenuHostedMmMasksSongs.cpp). Each stays MM's own key (SoH's Fast Ocarina
// Playback is only the nearest relative of Faster Song Playback); Underwater Ocarina is a Player key whose row belongs
// with the Ocarina. PersistentBunnyHood.State is the TU's internal toggle and has no row.
#define RSBS_CVAR_MM_MASKS_EQUIP_WHILE_SWIMMING "gEnhancements.Masks.EquipWhileSwimming"
#define RSBS_CVAR_MM_MASKS_BLAST_MASK_KEG "gEnhancements.Masks.BlastMaskKeg"
#define RSBS_CVAR_MM_MASKS_FAST_TRANSFORMATION "gEnhancements.Masks.FastTransformation"
#define RSBS_CVAR_MM_MASKS_FIERCE_DEITYS_ANYWHERE "gEnhancements.Masks.FierceDeitysAnywhere"
#define RSBS_CVAR_MM_MASKS_PERSISTENT_BUNNY_HOOD "gEnhancements.Masks.PersistentBunnyHood.Enabled"
#define RSBS_CVAR_MM_MASKS_NO_BLAST_MASK_COOLDOWN "gEnhancements.Masks.NoBlastMaskCooldown"
#define RSBS_CVAR_MM_MASKS_GORON_ROLLING_IGNORES_MAGIC "gEnhancements.Masks.GoronRollingIgnoresMagic"
#define RSBS_CVAR_MM_MASKS_GORON_ROLLING_FAST_SPIKES "gEnhancements.Masks.GoronRollingFastSpikes"
#define RSBS_CVAR_MM_SONGS_PAUSE_OWL_WARP "gEnhancements.Songs.PauseOwlWarp"
#define RSBS_CVAR_MM_SONGS_ENABLE_SUNS_SONG "gEnhancements.Songs.EnableSunsSong"
#define RSBS_CVAR_MM_SONGS_ZORA_EGG_COUNT "gEnhancements.Songs.ZoraEggCount"
#define RSBS_CVAR_MM_SONGS_FASTER_SONG_PLAYBACK "gEnhancements.Songs.FasterSongPlayback"
#define RSBS_CVAR_MM_SONGS_SKIP_SOARING_CUTSCENE "gEnhancements.Songs.SkipSoaringCutscene"
#define RSBS_CVAR_MM_PLAYER_UNDERWATER_OCARINA "gEnhancements.Player.UnderwaterOcarina"
// W6-CAM-GYRO
// #1235, #1236, #1240: MM's gyro aiming, free-look pitch limits, debug camera, Remote Bombchu Control and speed
// modifiers. None has an SoH twin except the speed modifiers, which are held apart from SoH's gSettings.WalkModifier.*
// (kMustStayDistinct below: different ranges, and MM enables walking and swimming separately). The camera and speed
// rows sit in SoH's Bindings window (SohInputEditorWindow.cpp, DrawMmCameraRows), Remote Bombchu Control on
// Enhancements > Items > Explosives (HostedMm/SohMenuHostedMmCamGyro.cpp).
#define RSBS_CVAR_MM_CAMERA_FP_GYRO_ENABLED "gEnhancements.Camera.FirstPerson.GyroEnabled"
#define RSBS_CVAR_MM_CAMERA_FP_GYRO_INVERT_X "gEnhancements.Camera.FirstPerson.GyroInvertX"
#define RSBS_CVAR_MM_CAMERA_FP_GYRO_INVERT_Y "gEnhancements.Camera.FirstPerson.GyroInvertY"
#define RSBS_CVAR_MM_CAMERA_FP_GYRO_SENSITIVITY_X "gEnhancements.Camera.FirstPerson.GyroSensitivityX"
#define RSBS_CVAR_MM_CAMERA_FP_GYRO_SENSITIVITY_Y "gEnhancements.Camera.FirstPerson.GyroSensitivityY"
#define RSBS_CVAR_MM_CAMERA_FREE_LOOK_MAX_PITCH "gEnhancements.Camera.FreeLook.MaxPitch"
#define RSBS_CVAR_MM_CAMERA_FREE_LOOK_MIN_PITCH "gEnhancements.Camera.FreeLook.MinPitch"
#define RSBS_CVAR_MM_CAMERA_DEBUG_CAM_ENABLE "gEnhancements.Camera.DebugCam.Enable"
#define RSBS_CVAR_MM_CAMERA_DEBUG_CAM_6DOF "gEnhancements.Camera.DebugCam.6DOF"
#define RSBS_CVAR_MM_CAMERA_DEBUG_CAM_SPEED "gEnhancements.Camera.DebugCam.CameraSpeed"
#define RSBS_CVAR_MM_PLAYER_ACTIONS_REMOTE_BOMBCHU "gEnhancements.PlayerActions.RemoteBombchu"
#define RSBS_CVAR_MM_SPEED_MODIFIER_ENABLE "gSettings.SpeedModifier.Enable"
#define RSBS_CVAR_MM_SPEED_MODIFIER_TOGGLE "gSettings.SpeedModifier.Toggle"
#define RSBS_CVAR_MM_SPEED_MODIFIER_WALK_ENABLE "gSettings.SpeedModifier.WalkEnable"
#define RSBS_CVAR_MM_SPEED_MODIFIER_WALK_MAPPING1 "gSettings.SpeedModifier.WalkMapping1"
#define RSBS_CVAR_MM_SPEED_MODIFIER_WALK_MAPPING2 "gSettings.SpeedModifier.WalkMapping2"
#define RSBS_CVAR_MM_SPEED_MODIFIER_SWIM_ENABLE "gSettings.SpeedModifier.SwimEnable"
#define RSBS_CVAR_MM_SPEED_MODIFIER_SWIM_MAPPING1 "gSettings.SpeedModifier.SwimMapping1"
#define RSBS_CVAR_MM_SPEED_MODIFIER_SWIM_MAPPING2 "gSettings.SpeedModifier.SwimMapping2"
// W6-GFX
// W6-GFX (#1237): MM's clock style, 24-hour clock, motion blur and scene-geometry keys, each written by one hosted row
// on SoH's Enhancements > Graphics page (SohGui/HostedMm/SohMenuHostedMmGraphics.cpp). None has an SoH twin. The two
// dynamic motion blur rows bind MM registers, not keys (src/common/mm_graphics_bridge.h), so they have no define.
#define RSBS_CVAR_MM_GRAPHICS_CLOCK_TYPE "gEnhancements.Graphics.ClockType"
#define RSBS_CVAR_MM_GRAPHICS_24_HOURS_CLOCK "gEnhancements.Graphics.24HoursClock"
#define RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_MODE "gEnhancements.Graphics.MotionBlur.Mode"
#define RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_INTERPOLATE "gEnhancements.Graphics.MotionBlur.Interpolate"
#define RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_STRENGTH "gEnhancements.Graphics.MotionBlur.Strength"
#define RSBS_CVAR_MM_GRAPHICS_FIX_GEOMETRY_SEAMS "gEnhancements.Graphics.FixSceneGeometrySeams"
#define RSBS_CVAR_MM_GRAPHICS_NO_GEOMETRY_DISTANCE_CHECK "gEnhancements.Graphics.DisableSceneGeometryDistanceCheck"
// W6-FIXES
// W6-FIXES (#1237, #1236): MM's Fixes and Restorations keys, each written by one hosted row on SoH's Enhancements >
// Fixes page (SohGui/HostedMm/SohMenuHostedMmFixes.cpp). Tatl ISG stays held apart from SoH's Easy ISG (P2 below). The
// ammo key keeps the spelling z_parameter.c reads, never 2Ship's v1 rename to gFixes.FixButtonEnvColor. Fix Console
// Crashes has no row on purpose (the single exe always keeps the crash fixes on), so it has no define either.
#define RSBS_CVAR_MM_CAMERA_FIX_TARGETTING_CAMERA_SNAP "gEnhancements.Camera.FixTargettingCameraSnap"
#define RSBS_CVAR_MM_FIXES_EPONA_STEALING_SWORD "gFixes.FixEponaStealingSword"
#define RSBS_CVAR_MM_FIXES_FIERCE_DEITY_Z_TARGET_MOVEMENT "gEnhancements.Fixes.FierceDeityZTargetMovement"
#define RSBS_CVAR_MM_FIXES_AMMO_COUNT_ENV_COLOR "gFixes.FixAmmoCountEnvColor"
#define RSBS_CVAR_MM_FIXES_IKANA_GREAT_FAIRY_FOUNTAIN_COLOR "gFixes.FixIkanaGreatFairyFountainColor"
#define RSBS_CVAR_MM_FIXES_CONTROL_CHARACTERS "gEnhancements.Fixes.ControlCharacters"
#define RSBS_CVAR_MM_FIXES_COMPLETED_HEART_CONTAINER_AUDIO "gEnhancements.Fixes.CompletedHeartContainerAudio"
#define RSBS_CVAR_MM_RESTORATIONS_POWER_CROUCH_STAB "gEnhancements.Restorations.PowerCrouchStab"
#define RSBS_CVAR_MM_RESTORATIONS_TATL_ISG "gEnhancements.Restorations.TatlISG"
#define RSBS_CVAR_MM_RESTORATIONS_CONSTANT_FLIPS_HOPS "gEnhancements.Restorations.ConstantFlipsHops"
#define RSBS_CVAR_MM_RESTORATIONS_SIDE_ROLL "gEnhancements.Restorations.SideRoll"
#define RSBS_CVAR_MM_RESTORATIONS_OOT_FASTER_SWIM "gEnhancements.Restorations.OoTFasterSwim"
#define RSBS_CVAR_MM_RESTORATIONS_WOODFALL_MOUNTAIN_APPEARANCE "gEnhancements.Restorations.WoodfallMountainAppearance"
#define RSBS_CVAR_MM_RESTORATIONS_JP_GROTTOS "gEnhancements.Restorations.JPGrottos"
#define RSBS_CVAR_MM_RESTORATIONS_BONK_COLLISION "gEnhancements.Restorations.BonkCollision"
// W6-MODES-DIFF
// W6-MINIGAMES
// W6-TIMERS
// W6-HUD-A
// #1239: MM's HUD placement keys, one family composed per element as PREFIX + "<Element>" + ".Mode" / ".Position.X" /
// ".Position.Y" / ".Scale" (src/common/mm_hud_editor_elements.h, MM's HUD_EDITOR_ELEMENT). Written by the Majora's
// Mask section of SoH's Cosmetics Editor, HUD Placement tab; locked by the classification lock's HUD-editor leg.
#define RSBS_CVAR_MM_HUD_EDITOR_PREFIX "gHudEditor."
// W6-COS-A
// W6-AUDIO-SEQ
// W6-DEV

/**
 * MM's damage multiplier key (#1037). Held apart from SoH's gEnhancements.DamageMult (kMustStayDistinct below), so MM's
 * own row is hosted beside SoH's on Enhancements > Difficulty. games/oot may not spell the key as a literal (the
 * classification lock's check 3c-2: an OoT reader of MM's key converges the pair from OoT's side), so the hosted
 * row binds it through this constant.
 */
#define RSBS_CVAR_MM_DAMAGE_MULTIPLIER "gEnhancements.DifficultyOptions.DamageMultiplier"

// L7 (#1133, ruling D5): the curated MM enhancement rows' keys (kHostedMmEnhancements below), each bound by its row on
// SoH's own Enhancements pages, beside the SoH row that holds its twin or in a "Majora's Mask" block
// (SohMenuComboMmEnhancements.cpp). The Combo > Majora's Mask page that drew them from the manifest retired with the
// Combo header; games/oot binds the define, never the literal.
#define RSBS_CVAR_MM_KALEIDO_GAME_OVER "gEnhancements.Kaleido.GameOver"
#define RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME "gEnhancements.Songs.BetterSongOfDoubleTime"
#define RSBS_CVAR_MM_SKIP_SOT_CUTSCENES "gEnhancements.Songs.SkipSoTCutscenes"
#define RSBS_CVAR_MM_AUTOSAVE_INTERVAL "gEnhancements.Saving.AutosaveInterval"

// W6-CAM-AIM (#1236): MM's aiming and camera inversion / sensitivity keys and its shield Y inversion. Held apart from
// SoH's (P3, P7 and the three first-person rows beside P3 in kMustStayDistinct), so each has an MM row beside its SoH
// twin in SoH's Bindings window, Camera Controls panel (SohInputEditorWindow.cpp). MM reads the three *_INVERT_Y
// aiming and camera keys with default 1 (inverted), and their rows declare that default.
#define RSBS_CVAR_MM_CAMERA_FP_DISABLE_AUTO_CENTER "gEnhancements.Camera.FirstPerson.DisableFirstPersonAutoCenterView"
#define RSBS_CVAR_MM_CAMERA_FP_INVERT_X "gEnhancements.Camera.FirstPerson.InvertX"
#define RSBS_CVAR_MM_CAMERA_FP_INVERT_Y "gEnhancements.Camera.FirstPerson.InvertY"
#define RSBS_CVAR_MM_CAMERA_FP_SENSITIVITY_X "gEnhancements.Camera.FirstPerson.SensitivityX"
#define RSBS_CVAR_MM_CAMERA_FP_SENSITIVITY_Y "gEnhancements.Camera.FirstPerson.SensitivityY"
#define RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_ENABLED "gEnhancements.Camera.FirstPerson.RightStickEnabled"
#define RSBS_CVAR_MM_CAMERA_FP_MOVE_IN_FIRST_PERSON "gEnhancements.Camera.FirstPerson.MoveInFirstPerson"
#define RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_INVERT_X "gEnhancements.Camera.FirstPerson.RightStickInvertX"
#define RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_INVERT_Y "gEnhancements.Camera.FirstPerson.RightStickInvertY"
#define RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_SENSITIVITY_X "gEnhancements.Camera.FirstPerson.RightStickSensitivityX"
#define RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_SENSITIVITY_Y "gEnhancements.Camera.FirstPerson.RightStickSensitivityY"
#define RSBS_CVAR_MM_CAMERA_RIGHT_STICK_INVERT_X "gEnhancements.Camera.RightStick.InvertXAxis"
#define RSBS_CVAR_MM_CAMERA_RIGHT_STICK_INVERT_Y "gEnhancements.Camera.RightStick.InvertYAxis"
#define RSBS_CVAR_MM_CAMERA_RIGHT_STICK_SENSITIVITY_X "gEnhancements.Camera.RightStick.CameraSensitivity.X"
#define RSBS_CVAR_MM_CAMERA_RIGHT_STICK_SENSITIVITY_Y "gEnhancements.Camera.RightStick.CameraSensitivity.Y"
#define RSBS_CVAR_MM_EQUIPMENT_INVERT_SHIELD_Y "gEnhancements.Equipment.InvertShieldY"

/**
 * Inventory §4 rows (P1-P7, minus the ones with no single representative key and
 * minus P6, which #1062 converged: the D-pad and right-stick ocarina switches)
 * plus §5.3's four PARKED rows, which are real convergence candidates blocked
 * on maintainer answers rather than settled non-merges.
 */
inline constexpr DistinctPair kMustStayDistinct[] = {
    { "P1 text speed", RSBS_CVAR_MM_DIALOGUE_FAST_TEXT, "gEnhancements.TextSpeed",
      "Units are incompatible: OoT is a 1-5x integer multiplier, MM is a boolean. Merging writes 1 = "
      "'no speedup', so the control silently does nothing. MM's FastText also bundles hold-B-to-advance, "
      "which OoT exposes as the SEPARATE key gEnhancements.SkipText — one control cannot express both "
      "decompositions." },
    { "P2 infinite sword glitch", RSBS_CVAR_MM_RESTORATIONS_TATL_ISG, "gCheats.EasyISG",
      "Taxonomy conflict: OoT treats ISG as a cheat to grant, MM treats restoring Navi-ISG via Tatl as "
      "authenticity, and the gate differs (OoT's is an unconditional passive grant, MM's restores a behaviour the "
      "N64 game had). Ruled (ADR 0004): the keys stay distinct, and MM's own row, \"Tatl ISG (restoration)\", is "
      "hosted on Enhancements > Fixes > Glitch Restorations (W6-FIXES)." },
    { "P3 camera axis decomposition", "gEnhancements.Camera.FirstPerson.InvertX",
      "gSettings.Controls.InvertAimingXAxis",
      "Different axis decomposition: OoT splits by context (free-look / aiming / shield / Z-aim), MM by "
      "input device (right stick / gyro / first-person). No 1:1 mapping — OoT has no gyro axis, MM has no "
      "shield-aim axis. Merging drops settings on both sides." },
    // W6-CAM-AIM: the three first-person switches the inventory covered only by its Camera category claim. Each
    // matches SoH's twin in value space (bool, default off) and JSON shape (int), but not in gating.
    { "P3 first-person auto-centering", RSBS_CVAR_MM_CAMERA_FP_DISABLE_AUTO_CENTER,
      "gSettings.DisableFirstPersonAutoCenterView",
      "Same bool, different gating: MM's view never auto-centers while its own Gyro Aiming is on (z_player.c "
      "reads it OR GyroEnabled, and 2Ship's row is disabled then), while SoH reads its key alone and clamps the "
      "pitch itself. Each game keeps its own row in the Bindings window." },
    { "P3 right-stick aiming switch", RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_ENABLED, "gSettings.Controls.RightStickAim",
      "Same bool, different gating: SoH's switch also feeds the right stick into Z-target weapon aiming and uses "
      "SoH's own aiming inversion and sensitivity; MM's switch gates a separate right-stick inversion and "
      "sensitivity set (P3) and MM's move-while-aiming. Merging would make one switch mean two things." },
    { "P3 move while aiming", RSBS_CVAR_MM_CAMERA_FP_MOVE_IN_FIRST_PERSON, "gSettings.MoveInFirstPerson",
      "Same bool, different gating: SoH reads its key on its own, while MM moves Link only while its right-stick "
      "aiming switch is also on, and never on Epona, the swamp boat or in the archery games." },
    { "P7 shield aim inversion", "gEnhancements.Equipment.InvertShieldY", "gSettings.Controls.InvertShieldAimingYAxis",
      "Different namespace AND different axis coverage: OoT has X+Y, MM has Y only. Merging loses OoT's "
      "X axis." },
    { "Climb speed (inventory §5.3 group A, reclassified P by #462)", RSBS_CVAR_MM_PLAYER_CLIMB_SPEED,
      "gEnhancements.ClimbSpeed",
      "Listed in §5.3 group A as 'both numeric', but representation verification found INCOMPATIBLE UNITS: "
      "OoT reads it as an ADDITIVE offset (slider '+%d', 0-12, default 0; playSpeed += phi_f2 * value) while "
      "MM reads it as a MULTIPLIER (slider 1-5, default 1; *speed *= value, gated value>1). The neutral value "
      "(0 vs 1) and the range (0-12 vs 1-5) both disagree, so a shared slider cannot mean both. Same failure "
      "shape as P1 text speed. Converge only after a maintainer decides which unit wins and MM's read/menu are "
      "converted to it — a plain key swap is a regression, not a fix." },
    // W6-CAM-GYRO (#1235): the speed modifiers, the #1037 value-space ruling applied.
    { "Speed modifier value space (W6-CAM-GYRO)", RSBS_CVAR_MM_SPEED_MODIFIER_WALK_MAPPING1,
      "gSettings.WalkModifier.Mapping1",
      "Same idea (hold or toggle M1 / M2 to scale Link's speed), different value spaces and gating: SoH's walk and "
      "swim multipliers run 0-5x behind one enable, while MM's walk runs 0-15x and its swim 0-8.75x, each behind "
      "its own walk or swim enable as well as the master one. A shared slider would clip MM's range or hand SoH "
      "values it never offers. Each game keeps its own rows under the Bindings window's Modifier Buttons." },
    { "Autosave interval (inventory §5.3 group A caveat)", "gEnhancements.Saving.AutosaveInterval",
      "(none — OoT hardcodes THREE_MINUTES_IN_UNIX)",
      "The Autosave ENABLE flag is genuinely shared-intent, but OoT's interval is hardcoded with no CVar. "
      "The interval is MM-only and must NOT ride along with an Autosave rename." },
    { "Damage multiplier value space (#1037, held apart and hosted)", RSBS_CVAR_MM_DAMAGE_MULTIPLIER,
      "gEnhancements.DamageMult",
      "Same intent (a left-shift of incoming damage: OoT z_player.c `1 << DamageMult`, MM `*damage <<= value`) "
      "and 0-4 agree (1x-16x), but the VALUE SPACES differ: OoT's list runs 0-8 (to 256x) while MM's is 0-4 plus "
      "10 = '1 Hit KO'. A plain key swap leaves OoT's 5-8 with no entry in MM's list and MM's 10 with none in "
      "OoT's, so each menu would show a blank for the other's value. Ruled: the keys stay distinct and MM's own "
      "row (its own value list) is hosted beside SoH's on Enhancements > Difficulty." },
    { "Bombchu drops rule (W6-PLAYER, held apart and hosted)", RSBS_CVAR_MM_EQUIPMENT_CHU_DROPS,
      "gEnhancements.EnableBombchuDrops",
      "Same bool and JSON shape, different RULE: SoH's EnItem00_ConvertBombDropToBombchu never drops Bombchus "
      "Link does not have and favours whichever of Bombs and Bombchus runs lower, and an Ocarina of Time seed can "
      "force SoH's row on; MM's ChuDrops.cpp gives a Bomb drop an even chance to be a Bombchu. In the single exe "
      "MM's rule also keeps SoH's first clause (review H1): it converts only while INV_CONTENT(ITEM_BOMBCHU) == "
      "ITEM_BOMBCHU already holds, because the Bombchu count is a shared PROGRESSION quantity (shared_items.h) and "
      "2Ship's unguarded rule would set the Bombchu item in Termina (z_parameter.c Item_Give), the next harvest "
      "would carry the count into the pool, and OoT_ApplySharedAmmoCount would grant Hyrule the Bombchu item and "
      "open its Bombchu shops before the seed placed them (OoT_SharedAmmoGrantBlocked stops that only when the "
      "seed's Bombchu Bag option is on). It still never weighs the scarcer ammo, so one switch would still mean "
      "two rules, and a seed-forced SoH row would show on while MM followed the hidden value. Ruled: the keys stay "
      "distinct and MM's row is hosted beside SoH's on Enhancements > Difficulty > Drops. MM-only key: an "
      "importer must carry it as MM's own and never onto SoH's." },
    { "PARKED: infinite ammo scope", RSBS_CVAR_MM_CHEATS_INFINITE_CONSUMABLES, "gCheats.InfiniteAmmo",
      "MM's 'consumables' scope may be wider than OoT's 'ammo'. Pending a maintainer answer; do not "
      "converge until it lands." },
    { "PARKED: item restriction gate", RSBS_CVAR_MM_CHEATS_UNRESTRICTED_ITEMS, "gCheats.NoRestrictItems",
      "Same intent (drop item gating) but DIFFERENT GATE: OoT ignores age restrictions, MM lets all forms "
      "use all items. Pending a maintainer answer; do not converge until it lands." },
};

/**
 * A pair where SoH's control drives MM through a VALUE TRANSLATION (#1060).
 *
 * Neither a rename (kConvergedKeys: the value spaces differ, so a key swap
 * would mean something else in MM) nor a held-apart pair (kMustStayDistinct:
 * there is one control for the concept, and it is SoH's). MM keeps its own key
 * only for its elided widget; every linked MM reader calls one translator that
 * reads `ootKey` and returns MM's value. CvarClassification pins each row and
 * checks the tree agrees.
 */
struct TranslatedKey {
    int issue;              ///< The issue that decided the translation.
    const char* mmKey;      ///< MM's key. Named in games/mm only by the translator and elided widgets.
    const char* ootKey;     ///< SoH's key: the one control writes it.
    const char* translator; ///< The games/mm-relative TU that maps ootKey's values onto MM's.
    const char* why;        ///< Why a rename would be wrong.
};

inline constexpr TranslatedKey kTranslatedKeys[] = {
    { 1060, "gEnhancements.Cutscenes.SkipGetItemCutscenes", "gRandoEnhancements.TimeSavers.SkipGetItemAnimation",
      "2s2h/Rando/StaticData/Items.cpp",
      "SoH has three tiers (Off / Junk Items / All Items), MM four (Never / Junk Items Only / Everything But "
      "Major / Always). A rename would give SoH's All Items (2) MM's Everything But Major and keep presenting "
      "majors, so Rando::StaticData::SkipGetItemCutsceneTierFromSoh maps Off->Never, Junk Items->Junk Items "
      "Only, All Items->Always." },
    { 1239, "gAudioEditor.LinkVoiceFreqMultiplier.Enable", "gAudioEditor.LinkVoiceFreqMultiplier",
      "2s2h/Enhancements/Audio/Sfx/LinksVoicePitchMultiplier.cpp",
      "SoH writes one float leaf (0.4-2.5, default 1.0); MM reads a bool .Enable beside a float .Scale. LUS stores "
      "CVars as dot-nested JSON, so SoH's leaf cannot also be the parent of MM's two keys. The translator arms MM's "
      "pitch hook when SoH's value is not 1.0." },
    { 1239, "gAudioEditor.LinkVoiceFreqMultiplier.Scale", "gAudioEditor.LinkVoiceFreqMultiplier",
      "2s2h/Enhancements/Audio/Sfx/LinksVoicePitchMultiplier.cpp",
      "The other half of the same pair: MM's .Scale is SoH's value itself (both games use 0.4-2.5), read through "
      "the same translator, so nothing writes MM's two keys." },
};
inline constexpr std::size_t kTranslatedKeyCount = sizeof(kTranslatedKeys) / sizeof(kTranslatedKeys[0]);

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
    // Developer tooling. gDeveloperTools.DebugSaveFileMode joined this list when
    // #454 was fixed: MM's clobbering write in RegisterDebugMode() was retired
    // and its read default aligned to OoT's 1, so the key is now genuine class
    // (S) — shared spelling, shared meaning, shared default, no writer. See the
    // note where kDisputedClassificationKeys used to carry it.
    "gDeveloperTools.DebugEnabled",
    "gDeveloperTools.DebugSaveFileMode",
    "gDeveloperTools.FrameAdvanceTick",
    "gDeveloperTools.LogLevel",
    // (The actor draw-distance and widescreen-culling keys that stood here were
    // NOT shared: the ADR's measurement matched them in OoT's uncalled
    // ShipUtils.cpp copy, while every live OoT reader and SoH's menu use
    // gEnhancements.DisableDrawDistance / .WidescreenActorCulling. #966 moved MM
    // onto those two keys, so they are converged rows above: #1027, #1028.)
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
 * Keys the two governing documents classified DIFFERENTLY. Kept as a named
 * table so a future re-dispute has a home, and asserted on only to the extent
 * both readings agree: a key here is never swept into the convergence set.
 *
 * EMPTY as of #454's fix (2026-08-02). It carried exactly one key,
 * `gDeveloperTools.DebugSaveFileMode`, which is now genuine class (S) and lives
 * in kSharedIntentKeys.
 *
 * The history, because the resolution turned on it. ADR 0003 §4.1 first called
 * the key (S) — "value spaces align, only the unwritten fallback differs,
 * acceptable". ADR 0004 §2d and inventory §3.3 BUG 2 ruled it (P), and NOT on
 * the defaults: the argument ADR 0003 rests on is false on its own terms,
 * because MM did not merely READ the shared key with a different default — its
 * registrar WROTE 0 into it whenever debug mode is off (the default state):
 *
 *   games/mm/2s2h/DeveloperTools/DeveloperTools.cpp, in RegisterDebugMode():
 *       if (!CVAR_DEBUG_MODE) {
 *           CVarSetInteger(CVAR_SAVE_FILE_MODE_NAME, DEBUG_SAVE_INFO_NONE);
 *
 * so the key was never left unwritten and OoT's default was destroyed on every
 * launch. That write is now RETIRED and MM's read default aligned to OoT's 1 in
 * the same change that moved the key into kSharedIntentKeys — so the manifest
 * only claims (S) against source that no longer contains the (P)-making write.
 *
 * An empty aggregate array literal (`= {}`) is ill-formed under MSVC (C2466: an
 * array may not have zero size), so the empty table is expressed as a null
 * pointer with an explicit zero count rather than an empty brace-init. The
 * disputed-key loop in test_cvar_classification.c is bounded by the count and so
 * never dereferences the pointer.
 *
 * Do not converge, split, or re-default DebugSaveFileMode as a side effect of
 * other work; it is a deliberate (S) now, not an accident.
 */
inline constexpr const char* const* kDisputedClassificationKeys = nullptr;
inline constexpr std::size_t kDisputedClassificationKeyCount = 0;

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

/**
 * Tier-4 key classification (ADR 0004 §6's scope note; ADR 0011 increment 2).
 *
 * State 4 (frozen-at-creation) "applies to a key when a world built from it
 * exists — that is, to tier-3 and tier-4 WORLD-IDENTITY keys after creation.
 * It does NOT apply to preference keys, which have no identity role and stay
 * live forever. Classifying each key into one bucket or the other is owed by
 * the same work that ships the freeze — an unclassified key defaults to
 * identity, because guessing 'preference' for an identity key is the failure
 * this state exists to prevent."
 *
 * So every key in the combo's own `gCombo.` namespace is classified HERE, at
 * introduction, and the cvar-classification lock scans the tree for the
 * namespace: a `gCombo.` literal with no row is a red build that names it.
 */
enum class ComboKeyClass {
    /// World identity: authors a frozen record up to the creation event, is
    /// read-only after it (ADR 0004 §6 state 4), and is folded into the
    /// identity an arrival or load compares and refuses on.
    Identity,
    /// No identity role. Live forever; never frozen, never compared.
    Preference,
};

struct ComboKey {
    const char* key;
    ComboKeyClass cls;
    const char* why; ///< What the key authors, or why it is only a preference.
};

/// The namespace the lock scans. A literal equal to a bare prefix (ending in
/// '.') is a prefix constant like this one, not a key, and the lock skips it.
inline constexpr const char* kComboKeyPrefix = "gCombo.";
/// The identity sub-namespace: every key under it MUST be classified Identity.
inline constexpr const char* kComboIdentityKeyPrefix = "gCombo.Rando.";

inline constexpr ComboKey kComboKeys[] = {
    // ---- gCombo.Rando.*: the authorable fields of ComboSettingsRecord (ADR
    //      0011 increment 2, less the two pool sizes #801 and the two item
    //      classes #834 retired, plus ADR 0010 D1's goal and #886's Shared
    //      Kokiri Sword; #668's shared ocarina was retired by #879). All IDENTITY.
    { RSBS_CVAR_COMBO_RANDO_DIRECTION, ComboKeyClass::Identity,
      "ComboSettingsRecord.direction (RSBS_COMBO_DIR_*): which placement passes run" },
    { RSBS_CVAR_COMBO_RANDO_GOAL, ComboKeyClass::Identity,
      "ComboSettingsRecord.goal (RSBS_COMBO_GOAL_*, ADR 0010 D1): the expression the creation's proof must "
      "satisfy" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_KOKIRI_SWORD, ComboKeyClass::Identity,
      "ComboSettingsRecord.comboFlags' RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD bit (#886): the Kokiri Sword is ONE "
      "monotonic shared item across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SHIELDS, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_SHIELDS bit (#1068): the Hylian/Hero's Shield and the "
      "Mirror Shield are each ONE shared item across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_LENS, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_LENS bit (#1065): the Lens of Truth is ONE shared-pair "
      "family across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SONG_EPONA, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_SONG_EPONA bit (#1066): Epona's Song is ONE "
      "shared-pair family across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SONG_STORMS, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_SONG_STORMS bit (#1066): the Song of Storms is ONE "
      "shared-pair family across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_OCARINA_BUTTONS, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_OCARINA_BUTTONS bit (#1067): each of the five ocarina "
      "buttons is ONE shared item across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SONG_TIME, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_SONG_TIME bit (#1069): the Song of Time is ONE "
      "shared-pair family across both games, MM starts without it and its moon crash is a New Cycle" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_FIRE, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_ARROW_FIRE bit (#968): the Fire Arrows are ONE "
      "shared-pair family across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_ICE, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_ARROW_ICE bit (#968): the Ice Arrows are ONE "
      "shared-pair family across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_LIGHT, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_ARROW_LIGHT bit (#968): the Light Arrows are ONE "
      "shared-pair family across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_BOTTLES bit (#970): the bottle count is ONE shared "
      "quantity across both games, up to OoT's four slots" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_GORON, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_GORON bit (#978): the Goron Mask is ONE shared-pair "
      "family "
      "across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_ZORA, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_ZORA bit (#978): the Zora Mask is ONE shared-pair family "
      "across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_BUNNY, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_BUNNY bit (#978): the Bunny Hood is ONE shared-pair "
      "family "
      "across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_KEATON, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_KEATON bit (#978): the Keaton Mask is ONE shared-pair "
      "family "
      "across both games" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_TRUTH, ComboKeyClass::Identity,
      "ComboItemRulesRecord.crossItemFlags' RSBS_XITEM_SHARED_TRUTH bit (#978): the Mask of Truth is ONE shared-pair "
      "family "
      "across both games" },
    // ---- gCombo.Windows.*: the common-owned windows' visibility (ADR 0008).
    //      All PREFERENCE: whether a pane is open says nothing about a world.
    { RSBS_CVAR_COMBO_WINDOW_SPOILER, ComboKeyClass::Preference, "cross-game spoiler window visibility (#496)" },
    { RSBS_CVAR_COMBO_WINDOW_TRACKER, ComboKeyClass::Preference,
      "combo tracker window visibility (#458; unread since #1132)" },
    { RSBS_CVAR_COMBO_WINDOW_COMBO_SETTINGS, ComboKeyClass::Preference,
      "combo settings pane visibility (ADR 0011 increment 2)" },
    { RSBS_CVAR_COMBO_WINDOW_ITEM_TRACKER, ComboKeyClass::Preference,
      "unified item tracker overlay visibility (#458; ADR 0004's 2026-09-30 amendment; unread since #1132)" },
    // ---- gCombo.Tracker.Items.*: the unified Item Tracker overlay's look
    //      (#458 U2). All PREFERENCE: how the overlay draws says nothing about
    //      a world. #1141 retired the five SoH twins (kComboRetiredKeys).
    { RSBS_CVAR_COMBO_ITEMS_OPACITY, ComboKeyClass::Preference, "item tracker overlay: alpha" },
    { RSBS_CVAR_COMBO_ITEMS_SECTION_OOT, ComboKeyClass::Preference, "item tracker overlay: draws the OoT section" },
    { RSBS_CVAR_COMBO_ITEMS_SECTION_MM, ComboKeyClass::Preference, "item tracker overlay: draws the MM section" },
    { RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED, ComboKeyClass::Preference,
      "item tracker overlay: draws the shared section" },
    { RSBS_CVAR_COMBO_ITEMS_MM_TOKENS, ComboKeyClass::Preference, "item tracker overlay: MM's Tokens group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_STRAY_FAIRIES, ComboKeyClass::Preference,
      "item tracker overlay: MM's Stray Fairies group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_DUNGEON, ComboKeyClass::Preference, "item tracker overlay: MM's Dungeon group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_BOSS_SOULS, ComboKeyClass::Preference,
      "item tracker overlay: MM's Boss Souls group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_ENEMY_SOULS, ComboKeyClass::Preference,
      "item tracker overlay: MM's Enemy Souls group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_OWL_STATUES, ComboKeyClass::Preference,
      "item tracker overlay: MM's Owl Statues group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_TIME, ComboKeyClass::Preference, "item tracker overlay: MM's Time group (#1141)" },
    { RSBS_CVAR_COMBO_ITEMS_MM_MISC, ComboKeyClass::Preference, "item tracker overlay: MM's Misc group (#1141)" },
    // ---- gCombo.Tracker.Checks.*: the Combo Tracker's own navigation (#1131).
    //      All PREFERENCE: where the window scrolls, or which area it shows,
    //      says nothing about a world.
    { RSBS_CVAR_COMBO_CHECKS_AUTO_SCROLL, ComboKeyClass::Preference,
      "combo tracker: the active game's pane scrolls to the current area (#1131)" },
    { RSBS_CVAR_COMBO_CHECKS_ONLY_CURRENT_AREA, ComboKeyClass::Preference,
      "combo tracker: seeds each pane's Only Show Current Area toggle (#1131)" },
};

/**
 * Retired gCombo.* keys (#1141): a key that left kComboKeys because a SoH key
 * now holds the setting, and where its saved value goes. Spelled here and
 * nowhere else; the Item Tracker overlay's one-shot carry
 * (ComboItemTrackerCarryRetiredKeys) is their only reader, and the
 * cvar-classification lock accepts these literals only as retired keys, never
 * as live ones. `stored` is the suffix the value is stored under (a colour
 * picker's ".Value").
 */
struct ComboRetiredKey {
    const char* key;
    const char* carriedTo;
    const char* stored;
};
inline constexpr ComboRetiredKey kComboRetiredKeys[] = {
    // The Item Tracker overlay's five twins of SoH's Item Tracker Settings
    // (operator ruling IT-D6: one set of settings, SoH's).
    { "gCombo.Tracker.Items.WindowType", "gTrackers.ItemTracker.WindowType", "" },
    { "gCombo.Tracker.Items.Draggable", "gTrackers.ItemTracker.Draggable", "" },
    { "gCombo.Tracker.Items.ShowOnlyPaused", "gTrackers.ItemTracker.ShowOnlyPaused", "" },
    { "gCombo.Tracker.Items.BgColor", "gTrackers.ItemTracker.BgColor", ".Value" },
    { "gCombo.Tracker.Items.IconSize", "gTrackers.ItemTracker.IconSize", "" },
};
inline constexpr std::size_t kComboRetiredKeyCount = sizeof(kComboRetiredKeys) / sizeof(kComboRetiredKeys[0]);

constexpr bool ComboKeyHasPrefix(const char* key, const char* prefix) {
    for (std::size_t i = 0; prefix[i] != '\0'; i++) {
        if (key[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

/// Every key under the identity sub-namespace is classified Identity. A
/// compile-time twin of the lock's runtime check, so a row added as
/// Preference by mistake is a red build before it is a red test.
constexpr bool ComboIdentityKeysAreIdentity() {
    for (const ComboKey& row : kComboKeys) {
        if (ComboKeyHasPrefix(row.key, kComboIdentityKeyPrefix) && row.cls != ComboKeyClass::Identity) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// The curated MM enhancement toggles hosted in the unified menu (#682)
// ============================================================================
/**
 * WHY THIS TABLE IS HERE. `games/mm/2s2h/BenGui/BenMenu.cpp` is excluded from
 * the single exe, so MM's own enhancement toggles have no widget anywhere: the
 * only way to reach them was `shipofharkinian.json` or the console. #682 asks
 * for a CURATED allowlist rather than BenMenu's whole table, each row carrying
 * ADR 0004 section 5's three-part liveness evidence (the provider TU links, its
 * registrar runs, its hook type has an MM dispatch point).
 *
 * The rows live in this header, next to the classification tables, for the
 * reason the MM randomizer-options table gives for its own descriptors: a menu
 * row that spells its own CVar literal can drift from the provider that reads
 * it, and the drift is invisible — the widget flips a key nothing consults
 * (#499's exact failure). One table, read by the menu rows AND by the locks,
 * makes a typo a red test instead of a dead control.
 *
 * WHERE THE ROWS ARE (#1133, ruling D5). Until #1133 a Combo > Majora's Mask
 * page drew this table; the Combo header retired, and each OwnRow entry is now
 * a row on the SoH Enhancements page that holds its twin or nearest relative,
 * or in a "Majora's Mask" block there (SohMenuComboMmEnhancements.cpp, through
 * the hosted-row seam). Each such row binds its key's RSBS_CVAR_MM_* define and
 * takes its label, tooltip, widget and presentation from the entry for that
 * define, so the keys below are spelled through those defines.
 *
 * SCOPE, deliberately narrow. Four keys, the four #682 names. Widening the
 * allowlist is a separate decision, because `2ship_enh` is a plain archive by
 * design (see `.github/scripts/check-registrar-elision.sh`: most of its members
 * must legitimately stay dead in single-exe) and "WHOLE_ARCHIVE the lot" is
 * its own call. That script, citing #516, records why the archive stays
 * plain and its registrar audit stays report-only.
 *
 * NOT world identity. Every key here is a PREFERENCE: it shapes how the game
 * plays for this player, never what the paired world contains, so none of them
 * is frozen at creation and none enters a digest. That is why the table is not a
 * `ComboKey` — it needs no `ComboKeyClass` at all.
 */
enum class MmEnhancementLiveness {
    /// ADR 0004 section 5 satisfied on all three legs: the provider TU survives
    /// the link, its registrar runs (or it needs none — see `registrarKey`), and
    /// the hook type it rides has a real MM dispatch point.
    Live,
    /// Some legs live, at least one dormant. Draw the row DISABLED with the
    /// reason; a control that half-arms is worse than no control (#682).
    Partial,
    /// The provider is elided, its registrar never runs, or its hook type has no
    /// MM dispatch point (#438). Draw it disabled with the reason.
    Dormant,
};

/** Where the control for a key lives. */
enum class MmEnhancementHosting {
    /// This page owns the widget: nothing else in the unified menu writes the key.
    OwnRow,
    /// A row ALREADY EXISTS elsewhere in the unified menu and writes this key,
    /// and OoT's `ShipInit::Init` forwards the change to MM's registrar map
    /// (#539/#614), so MM's half re-arms from that click. This page points at it
    /// instead of adding a second writer over one key.
    HostedElsewhere,
};

/**
 * The widget a row draws (#693). Every row before #693 was a boolean read with
 * `CVarGetInteger`, so the page emitted checkboxes unconditionally; the MM
 * autosave interval is an integer in MINUTES, and a checkbox over it would write
 * 1 (a one-minute interval) for "on" and 0 for "off" (an interval of zero, which
 * saves on every eligible frame). The kind is data rather than a guess the page
 * makes from the key, so the menu lock can assert it.
 */
enum class MmEnhancementWidget {
    /// `WIDGET_CVAR_CHECKBOX`; the provider reads the key as a boolean.
    Checkbox,
    /// `WIDGET_CVAR_SLIDER_INT` over [sliderMin, sliderMax], default
    /// `sliderDefault`, which MUST equal the provider's own read default so that
    /// "reset to default" and "never touched" mean the same value.
    SliderInt,
};

/** One hosted MM enhancement, with the evidence for its liveness class. */
struct HostedMmEnhancement {
    const char* key;     ///< The CVar the provider TU reads. Never NULL or empty.
    const char* label;   ///< Menu row name (OwnRow) or the row that points at it.
    const char* tooltip; ///< One-or-two sentences for the player. Never empty.
    /// Repo-relative provider TU(s) that READ `key`, with line anchors. This is
    /// leg 1 of section 5's test written down.
    const char* provider;
    /// The `S2H::ShipInit` path the provider's registrar is keyed on — normally
    /// `key` itself. NULL for a provider that needs NO registrar because it
    /// reads the CVar inline on a path the frame loop already runs (leg 2 is
    /// then not "the registrar ran" but "there is nothing to arm").
    const char* registrarKey;
    /// How a lock ATTRIBUTES the registrar to this provider without naming its
    /// symbol (naming it would itself be the inbound reference that keeps the TU
    /// in the link, and the probe would pass vacuously — the discipline
    /// mm_registrar_coverage_test.cpp and mm_clock_shuffle_songs_test.cpp
    /// follow). EMPTY exactly when `registrarKey` is NULL.
    const char* registryProbe;
    MmEnhancementHosting hosting;
    MmEnhancementLiveness liveness;
    /// Why the row is not fully live, for the disabled tooltip. EMPTY (never
    /// NULL) exactly when `liveness` is Live — a dormant row with no reason is a
    /// control that looks broken, and a live row with one is a control that
    /// looks broken and is not.
    const char* reason;

    // ---- #693: trailing and defaulted, so the checkbox rows need no edit ----
    MmEnhancementWidget widget = MmEnhancementWidget::Checkbox;
    int sliderMin = 0;                  ///< SliderInt only; 0 for a Checkbox.
    int sliderMax = 0;                  ///< SliderInt only; 0 for a Checkbox.
    int sliderDefault = 0;              ///< SliderInt only: the provider's read default.
    const char* sliderFormat = nullptr; ///< SliderInt only: the value's printf format.
    /// A key whose value gates whether this row means anything at all: the row is
    /// HIDDEN while it reads 0, the precedent being OoT's own "Notification on
    /// Autosave" row (SohMenuEnhancements.cpp). NULL for an ungated row. This is
    /// not a liveness class: the provider is linked either way; the gate says the
    /// SETTING is moot while its parent feature is off.
    const char* shownWhileKey = nullptr;

    // ---- #747: trailing and defaulted, so every Live row needs no edit ----
    /// The tracker issue that records why the row is not fully live: nonzero
    /// EXACTLY when `liveness` is not Live. The capability registry's split
    /// (`SohMenuCapabilityRecord{gate, issue}`, ADR 0004's 2026-09-27
    /// amendment) applied to this table: `reason` is player text drawn after
    /// "- " in SoH's disabled tooltip and never prints a tracker number, so the
    /// number lives here, where the locks read it. A non-live row with no issue
    /// is a reason nobody can trace and so nobody retires (#438's remainder,
    /// then #669).
    uint32_t issue = 0;
};

inline constexpr HostedMmEnhancement kHostedMmEnhancements[] = {
    // MM's kaleido game-over chain: "GAME OVER", the save prompt, "Continue
    // playing?". NO REGISTRAR AT ALL — both readers are inline CVarGetInteger
    // calls in MM's own decomp, on paths z_play.c already drives every frame
    // (MM_GameOver_Update at z_play.c:1173, the kaleido prompt draw inside
    // KaleidoScope_Draw), and both TUs are in `2ship_src`, pulled by real
    // references rather than by archive semantics. So leg 2 is vacuous by
    // construction and leg 3 is z_play's own switch. #653 traced MM death
    // straight to vanilla respawn because NOTHING in this repository writes the
    // key: no widget, no ShipInit seed, no preset. This row is that fix. The
    // operator's 2026-09-16 ruling keeps the DEFAULT (off, vanilla respawn);
    // exposing the toggle does not change it.
    { RSBS_CVAR_MM_KALEIDO_GAME_OVER, "Game Over Prompt",
      "Opens the Game Over screen and its save prompt on death instead of respawning at the area entrance; the "
      "screen's artwork is not drawn yet. Majora's Mask only.",
      "games/mm/src/code/z_game_over.c:84, "
      "games/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c:978",
      nullptr, "", MmEnhancementHosting::OwnRow, MmEnhancementLiveness::Live, "" },

    // The two ClockShuffle-consuming Songs enhancements. #679 carved their TUs
    // into `2ship_enh_clockshuffle` and links it WHOLE_ARCHIVE, and
    // check-registrar-elision.sh audits that archive as `required`, so leg 1 is
    // enforced on every CI build. Legs 2 and 3 are MMClockShuffleSongs'
    // registry-content probes.
    { RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME, "Better Double Time",
      "Lets the Song of Double Time pick any time of day instead of only the next dawn or dusk, and with Clock "
      "Shuffle on refuses a half-day this file does not own. Majora's Mask only.",
      "games/mm/2s2h/Enhancements/Songs/BetterSongOfDoubleTime.cpp:472", "gEnhancements.Songs.BetterSongOfDoubleTime",
      "ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT]", MmEnhancementHosting::OwnRow,
      MmEnhancementLiveness::Live, "" },
    { RSBS_CVAR_MM_SKIP_SOT_CUTSCENES, "Skip Time Song Cutscenes",
      "Skips the Song of Time, Inverted Song of Time and Song of Double Time cutscenes, and with Clock Shuffle on "
      "returns the Song of Time to the earliest half-day this file owns. Majora's Mask only.",
      "games/mm/2s2h/Enhancements/Songs/SkipSoTCutscenes.cpp:101", "gEnhancements.Songs.SkipSoTCutscenes",
      "OnActorUpdate[ACTOR_EN_TEST6]", MmEnhancementHosting::OwnRow, MmEnhancementLiveness::Live, "" },

    // AUTOSAVE IS ALREADY HOSTED, and #682's premise for this one row is false.
    // `gEnhancements.Autosave` is the CANONICAL converged key (see
    // kConvergedKeys above: MM's `gEnhancements.Saving.Autosave` retired onto
    // it), and OoT's own Enhancements page has carried a checkbox on it all
    // along (games/oot/soh/SohGui/SohMenuEnhancements.cpp:156). Since #539 every
    // SohMenu widget funnels through OoT's `ShipInit::Init`, which forwards to
    // `MM_ShipInit_OnCVarChanged`, and since #614/#629 MM's `RegisterAutosave`
    // sits in `S2H::ShipInit` under this exact key — so that one click already
    // re-arms BOTH halves. A second checkbox here would be a second writer over
    // one key with nothing to gain, which is the duplicate-host shape #655
    // removed for the tier-4 rules. Until #1133 the Combo > Majora's Mask page
    // drew this entry as a pointer (a separator carrying `label` over a gray
    // note that was `tooltip`); since the rows moved onto SoH's pages it draws
    // nothing: the interval's row sits directly after SoH's Autosave, whose
    // tooltip says it applies to both games, so SoH's row IS the one row
    // (#1119's form). The entry stays as the evidence behind the interval's
    // gate (`shownWhileKey`), which the honesty rule requires to be a key this
    // table accounts for.
    { "gEnhancements.Autosave", "Autosave",
      "Autosave is under Enhancements > Quality of Life and applies to both games.",
      "games/mm/2s2h/Enhancements/Saving/SavingEnhancements.cpp:485", "gEnhancements.Autosave", "OnGameStateDrawFinish",
      MmEnhancementHosting::HostedElsewhere, MmEnhancementLiveness::Live, "" },

    // #693: MM's autosave INTERVAL, the Autosave item's only genuinely
    // un-hosted half. Minutes, 1-60, default 5: BenMenu.cpp's own slider
    // (excluded from this build) and HandleAutoSave's read default, verbatim.
    //
    // THE THREE LEGS. (1) The provider TU links: it is SavingEnhancements.cpp,
    // the same TU as the Autosave row above, whose ShipInit registrar
    // MMEnhancementToggles leg 1 counts (and GameExports_SingleExe.cpp's
    // extern "C" references pin it besides). (2) NO REGISTRAR OF ITS OWN, so
    // `registrarKey` is NULL: HandleAutoSave reads the key inline on every tick
    // through SavingEnhancements_AutosaveIntervalMs, so a change takes effect on
    // the next tick with nothing to re-arm. What arms the tick is
    // RegisterAutosave under gEnhancements.Autosave, which is exactly
    // `shownWhileKey`: while Autosave is off the interval is moot and the row
    // hides. (3) The hook HandleAutoSave rides, OnGameStateUpdate, is dispatched
    // by MM's game.c (GameState_Update) through the single-exe S2H::GameHooks
    // executor in GameExports_SingleExe.cpp. MMEnhancementToggles leg 6 measures
    // the read site itself: key cleared, at the minimum and at the maximum; leg 7
    // measures the CONSUMER: it arms the gate, invokes the one OnGameStateUpdate
    // registrant that arming added (the real tick into HandleAutoSave) with the
    // last save 90 s ago, and requires the interval check to pass at 1 minute and
    // block at 2, cleared and the maximum, then that the tick is gone with the
    // gate off.
    //
    // DISTINCT FROM OoT, and it must stay so (kMustStayDistinct above, the
    // static_assert at the end of this header, and CvarClassification's (3c-2)
    // scan, which fails if games/oot ever names this key). OoT's interval is a hardcoded
    // THREE_MINUTES_IN_UNIX with no CVar, so there is no OoT key to converge
    // ONTO. Converging would mean either a slider that claims to set OoT's
    // interval and does nothing there (#499's shape again), or an edit to OoT's
    // vendored Autosave.cpp that invents a CVar upstream does not have and moves
    // OoT's 3-minute behaviour onto MM's 5-minute default without anyone asking.
    // Hence the "Majora's Mask only" label. A preference: no freeze, no digest,
    // no .redsave impact.
    { RSBS_CVAR_MM_AUTOSAVE_INTERVAL, "MM Autosave Interval: %d minutes",
      "Sets how often the owl autosave fires; Ocarina of Time keeps its fixed 3 minutes. Majora's Mask only.",
      "games/mm/2s2h/Enhancements/Saving/SavingEnhancements.cpp:216 (read by HandleAutoSave, :223)", nullptr, "",
      MmEnhancementHosting::OwnRow, MmEnhancementLiveness::Live, "", MmEnhancementWidget::SliderInt, 1, 60, 5,
      "%d minutes", "gEnhancements.Autosave" },
};

inline constexpr std::size_t kHostedMmEnhancementCount =
    sizeof(kHostedMmEnhancements) / sizeof(kHostedMmEnhancements[0]);

/** constexpr `str == ""`, for the honesty check below. A NULL reads as empty. */
constexpr bool MmEnhStringEmpty(const char* s) {
    return s == nullptr || s[0] == '\0';
}

/** constexpr `strcmp(a, b) == 0`. Both must be non-NULL. */
constexpr bool MmEnhStringEqual(const char* a, const char* b) {
    std::size_t i = 0;
    for (; a[i] != '\0' && b[i] != '\0'; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return a[i] == b[i];
}

/** constexpr: is `key` held apart as an MM key in kMustStayDistinct? */
constexpr bool MmEnhKeyMustStayDistinct(const char* key) {
    for (const DistinctPair& pair : kMustStayDistinct) {
        if (MmEnhStringEqual(pair.mmKey, key)) {
            return true;
        }
    }
    return false;
}

/**
 * The invariants a row cannot be allowed to break, checked at COMPILE time so a
 * badly-classified row is a red build before it is a red test:
 *
 *  - key, label, tooltip and provider are all present. A row with no provider
 *    has no leg-1 evidence at all, which is the claim-without-measurement #669
 *    was filed about.
 *  - `reason` is empty EXACTLY when the row is Live. Both directions.
 *  - (#747) `issue` is zero EXACTLY when the row is Live. Both directions: a
 *    non-live row names the tracker that retires it, and a Live row has none.
 *  - `registryProbe` is empty EXACTLY when `registrarKey` is NULL. A probe with
 *    no registrar to attribute, or a registrar nothing can attribute, is a
 *    liveness claim nobody can check.
 *  - keys are unique. Two rows on one key would draw two controls over it.
 *  - (#693) a Checkbox row carries no slider data; a SliderInt row has a
 *    non-empty range with its default inside it and a format, and is its own
 *    writer (a pointer row draws no control, so a slider kind there is a lie).
 *  - (#693) a gate key is never the row's own key, and names a key some OTHER
 *    row of this manifest accounts for, so the gate is itself a hosted,
 *    evidenced key rather than a free-floating literal.
 */
constexpr bool HostedMmEnhancementRowsAreHonest(const HostedMmEnhancement* rows, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        const HostedMmEnhancement& row = rows[i];
        if (MmEnhStringEmpty(row.key) || MmEnhStringEmpty(row.label) || MmEnhStringEmpty(row.tooltip) ||
            MmEnhStringEmpty(row.provider)) {
            return false;
        }
        if (row.reason == nullptr || row.registryProbe == nullptr) {
            return false;
        }
        if (MmEnhStringEmpty(row.reason) != (row.liveness == MmEnhancementLiveness::Live)) {
            return false;
        }
        if ((row.issue == 0) != (row.liveness == MmEnhancementLiveness::Live)) {
            return false;
        }
        if (MmEnhStringEmpty(row.registryProbe) != (row.registrarKey == nullptr)) {
            return false;
        }
        for (std::size_t j = i + 1; j < count; j++) {
            if (MmEnhStringEqual(row.key, rows[j].key)) {
                return false;
            }
        }
        if (row.widget == MmEnhancementWidget::Checkbox) {
            if (row.sliderMin != 0 || row.sliderMax != 0 || row.sliderDefault != 0 || row.sliderFormat != nullptr) {
                return false;
            }
        } else {
            if (row.hosting != MmEnhancementHosting::OwnRow || row.sliderMin >= row.sliderMax ||
                row.sliderDefault < row.sliderMin || row.sliderDefault > row.sliderMax ||
                MmEnhStringEmpty(row.sliderFormat)) {
                return false;
            }
        }
        if (row.shownWhileKey != nullptr) {
            if (MmEnhStringEqual(row.shownWhileKey, row.key)) {
                return false;
            }
            bool gateHosted = false;
            for (std::size_t j = 0; j < count; j++) {
                if (j != i && MmEnhStringEqual(row.shownWhileKey, rows[j].key)) {
                    gateHosted = true;
                }
            }
            if (!gateHosted) {
                return false;
            }
        }
    }
    return true;
}

/** The shipped manifest's honesty. The table-taking form above exists so the
 *  locks can hold a SYNTHETIC non-live table to the same rules and watch each
 *  one go red (MenuMmEnhancementRows leg 7), which the all-Live shipped table
 *  cannot show. */
constexpr bool HostedMmEnhancementsAreHonest() {
    return HostedMmEnhancementRowsAreHonest(kHostedMmEnhancements, kHostedMmEnhancementCount);
}

inline constexpr std::size_t kConvergedKeyCount = sizeof(kConvergedKeys) / sizeof(kConvergedKeys[0]);
inline constexpr std::size_t kRetiredKeyPrefixCount = sizeof(kRetiredKeyPrefixes) / sizeof(kRetiredKeyPrefixes[0]);
inline constexpr std::size_t kMustStayDistinctCount = sizeof(kMustStayDistinct) / sizeof(kMustStayDistinct[0]);
inline constexpr std::size_t kSharedIntentKeyCount = sizeof(kSharedIntentKeys) / sizeof(kSharedIntentKeys[0]);
inline constexpr std::size_t kDeliberateSharedCheatKeyCount =
    sizeof(kDeliberateSharedCheatKeys) / sizeof(kDeliberateSharedCheatKeys[0]);
// kDisputedClassificationKeyCount is defined at the (now-empty) table itself,
// because that table is a null pointer with no sizeof to divide — see there.
inline constexpr std::size_t kMenuIndexKeyCount = sizeof(kMenuIndexKeys) / sizeof(kMenuIndexKeys[0]);

// Tier 1 (#34) landed 9 keys — 6 audio + 3 tunic. #462 adds 23 of the inventory
// §5.3 remainder's 26 rows (the 3 held out are ClimbSpeed and the two parked
// cheat pairs, all in kMustStayDistinct). 9 + 23 = 32. The #966 audit adds 11
// key-name-gap rows (#1027-#1036 and #1038; #1037's damage multiplier is held
// out in kMustStayDistinct), and #1062 / #1064 add four more (the ocarina D-pad,
// right-stick and no-dropped-input switches, Containers Match Contents), and #1123 one (Skip Boss
// Introductions), and #1239 one (the fairy's call). 32 + 11 + 4 + 1 + 1 = 49. Pinning the count makes a silently
// dropped or double-added row a compile error.
static_assert(kConvergedKeyCount == 49, "tier 1 (9) + inventory §5.3 remainder (23 of 26; 3 held out) + the #966 "
                                        "key-name gaps (11 of 12; #1037 held out) + #1062 (3) + #1064 (1) + #1123 (1) "
                                        "+ #1239 (1) = 49");

// ADR 0003 Appendix B measured 26 class-(S) collisions. DebugSaveFileMode
// rejoined when #454 was fixed (MM's clobber retired, read default aligned to
// OoT's 1). Two of the 26 were never shared: the draw-distance and
// widescreen-culling spellings matched only in OoT's uncalled ShipUtils.cpp, so
// #966 (#1027, #1028) converged them as ordinary rows and this table carries 24.
// Pinning the count makes a silent drop a compile error rather than a quietly
// weaker lock.
static_assert(kSharedIntentKeyCount == 24,
              "ADR 0003 Appendix B's 26 class-(S) keys less the two actor-cull spellings #966 moved to converged "
              "rows (#1027, #1028); includes DebugSaveFileMode — restored to (S) by #454 once MM's "
              "DeveloperTools.cpp write was retired and its read default aligned to OoT's 1");
static_assert(kMenuIndexKeyCount == 4, "four menu-index keys — #451");
// The disputed table is empty since #454's fix. Pinned at 0 like every other
// table here, so a key silently re-added to it is a compile error rather than a
// quietly reopened dispute.
static_assert(kDisputedClassificationKeyCount == 0,
              "no disputed keys — gDeveloperTools.DebugSaveFileMode was resolved to (S) and moved into "
              "kSharedIntentKeys by #454");

inline constexpr std::size_t kComboKeyCount = sizeof(kComboKeys) / sizeof(kComboKeys[0]);
// Eighteen identity keys (the five of ADR 0011 increment 2 less the two pool sizes
// #801 and the two item classes #834 retired, Goal (ADR 0010 D1),
// SharedKokiriSword (#886), SharedShields (#1068), SharedLens (#1065),
// SharedSongEpona and SharedSongStorms (#1066), SharedOcarinaButtons (#1067),
// SharedSongTime (#1069), SharedArrowFire, SharedArrowIce and SharedArrowLight (#968), SharedBottles
// (#970), SharedMaskGoron, SharedMaskZora, SharedMaskBunny, SharedMaskKeaton and
// SharedMaskTruth (#978); #879 retired SharedOcarina (#668)) + four
// window-visibility preferences (the MM randomizer options window's left with
// it on 2026-09-27, when the options became Combo pages; #458 U0 added the
// unified item tracker overlay's) + twelve gCombo.Tracker.Items.* preferences
// (#458 U2, the overlay's look: nine, of which #1141 retired the five SoH twins
// into kComboRetiredKeys and added MM's eight group keys) + two
// gCombo.Tracker.Checks.* preferences (#1131, the Combo Tracker's navigation).
// Pinning the count makes a silently dropped row a compile error; the lock's
// tree scan makes a silently ADDED key a red test.
static_assert(kComboKeyCount == 36, "eighteen gCombo.Rando.* identity keys + four gCombo.Windows.* preferences + "
                                    "twelve gCombo.Tracker.Items.* preferences + two gCombo.Tracker.Checks.* "
                                    "preferences = 36");
static_assert(kComboRetiredKeyCount == 5, "#1141: the Item Tracker overlay's five twins of SoH's keys");

// #682's curated allowlist was exactly the four keys that issue named; #693 adds
// a fifth, deliberately: the Autosave row's MM-only interval, whose provider is
// the same already-linked TU (SavingEnhancements.cpp), so it widens no archive
// and is not the "WHOLE_ARCHIVE the lot" call (2ship_enh stays a plain archive;
// check-registrar-elision.sh and #516 record why). Pinning the count makes
// both a silently dropped row and a quietly WIDENED allowlist a compile error.
static_assert(kHostedMmEnhancementCount == 5,
              "the curated MM enhancement allowlist: #682's Kaleido.GameOver, Songs.BetterSongOfDoubleTime, "
              "Songs.SkipSoTCutscenes and Autosave, plus #693's Saving.AutosaveInterval");
// #693: hosting the interval must not quietly converge it with OoT's. OoT has no
// interval CVar (THREE_MINUTES_IN_UNIX), so the key stays an MM-only key held in
// kMustStayDistinct. This assert is unconditional and does not look at the
// manifest: all it refuses is the key being DROPPED from that table, the first
// step of exactly the rename the table exists to refuse. The other direction --
// OoT starting to read the key while it stays listed -- is not a compile-time
// question; CvarClassification's (3c-2) source scan is what fails on it.
static_assert(MmEnhKeyMustStayDistinct("gEnhancements.Saving.AutosaveInterval"),
              "the hosted MM autosave interval must stay in kMustStayDistinct: OoT hardcodes its own 3-minute "
              "interval with no CVar, so the two intervals must not converge (#693)");
static_assert(HostedMmEnhancementsAreHonest(),
              "a hosted MM enhancement row is dishonest: a missing key/label/tooltip/provider, a reason or an issue "
              "that does not match its liveness class, a registry probe with no registrar (or the reverse), or two "
              "rows on one key");
static_assert(ComboIdentityKeysAreIdentity(),
              "every gCombo.Rando.* key authors the frozen ComboSettingsRecord and MUST be classified Identity "
              "(ADR 0004 §6 state 4; ADR 0011 increment 2)");

} // namespace RSBS

#endif // __cplusplus

#endif // RSBS_CVAR_SHARED_KEYS_H
