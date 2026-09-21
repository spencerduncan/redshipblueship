#include "SohMenu.h"
#include <ship/window/gui/GuiMenuBar.h>
#include <ship/window/gui/GuiElement.h>
#include <ship/utils/StringHelper.h>
#include <spdlog/fmt/fmt.h>

#include <string>

// src/common - the capability predicates' observed facts and the one
// definition of ADR 0004 section 4.2's marker (#497 steps 3 and 5). ADR 0008
// rule 5 for an OoT-hosted row: these accessors, never gComboCtx and never
// either game's gSaveContext.
#include "combo_settings_view.h"
#include "cvar_shared_keys.h"
#include "foreign_items.h"
#include "game.h"

extern "C" {
extern PlayState* OoT_gPlayState;
}

extern std::unordered_map<s16, const char*> warpPointSceneList;

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;

using namespace UIWidgets;

void SohMenu::AddSidebarEntry(std::string sectionName, std::string sidebarName, uint32_t columnCount) {
    assert(!sectionName.empty());
    assert(!sidebarName.empty());
    menuEntries.at(sectionName).sidebars.emplace(sidebarName, SidebarEntry{ .columnCount = columnCount });
    menuEntries.at(sectionName).sidebarOrder.push_back(sidebarName);
}

WidgetInfo& SohMenu::AddWidget(WidgetPath& pathInfo, std::string widgetName, WidgetType widgetType) {
    assert(!widgetName.empty());                        // Must be unique
    assert(menuEntries.contains(pathInfo.sectionName)); // Section/header must already exist
    assert(menuEntries.at(pathInfo.sectionName).sidebars.contains(pathInfo.sidebarName)); // Sidebar must already exist
    std::unordered_map<std::string, SidebarEntry>& sidebar = menuEntries.at(pathInfo.sectionName).sidebars;
    uint8_t column = pathInfo.column;
    if (sidebar.contains(pathInfo.sidebarName)) {
        while (sidebar.at(pathInfo.sidebarName).columnWidgets.size() < column + 1) {
            sidebar.at(pathInfo.sidebarName).columnWidgets.push_back({});
        }
    }
    SidebarEntry& entry = sidebar.at(pathInfo.sidebarName);
    entry.columnWidgets.at(column).push_back({ .name = widgetName, .type = widgetType });
    WidgetInfo& widget = entry.columnWidgets.at(column).back();
    switch (widgetType) {
        case WIDGET_CHECKBOX:
        case WIDGET_CVAR_CHECKBOX:
            widget.options = std::make_shared<CheckboxOptions>();
            break;
        case WIDGET_SLIDER_FLOAT:
        case WIDGET_CVAR_SLIDER_FLOAT:
            widget.options = std::make_shared<FloatSliderOptions>();
            break;
        case WIDGET_SLIDER_INT:
        case WIDGET_CVAR_SLIDER_INT:
            widget.options = std::make_shared<IntSliderOptions>();
            break;
        case WIDGET_COMBOBOX:
        case WIDGET_CVAR_COMBOBOX:
        case WIDGET_AUDIO_BACKEND:
        case WIDGET_VIDEO_BACKEND:
            widget.options = std::make_shared<ComboboxOptions>();
            break;
        case WIDGET_BUTTON:
            widget.options = std::make_shared<ButtonOptions>();
            break;
        case WIDGET_WINDOW_BUTTON:
            widget.options = std::make_shared<WindowButtonOptions>();
            break;
        case WIDGET_CVAR_COLOR_PICKER:
        case WIDGET_COLOR_PICKER:
            widget.options = std::make_shared<ColorPickerOptions>();
            break;
        case WIDGET_SEPARATOR_TEXT:
        case WIDGET_TEXT:
            widget.options = std::make_shared<TextOptions>();
            break;
        case WIDGET_SEARCH:
        case WIDGET_SEPARATOR:
        default:
            widget.options = std::make_shared<WidgetOptions>();
    }
    return widget;
}

SohMenu::SohMenu(const std::string& consoleVariable, const std::string& name)
    : Menu(consoleVariable, name, 0, UIWidgets::Colors::LightBlue) {
}

