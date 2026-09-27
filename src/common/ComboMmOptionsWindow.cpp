/**
 * @file ComboMmOptionsWindow.cpp
 * @brief Renders MM's randomizer option set in-game (#497 step 4, #499).
 *
 * See ComboMmOptionsWindow.h for the contract. Every value drawn here comes
 * from combo_mm_options_view.h; this file holds no state of its own and caches
 * nothing, so a CVar changed anywhere else shows up on the next frame.
 *
 * DRAWN WITH SoH's WIDGETS (UI parity M6; docs/ui-style-guide.md section 10).
 * Every row goes through the `combo_ui` seam (combo_ui.h), which the shipped
 * binary implements with SoH's own helpers (games/oot/soh/SohGui/ComboUiSoh.cpp),
 * so the pane reads as one of SoH's randomizer option pages: themed widgets,
 * combobox and slider labels above, a SeparatorText per group, gray notes for
 * state, one orange sentence per warning, and every disabled row's reason in its
 * disabled tooltip in SoH's shape rather than printed beside it. The Tricks
 * section below the groups follows SoH's Tricks page (area tree nodes; the
 * control, the tag chips, then the name) through the same seam (M6 part 2).
 */

#include "ComboMmOptionsWindow.h"

#include <cfloat>
#include <cstdio>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "ComboSettingsWindow.h" // Combo_ComboSettingsWindow_Init — the tier-4 twin of this pane
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h" // the per-trick table (#578 part 1)
#include "combo_ui.h"             // SoH's widgets, without an SoH header (UI parity M6)
#include "context.h"              // Context_GetCurrentGame — for the "MM is suspended" state

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

/**
 * The frozen state's disabled reason, in SoH's reason style (a short Title Case
 * fragment, "Save Not Loaded"). The same words SohMenu gives its FROZEN
 * presentation (SohMenu::PresentationLabel) and the Combo > Cross-Game Rules
 * rows carry, so the paired world's two authoring surfaces say one thing.
 */
constexpr const char* kFrozenReason = "Already Decided";

/**
 * The state note: one gray note at the top of the pane, SoH's way of making a
 * state legible without hovering (docs/ui-style-guide.md R-S3, R-X1), and the
 * shape Combo > Cross-Game Rules' status row already has.
 *
 * Three states, three different facts:
 *  - FROZEN (ADR 0004 §6's fourth state, #564 V25): the profile was stamped into
 *    the world's identity at creation. The note names the ACTUAL escape, which
 *    is not obvious and is not "create a new paired world": the stamp lands at
 *    generation, so re-generating re-stamps the same profile, and the only
 *    unfreezing events are Context_InvalidateSessionState's drop paths, the
 *    title screen first among them. It prints NO fingerprint: the one number a
 *    player is shown for the world is Combo > Cross-Game Rules' (the whole
 *    pair's comboSettingsHash, which folds this profile's digest in), and a
 *    second "fingerprint" here, the MM profile digest alone, would name the
 *    same world with a different number.
 *  - PAIRED, NOT FROZEN (#564 V8): a legacy pre-freeze pair, whose profile
 *    freezes at its first crossing.
 *  - UNPAIRED: no world yet; these freeze into the next one at generation.
 */
void DrawStatusNote(bool frozen) {
    ComboMMProfileSummary summary;
    Combo_MMProfileSummary(&summary);

    if (frozen) {
        Ui().NoteText("Already decided when this world was created. Return to the title screen to choose options "
                      "for a new world.");
    } else if (summary.paired) {
        Ui().NoteText("Your paired world predates saved Majora's Mask options. These are saved into it when you "
                      "first cross into Majora's Mask.");
    } else {
        // "Not paired" and "paired with a default profile" are different facts:
        // a seedless header would describe a world that does not exist.
        Ui().NoteText("No paired world yet. These options are saved into the next paired world when it is "
                      "generated.");
    }

    // ADR 0004 §6's third state, editable but not the running game. Not shown
    // once frozen, where "editable" would be false.
    if (!frozen && Context_GetCurrentGame() != GAME_MM) {
        Ui().NoteText("Majora's Mask is suspended; these options stay editable.");
    }
}

