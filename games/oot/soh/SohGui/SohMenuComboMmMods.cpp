/**
 * @file SohMenuComboMmMods.cpp
 * @brief #706 — Majora's Mask's mod list (enable, disable, reorder), hosted as a
 *        CONTRIBUTED page in the tier-4 Combo section.
 *
 * THE PROBLEM. Since #670 MM mounts `mods/mm/`, but every archive there, in path
 * order: MM had no enabled set and no menu, so the player's controls were renaming
 * a file and moving it out of the folder. OoT's half of the same tree has its mod
 * menu (Settings > Mod Menu, games/oot/soh/Enhancements/mod_menu.cpp). The model
 * this page edits is src/common/mm_mod_set.h; MM's mount reads the same model, so
 * the page and the game cannot disagree about what is enabled.
 *
 * WHY A CONTRIBUTED PAGE. ADR 0004's Combo section hosts MM's pages, and
 * `SohGui::RegisterComboSectionPage` exists so a page can be added without editing
 * SohMenuCombo.cpp (SohMenu.h documents the seam, and
 * SohMenuComboMmEnhancements.cpp is its first consumer). This TU is under
 * games/oot/soh, whose archives are WHOLE_ARCHIVE'd, so the file-scope registrar
 * cannot be elided (#516/#640's class). OoT's own mod menu is left exactly as SoH
 * shipped it (docs/ui-style-guide.md rule 0): MM's mods get a page of their own
 * rather than rows in OoT's list, which would also have meant sharing OoT's
 * `EnabledMods` CVar.
 *
 * THE LOOK is OoT's mod menu (the UI snapshot compares the page with
 * Randomizer/Tricks/Glitches, SoH's other two-column Disabled/Enabled table,
 * because OoT's mod menu page cannot be drawn in the harness): a two-column bordered
 * table, 25 px theme-coloured UIWidgets::StateButton arrows before each file name,
 * the arrow at the end of the list disabled. OoT's own two-column layout (Enabled
 * Mods / Disabled Mods, with left/right arrows) is still in mod_menu.cpp, commented
 * out because OoT cannot apply a change at runtime; MM's page uses it, because MM
 * CAN apply a change without a restart whenever MM has not started yet in this
 * session, and says which of the two cases the player is in. Changes are saved
 * immediately (there is no Edit / Apply step: nothing here closes the game).
 *
 * Locked by MMModSet (the model: enable, disable, reorder, the rules, persistence)
 * and MMModsMount (a disabled archive is neither mounted nor registered and the
 * chosen order wins, across both switch directions); drawn and compared by
 * UiSnapshot.
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common only (ADR 0002 / ADR 0008 rule 5): the model and MM's mods root.
#include "mm_mod_set.h"
#include "mod_archives.h"

#include <string>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** The sidebar label. Selection persists by display name, so this is the one
 *  spelling; the harness finds the page through the registry, not this literal. */
constexpr const char* kMmModsPage = "MM Mods";

/** The note row's text, rewritten by its PreFunc every frame. */
std::string sMmModsNote;

/** First draw scans lazily, so opening the menu before MM ever started still shows
 *  the folder's contents. */
void EnsureScanned() {
    if (!Combo_MMModSet_Scanned()) {
        (void)Combo_MMModSet_Scan(Combo_ModsRootForGame(GAME_MM));
    }
}

void MmModsNotePreFunc(WidgetInfo& info) {
    EnsureScanned();
    if (Combo_MMModSet_EnabledCount() == 0 && Combo_MMModSet_DisabledCount() == 0) {
        sMmModsNote = "No Majora's Mask mods found. Put .o2r files in the mods/mm folder, then rescan.";
    } else if (Combo_MMModSet_RestartPending()) {
        sMmModsNote = "Majora's Mask has already loaded its mods. Restart the game to apply these changes.";
    } else if (Combo_MMModSet_MountedThisSession()) {
        sMmModsNote = "Majora's Mask has already loaded its mods. Changes here apply after a restart.";
    } else {
        sMmModsNote = "Changes apply when Majora's Mask starts. Mods higher in the list override those below them.";
    }
    info.name = sMmModsNote;
}

/** One pending edit, applied after the table is drawn so the lists are not changed
 *  while they are being iterated (the shape of OoT's DrawMods). */
enum class MmModAction { None, Raise, Lower, Disable, Enable };

