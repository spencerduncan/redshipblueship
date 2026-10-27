/**
 * @file SohMenuHostedMmMinigames.cpp
 * @brief Majora's Mask's minigame options, hosted on SoH's Enhancements > Minigames page (#1236), through the
 *        hosted-row seam (SohMenuHostedMm.h).
 *
 * 2Ship draws these rows in its own menu (the excluded BenMenu.cpp: Difficulty > Minigames, and Mark Shooting Gallery
 * Octoroks under Accessibility), so in the single exe nothing wrote the keys, and their twelve TUs were left out of
 * the link. Each key is bound here through its RSBS_CVAR_MM_* define (cvar_shared_keys.h, W6-MINIGAMES slot) and read
 * by the 2s2h TU games/mm/CMakeLists.txt now links whole (W6-MINIGAMES). SoH's page calls these in one line each
 * (SohMenuEnhancements.cpp):
 *  - Shooting Gallery: the Town and Swamp archery scores and Mark Gallery Octoroks, after SoH's own gallery rows (its
 *    nearest relatives). They stay usable while SoH's "Customize Behavior" is off: that switch is Hyrule's.
 *  - a "Majora's Mask" block at the end of the page for the rest, which have no SoH twin.
 * None of the keys is shared with SoH. Every slider defaults to MM's vanilla value, the one value that leaves its TU
 * disarmed; the three archery scores keep 2180, 50 and 20 because GalleryTwofer.cpp and SwampBoatSkip.cpp read them.
 * Each TU only moves a win condition: the reward is still the vanilla actor's, which the rando hooks take
 * (mm_minigame_offer_hooks_test.cpp runs its rows with these keys on).
 *
 * Labels keep 2Ship's meaning, shortened to fit SoH's columns where 2Ship's were longer, and every tooltip ends
 * "Majora's Mask only."
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

#include <map>

namespace SohGui {

using namespace UIWidgets;

// Minigames > Shooting Gallery, after SoH's Adult Starting Ammunition.
void AddMmShootingGalleryRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Shooting Gallery", "Town Archery Score: %d points", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_TOWN_ARCHERY_SCORE)
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max(50)
                     .DefaultValue(50)
                     .Format("%d points")
                     .Tooltip("Sets the score that wins the Town Shooting Gallery. Reaching it ends the game. Majora's "
                              "Mask only."));
    AddHostedMmWidget(menu, path, "Shooting Gallery", "Swamp Archery Score: %d points", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_SWAMP_ARCHERY_SCORE)
        .Options(IntSliderOptions()
                     .Min(1000)
                     .Max(2180)
                     .DefaultValue(2180)
                     .Format("%d points")
                     .Tooltip("Sets the score that wins the Swamp Shooting Gallery. A lower score also speeds up the "
                              "final count. "
                              "Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Shooting Gallery", "Mark Gallery Octoroks", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_MARK_SHOOTING_GALLERY_OCTOROKS)
        .Options(CheckboxOptions().Tooltip("Marks each Octorok in the Town Shooting Gallery with whether it should be "
                                           "hit. Majora's Mask only."));
}

// The end of the Minigames page: MM's minigames with no SoH twin.
void AddMmMinigameRows(SohMenu& menu, WidgetPath& path) {
    // Static storage: the combobox keeps a pointer to the map. 2Ship's AlwaysWinDoggyRaceOptions and
    // GoronRaceDifficultyOptions orders (Enhancements.h).
    static const std::map<int32_t, const char*> doggyRaceOptions = {
        { 0, "Off" },
        { 1, "With Mask of Truth" },
        { 2, "Always" },
    };
    static const std::map<int32_t, const char*> goronRaceOptions = {
        { 0, "Vanilla" },
        { 1, "Balanced" },
        { 2, "Skip" },
    };

    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Bombers Hide-and-Seek: %d kids", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_BOMBERS_HIDE_AND_SEEK)
        .Options(IntSliderOptions().Min(1).Max(5).DefaultValue(5).Format("%d kids").Tooltip(
            "Sets how many Bomber Kids must be found to win their hide-and-seek. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Swordsman School: %d points", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_SWORDSMAN_SCHOOL_SCORE)
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max(30)
                     .DefaultValue(30)
                     .Format("%d points")
                     .Tooltip("Sets the score that wins the Swordsman's School. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Honey & Darling Day 1: %d hits", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_HONEY_AND_DARLING_DAY1)
        .Options(IntSliderOptions().Min(1).Max(8).DefaultValue(8).Format("%d hits").Tooltip(
            "Sets the score that wins Honey & Darling's Bombchu game on the first day. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Honey & Darling Day 2: %d hits", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_HONEY_AND_DARLING_DAY2)
        .Options(IntSliderOptions().Min(1).Max(8).DefaultValue(8).Format("%d hits").Tooltip(
            "Sets the score that wins Honey & Darling's Bomb game on the second day. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Honey & Darling Day 3: %d hits", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_HONEY_AND_DARLING_DAY3)
        .Options(IntSliderOptions().Min(1).Max(16).DefaultValue(16).Format("%d hits").Tooltip(
            "Sets the score that wins Honey & Darling's Bow game on the final day. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Romani Practice: %d balloons", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_ROMANI_TARGET_PRACTICE)
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max(10)
                     .DefaultValue(10)
                     .Format("%d balloons")
                     .Tooltip("Sets how many balloons win Romani's target practice. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Always Win Doggy Race", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_ALWAYS_WIN_DOGGY_RACE)
        .Options(ComboboxOptions()
                     .ComboMap(doggyRaceOptions)
                     .DefaultIndex(0)
                     .Tooltip("Makes the dog Link picked win the Doggy Race:\n"
                              "- With Mask of Truth: once Link owns the Mask of Truth.\n"
                              "- Always: every race.\n"
                              "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Cucco Shack Count: %d Cuccos", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_CUCCO_SHACK_CUCCO_COUNT)
        .Options(
            IntSliderOptions()
                .Min(1)
                .Max(10)
                .DefaultValue(10)
                .Format("%d Cuccos")
                .Tooltip(
                    "Sets how many chicks must grow up to make Grog at the Cucco Shack happy. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Skip Gorman Horse Race", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_SKIP_HORSE_RACE)
        .Options(CheckboxOptions().Tooltip("Wins the Gorman Track horse race at once. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Beaver Race Rings: %d rings", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_BEAVER_RACE_RINGS_COLLECTED)
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max(20)
                     .DefaultValue(20)
                     .Format("%d rings")
                     .Tooltip("Sets the rings both beaver races need. At 20 the races keep their own 20 and 25. "
                              "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Skip Little Beaver Races", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_SKIP_LITTLE_BEAVER)
        .Options(CheckboxOptions().Tooltip(
            "Skips the races against the younger beaver brother, so only the older one is raced. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Goron Race", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_DIFFICULTY_GORON_RACE)
        .Options(ComboboxOptions()
                     .ComboMap(goronRaceOptions)
                     .DefaultIndex(0)
                     .Tooltip("Sets how the other Gorons race:\n"
                              "- Vanilla: those ahead of Link slow down, those behind speed up.\n"
                              "- Balanced: those behind Link do not speed up.\n"
                              "- Skip: Link wins at once.\n"
                              "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Boat Archery Score: %d points", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_BOAT_ARCHERY_SCORE)
        .Options(
            IntSliderOptions()
                .Min(1)
                .Max(50)
                .DefaultValue(20)
                .Format("%d points")
                .Tooltip("Sets the first target score of Koume's boat archery in each cycle. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Koume's Health: %d hits", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_MINIGAMES_BOAT_ARCHERY_HEALTH)
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled = CVarGetInteger(RSBS_CVAR_MM_MINIGAMES_BOAT_ARCHERY_INVINCIBLE, 0) != 0;
            info.options->disabledTooltip = "This option is disabled because \"Invincible Koume\" is on.";
        })
        .Options(IntSliderOptions().Min(1).Max(30).DefaultValue(10).Format("%d hits").Tooltip(
            "Sets how many hits Koume takes before boat archery ends. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Invincible Koume", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_BOAT_ARCHERY_INVINCIBLE)
        .Options(CheckboxOptions().Tooltip("Keeps Koume's health from dropping when she is hit in boat archery. "
                                           "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Treasure Shop Full Maze", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MINIGAMES_TREASURE_CHEST_SHOP_FULL_MAZE)
        .Options(CheckboxOptions().Tooltip(
            "Shows the whole Treasure Chest Shop maze instead of only the walls near Link. Majora's Mask only."));
}

} // namespace SohGui
