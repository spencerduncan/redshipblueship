#include "SohMenu.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipUtils.h"
#include "soh/SohGui/SohGui.hpp"

extern "C" {
#include "variables.h"
}

#ifdef RSBS_SINGLE_EXECUTABLE
#include "soh/Notification/Notification.h"
#include "pairing_refusal_toast.h" // src/common — RSBS_REFUSAL_TOAST_PREFIX, the refusal toasts' one prefix
#include "seed_package.h"          // src/common — #1044: the seed package Generate writes and a file binds
#endif

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

static const std::map<int32_t, const char*> skipGetItemAnimationOptions = {
    { SGIA_DISABLED, "Disabled" },
    { SGIA_JUNK, "Junk Items" },
    { SGIA_ALL, "All Items" },
};

static bool locationsDirty = true;
static bool tricksDirty = true;
static char seedString[MAX_SEED_STRING_SIZE];

#ifdef RSBS_SINGLE_EXECUTABLE
// RSBS (#1044): THE SEED PACKAGE ROWS of Randomizer > General, in the shape of the
// Plandomizer's spoiler picker: a combo box over Randomizer/*.rsbs-seed.zip and a
// Load button, then an Export button that writes the staged package under a name
// typed beside it, and one line saying what is staged. Loading stages a package
// exactly as Generate stages the one it writes, so the file select's "Start
// Randomizer" creates a file from it; the gate is Generate's (the file select, no
// file loaded, no generation running).
static void DrawSeedPackageRows() {
    static std::vector<std::string> sPackages;
    static size_t sSelected = 0;
    static char sExportName[96] = "";
    const std::string dir = Ship::Context::GetPathRelativeToAppDirectory("Randomizer");
    sPackages = rsbs::seedpkg::ListDirectory(dir);
    if (sSelected >= sPackages.size()) {
        sSelected = 0;
    }
    const bool atFileSelect = gSaveContext.gameMode == GAMEMODE_FILE_SELECT && !GameInteractor::IsSaveLoaded();
    const bool generating = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0) != 0;

    if (!sPackages.empty()) {
        UIWidgets::Combobox("##SeedPackages", &sSelected, sPackages,
                            UIWidgets::ComboboxOptions().Color(THEME_COLOR).LabelPosition(UIWidgets::LabelPositions::None));
    } else {
        ImGui::Text("No seed packages found.");
    }
    ImGui::BeginDisabled(sPackages.empty() || !atFileSelect || generating);
    if (UIWidgets::Button("Load seed package",
                          UIWidgets::ButtonOptions()
                              .Color(THEME_COLOR)
                              .Size(UIWidgets::Sizes::Inline)
                              .Tooltip("Stages the selected seed package for the next file you create: its whole "
                                       "paired world, both games' settings and its spoiler, as Generate would."))) {
        const std::string file = sPackages[sSelected];
        const int rc = OoT_SeedPackage_LoadFile((dir + "/" + file).c_str());
        if (rc == RSBS_SEEDPKG_OK) {
            Notification::Emit({ .prefix = "Seed package loaded:", .message = file });
        } else {
            Notification::Emit({ .prefix = RSBS_REFUSAL_TOAST_PREFIX, .message = SeedPackage_RefusalWords(rc) });
        }
    }
    ImGui::EndDisabled();

    char summary[512];
    const bool staged = OoT_SeedPackage_StagedSummary(summary, sizeof(summary)) >= 0;
    std::string exportName = sExportName;
    ImGui::BeginDisabled(!staged || exportName.empty());
    ImGui::SameLine();
    if (UIWidgets::Button("Export seed package",
                          UIWidgets::ButtonOptions()
                              .Color(THEME_COLOR)
                              .Size(UIWidgets::Sizes::Inline)
                              .Tooltip("Writes the staged seed package to the Randomizer folder under the name "
                                       "typed below, to share it."))) {
        exportName += RSBS_SEED_PACKAGE_EXTENSION;
        const int rc = OoT_SeedPackage_ExportStaged((dir + "/" + exportName).c_str());
        if (rc == RSBS_SEEDPKG_OK) {
            Notification::Emit({ .prefix = "Seed package exported:", .message = exportName });
        } else {
            Notification::Emit({ .prefix = "Not exported:", .message = SeedPackage_RefusalWords(rc) });
        }
    }
    ImGui::EndDisabled();
    UIWidgets::PushStyleInput(THEME_COLOR);
    ImGui::InputText("##SeedPackageExportName", sExportName, sizeof(sExportName),
                     ImGuiInputTextFlags_CallbackCharFilter, UIWidgets::TextFilters::FilterAlphaNum);
    if (strnlen(sExportName, sizeof(sExportName)) == 0) {
        ImGui::SameLine(17.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "Name for the exported package");
    }
    UIWidgets::PopStyleInput();
    if (staged) {
        ImGui::TextWrapped("Seed package: %s", summary);
    } else {
        ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Gray), "No seed package staged.");
    }
}
#endif
static std::set<RandomizerCheck> excludedLocations;
static std::set<RandomizerTrick> enabledTricks;
static std::set<RandomizerTrick> enabledGlitches;