void SohMenu::AddMenuElements() {
    AddMenuSettings();
    AddMenuEnhancements();
    AddMenuRandomizer();
    // Tier 4 (#497 step 6). Placed where ADR 0004 section 4's table puts it:
    // after Randomizer, before Network.
    AddMenuCombo();
    AddMenuNetwork();
    AddMenuDevTools();

    if (CVarGetInteger(CVAR_SETTING("Menu.SidebarSearch"), 0)) {
        InsertSidebarSearch();
    }

    for (auto& initFunc : MenuInit::GetInitFuncs()) {
        initFunc();
    }

    // ADR 0004 section 4.2 (#497 step 5), LAST: every section and every
    // MenuInit registrar has contributed by now, and the marker is derived from
    // RSBS::kSharedIntentKeys rather than hand-tagged per row, so a
    // shared-intent row added after this lane still gets marked.
    ApplySharedIntentMarkers();

    mMenuElementsInitialized = true;
}

void SohMenu::InitElement() {
    Ship::Menu::InitElement();

    disabledMap = {
        { DISABLE_FOR_NO_VSYNC,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetInstance()->GetWindow()->CanDisableVerticalSync();
           },
            "Disabling VSync not supported" } },
        { DISABLE_FOR_NO_WINDOWED_FULLSCREEN,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetInstance()->GetWindow()->SupportsWindowedFullscreen();
           },
            "Windowed Fullscreen not supported" } },
        { DISABLE_FOR_NO_MULTI_VIEWPORT,
          { [](disabledInfo& info) -> bool {
               return !Ship::Context::GetInstance()->GetWindow()->GetGui()->SupportsViewports();
           },
            "Multi-viewports not supported" } },
        { DISABLE_FOR_NOT_DIRECTX,
          { [](disabledInfo& info) -> bool {
               return Ship::Context::GetInstance()->GetWindow()->GetWindowBackend() !=
                      Ship::WindowBackend::FAST3D_DXGI_DX11;
           },
            "Available Only on DirectX" } },
        { DISABLE_FOR_DIRECTX,
          { [](disabledInfo& info) -> bool {
               return Ship::Context::GetInstance()->GetWindow()->GetWindowBackend() ==
                      Ship::WindowBackend::FAST3D_DXGI_DX11;
           },
            "Not Available on DirectX" } },
        { DISABLE_FOR_MATCH_REFRESH_RATE_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_SETTING("MatchRefreshRate"), 0); },
            "Match Refresh Rate is Enabled" } },
        { DISABLE_FOR_ADVANCED_RESOLUTION_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".Enabled", 0); },
            "Advanced Resolution Enabled" } },
        { DISABLE_FOR_VERTICAL_RES_TOGGLE_ON,
          { [](disabledInfo& info) -> bool {
               return CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".VerticalResolutionToggle", 0);
           },
            "Vertical Resolution Toggle Enabled" } },
        { DISABLE_FOR_LOW_RES_MODE_ON,
          { [](disabledInfo& info) -> bool { return CVarGetInteger(CVAR_LOW_RES_MODE, 0); }, "N64 Mode Enabled" } },
        { DISABLE_FOR_NULL_PLAY_STATE,
          { [](disabledInfo& info) -> bool { return OoT_gPlayState == NULL; }, "Save Not Loaded" } },
        { DISABLE_FOR_DEBUG_MODE_OFF,
          { [](disabledInfo& info) -> bool { return !CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0); },
            "Debug Mode is Disabled" } },
        { DISABLE_FOR_FRAME_ADVANCE_OFF,
          { [](disabledInfo& info) -> bool { return !(OoT_gPlayState != nullptr && OoT_gPlayState->frameAdvCtx.enabled); },
            "Frame Advance is Disabled" } },
        { DISABLE_FOR_ADVANCED_RESOLUTION_OFF,
          { [](disabledInfo& info) -> bool { return !CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".Enabled", 0); },
            "Advanced Resolution is Disabled" } },
        { DISABLE_FOR_VERTICAL_RESOLUTION_OFF,
          { [](disabledInfo& info) -> bool {
               return !CVarGetInteger(CVAR_PREFIX_ADVANCED_RESOLUTION ".VerticalResolutionToggle", 0);
           },
            "Vertical Resolution Toggle is Off" } },
    };
}

void SohMenu::UpdateElement() {
    Ship::Menu::UpdateElement();
}

void SohMenu::Draw() {
    Ship::Menu::Draw();
}

void SohMenu::DrawElement() {
    if (mMenuElementsInitialized) {
        Ship::Menu::DrawElement();
    }
}

// ============================================================================
// ADR 0004 section 5 capability gating (#497 step 3)
// ============================================================================
// See SohMenu.h for why this is a sibling registry rather than more disabledMap
// rows, why it is process-wide, and why every predicate is an OBSERVED fact.

