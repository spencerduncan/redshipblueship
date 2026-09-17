#include "SohMenu.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipUtils.h"
#include "soh/SohGui/SohGui.hpp"

// src/common — the Cross-Game combo rules' model (#655, ADR 0011 increment 2).
// ADR 0008 rule 5's restatement for an OoT-hosted row: the row may read the
// model through these accessors, but never gComboCtx and never either game's
// gSaveContext.
#include "combo_settings_view.h"
#include "foreign_items.h"

#include <cstdio> // snprintf, for the combo-rule status line and fingerprint

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

// ============================================================================
// Cross-Game combo rules (#655; ADR 0011 increment 2, #498)
// ============================================================================
// The six tier-4 `gCombo.Rando.*` keys — direction, per-direction pool sizes,
// per-direction item classes, and the shared ocarina (#668) — render as ROWS in
// the interim Cross-Game page
// below. PR #652 shipped them as a common-owned pop-out pane
// (src/common/ComboSettingsWindow.cpp); operator direction after the 2026-09-11
// nightly was that they "should be built into the menu itself like all the
// other combo settings instead of being pop out panes", which is #655. Nothing
// but the presentation moved: the model, the creation-time resolver and the
// freeze gate are all still src/common's.
//
// WHY THE POINTER-BASED WIDGET TYPES AND NOT WIDGET_CVAR_*. A WIDGET_CVAR_*
// widget is its own writer — UIWidgets::CVarCombobox/CVarCheckbox/CVarSliderInt
// call CVarSetInteger themselves — so binding these keys to one would put a
// second, ungated writer beside src/common's. ADR 0004 §6's enforcement rule is
// that the gate lives on the src/common write choke points and NOT on the
// widget, precisely because a greyed widget over an open writer is decorative.
// So each row is a pointer-based widget over a staging buffer: its PreFunc
// refreshes the buffer FROM the model every frame, the widget edits the buffer,
// and its Callback offers the edit to Combo_ComboSettingSet, which refuses once
// Combo_ComboSettingsFrozen(). A refused write simply loses — the next frame's
// PreFunc overwrites the buffer with the model's value again — so the rows can
// never disagree with the record about what this world's rules are.
static int32_t comboRuleDirection;
static int32_t comboRulePoolSize[2];  // [0] OoT items -> MM checks, [1] MM items -> OoT checks
static bool comboRuleItemClass[2][6]; // [0] OoT pool, [1] MM pool; second index is comboRuleClassBits'
static bool comboRuleSharedOcarina;   // #668: ComboSettingsRecord.comboFlags' shared-ocarina bit

// The allocated RSBS_ITEMCLASS_* bits in bit order (foreign_items.h). Appending
// a class is a new checkbox here; re-pointing an existing bit is forbidden
// there, because these are .redsave format.
static const uint16_t comboRuleClassBits[6] = {
    (uint16_t)RSBS_ITEMCLASS_PROGRESSION,    (uint16_t)RSBS_ITEMCLASS_SONGS,
    (uint16_t)RSBS_ITEMCLASS_MASKS,          (uint16_t)RSBS_ITEMCLASS_DUNGEON_ITEMS,
    (uint16_t)RSBS_ITEMCLASS_DUNGEON_REWARD, (uint16_t)RSBS_ITEMCLASS_SIDEQUEST,
};

// The four pinned RSBS_COMBO_DIR_* enumerators (1..4, static_asserted in
// foreign_items.h because they are .redsave format), with the copy that says
// what each one does rather than what it is called.
static const std::map<int32_t, const char*> comboRuleDirectionOptions = {
    { (int32_t)RSBS_COMBO_DIR_OFF, "Off (paired world, no crossings)" },
    { (int32_t)RSBS_COMBO_DIR_FORWARD, "Ocarina of Time items into Majora's Mask only" },
    { (int32_t)RSBS_COMBO_DIR_REVERSE, "Majora's Mask items into Ocarina of Time only" },
    { (int32_t)RSBS_COMBO_DIR_BOTH, "Both directions" },
};

// A direction value the direction row's combo map had to be taught about at
// runtime, or 0. See ComboRuleDirectionPreFunc: std::map::at throws, and
// MenuDrawItem catches only bad_variant_access.
static int32_t comboRuleUnknownDirection = 0;

