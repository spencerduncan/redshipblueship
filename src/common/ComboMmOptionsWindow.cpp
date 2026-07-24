/**
 * @file ComboMmOptionsWindow.cpp
 * @brief Renders MM's randomizer option set in-game (#497 step 4, #499).
 *
 * See ComboMmOptionsWindow.h for the contract. Every value drawn here comes
 * from combo_mm_options_view.h; this file holds no state of its own and caches
 * nothing, so a CVar changed anywhere else shows up on the next frame.
 */

#include "ComboMmOptionsWindow.h"

#include <cstdio>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_mm_options_view.h"
#include "context.h" // Context_GetCurrentGame — for the "MM is suspended" state

namespace ComboGui {

namespace {

/**
 * Draw the pane-wide warnings.
 *
 * These are pane-wide because their causes are: they are properties of the
 * paired-generation pipeline, not of any one option. Repeating them 47 times as
 * per-row reason strings would bury the per-row reasons that ARE specific.
 *
 * A third standing warning used to live here — the dormant cycle-save hooks —
 * and was retired by #514, which gave Before/AfterEndOfCycleSave real dispatch.
 * It is not replaced by a "now fixed" notice: the pane states current hazards,
 * and a resolved one is simply absent.
 */
void DrawStandingWarnings() {
    const ImVec4 warn(0.98f, 0.76f, 0.24f, 1.0f);

    // (1) The timing constraint (#499). The profile is snapshotted when MM's
    // cross-game arrival dispatches OnSaveInit, and an existing MM save is
    // never regenerated. Changing an option after crossing does nothing at
    // all, which is exactly the kind of silent no-op this pane exists to stop
    // being possible.
    ImGui::TextColored(warn, "These apply to the NEXT Majora's Mask file.");
    ImGui::TextWrapped("The paired MM world snapshots these options the first time you cross into Majora's Mask. An "
                       "MM save that already exists is never regenerated, so set them before you cross.");
    ImGui::Spacing();

    // (2) The silent-vanilla-revert hazard. A paired generation that throws
    // reverts the save to vanilla with no retry and no error surface; the
    // player just gets a vanilla MM. Logic mode is the setting most likely to
    // cause it, so the warning lives next to the options rather than in a log.
    ImGui::TextColored(warn, "A failed generation falls back to vanilla Majora's Mask.");
    ImGui::TextWrapped("Paired generation is attempt-free by design. If your settings make the fill dead-end, the MM "
                       "file becomes an ordinary vanilla file rather than reporting an error. Glitchless logic is the "
                       "setting most likely to do this.");
    ImGui::Separator();
}

/** The pairing header: which world these options describe, if any. */
void DrawPairingSummary() {
    ComboMMProfileSummary summary;
    Combo_MMProfileSummary(&summary);

    if (!summary.paired) {
        // "Not paired" and "paired with a default profile" are different facts.
        // Rendering a seedless header would tell the player these options
        // belong to a world that does not exist.
        ImGui::TextWrapped("No paired world yet. Generate a randomized Ocarina of Time world; the Majora's Mask half "
                           "derives from it, using the options below.");
        ImGui::Separator();
        return;
    }

    ImGui::Text("Paired seed: %u", (unsigned)summary.sharedRandoSeed);
    ImGui::Text("OoT settings digest: %08X", (unsigned)summary.sharedRandoSettingsHash);
    if (summary.mmProfileDigest == 0) {
        // Zero is "unset", not "broken": the MM half has not been generated
        // yet, which is the normal state for a player still in OoT.
        ImGui::TextUnformatted("MM profile digest: not resolved yet (no MM file generated)");
    } else {
        ImGui::Text("MM profile digest: %08X", (unsigned)summary.mmProfileDigest);
    }
    ImGui::Separator();
}

/**
 * The third presentation state (ADR 0004 section 6): editable, but not the
 * running game. Distinct from "live" and from "disabled by capability" —
 * collapsing it into either would misdescribe it.
 */
void DrawActiveGameState() {
    if (Context_GetCurrentGame() == GAME_MM) {
        ImGui::TextUnformatted("Majora's Mask - running");
    } else {
        ImGui::TextDisabled("Majora's Mask - suspended (these stay editable)");
    }
    ImGui::Spacing();
}

/** Tooltip helper: MM's own menu attaches these on hover, so this one does too. */
void HoverTooltip(const char* text) {
    if (text != nullptr && text[0] != '\0' && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}

/** True when the row must be drawn disabled with its reason (ADR 0004 §5). */
bool IsCapabilityBlocked(const ComboMMOptionDesc* desc) {
    return desc->liveness == COMBO_MM_LIVENESS_PARTIAL || desc->liveness == COMBO_MM_LIVENESS_DORMANT;
}

void DrawOptionRow(const ComboMMOptionDesc* desc) {
    const bool blocked = IsCapabilityBlocked(desc);

    if (blocked) {
        ImGui::BeginDisabled();
    }

    int32_t value = Combo_MMOptionGetValue(desc);

    switch ((ComboMMOptionWidget)desc->widget) {
        case COMBO_MM_WIDGET_CHECKBOX: {
            bool on = value != 0;
            if (ImGui::Checkbox(desc->label, &on)) {
                Combo_MMOptionSetValue(desc, on ? 1 : 0);
            }
            break;
        }
        case COMBO_MM_WIDGET_COMBO: {
            // valueCount is asserted non-zero for every combo row by the
            // MMRandoOptions lock; the guard is here so a broken table
            // degrades to an unusable row rather than indexing out of bounds.
            const char* preview = "(no values)";
            if (desc->valueLabels != NULL && desc->valueCount > 0 && value >= 0 && value < (int32_t)desc->valueCount) {
                preview = desc->valueLabels[value];
            }
            if (desc->valueLabels != NULL && ImGui::BeginCombo(desc->label, preview)) {
                for (int i = 0; i < (int)desc->valueCount; i++) {
                    const bool selected = (value == i);
                    if (ImGui::Selectable(desc->valueLabels[i], selected)) {
                        Combo_MMOptionSetValue(desc, i);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case COMBO_MM_WIDGET_SLIDER: {
            int v = (int)value;
            if (ImGui::SliderInt(desc->label, &v, (int)desc->minValue, (int)desc->maxValue)) {
                Combo_MMOptionSetValue(desc, (int32_t)v);
            }
            break;
        }
        case COMBO_MM_WIDGET_TIME: {
            // Minutes since midnight, shown as the clock time the player sees.
            int v = (int)value;
            char label[32];
            snprintf(label, sizeof(label), "%02d:%02d", v / 60, v % 60);
            if (ImGui::SliderInt(desc->label, &v, (int)desc->minValue, (int)desc->maxValue, label)) {
                Combo_MMOptionSetValue(desc, (int32_t)v);
            }
            break;
        }
        default:
            ImGui::TextDisabled("%s (unsupported widget)", desc->label);
            break;
    }

    HoverTooltip(desc->tooltip);

    if (blocked) {
        ImGui::EndDisabled();
        // The reason is NOT a tooltip. ADR 0004 §5 requires the explanation to
        // be legible without hovering, because a disabled control with no
        // visible cause reads as a bug in the port.
        ImGui::SameLine();
        ImGui::TextDisabled("- %s", desc->disabledReason);
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

    ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(kComboMMOptionsWindowName, nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::End();
        return;
    }

    DrawElement();

    ImGui::End();
}

void ComboMmOptionsWindow::DrawElement() {
    const int count = Combo_MMOptionCount();
    if (count == 0) {
        // The MM descriptor table did not register. Say so rather than
        // rendering an empty pane, which would read as "MM has no options".
        ImGui::TextWrapped("Majora's Mask option table is not available in this build.");
        return;
    }

    DrawPairingSummary();
    DrawActiveGameState();
    DrawStandingWarnings();

    // Grouped in MM's own taxonomy rather than in id order: id order is the
    // enum's, which interleaves access conditions with hints with shuffles.
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

        if (!ImGui::CollapsingHeader(Combo_MMOptionGroupName(group), ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }
        ImGui::PushID((int)group);
        for (int i = 0; i < count; i++) {
            const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
            if (desc == NULL || desc->group != group) {
                continue;
            }
            ImGui::PushID(i);
            DrawOptionRow(desc);
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Reset all to defaults")) {
        // Clears the CVars rather than writing the defaults back. The two are
        // NOT the same: ResolvePairedProfile distinguishes "the player chose
        // this" from "nobody ever touched it" via CVarExists, and writing a
        // value equal to the default would make the pin think a choice was
        // made.
        for (int i = 0; i < count; i++) {
            Combo_MMOptionClear(Combo_MMOptionAt(i));
        }
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

    gui->AddGuiWindow(
        std::make_shared<ComboMmOptionsWindow>(kComboMMOptionsVisibilityCVar, kComboMMOptionsWindowName));
}

} // namespace ComboGui

extern "C" void Combo_MMOptionsWindow_Init(void) {
    // Publish MM's descriptor table first. Done here rather than from a
    // file-scope registrar in the MM TU because that table is derived from a
    // std::map in another translation unit — see MM_RandoOptionsUi_Register.
    // This runs unconditionally, even in the headless case below, so the model
    // is populated for tests that never construct a Gui.
    MM_RandoOptionsUi_Register();

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