void DrawLocationsMenu(WidgetInfo& info) {
    auto ctx = OTRGlobals::Instance->gRandoContext;
    static ImVec2 cellPadding(8.0f, 8.0f);
    bool generating = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
    bool disableEditingRandoSettings = generating || CVarGetInteger(CVAR_GENERAL("OnFileSelectNameEntry"), 0);
    ImGui::BeginDisabled(CVarGetInteger(CVAR_SETTING("DisableChanges"), 0) || disableEditingRandoSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
    if (locationsDirty) {
        RandomizerCheckObjects::UpdateImGuiVisibility();
        // todo: this efficently when we build out cvar array support
        std::stringstream excludedLocationStringStream(CVarGetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), ""));
        std::string excludedLocationString;
        excludedLocations.clear();
        while (getline(excludedLocationStringStream, excludedLocationString, ',')) {
            excludedLocations.insert((RandomizerCheck)std::stoi(excludedLocationString));
        }
        locationsDirty = false;
    }

    if (ImGui::BeginTable("tableRandoLocations", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Included", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::TableSetupColumn("Excluded", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::TableHeadersRow();
        ImGui::PopItemFlag();
        ImGui::TableNextRow();

        // COLUMN 1 - INCLUDED LOCATIONS
        ImGui::TableNextColumn();
        // window->DC.CurrLineTextBaseOffset = 0.0f;

        static ImGuiTextFilter locationSearch;
        UIWidgets::PushStyleInput(THEME_COLOR);
        locationSearch.Draw();
        UIWidgets::PopStyleInput();

        ImGui::BeginChild("ChildIncludedLocations", ImVec2(0, -8));
        for (auto& [rcArea, locations] : RandomizerCheckObjects::GetAllRCObjectsByArea()) {
            bool hasItems = false;
            for (RandomizerCheck rc : locations) {
                if (ctx->GetItemLocation(rc)->IsVisible() && !excludedLocations.count(rc) &&
                    locationSearch.PassFilter(Rando::StaticData::GetLocation(rc)->GetName().c_str())) {

                    hasItems = true;
                    break;
                }
            }

            if (hasItems) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (ImGui::TreeNode(RandomizerCheckObjects::GetRCAreaName(rcArea).c_str())) {
                    for (auto& location : locations) {
                        if (ctx->GetItemLocation(location)->IsVisible() && !excludedLocations.count(location) &&
                            locationSearch.PassFilter(Rando::StaticData::GetLocation(location)->GetName().c_str())) {
                            UIWidgets::PushStyleButton(THEME_COLOR, ImVec2(7.f, 5.f));
                            if (ImGui::ArrowButton(std::to_string(location).c_str(), ImGuiDir_Right)) {
                                excludedLocations.insert(location);
                                // todo: this efficently when we build out cvar array support
                                std::string excludedLocationString = "";
                                for (auto excludedLocationIt : excludedLocations) {
                                    excludedLocationString += std::to_string(excludedLocationIt);
                                    excludedLocationString += ",";
                                }
                                CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"),
                                              excludedLocationString.c_str());
                                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                                locationsDirty = true;
                            }
                            UIWidgets::PopStyleButton();
                            ImGui::SameLine();
                            ImGui::Text("%s", Rando::StaticData::GetLocation(location)->GetShortName().c_str());
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        // COLUMN 2 - EXCLUDED LOCATIONS
        ImGui::TableNextColumn();
        // window->DC.CurrLineTextBaseOffset = 0.0f;

        ImGui::BeginChild("ChildExcludedLocations", ImVec2(0, -8));
        for (auto& [rcArea, locations] : RandomizerCheckObjects::GetAllRCObjectsByArea()) {
            bool hasItems = false;
            for (RandomizerCheck rc : locations) {
                if (ctx->GetItemLocation(rc)->IsVisible() && excludedLocations.count(rc)) {
                    hasItems = true;
                    break;
                }
            }

            if (hasItems) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (ImGui::TreeNode(RandomizerCheckObjects::GetRCAreaName(rcArea).c_str())) {
                    for (auto& location : locations) {
                        auto elfound = excludedLocations.find(location);
                        if (ctx->GetItemLocation(location)->IsVisible() && elfound != excludedLocations.end()) {
                            UIWidgets::PushStyleButton(THEME_COLOR, ImVec2(7.f, 5.f));
                            if (ImGui::ArrowButton(std::to_string(location).c_str(), ImGuiDir_Left)) {
                                excludedLocations.erase(elfound);
                                // todo: this efficently when we build out cvar array support
                                std::string excludedLocationString = "";
                                for (auto excludedLocationIt : excludedLocations) {
                                    excludedLocationString += std::to_string(excludedLocationIt);
                                    excludedLocationString += ",";
                                }
                                if (excludedLocationString == "") {
                                    CVarClear(CVAR_RANDOMIZER_SETTING("ExcludedLocations"));
                                } else {
                                    CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"),
                                                  excludedLocationString.c_str());
                                }
                                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                                locationsDirty = true;
                            }
                            UIWidgets::PopStyleButton();
                            ImGui::SameLine();
                            ImGui::Text("%s", Rando::StaticData::GetLocation(location)->GetShortName().c_str());
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }
    ImGui::PopStyleVar(1);
    // ImGui::EndTabItem();
    ImGui::EndDisabled();
}

void DrawTricksMenu(WidgetInfo& info) {
    auto ctx = OTRGlobals::Instance->gRandoContext;
    auto randoSettings = Rando::Settings::GetInstance();
    static ImVec2 cellPadding(8.0f, 8.0f);
    bool generating = CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0);
    bool disableEditingRandoSettings = generating || CVarGetInteger(CVAR_GENERAL("OnFileSelectNameEntry"), 0);
    if (tricksDirty) {
        tricksDirty = false;
        // RandomizerTricks::UpdateImGuiVisibility();
        //  todo: this efficently when we build out cvar array support
        std::stringstream enabledTrickStringStream(CVarGetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), ""));
        std::string enabledTrickString;
        enabledTricks.clear();
        while (getline(enabledTrickStringStream, enabledTrickString, ',')) {
            enabledTricks.insert((RandomizerTrick)std::stoi(enabledTrickString));
        }
        std::stringstream enabledGlitchStringStream(CVarGetString(CVAR_RANDOMIZER_SETTING("EnabledGlitches"), ""));
        std::string enabledGlitchString;
        enabledGlitches.clear();
        while (getline(enabledGlitchStringStream, enabledGlitchString, ',')) {
            enabledGlitches.insert((RandomizerTrick)std::stoi(enabledGlitchString));
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
    ImGui::BeginDisabled(CVarGetInteger(CVAR_SETTING("DisableChanges"), 0) || disableEditingRandoSettings);

    // Tricks
    static std::map<RandomizerArea, bool> areaTreeDisabled{
        { RA_NONE, true },
        { RA_KOKIRI_FOREST, true },
        { RA_THE_LOST_WOODS, true },
        { RA_SACRED_FOREST_MEADOW, true },
        { RA_HYRULE_FIELD, true },
        { RA_LAKE_HYLIA, true },
        { RA_GERUDO_VALLEY, true },
        { RA_GERUDO_FORTRESS, true },
        { RA_HAUNTED_WASTELAND, true },
        { RA_DESERT_COLOSSUS, true },
        { RA_THE_MARKET, true },
        { RA_HYRULE_CASTLE, true },
        { RA_KAKARIKO_VILLAGE, true },
        { RA_THE_GRAVEYARD, true },
        { RA_DEATH_MOUNTAIN_TRAIL, true },
        { RA_GORON_CITY, true },
        { RA_DEATH_MOUNTAIN_CRATER, true },
        { RA_ZORAS_RIVER, true },
        { RA_ZORAS_DOMAIN, true },
        { RA_ZORAS_FOUNTAIN, true },
        { RA_LON_LON_RANCH, true },
        { RA_DEKU_TREE, true },
        { RA_DODONGOS_CAVERN, true },
        { RA_JABU_JABUS_BELLY, true },
        { RA_FOREST_TEMPLE, true },
        { RA_FIRE_TEMPLE, true },
        { RA_WATER_TEMPLE, true },
        { RA_SPIRIT_TEMPLE, true },
        { RA_SHADOW_TEMPLE, true },
        { RA_BOTTOM_OF_THE_WELL, true },
        { RA_ICE_CAVERN, true },
        { RA_GERUDO_TRAINING_GROUND, true },
        { RA_GANONS_CASTLE, true },
    };
    static std::map<RandomizerArea, bool> areaTreeEnabled{
        { RA_NONE, true },
        { RA_KOKIRI_FOREST, true },
        { RA_THE_LOST_WOODS, true },
        { RA_SACRED_FOREST_MEADOW, true },
        { RA_HYRULE_FIELD, true },
        { RA_LAKE_HYLIA, true },
        { RA_GERUDO_VALLEY, true },
        { RA_GERUDO_FORTRESS, true },
        { RA_HAUNTED_WASTELAND, true },
        { RA_DESERT_COLOSSUS, true },
        { RA_THE_MARKET, true },
        { RA_HYRULE_CASTLE, true },
        { RA_KAKARIKO_VILLAGE, true },
        { RA_THE_GRAVEYARD, true },
        { RA_DEATH_MOUNTAIN_TRAIL, true },
        { RA_GORON_CITY, true },
        { RA_DEATH_MOUNTAIN_CRATER, true },
        { RA_ZORAS_RIVER, true },
        { RA_ZORAS_DOMAIN, true },
        { RA_ZORAS_FOUNTAIN, true },
        { RA_LON_LON_RANCH, true },
        { RA_DEKU_TREE, true },
        { RA_DODONGOS_CAVERN, true },
        { RA_JABU_JABUS_BELLY, true },
        { RA_FOREST_TEMPLE, true },
        { RA_FIRE_TEMPLE, true },
        { RA_WATER_TEMPLE, true },
        { RA_SPIRIT_TEMPLE, true },
        { RA_SHADOW_TEMPLE, true },
        { RA_BOTTOM_OF_THE_WELL, true },
        { RA_ICE_CAVERN, true },
        { RA_GERUDO_TRAINING_GROUND, true },
        { RA_GANONS_CASTLE, true },
    };

    static std::map<Rando::Tricks::Tag, bool> showTag{
        { Rando::Tricks::Tag::NOVICE, true },   { Rando::Tricks::Tag::INTERMEDIATE, true },
        { Rando::Tricks::Tag::ADVANCED, true }, { Rando::Tricks::Tag::EXPERT, true },
        { Rando::Tricks::Tag::EXTREME, true },  { Rando::Tricks::Tag::EXPERIMENTAL, true },
        { Rando::Tricks::Tag::GLITCH, false },
    };
    static ImGuiTextFilter trickSearch;
    UIWidgets::PushStyleInput(THEME_COLOR);
    trickSearch.Draw("Filter (inc,-exc)", 490.0f);
    UIWidgets::PopStyleInput();
    if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("LogicRules"), RO_LOGIC_GLITCHLESS) != RO_LOGIC_NO_LOGIC) {
        ImGui::SameLine();
        if (UIWidgets::Button("Disable All", UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(250.f, 0.f)))) {
            for (int i = 0; i < RT_MAX; i++) {
                auto etfound = enabledTricks.find(static_cast<RandomizerTrick>(i));
                if (etfound != enabledTricks.end()) {
                    enabledTricks.erase(etfound);
                }
            }
            std::string enabledTrickString = "";
            for (auto enabledTrickIt : enabledTricks) {
                enabledTrickString += std::to_string(enabledTrickIt);
                enabledTrickString += ",";
            }
            CVarClear(CVAR_RANDOMIZER_SETTING("EnabledTricks"));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            tricksDirty = true;
        }
        ImGui::SameLine();
        if (UIWidgets::Button("Enable All", UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(250.f, 0.f)))) {
            for (int i = 0; i < RT_MAX; i++) {
                if (!enabledTricks.count(static_cast<RandomizerTrick>(i))) {
                    enabledTricks.insert(static_cast<RandomizerTrick>(i));
                }
            }
            std::string enabledTrickString = "";
            for (auto enabledTrickIt : enabledTricks) {
                enabledTrickString += std::to_string(enabledTrickIt);
                enabledTrickString += ",";
            }
            CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), enabledTrickString.c_str());
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            tricksDirty = true;
        }
    }
    if (ImGui::BeginTable("trickTags", static_cast<int>(showTag.size()),
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
        for (auto [rtTag, isShown] : showTag) {
            ImGui::TableNextColumn();
            if (isShown) {
                ImGui::PushStyleColor(ImGuiCol_Text, Rando::Tricks::GetTextColor(rtTag));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 1.0f, 1.0f, 1.0f });
            }
            ImGui::PushStyleColor(ImGuiCol_Header, Rando::Tricks::GetTagColor(rtTag));
            ImGui::Selectable(Rando::Tricks::GetTagName(rtTag).c_str(), &showTag[rtTag]);
            ImGui::PopStyleColor(2);
        }
        ImGui::EndTable();
    }

    if (ImGui::BeginTable("tableRandoTricks", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Disabled Tricks", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::TableSetupColumn("Enabled Tricks", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::TableHeadersRow();
        ImGui::PopItemFlag();
        ImGui::TableNextRow();

        if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("LogicRules"), RO_LOGIC_GLITCHLESS) != RO_LOGIC_NO_LOGIC) {
            // COLUMN 1 - DISABLED TRICKS
            ImGui::TableNextColumn();
            // window->DC.CurrLineTextBaseOffset = 0.0f;

            if (UIWidgets::Button("Collapse All##disabled",
                                  UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int i = 0; i < RA_MAX; i++) {
                    areaTreeDisabled[static_cast<RandomizerArea>(i)] = false;
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button("Open All##disabled",
                                  UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int i = 0; i < RA_MAX; i++) {
                    areaTreeDisabled[static_cast<RandomizerArea>(i)] = true;
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button("Enable Visible",
                                  UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int i = 0; i < RT_MAX; i++) {
                    auto option = randoSettings->GetTrickOption(static_cast<RandomizerTrick>(i));
                    if (!enabledTricks.count(static_cast<RandomizerTrick>(i)) &&
                        trickSearch.PassFilter(option.GetName().c_str()) && areaTreeDisabled[option.GetArea()] &&
                        Rando::Tricks::CheckTags(showTag, option.GetTags())) {
                        enabledTricks.insert(static_cast<RandomizerTrick>(i));
                    }
                }
                std::string enabledTrickString = "";
                for (auto enabledTrickIt : enabledTricks) {
                    enabledTrickString += std::to_string(enabledTrickIt);
                    enabledTrickString += ",";
                }
                CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), enabledTrickString.c_str());
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                tricksDirty = true;
            }

            ImGui::BeginChild("ChildTricksDisabled", ImVec2(0, -8), false, ImGuiWindowFlags_HorizontalScrollbar);

            for (auto [area, trickIds] : randoSettings->mTricksByArea) {
                bool hasTricks = false;
                for (auto rt : trickIds) {
                    auto option = randoSettings->GetTrickOption(rt);
                    if (!option.IsHidden() && trickSearch.PassFilter(option.GetName().c_str()) &&
                        !enabledTricks.count(rt) && Rando::Tricks::CheckTags(showTag, option.GetTags())) {
                        hasTricks = true;
                        break;
                    }
                }
                if (hasTricks) {
                    ImGui::TreeNodeSetOpen(ImGui::GetID((Rando::Tricks::GetAreaName(area) + "##disabled").c_str()),
                                           areaTreeDisabled[area]);
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::TreeNode((Rando::Tricks::GetAreaName(area) + "##disabled").c_str())) {
                        for (auto rt : trickIds) {
                            auto option = randoSettings->GetTrickOption(rt);
                            if (!option.IsHidden() && trickSearch.PassFilter(option.GetName().c_str()) &&
                                !enabledTricks.count(rt) && Rando::Tricks::CheckTags(showTag, option.GetTags())) {
                                ImGui::TreeNodeSetOpen(
                                    ImGui::GetID((Rando::Tricks::GetAreaName(option.GetArea()) + "##disabled").c_str()),
                                    areaTreeDisabled[option.GetArea()]);
                                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                                UIWidgets::PushStyleButton(THEME_COLOR, ImVec2(7.f, 5.f));
                                if (ImGui::ArrowButton(std::to_string(rt).c_str(), ImGuiDir_Right)) {
                                    enabledTricks.insert(rt);
                                    std::string enabledTrickString = "";
                                    for (auto enabledTrickIt : enabledTricks) {
                                        enabledTrickString += std::to_string(enabledTrickIt);
                                        enabledTrickString += ",";
                                    }
                                    CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), enabledTrickString.c_str());
                                    Ship::Context::GetInstance()
                                        ->GetWindow()
                                        ->GetGui()
                                        ->SaveConsoleVariablesNextFrame();
                                    tricksDirty = true;
                                }
                                UIWidgets::PopStyleButton();
                                Rando::Tricks::DrawTagChips(option.GetTags(), option.GetName());
                                ImGui::SameLine();
                                ImGui::Text("%s", option.GetName().c_str());
                                UIWidgets::Tooltip(option.GetDescription().c_str());
                            }
                        }
                        areaTreeDisabled[area] = true;
                        ImGui::TreePop();
                    } else {
                        areaTreeDisabled[area] = false;
                    }
                }
            }
            ImGui::EndChild();

            // COLUMN 2 - ENABLED TRICKS
            ImGui::TableNextColumn();
            // window->DC.CurrLineTextBaseOffset = 0.0f;

            if (UIWidgets::Button("Collapse All##enabled",
                                  UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int i = 0; i < RA_MAX; i++) {
                    areaTreeEnabled[static_cast<RandomizerArea>(i)] = false;
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button("Open All##enabled",
                                  UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int i = 0; i < RA_MAX; i++) {
                    areaTreeEnabled[static_cast<RandomizerArea>(i)] = true;
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button("Disable Visible",
                                  UIWidgets::ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int i = 0; i < RT_MAX; i++) {
                    auto option = randoSettings->GetTrickOption(static_cast<RandomizerTrick>(i));
                    if (enabledTricks.count(static_cast<RandomizerTrick>(i)) &&
                        trickSearch.PassFilter(option.GetName().c_str()) && areaTreeEnabled[option.GetArea()] &&
                        Rando::Tricks::CheckTags(showTag, option.GetTags())) {
                        enabledTricks.erase(static_cast<RandomizerTrick>(i));
                    }
                }
                std::string enabledTrickString = "";
                for (auto enabledTrickIt : enabledTricks) {
                    enabledTrickString += std::to_string(enabledTrickIt);
                    enabledTrickString += ",";
                }
                if (enabledTricks.size() == 0) {
                    CVarClear(CVAR_RANDOMIZER_SETTING("EnabledTricks"));
                } else {
                    CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), enabledTrickString.c_str());
                }
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                tricksDirty = true;
            }

            ImGui::BeginChild("ChildTricksEnabled", ImVec2(0, -8), false, ImGuiWindowFlags_HorizontalScrollbar);

            for (auto [area, trickIds] : randoSettings->mTricksByArea) {
                bool hasTricks = false;
                for (auto rt : trickIds) {
                    auto option = randoSettings->GetTrickOption(rt);
                    if (!option.IsHidden() && trickSearch.PassFilter(option.GetName().c_str()) &&
                        enabledTricks.count(rt) && Rando::Tricks::CheckTags(showTag, option.GetTags())) {
                        hasTricks = true;
                        break;
                    }
                }
                if (hasTricks) {
                    ImGui::TreeNodeSetOpen(ImGui::GetID((Rando::Tricks::GetAreaName(area) + "##enabled").c_str()),
                                           areaTreeEnabled[area]);
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::TreeNode((Rando::Tricks::GetAreaName(area) + "##enabled").c_str())) {
                        for (auto rt : trickIds) {
                            auto option = randoSettings->GetTrickOption(rt);
                            if (!option.IsHidden() && trickSearch.PassFilter(option.GetName().c_str()) &&
                                enabledTricks.count(rt) && Rando::Tricks::CheckTags(showTag, option.GetTags())) {
                                ImGui::TreeNodeSetOpen(
                                    ImGui::GetID((Rando::Tricks::GetAreaName(option.GetArea()) + "##enabled").c_str()),
                                    areaTreeEnabled[option.GetArea()]);
                                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                                UIWidgets::PushStyleButton(THEME_COLOR, ImVec2(7.f, 5.f));
                                if (ImGui::ArrowButton(std::to_string(rt).c_str(), ImGuiDir_Left)) {
                                    enabledTricks.erase(rt);
                                    std::string enabledTrickString = "";
                                    for (auto enabledTrickIt : enabledTricks) {
                                        enabledTrickString += std::to_string(enabledTrickIt);
                                        enabledTrickString += ",";
                                    }
                                    if (enabledTrickString == "") {
                                        CVarClear(CVAR_RANDOMIZER_SETTING("EnabledTricks"));
                                    } else {
                                        CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"),
                                                      enabledTrickString.c_str());
                                    }
                                    Ship::Context::GetInstance()
                                        ->GetWindow()
                                        ->GetGui()
                                        ->SaveConsoleVariablesNextFrame();
                                    tricksDirty = true;
                                }
                                UIWidgets::PopStyleButton();
                                Rando::Tricks::DrawTagChips(option.GetTags(), option.GetName());
                                ImGui::SameLine();
                                ImGui::Text("%s", option.GetName().c_str());
                                UIWidgets::Tooltip(option.GetDescription().c_str());
                            }
                        }
                        areaTreeEnabled[area] = true;
                        ImGui::TreePop();
                    } else {
                        areaTreeEnabled[area] = false;
                    }
                }
            }

            ImGui::EndChild();
        } else {
            ImGui::TableNextColumn();
            ImGui::BeginChild("ChildTricksDisabled", ImVec2(0, -8));
            ImGui::Text("Requires Logic Turned On.");
            ImGui::EndChild();
            ImGui::TableNextColumn();
            ImGui::BeginChild("ChildTricksEnabled", ImVec2(0, -8));
            ImGui::Text("Requires Logic Turned On.");
            ImGui::EndChild();
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar(1);
}

