/**
 * @file SohMenuCombo.cpp
 * @brief The tier-4 **Combo** section of the one live menu - ADR 0004 section 4's
 *        ninth top-level header and #497 step 6.
 *
 * WHAT MOVED HERE AND WHY. ADR 0004 section 4's layout table gives the combined
 * product one section of its own for the surface that has no upstream
 * counterpart: pairing status, `.redsave` slot management, entrance links,
 * hot-swap. Until this file that section did not exist, and the tier-4
 * `gCombo.Rando.*` rules lived in a page called `Randomizer -> Cross-Game`
 * (PR #509, then #655) that said so in-tree: *"This is the interim, NOT #497
 * step 3"*. The rules are combo-level, not randomizer-level - they govern the
 * crossing between two games rather than either game's seed - so the interim
 * host was always the wrong tier. They move verbatim; the model, the
 * creation-time resolver and the freeze gate are all still src/common's, and
 * nothing about how a row reaches them changed.
 *
 * THE OLD PAGE IS NOT DELETED. `Randomizer -> Cross-Game` survives with a
 * pointer row (AddCrossGamePointerWidgets, SohMenuRandomizer.cpp). That is not
 * politeness: sidebar selection persists BY DISPLAY-NAME STRING into
 * `gSettings.Menu.RandomizerSidebarSection`, which is the measured reason ADR
 * 0004's resolved call 1 refused to promote Cheats out of Enhancements. Deleting
 * the page would strand every config holding `"Cross-Game"` on a header with no
 * such sidebar.
 *
 * WHAT IS NOT BUILT HERE, named rather than implied. ADR 0004 section 4's table
 * lists four Combo sidebars - pairing status, save slots, entrance links,
 * hot-swap. Two pages ship here: `Cross-Game Rules` and `Windows`; save slots
 * are the contributed `Save Files` page (SohMenuComboSaveFiles.cpp, 2026-09-28,
 * which absorbed the never-constructed ComboMenuBar's `.redsave` panel; that
 * file is deleted). Pairing status, entrance links and hot-swap would be EMPTY
 * pages today, and an empty multi-column page is #640's failure mode exactly
 * (Menu::DrawElement's unconditional SetNextWindowPos goes unconsumed and
 * undocks libultraship's "Main Game" window), so they are left unregistered
 * until they have content.
 *
 * THE EXTENSION POINT. Other TUs contribute pages through
 * SohGui::RegisterComboSectionPage (declared in SohMenu.h, implemented below)
 * rather than by editing this file. See that declaration for the two things that
 * bite: a file-scope registrar in a plain archive is dropped by the linker, and
 * an empty page is #640.
 *
 * Locked ROM-free by MenuComboSection (the section, its pages, the pointer row
 * and the extension point) and by ComboSettingsRows (the six rules' own
 * behaviour, which followed them here).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common - the Cross-Game combo rules' model (#655, ADR 0011 increment 2).
// ADR 0008 rule 5's restatement for an OoT-hosted row: the row may read the
// model through these accessors, but never gComboCtx and never either game's
// gSaveContext.
#include "combo_settings_view.h"
#include "foreign_items.h"
// The windows the Windows page opens: their registered names and visibility
// CVars, from the constants the windows themselves are built with.
#include "ComboSpoilerWindow.h"
#include "ComboTrackerWindow.h"
#include "ComboItemTrackerWindow.h"
#include "cvar_shared_keys.h"

#include <cctype>  // toupper, for the disabled reason's Title Case
#include <cstdio>  // snprintf, for the combo-rule status line and fingerprint
#include <cstring> // strcmp, for the renamed sidebar's persisted selection
#include <functional>
#include <string>
#include <vector>

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// ============================================================================
// Cross-Game combo rules (#655; ADR 0011 increment 2, #498; #497 step 6)
// ============================================================================
// The three tier-4 `gCombo.Rando.*` keys — direction, the shared ocarina
// (#668) and the goal (ADR 0010 D1); the two pool-size rows were retired by
// #801 and the two item-class groups by #834 — render as ROWS in
// the Cross-Game Rules page of the tier-4 Combo section below. #497 step 6
// moved them there from the interim host, Randomizer → Cross-Game.
// PR #652 shipped them as a common-owned pop-out pane
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
static bool comboRuleSharedOcarina; // #668: ComboSettingsRecord.comboFlags' shared-ocarina bit
static int32_t comboRuleGoal;       // ADR 0010 D1: ComboSettingsRecord.goal (RSBS_COMBO_GOAL_*)

// The four pinned RSBS_COMBO_DIR_* enumerators (1..4, static_asserted in
// foreign_items.h because they are .redsave format). Short Title Case values,
// as SoH's comboboxes have; what each one does is the tooltip's "Value: effect"
// list (docs/ui-style-guide.md R-N6, SohMenuSettings.cpp's Boot Sequence).
static const std::map<int32_t, const char*> comboRuleDirectionOptions = {
    { (int32_t)RSBS_COMBO_DIR_OFF, "Off" },
    { (int32_t)RSBS_COMBO_DIR_FORWARD, "OoT Items to MM" },
    { (int32_t)RSBS_COMBO_DIR_REVERSE, "MM Items to OoT" },
    { (int32_t)RSBS_COMBO_DIR_BOTH, "Both Directions" },
};

// The five pinned RSBS_COMBO_GOAL_* enumerators (1..5, static_asserted in
// foreign_items.h), named in OoTMM's own words for its `goal` setting, whose
// values these are (packages/core/src/settings/data.ts lists them in the order
// any, ganon, majora, both, triforce, triforce3; triforce3 is not offered).
// The dropdown does NOT follow that order: ComboboxOptions' comboMap is a
// std::map keyed by the stored value, so it lists them in enumerator order --
// the default first, then the values in the order they were appended to the
// pinned table. Reordering would need a second, display-only numbering between
// the row and the record, which the frozen-state and unknown-value legs read
// directly.
static const std::map<int32_t, const char*> comboRuleGoalOptions = {
    { (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH, "Ganon & Majora" },
    { (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER, "Any Final Boss" },
    { (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT, "Triforce Hunt" },
    { (int32_t)RSBS_COMBO_GOAL_BEAT_OOT, "Ganon" },
    { (int32_t)RSBS_COMBO_GOAL_BEAT_MM, "Majora" },
};

// The disabled tooltip of every rule row once the world is frozen, rebuilt by
// ComboRuleDecidedTooltip. Storage for the const char* WidgetOptions holds.
static std::string comboRuleDecidedTooltip;

/**
 * The Reset confirm's registrar: SohGui::RegisterPopup. A pointer so the headless
 * lock (soh_combo_settings_rows_test.cpp) can record the popup and run its Reset
 * button. SoH's popup machinery only runs a button from an ImGui frame, so the
 * lock could not reach the action any other way. Externally linked and declared
 * in no header, like AddComboRulesWidgets. Production never reassigns it.
 */
