/**
 * @file combo_preset_profile.h
 * @brief The paired half of a preset's Rando Settings block (#1368): MM's
 *        options, trap types, tricks and excluded checks, and the Cross-Game
 *        rules, saved beside SoH's gRandoSettings and applied through each
 *        key's own writer.
 *
 * WHY. SoH's Settings > Presets saves the Rando Settings block as the
 * gRandoSettings CVar subtree and applies it with Config::SetBlock. On a paired
 * file that is half the profile: the MM half (gRando.Options.*, gRando.Traps.*,
 * gRando.Tricks.*, gRando.ExcludedChecks) and the rules (gCombo.Rando.*) shape
 * the same world and freeze into the same identity, so a preset that left them
 * out gave a second player a different MM digest and a different world from the
 * preset's author. OoTMM's presets are whole settings; so is this block now.
 *
 * WHY NOT SetBlock. These keys have writers that refuse what must not be
 * written: Combo_MMTrickSetValue refuses a frozen, reserved or unbound trick,
 * Combo_ComboSettingSet a frozen record or an out-of-space value,
 * Combo_MMLocationsSetExcluded a frozen profile or Vanilla logic. A block write
 * would go round every one of them, and the gRando/gCombo roots also hold keys
 * that are not randomizer settings (tracker layout, spoiler file, windows). So
 * the profile is applied key by key through the writers, and only keys a
 * registered table describes are touched.
 *
 * THE SHAPE. A JSON object mirroring the CVar tree, as SoH's blocks do:
 *
 *     { "gRando": { "Options": { "RO_SHUFFLE_GOLD_SKULLTULAS": 1 }, "Tricks": { "MMRT_LENS": 1 },
 *                   "Traps": { "Freeze": 1 }, "ExcludedChecks": "26,77," },
 *       "gCombo": { "Rando": { "Logic": 3, "SharedLens": 1 } } }
 *
 * MM's own Logic (gRando.Options.RO_LOGIC) is not part of it (#1367): the page
 * shows that row read-only, set by [Both Games] Logic, which the profile carries
 * as gCombo.Rando.Logic. A profile that holds it has it refused.
 *
 * stored under COMBO_PRESET_PAIRED_PROFILE_KEY in the block, beside
 * "gRandoSettings". The key is deliberately not a CVar root: a build without
 * this seam applies it as an inert CVar subtree of that name, where a "gRando"
 * entry would have replaced the whole gRando block. A null profile is a reset.
 *
 * Game-header-free: every key comes from a registered descriptor table.
 */

#ifndef RSBS_COMMON_COMBO_PRESET_PROFILE_H
#define RSBS_COMMON_COMBO_PRESET_PROFILE_H

#ifdef __cplusplus

#include <string>

#include <nlohmann/json.hpp>

/** The Rando Settings block's entry for the paired profile, beside gRandoSettings. */
#define COMBO_PRESET_PAIRED_PROFILE_KEY "pairedProfile"

/** What an apply did, for the log line and the locks. */
struct ComboPresetProfileReport {
    /** False when the profile was refused whole (a paired world is decided, or
     *  the value is not an object or null); nothing was written then. */
    bool applied = false;
    /** Keys set to a value the profile holds. */
    int written = 0;
    /** Keys put back to "never chosen" (overwrite, or a reset). */
    int cleared = 0;
    /** Keys the profile holds that a writer refused: a reserved or unbound
     *  trick, an option that cannot change the world in this build, an
     *  out-of-space Cross-Game value, excluded checks under Vanilla logic, an
     *  id no check has, a value of the wrong type. */
    int refused = 0;
    /** Leaves of the profile no table describes (a key of another build, or
     *  one that is not a randomizer setting); never written. */
    int ignored = 0;
};

/**
 * The authored paired profile, as the block stores it: every explicitly chosen
 * MM option and trap type, every trick that is on, the excluded checks (unless
 * Vanilla logic makes the list inert), and every explicitly chosen Cross-Game
 * rule ([Both Games] Logic among them, #1367). Unchosen keys are left out, so
 * applying the result puts them back to "never chosen". An empty object when
 * nothing is chosen.
 */
nlohmann::json ComboPresetProfile_Capture(void);

/**
 * Why a profile cannot be applied now, as the sentence our randomizer pages
 * show ("Already decided when this world was created ..."), or an empty string
 * when it can. A preset is one profile, so while a paired world is decided the
 * whole Rando Settings block is refused, gRandoSettings with it.
 */
std::string ComboPresetProfile_RefusalNote(void);

/**
 * Apply @p profile through the writers. Null resets every paired-profile key.
 * Otherwise each key the profile holds is written, and with @p merge false
 * (SoH's "overwrite" block strategy) every key it does not hold is cleared;
 * with @p merge true those are left alone. Refused whole, writing nothing,
 * while ComboPresetProfile_RefusalNote() is non-empty.
 *
 * Writes the CVar store only; the caller persists it (CVarSave) when the write
 * is the player's.
 */
ComboPresetProfileReport ComboPresetProfile_Apply(const nlohmann::json& profile, bool merge);

#endif // __cplusplus

#endif // RSBS_COMMON_COMBO_PRESET_PROFILE_H