namespace {

/**
 * The built-in capabilities' reasons. String literals, because
 * `UIWidgets::WidgetOptions::disabledTooltip` is a `const char*` the draw path
 * reads after the PreFunc returns - a std::string temporary would dangle.
 *
 * Each one names the issue that tracks the absence. That is not decoration: the
 * two stale-reason incidents this mechanism exists to prevent (#438's remainder,
 * then #669) were both reasons nobody could trace back to a tracker, and the
 * gating lock refuses a reason with no `#NNN` in it for exactly that reason.
 */
constexpr const char* kCapReasonMMHosted =
    "Not yet available: Majora's Mask support is not in this build - MM's item pool never registered (#392)";
constexpr const char* kCapReasonComboHosted =
    "Not yet available: the cross-game settings store is not reachable in this process (#498)";
constexpr const char* kCapReasonComboPaired =
    "Not yet available: no paired Majora's Mask world yet - generate a randomized Ocarina of Time seed first (#492)";
constexpr const char* kCapReasonSingleExe =
    "Only available in the combined RedShipBlueShip executable (#392)";

/**
 * MM's half, observed rather than named (#640's rule). MM's
 * 2s2h/Rando/ForeignItemsSingleExe.cpp publishes its static pool from a
 * file-scope initializer, so a non-empty MM pool means that TU linked AND its
 * initializer ran - ADR 0004 section 5 parts 1 and 2 for the WHOLE_ARCHIVE'd
 * `2ship_rando`. Taking the address of anything in that TU instead would BE the
 * explicit reference that keeps it in the link, and the gate would pass
 * vacuously.
 */
bool MMHostedAbsent(disabledInfo& info) {
    const ComboForeignItemDef* pool = nullptr;
    info.value = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &pool);
    return info.value <= 0;
}

bool ComboHostedAbsent(disabledInfo& info) {
    (void)info;
    return !Combo_ComboSettingStoreAvailable();
}

bool ComboPairedAbsent(disabledInfo& info) {
    (void)info;
    ComboSettingsSummary summary;
    Combo_ComboSettingsSummary(&summary);
    return !summary.paired;
}

bool SingleExeAbsent(disabledInfo& info) {
    (void)info;
#ifdef RSBS_SINGLE_EXECUTABLE
    return false;
#else
    return true;
#endif
}

/** Does @p text carry a `#NNN` issue reference? */
bool NamesAnIssue(const char* text) {
    if (text == nullptr) {
        return false;
    }
    for (const char* p = text; *p != '\0'; p++) {
        if (*p == '#' && p[1] >= '0' && p[1] <= '9') {
            return true;
        }
    }
    return false;
}

} // namespace

std::unordered_map<uint32_t, disabledInfo>& SohMenu::GetCapabilityMap() {
    // A function-local static, not a member: a contributing TU's file-scope
    // registrar runs before any SohMenu exists, and a row's PreFunc has to
    // resolve its capability in a harness that never reaches InitElement.
    static std::unordered_map<uint32_t, disabledInfo> capabilityMap;
    static bool builtinsInstalled = false;
    if (!builtinsInstalled) {
        // Assigned directly rather than through RegisterCapability, which would
        // re-enter this accessor while the flag is still false.
        builtinsInstalled = true;
        capabilityMap[(uint32_t)SOH_MENU_CAP_MM_HOSTED] = { MMHostedAbsent, kCapReasonMMHosted };
        capabilityMap[(uint32_t)SOH_MENU_CAP_COMBO_HOSTED] = { ComboHostedAbsent, kCapReasonComboHosted };
        capabilityMap[(uint32_t)SOH_MENU_CAP_COMBO_PAIRED] = { ComboPairedAbsent, kCapReasonComboPaired };
        capabilityMap[(uint32_t)SOH_MENU_CAP_SINGLE_EXE] = { SingleExeAbsent, kCapReasonSingleExe };
    }
    return capabilityMap;
}

void SohMenu::RegisterCapability(uint32_t key, DisableInfoFunc absentWhen, const char* reason) {
    if (absentWhen == nullptr || reason == nullptr || reason[0] == '\0') {
        SPDLOG_ERROR("SohMenu::RegisterCapability({}): refused - a capability needs both an observed predicate and a "
                     "reason a player can read",
                     key);
        return;
    }
    if (!NamesAnIssue(reason)) {
        // Logged, not refused: a build that silently dropped the capability
        // would draw the row ENABLED, which is the one outcome section 5 forbids.
        // The gating lock is what turns this into a red test.
        SPDLOG_WARN("SohMenu::RegisterCapability({}): reason names no issue (#NNN) - \"{}\". A reason nobody can "
                    "trace is a reason nobody retires.",
                    key, reason);
    }
    GetCapabilityMap()[key] = { absentWhen, reason };
}