void SohMenu::AddMenuRandomizer() {
    // Add Randomizer Menu
    AddMenuEntry("Randomizer", CVAR_SETTING("Menu.RandomizerSidebarSection"));

    // Seed Settings
    WidgetPath path = { "Randomizer", "General", SECTION_COLUMN_1 };
    AddSidebarEntry("Randomizer", path.sidebarName, 2);
    AddWidget(path,
              "Be sure to explore the Presets and Enhancements Menus for various Speedups and Quality of life changes!",
              WIDGET_TEXT)
        .Options(TextOptions().Color(UIWidgets::Colors::Gray));
    AddWidget(path, "Seed Entry", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Manual seed entry", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_SETTING("ManualSeedEntry"))
        .Options(CheckboxOptions().DefaultValue(true));
    AddWidget(path, "Seed", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("ManualSeedEntry"), 0)) {
            UIWidgets::PushStyleInput(THEME_COLOR);
            ImGui::InputText("##RandomizerSeed", seedString, MAX_SEED_STRING_SIZE,
                             ImGuiInputTextFlags_CallbackCharFilter, UIWidgets::TextFilters::FilterAlphaNum);
            UIWidgets::Tooltip("Characters from a-z, A-Z, and 0-9 are supported.\n"
                               "Character limit is 1023, after which the seed will be truncated.\n");
            ImGui::SameLine();
            if (UIWidgets::Button(
                    ICON_FA_RANDOM,
                    UIWidgets::ButtonOptions()
                        .Size(UIWidgets::Sizes::Inline)
                        .Color(THEME_COLOR)
                        .Padding(ImVec2(10.f, 6.f))
                        .Tooltip("Creates a new random seed value to be used when generating a randomizer"))) {
                char newSeed[11];
                for (size_t i = 0; i < 10; i++) {
                    newSeed[i] = '0' + ShipUtils::Random(0, 10);
                }
                newSeed[10] = '\0';
                SohUtils::CopyStringToCharArray(seedString, newSeed, MAX_SEED_STRING_SIZE);
            }
            ImGui::SameLine();
            if (UIWidgets::Button(ICON_FA_ERASER, UIWidgets::ButtonOptions()
                                                      .Size(UIWidgets::Sizes::Inline)
                                                      .Color(THEME_COLOR)
                                                      .Padding(ImVec2(10.f, 6.f)))) {
                memset(seedString, 0, MAX_SEED_STRING_SIZE);
            }
            if (strnlen(seedString, MAX_SEED_STRING_SIZE) == 0) {
                ImGui::SameLine(17.0f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "Leave blank for random seed");
            }
            UIWidgets::PopStyleInput();
        }
    });
    AddWidget(path, "Generate Randomizer", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            OTRGlobals::Instance->gRandoContext->SetSpoilerLoaded(false);
            GenerateRandomizer(CVarGetInteger(CVAR_RANDOMIZER_SETTING("ManualSeedEntry"), 0) ? seedString : "");
        })
        // RSBS (#564 V12, the audit's Phase 0 item 4): in the single executable
        // this gate is LOAD-BEARING for the paired world's identity, not only a
        // convenience. Generation is the one stamp site of gComboCtx's pairing
        // identity (3drando/playthrough.cpp's creation freeze writes
        // sharedRandoSeed, sharedRandoSettingsHash, mmProfileDigest and the
        // combo record) and it replaces Rando::Context's world. While a file is
        // loaded that identity is the file's, and every commit writes it into
        // the .redsave and its digest into the .sav, so a generation reachable
        // mid-play would re-stamp a running paired file with another world.
        // Keep generation unreachable outside the file select (SoH's gen_rando
        // console command does not go through this button: #909).
        .PreFunc([](WidgetInfo& info) {
            info.options->Disabled((gSaveContext.gameMode != GAMEMODE_FILE_SELECT) || GameInteractor::IsSaveLoaded());
        })
        .Options(ButtonOptions()
                     .Size(ImVec2(250.f, 0.f))
                     .DisabledTooltip("Must be on File Select to generate a randomizer seed."));
    AddWidget(path, "Spoiler File", WIDGET_CUSTOM)
        .CustomFunction([](WidgetInfo& info) {
            JoinRandoGenerationThread();
            if (!CVarGetInteger(CVAR_RANDOMIZER_SETTING("DontGenerateSpoiler"), 0)) {
                std::string spoilerfilepath = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
                ImGui::Text("Spoiler File: %s", spoilerfilepath.c_str());
            }
        })
        .SameLine(true);