void (*gComboRulePopupRegistrar)(std::string, std::string, std::string, std::string, std::function<void()>,
                                 std::function<void()>) = SohGui::RegisterPopup;

/** Queues a confirm popup through gComboRulePopupRegistrar. */
static void ComboRuleRegisterPopup(const char* title, const char* message, const char* confirm, const char* cancel,
                                   std::function<void()> onConfirm) {
    gComboRulePopupRegistrar(title, message, confirm, cancel, std::move(onConfirm), nullptr);
}

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
 * The CHECKBOX row's name: the same marker and label, with the marker on a line
 * of its own above the label ("[Both Games]" then "Shared Ocarina").
 *
 * WHY TWO LINES. A combobox draws its label ABOVE the box, so its name has the
 * whole column; a checkbox draws its label to the RIGHT of a ~44 px box
 * (docs/ui-style-guide.md section 2). At min-832x600, the width contract
 * (section 1), "[Both Games] Shared Ocarina" on one line overruns its column
 * and runtime lint R9 fails the page. The operator ruled the name (2026-10-01),
 * so the row is not shortened; the marker goes on a line of its own instead.
 *
 * The break is an AUTHORED newline, a project choice with no SoH precedent: no
 * SoH row authors a newline in its name. SoH's own wrap (Menu::MenuDrawItem
 * passes a checkbox name through UIWidgets::WrappedText at 90 / columns
 * characters, 45 on this two-column page) would leave this 27-character name
 * on one line, so the row is two lines at EVERY profile, desk-1280x800
 * included, not only where it would overrun. Only the shape matches SoH's
 * wrapped checkbox rows: the box at the top, the label beside it, right of
 * the box as the style guide requires. WrappedText keeps the authored newline
 * (it restarts its count there). The newline is also part of the search key
 * (Menu.cpp keeps it), so a search across the marker into the label does not
 * match this row; "shared ocarina" does.
 */