bool SohMenu::CapabilityPresent(uint32_t key) {
    auto& map = GetCapabilityMap();
    auto it = map.find(key);
    if (it == map.end()) {
        // A row asked for something nothing publishes. Greying it is the honest
        // answer; letting it look live is the vacuous-gate class in UI form.
        return false;
    }
    // `active` means "this reason applies", matching disabledMap's own
    // convention (DISABLE_FOR_NO_VSYNC evaluates !CanDisableVerticalSync()), so
    // the predicate answers ABSENT and presence is its negation. One meaning of
    // `active` across both maps is worth the inversion here.
    it->second.active = it->second.evaluation(it->second);
    return !it->second.active;
}

const char* SohMenu::CapabilityReason(uint32_t key) {
    auto& map = GetCapabilityMap();
    auto it = map.find(key);
    if (it == map.end() || it->second.reason == nullptr) {
        return "";
    }
    return it->second.reason;
}

void SohMenu::ApplyCapabilityGate(WidgetInfo& info, uint32_t key) {
    if (CapabilityPresent(key)) {
        return;
    }
    const char* reason = CapabilityReason(key);
    if (reason[0] == '\0') {
        // An unregistered key. Say so rather than greying the row with an empty
        // tooltip, which reads as a bug in the menu instead of a missing
        // capability.
        reason = "Not yet available: this control declares a capability nothing in this build publishes (#497)";
    }
    ApplyPresentation(info, info.name, SOH_MENU_PRESENT_CAPABILITY, reason);
}

WidgetFunc SohMenu::CapabilityGate(uint32_t key) {
    return [key](WidgetInfo& info) { ApplyCapabilityGate(info, key); };
}

WidgetFunc SohMenu::CapabilityGate(uint32_t key, WidgetFunc chained) {
    return [key, chained](WidgetInfo& info) {
        if (chained != nullptr) {
            chained(info);
        }
        if (info.isHidden) {
            // A hidden row is not drawn at all, so gating it would only spend
            // the predicate. MenuDrawItem returns on isHidden too.
            return;
        }
        ApplyCapabilityGate(info, key);
    };
}

// ============================================================================
// ADR 0004 section 4.2's marker and section 6's four presentation states
// (#497 step 5)
// ============================================================================

const char* SohMenu::SharedIntentMarker() {
    // One definition of the badge, owned by src/common because the tier-4 rows
    // already render it and because a requirement with two spellings is a
    // requirement half the menu will fail.
    return Combo_ComboSettingSharedMarker();
}

bool SohMenu::IsSharedIntentKey(const char* cVar) {
    if (cVar == nullptr || cVar[0] == '\0') {
        return false;
    }
    const std::string key(cVar);
    for (std::size_t i = 0; i < RSBS::kSharedIntentKeyCount; i++) {
        const std::string entry(RSBS::kSharedIntentKeys[i]);
        if (entry.empty()) {
            continue;
        }
        // An entry ending in '.' is a shared macro PREFIX, not a key:
        // CVAR_INPUT_VIEWER(var) is defined identically in both trees, so its
        // ~60 keys collide by construction and the manifest carries them as one
        // row (see cvar_shared_keys.h).
        if (entry.back() == '.') {
            if (key.rfind(entry, 0) == 0) {
                return true;
            }
            continue;
        }
        if (key == entry) {
            return true;
        }
    }
    return false;
}

std::string SohMenu::MarkRowName(const std::string& base, const char* cVar) {
    if (!IsSharedIntentKey(cVar)) {
        return base;
    }
    const std::string marker(SharedIntentMarker());
    if (marker.empty() || base.rfind(marker, 0) == 0) {
        return base; // idempotent
    }
    return marker + " " + base;
}

