/**
 * @file SohMenuComboMmEnhancements.cpp
 * @brief #682 — the curated Majora's Mask enhancement toggles, hosted as a
 *        CONTRIBUTED page in the tier-4 Combo section.
 *
 * THE PROBLEM. `games/mm/2s2h/BenGui/BenMenu.cpp` is excluded from the single
 * exe (games/mm/CMakeLists.txt), so MM's enhancement toggles had no widget
 * anywhere in the binary. `shipofharkinian.json` and the console were the whole
 * surface. #653 is what that costs in practice: MM death went straight to
 * vanilla respawn for the life of the project because nothing could set
 * `gEnhancements.Kaleido.GameOver`, and the ADR 0009 4a/4b machinery built on
 * top of the kaleido prompt described a screen a default build cannot show.
 *
 * WHY A CONTRIBUTED PAGE AND NOT AN EDIT TO SohMenuCombo.cpp. #497 step 6 left
 * `SohGui::RegisterComboSectionPage` precisely so a page can be added without
 * touching the section's own TU (SohMenu.h documents the seam). This file is its
 * first production consumer: a file-scope `RegisterComboSectionPage_t` runs
 * before `AddMenuElements()`, `AddMenuCombo()` creates the sidebar and calls the
 * registrar below. That is safe here for the reason the seam's doc comment
 * names: this TU is under `games/oot/soh`, and all three OoT archives are
 * WHOLE_ARCHIVE'd since #341/#640, so the file-scope registrar cannot be elided
 * the way a plain-archive one would be (#516/#640's class).
 *
 * WHY THE COMBO SECTION AND NOT Enhancements. ADR 0004 section 4 gives the
 * combined product one section for the surface that has no upstream counterpart.
 * These rows are not OoT enhancements and there is no MM Enhancements header to
 * put them under; filing them beside OoT's own enhancement rows would say they
 * apply to the game the player is currently in, which is exactly what they do
 * not do. Every row here is labelled "Majora's Mask only" in its tooltip.
 *
 * WHERE THE ROWS COME FROM. `RSBS::kHostedMmEnhancements`
 * (src/common/cvar_shared_keys.h). The table, not this file, is the manifest:
 * each row carries its CVar, its provider TU with line anchors, the ShipInit
 * path its registrar is keyed on, how a lock attributes that registrar, and its
 * ADR 0004 section 5 liveness class with a reason. Spelling the CVar literals
 * here instead would let a menu row drift from the provider that reads it, and
 * the drift is invisible — the widget flips a key nothing consults, which is
 * #499's failure exactly.
 *
 * WHY THE ROWS ARE UNGATED, with step 3's capability gate available. Lane D's
 * rule, applied unchanged: "a gate whose predicate is a constant is the
 * decoration ADR 0004 section 5 is against" (SohMenuCombo.cpp). MM's half cannot
 * be absent from this binary, so `SOH_MENU_CAP_SINGLE_EXE` would evaluate
 * constant here. What CAN be absent is the provider's liveness, and that is not
 * a runtime capability — it is a link-time fact the manifest records and the
 * locks measure, so a non-Live row is rendered through
 * `SohMenu::ApplyPresentation(SOH_MENU_PRESENT_CAPABILITY, reason)` from its own
 * PreFunc rather than through a capability key nothing could evaluate.
 *
 * ALL FIVE ROWS ARE LIVE TODAY and no carve-out was needed for any of them:
 * #679 already WHOLE_ARCHIVE'd the two Songs providers, `SavingEnhancements.cpp`
 * is pulled into the link by the extern "C" references `GameExports_SingleExe.cpp`
 * makes to it (and its `RegisterSavingEnhancements` is on
 * check-registrar-elision.sh's required-symbol allowlist), and the game-over
 * readers are decomp TUs in `2ship_src` that `z_play.c` calls. #693's autosave
 * interval is read by that same SavingEnhancements.cpp, on the tick the Autosave
 * registrar arms. The disabled
 * branch below is therefore untaken by the shipped data; it is kept, and locked
 * from synthetic rows by MenuMmEnhancementRows, because the next row added to
 * the allowlist may well not be live, and discovering that the disabled path was
 * never written would be discovering it at the worst time.
 *
 * Locked ROM-free by MenuMmEnhancementRows (this page's shape, every row's CVar
 * binding and presentation) and by MMEnhancementToggles (the MM-side liveness
 * evidence: the registrars linked, ran, and re-arm from the key the row writes).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// The manifest. ADR 0008 rule 5's restatement for an OoT-hosted row: the row
// reads src/common, never MM's headers and never either game's gSaveContext.
#include "cvar_shared_keys.h"

#include <cstddef>
#include <string>

namespace SohGui {

using namespace UIWidgets;

namespace {

/**
 * The page's sidebar label. A `std::string` compare against this is how
 * MenuMmEnhancementRows finds the page, and sidebar selection persists BY
 * DISPLAY-NAME STRING into `gSettings.Menu.ComboSidebarSection`, so renaming it
 * strands every config that last had it open — the measured reason ADR 0004's
 * resolved call 1 refused to rename a header.
 */