void DrawArrow(const std::string& id, const char* icon, const char* tooltip, bool disabled, MmModAction action,
               const std::string& key, MmModAction& pendingAction, std::string& pendingKey) {
    if (disabled) {
        ImGui::BeginDisabled();
    }
    if (StateButton(id.c_str(), icon, ImVec2(25, 25), ButtonOptions().Color(THEME_COLOR))) {
        pendingAction = action;
        pendingKey = key;
    }
    Tooltip(tooltip);
    if (disabled) {
        ImGui::EndDisabled();
    }
}

void DrawMmModList(WidgetInfo& info) {
    EnsureScanned();
    MmModAction pendingAction = MmModAction::None;
    std::string pendingKey;

    if (ImGui::BeginTable("tableMmMods", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Enabled Mods", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::TableSetupColumn("Disabled Mods", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::TableHeadersRow();
        ImGui::PopItemFlag();
        ImGui::TableNextRow();

        // Enabled, highest priority first: the reading OoT's list has ("mod
        // priority is top to bottom").
        ImGui::TableNextColumn();
        const int enabledCount = Combo_MMModSet_EnabledCount();
        for (int i = 0; i < enabledCount; i++) {
            const char* raw = Combo_MMModSet_EnabledKey(i);
            if (raw == nullptr) {
                continue;
            }
            const std::string key(raw);
            ImGui::PushID(key.c_str());
            DrawArrow("up", ICON_FA_ARROW_UP, "Moves this mod up, so it overrides the mods below it.", i == 0,
                      MmModAction::Raise, key, pendingAction, pendingKey);
            ImGui::SameLine();
            DrawArrow("down", ICON_FA_ARROW_DOWN, "Moves this mod down, below the next mod.", i == enabledCount - 1,
                      MmModAction::Lower, key, pendingAction, pendingKey);
            ImGui::SameLine();
            DrawArrow("off", ICON_FA_ARROW_RIGHT, "Disables this mod. Majora's Mask will not load it.", false,
                      MmModAction::Disable, key, pendingAction, pendingKey);
            ImGui::SameLine();
            ImGui::Text("%s", key.c_str());
            ImGui::PopID();
        }

        ImGui::TableNextColumn();
        const int disabledCount = Combo_MMModSet_DisabledCount();
        for (int i = 0; i < disabledCount; i++) {
            const char* raw = Combo_MMModSet_DisabledKey(i);
            if (raw == nullptr) {
                continue;
            }
            const std::string key(raw);
            ImGui::PushID(key.c_str());
            DrawArrow("on", ICON_FA_ARROW_LEFT, "Enables this mod at the top of the list.", false, MmModAction::Enable,
                      key, pendingAction, pendingKey);
            ImGui::SameLine();
            ImGui::Text("%s", key.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    switch (pendingAction) {
        case MmModAction::Raise:
            (void)Combo_MMModSet_Raise(pendingKey.c_str());
            break;
        case MmModAction::Lower:
            (void)Combo_MMModSet_Lower(pendingKey.c_str());
            break;
        case MmModAction::Disable:
            (void)Combo_MMModSet_SetEnabled(pendingKey.c_str(), false);
            break;
        case MmModAction::Enable:
            (void)Combo_MMModSet_SetEnabled(pendingKey.c_str(), true);
            break;
        case MmModAction::None:
        default:
            break;
    }
}

} // namespace

/**
 * The contributed page's registrar. One column, like Settings > Mod Menu: the
 * gray state note, the section, a rescan button and the list.
 */
void AddMmModsWidgets(SohMenu& menu, WidgetPath& path) {
    path.column = SECTION_COLUMN_1;
    // The state note: whether a change applies now or after a restart, or where the
    // files go when there are none. Rewritten every frame, so kept out of search.
    menu.AddWidget(path, "MM Mods Status", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(MmModsNotePreFunc)
        .Options(TextOptions().Color(Colors::Gray));
    menu.AddWidget(path, "Majora's Mask Mods", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Rescan Mods Folder", WIDGET_BUTTON)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { (void)Combo_MMModSet_Scan(Combo_ModsRootForGame(GAME_MM)); })
        .Options(ButtonOptions()
                     .Size(Sizes::Inline)
                     .Tooltip("Scans the mods/mm folder again for Majora's Mask mods you added or removed."));
    menu.AddWidget(path, "MM Mod List", WIDGET_CUSTOM).RaceDisable(false).CustomFunction(DrawMmModList);
}

/** File-scope, so it runs before AddMenuElements() (SohMenu.h's contract). One
 *  column, and the page always holds its note, section, button and list, so it is
 *  never #640's empty page. */
static RegisterComboSectionPage_t sMmModsPage(kMmModsPage, 1, AddMmModsWidgets);

/** The page's registered name, for the harness and locks. */
const char* MmModsPageName() {
    return kMmModsPage;
}

} // namespace SohGui
