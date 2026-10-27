/**
 * @file SohMenuHostedMmModesDifficulty.cpp
 * @brief Majora's Mask's Modes and Difficulty rows, hosted on SoH's Enhancements pages through the hosted-row seam
 *        (#1236, #1240, lane W6-MODES-DIFF).
 *
 * Twelve 2Ship rows whose only widgets sat in the excluded BenMenu.cpp, so nothing in the single exe wrote their keys
 * and none of their TUs was linked (games/mm/CMakeLists.txt carves them into ship__Enh_registrars beside this change).
 * Each row binds its RSBS_CVAR_MM_* define (src/common/cvar_shared_keys.h), and SoH's page calls each function below
 * in one line:
 *  - Extra Modes: a "Majora's Mask" block at the end of the first column (Play as Kafei, Hyrule Warriors Styled Link,
 *    Time Moves When You Move). SoH has no twin of any of them.
 *  - Difficulty: a "Majora's Mask" block after SoH's Enemies section, for 2Ship's Difficulty rows and the two rows of
 *    its 'Other' block (Milk Run Reward Options, Ammo Buyback Options), which have no nearer SoH relative.
 *
 * What is NOT here:
 *  - Mirrored World. SoH's own row on Extra Modes is the one control; it reaches MM through a translation
 *    (RSBS::kTranslatedKeys, games/mm/2s2h/Enhancements/Modes/MirroredWorld.cpp), so it gets no second row.
 *  - Delete File on Death (INTENTIONAL, no row). A paired file is one game: a death may delete only the whole paired
 *    save, and 2Ship's DeleteFileOnDeath.cpp calls MM's SaveManager, which the single exe does not compile.
 *
 * The comboboxes' maps follow 2Ship's option orders (games/mm/2s2h/Enhancements/Enhancements.h), which games/oot
 * cannot include; static storage, since a combobox keeps a pointer to its map.
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

#include <map>

namespace SohGui {

using namespace UIWidgets;

/** Enhancements > Extra Modes, at the end of the first column. */
void AddMmExtraModesRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Play as Kafei", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MODES_PLAY_AS_KAFEI)
        .Options(CheckboxOptions().Tooltip(
            "Plays as Kafei instead of Link in his Hylian form, from the next scene change. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Hyrule Warriors Styled Link", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MODES_HYRULE_WARRIORS_STYLED_LINK)
        .Options(CheckboxOptions().Tooltip("Hangs the Keaton and Fierce Deity masks on Link once you have them, as he "
                                           "wears them in Hyrule Warriors. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Time Moves When You Move", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MODES_TIME_MOVES_WHEN_YOU_MOVE)
        .Options(CheckboxOptions().Tooltip(
            "Makes time pass only while Link moves or plays the ocarina. Majora's Mask only."));
}

/** Enhancements > Difficulty, after SoH's Enemies section. */
void AddMmDifficultyRows(SohMenu& menu, WidgetPath& path) {
    static const std::map<int32_t, const char*> dekuGuardSearchBallsOptions = {
        { 0, "Night Only" },
        { 1, "Never" },
        { 2, "Always" },
    };
    static const std::map<int32_t, const char*> gibdoTradeSequenceOptions = {
        { 0, "Vanilla" },
        { 1, "MM3D" },
        { 2, "No Trade" },
    };
    static const std::map<int32_t, const char*> hiddenGrottosVisibilityOptions = {
        { 0, "Off" },
        { 1, "Wear Mask of Truth" },
        { 2, "Have Mask of Truth" },
        { 3, "Always" },
    };
    static const std::map<int32_t, const char*> cremiaRewardOptions = {
        { 0, "Vanilla" },
        { 1, "Hug" },
        { 2, "Rupee" },
    };
    static const std::map<int32_t, const char*> ammoBuybackOptions = {
        { 0, "Vanilla" },
        { 1, "Full Price" },
        { 2, "Half Price" },
    };

    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Jinxed Timer: %d seconds", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_JINXED_TIMER)
        .Options(IntSliderOptions()
                     .Min(0)
                     .Max(60)
                     .DefaultValue(60)
                     .Format("%d seconds")
                     .Tooltip("Sets how long a Jinx lasts; 0 prevents it entirely. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Lower Bank Reward Thresholds", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_LOWER_BANK_REWARD_THRESHOLDS)
        .Options(CheckboxOptions().Tooltip("Lowers the rupees the bank asks for its rewards from 200, 1000 and 5000 "
                                           "to 100, 500 and 1000. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Disable Takkuri Steal", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_DISABLE_TAKKURI_STEAL)
        .Options(CheckboxOptions().Tooltip("Stops the Takkuri stealing key items like bottles and swords; it may "
                                           "still steal other items. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Deku Guard Search Balls", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_DEKU_GUARD_SEARCH_BALLS)
        .Options(ComboboxOptions()
                     .ComboMap(dekuGuardSearchBallsOptions)
                     .DefaultIndex(0)
                     .Tooltip("Chooses when the Deku Palace guards' search balls show: Never (as in Majora's Mask "
                              "3D), Night Only (as on the N64) or Always. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Gibdo Trade Sequence Options", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_GIBDO_TRADE_SEQUENCE)
        .Options(ComboboxOptions()
                     .ComboMap(gibdoTradeSequenceOptions)
                     .DefaultIndex(0)
                     .Tooltip("Changes the Gibdo trades: MM3D takes one of each item and accepts a Red Potion for a "
                              "Blue one; No Trade makes them leave without taking anything. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Hidden Grottos Visibility", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_HIDDEN_GROTTOS_VISIBILITY)
        .Options(ComboboxOptions()
                     .ComboMap(hiddenGrottosVisibilityOptions)
                     .DefaultIndex(0)
                     .Tooltip("Marks hidden grottos with sparkles while you wear the Mask of Truth (as in Majora's "
                              "Mask 3D), once you have it, or always. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Frog Choir Count: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_FROG_CHOIR_COUNT)
        .Options(IntSliderOptions().Min(1).Max(5).DefaultValue(5).Format("%d").Tooltip(
            "Sets how many frogs you need to save for the choir performance. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Milk Run Reward Options", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_CREMIA_HUGS)
        .Options(ComboboxOptions()
                     .ComboMap(cremiaRewardOptions)
                     .DefaultIndex(0)
                     .Tooltip("Chooses the reward for winning the Milk Run after the first time: Vanilla (either), "
                              "Hug or Rupee. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Ammo Buyback Options", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_ITEMS_AMMO_BUYBACK)
        .Options(ComboboxOptions()
                     .ComboMap(ammoBuybackOptions)
                     .DefaultIndex(0)
                     .Tooltip("Lets the Curiosity Shop owner buy your ammo, Magic Beans and Powder Kegs at full or "
                              "half price. Majora's Mask only."));
}

} // namespace SohGui
