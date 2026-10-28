/**
 * @file SohMenuComboTrackerSettings.cpp
 * @brief The unified trackers' rows on SoH's own Randomizer > Item Tracker and
 *        Check Tracker pages (#1132, #1141).
 *
 * THE ITEM TRACKER'S ROWS (#1141, lane IT-3; operator ruling IT-D6: one set of
 * settings, SoH's). The unified Item Tracker is SoH's Item Tracker in the
 * single exe, so its settings are SoH's page: SoH's own rows there (its toggle,
 * which opens the unified overlay by name, and its embedded Item Tracker
 * Settings, whose gTrackers.ItemTracker.* keys the overlay reads through
 * ComboReadItemTrackerKeys), then, through SohGui::RegisterSohPageRows, the
 * overlay's rows SoH's settings window has no twin for: Move to Default
 * Position (floating only; #962) and the overlay's Opacity under SoH's Item
 * Tracker Settings separator, which of its three sections it draws, and Majora's
 * Mask's eight groups (gCombo.Tracker.Items.*, cvar_shared_keys.h), each with the
 * overlay's own default and range. Combo > Tracker Settings, which held these
 * and the five twins of SoH's keys (retired into RSBS::kComboRetiredKeys and
 * carried into SoH's keys once), is gone; a selection persisted on it is
 * carried to this page (SohMenuCombo.cpp, kComboHeaderCarries).
 *
 * THE CHECK TRACKER'S ROWS (#1132, operator ruling D1: "replace the soh
 * tracker"). The unified Check Tracker is SoH's Check Tracker in the single exe,
 * so its settings are SoH's page: SoH's own rows there (its toggle, which opens
 * the unified window by name, and its embedded Check Tracker Settings, whose keys
 * the window reads), then, through SohGui::RegisterSohPageRows, the window's two
 * keys of its own (#1131: Auto Scroll To Current Area and Only Show Current Area,
 * gCombo.Tracker.Checks.*, with MM's tracker's defaults) under SoH's Check Tracker
 * Settings separator (a third, #1287's Only Show Current Area Toggle, is a row of
 * SoH's embedded settings window itself, in its Tracker Header Visibility group
 * beside SoH's per-control rows), and the Cross-Game Spoiler's toggle under its own
 * separator (it was on Combo > Windows, which #1132 retired; it reads the
 * crossings ruling D10 cut from the tracker). SoH's windows are not copied or
 * edited (rule 0).
 *
 * Locked by MenuComboSection leg 7 (every gCombo.Tracker.Items.* key written
 * exactly once on SoH's Item Tracker page with the overlay's widget, default and
 * range, under its separator, Move to Default Position floating only, no SoH key
 * or retired twin written, Combo > Tracker Settings gone) and leg 3 (SoH's two
 * tracker pages' rows and their order, each gCombo.Tracker.Checks.* page key once
 * under SoH's separator, the Spoiler's toggle), and drawn by UiSnapshot
 * (Randomizer > Item Tracker and Check Tracker).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common only (ADR 0002): the overlay's keys, window types and ranges, and
// the Cross-Game Spoiler's registered name and visibility key.
#include "ComboItemTrackerWindow.h"
#include "ComboSpoilerWindow.h"
#include "cvar_shared_keys.h"

#include <map>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** Move to Default Position applies to the floating overlay only, as SoH's Item
 *  Tracker Settings shows Enable Dragging only while SoH's Window Type is
 *  Floating (read through the overlay's one seam). */
void HideUnlessFloating(WidgetInfo& info) {
    info.isHidden = ComboGui::ComboReadItemTrackerKeys().windowType != ComboGui::COMBO_ITEM_TRACKER_FLOATING;
}

/** One of Majora's Mask's groups on the page: its row label (MM's own group
 *  name), its key and its tooltip. */
struct ItemTrackerGroupRow {
    const char* label;
    const char* key;
    const char* tooltip;
};
const ItemTrackerGroupRow kItemTrackerMMGroups[] = {
    { "Tokens", RSBS_CVAR_COMBO_ITEMS_MM_TOKENS,
      "Shows the Swamp and Ocean Gold Skulltula tokens of Majora's Mask in the Item Tracker." },
    { "Stray Fairies", RSBS_CVAR_COMBO_ITEMS_MM_STRAY_FAIRIES,
      "Shows the stray fairies of Clock Town and the four temples of Majora's Mask in the Item Tracker." },
    { "Dungeon", RSBS_CVAR_COMBO_ITEMS_MM_DUNGEON,
      "Shows the boss keys, small keys, maps and compasses of Majora's Mask in the Item Tracker." },
    { "Boss Souls", RSBS_CVAR_COMBO_ITEMS_MM_BOSS_SOULS,
      "Shows the boss souls of Majora's Mask in the Item Tracker, in a file that shuffles them." },
    { "Enemy Souls", RSBS_CVAR_COMBO_ITEMS_MM_ENEMY_SOULS,
      "Shows how many enemy souls of Majora's Mask you hold in the Item Tracker, in a file that shuffles them." },
    { "Owl Statues", RSBS_CVAR_COMBO_ITEMS_MM_OWL_STATUES,
      "Shows the owl statues of Majora's Mask in the Item Tracker, in a file that shuffles them." },
    { "Time", RSBS_CVAR_COMBO_ITEMS_MM_TIME,
      "Shows the half-days of Majora's Mask in the Item Tracker, in a file that shuffles them." },
    { "Misc", RSBS_CVAR_COMBO_ITEMS_MM_MISC,
      "Shows the ability to swim in Majora's Mask in the Item Tracker, in a file that shuffles it." },
};

} // namespace