// The status line above the rows, rebuilt each frame into the widget's name.
static std::string comboRuleStatusText;

/**
 * The row NAME for @p id: ADR 0004 §4.2's persistent marker, then the model's
 * label. Built from Combo_ComboSettingSharedMarker / Combo_ComboSettingLabel so
 * the badge and the wording have one definition and a headless lock can
 * reconstruct exactly what the row is called.
 */
static std::string ComboRuleRowName(ComboSettingId id) {
    return std::string(Combo_ComboSettingSharedMarker()) + " " + Combo_ComboSettingLabel(id);
}

/**
 * WHICH values the rows show, and whether they are still a choice (ADR 0004 §6
 * state 4).
 *
 * Post-creation the values come FROM THE SAVE — the frozen record, through the
 * src/common accessor — and never from the CVar: after creation the two may
 * legitimately differ, and the save is the one the world was built from.
 * Pre-creation the resolver is used, i.e. exactly what a creation event would
 * freeze if it ran this frame.
 *
 * @return true when the record is frozen (the rows are read-only).
 */
static bool ComboRuleShownRecord(ComboSettingsRecord* out) {
    if (Combo_ComboSettingsFrozen()) {
        ComboSettingsSummary summary;
        Combo_ComboSettingsSummary(&summary);
        *out = summary.record;
        return true;
    }
    Combo_ResolveComboSettings(out);
    return false;
}

/**
 * The read-only half of state 4. The reason string is the MODEL's
 * (Combo_ComboSettingReadOnlyReason) and is deliberately NOT a capability
 * reason: a capability gate says "not yet available" and sends a player hunting
 * for a missing feature, while a freeze says "already decided". It tracks
 * Combo_ComboSettingsFrozen() exactly, so the greying here and the writers'
 * refusal in src/common cannot disagree about which state they are in.
 *
 * The greying is honest presentation, not the gate — Combo_ComboSettingSet
 * refuses on its own, which is what makes this safe to be merely cosmetic.
 * Under a race lockout MenuDrawItem replaces this tooltip with its own reason;
 * that is why the status row above the group states the freeze in text, where
 * §4.2 and §6 want it: legible without hovering.
 */
static void ComboRuleApplyDecided(WidgetInfo& info, bool decided) {
    if (!decided) {
        return;
    }
    const char* reason = Combo_ComboSettingReadOnlyReason();
    info.options->disabled = true;
    info.options->disabledTooltip = (reason != nullptr) ? reason : "";
}

// Defined below AddMenuRandomizer, which calls it; see its doc comment for why
// the Cross-Game page is a named function of its own rather than part of that
// body.
void AddCrossGameWidgets(SohMenu& menu, WidgetPath& path);

/** The item-class staging index for @p id: 0 for the OoT pool, 1 for MM's. */
static int ComboRuleClassIndex(ComboSettingId id) {
    return (id == COMBO_SETTING_ITEM_CLASS_OOT) ? 0 : 1;
}

/** The class mask @p record holds for staging index @p which. */
static uint16_t ComboRuleClassMask(const ComboSettingsRecord& record, int which) {
    return (which == 0) ? record.itemClassOoT : record.itemClassMM;
}

/**
 * The direction row's per-frame refresh.
 *
 * The combo-map guard is load-bearing, not defensive noise: UIWidgets::Combobox
 * previews with `comboMap.at(*value)`, and std::map::at THROWS on a key it does
 * not hold while MenuDrawItem catches only std::bad_variant_access — so an
 * unknown direction would take the process down. A frozen record can carry one
 * legitimately: RSBS_COMBO_DIR_* is append-only .redsave format, so a save
 * written by a later build may name a direction this one does not know. It is
 * shown as what it is rather than clamped to the nearest label, because a clamp
 * would display a rule the world was not built from — the precise error ADR
 * 0004 §6 state 4 exists to prevent. The taught entry is withdrawn again as
 * soon as the shown value is one of the four pinned enumerators, so the dropdown
 * does not accumulate values nobody can choose.
 */