int SohMenu::ApplySharedIntentMarkers() {
    // WHY A PASS AND NOT A CALL PER ROW. Section 4.2's requirement is on every
    // tier-2 entry, and the key set is already checked in
    // (RSBS::kSharedIntentKeys), so deriving the marker from the manifest is what
    // makes it impossible for a NEW shared-intent row to ship unmarked - the
    // alternative, a .Marked() call each author must remember, is the shape of
    // requirement that ends up satisfied on the rows someone thought about.
    //
    // WHY HERE AND NOT IN AddWidget. The cVar arrives AFTER the widget does:
    // AddWidget returns a reference and the caller chains .CVar(...) onto it, so
    // at AddWidget time there is nothing to match against the manifest. The pass
    // therefore runs at the end of AddMenuElements(), once every section and
    // every MenuInit registrar has contributed.
    //
    // WHAT IT DOES NOT REACH: Menu.cpp's `extraSearchWidgets`, whose WidgetInfos
    // live in their callers rather than in a section. Those are search-only
    // aliases of rows registered elsewhere today, so they inherit the marker
    // through the reference; a future standalone one would not, and that is
    // recorded here rather than silently true.
    int marked = 0;
    for (auto& [sectionName, section] : menuEntries) {
        (void)sectionName;
        for (auto& [sidebarName, sidebar] : section.sidebars) {
            (void)sidebarName;
            for (auto& column : sidebar.columnWidgets) {
                for (WidgetInfo& widget : column) {
                    const std::string marked_name = MarkRowName(widget.name, widget.cVar);
                    if (marked_name != widget.name) {
                        widget.name = marked_name;
                        marked++;
                    }
                }
            }
        }
    }
    return marked;
}

const char* SohMenu::PresentationLabel(SohMenuPresentation state) {
    switch (state) {
        case SOH_MENU_PRESENT_LIVE:
            return ""; // never labelled: a live row that explains itself reads as broken
        case SOH_MENU_PRESENT_INACTIVE_GAME:
            return "not active now";
        case SOH_MENU_PRESENT_CAPABILITY:
            return "not yet available";
        case SOH_MENU_PRESENT_FROZEN:
            return "already decided";
        default:
            return "";
    }
}

void SohMenu::ApplyPresentation(WidgetInfo& info, const std::string& baseName, SohMenuPresentation state,
                                const char* reason) {
    const char* label = PresentationLabel(state);
    const bool haveReason = (reason != nullptr && reason[0] != '\0');

    if (state == SOH_MENU_PRESENT_LIVE) {
        if (haveReason) {
            // Section 6: a live entry carries no gate reason. A control that
            // works and explains why it does not is the same lie as a control
            // that does not work and says nothing.
            SPDLOG_WARN("SohMenu::ApplyPresentation(\"{}\"): LIVE with a reason (\"{}\") - reason dropped",
                        baseName, reason);
        }
        info.name = baseName;
        if (info.options != nullptr) {
            info.options->disabled = false;
            info.options->disabledTooltip = "";
        }
        return;
    }

    const char* detail = haveReason ? reason : label;

    // The two rules section 6 states in prose, enforced here so a caller cannot
    // swap them by accident: "a capability gate says NOT YET AVAILABLE, a freeze
    // says ALREADY DECIDED, and a player who reads the wrong one goes looking
    // for a bug in the right one."
    if (state == SOH_MENU_PRESENT_CAPABILITY && haveReason &&
        std::string(reason).find(PresentationLabel(SOH_MENU_PRESENT_FROZEN)) != std::string::npos) {
        SPDLOG_WARN("SohMenu::ApplyPresentation(\"{}\"): a CAPABILITY gate may not borrow the freeze reason "
                    "(\"{}\") - substituting the capability label",
                    baseName, reason);
        detail = label;
    }
    if (state == SOH_MENU_PRESENT_FROZEN && haveReason) {
        for (auto& [key, capability] : GetCapabilityMap()) {
            (void)key;
            if (capability.reason != nullptr && std::string(reason) == capability.reason) {
                SPDLOG_WARN("SohMenu::ApplyPresentation(\"{}\"): a FROZEN row may not carry a capability reason "
                            "(\"{}\") - substituting the freeze label",
                            baseName, reason);
                detail = label;
                break;
            }
        }
    }

    // The state goes in the row NAME as well as the tooltip. Section 4.2's
    // "legible without hovering" is the reason, and there is a mechanical one
    // too: MenuDrawItem's race-lockout branch overwrites disabledTooltip
    // outright, so a state that lived only there would vanish under a race
    // lockout.
    std::string suffix = std::string(label);
    if (haveReason && std::string(detail) != std::string(label)) {
        suffix += ": ";
        suffix += detail;
    }
    info.name = suffix.empty() ? baseName : (baseName + " - " + suffix);

    if (info.options == nullptr) {
        return;
    }
    if (state == SOH_MENU_PRESENT_INACTIVE_GAME) {
        // Section 6 point 2: readable AND EDITABLE. Collapsing this into
        // "disabled" would wrongly imply the setting is broken; the label is what
        // denies "this takes effect now".
        info.options->disabled = false;
        return;
    }
    info.options->disabled = true;
    info.options->disabledTooltip = (detail != nullptr) ? detail : label;
}

} // namespace SohGui
