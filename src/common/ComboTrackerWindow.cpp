/**
 * @file ComboTrackerWindow.cpp
 * @brief Renders the combo tracker model in-game (#458; ADR 0008).
 *
 * See ComboTrackerWindow.h for the contract. Every value drawn here comes
 * from combo_tracker_view.h; this file holds no state of its own and caches
 * nothing, so progress made mid-session updates on the next frame.
 *
 * It is drawn the way SoH draws its own tracker and editor panes
 * (docs/ui-style-guide.md section 10): the title bar's close button, themed
 * collapsing headers (UIWidgets::PushStyleCombobox(THEME_COLOR), as the Check
 * Tracker Settings pane's section headers are), gray notes for state, and
 * SoH's table shape (cell padding 8x8, horizontal and vertical borders, a
 * header row). The header row stays in ImGui's table-header gray, as in the
 * Check Tracker Settings pane: the theme's Header / HeaderHovered /
 * HeaderActive colours reach a table's header row only while it is hovered or
 * clicked. A check's status is a FontAwesome glyph, a
 * project convention (SoH's check tracker marks status by row colour instead),
 * and it is the ONE notation for it: the crossing tables lead each host check's
 * name with the same glyph the Checks lists use.
 * src/common cannot include UIWidgets, so every styled element goes through the
 * combo_ui seam.
 */

#include "ComboTrackerWindow.h"

#include <cfloat>
#include <cstdio>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_item_view.h" // OoT_ItemAdapter_Register (#458 U1a)
#include "combo_tracker_view.h"
#include "combo_ui.h"
#include "context.h" // GameId

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

/**
 * A check's status as a FontAwesome glyph (a project convention, style guide
 * section 10: SoH's check tracker colours the row instead, and uses glyphs only
 * on its skip/lock buttons): a ticked box when collected, a box with a minus
 * when the player skipped it, an empty box otherwise.
 */
const char* CheckGlyph(const ComboTrackerCheckRow& row) {
    if (row.obtained) {
        return ICON_FA_CHECK_SQUARE_O;
    }
    return row.skipped ? ICON_FA_MINUS_SQUARE_O : ICON_FA_SQUARE_O;
}

/** A crossing's found state in the Checks lists' notation (CheckGlyph). */
const char* FoundGlyph(uint8_t found) {
    if (found == COMBO_TRACKER_FOUND_YES) {
        return ICON_FA_CHECK_SQUARE_O;
    }
    return found == COMBO_TRACKER_FOUND_NO ? ICON_FA_SQUARE_O : ICON_FA_QUESTION_CIRCLE_O;
}

/**
 * Wrapped text with balanced lines: the narrowest wrap width that still takes
 * no more lines than the cell's full width does. A long name then breaks into
 * even lines instead of leaving its last word alone on one ("Stone Tower
 * Temple Entrance Small Crate" over "02"). Text that fits is drawn as is.
 */