static void ComboRuleDirectionPreFunc(WidgetInfo& info) {
    ComboSettingsRecord shown;
    const bool decided = ComboRuleShownRecord(&shown);
    comboRuleDirection = (int32_t)shown.direction;

    auto options = std::static_pointer_cast<ComboboxOptions>(info.options);
    if (comboRuleUnknownDirection != 0 && comboRuleUnknownDirection != comboRuleDirection) {
        options->comboMap.erase(comboRuleUnknownDirection);
        comboRuleUnknownDirection = 0;
    }
    if (!options->comboMap.contains(comboRuleDirection)) {
        static char unknownLabel[32];
        snprintf(unknownLabel, sizeof(unknownLabel), "(unknown direction %d)", (int)comboRuleDirection);
        options->comboMap[comboRuleDirection] = unknownLabel;
        comboRuleUnknownDirection = comboRuleDirection;
    }

    ComboRuleApplyDecided(info, decided);
}

/**
 * The status line above the rows — ADR 0004 §6 state 4's "labelled with the
 * reason and with the identity it is frozen to", and the one place the state is
 * legible without hovering (a disabled widget's tooltip is not, and under a race
 * lockout MenuDrawItem overwrites that tooltip anyway).
 *
 * Three states, because they are three different facts and only one of them is
 * "the defaults are fine":
 *   - frozen: the reason from the MODEL, plus the fingerprint the world was
 *     built from and the escape that actually exists (the title screen, where
 *     Context_InvalidateSessionState drops the freeze — not "create a new
 *     file", which is advice a player cannot act on from here);
 *   - paired but not frozen: a legacy pre-carve pair (ADR 0011 decision 4.2),
 *     whose rules are the shipped defaults recorded at its first crossing, so
 *     these rows describe the NEXT world rather than the live one;
 *   - unpaired: these freeze into the next paired world at generation.
 */
static void ComboRuleStatusPreFunc(WidgetInfo& info) {
    ComboSettingsSummary summary;
    Combo_ComboSettingsSummary(&summary);
    const char* reason = Combo_ComboSettingReadOnlyReason();

    char buffer[768];
    if (reason != nullptr && !summary.paired) {
        // A frozen record with no live pairing is a state no created combo file
        // may be in (ADR 0011 decision 4.2). Combo_ComboSettingsSummary
        // deliberately reports an ABSENT record for it rather than presenting
        // gComboCtx's zeros as rules, so the rows below show zeros — say why,
        // instead of letting a player read "no classes armed" as their world.
        snprintf(buffer, sizeof(buffer),
                 "The cross-game rules are frozen but no paired world is live — corrupt session state. The values "
                 "below are not this session's rules; return to the title screen.");
    } else if (reason != nullptr) {
        snprintf(buffer, sizeof(buffer),
                 "Already decided: this paired world's cross-game rules were frozen into its identity "
                 "(fingerprint %08X) when the world was created. The values below are read from the save, not "
                 "from the settings store, and cannot be changed for this pair. To play under different rules, "
                 "return to the title screen — these unlock there — then set them and generate a new seed.",
                 (unsigned)summary.comboSettingsHash);
    } else if (summary.paired) {
        snprintf(buffer, sizeof(buffer),
                 "These freeze into the paired world's identity. The world you are paired with predates them: it "
                 "was generated when there was only one rule set, and the shipped defaults are recorded into it at "
                 "its first crossing. The values below describe the NEXT world you create — leave them at the "
                 "defaults before crossing into this one.");
    } else {
        snprintf(buffer, sizeof(buffer),
                 "These freeze into the paired world's identity. They govern the crossing between both games and "
                 "are decided once, when a randomized Ocarina of Time world is generated. After that the record in "
                 "the save is the authority, and a crossing or a load whose authored rules no longer match it is "
                 "refused by name.");
    }
    comboRuleStatusText = buffer;
    info.name = comboRuleStatusText;
}

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

    AddCrossGameWidgets(*this, path);
}