#ifdef RSBS_SINGLE_EXECUTABLE
    // RSBS (#1044): Generate writes the paired world as a seed package; these rows
    // load one from the Randomizer folder and export the staged one.
    AddWidget(path, "Seed Package", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Seed Package Rows", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) { DrawSeedPackageRows(); });
#endif

    // Enhancements
    AddWidget(path, "Enhancements", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "These enhancements are only useful in the Randomizer mode but do not affect the randomizer logic.",
              WIDGET_TEXT)
        .Options(TextOptions().Color(UIWidgets::Colors::Gray));
    AddWidget(path, "Rando-Relevant Navi Hints", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("RandoRelevantNavi"))
        .Options(CheckboxOptions()
                     .Tooltip("Replace Navi's overworld quest hints with rando-related gameplay hints.")
                     .DefaultValue(true));
    AddWidget(path, "Random Rupee Names", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("RandomizeRupeeNames"))
        .RaceDisable(false)
        .Options(CheckboxOptions()
                     .Tooltip("When obtaining Rupees, randomize what the Rupee is called in the textbox.")
                     .DefaultValue(true));
    AddWidget(path, "Use Custom Key Models", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("CustomKeyModels"))
        .Options(
            CheckboxOptions()
                .Tooltip("Use Custom graphics for Dungeon Keys, Big and Small, so that they can be easily told apart.")
                .DefaultValue(true));
    AddWidget(path, "Map & Compass Colors Match Dungeon", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("ColoredMapsAndCompasses"))
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled = !(OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_MAPANDCOMPASS)
                                           .IsNot(RO_DUNGEON_ITEM_LOC_STARTWITH) &&
                                       OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_MAPANDCOMPASS)
                                           .IsNot(RO_DUNGEON_ITEM_LOC_VANILLA) &&
                                       OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_MAPANDCOMPASS)
                                           .IsNot(RO_DUNGEON_ITEM_LOC_OWN_DUNGEON));
            info.options->disabledTooltip =
                "This setting is disabled because a savefile is loaded without the map & compass.\n"
                "Shuffle settings set to \"Any Dungeon\", \"Overworld\" or \"Anywhere\".";
        })
        .Options(
            CheckboxOptions()
                .Tooltip("Matches the color of maps & compasses to the dungeon they belong to. "
                         "This helps identify maps & compasses from afar and adds a little bit of flair.\n\nThis only "
                         "applies to seeds with maps & compasses shuffled to \"Any Dungeon\", \"Overworld\", or "
                         "\"Anywhere\".")
                .DefaultValue(true));
    AddWidget(path, "Jabber Nut Colors Match Kind", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("GenericJabberNutModel"))
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled = !OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_SPEAK);
            info.options->disabledTooltip =
                "This setting is disabled because a savefile is loaded without Shuffle Speak.";
        })
        .RaceDisable(false)
        .Options(CheckboxOptions()
                     .Tooltip("With Shuffle Speak, jabber nut model & color will be generic.")
                     .DefaultValue(true));
    AddWidget(path, "Quest Item Fanfares", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("QuestItemFanfares"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Play unique fanfares when obtaining quest items (medallions/stones/songs). Note that these "
            "fanfares can be longer than usual."));
    AddWidget(path, "Mysterious Shuffled Items", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"))
        .Options(CheckboxOptions().Tooltip(
            "Displays a \"Mystery Item\" model in place of any freestanding/GS/shop items that were shuffled, "
            "and replaces item names for them and scrubs and merchants, regardless of hint settings, "
            "so you never know what you're getting."));
    AddWidget(path, "Simpler Boss Soul Models", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("SimplerBossSoulModels"))
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "When shuffling boss souls, they'll appear as a simpler model instead of showing the boss' models."
            "This might make boss souls more distinguishable from a distance, and can help with performance."));
    AddWidget(path, "Skip Get Item Animations", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"))
        .Options(ComboboxOptions().ComboMap(skipGetItemAnimationOptions).DefaultIndex(SGIA_JUNK));
    AddWidget(path, "Item Scale: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimationScale"))
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled =
                !CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("TimeSavers.SkipGetItemAnimation"), SGIA_JUNK);
            info.options->disabledTooltip =
                "This slider only applies when using the \"Skip Get Item Animations\" option.";
        })
        .Options(FloatSliderOptions().Min(5.0f).Max(15.0f).Format("%.2f").DefaultValue(10.0f).Tooltip(
            "The size of the item when it is picked up."));
    AddWidget(path, "Signs Hint Entrances", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_RANDOMIZER_ENHANCEMENT("EntrancesOnSigns"))
        .Options(CheckboxOptions().Tooltip("If enabled, signs near loading zones will tell you where they lead to."));

    auto randoSettings = Rando::Settings::GetInstance();
    randoSettings->CreateOptions();
    randoSettings->GetOptionGroup(RSG_MENU_SIDEBAR_LOGIC_ACCESS).AddWidgets(path);
    randoSettings->GetOptionGroup(RSG_MENU_SIDEBAR_DUNGEONS).AddWidgets(path);
    randoSettings->GetOptionGroup(RSG_MENU_SIDEBAR_SHUFFLES).AddWidgets(path);
    randoSettings->GetOptionGroup(RSG_MENU_SIDEBAR_HINTS_TRAPS).AddWidgets(path);
    randoSettings->GetOptionGroup(RSG_MENU_SIDEBAR_STARTING_ITEMS).AddWidgets(path);
    path.sidebarName = "Locations";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);
    AddWidget(path, "Excluded Locations", WIDGET_CUSTOM).CustomFunction(DrawLocationsMenu);
    path.sidebarName = "Tricks/Glitches";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);
    AddWidget(path, "Tricks/Glitches", WIDGET_CUSTOM).CustomFunction(DrawTricksMenu);

    // RSBS (#1101): one randomizer menu for both games. MM Randomizer, MM Tricks
    // and Cross-Game (the cross-game rules), registered by SohMenuCombo.cpp.
    AddComboRandomizerPages(*this, path);

    // Plandomizer
    path.sidebarName = "Plandomizer";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);
    AddWidget(path, "Popout Plandomizer Window", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("PlandomizerEditor"))
        .RaceDisable(false)
        .WindowName("Plandomizer Editor")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Randomizer Settings Window."));

    // Item Tracker
    path.sidebarName = "Item Tracker";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);

    AddWidget(path, "Item Tracker", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Toggle Item Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("ItemTracker"))
        .RaceDisable(false)
        .WindowName("Item Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Item Tracker.").EmbedWindow(false));

    AddWidget(path, "Item Tracker Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Item Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("ItemTrackerSettings"))
        .RaceDisable(false)
        .WindowName("Item Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Item Tracker Settings Window."));

    // Entrance Tracker
    path.sidebarName = "Entrance Tracker";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);

    AddWidget(path, "Entrance Tracker", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Toggle Entrance Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("EntranceTracker"))
        .RaceDisable(false)
        .WindowName("Entrance Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Entrance Tracker.").EmbedWindow(false));

    AddWidget(path, "Entrance Tracker Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Entrance Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("EntranceTrackerSettings"))
        .RaceDisable(false)
        .WindowName("Entrance Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Entrance Tracker Settings Window."));

    // Check Tracker
    path.sidebarName = "Check Tracker";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);

    AddWidget(path, "Check Tracker", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Toggle Check Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("CheckTracker"))
        .RaceDisable(false)
        .WindowName("Check Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Check Tracker.").EmbedWindow(false));

    AddWidget(path, "Check Tracker Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Popout Check Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("CheckTrackerSettings"))
        .RaceDisable(false)
        .WindowName("Check Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Check Tracker Settings Window."));
}

} // namespace SohGui
