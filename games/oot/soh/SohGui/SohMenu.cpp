#include "SohMenu.h"
#include <ship/window/gui/GuiMenuBar.h>
#include <ship/window/gui/GuiElement.h>
#include <ship/utils/StringHelper.h>
#include <spdlog/fmt/fmt.h>

#include <cctype>
#include <string>
#include <unordered_map>

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

/** A built-in capability's player text and the issue that tracks its absence. */
struct CapabilityReasonText {
    const char* text;
    uint32_t issue;
};

/**
 * The built-in capabilities' reasons, split into what a player reads and what
 * the record keeps (ADR 0004's 2026-09-27 amendment: "follow the SoH idiom").
 * The text is in SoH's disabled-reason style - a short fragment, mostly Title
 * Case, like disabledMap's "Save Not Loaded" or "Available Only on DirectX" (a
 * few of SoH's are sentence case: "Disabling VSync not supported") - and carries
 * no issue number, because no SoH disabled reason does. The issue is recorded
 * beside it, where the gating lock reads it: the two stale-reason incidents this
 * mechanism exists to prevent (#438's remainder, then #669) were reasons nobody
 * could trace back to a tracker, and that requirement moved from the visible
 * string to the record, it did not go away. String literals, because the text
 * reaches a `const char*` the draw path reads after the PreFunc returns.
 */
constexpr CapabilityReasonText kCapReasonMMHosted = { "Majora's Mask Not in This Build", 392 };
constexpr CapabilityReasonText kCapReasonComboHosted = { "Cross-Game Settings Unavailable", 498 };
constexpr CapabilityReasonText kCapReasonComboPaired = { "No Paired World Yet", 492 };
constexpr CapabilityReasonText kCapReasonSingleExe = { "Available Only in RedShipBlueShip", 392 };
/** A row that asks for a capability nothing in this build publishes (#497). */
constexpr CapabilityReasonText kCapReasonUnregistered = { "Not Available in This Build", 497 };

/** SoH's disabled-tooltip opening, exactly as MenuDrawItem writes it (Menu.cpp). */
constexpr const char* kDisabledTooltipHead = "This setting is disabled because: \n";

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

/** Does @p text carry a `#NNN` issue reference? Player text must not: the
 *  number belongs to the record (SohMenuCapabilityRecord::issue). */
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

/** Case-insensitive containment, for the two borrowed-wording rules: the
 *  fragments are Title Case now and a caller's text may not be. */
bool ContainsNoCase(const char* haystack, const char* needle) {
    if (haystack == nullptr || needle == nullptr || needle[0] == '\0') {
        return false;
    }
    std::string h(haystack);
    std::string n(needle);
    for (char& ch : h) {
        ch = (char)std::tolower((unsigned char)ch);
    }
    for (char& ch : n) {
        ch = (char)std::tolower((unsigned char)ch);
    }
    return h.find(n) != std::string::npos;
}

SohMenuCapabilityRecord MakeRecord(DisableInfoFunc absentWhen, const CapabilityReasonText& reason) {
    SohMenuCapabilityRecord record;
    record.gate = { absentWhen, reason.text };
    record.issue = reason.issue;
    return record;
}

} // namespace

std::unordered_map<uint32_t, SohMenuCapabilityRecord>& SohMenu::GetCapabilityMap() {
    // A function-local static, not a member: a contributing TU's file-scope
    // registrar runs before any SohMenu exists, and a row's PreFunc has to
    // resolve its capability in a harness that never reaches InitElement.
    static std::unordered_map<uint32_t, SohMenuCapabilityRecord> capabilityMap;
    static bool builtinsInstalled = false;
    if (!builtinsInstalled) {
        // Assigned directly rather than through RegisterCapability, which would
        // re-enter this accessor while the flag is still false.
        builtinsInstalled = true;
        capabilityMap[(uint32_t)SOH_MENU_CAP_MM_HOSTED] = MakeRecord(MMHostedAbsent, kCapReasonMMHosted);
        capabilityMap[(uint32_t)SOH_MENU_CAP_COMBO_HOSTED] = MakeRecord(ComboHostedAbsent, kCapReasonComboHosted);
        capabilityMap[(uint32_t)SOH_MENU_CAP_COMBO_PAIRED] = MakeRecord(ComboPairedAbsent, kCapReasonComboPaired);
        capabilityMap[(uint32_t)SOH_MENU_CAP_SINGLE_EXE] = MakeRecord(SingleExeAbsent, kCapReasonSingleExe);
    }
    return capabilityMap;
}