/**
 * The Item Tracker's rows on SoH's Randomizer > Item Tracker page (#1141, lane
 * IT-3), after SoH's own (the page's path, first column pinned, handed over by
 * AddContributedSohPageRows): they continue SoH's Item Tracker Settings group
 * with the two rows its settings window has no twin for, then the overlay's
 * sections and Majora's Mask's groups under separators of their own.
 */
void AddItemTrackerPageRows(SohMenu& menu, WidgetPath& path) {
    // Move to Default Position (#962): the floating overlay starts under the
    // hearts; this returns a dragged one there. Not named Reset, which the
    // parity lint reserves for confirmed destructive buttons.
    menu.AddWidget(path, "Move to Default Position##ItemTracker", WIDGET_BUTTON)
        .RaceDisable(false)
        .PreFunc(HideUnlessFloating)
        .Callback([](WidgetInfo& info) { ComboGui::ComboItemTrackerResetPosition(); })
        .Options(ButtonOptions()
                     .Size(Sizes::Inline)
                     .Tooltip("Puts the floating Item Tracker back where it starts, below the hearts at the left "
                              "of the game."));
    menu.AddWidget(path, "Tracker Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(RSBS_CVAR_COMBO_ITEMS_OPACITY)
        .RaceDisable(false)
        // IsPercentage() first: it resets the range to 0..1, and the overlay
        // clamps at 0.1 (ComboReadItemTrackerKeys).
        .Options(FloatSliderOptions().IsPercentage().Min(0.1f).Max(1.0f).DefaultValue(1.0f).Tooltip(
            "Sets the opacity of the whole Item Tracker, its icons included."));

    menu.AddWidget(path, "Item Tracker Sections", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Ocarina of Time##ItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_OOT)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items you hold in Ocarina of Time in the Item Tracker, as of your last game switch or "
            "save while you play Majora's Mask."));
    menu.AddWidget(path, "Majora's Mask##ItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_MM)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items you hold in Majora's Mask in the Item Tracker, as of your last game switch or save "
            "while you play Ocarina of Time."));
    menu.AddWidget(path, "Shared##ItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items and resources the two games share in the Item Tracker, as of your last game "
            "switch or save."));

    // MM's groups, one checkbox each, named as MM's own tracker names them
    // (the MM item adapter's groups). Ocarina of Time's sections are SoH's
    // own Inventory, Equipment, Misc, Dungeon Rewards and Songs rows in the
    // Item Tracker Settings window above.
    menu.AddWidget(path, "Majora's Mask Groups", WIDGET_SEPARATOR_TEXT);
    for (const ItemTrackerGroupRow& group : kItemTrackerMMGroups) {
        menu.AddWidget(path, std::string(group.label) + "##ItemTracker", WIDGET_CVAR_CHECKBOX)
            .CVar(group.key)
            .RaceDisable(false)
            .Options(CheckboxOptions().DefaultValue(true).Tooltip(group.tooltip));
    }
}

/**
 * The Check Tracker's rows on SoH's Randomizer > Check Tracker page (#1132),
 * after SoH's own (the page's path, first column pinned, handed over by
 * AddContributedSohPageRows): they continue SoH's Check Tracker Settings group,
 * then the Cross-Game Spoiler gets a separator of its own, as SoH's tracker
 * pages give every window one.
 */
void AddCheckTrackerPageRows(SohMenu& menu, WidgetPath& path) {
    // #1131: the window's two keys of its own, in SoH's checkbox shape with the
    // window's defaults (ComboTrackerWindow.h, ComboTrackerNavKeys).
    menu.AddWidget(path, "Auto Scroll To Current Area##CheckTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_CHECKS_AUTO_SCROLL)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Scrolls the Check Tracker to the area you are in each time you enter a new one, in the game you are "
            "playing."));
    menu.AddWidget(path, "Only Show Current Area##CheckTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_CHECKS_ONLY_CURRENT_AREA)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Shows only the checks in the area you are in, in both games' lists of the Check Tracker. Each list also "
            "has its own Only Show Current Area toggle."));
    // The Cross-Game Spoiler, from Combo > Windows: it reveals the paired seed's
    // crossings, so it is race-disabled like a spoiler tool. WINDOW_BUTTON reads
    // .CVar only for its label and opens the window it finds by .WindowName, so
    // both come from the constants the window is built with.
    menu.AddWidget(path, "Cross-Game Spoiler", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle Cross-Game Spoiler", WIDGET_WINDOW_BUTTON)
        .CVar(ComboGui::kComboSpoilerVisibilityCVar)
        .RaceDisable(true)
        .WindowName(ComboGui::kComboSpoilerWindowName)
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Cross-Game Spoiler.").EmbedWindow(false));
}

/** File-scope, so they run before AddMenuElements() (SohMenu.h's contract);
 *  this TU is under games/oot/soh, whose archives are WHOLE_ARCHIVE'd, so the
 *  registrars are not elided. SoH's pages always hold their own rows, so a
 *  contribution can never make one #640's empty page. */
static RegisterSohPageRows_t sItemTrackerRows("Randomizer", "Item Tracker", AddItemTrackerPageRows);
static RegisterSohPageRows_t sCheckTrackerRows("Randomizer", "Check Tracker", AddCheckTrackerPageRows);

} // namespace SohGui