static std::string ComboRuleCheckboxRowName(ComboSettingId id) {
    return std::string(Combo_ComboSettingSharedMarker()) + "\n" + Combo_ComboSettingLabel(id);
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
 * The disabled tooltip, built from the MODEL's reason each time it is needed:
 * MenuDrawItem's own disabled shape ("This setting is disabled because: \n" then
 * "\n- Reason", Menu.cpp), with Combo_ComboSettingReadOnlyReason ("already
 * decided") put in SoH's Title Case reason style ("Race Lockout Active"). It is
 * written here rather than through a DisableOption because the reason is not
 * one of SoH's. Nothing is added to the model's words: the frozen-but-unpaired
 * (corrupt) state greys the same rows, and the gray note is where the states
 * differ.
 */
static const char* ComboRuleDecidedTooltip() {
    const char* reason = Combo_ComboSettingReadOnlyReason();
    std::string text = "This setting is disabled because: \n\n- ";
    bool wordStart = true;
    for (const char* c = (reason != nullptr) ? reason : ""; *c != '\0'; c++) {
        text += wordStart ? (char)std::toupper((unsigned char)*c) : *c;
        wordStart = (*c == ' ');
    }
    if (text != comboRuleDecidedTooltip) {
        comboRuleDecidedTooltip = text;
    }
    return comboRuleDecidedTooltip.c_str();
}

/**
 * The read-only half of state 4. The reason is the MODEL's
 * (Combo_ComboSettingReadOnlyReason, "already decided", via
 * ComboRuleDecidedTooltip), shown in the shape
 * MenuDrawItem gives every disabled SoH row, and is deliberately NOT a capability
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
    info.options->disabled = true;
    info.options->disabledTooltip = ComboRuleDecidedTooltip();
}

/**
 * Reset's action, run by the confirm popup's Reset button. Clears rather than
 * writing the defaults back: an unset key and a key explicitly holding the
 * default resolve identically today, but only the cleared one reads as "the
 * player never touched it".
 */
static void ComboRuleResetAll() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        Combo_ComboSettingClear((ComboSettingId)i);
    }
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
        snprintf(unknownLabel, sizeof(unknownLabel), "Unknown (%d)", (int)comboRuleDirection);
        options->comboMap[comboRuleDirection] = unknownLabel;
        comboRuleUnknownDirection = comboRuleDirection;
    }

    ComboRuleApplyDecided(info, decided);
}

// A goal value the goal row's combo map had to be taught about at runtime, or
// 0. The direction row's guard, for the same reason: std::map::at throws on an
// unknown key, and a frozen record from a later build (or a legacy record's 0,
// "unset") can carry a goal this build has no label for.
static int32_t comboRuleUnknownGoal = 0;
static bool comboRuleUnknownGoalTaught = false;

/**
 * The goal row's per-frame refresh: the direction row's, over the goal. The
 * value is shown as what it is ("Unknown (n)") rather than clamped to a label,
 * because a clamp would display a goal the world was not proved against.
 */
