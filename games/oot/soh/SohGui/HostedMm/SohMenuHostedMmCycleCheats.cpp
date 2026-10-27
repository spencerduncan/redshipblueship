/**
 * @file SohMenuHostedMmCycleCheats.cpp
 * @brief Majora's Mask's cheats and Time Cycle options, hosted on SoH's Enhancements > Cheats and Enhancements >
 *        Quality of Life pages (#1236, #1240), through the hosted-row seam.
 *
 * 2Ship draws these rows in its own menu (the excluded BenMenu.cpp), so in the single exe nothing wrote the keys and
 * their providers were left out of the link. Each key is bound here through its RSBS_CVAR_MM_* define and read by
 * the 2s2h TU games/mm/CMakeLists.txt now links whole (W6-CYCLE-CHEATS):
 *  - Cheats > Infinite...: "MM Consumables" right after SoH's Ammo (Cheats/Infinite.cpp). The pair is PARKED in
 *    kMustStayDistinct: MM refills every consumable (beans and the Powder Keg too), SoH its ammo.
 *  - Cheats > Items: "MM Unrestricted Items" right after SoH's Unrestricted Items (Cheats/UnrestrictedItems.cpp).
 *    PARKED too: MM lets every form use every item, SoH lifts the age restrictions.
 *  - the Cheats page's "Majora's Mask" block, at the end of the page: Longer Deku Flower Glide, Elegy of Emptiness
 *    Anywhere, Stop Time in Dungeons and Unbreakable Razor Sword. None has an OoT twin.
 *  - Quality of Life > Time of Day, a "Majora's Mask" block: the Time Cycle rows 2Ship keeps under Gameplay.
 *
 * Do Not Reset Rupees and Do Not Reset Consumables have NO row, on purpose. The combo already keeps the shared rupees
 * and the five shared ammo counts through every Song of Time (MM_RegisterSharedResourceCycleHooks), and the Powder
 * Keg and the Magic Beans follow the world's frozen Keep Items on Cycle Reset (#1175). Under one-game semantics a
 * combo-level decision freezes at file creation, so a live per-player switch may not overrule it; Cycle/EndOfCycle.cpp
 * holds those two restores off in the single exe.
 *
 * Every row keeps 2Ship's wording, in SoH's Title Case, and its tooltip ends "Majora's Mask only."
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

#include <map>

namespace SohGui {

using namespace UIWidgets;

// Cheats > Infinite..., right after SoH's Ammo.
void AddMmInfiniteConsumablesRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Infinite...", "MM Consumables", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CHEATS_INFINITE_CONSUMABLES)
        .Options(CheckboxOptions().Tooltip("Keeps every consumable you have found at its maximum, the Magic Beans and "
                                           "the Powder Keg included. Majora's Mask only."));
}

// Cheats > Items, right after SoH's Unrestricted Items.
void AddMmUnrestrictedItemsRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Items", "MM Unrestricted Items", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CHEATS_UNRESTRICTED_ITEMS)
        .Options(CheckboxOptions().Tooltip("Allows every form to use every item. Majora's Mask only."));
}

// The end of the Cheats page: MM's cheats with no OoT twin.
void AddMmCheatsBlock(SohMenu& menu, WidgetPath& path) {
    // Static storage: the combobox keeps a pointer to the map. 2Ship's TimeStopOptions order (Enhancements.h).
    static const std::map<int32_t, const char*> timeStopOptions = {
        { 0, "Off" },
        { 1, "Temples" },
        { 2, "Temples + Mini Dungeons" },
    };
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Longer Deku Flower Glide", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CHEATS_LONGER_FLOWER_GLIDE)
        .Options(CheckboxOptions().Tooltip(
            "Allows Deku Link to glide longer, no longer dropping after a certain distance. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Elegy of Emptiness Anywhere", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CHEATS_ELEGY_ANYWHERE)
        .Options(CheckboxOptions().Tooltip("Allows the Elegy of Emptiness to be played outside of Ikana. Majora's "
                                           "Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Stop Time in Dungeons", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_CHEATS_TEMPLE_TIME_STOP)
        .Options(ComboboxOptions()
                     .ComboMap(timeStopOptions)
                     .DefaultIndex(0)
                     .Tooltip("Stops the clock in the four temples, or also in the Spider Houses, Pirates' Fortress, "
                              "Beneath the Well, Ikana Castle and the Secret Shrine. Applies on a room change. "
                              "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Unbreakable Razor Sword", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CHEATS_UNBREAKABLE_RAZOR_SWORD)
        .Options(CheckboxOptions().Tooltip("Allows the Razor Sword to be used indefinitely without dulling its blade. "
                                           "Always on in a randomizer file. Majora's Mask only."));
}

// Quality of Life > Time of Day: MM's Time Cycle options, under their own block.
void AddMmTimeCycleBlock(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Do Not Reset Bottle Contents", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CYCLE_DO_NOT_RESET_BOTTLE_CONTENT)
        .Options(
            CheckboxOptions().Tooltip("Playing the Song of Time does not empty your bottles. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Do Not Reset Razor Sword", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CYCLE_DO_NOT_RESET_RAZOR_SWORD)
        .Options(CheckboxOptions().Tooltip("Playing the Song of Time does not turn the Razor Sword back into the "
                                           "Kokiri Sword. Always on in a randomizer file. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Do Not Reset Time Speed", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CYCLE_DO_NOT_RESET_TIME_SPEED)
        .Options(CheckboxOptions().Tooltip("Playing the Song of Time keeps the clock speed the Inverted Song of Time "
                                           "set. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Keep Express Mail", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CYCLE_KEEP_EXPRESS_MAIL)
        .Options(CheckboxOptions().Tooltip("Keeps the Express Mail to Mama after its first delivery, so both "
                                           "deliveries can be made in one cycle. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Stop Oceanside Spider House Squatter", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CYCLE_STOP_OCEANSIDE_SPIDER_HOUSE_SQUATTER)
        .Options(CheckboxOptions().Tooltip("The Oceanside Spider House squatter does not move in until you have "
                                           "spoken to him. Always on in a randomizer file. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Oceanside Wallet Any Day", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CYCLE_OCEANSIDE_WALLET_ANY_DAY)
        .Options(CheckboxOptions().Tooltip("Allows the Oceanside Spider House wallet reward to be collected on any "
                                           "day. Always on in a randomizer file. Majora's Mask only."));
}

} // namespace SohGui