constexpr const char* kMmEnhancementsPage = "MM Enhancements";

/** ADR 0004 section 6's state for a row whose provider is not fully live. */
SohMenuPresentation PresentationFor(RSBS::MmEnhancementLiveness liveness) {
    switch (liveness) {
        case RSBS::MmEnhancementLiveness::Live:
            return SOH_MENU_PRESENT_LIVE;
        case RSBS::MmEnhancementLiveness::Partial:
        case RSBS::MmEnhancementLiveness::Dormant:
        default:
            // Section 5: the behaviour is absent. Disabled, with the reason.
            // Partial and Dormant are the same PRESENTATION and differ only in
            // what the reason says, which is how the common-owned MM options
            // pane already renders COMBO_MM_LIVENESS_PARTIAL.
            return SOH_MENU_PRESENT_CAPABILITY;
    }
}

/**
 * The PreFunc for a row whose manifest entry is not Live.
 *
 * It must be a PreFunc and not a one-shot at registration: `MenuDrawItem` runs
 * `WidgetInfo::ResetDisables()` before every draw, which clears `disabled`,
 * `isHidden` and `activeDisables` — so a disable applied anywhere else is gone
 * by the time the row is drawn. `ApplyPresentation` is idempotent in its base
 * name for the matching reason (it normalises through
 * `StripPresentationSuffix`), which is what makes passing `info.name` back in
 * every frame safe rather than a suffix that compounds.
 *
 * `index` is captured by value, not a pointer into the table: the lambda
 * outlives this function and `WidgetInfo` stores it as a `WidgetFunc`.
 */
WidgetFunc PresentationPreFunc(std::size_t index) {
    return [index](WidgetInfo& info) {
        const RSBS::HostedMmEnhancement& row = RSBS::kHostedMmEnhancements[index];
        // `reason` is a string literal out of the manifest, so it outlives the
        // frame — which it must, because ApplyPresentation stores it into
        // `WidgetOptions::disabledTooltip`, a `const char*` the draw path reads
        // after this PreFunc returns.
        SohMenu::ApplyPresentation(info, info.name, PresentationFor(row.liveness), row.reason);
    };
}

/**
 * #693: the PreFunc for a row whose setting is moot while another key is off.
 * HIDDEN, not disabled, and deliberately not through ApplyPresentation: the
 * provider is not absent (which is what SOH_MENU_PRESENT_CAPABILITY says), the
 * parent feature is simply off, and OoT's own "Notification on Autosave" row
 * already renders exactly this relationship by hiding. `MenuDrawItem` runs
 * `ResetDisables()` (which clears `isHidden`) before every draw, so this must be
 * a PreFunc re-evaluated each frame, and a row that is ALSO non-live composes
 * both: the gate first, then the presentation.
 */
WidgetFunc ShownWhilePreFunc(std::size_t index) {
    return [index](WidgetInfo& info) {
        const RSBS::HostedMmEnhancement& row = RSBS::kHostedMmEnhancements[index];
        info.isHidden = CVarGetInteger(row.shownWhileKey, 0) == 0;
        if (row.liveness != RSBS::MmEnhancementLiveness::Live) {
            SohMenu::ApplyPresentation(info, info.name, PresentationFor(row.liveness), row.reason);
        }
    };
}

} // namespace

/**
 * The contributed page's registrar. Externally linked and declared in no header,
 * the way `AddCrossGamePointerWidgets` and the other Combo registrars are: the
 * extension point hands it the section and a PINNED `path.column`, and
 * MenuMmEnhancementRows reaches it through `AddMenuCombo()` rather than by
 * name, so that the row exercises the real seam instead of a shortcut.
 */