static void ComboRuleGoalPreFunc(WidgetInfo& info) {
    ComboSettingsRecord shown;
    const bool decided = ComboRuleShownRecord(&shown);
    comboRuleGoal = (int32_t)shown.goal;

    auto options = std::static_pointer_cast<ComboboxOptions>(info.options);
    if (comboRuleUnknownGoalTaught && comboRuleUnknownGoal != comboRuleGoal) {
        options->comboMap.erase(comboRuleUnknownGoal);
        comboRuleUnknownGoalTaught = false;
    }
    if (!options->comboMap.contains(comboRuleGoal)) {
        static char unknownLabel[32];
        snprintf(unknownLabel, sizeof(unknownLabel), "Unknown (%d)", (int)comboRuleGoal);
        options->comboMap[comboRuleGoal] = unknownLabel;
        comboRuleUnknownGoal = comboRuleGoal;
        comboRuleUnknownGoalTaught = true;
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
 *   - paired but not frozen: a legacy pre-carve pair (ADR 0011 decision 4.2).
 *     Its first crossing freezes the SHIPPED DEFAULTS
 *     (Combo_FreezeLegacyComboSettings), and the same crossing then compares
 *     that record against the live CVars (Combo_ComboSettingsDivergence). A
 *     rule edited before that first crossing is REFUSED there, so the note must
 *     tell the player to keep the defaults until they have crossed once. After
 *     that the record is frozen and the frozen branch applies;
 *   - unpaired: these freeze into the next paired world at generation.
 */
static void ComboRuleStatusPreFunc(WidgetInfo& info) {
    ComboSettingsSummary summary;
    Combo_ComboSettingsSummary(&summary);
    const char* reason = Combo_ComboSettingReadOnlyReason();

    char buffer[256];
    if (reason != nullptr && !summary.paired) {
        // A frozen record with no live pairing is a state no created combo file
        // may be in (ADR 0011 decision 4.2). Combo_ComboSettingsSummary
        // deliberately reports an ABSENT record for it rather than presenting
        // gComboCtx's zeros as rules, so the rows below show zeros: say so,
        // instead of letting a player read them as their world's rules.
        snprintf(buffer, sizeof(buffer),
                 "Session state is corrupt: these rules are frozen but no paired world is loaded, so the values "
                 "below are not in effect. Return to the title screen.");
    } else if (reason != nullptr) {
        snprintf(buffer, sizeof(buffer),
                 "Already decided when this world was created (fingerprint %08X). Return to the title screen to "
                 "choose rules for a new world.",
                 (unsigned)summary.comboSettingsHash);
    } else if (summary.paired) {
        snprintf(buffer, sizeof(buffer),
                 "Your paired world predates these rules and will use the defaults. Keep them at the defaults "
                 "until you have crossed into it once.");
    } else {
        // No paired file is loaded, so the rows author the NEXT world. Loading
        // an existing paired file puts that file's own rules back (#781), so an
        // edit here never reaches a world that already exists. Two lines at the
        // column's width, as Randomizer > General's note is; the freeze itself
        // is the frozen state's own sentence.
        snprintf(buffer, sizeof(buffer),
                 "These rules apply to the next paired world you create. Loading a paired file restores its own "
                 "rules.");
    }
    comboRuleStatusText = buffer;
    info.name = comboRuleStatusText;
}

// ============================================================================
// The contributed-page registry (#497 step 6's extension point)
// ============================================================================

namespace {

/** The contributed pages. A function-local static, like MenuInit's registries:
 *  a contributor's file-scope initializer runs before any SohMenu exists. */
std::vector<ComboSectionPage>& MutableComboSectionPages() {
    static std::vector<ComboSectionPage> pages;
    return pages;
}

} // namespace

void RegisterComboSectionPage(const char* sidebarName, uint32_t columnCount, ComboPageRegistrar registrar) {
    if (sidebarName == nullptr || sidebarName[0] == '\0' || registrar == nullptr || columnCount == 0) {
        SPDLOG_ERROR("RegisterComboSectionPage: refused - a page needs a name, a registrar and at least one column");
        return;
    }
    auto& pages = MutableComboSectionPages();
    for (ComboSectionPage& page : pages) {
        if (page.sidebarName == sidebarName) {
            // Replace and complain. Two registrars claiming one page is the
            // ambiguity this registry would otherwise hide, and the losing one
            // would simply never draw.
            SPDLOG_WARN("RegisterComboSectionPage(\"{}\"): a page under that name was already registered - replacing",
                        sidebarName);
            page.columnCount = columnCount;
            page.registrar = registrar;
            return;
        }
    }
    pages.push_back(ComboSectionPage{ sidebarName, columnCount, registrar });
}

bool UnregisterComboSectionPage(const char* sidebarName) {
    if (sidebarName == nullptr) {
        return false;
    }
    auto& pages = MutableComboSectionPages();
    for (std::size_t i = 0; i < pages.size(); i++) {
        if (pages.at(i).sidebarName == sidebarName) {
            pages.erase(pages.begin() + (long)i);
            return true;
        }
    }
    return false;
}

const std::vector<ComboSectionPage>& GetComboSectionPages() {
    return MutableComboSectionPages();
}

// ============================================================================
// The two shipped pages
// ============================================================================

/**
 * The tier-4 combo rules (#655, #668), moved from `Randomizer -> Cross-Game` by
 * #497 step 6. Externally linked and declared in no header for the same reason
 * the interim registrar was: the rules' headless lock
 * (games/oot/soh/soh_combo_settings_rows_test.cpp) has to register these rows in
 * order to assert anything about them, and it declares this symbol itself the way
 * the SohGui TUs already declare SohGui::mSohMenu, so no production header grows
 * a test-only entry point.
 *
 * The caller owns the sidebar: AddMenuCombo() creates it with two columns, and
 * this function pins `path.column` before each column's first row. That is a change from the interim registrar,
 * which had to pin the column itself because five `Rando::Settings` option groups
 * ran immediately before it and left `path.column` wherever their last COLUMN
 * container landed (option.cpp:469-472). Nothing runs before this page in the
 * Combo section, but AddMenuCombo pins it anyway - the cost of being wrong is a
 * row that is registered and never drawn (#640's failure mode with the halves
 * swapped), which no compile catches.
 */
void AddComboRulesWidgets(SohMenu& menu, WidgetPath& path) {
    // Three settings, rendered as rows rather than as the pop-out pane PR #652
    // shipped. See the block comment at the top of this file for why every row
    // is a pointer-based widget over a src/common writer rather than a
    // WIDGET_CVAR_* one, and for which value each row shows in which state.
    //
    // The layout is SoH's Randomizer > General (SohMenuRandomizer.cpp), the page
    // docs/ui-style-guide.md section 12 compares this one with: two columns, the
    // first opened by one gray note, then SeparatorText groups, and the primary
    // button at 250 px. Column 1 holds every rule. Column 2 held the two
    // item-class sets until #834 retired them; it is left empty rather than the
    // page dropping to one column, so the rows keep the two-column measure the
    // reference page is compared at.

    // ---- Column 1: the state note, the crossing, Reset ---------------------
    path.column = SECTION_COLUMN_1;
    // The state note. Its name is rewritten every frame by its PreFunc, so it is
    // kept out of the menu search rather than seeding it with a sentence.
    menu.AddWidget(path, "Combo Rules Status", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(ComboRuleStatusPreFunc)
        .Options(TextOptions().Color(UIWidgets::Colors::Gray));
    // The goal (ADR 0010 D1): OoTMM's `goal` setting, its values and its
    // default. Its own group because it is what the whole world is proved
    // against, not a rule about the crossing.
    menu.AddWidget(path, "Win Condition", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, ComboRuleRowName(COMBO_SETTING_GOAL), WIDGET_COMBOBOX)
        .ValuePointer(&comboRuleGoal)
        .PreFunc(ComboRuleGoalPreFunc)
        .Callback([](WidgetInfo& info) { Combo_ComboSettingSet(COMBO_SETTING_GOAL, comboRuleGoal); })
        .Options(ComboboxOptions()
                     .ComboMap(comboRuleGoalOptions)
                     .Tooltip("Chooses which final boss the seed guarantees you can reach and defeat. The paired "
                              "game ends when the goal is met. A final boss defeated before then sends you back "
                              "into its game.\n\n"
                              "Ganon & Majora: Defeat both Ganon and Majora, in any order. Both games are "
                              "finishable.\n"
                              "Any Final Boss: Defeat either Ganon or Majora. The other game may be unfinishable.\n"
                              "Triforce Hunt: Collect Triforce Pieces from both games. Paired worlds cannot be "
                              "created with this goal yet.\n"
                              "Ganon: Defeat Ganon. Majora's Mask may be unfinishable.\n"
                              "Majora: Defeat Majora. Ocarina of Time may be unfinishable."));

    menu.AddWidget(path, "Item Crossing", WIDGET_SEPARATOR_TEXT);

    menu.AddWidget(path, ComboRuleRowName(COMBO_SETTING_DIRECTION), WIDGET_COMBOBOX)
        .ValuePointer(&comboRuleDirection)
        .PreFunc(ComboRuleDirectionPreFunc)
        .Callback([](WidgetInfo& info) { Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, comboRuleDirection); })
        .Options(ComboboxOptions()
                     .ComboMap(comboRuleDirectionOptions)
                     .Tooltip("Chooses which way items may cross between the two games.\n\n"
                              "Off: No items cross. The two games are still one paired world.\n"
                              "OoT Items to MM: Ocarina of Time items may appear in Majora's Mask.\n"
                              "MM Items to OoT: Majora's Mask items may appear in Ocarina of Time.\n"
                              "Both Directions: Items may cross both ways."));

    // No pool-size rows (#801): under the single bag how many items cross is an
    // outcome of the fill, not a setting, and the "Max OoT Items" / "Max MM
    // Items" sliders only re-seeded the world.

    // The shared ocarina (#668). A comboFlags BIT rather than a field of its
    // own, so the row is a plain checkbox over the model's 0/1 space; everything
    // else about it is the other rows' pattern verbatim: the staging buffer, the
    // PreFunc that refreshes from the record, the Callback that offers the edit
    // to src/common's writer, and the marker in the name. The name is the
    // two-line checkbox form (ComboRuleCheckboxRowName says why).
    menu.AddWidget(path, ComboRuleCheckboxRowName(COMBO_SETTING_SHARED_OCARINA), WIDGET_CHECKBOX)
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
            "Makes the ocarina one item across both games: finding an ocarina in either game gives you one in the "
            "other. Majora's Mask's ocarina gives the Fairy Ocarina in Ocarina of Time."));

    // Reset confirms first, as SoH's destructive buttons do ("Clear Config",
    // SohMenuSettings.cpp); the popup's own Reset button does the work.
    menu.AddWidget(path, "Reset Combo Rules", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            ComboRuleRegisterPopup("Reset Combo Rules",
                                   "This will reset every cross-game rule to its default value.\nContinue?", "Reset",
                                   "Cancel", ComboRuleResetAll);
        })
        .PreFunc([](WidgetInfo& info) {
            // A live Reset under a frozen record would be a control that
            // (correctly) does nothing: ADR 0004 section 5's vacuous-gate class.
            ComboRuleApplyDecided(info, Combo_ComboSettingsFrozen());
        })
        .Options(ButtonOptions()
                     .Size(ImVec2(250.f, 0.f))
                     .Tooltip("Resets every cross-game rule to the value RedShipBlueShip ships with."));

    // No item-class groups (#834): under the single bag every bag row is
    // progression, so no rule read the other class bits and the Progression
    // box only repeated what Crossing Direction says; the "OoT Classes" / "MM
    // Classes" checkboxes only re-seeded the world.
}

