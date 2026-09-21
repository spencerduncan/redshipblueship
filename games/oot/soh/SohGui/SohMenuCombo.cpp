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
 * hot-swap. Two pages ship: `Cross-Game Rules` and `Cross-Game Windows`. The
 * other three would be EMPTY pages today, and an empty multi-column page is
 * #640's failure mode exactly (Menu::DrawElement's unconditional
 * SetNextWindowPos goes unconsumed and undocks libultraship's "Main Game"
 * window), so they are left unregistered until they have content. Absorbing
 * `ComboMenuBar`'s `.redsave` file-select panel - the ADR's stated home for save
 * slots - is still open.
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

#include <cstdio> // snprintf, for the combo-rule status line and fingerprint
#include <string>
#include <vector>

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// ============================================================================
// Cross-Game combo rules (#655; ADR 0011 increment 2, #498; #497 step 6)
// ============================================================================
// The six tier-4 `gCombo.Rando.*` keys — direction, per-direction pool sizes,
// per-direction item classes, and the shared ocarina (#668) — render as ROWS in
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
static int32_t comboRulePoolSize[2];  // [0] OoT items -> MM checks, [1] MM items -> OoT checks
static bool comboRuleItemClass[2][6]; // [0] OoT pool, [1] MM pool; second index is comboRuleClassBits'
static bool comboRuleSharedOcarina;   // #668: ComboSettingsRecord.comboFlags' shared-ocarina bit

// The allocated RSBS_ITEMCLASS_* bits in bit order (foreign_items.h). Appending
// a class is a new checkbox here; re-pointing an existing bit is forbidden
// there, because these are .redsave format.
static const uint16_t comboRuleClassBits[6] = {
    (uint16_t)RSBS_ITEMCLASS_PROGRESSION,   (uint16_t)RSBS_ITEMCLASS_SONGS,          (uint16_t)RSBS_ITEMCLASS_MASKS,
    (uint16_t)RSBS_ITEMCLASS_DUNGEON_ITEMS, (uint16_t)RSBS_ITEMCLASS_DUNGEON_REWARD, (uint16_t)RSBS_ITEMCLASS_SIDEQUEST,
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
 * The caller owns the sidebar: AddMenuCombo() creates it and pins `path.column`,
 * so this function only adds rows. That is a change from the interim registrar,
 * which had to pin the column itself because five `Rando::Settings` option groups
 * ran immediately before it and left `path.column` wherever their last COLUMN
 * container landed (option.cpp:469-472). Nothing runs before this page in the
 * Combo section, but AddMenuCombo pins it anyway - the cost of being wrong is a
 * row that is registered and never drawn (#640's failure mode with the halves
 * swapped), which no compile catches.
 */
void AddComboRulesWidgets(SohMenu& menu, WidgetPath& path) {
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
                    comboRuleItemClass[which][bit] = (ComboRuleClassMask(shown, which) & comboRuleClassBits[bit]) != 0;
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
}

/**
 * The common-owned cross-game windows and MM's four trackers, moved here with
 * the rules (#497 step 6). Same registrar shape and same reason for being
 * externally linked.
 *
 * WINDOW_BUTTON reads .CVar only for the open/close label and calls the window's
 * own ToggleVisibility (UIWidgets.cpp:198), so .CVar MUST equal the window's ctor
 * visibility CVar and .WindowName its registered name. The string literals below
 * match the OoT tracker rows in SohMenuRandomizer.cpp (which hardcode the same
 * way); the authoritative constants are ComboGui::kComboMMOptions* /
 * kComboSpoiler* / kComboTracker* in src/common/ComboMmOptionsWindow.h,
 * ComboSpoilerWindow.h and ComboTrackerWindow.h, and
 * kCheckTracker* / kItemTracker* in games/mm/2s2h/TrackersGuiSingleExe.h.
 * (Spaced deliberately: "kCheckTracker*" followed immediately by "/" closes this
 * block comment, which is what it did before this line was fixed.)
 *
 * WHY THESE ROWS ARE UNGATED, now that #497 step 3 gives them a gate to use.
 * Both common-owned windows read only gComboCtx and CVars, never either game's
 * gSaveContext (ADR 0008 rule 5), so they are safe under every GameId - there is
 * no capability to be absent. MM's four trackers are MMActiveGated: they draw
 * only while MM is the running game, and under OoT the window opens blank, which
 * is the upstream behaviour rather than a broken control. Gating them on
 * SOH_MENU_CAP_MM_HOSTED would be defensible if MM's half could be absent from
 * this binary; it cannot be, and a gate whose predicate is a constant is the
 * decoration ADR 0004 section 5 is against.
 */
void AddComboWindowWidgets(SohMenu& menu, WidgetPath& path) {
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

    // PIN THE COLUMN on every page. Each page below declares one column, so a row
    // parked in a higher one is registered and never drawn - Menu::DrawElement
    // iterates columnCount columns, not every column a caller left behind.
    WidgetPath path = { "Combo", "Cross-Game Rules", SECTION_COLUMN_1 };
    AddSidebarEntry("Combo", path.sidebarName, 1);
    AddComboRulesWidgets(*this, path);

    path.sidebarName = "Cross-Game Windows";
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