void AddMmEnhancementWidgets(SohMenu& menu, WidgetPath& path) {
    menu.AddWidget(path, "Majora's Mask Enhancements", WIDGET_SEPARATOR_TEXT);

    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& row = RSBS::kHostedMmEnhancements[i];

        if (row.hosting == RSBS::MmEnhancementHosting::HostedElsewhere) {
            // A POINTER ROW, not a second checkbox. Something in the unified menu
            // already writes this key, and since #539 every SohMenu widget
            // funnels through OoT's `ShipInit::Init`, which forwards to MM's
            // registrar map — so the existing control already re-arms both
            // halves. A second widget over one key would add no reach and one
            // more way for two surfaces to disagree, which is the duplicate-host
            // shape #655 removed for the tier-4 rules.
            //
            // Two widgets, the same shape `AddCrossGamePointerWidgets` uses for
            // the stranded interim page: a searchable SEPARATOR_TEXT carrying the
            // short label, then the sentence as a gray TEXT row kept OUT of the
            // menu search. A widget's NAME is what the search indexes, so seeding
            // it with a sentence buries the rows a player was looking for; and
            // `TextOptions` has no `Tooltip` override, so a text row's
            // explanation has to BE its name rather than hide behind a hover.
            menu.AddWidget(path, row.label, WIDGET_SEPARATOR_TEXT);
            menu.AddWidget(path, row.tooltip, WIDGET_TEXT)
                .RaceDisable(false)
                .HideInSearch(true)
                .Options(TextOptions().Color(Colors::Gray));
            continue;
        }

        // WIDGET_CVAR_CHECKBOX, and it is its own writer (UIWidgets::CVarCheckbox
        // calls CVarSetInteger and then ShipInit::Init). That is CORRECT here and
        // wrong for the tier-4 rules next door, and the difference is worth
        // stating: a tier-4 rule authors world identity and has a src/common
        // write choke point that must refuse post-creation, so binding it to a
        // self-writing widget would put a second ungated writer beside the gate.
        // These keys are preferences with no freeze and no choke point, so the
        // widget IS the writer, and the ShipInit::Init call it already makes is
        // what re-arms MM (#539).
        //
        // #693 adds the one non-boolean row, MM's autosave interval, as a
        // WIDGET_CVAR_SLIDER_INT in minutes. Same writer argument: the slider
        // calls CVarSetInteger itself, and its provider reads the key inline on
        // every tick, so there is nothing for ShipInit to re-arm.
        WidgetInfo* info = nullptr;
        if (row.widget == RSBS::MmEnhancementWidget::SliderInt) {
            info = &menu.AddWidget(path, row.label, WIDGET_CVAR_SLIDER_INT)
                        .CVar(row.key)
                        .RaceDisable(false)
                        .Options(IntSliderOptions()
                                     .Min(row.sliderMin)
                                     .Max(row.sliderMax)
                                     .DefaultValue(row.sliderDefault)
                                     .Format(row.sliderFormat)
                                     .Tooltip(row.tooltip));
        } else {
            info = &menu.AddWidget(path, row.label, WIDGET_CVAR_CHECKBOX)
                        .CVar(row.key)
                        .RaceDisable(false)
                        .Options(CheckboxOptions().Tooltip(row.tooltip));
        }
        if (row.shownWhileKey != nullptr) {
            info->PreFunc(ShownWhilePreFunc(i));
        } else if (row.liveness != RSBS::MmEnhancementLiveness::Live) {
            info->PreFunc(PresentationPreFunc(i));
        }
    }
}

/**
 * THE REGISTRATION. File-scope, because `AddMenuCombo()` walks the registry and
 * registration must therefore happen before `AddMenuElements()` runs — a
 * function-local call would need a caller, and the only candidate is the TU this
 * seam exists to avoid editing.
 *
 * One column, and the page holds seven widgets (the heading separator, three
 * checkboxes, the pointer row's separator + text, and #693's interval slider
 * right after the pointer row it belongs to), so it is not #640's empty
 * multi-column page.
 */
static RegisterComboSectionPage_t sMmEnhancementsPage(kMmEnhancementsPage, 1, AddMmEnhancementWidgets);

/** The page's registered name, for the lock. Defined here so the test cannot
 *  drift from the registration by spelling the literal a second time. */
const char* MmEnhancementsPageName() {
    return kMmEnhancementsPage;
}

} // namespace SohGui