/**
 * The pane-wide warnings, one orange sentence each (SoH's warning colour).
 *
 * Pane-wide because their causes are: they are properties of the paired
 * generation pipeline, not of any one option. Repeating them 47 times as per-row
 * reasons would bury the per-row reasons that ARE specific.
 *
 * (1) The timing contract (#498/#564): the profile freezes into the paired
 * world's identity at the CREATION event, and after that a divergent arrival is
 * refused, never honored. Not shown once frozen: the state note says it.
 * (2) The silent vanilla revert: a paired generation that throws reverts the MM
 * file to vanilla with no error surface, and logic mode is the setting most
 * likely to cause it, so the warning lives next to the options.
 *
 * A third standing warning used to live here (the dormant cycle-save hooks) and
 * was retired by #514. The pane states current hazards; a resolved one is absent.
 */
void DrawStandingWarnings(bool frozen) {
    if (!frozen) {
        Ui().WarningText("Generating a paired world locks these options in, and a crossing whose options differ "
                         "is refused.");
    }
    Ui().WarningText("If these options make generation fail, Majora's Mask falls back to a vanilla file; "
                     "Glitchless logic is the most likely cause.");
}

/** True when the row must be drawn disabled with its reason (ADR 0004 §5). */
bool IsCapabilityBlocked(const ComboMMOptionDesc* desc) {
    return desc->liveness == COMBO_MM_LIVENESS_PARTIAL || desc->liveness == COMBO_MM_LIVENESS_DORMANT;
}

/**
 * One option row, through the seam.
 *
 * Disabled for either of two causes, and the reason goes in the DISABLED
 * TOOLTIP in SoH's shape ("This setting is disabled because:" then "- <reason>"
 * per cause), never beside the row: a capability block (ADR 0004 §5, the model's
 * own reason string) and the freeze. A row that is both lists both. The freeze is
 * also stated without hovering, once, by the state note (R-S3).
 */
void DrawOptionRow(const ComboMMOptionDesc* desc, bool frozen) {
    const bool blocked = IsCapabilityBlocked(desc);
    ComboUiWidgetOpts opts;
    opts.tooltip = desc->tooltip;
    opts.disabled = blocked || frozen;
    opts.disabledTooltip =
        ComboUi_DisabledTooltip(frozen ? kFrozenReason : nullptr, blocked ? desc->disabledReason : nullptr);

    int32_t value = Combo_MMOptionGetValue(desc);

    switch ((ComboMMOptionWidget)desc->widget) {
        case COMBO_MM_WIDGET_CHECKBOX: {
            bool on = value != 0;
            if (Ui().Checkbox(desc->label, &on, &opts)) {
                Combo_MMOptionSetValue(desc, on ? 1 : 0);
            }
            break;
        }
        case COMBO_MM_WIDGET_COMBO: {
            // valueCount is asserted non-zero for every combo row by the
            // MMRandoOptions lock; the seam still shows an index it has no label
            // for as "Unknown (N)" rather than indexing out of bounds.
            if (Ui().Combobox(desc->label, &value, desc->valueLabels, (int)desc->valueCount, &opts)) {
                Combo_MMOptionSetValue(desc, value);
            }
            break;
        }
        case COMBO_MM_WIDGET_SLIDER: {
            if (Ui().SliderInt(desc->label, &value, desc->minValue, desc->maxValue, "%d", &opts)) {
                Combo_MMOptionSetValue(desc, value);
            }
            break;
        }
        case COMBO_MM_WIDGET_TIME: {
            // Minutes since midnight, shown as the clock time the player sees.
            // The text is the slider's whole format (it holds no conversion).
            char clock[32];
            snprintf(clock, sizeof(clock), "%02d:%02d", (int)value / 60, (int)value % 60);
            if (Ui().SliderInt(desc->label, &value, desc->minValue, desc->maxValue, clock, &opts)) {
                Combo_MMOptionSetValue(desc, value);
            }
            break;
        }
        default: {
            char note[160];
            snprintf(note, sizeof(note), "%s cannot be shown in this build.", desc->label);
            Ui().NoteText(note);
            break;
        }
    }
}