/**
 * The Windows page: the common-owned cross-game windows and MM's four trackers,
 * moved here with the rules (#497 step 6). Same registrar shape and same reason
 * for being externally linked. It holds exactly the LIVE-PLAY tools, SoH's rule
 * for a window (docs/ui-style-guide.md section 10): the MM randomizer options
 * that used to open from here are pages since 2026-09-27.
 *
 * SHAPED LIKE SoH's OWN TRACKER PAGES (UI parity M5; docs/ui-style-guide.md
 * R-N7). Randomizer > Item Tracker gives each window its own SEPARATOR_TEXT over
 * a "Toggle <Window>" row whose tooltip reads "Toggles the <Window>.", and each
 * settings window its own separator over a "Popout <Window> Settings" row
 * ("Enables the separate <Window> Settings Window."). Every window here follows
 * that shape, so the page reads as one more SoH tracker page.
 *
 * WINDOW_BUTTON reads .CVar only for the open/close label and calls the window's
 * own ToggleVisibility (UIWidgets.cpp:198), so .CVar MUST equal the window's ctor
 * visibility CVar and .WindowName its registered name. Both come from the
 * constants the windows are built with: ComboGui::kComboSpoiler* /
 * kComboTracker* / kComboItemTracker* (src/common/ComboSpoilerWindow.h,
 * ComboTrackerWindow.h, ComboItemTrackerWindow.h) and
 * the RSBS_CVAR_MM_WINDOW_*
 * macros in src/common/cvar_shared_keys.h, which
 * games/mm/2s2h/TrackersGuiSingleExe.cpp static_asserts its own ctor CVars
 * against. The four MM window NAMES stay literals: their constants live in an MM
 * header this OoT TU does not include. MenuComboSection restates every pair as
 * literals on purpose, so the menu and the window still have to agree with a
 * third spelling.
 *
 * WHY THESE ROWS ARE UNGATED, now that #497 step 3 gives them a gate to use.
 * Both common-owned windows read only gComboCtx and CVars, never either game's
 * gSaveContext (ADR 0008 rule 5), so they are safe under every GameId - there is
 * no capability to be absent. MM's four trackers are MMActiveGated: they draw
 * only while MM is the running game, and under OoT the window opens blank, which
 * is the upstream behaviour rather than a broken control. Their tooltips say so
 * with the caveat every MM-only row carries ("Majora's Mask only.", guide
 * R-TT6), which is the answer to the blank window without a gate. Each names
 * the window by its registered "MM ..." name, as SoH's "Toggles the Item
 * Tracker." names its own, and stays under UIWidgets::WrappedText's 80
 * characters so it draws on one line as SoH's do (the earlier "Shows only while
 * Majora's Mask is running." wrapped a lone "running." onto a second line). Gating them on SOH_MENU_CAP_MM_HOSTED would
 * be defensible if MM's half could be absent from this binary; it cannot be, and a gate whose predicate is a constant
 * is the decoration ADR 0004 section 5 is against.
 */
