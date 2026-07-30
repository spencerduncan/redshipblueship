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

    // Cross-Game (RedShipBlueShip #509). Interim host for the two common-owned
    // combo windows (ADR 0008). Both were registered on the shared Gui at
    // startup (rsbs/src/main.cpp Combo_MMOptionsWindow_Init / Combo_SpoilerWindow_Init)
    // but nothing wrote their visibility CVars, so the only way to open them was
    // the console (set gCombo.Windows.MMOptions 1). These rows are the writer.
    //
    // WINDOW_BUTTON reads .CVar only for the open/close label and calls the
    // window's own ToggleVisibility (UIWidgets.cpp:198), so .CVar MUST equal the
    // window's ctor visibility CVar and .WindowName its registered name. String
    // literals match the tracker rows above (they hardcode the same way); the
    // authoritative constants are ComboGui::kComboMMOptions*/kComboSpoiler* in
    // src/common/ComboMmOptionsWindow.h / ComboSpoilerWindow.h.
    //
    // This is the interim, NOT #497 step 3 (the SohMenu capability-gating
    // manifest). Both windows read only gComboCtx/CVars, never either game's
    // gSaveContext (ADR 0008 rule 5), so they are safe under every GameId and the
    // rows are ungated.
    path.sidebarName = "Cross-Game";
    AddSidebarEntry("Randomizer", path.sidebarName, 1);

    AddWidget(path, "Cross-Game", WIDGET_SEPARATOR_TEXT);
    // Seed configuration — always reachable, like the trackers.
    AddWidget(path, "Toggle MM Randomizer Options", WIDGET_WINDOW_BUTTON)
        .CVar("gCombo.Windows.MMOptions")
        .RaceDisable(false)
        .WindowName("Majora's Mask Randomizer Options")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Toggles the Majora's Mask randomizer options pane (the paired MM world's settings).")
                     .EmbedWindow(false));
    // Spoiler — reveals paired-seed placements, so race-disabled like a spoiler tool.
    AddWidget(path, "Toggle Cross-Game Spoiler", WIDGET_WINDOW_BUTTON)
        .CVar("gCombo.Windows.Spoiler")
        .RaceDisable(true)
        .WindowName("Cross-Game Spoiler")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Toggles the cross-game spoiler (which MM check hosts which OoT item, and vice versa).")
                     .EmbedWindow(false));
    // Combo tracker (#458) — both games' progress at once, the inactive game's
    // included ("as of last freeze/save"). Race-disabled like the spoiler: its
    // cross-game section names items sitting on uncollected checks.
    // Constants: ComboGui::kComboTracker* in src/common/ComboTrackerWindow.h.
    AddWidget(path, "Toggle Combo Tracker", WIDGET_WINDOW_BUTTON)
        .CVar("gCombo.Windows.Tracker")
        .RaceDisable(true)
        .WindowName("Combo Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Toggles the combo tracker (both games' check progress at once, including the game "
                              "that is not running).")
                     .EmbedWindow(false));

    // MM's four tracker windows had the same unreachability bug as the two
    // windows above. #489 made them openable and correctly named (they register
    // under "MM "-prefixed names, since SoH owns the unprefixed ones), but
    // nothing in the live menu ever wrote their visibility CVars:
    //   - MM's own menu, BenGui.cpp SetupGuiElements, is excluded from single-exe
    //     (0 hits in redship.map) — that was their intended writer.
    //   - The OoT tracker rows above cannot serve: CVAR_WINDOW expands to
    //     "gOpenWindows.*" (CVAR_PREFIX_WINDOW, CMake/soh-cvars.cmake:8) while
    //     MM's trackers read "gWindows.*". Different CVars entirely, by design —
    //     the distinct namespaces are what avoid a store collision.
    //   - ItemTrackerSettings.cpp does link and has its own Enable/Disable
    //     button, but it is drawn INSIDE the settings window, which has no
    //     writer either. Chicken-and-egg.
    // So the only way to open them was the console. These rows are the writer.
    //
    // Names and CVars are the constants in games/mm/2s2h/TrackersGuiSingleExe.h
    // (kCheckTracker*/kItemTracker*), spelled as literals to match the rows
    // above. The windows are MMActiveGated, so they draw only while MM is the
    // running game — the buttons are usable from the first frame, the window
    // simply stays blank under OoT, which is the upstream behavior.
    //
    // "From the first frame" only holds because rsbs/src/main.cpp registers the
    // four windows at startup (#535). They used to register from MM_Rando_Init,
    // i.e. only once MM had booted in this process, which left this separator
    // sitting over four rows whose per-frame GetGuiWindow lookup failed. Menu.cpp
    // now greys such a row out instead of skipping it, but that is the fallback.
    //
    // EmbedWindow(false) on all four, unlike the OoT settings rows above: the
    // embed path calls window->DrawElement() directly, which bypasses the
    // MMActiveGated Draw wrapper — the only thing keeping MM tracker UI from
    // drawing while OoT is the running game, and (before mm.o2r is mounted) from
    // reaching for tracker icons that are not loaded. Pop-out only, so the gate
    // stays the single authority on when MM tracker UI draws.
    AddWidget(path, "Majora's Mask Trackers", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Toggle MM Item Tracker", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.ItemTracker")
        .RaceDisable(false)
        .WindowName("MM Item Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Majora's Mask item tracker.").EmbedWindow(false));
    AddWidget(path, "Popout MM Item Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.ItemTrackerSettings")
        .RaceDisable(false)
        .WindowName("MM Item Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Enables the Majora's Mask Item Tracker Settings window.")
                     .EmbedWindow(false));
    AddWidget(path, "Toggle MM Check Tracker", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.CheckTracker")
        .RaceDisable(false)
        .WindowName("MM Check Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Majora's Mask check tracker.").EmbedWindow(false));
    AddWidget(path, "Popout MM Check Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.CheckTrackerSettings")
        .RaceDisable(false)
        .WindowName("MM Check Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Enables the Majora's Mask Check Tracker Settings window.")
                     .EmbedWindow(false));
}

} // namespace SohGui