void TextBalanced(const char* text) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float full = ImGui::CalcTextSize(text).x;
    float wrap = avail;
    if (avail > 0.0f && full > avail) {
        const float height = ImGui::CalcTextSize(text, nullptr, false, avail).y;
        float lo = 1.0f;
        float hi = avail;
        for (int i = 0; i < 12 && hi - lo > 1.0f; i++) {
            const float mid = (lo + hi) * 0.5f;
            if (ImGui::CalcTextSize(text, nullptr, false, mid).y <= height) {
                hi = mid;
            } else {
                lo = mid;
            }
        }
        wrap = hi;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

/**
 * One game's panel: freshness note, summary lines, and a default-closed
 * per-check list. Fed ONLY by that game's adapter through the view — the
 * game argument is the origin tag, and nothing here compares ids across
 * panels (ADR 0002).
 */
void DrawGamePanel(uint8_t game, const char* title, const ComboTrackerIdentity& identity) {
    if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::PushID((int)game);

    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary(game, &summary);

    if (summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE) {
        // "No data" is NOT "zero progress": an MM shadow that was never
        // written and a never-created OoT heap context land here, and an
        // empty check list under a 0/0 counter would misreport them.
        Ui().NoteText(game == (uint8_t)GAME_MM
                          ? "No data yet. Majora's Mask has not been played this session, and no save holding its "
                            "progress is loaded."
                          : "No data yet. Ocarina of Time has not started this session.");
        ImGui::PopID();
        return;
    }

    if (!summary.hasWorld) {
        // One note, not two: how fresh the data is says nothing without a world.
        Ui().NoteText("Not a randomized world.");
        ImGui::PopID();
        return;
    }

    char note[96];
    snprintf(note, sizeof(note), "%s.", Combo_TrackerFreshnessLabel(game, summary.freshness));
    Ui().NoteText(note);

    // The pane's top line already prints the paired seed; a game whose own
    // seed is that same number would only repeat it (MM's final seed IS the
    // paired seed), so the panel prints its seed only when it says something new.
    if (!identity.paired || summary.seed != identity.sharedRandoSeed) {
        ImGui::Text("Seed: %u", (unsigned)summary.seed);
    }
    ImGui::Text("Checks: %d / %d", summary.obtained, summary.shuffled);
    if (summary.skipped > 0) {
        ImGui::Text("Skipped: %d", summary.skipped);
    }

    // Default-closed so the (potentially long) walk only runs when asked for.
    if (ImGui::TreeNode("Checks")) {
        const int count = Combo_TrackerCheckCount(game);
        for (int i = 0; i < count; i++) {
            ComboTrackerCheckRow row;
            if (!Combo_TrackerCheckAt(game, i, &row)) {
                break;
            }
            if (!row.shuffled) {
                continue;
            }
            if (row.name != nullptr) {
                ImGui::Text("%s %s", CheckGlyph(row), row.name);
            } else {
                // No name table loaded (e.g. OoT static data before OoT's
                // first boot): the game-local id is still an honest label.
                ImGui::Text("%s Check 0x%04X", CheckGlyph(row), (unsigned)row.checkId);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

} // namespace

/**
 * One direction's crossing table (declared in ComboTrackerWindow.h). The
 * direction is the accessor — the two directions are separate key spaces and
 * are never merged (ADR 0009). SoH's table shape (SohMenuRandomizer.cpp's
 * location tables; the Check Tracker Settings pane's table): cell padding 8x8,
 * horizontal and vertical borders, a header row. The header is a short Title
 * Case name (R-N1) with no count in it (R-N4); the gray note under it says how
 * many, which way, and how many of their checks were collected.
 */
void DrawCrossingList(uint8_t hostGame) {
    const bool inMM = hostGame == (uint8_t)GAME_MM;
    const char* title = inMM ? "In MM Checks" : "In OoT Checks";
    const char* itemGame = inMM ? "Ocarina of Time" : "Majora's Mask";
    const char* hostName = inMM ? "Majora's Mask" : "Ocarina of Time";

    ComboTrackerForeignProgress progress;
    Combo_TrackerForeignProgress(hostGame, &progress);
    const int count = progress.total;
    Ui().SeparatorText(title);
    char note[224];
    if (count == 0) {
        snprintf(note, sizeof(note), "No %s items were placed in %s checks.", itemGame, hostName);
        Ui().NoteText(note);
        return;
    }
    // The second sentence is the host game's own save: how many of these checks
    // it has collected, and, when that data is not live, as of when (the same
    // wording the game panel above prints, lower-cased to run on). With nothing
    // to read from the host game, no collected count is claimed at all.
    int len = snprintf(note, sizeof(note), "%d %s %s placed in %s checks.", count, itemGame,
                       count == 1 ? "item was" : "items were", hostName);
    if (progress.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE && len > 0 && len < (int)sizeof(note)) {
        if (progress.freshness == COMBO_TRACKER_FRESH_LIVE) {
            snprintf(note + len, sizeof(note) - (size_t)len, " %d collected.", progress.found);
        } else {
            char when[96];
            snprintf(when, sizeof(when), "%s", Combo_TrackerFreshnessLabel(hostGame, progress.freshness));
            if (when[0] >= 'A' && when[0] <= 'Z') {
                when[0] = (char)(when[0] - 'A' + 'a');
            }
            snprintf(note + len, sizeof(note) - (size_t)len, " %d collected, %s.", progress.found, when);
        }
    }
    Ui().NoteText(note);
    ImGui::PushID((int)hostGame);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
    if (ImGui::BeginTable("##Crossings", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        // Check names run longer than item names ("Stone Tower Temple ..."), so the
        // check column takes the larger share; both wrap in balanced lines rather
        // than clip or orphan a word. There is no third column for the collected state: the
        // found state is the glyph leading the check's name, the one notation
        // the per-game Checks lists above use for the same fact.
        ImGui::TableSetupColumn("Check", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; i++) {
            ComboTrackerForeignRow row;
            if (!Combo_TrackerForeignRowAt(hostGame, i, &row)) {
                break;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // The glyph, then the name beside it; a wrapped name's later lines
            // start under its first word, not under the glyph.
            ImGui::TextUnformatted(FoundGlyph(row.found));
            ImGui::SameLine();
            if (row.hostCheckName != nullptr) {
                TextBalanced(row.hostCheckName);
            } else {
                // No name table for the host game (its adapter is not
                // registered): the game-local id is still an honest label.
                char id[24];
                snprintf(id, sizeof(id), "Check 0x%04X", (unsigned)row.hostCheckId);
                TextBalanced(id);
            }
            ImGui::TableNextColumn();
            // The bare display name, as SoH's own item tables print one; the
            // row's article is for a sentence, and a table cell is not one.
            TextBalanced(row.itemName);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
}

void BeginComboPaneFit(ComboPaneFit& fit) {
    ImGui::SetNextWindowSize(ImVec2(kComboPaneWidth, kComboPaneHeight), ImGuiCond_FirstUseEver);
    if (fit.growTo > 0.0f && fit.width > 0.0f) {
        ImGui::SetNextWindowSize(ImVec2(fit.width, fit.growTo), ImGuiCond_Always);
        fit.growTo = 0.0f;
    }
    if (fit.contentHeight > 0.0f) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, fit.contentHeight));
    }
}

void EndComboPaneFit(ComboPaneFit& fit) {
    // Window-local cursor Y includes the title bar and the scroll, so after the
    // last item it is the height the whole pane needs, less the trailing item
    // spacing and plus the bottom padding.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float content = ImGui::GetCursorPosY() - style.ItemSpacing.y + style.WindowPadding.y;
    const ImVec2 size = ImGui::GetWindowSize();
    const bool fitted = fit.contentHeight > 0.0f && size.y >= fit.contentHeight - 0.5f;
    if (fitted && content > fit.contentHeight + 0.5f && size.y < kComboPaneHeight) {
        fit.growTo = content < kComboPaneHeight ? content : kComboPaneHeight;
    }
    fit.contentHeight = content;
    fit.width = size.x;
    fit.height = size.y;
}

void ComboTrackerWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible() (#489 cause 1; same pattern as ComboSpoilerWindow).
    if (!CVarGetInteger(kComboTrackerVisibilityCVar, 0)) {
        return;
    }

    // SoH's pane chrome (docs/ui-style-guide.md section 10): Ship::GuiWindow::Draw
    // passes its visibility to ImGui::Begin, so an SoH pane has a close button.
    // Closing clears the visibility CVar through SetVisibility, which also
    // schedules the save, as a closed SoH pane does.
    bool open = true;
    BeginComboPaneFit(mFit);
    if (ImGui::Begin(kComboTrackerWindowName, &open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        DrawElement();
        EndComboPaneFit(mFit);
    }
    ImGui::End();
    if (!open) {
        SetVisibility(false);
    }
}

void ComboTrackerWindow::DrawElement() {
    // Identity header. Unlike the spoiler, the per-game panels are NOT gated
    // on pairing — a solo OoT rando session has progress worth showing — so
    // an unpaired world gets one honest line, not an early return.
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);
    if (identity.paired) {
        // "Paired Seed", not "Seed": each game panel below may print its own
        // seed, and the two must not read as the same line.
        ImGui::Text("Paired Seed: %u", (unsigned)identity.sharedRandoSeed);
    } else {
        Ui().NoteText("No paired world.");
    }
    Ui().Spacer(0.0f);

    Ui().PushTheme();
    DrawGamePanel((uint8_t)GAME_OOT, "Ocarina of Time", identity);
    DrawGamePanel((uint8_t)GAME_MM, "Majora's Mask", identity);

    // "Crossings": the word the notes, the spoiler JSON (combo.crossingStore) and
    // the docs use for these rows.
    if (identity.paired && ImGui::CollapsingHeader("Crossings", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawCrossingList((uint8_t)GAME_MM);
        Ui().Spacer(0.0f);
        DrawCrossingList((uint8_t)GAME_OOT);
    }
    Ui().PopTheme();
}

void RegisterComboTrackerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: the production entry point may be reached
    // from more than one bring-up path, and AddGuiWindow rejects duplicates
    // silently rather than loudly.
    if (gui->GetGuiWindow(kComboTrackerWindowName) != nullptr) {
        return;
    }

    gui->AddGuiWindow(std::make_shared<ComboTrackerWindow>(kComboTrackerVisibilityCVar, kComboTrackerWindowName));
}

} // namespace ComboGui

extern "C" void Combo_TrackerWindow_Init(void) {
    // Register both adapters first, unconditionally — even in the headless
    // case below — so the model is populated for tests that never construct a
    // Gui. Explicit calls, not
    // file-scope registrars: the MM half reads std::maps in other TUs whose
    // static init order is unspecified, and a call site cannot be link-elided
    // the way an unreferenced registrar can (#516's dead-registrar class).
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    // The unified item view's OoT adapter (#458 U1a), registered with the check
    // adapters so the view has it from the same bring-up.
    OoT_ItemAdapter_Register();

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    ComboGui::RegisterComboTrackerWindow(gui);
}