void SohMenu::RegisterCapability(uint32_t key, DisableInfoFunc absentWhen, const char* reason, uint32_t issue) {
    if (absentWhen == nullptr || reason == nullptr || reason[0] == '\0') {
        SPDLOG_ERROR("SohMenu::RegisterCapability({}): refused - a capability needs both an observed predicate and a "
                     "reason a player can read",
                     key);
        return;
    }
    if (issue == 0) {
        // Logged, not refused: an unregistered key reads ABSENT, so dropping
        // this one would grey its rows with the generic fallback instead of the
        // reason the caller wrote. The gating lock is what turns this into a red
        // test (CapabilityTraceable).
        SPDLOG_WARN("SohMenu::RegisterCapability({}): no tracking issue recorded for \"{}\". A reason nobody can "
                    "trace is a reason nobody retires.",
                    key, reason);
    }
    if (NamesAnIssue(reason)) {
        SPDLOG_WARN("SohMenu::RegisterCapability({}): the player text \"{}\" carries an issue number; it belongs in "
                    "the record, not in the tooltip",
                    key, reason);
    }
    SohMenuCapabilityRecord record;
    record.gate = { absentWhen, reason };
    record.issue = issue;
    GetCapabilityMap()[key] = record;
}

bool SohMenu::UnregisterCapability(uint32_t key) {
    if (key < (uint32_t)SOH_MENU_CAP_BUILTIN_COUNT) {
        // A built-in is installed once by GetCapabilityMap()'s own bootstrap and
        // never re-installed, so withdrawing one would silently grey every row
        // that asks for it for the rest of the process.
        SPDLOG_ERROR("SohMenu::UnregisterCapability({}): refused - a built-in capability cannot be withdrawn", key);
        return false;
    }
    return GetCapabilityMap().erase(key) != 0;
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
    disabledInfo& gate = it->second.gate;
    gate.active = gate.evaluation(gate);
    return !gate.active;
}

const char* SohMenu::CapabilityReason(uint32_t key) {
    auto& map = GetCapabilityMap();
    auto it = map.find(key);
    if (it == map.end() || it->second.gate.reason == nullptr) {
        return "";
    }
    return it->second.gate.reason;
}

uint32_t SohMenu::CapabilityIssue(uint32_t key) {
    auto& map = GetCapabilityMap();
    auto it = map.find(key);
    return it == map.end() ? 0u : it->second.issue;
}

bool SohMenu::CapabilityTraceable(uint32_t key) {
    const char* text = CapabilityReason(key);
    return text[0] != '\0' && CapabilityIssue(key) != 0 && !NamesAnIssue(text);
}