/**
 * Register the interim Cross-Game page (#509, #655): the tier-4 combo rules, the
 * common-owned cross-game windows, and MM's four trackers.
 *
 * A named, externally linked function rather than part of AddMenuRandomizer's
 * body for one reason: the combo rules' headless lock
 * (games/oot/soh/soh_combo_settings_rows_test.cpp) has to register these rows in
 * order to assert anything about them, and AddMenuRandomizer as a whole cannot
 * run ROM-free. Its five Rando::Settings option groups reach Option::AddWidget,
 * which RUNS each option's callback at registration time
 * (Enhancements/randomizer/option.cpp:330), and those callbacks dereference
 * OTRGlobals::Instance->gRandoContext -- null in every display-free harness. So
 * the lock drives this page directly. It is declared in no header: the test
 * declares it itself, the way the SohGui TUs already declare SohGui::mSohMenu,
 * so no production header grows a test-only entry point.
 */
void AddCrossGameWidgets(SohMenu& menu, WidgetPath& path) {
    // PIN THE COLUMN. path.column is not ours to inherit: OptionGroup::AddWidgets
    // advances it per COLUMN container (option.cpp:469-472) and those groups run
    // immediately before this page, so what arrives here is whatever the last
    // settings column left behind. Menu::DrawElement draws only columnCount
    // columns, so a row parked past that is registered and never drawn -- #640's
    // failure mode with the halves swapped. This page declares one column below,
    // so it takes the first.
    path.column = SECTION_COLUMN_1;

    // Cross-Game (RedShipBlueShip #509). Host for the tier-4 combo rules' own
    // rows (#655, below) and interim host for the common-owned combo windows
    // (ADR 0008). Those were registered on the shared Gui at startup
    // (rsbs/src/main.cpp Combo_MMOptionsWindow_Init / Combo_SpoilerWindow_Init)
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
    menu.AddSidebarEntry("Randomizer", path.sidebarName, 1);

    // ---- The combo rules themselves (#655, #668) ---------------------------
    // Six settings, rendered as rows rather than as the pop-out pane PR #652
    // shipped. See the block comment at the top of this file for why every row
    // is a pointer-based widget over a src/common writer rather than a
    // WIDGET_CVAR_* one, and for which value each row shows in which state.
    menu.AddWidget(path, "Cross-Game Combo Rules", WIDGET_SEPARATOR_TEXT);
    // The state line. Its name is rewritten every frame by its PreFunc, so it is
    // kept out of the menu search rather than seeding it with a paragraph.
    menu.AddWidget(path, "Combo Rules Status", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(ComboRuleStatusPreFunc)
        .Options(TextOptions().Color(UIWidgets::Colors::Gray));

    menu.AddWidget(path, ComboRuleRowName(COMBO_SETTING_DIRECTION), WIDGET_COMBOBOX)
        .ValuePointer(&comboRuleDirection)
        .PreFunc(ComboRuleDirectionPreFunc)
        .Callback([](WidgetInfo& info) { Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, comboRuleDirection); })
        .Options(ComboboxOptions()
                     .ComboMap(comboRuleDirectionOptions)
                     .Tooltip("Which way items may cross between the two games. \"Off\" is a real paired world with "
                              "no crossings, not a broken one."));

    // Pool sizes. The label carries a %d because UIWidgets::SliderInt renders an
    // Above-positioned label through ImGui::Text(label, *value) — the same shape
    // as the "Item Scale: %.2f" row above. Bounds are the model's pinned space
    // (1..RSBS_FOREIGN_PLACEMENT_CAP), not local numbers: a slider that can
    // reach a value Combo_ComboSettingSet refuses is a control that lies.
    menu.AddWidget(path, ComboRuleRowName(COMBO_SETTING_POOL_SIZE_OOT) + ": %d", WIDGET_SLIDER_INT)
        .ValuePointer(&comboRulePoolSize[0])
        .PreFunc([](WidgetInfo& info) {
            ComboSettingsRecord shown;
            const bool decided = ComboRuleShownRecord(&shown);
            comboRulePoolSize[0] = (int32_t)shown.poolSizeOoT;
            ComboRuleApplyDecided(info, decided);
        })
        .Callback([](WidgetInfo& info) { Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_OOT, comboRulePoolSize[0]); })
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max((int32_t)RSBS_FOREIGN_PLACEMENT_CAP)
                     .DefaultValue(Combo_ComboSettingDefault(COMBO_SETTING_POOL_SIZE_OOT))
                     .Tooltip("How many Ocarina of Time items may be placed on Majora's Mask checks at most."));
    menu.AddWidget(path, ComboRuleRowName(COMBO_SETTING_POOL_SIZE_MM) + ": %d", WIDGET_SLIDER_INT)
        .ValuePointer(&comboRulePoolSize[1])
        .PreFunc([](WidgetInfo& info) {
            ComboSettingsRecord shown;
            const bool decided = ComboRuleShownRecord(&shown);
            comboRulePoolSize[1] = (int32_t)shown.poolSizeMM;
            ComboRuleApplyDecided(info, decided);
        })
        .Callback([](WidgetInfo& info) { Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_MM, comboRulePoolSize[1]); })
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max((int32_t)RSBS_FOREIGN_PLACEMENT_CAP)
                     .DefaultValue(Combo_ComboSettingDefault(COMBO_SETTING_POOL_SIZE_MM))
                     .Tooltip("How many Majora's Mask items may be placed on Ocarina of Time checks at most."));

    // The two item-class bitsets. One setting each, six checkboxes each: the
    // marker and the model's label ride on the group's header row, and the
    // Callback rebuilds the WHOLE mask from the six staging bits so the store
    // never holds a half-applied one. Checkbox names are prefixed per direction
    // because the six class names repeat and a widget name must be unique.
    for (const ComboSettingId classId : { COMBO_SETTING_ITEM_CLASS_OOT, COMBO_SETTING_ITEM_CLASS_MM }) {
        const int which = ComboRuleClassIndex(classId);
        const std::string rowPrefix = (which == 0) ? "Ocarina of Time item class: " : "Majora's Mask item class: ";

        menu.AddWidget(path, ComboRuleRowName(classId), WIDGET_SEPARATOR_TEXT);
        for (int bit = 0; bit < 6; bit++) {
            menu.AddWidget(path, rowPrefix + Combo_ForeignItemClassName(comboRuleClassBits[bit]), WIDGET_CHECKBOX)
                .ValuePointer(&comboRuleItemClass[which][bit])
                .PreFunc([which, bit](WidgetInfo& info) {
                    ComboSettingsRecord shown;
                    const bool decided = ComboRuleShownRecord(&shown);
                    comboRuleItemClass[which][bit] =
                        (ComboRuleClassMask(shown, which) & comboRuleClassBits[bit]) != 0;
                    ComboRuleApplyDecided(info, decided);
                })
                .Callback([classId, which](WidgetInfo& info) {
                    uint16_t next = 0;
                    for (int b = 0; b < 6; b++) {
                        if (comboRuleItemClass[which][b]) {
                            next |= comboRuleClassBits[b];
                        }
                    }
                    Combo_ComboSettingSet(classId, (int32_t)next);
                })
                .Options(CheckboxOptions().Tooltip(
                    "Whether items of this class may cross. An empty set is a legitimate world in which this "
                    "direction places nothing."));
        }
        // An empty mask is legal (ADR 0011 decision 3.3) but worth naming: the
        // placement pass logs "no crossings" and places nothing, and a player
        // who did not mean that would read it as a broken pool.
        menu.AddWidget(path,
                       which == 0 ? "No Ocarina of Time classes armed: this direction places nothing."
                                  : "No Majora's Mask classes armed: this direction places nothing.",
                       WIDGET_TEXT)
            .RaceDisable(false)
            .HideInSearch(true)
            .PreFunc([which](WidgetInfo& info) {
                ComboSettingsRecord shown;
                ComboRuleShownRecord(&shown);
                info.isHidden = ComboRuleClassMask(shown, which) != 0;
            })
            .Options(TextOptions().Color(UIWidgets::Colors::Gray));
    }

    // The shared ocarina (#668). A comboFlags BIT rather than a field of its
    // own, so the row is a plain checkbox over the model's 0/1 space; everything
    // else about it is the five rows' pattern verbatim — the staging buffer, the
    // PreFunc that refreshes from the record, the Callback that offers the edit
    // to src/common's writer, and the marker in the name.
    menu.AddWidget(path, ComboRuleRowName(COMBO_SETTING_SHARED_OCARINA), WIDGET_CHECKBOX)
        .ValuePointer(&comboRuleSharedOcarina)
        .PreFunc([](WidgetInfo& info) {
            ComboSettingsRecord shown;
            const bool decided = ComboRuleShownRecord(&shown);
            comboRuleSharedOcarina = (shown.comboFlags & (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA) != 0;
            ComboRuleApplyDecided(info, decided);
        })
        .Callback([](WidgetInfo& info) {
            Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, comboRuleSharedOcarina ? 1 : 0);
        })
        .Options(CheckboxOptions().Tooltip(
            "Treat the ocarina as ONE instrument across both games: finding an ocarina in either game gives you "
            "one in the other. Ocarina of Time's Fairy Ocarina counts, because Majora's Mask has only one "
            "ocarina; Majora's Mask's ocarina gives you the Fairy Ocarina in Ocarina of Time, never the Ocarina "
            "of Time itself."));

    menu.AddWidget(path, "Reset Combo Rules To Defaults", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            // A live Reset under a frozen record would be a control that
            // (correctly) does nothing — ADR 0004 §5's vacuous-gate class.
            ComboRuleApplyDecided(info, Combo_ComboSettingsFrozen());
        })
        .Callback([](WidgetInfo& info) {
            // Clears rather than writing the defaults back: an unset key and a
            // key explicitly holding the default resolve identically today, but
            // only the cleared one reads as "the player never touched it".
            for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
                Combo_ComboSettingClear((ComboSettingId)i);
            }
        })
        .Options(ButtonOptions()
                     .Size(ImVec2(250.f, 0.f))
                     .Tooltip("Clears all six rules back to the values RedShipBlueShip ships with."));

    // ---- The common-owned cross-game windows --------------------------------
    menu.AddWidget(path, "Cross-Game Windows", WIDGET_SEPARATOR_TEXT);
    // Seed configuration — always reachable, like the trackers.
    menu.AddWidget(path, "Toggle MM Randomizer Options", WIDGET_WINDOW_BUTTON)
        .CVar("gCombo.Windows.MMOptions")
        .RaceDisable(false)
        .WindowName("Majora's Mask Randomizer Options")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Toggles the Majora's Mask randomizer options pane (the paired MM world's settings).")
                     .EmbedWindow(false));
    // NO "Toggle Combo Settings" ROW (#655). PR #652 put one here, opening the
    // common-owned ComboSettingsWindow pane. The five settings it rendered are
    // the rows above now, so a button that opens a second surface over the same
    // five keys would be the only way for the two to disagree. The window stays
    // REGISTERED (ComboMmOptionsWindow.cpp and rsbs/src/main.cpp both call
    // Combo_ComboSettingsWindow_Init, and its headless lock still drives it) but
    // nothing in the menu writes gCombo.Windows.ComboSettings any more, so it
    // does not appear. See src/common/ComboSettingsWindow.h for why it was kept
    // rather than deleted.
    //
    // Spoiler — reveals paired-seed placements, so race-disabled like a spoiler tool.
    menu.AddWidget(path, "Toggle Cross-Game Spoiler", WIDGET_WINDOW_BUTTON)
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
    menu.AddWidget(path, "Toggle Combo Tracker", WIDGET_WINDOW_BUTTON)
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
    menu.AddWidget(path, "Majora's Mask Trackers", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle MM Item Tracker", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.ItemTracker")
        .RaceDisable(false)
        .WindowName("MM Item Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Majora's Mask item tracker.").EmbedWindow(false));
    menu.AddWidget(path, "Popout MM Item Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.ItemTrackerSettings")
        .RaceDisable(false)
        .WindowName("MM Item Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Enables the Majora's Mask Item Tracker Settings window.")
                     .EmbedWindow(false));
    menu.AddWidget(path, "Toggle MM Check Tracker", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.CheckTracker")
        .RaceDisable(false)
        .WindowName("MM Check Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Majora's Mask check tracker.").EmbedWindow(false));
    menu.AddWidget(path, "Popout MM Check Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.CheckTrackerSettings")
        .RaceDisable(false)
        .WindowName("MM Check Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Enables the Majora's Mask Check Tracker Settings window.")
                     .EmbedWindow(false));
}

} // namespace SohGui
