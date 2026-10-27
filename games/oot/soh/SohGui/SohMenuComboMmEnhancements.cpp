/**
 * @file SohMenuComboMmEnhancements.cpp
 * @brief #682 — the curated Majora's Mask enhancement rows, hosted on Ship of
 *        Harkinian's own Enhancements pages beside their twins (#1133); MM's
 *        damage multiplier row (#1037); and the registration of MM's randomizer
 *        pages (#1101).
 *
 * THE PROBLEM. `games/mm/2s2h/BenGui/BenMenu.cpp` is excluded from the single
 * exe (games/mm/CMakeLists.txt), so MM's enhancement toggles had no widget
 * anywhere in the binary. `shipofharkinian.json` and the console were the whole
 * surface. #653 is what that costs in practice: MM death went straight to
 * vanilla respawn for the life of the project because nothing could set
 * `gEnhancements.Kaleido.GameOver`, and the ADR 0009 4a/4b machinery built on
 * top of the kaleido prompt described a screen a default build cannot show.
 *
 * WHERE THE ROWS ARE (#1133, operator ruling D5 of 2026-10-22: "fold into the
 * applicable SoH pane per setting"). Until #1133 they were a page of the tier-4
 * Combo header, "Majora's Mask" ("MM Enhancements" until 2026-09-27). That
 * header retired, and each row is hosted on the SoH page that holds its twin or
 * nearest relative, through the hosted-row seam (HostedMm/SohMenuHostedMm.h),
 * by a function below that SoH's page calls in one line (the #1037 shape):
 *  - MM Autosave Interval: Enhancements > Quality of Life > Saving, directly
 *    after SoH's Autosave. Autosave itself is the converged key both games read
 *    (`gEnhancements.Autosave`), so SoH's row is the one row for both games
 *    (#1119's form) and the old page's pointer to it is gone; the interval is
 *    MM's own (OoT's is a fixed 3 minutes), held apart in kMustStayDistinct.
 *  - Skip Time Song Cutscenes: Enhancements > Skips & Speed-ups > Cutscenes,
 *    directly after SoH's Skip Song Cutscenes, its nearest relative.
 *  - Game Over Prompt and Better Double Time have no twin, so they are a
 *    "Majora's Mask" block on Enhancements > Quality of Life, after Time of Day:
 *    Quality of Life is where SoH keeps how play flows around saving and the
 *    time of day, and the Game Over Prompt is a save prompt on death while
 *    Better Double Time picks the hour the Song of Double Time skips to.
 * Each row keeps its key and its label; SohMenuCombo.cpp carries a menu left
 * open on the old page to Enhancements > Quality of Life.
 *
 * WHERE THE ROWS COME FROM. `RSBS::kHostedMmEnhancements`
 * (src/common/cvar_shared_keys.h). The table, not this file, is the manifest:
 * each row carries its CVar, its provider TU with line anchors, the ShipInit
 * path its registrar is keyed on, how a lock attributes that registrar, and its
 * ADR 0004 section 5 liveness class with a reason. A row here binds its key's
 * RSBS_CVAR_MM_* define straight in `.CVar(...)` (games/oot never spells an MM
 * key; the classification lock's generic leg holds each define to one row) and
 * takes its label, tooltip, widget and presentation from the manifest entry for
 * that define, so a row cannot drift from the provider that reads it.
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
 * ALL FOUR ROWS ARE LIVE TODAY and no carve-out was needed for any of them:
 * #679 already WHOLE_ARCHIVE'd the two Songs providers, `SavingEnhancements.cpp`
 * is pulled into the link by the extern "C" references `GameExports_SingleExe.cpp`
 * makes to it (and its `RegisterSavingEnhancements` is on
 * check-registrar-elision.sh's required-symbol allowlist), and the game-over
 * readers are decomp TUs in `2ship_src` that `z_play.c` calls. #693's autosave
 * interval is read by that same SavingEnhancements.cpp, on the tick the Autosave
 * registrar arms. The disabled branch below is therefore untaken by the shipped
 * data; it is kept, and locked from synthetic rows by MenuMmEnhancementRows,
 * because the next row added to the allowlist may well not be live, and
 * discovering that the disabled path was never written would be discovering it
 * at the worst time.
 *
 * Locked ROM-free by MenuMmEnhancementRows (each row's SoH page, separator and
 * neighbour, its CVar binding and presentation), by CVarClassification (each
 * define bound by exactly one row) and by MMEnhancementToggles (the MM-side
 * liveness evidence: the registrars linked, ran, and re-arm from the key the row
 * writes).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"
// The hosted-row seam (#1233): every row below goes through it.
#include "soh/SohGui/HostedMm/SohMenuHostedMm.h"

// The manifest. ADR 0008 rule 5's restatement for an OoT-hosted row: the row
// reads src/common, never MM's headers and never either game's gSaveContext.
#include "cvar_shared_keys.h"
// The MM randomizer pages' names and column count, registered below.
#include "combo_mm_locations_view.h" // COMBO_MM_LOCATIONS_PAGE_NAME
#include "combo_mm_options_page.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** Is @p key one of the manifest's keys? constexpr, so a row below that binds a
 *  define the manifest does not list is a red build, not a row with no entry. */