void AddComboWindowWidgets(SohMenu& menu, WidgetPath& path) {
    // ---- The common-owned cross-game windows --------------------------------
    // NO "Toggle MM Randomizer Options" ROW (2026-09-27, ADR 0004's host
    // amendment). MM's randomizer options and tricks are settings chosen before
    // the paired world is created, so they are pages now (Combo > MM Randomizer
    // and Combo > MM Tricks, SohMenuComboMmRandomizer.cpp), the way SoH keeps its
    // own randomizer settings on pages. This page keeps only the tools a player
    // uses DURING play, as SoH's Item Tracker and Check Tracker pages do.
    //
    // NO "Toggle Combo Settings" ROW (#655). PR #652 put one here, opening the
    // common-owned ComboSettingsWindow pane. The five settings it rendered are
    // the rows of Cross-Game Rules now, so a button that opens a second surface
    // over the same five keys would be the only way for the two to disagree. The
    // window stays REGISTERED (Combo_MMOptionsPages_Init and rsbs/src/main.cpp
    // both call Combo_ComboSettingsWindow_Init, and its headless lock still
    // drives it) but nothing in the menu writes gCombo.Windows.ComboSettings any
    // more, so it does not appear. See src/common/ComboSettingsWindow.h for why
    // it was kept rather than deleted.
    //
    // Spoiler - reveals paired-seed placements, so race-disabled like a spoiler tool.
    menu.AddWidget(path, "Cross-Game Spoiler", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle Cross-Game Spoiler", WIDGET_WINDOW_BUTTON)
        .CVar(ComboGui::kComboSpoilerVisibilityCVar)
        .RaceDisable(true)
        .WindowName(ComboGui::kComboSpoilerWindowName)
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Cross-Game Spoiler.").EmbedWindow(false));
    // Combo tracker (#458) - both games' progress at once, the inactive game's
    // included ("As of the last game switch or save"). Race-disabled like the spoiler: its
    // cross-game section names items sitting on uncollected checks.
    menu.AddWidget(path, "Combo Tracker", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle Combo Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(ComboGui::kComboTrackerVisibilityCVar)
        .RaceDisable(true)
        .WindowName(ComboGui::kComboTrackerWindowName)
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Combo Tracker.").EmbedWindow(false));
    // The unified Item Tracker overlay (#458 U2): both games' items, the
    // inactive game's from its snapshot. Not race-disabled, as SoH's own Toggle
    // Item Tracker row is not: it shows only what the player holds.
    menu.AddWidget(path, "Combo Item Tracker", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle Combo Item Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(ComboGui::kComboItemTrackerVisibilityCVar)
        .RaceDisable(false)
        .WindowName(ComboGui::kComboItemTrackerWindowName)
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Combo Item Tracker.").EmbedWindow(false));

    // MM's four tracker windows had the same unreachability bug as the two
    // windows above. #489 made them openable and correctly named (they register
    // under "MM "-prefixed names, since SoH owns the unprefixed ones), but
    // nothing in the live menu ever wrote their visibility CVars:
    //   - MM's own menu, BenGui.cpp SetupGuiElements, is excluded from single-exe
    //     (0 hits in redship.map) - that was their intended writer.
    //   - The OoT tracker rows cannot serve: CVAR_WINDOW expands to
    //     "gOpenWindows.*" (CVAR_PREFIX_WINDOW, CMake/soh-cvars.cmake:8) while
    //     MM's trackers read "gWindows.*". Different CVars entirely, by design -
    //     the distinct namespaces are what avoid a store collision.
    //   - ItemTrackerSettings.cpp does link and has its own Enable/Disable
    //     button, but it is drawn INSIDE the settings window, which has no
    //     writer either. Chicken-and-egg.
    // So the only way to open them was the console. These rows are the writer.
    //
    // The windows are MMActiveGated, so they draw only while MM is the running
    // game - the buttons are usable from the first frame, the window simply
    // stays blank under OoT, which is the upstream behavior and what each
    // tooltip's trailing "Majora's Mask only." tells the player.
    //
    // "From the first frame" only holds because rsbs/src/main.cpp registers the
    // four windows at startup (#535). They used to register from MM_Rando_Init,
    // i.e. only once MM had booted in this process, which left these rows over
    // a per-frame GetGuiWindow lookup that failed. Menu.cpp now greys such a
    // row out instead of skipping it, but that is the fallback.
    //
    // EmbedWindow(false) on all four, like SoH's own tracker toggles: the embed
    // path calls window->DrawElement() directly, which bypasses the
    // MMActiveGated Draw wrapper - the only thing keeping MM tracker UI from
    // drawing while OoT is the running game, and (before mm.o2r is mounted) from
    // reaching for tracker icons that are not loaded. Pop-out only, so the gate
    // stays the single authority on when MM tracker UI draws.
    menu.AddWidget(path, "MM Item Tracker", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle MM Item Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(RSBS_CVAR_MM_WINDOW_ITEM_TRACKER)
        .RaceDisable(false)
        .WindowName("MM Item Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the MM Item Tracker. Majora's Mask only.").EmbedWindow(false));
    menu.AddWidget(path, "MM Item Tracker Settings", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Popout MM Item Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar(RSBS_CVAR_MM_WINDOW_ITEM_TRACKER_SETTINGS)
        .RaceDisable(false)
        .WindowName("MM Item Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Enables the separate MM Item Tracker Settings Window. Majora's Mask only.")
                     .EmbedWindow(false));
    menu.AddWidget(path, "MM Check Tracker", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle MM Check Tracker", WIDGET_WINDOW_BUTTON)
        .CVar(RSBS_CVAR_MM_WINDOW_CHECK_TRACKER)
        .RaceDisable(false)
        .WindowName("MM Check Tracker")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the MM Check Tracker. Majora's Mask only.").EmbedWindow(false));
    menu.AddWidget(path, "MM Check Tracker Settings", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Popout MM Check Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar(RSBS_CVAR_MM_WINDOW_CHECK_TRACKER_SETTINGS)
        .RaceDisable(false)
        .WindowName("MM Check Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions()
                     .Tooltip("Enables the separate MM Check Tracker Settings Window. Majora's Mask only.")
                     .EmbedWindow(false));
}

/**
 * The Windows page's sidebar name, and the name it shipped under.
 *
 * RENAMED 2026-09-27 (UI parity, lane U3). "Cross-Game Windows" is wider than
 * the 200 px sidebar in Montserrat 24: ModernMenuSidebarEntry centres a label
 * on the child's work rect, so an over-wide one is cut on BOTH sides, and the
 * snapshot harness showed it as "ross-Game Window" at 1280x800. SoH's own
 * sidebar names are one or two short words ("Item Tracker", "Entrance
 * Tracker"); under the Combo header the page needs no qualifier.
 *
 * Selection persists BY DISPLAY NAME in gSettings.Menu.ComboSidebarSection, so
 * a config that last had the page open under its old name would otherwise fall
 * back to the section's first page. ComboSidebarCarryRenamedSelection moves it
 * to the new name, once, before the menu's first draw reads the key.
 *
 * The same day the contributed "MM Enhancements" page became "Majora's Mask"
 * (SohMenuComboMmEnhancements.cpp says why). Its new name is spelled a second
 * time in the table below because that TU's constant is file-local on purpose;
 * MenuComboSection leg 6 requires every carried-to name to be a registered
 * page, so the two spellings cannot drift apart without a red row.
 */
static constexpr const char* kComboWindowsPage = "Windows";

struct ComboSidebarRename {
    const char* formerName;
    const char* currentName;
};
static constexpr ComboSidebarRename kComboSidebarRenames[] = {
    { "Cross-Game Windows", kComboWindowsPage },
    { "MM Enhancements", "Majora's Mask" },
};

static void ComboSidebarCarryRenamedSelection(const char* sidebarCvar) {
    const char* selected = CVarGetString(sidebarCvar, "");
    if (selected == nullptr) {
        return;
    }
    for (const ComboSidebarRename& rename : kComboSidebarRenames) {
        if (strcmp(selected, rename.formerName) == 0) {
            CVarSetString(sidebarCvar, rename.currentName);
            return;
        }
    }
}

/**
 * The tier-4 **Combo** section itself (ADR 0004 section 4, #497 step 6).
 *
 * `Menu.ComboSidebarSection` is a new `gSettings.Menu.*` key, minted the way
 * `Menu.RandomizerSidebarSection` and `Menu.NetworkSidebarSection` were: a
 * top-level header needs one, because `MainMenuEntry::sidebarCvar` is where the
 * last-viewed sidebar persists. It is deliberately NOT added to
 * `RSBS::kMenuIndexKeys`, and that is a considered choice rather than an
 * oversight - that table holds the FOUR keys #451 contends over, which is why
 * neither of the two existing sidebar keys above appears in it either. #451's
 * arming condition is an MM-side shell indexing one of those four, and the
 * cvar-classification lock's MM-side reader allowlist is what watches for it;
 * a fifth key with no MM-side reader does not arm anything. ADR 0004's resolved
 * call 1 declined to mint a key for a purely COSMETIC promotion; this one hosts
 * a section that did not exist.
 */
void SohMenu::AddMenuCombo() {
    AddMenuEntry("Combo", CVAR_SETTING("Menu.ComboSidebarSection"));
    ComboSidebarCarryRenamedSelection(CVAR_SETTING("Menu.ComboSidebarSection"));

    // PIN THE COLUMN on every page. A row parked in a column past the page's
    // count is registered and never drawn - Menu::DrawElement iterates
    // columnCount columns, not every column a caller left behind. Cross-Game
    // Rules declares two (its registrar pins its column itself; the second has
    // been empty since #834 retired the item-class groups); every other page
    // one.
    WidgetPath path = { "Combo", "Cross-Game Rules", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo", path.sidebarName, 2);
    AddComboRulesWidgets(*this, path);

    path.sidebarName = kComboWindowsPage;
    path.column = SECTION_COLUMN_1;
    AddSidebarEntry("Combo", path.sidebarName, 1);
    AddComboWindowWidgets(*this, path);

    // The extension point (#497 step 6). Contributed pages come LAST so a
    // contributor cannot reorder the shipped ones, and each gets its sidebar
    // created here rather than in its own registrar - a registrar that had to
    // call AddSidebarEntry itself could name a section it does not belong to.
    for (const ComboSectionPage& page : GetComboSectionPages()) {
        if (page.registrar == nullptr || page.sidebarName.empty()) {
            continue;
        }
        path.sidebarName = page.sidebarName;
        path.column = SECTION_COLUMN_1;
        AddSidebarEntry("Combo", path.sidebarName, page.columnCount);
        page.registrar(*this, path);
    }
}

} // namespace SohGui
