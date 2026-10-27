/**
 * @file SohMenuHostedMm.h
 * @brief The hosted-row seam: how a Majora's Mask setting gets a row on one of Ship of Harkinian's own menu pages
 *        (#1133 ruling D5, #1233).
 *
 * THE RULE THIS SERVES. Every MM row goes on the SoH page that holds its twin or nearest relative, directly after that
 * row, through a project-added function that SoH's page calls in ONE line (the #1037 shape: SohMenuEnhancements.cpp
 * calls AddMmDamageMultiplierRow right after SoH's own Damage Multiplier). A row with no twin goes under a
 * "Majora's Mask" block (AddHostedMmBlock) on the most applicable SoH page. Nothing goes on the Combo > Majora's Mask
 * page any more.
 *
 * WHERE A ROW'S CODE LIVES. Every page lane puts its row TU in this directory, games/oot/soh/SohGui/HostedMm/. The
 * directory is enrolled whole in .github/clang-format-paths.txt and in .github/scripts/ui-lint-files.txt (MENU), so a
 * new file here is formatted and linted with no list edit. All three OoT archives are WHOLE_ARCHIVE'd (#341/#640),
 * and the row function is called by name from SoH's page, so a row TU here cannot be elided.
 *
 * HOW A ROW IS ADDED. In SoH's voice and with SoH's widgets, through AddHostedMmWidget instead of SohMenu::AddWidget:
 *
 *     void AddMmFooRow(SohMenu& menu, WidgetPath& path) {
 *         AddHostedMmWidget(menu, path, "Health", "MM Foo", WIDGET_CVAR_CHECKBOX)
 *             .CVar(RSBS_CVAR_MM_FOO)
 *             .Options(CheckboxOptions().Tooltip("Makes Link foo. Majora's Mask only."));
 *     }
 *
 * The seam returns a small handle whose .CVar(key) (or .Accessor(name), for a row that binds a src/common accessor
 * instead of a key) records the binding and returns the WidgetInfo, so the rest of the chain is SoH's own. Bind the
 * RSBS_CVAR_MM_* define straight in .CVar(...): games/oot never spells an MM key as a literal, and the classification
 * lock's generic leg (3f-2c) finds the binding in exactly that form.
 *
 * LABELS AND TOOLTIPS (docs/ui-style-guide.md; the #1037 row is the model):
 *  - a row whose SoH twin sits beside it is labelled "MM <SoH's label>" ("MM Damage Multiplier");
 *  - a row with no twin keeps the plain label it would have in 2Ship, in SoH's Title Case;
 *  - every hosted row's tooltip opens with a present-tense verb and ENDS "Majora's Mask only.";
 *  - the "[Both Games]" labels on SoH's own rows are left untouched (operator ruling 2026-09-27).
 *
 * WHAT THE REGISTRY BUYS. Every row added through the seam is recorded with its header, sidebar, section (the SEPARATOR
 * TEXT it sits under, "" when the page has none above it), name and binding. Two consumers walk it, so a page lane adds
 * registry entries and never a harness block or a menu leg:
 *  - games/oot/soh/soh_ui_snapshot.cpp captures every SoH page holding a hosted row, hovers every hosted row and runs
 *    the runtime lint over them;
 *  - games/oot/soh/soh_menu_mm_enhancements_test.cpp (leg 9) finds every registered row on its page, under its
 *    section, bound to its key or accessor.
 * The registry is filled when a menu is built (the live menu, or a test's probe of SoH's own builders); a row is
 * keyed by header, sidebar and name, so building the same page twice records it once.
 */

#pragma once

#include "soh/SohGui/SohMenu.h"

#include <cstddef>
#include <string>
#include <vector>

namespace SohGui {

/** One row added through the seam. */
struct HostedMmRow {
    std::string header;             ///< SoH's menu header ("Enhancements").
    std::string sidebar;            ///< The sidebar page ("Difficulty").
    std::string section;            ///< The SEPARATOR_TEXT the row sits under ("Health"); "" when none is above it.
    std::string name;               ///< The row's registered name (its label, format string included).
    const char* key = nullptr;      ///< The CVar the row binds (an RSBS_CVAR_* define), or null.
    const char* accessor = nullptr; ///< The src/common accessor the row binds instead of a key, or null.
};

/** The handle AddHostedMmWidget returns: bind the row, then carry on with SoH's chain. */
class HostedMmWidget {
  public:
    HostedMmWidget(WidgetInfo& info, std::size_t index) : mInfo(info), mIndex(index) {
    }
    /** Bind the row to @p key (an RSBS_CVAR_* define) and return the WidgetInfo for the rest of the chain. */
    WidgetInfo& CVar(const char* key);
    /** Bind the row to the src/common accessor named @p accessorName instead of a key (the row draws through its
     *  ValuePointer, PreFunc or CustomFunction and carries no CVar), and return the WidgetInfo. */
    WidgetInfo& Accessor(const char* accessorName);

  private:
    WidgetInfo& mInfo;
    std::size_t mIndex;
};

/** The block heading for rows with no twin on the page. SoH's fonts carry Latin-1 only, so it holds no dash. */
inline constexpr const char* kHostedMmBlockLabel = "Majora's Mask";

/**
 * Add a hosted MM row on SoH's page @p path, under the section @p section (the SEPARATOR_TEXT the row sits under, ""
 * when the page has none above it; kHostedMmBlockLabel under AddHostedMmBlock). Wraps SohMenu::AddWidget and records
 * the row in the registry; bind it with the handle's .CVar() or .Accessor().
 */
HostedMmWidget AddHostedMmWidget(SohMenu& menu, WidgetPath& path, const char* section, const std::string& name,
                                 WidgetType type);

/** Draw the "Majora's Mask" block heading in SoH's SEPARATOR_TEXT style, for the rows with no twin on the page. */
WidgetInfo& AddHostedMmBlock(SohMenu& menu, WidgetPath& path);

/** Every row added through the seam, in first-registration order. */
const std::vector<HostedMmRow>& HostedMmRows();

/** Is the row @p name on @p header / @p sidebar one the seam added? (The harness's per-row lint and R8 dump.) */
bool IsHostedMmRow(const std::string& header, const std::string& sidebar, const std::string& name);

} // namespace SohGui