constexpr bool CuratedKeyListed(const char* key) {
    for (const RSBS::HostedMmEnhancement& row : RSBS::kHostedMmEnhancements) {
        if (RSBS::MmEnhStringEqual(row.key, key)) {
            return true;
        }
    }
    return false;
}
static_assert(CuratedKeyListed(RSBS_CVAR_MM_AUTOSAVE_INTERVAL) && CuratedKeyListed(RSBS_CVAR_MM_SKIP_SOT_CUTSCENES) &&
                  CuratedKeyListed(RSBS_CVAR_MM_KALEIDO_GAME_OVER) &&
                  CuratedKeyListed(RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME),
              "every curated row hosted below binds a key the manifest (RSBS::kHostedMmEnhancements) lists");

/** The manifest entry for @p key, which CuratedKeyListed proved is there. */
const RSBS::HostedMmEnhancement& CuratedRow(const char* key) {
    for (const RSBS::HostedMmEnhancement& row : RSBS::kHostedMmEnhancements) {
        if (RSBS::MmEnhStringEqual(row.key, key)) {
            return row;
        }
    }
    return RSBS::kHostedMmEnhancements[0]; // unreachable: the static_assert above
}

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
 * `entry` points into a table of STATIC storage (the shipped manifest, or a
 * lock's synthetic one): the lambda outlives this function and `WidgetInfo`
 * stores it as a `WidgetFunc`, so the row it reads must outlive the menu.
 */
WidgetFunc PresentationPreFunc(const RSBS::HostedMmEnhancement* entry) {
    return [entry](WidgetInfo& info) {
        const RSBS::HostedMmEnhancement& row = *entry;
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
 * parent feature is simply off, and OoT's own "Notification on Autosave" row,
 * right below the interval since #1133, renders exactly this relationship by
 * hiding. `MenuDrawItem` runs `ResetDisables()` (which clears `isHidden`) before
 * every draw, so this must be a PreFunc re-evaluated each frame, and a row that
 * is ALSO non-live composes both: the gate first, then the presentation.
 */
WidgetFunc ShownWhilePreFunc(const RSBS::HostedMmEnhancement* entry) {
    return [entry](WidgetInfo& info) {
        const RSBS::HostedMmEnhancement& row = *entry;
        info.isHidden = CVarGetInteger(row.shownWhileKey, 0) == 0;
        if (row.liveness != RSBS::MmEnhancementLiveness::Live) {
            SohMenu::ApplyPresentation(info, info.name, PresentationFor(row.liveness), row.reason);
        }
    };
}

/**
 * #747: the one gray note a block carries while any row in it is drawn
 * disabled. The MM Randomizer page's group note, the same macro, so the
 * Majora's Mask surfaces say one thing one way and cannot drift. It is this
 * project's sentence, not SoH's: SoH's original menus have no gray capability
 * note to copy (their disabled state is tooltip-only). A sentence-case sentence
 * (SoH's gray-note voice, style guide R-S3a), never the rows' Title Case reason
 * fragment: that stays in each row's disabled tooltip, which "Hover one" points
 * at.
 */
constexpr const char* kGroupNote = COMBO_MM_GROUP_NOTE_UNAVAILABLE;

/**
 * Is @p row drawn DISABLED right now? A row of ours whose provider is not fully
 * live, and which is not hidden by its gate: a hidden row draws nothing, so a
 * note pointing at it would say "hover one" over nothing. The gate is read the
 * way ShownWhilePreFunc reads it, every frame.
 */
bool DrawsDisabled(const RSBS::HostedMmEnhancement& row) {
    return row.hosting == RSBS::MmEnhancementHosting::OwnRow && row.liveness != RSBS::MmEnhancementLiveness::Live &&
           (row.shownWhileKey == nullptr || CVarGetInteger(row.shownWhileKey, 0) != 0);
}

/**
 * #747: a block's note, directly under its heading and above its rows (style
 * guide R-S3: one gray note row above the group). Registered only for a block
 * that holds a non-live row of ours (liveness is a link-time fact the table
 * records, so this is decided at registration), so a block whose rows are all
 * Live -- the shipped one, today -- draws exactly its rows. Shown while any row
 * of the block is drawn disabled, hidden otherwise (ApplyPresentationNote's LIVE
 * hides it). It is the part of the state that is legible WITHOUT hovering and
 * that a race lockout cannot replace: `.RaceDisable(false)`, and a TEXT row has
 * no tooltip for MenuDrawItem's lockout branch to rebuild. Kept out of the menu
 * search, like every gray note: its sentence is not a setting. @p rows point into
 * static storage.
 */
void AddGroupNote(SohMenu& menu, WidgetPath& path, std::vector<const RSBS::HostedMmEnhancement*> rows) {
    bool anyNonLive = false;
    for (const RSBS::HostedMmEnhancement* row : rows) {
        anyNonLive = anyNonLive || (row->hosting == RSBS::MmEnhancementHosting::OwnRow &&
                                    row->liveness != RSBS::MmEnhancementLiveness::Live);
    }
    if (!anyNonLive) {
        return;
    }
    menu.AddWidget(path, "Majora's Mask Group Note", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc([rows](WidgetInfo& note) {
            bool disabled = false;
            for (const RSBS::HostedMmEnhancement* row : rows) {
                disabled = disabled || DrawsDisabled(*row);
            }
            SohMenu::ApplyPresentationNote(note, disabled ? SOH_MENU_PRESENT_CAPABILITY : SOH_MENU_PRESENT_LIVE,
                                           kGroupNote);
        })
        .Options(TextOptions().Color(Colors::Gray));
}

/** The widget type a curated row draws: an integer key gets SoH's int slider (#693), a boolean SoH's checkbox. */
WidgetType CuratedWidgetType(const RSBS::HostedMmEnhancement& row) {
    return row.widget == RSBS::MmEnhancementWidget::SliderInt ? WIDGET_CVAR_SLIDER_INT : WIDGET_CVAR_CHECKBOX;
}

/**
 * The rest of a curated row, after its widget is added and bound: SoH's options
 * with the manifest's tooltip (and, for a slider, its range, default and
 * format), never race-disabled, and its per-frame PreFunc.
 *
 * The widget is its own writer (UIWidgets::CVarCheckbox and CVarSliderInt call
 * CVarSetInteger and then ShipInit::Init). That is CORRECT here and wrong for
 * the tier-4 rules: a tier-4 rule authors world identity and has a src/common
 * write choke point that must refuse post-creation, so binding it to a
 * self-writing widget would put a second ungated writer beside the gate. These
 * keys are preferences with no freeze and no choke point, so the widget IS the
 * writer, and the ShipInit::Init call it already makes is what re-arms MM
 * (#539). The interval's provider reads its key inline on every tick, so there
 * is nothing for ShipInit to re-arm there.
 */
void ApplyCuratedRow(WidgetInfo& info, const RSBS::HostedMmEnhancement& row) {
    if (row.liveness != RSBS::MmEnhancementLiveness::Live && row.issue == 0) {
        // Unreachable for the shipped table: HostedMmEnhancementsAreHonest()
        // is a static_assert on it, so such a row is a red build. Only a
        // synthetic table can reach this, and it gets the same words, logged.
        SPDLOG_WARN("Majora's Mask: the non-live row \"{}\" records no tracking issue. A reason nobody can trace is a "
                    "reason nobody retires.",
                    row.key);
    }
    info.RaceDisable(false);
    if (row.widget == RSBS::MmEnhancementWidget::SliderInt) {
        info.Options(IntSliderOptions()
                         .Min(row.sliderMin)
                         .Max(row.sliderMax)
                         .DefaultValue(row.sliderDefault)
                         .Format(row.sliderFormat)
                         .Tooltip(row.tooltip));
    } else {
        info.Options(CheckboxOptions().Tooltip(row.tooltip));
    }
    if (row.shownWhileKey != nullptr) {
        info.PreFunc(ShownWhilePreFunc(&row));
    } else if (row.liveness != RSBS::MmEnhancementLiveness::Live) {
        info.PreFunc(PresentationPreFunc(&row));
    }
}

} // namespace

/**
 * The block builder over a manifest table: the "Majora's Mask" block heading,
 * the block's group note (#747) and each row of ours in table order. A pointer
 * row (MmEnhancementHosting::HostedElsewhere) draws NOTHING: its key's control
 * is SoH's own row, and a second widget over one key would only add a way for
 * two surfaces to disagree (the duplicate-host shape #655 removed).
 *
 * The shipped block (AddMmQualityOfLifeBlock below) is built from the same
 * helpers through the hosted-row seam; this builder adds its rows with
 * SohMenu::AddWidget instead, because MenuMmEnhancementRows and the UiSnapshot
 * harness hand it SYNTHETIC tables holding non-live rows (every shipped row is
 * Live, so the disabled rows and their note would otherwise be code no test and
 * no pixel ever reached), and a synthetic row in the seam's registry would be a
 * hosted row on a page no menu has. Externally linked and declared in no header,
 * like AddComboRulesWidgets. @p rows must have static storage: the rows'
 * PreFuncs keep pointers into it for the life of the menu.
 */
void AddMmEnhancementRows(SohMenu& menu, WidgetPath& path, const RSBS::HostedMmEnhancement* rows, std::size_t count) {
    AddHostedMmBlock(menu, path);
    std::vector<const RSBS::HostedMmEnhancement*> ours;
    for (std::size_t i = 0; i < count; i++) {
        if (rows[i].hosting == RSBS::MmEnhancementHosting::OwnRow) {
            ours.push_back(&rows[i]);
        }
    }
    AddGroupNote(menu, path, ours);
    for (const RSBS::HostedMmEnhancement* row : ours) {
        ApplyCuratedRow(menu.AddWidget(path, row->label, CuratedWidgetType(*row)).CVar(row->key), *row);
    }
}

/**
 * #693's interval, on Enhancements > Quality of Life > Saving, directly after
 * SoH's Autosave (SohMenuEnhancements.cpp calls it in one line, L7 #1133). Shown
 * only while Autosave is on, as SoH's Notification on Autosave below it is.
 */
void AddMmAutosaveIntervalRow(SohMenu& menu, WidgetPath& path) {
    const RSBS::HostedMmEnhancement& row = CuratedRow(RSBS_CVAR_MM_AUTOSAVE_INTERVAL);
    ApplyCuratedRow(
        AddHostedMmWidget(menu, path, "Saving", row.label, CuratedWidgetType(row)).CVar(RSBS_CVAR_MM_AUTOSAVE_INTERVAL),
        row);
}

/**
 * Skip Time Song Cutscenes, on Enhancements > Skips & Speed-ups > Cutscenes,
 * directly after SoH's Skip Song Cutscenes (SohMenuEnhancements.cpp calls it in
 * one line, L7 #1133). SoH has no Song of Time cutscene of this kind, so it is
 * the nearest relative, not a twin: the row keeps its plain label.
 */
void AddMmSkipTimeSongCutscenesRow(SohMenu& menu, WidgetPath& path) {
    const RSBS::HostedMmEnhancement& row = CuratedRow(RSBS_CVAR_MM_SKIP_SOT_CUTSCENES);
    ApplyCuratedRow(AddHostedMmWidget(menu, path, "Cutscenes", row.label, CuratedWidgetType(row))
                        .CVar(RSBS_CVAR_MM_SKIP_SOT_CUTSCENES),
                    row);
}

/**
 * The Majora's Mask block on Enhancements > Quality of Life, after Time of Day
 * (SohMenuEnhancements.cpp calls it in one line, L7 #1133): the two curated rows
 * with no twin on any SoH page, Game Over Prompt then Better Double Time, the
 * manifest's order, under the seam's block heading and the block's note.
 */
void AddMmQualityOfLifeBlock(SohMenu& menu, WidgetPath& path) {
    const RSBS::HostedMmEnhancement& gameOver = CuratedRow(RSBS_CVAR_MM_KALEIDO_GAME_OVER);
    const RSBS::HostedMmEnhancement& doubleTime = CuratedRow(RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME);
    AddHostedMmBlock(menu, path);
    AddGroupNote(menu, path, { &gameOver, &doubleTime });
    ApplyCuratedRow(AddHostedMmWidget(menu, path, kHostedMmBlockLabel, gameOver.label, CuratedWidgetType(gameOver))
                        .CVar(RSBS_CVAR_MM_KALEIDO_GAME_OVER),
                    gameOver);
    ApplyCuratedRow(AddHostedMmWidget(menu, path, kHostedMmBlockLabel, doubleTime.label, CuratedWidgetType(doubleTime))
                        .CVar(RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME),
                    doubleTime);
}

/**
 * MM's randomizer options, locations and tricks (SohMenuComboMmRandomizer.cpp),
 * registered HERE rather than from their own TU. The registry keeps
 * registration order, and file-scope initializers in different TUs run in an
 * order the language leaves to the linker, so this is the one way to pin their
 * order: MM Randomizer, MM Locations, then MM Tricks.
 *
 * All three are hosted under SoH's Randomizer header (#1101, operator ruling
 * 2026-10-21, option C): AddComboRandomizerPages draws them after
 * Tricks/Glitches and before Cross-Game, so the one randomizer menu holds both
 * games' settings. MenuMmRandomizerPages pins host and order.
 */
void AddMmRandomizerOptionsWidgets(SohMenu& menu, WidgetPath& path);
void AddMmLocationsWidgets(SohMenu& menu, WidgetPath& path);
void AddMmTricksWidgets(SohMenu& menu, WidgetPath& path);
static RegisterComboSectionPage_t sMmRandomizerPage(COMBO_MM_OPTIONS_PAGE_NAME, COMBO_MM_OPTIONS_PAGE_COLUMNS,
                                                    AddMmRandomizerOptionsWidgets, ComboPageHost::Randomizer);
// MM's excluded checks (#1101 lane R3), between the options and the tricks: the
// ruling's order. One column, as SoH's Randomizer > Locations.
static RegisterComboSectionPage_t sMmLocationsPage(COMBO_MM_LOCATIONS_PAGE_NAME, 1, AddMmLocationsWidgets,
                                                   ComboPageHost::Randomizer);
static RegisterComboSectionPage_t sMmTricksPage(COMBO_MM_TRICKS_PAGE_NAME, 1, AddMmTricksWidgets,
                                                ComboPageHost::Randomizer);

/**
 * #1037: MM's own damage multiplier row, hosted beside SoH's on Enhancements > Difficulty.
 *
 * SoH's "Damage Multiplier" writes gEnhancements.DamageMult (0-8: 1x to 256x, left-shifts) and MM reads its own key
 * with a different list (0-4 plus 10 = 1 Hit KO), so the keys stay apart (RSBS::kMustStayDistinct) and each game
 * has its own control. This is the project-added function: SohMenuEnhancements.cpp calls it right after SoH's row
 * and nothing else of SoH's page changes. The key comes from the src/common constant because games/oot may not
 * spell MM's key (the classification lock's 3c-2). The widget is its own writer, and the ShipInit::Init it makes
 * re-arms MM's registrar (#539). Added through the hosted-row seam (HostedMm/SohMenuHostedMm.h), so the UI snapshot
 * captures, hovers and lints it on SoH's page, and the menu lock's leg 9 holds it under "Health".
 */
void AddMmDamageMultiplierRow(SohMenu& menu, WidgetPath& path) {
    // MM's value list, in SoH's voice (allPowers): the left-shift of the damage MM applies, 0-4, and 10 = 1 Hit KO.
    // Static storage: the combobox keeps a pointer to the map.
    static const std::map<int32_t, const char*> mmDamageMultipliers = {
        { 0, "Vanilla (1x)" }, { 1, "Double (2x)" },   { 2, "Quadruple (4x)" },
        { 3, "Octuple (8x)" }, { 4, "Foolish (16x)" }, { 10, "1 Hit KO" },
    };
    AddHostedMmWidget(menu, path, "Health", "MM Damage Multiplier", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_DAMAGE_MULTIPLIER)
        .Options(
            ComboboxOptions()
                .ComboMap(mmDamageMultipliers)
                .DefaultIndex(0)
                .Tooltip("Multiplies the damage Link takes from all sources, up to 1 Hit KO. Majora's Mask only."));
}

/** The group note's sentence (#747), for the lock and the UiSnapshot harness,
 *  so neither spells the literal a second time. */
const char* MmEnhancementsGroupNoteText() {
    return kGroupNote;
}

} // namespace SohGui