/**
 * One trick row, in SoH's trick-list shape (DrawTricksMenu, SohMenuRandomizer.cpp):
 * the control, the tag chips with the difficulty rung first, then the name, with
 * the description as the row's tooltip. The control is the themed checkbox the
 * option rows above use rather than DrawTricksMenu's move-to-the-other-column
 * arrow: this pane is one column of settings, and a trick is one more setting in
 * it (docs/ui-style-guide.md section 10, "Trick lists").
 *
 * Disabled the way an option row is, with the reason in the DISABLED TOOLTIP in
 * SoH's shape and never printed beside the row: the table's own reason ("Needs
 * Hover Boots From Ocarina of Time" for a reserved row, "Not Yet Supported by
 * Logic" for an unbound one; combo_mm_tricks_view.h) and the freeze. Both causes
 * matter to say: a trick that silently did nothing would be the vacuous gate
 * this pane exists to refuse (ADR 0004 §5). The whole row, name included, shows
 * that tooltip on hover.
 */
void DrawTrickRow(const ComboMMTrickDesc* desc, bool frozen) {
    const bool blocked = desc->disabledReason != nullptr && desc->disabledReason[0] != '\0';
    ComboUiWidgetOpts opts;
    opts.tooltip = desc->tooltip;
    opts.disabled = blocked || frozen;
    opts.disabledTooltip =
        ComboUi_DisabledTooltip(frozen ? kFrozenReason : nullptr, blocked ? desc->disabledReason : nullptr);

    // No visible label on the box: the chips sit between the control and the
    // name, as in SoH's rows. The key's own name keeps the id unique.
    char id[96];
    snprintf(id, sizeof(id), "##%s", desc->name);
    bool on = Combo_MMTrickGetValue(desc);
    if (Ui().Checkbox(id, &on, &opts)) {
        Combo_MMTrickSetValue(desc, on);
    }
    for (int c = 0; c < (int)desc->chipCount && c < COMBO_MM_TRICK_MAX_CHIPS; c++) {
        Ui().TagChip(desc->chipLabels[c], desc->chipTones[c], &opts);
    }
    Ui().RowText(desc->label, &opts);
}

/**
 * The Tricks section: a SeparatorText like every option group above it, one gray
 * note, then SoH's area tree nodes (DrawTricksMenu's grouping).
 *
 * The areas start closed, unlike SoH's Tricks page, which opens them: that page
 * holds nothing but tricks, and here 86 open rows would sit between the options
 * and the Reset button. Frozen disables every row (each with the freeze as its
 * reason, like the option rows) but never the tree nodes: a frozen trick set is
 * worth reading, so its areas still open.
 */