void SohMenu::ApplyCapabilityGate(WidgetInfo& info, uint32_t key) {
    if (CapabilityPresent(key)) {
        // Nothing to do. MenuDrawItem's ResetDisables() already cleared
        // `disabled` before this PreFunc ran, and in the chained form another
        // PreFunc may legitimately have set it again for a reason that is not
        // this gate's - so calling ApplyPresentation(LIVE) here would un-disable
        // a row somebody else disabled. The name is the row's own in every state.
        return;
    }
    const char* reason = CapabilityReason(key);
    if (reason[0] == '\0') {
        // An unregistered key. Say so rather than greying the row with an empty
        // tooltip, which reads as a bug in the menu instead of a missing
        // capability.
        reason = kCapReasonUnregistered.text;
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

WidgetFunc SohMenu::CapabilityNote(uint32_t key, const char* sentence) {
    return [key, sentence](WidgetInfo& note) {
        if (CapabilityPresent(key)) {
            ApplyPresentationNote(note, SOH_MENU_PRESENT_LIVE);
            return;
        }
        // A sentence-case sentence, like SoH's own gray notes and this group's
        // siblings ("Already decided when this world was created."): the
        // caller's, or the state's canonical one. The capability's Title Case
        // reason fragment stays in the rows' disabled tooltip, where SoH puts a
        // reason; pasting a fragment into a sentence reads as neither.
        ApplyPresentationNote(note, SOH_MENU_PRESENT_CAPABILITY, sentence);
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
    // SoH's disabled-reason style: short fragments, mostly Title Case ("Save
    // Not Loaded", Menu.cpp's "Race Lockout Active").
    switch (state) {
        case SOH_MENU_PRESENT_LIVE:
            return ""; // never explained: a live row that explains itself reads as broken
        case SOH_MENU_PRESENT_INACTIVE_GAME:
            return "Not Active Now";
        case SOH_MENU_PRESENT_CAPABILITY:
            return "Not Yet Available";
        case SOH_MENU_PRESENT_FROZEN:
            return "Already Decided";
        default:
            return "";
    }
}

const char* SohMenu::PresentationNoteText(SohMenuPresentation state) {
    switch (state) {
        case SOH_MENU_PRESENT_INACTIVE_GAME:
            // MAJORA'S-MASK-SPECIFIC (the operator's wording): every
            // INACTIVE_GAME group today is an MM group shown while OoT plays. A
            // group whose suspended game is Ocarina of Time must pass its own
            // sentence to ApplyPresentationNote; this default would be false there.
            return "Majora's Mask is suspended; these take effect when you return.";
        case SOH_MENU_PRESENT_CAPABILITY:
            return "These settings are not available yet.";
        case SOH_MENU_PRESENT_FROZEN:
            return "Already decided when this world was created.";
        case SOH_MENU_PRESENT_LIVE:
        default:
            return "";
    }
}

const char* SohMenu::DisabledTooltip(const char* reason) {
    // Node-based storage: a stored string's c_str() survives rehashing, and one
    // copy per distinct reason bounds it by the reasons the menu can show.
    static std::unordered_map<std::string, std::string> tooltips;
    const std::string key = (reason != nullptr) ? reason : "";
    auto it = tooltips.find(key);
    if (it == tooltips.end()) {
        it = tooltips.emplace(key, std::string(kDisabledTooltipHead) + "\n- " + key).first;
    }
    return it->second.c_str();
}

void SohMenu::ApplyPresentationNote(WidgetInfo& note, SohMenuPresentation state, const char* sentence) {
    if (state == SOH_MENU_PRESENT_LIVE) {
        // A live group says nothing about itself.
        note.isHidden = true;
        return;
    }
    const char* text = (sentence != nullptr && sentence[0] != '\0') ? sentence : PresentationNoteText(state);
    if (note.name != text) {
        note.name = text;
    }
}

std::string SohMenu::StripPresentationSuffix(const std::string& name) {
    // Loop, because a name that accumulated several suffixes (the defect the old
    // name-writing presentation had to guard against) normalises in one call.
    // Both spellings: the lower-case labels the old composition wrote, and the
    // current Title Case fragments.
    std::string out = name;
    for (bool cut = true; cut;) {
        cut = false;
        for (int s = 0; s < (int)SOH_MENU_PRESENT_COUNT && !cut; s++) {
            const std::string title = PresentationLabel((SohMenuPresentation)s);
            if (title.empty()) {
                continue; // LIVE is never labelled, so nothing to cut
            }
            std::string lower = title;
            for (char& ch : lower) {
                ch = (char)std::tolower((unsigned char)ch);
            }
            for (const std::string& label : { title, lower }) {
                const std::string needle = " - " + label;
                const std::size_t at = out.rfind(needle);
                if (at == std::string::npos) {
                    continue;
                }
                // Only a SUFFIX, in exactly the shape the old composition wrote:
                // the label ends the string, or a ": <detail>" follows it. A
                // registered row that merely contains the words is not rewritten.
                const std::size_t after = at + needle.size();
                if (after == out.size() || out.compare(after, 2, ": ") == 0) {
                    out.erase(at);
                    cut = true;
                    break;
                }
            }
        }
    }
    return out;
}

void SohMenu::ApplyPresentation(WidgetInfo& info, const std::string& baseName, SohMenuPresentation state,
                                const char* reason) {
    const bool haveReason = (reason != nullptr && reason[0] != '\0');

    // The row's own name, in every state. SoH never writes a state or an
    // explanation into an interactive row's name: a button may relabel the
    // action it performs ("Enable##Sail" / "Disable##Sail", SohMenuNetwork.cpp),
    // and a TEXT row may carry a live value, but a reason goes in the tooltip
    // (and, here, the group's gray note). Compared
    // before it is assigned, because callers pass `info.name` itself and
    // baseName may alias the member this writes.
    const std::string base = StripPresentationSuffix(baseName);
    if (info.name != base) {
        info.name = base;
    }

    if (state == SOH_MENU_PRESENT_LIVE) {
        if (haveReason) {
            // Section 6: a live entry carries no gate reason. A control that
            // works and explains why it does not is the same lie as a control
            // that does not work and says nothing.
            SPDLOG_WARN("SohMenu::ApplyPresentation(\"{}\"): LIVE with a reason (\"{}\") - reason dropped", base,
                        reason);
        }
        if (info.options != nullptr) {
            info.options->disabled = false;
            info.options->disabledTooltip = "";
        }
        return;
    }

    if (state == SOH_MENU_PRESENT_INACTIVE_GAME) {
        // Section 6 point 2: readable AND EDITABLE. Collapsing this into
        // "disabled" would wrongly imply the setting is broken; the group's gray
        // note (ApplyPresentationNote) is what denies "this takes effect now".
        if (info.options != nullptr) {
            info.options->disabled = false;
        }
        return;
    }

    const char* label = PresentationLabel(state);
    const char* detail = haveReason ? reason : label;

    // The two rules section 6 states in prose, enforced here so a caller cannot
    // swap them by accident: "a capability gate says NOT YET AVAILABLE, a freeze
    // says ALREADY DECIDED, and a player who reads the wrong one goes looking
    // for a bug in the right one."
    if (state == SOH_MENU_PRESENT_CAPABILITY && haveReason &&
        ContainsNoCase(reason, PresentationLabel(SOH_MENU_PRESENT_FROZEN))) {
        SPDLOG_WARN("SohMenu::ApplyPresentation(\"{}\"): a CAPABILITY gate may not borrow the freeze reason "
                    "(\"{}\") - substituting the capability label",
                    base, reason);
        detail = label;
    }
    if (state == SOH_MENU_PRESENT_FROZEN && haveReason) {
        for (auto& [key, capability] : GetCapabilityMap()) {
            (void)key;
            if (capability.gate.reason != nullptr && std::string(reason) == capability.gate.reason) {
                SPDLOG_WARN("SohMenu::ApplyPresentation(\"{}\"): a FROZEN row may not carry a capability reason "
                            "(\"{}\") - substituting the freeze label",
                            base, reason);
                detail = label;
                break;
            }
        }
    }

    if (info.options == nullptr) {
        return;
    }
    // SoH's disabled row: greyed, with MenuDrawItem's own tooltip shape. Under a
    // race lockout MenuDrawItem rebuilds the tooltip from `activeDisables` and
    // appends "- Race Lockout Active": SoH's own disabledMap rows keep their
    // reason there, but a tooltip written directly, as this one must be (a
    // capability key is not a DisableOption), is REPLACED. That is a known
    // divergence from SoH under race lockout, and why the group's note, not this
    // tooltip, carries the state.
    info.options->disabled = true;
    info.options->disabledTooltip = DisabledTooltip(detail);
}

} // namespace SohGui