void DrawTricksSection(bool frozen) {
    Ui().SeparatorText("Tricks");
    const int count = Combo_MMTrickCount();
    if (count == 0) {
        // Distinct from "MM has no tricks": say the table is missing.
        Ui().NoteText("The Majora's Mask trick table is not available in this build.");
        return;
    }

    int settable = 0;
    for (int i = 0; i < count; i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc != nullptr && desc->bound && !desc->reserved) {
            settable++;
        }
    }
    // The honest headline. Saying "86 tricks" and drawing 60 dead checkboxes
    // would be the overclaim; naming the split is what makes the dead ones read
    // as not yet supported rather than broken.
    char note[200];
    snprintf(note, sizeof(note),
             "%d of %d tricks are supported by the randomizer logic so far. The others are shown, but cannot be "
             "turned on yet.",
             settable, count);
    Ui().NoteText(note);

    ImGui::PushID("tricks");
    // Grouped by area, in area-enum order, skipping empty areas — same rule the
    // option groups follow, for the same reason (an always-empty section implies
    // something is missing from it). The bound is the `uint8_t` field's range
    // rather than MMRTA_MAX: this file must not learn an MM enum (ADR 0009 D3),
    // and the descriptor's own `areaName` supplies the node text. The id stack
    // ("tricks", then the area number) is what the UI snapshot harness's
    // tricks-open state names to open an area.
    for (int area = 0; area < 256; area++) {
        const char* areaName = nullptr;
        for (int i = 0; i < count && areaName == nullptr; i++) {
            const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
            if (desc != nullptr && (int)desc->area == area) {
                areaName = desc->areaName;
            }
        }
        if (areaName == nullptr) {
            continue;
        }
        ImGui::PushID(area);
        if (ImGui::TreeNode(areaName)) {
            for (int i = 0; i < count; i++) {
                const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
                if (desc == nullptr || (int)desc->area != area) {
                    continue;
                }
                ImGui::PushID(i);
                DrawTrickRow(desc, frozen);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

/**
 * Reset's action, run by the confirm's Reset button.
 *
 * Clears the CVars rather than writing the defaults back. The two are NOT the
 * same: ResolvePairedProfile distinguishes "the player chose this" from "nobody
 * ever touched it" via the explicit-value probe, and writing a value equal to the
 * default would make the pin think a choice was made. Tricks reset with the
 * options: a reset that left a trick armed would leave the profile, and so the
 * frozen identity, in a state the button claims it cleared. Under a freeze the
 * writers refuse on their own, so a confirm answered after a freeze landed
 * changes nothing.
 */
void ResetAllOptions(void*) {
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        Combo_MMOptionClear(Combo_MMOptionAt(i));
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        Combo_MMTrickClear(Combo_MMTrickAt(i));
    }
}

} // namespace

void ComboMmOptionsWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible(): Ship::GuiWindow reads the CVar exactly once, in its ctor,
    // and only ever writes visibility -> CVar afterwards, so a window with no
    // menu row could otherwise never be opened after registration (#489 cause
    // 1; the spoiler window and MM's check tracker do the same).
    if (!CVarGetInteger(kComboMMOptionsVisibilityCVar, 0)) {
        return;
    }

    // SoH's pane chrome: Ship::GuiWindow::Draw passes its visibility to
    // ImGui::Begin, so every SoH pane (the tracker settings popouts among them)
    // carries a close button in its title bar. Closing clears the visibility
    // CVar through SetVisibility, which also schedules the save, exactly as a
    // closed SoH pane does.
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(kComboMMOptionsMinWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin(kComboMMOptionsWindowName, &open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        DrawElement();
    }
    ImGui::End();
    if (!open) {
        SetVisibility(false);
    }
}

void ComboMmOptionsWindow::DrawElement() {
    const int count = Combo_MMOptionCount();
    if (count == 0) {
        // The MM descriptor table did not register. Say so rather than
        // rendering an empty pane, which would read as "MM has no options".
        Ui().NoteText("The Majora's Mask option table is not available in this build.");
        return;
    }

    // Read once per frame so every widget below agrees with the note: the
    // writers reject on their own (the pane is not the gate, just its face).
    const bool frozen = Combo_MMProfileFrozen();

    DrawStatusNote(frozen);
    DrawStandingWarnings(frozen);

    // Grouped in MM's own taxonomy rather than in id order: id order is the
    // enum's, which interleaves access conditions with hints with shuffles. A
    // SeparatorText per group, as SoH's randomizer option pages do
    // (option.cpp's OptionGroup::AddWidgets), rather than a collapsing header:
    // a settings surface shows its settings.
    for (uint8_t group = 0; group < (uint8_t)COMBO_MM_GROUP_COUNT; group++) {
        // Count first so an empty group draws no header at all — a permanently
        // empty section implies options are missing from it.
        int inGroup = 0;
        for (int i = 0; i < count; i++) {
            const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
            if (desc != NULL && desc->group == group) {
                inGroup++;
            }
        }
        if (inGroup == 0) {
            continue;
        }

        Ui().SeparatorText(Combo_MMOptionGroupName(group));
        ImGui::PushID((int)group);
        for (int i = 0; i < count; i++) {
            const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
            if (desc == NULL || desc->group != group) {
                continue;
            }
            ImGui::PushID(i);
            DrawOptionRow(desc, frozen);
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    // After the option groups: tricks are a different id space and a different
    // save array, so they are a section of their own rather than a tenth group
    // (combo_mm_tricks_view.h explains why the tables are separate).
    DrawTricksSection(frozen);

    // The Reset button is a 47-write batch plus the tricks. SoH's primary-action
    // width, and SoH's confirm-first rule for a destructive button (R-S5). Under a
    // frozen profile it is disabled with the freeze as its reason: a live Reset
    // there would be a control that (correctly) does nothing, ADR 0004 §5's
    // vacuous gate.
    Ui().Spacer(0.0f);
    ComboUiWidgetOpts resetOpts;
    resetOpts.tooltip = "Resets every Majora's Mask randomizer option and trick to its default value.";
    resetOpts.disabled = frozen;
    resetOpts.disabledTooltip = ComboUi_DisabledTooltip(frozen ? kFrozenReason : nullptr, nullptr);
    if (Ui().Button("Reset All to Defaults", COMBO_UI_WIDTH_PRIMARY, &resetOpts) && !frozen) {
        Combo_MMOptionsRequestReset();
    }
}

void RegisterComboMmOptionsWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: the production entry point may be reached
    // from more than one bring-up path, and AddGuiWindow rejects duplicates
    // silently rather than loudly.
    if (gui->GetGuiWindow(kComboMMOptionsWindowName) != nullptr) {
        return;
    }

    gui->AddGuiWindow(std::make_shared<ComboMmOptionsWindow>(kComboMMOptionsVisibilityCVar, kComboMMOptionsWindowName));
}

} // namespace ComboGui

extern "C" void Combo_MMOptionsRequestReset(void) {
    // SoH's confirm shape ("Clear Config", SohMenuSettings.cpp): a title, what
    // the action does, "Continue?", the action's verb, Cancel.
    ComboUi_Get()->Confirm(ComboGui::kComboMMOptionsResetTitle,
                           "This will reset every Majora's Mask randomizer option and trick to its default value.\n"
                           "Continue?",
                           "Reset", "Cancel", ComboGui::ResetAllOptions, nullptr);
}

extern "C" void Combo_MMOptionsWindow_Init(void) {
    // Publish MM's descriptor table first. Done here rather than from a
    // file-scope registrar in the MM TU because that table is derived from a
    // std::map in another translation unit — see MM_RandoOptionsUi_Register.
    // This runs unconditionally, even in the headless case below, so the model
    // is populated for tests that never construct a Gui.
    MM_RandoOptionsUi_Register();
    // The trick table, published the same way and for the same reason (#578
    // part 1): it is derived from another TU's namespace-scope std::map, so it
    // cannot be a file-scope registrar. Also unconditional, so the headless
    // harness has the model populated without a Gui.
    MM_RandoTricksUi_Register();

    // The paired world's OTHER authoring pane — the five tier-4 combo settings
    // (ADR 0011 increment 2) — registers alongside this one. The two are the
    // two halves of one authoring surface: one creation event freezes MM's
    // tier-3 profile and the combo's tier-4 rules together (decision 4.1's
    // order), so they share a bring-up point. Called BEFORE this function's own
    // headless early-out because it carries its own (and is idempotent, so
    // rsbs/src/main.cpp may also call it directly).
    Combo_ComboSettingsWindow_Init();

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    ComboGui::RegisterComboMmOptionsWindow(gui);
}
