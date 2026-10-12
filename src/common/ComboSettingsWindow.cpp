/**
 * @file ComboSettingsWindow.cpp
 * @brief Renders the tier-4 combo settings (ADR 0011 increment 2; the two
 *        pool sizes retired by #801 and the two item-class masks by #834).
 *
 * See ComboSettingsWindow.h for the contract. Every value drawn here comes
 * from combo_settings_view.h (pre-creation) or Combo_ComboSettingsSummary
 * (post-creation); this file holds no state of its own and caches nothing,
 * so a CVar changed anywhere else shows up on the next frame and a freeze
 * flips the pane read-only on the frame it lands.
 */

#include "ComboSettingsWindow.h"

#include <cctype>
#include <cstdio>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_mm_options_view.h" // Combo_MMProfileSummary: the paired seed, for the header
#include "combo_settings_view.h"
#include "context.h"
#include "foreign_items.h"

namespace ComboGui {

namespace {

const ImVec4 kWarnColor(0.98f, 0.76f, 0.24f, 1.0f);
const ImVec4 kDecidedColor(0.55f, 0.78f, 0.98f, 1.0f);

// THE REASON STRING for ADR 0004 §6 state 4 comes from the MODEL
// (Combo_ComboSettingReadOnlyReason, combo_settings_view.h) and is not spelled
// here: it is "already decided" and not a capability reason -- a capability
// gate says "not yet available", a freeze says "already chosen", and a player
// who reads the wrong one goes looking for a bug in the wrong place. Owning it
// model-side is what makes that requirement assertable (combo-settings-
// authoring locks the string and both of its states); a literal held only by
// the renderer could only be verified by looking at the window.

/** The inline tag every read-only row carries beside its widget (ADR 0004 §5:
 *  the cause must be legible without hovering). */
void DecidedTag() {
    const char* reason = Combo_ComboSettingReadOnlyReason();
    ImGui::SameLine();
    ImGui::TextDisabled("- %s", reason != nullptr ? reason : "");
}

/** The state-4 heading: the same model string, sentence-cased. One spelling of
 *  the reason, in one place, however it is presented. */
void DecidedHeading() {
    const char* reason = Combo_ComboSettingReadOnlyReason();
    if (reason == nullptr || reason[0] == '\0') {
        return;
    }
    char heading[64];
    snprintf(heading, sizeof(heading), "%s", reason);
    heading[0] = (char)toupper((unsigned char)heading[0]);
    ImGui::TextColored(kDecidedColor, "%s", heading);
}

/** The pairing header: which world these rules describe, if any. */
void DrawPairingSummary(const ComboSettingsSummary& summary) {
    if (!summary.paired) {
        // "Not paired" and "paired under the defaults" are different facts. A
        // seedless header would tell the player these rules belong to a world
        // that does not exist.
        ImGui::TextUnformatted("No paired world.");
        ImGui::Separator();
        return;
    }

    ComboMMProfileSummary profile;
    Combo_MMProfileSummary(&profile);
    ImGui::Text("Paired seed: %u", (unsigned)profile.sharedRandoSeed);
    if (summary.frozen) {
        ImGui::Text("Rules fingerprint: %08X", (unsigned)summary.comboSettingsHash);
    } else {
        // formatVersion == 0 on a PAIRED world means a legacy pre-carve pair
        // (ADR 0011 decision 4.2): the shipped defaults freeze at its first
        // crossing (accepted answer O5). For a post-carve created file this
        // state is unreachable, so it is named rather than rendered as a
        // benign default.
        ImGui::TextUnformatted("Rules: not frozen");
    }
    ImGui::Separator();
}

/**
 * The FOURTH presentation state (ADR 0004 §6 state 4): frozen-at-creation,
 * read-only, with the reason stated where the rows are: the model's heading
 * here and the same words beside every row (DecidedTag). No paragraph of
 * mechanism under it (#927): the Cross-Game Rules page's state note says
 * what a player can do about it.
 */
void DrawDecidedBanner(const ComboSettingsSummary& summary) {
    DecidedHeading();
    if (!summary.paired) {
        // A frozen record with no live pairing is a state no created combo
        // file may be in (decision 4.2). Say so rather than render zeros as
        // rules.
        ImGui::TextColored(kWarnColor, "Rules are frozen but no paired world is live — corrupt session state.");
    }
    ImGui::Separator();
}

/**
 * Pre-creation: what these are, in the Cross-Game Rules page's own words (its
 * state note), so "editable" is not mistaken for "in effect now". One sentence,
 * no paragraph of mechanism (#927). A legacy pre-carve pair (paired, not
 * frozen) also gets that page's instruction, because an edit here before its
 * first crossing is not harmless: the player acts on it.
 */
void DrawAuthoringNotice(const ComboSettingsSummary& summary) {
    ImGui::TextUnformatted("These rules apply to the next paired world you create.");
    if (summary.paired && !summary.frozen) {
        ImGui::TextUnformatted("Keep them at the defaults until you have crossed into this paired world once.");
    }
    ImGui::Separator();
}

void DrawDirectionRow(uint8_t direction, bool decided) {
    // Indexed by RSBS_COMBO_DIR_* - 1. The enumerators are pinned 1..4
    // (foreign_items.h's static_assert), so the arithmetic is format, not luck.
    static const char* const kLabels[4] = {
        "Off (paired world, no crossings)",
        "Ocarina of Time items into Majora's Mask only",
        "Majora's Mask items into Ocarina of Time only",
        "Both directions",
    };
    char unknown[32];
    const char* preview = nullptr;
    if (direction >= (uint8_t)RSBS_COMBO_DIR_OFF && direction <= (uint8_t)RSBS_COMBO_DIR_BOTH) {
        preview = kLabels[direction - 1u];
    } else {
        // Only reachable from a frozen record carrying a value this build does
        // not know — shown as what it is rather than as the nearest label.
        snprintf(unknown, sizeof(unknown), "(unknown %u)", (unsigned)direction);
        preview = unknown;
    }

    if (decided) {
        ImGui::BeginDisabled();
    }
    if (ImGui::BeginCombo(Combo_ComboSettingLabel(COMBO_SETTING_DIRECTION), preview)) {
        for (int i = 0; i < 4; i++) {
            const uint8_t value = (uint8_t)(i + 1);
            const bool selected = (value == direction);
            if (ImGui::Selectable(kLabels[i], selected)) {
                Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)value);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (decided) {
        ImGui::EndDisabled();
        DecidedTag();
    }
}

} // namespace

void ComboSettingsWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible(): Ship::GuiWindow reads the CVar exactly once, in its ctor,
    // and only ever writes visibility -> CVar afterwards, so a window with no
    // menu row could otherwise never be opened after registration (#489 cause
    // 1; every sibling common-owned window does the same).
    if (!CVarGetInteger(kComboSettingsVisibilityCVar, 0)) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(kComboSettingsWindowName, nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::End();
        return;
    }

    DrawElement();

    ImGui::End();
}

void ComboSettingsWindow::DrawElement() {
    ComboSettingsSummary summary;
    Combo_ComboSettingsSummary(&summary);

    // Read once per frame so every widget agrees with the banner. The SAME
    // predicate the writers gate on, so the greying and the refusal cannot
    // disagree: the pane is not the gate, just its face.
    const bool decided = Combo_ComboSettingsFrozen();

    // WHICH values to show (ADR 0004 §6 state 4): post-creation, the record
    // FROM THE SAVE through the src/common accessor — never the CVar, which
    // may legitimately differ now and describes no world. Pre-creation, the
    // resolver, i.e. exactly what a creation event would freeze right now.
    ComboSettingsRecord shown;
    if (decided) {
        shown = summary.record;
    } else {
        Combo_ResolveComboSettings(&shown);
    }

    DrawPairingSummary(summary);
    if (decided) {
        DrawDecidedBanner(summary);
    } else {
        DrawAuthoringNotice(summary);
    }

    DrawDirectionRow(shown.direction, decided);

    ImGui::Separator();
    if (decided) {
        // A live Reset under a frozen record would be a control that
        // (correctly) does nothing — ADR 0004 §5's vacuous gate. Disabled, with
        // its cause inline.
        ImGui::BeginDisabled();
        ImGui::Button("Reset all to defaults");
        ImGui::EndDisabled();
        DecidedTag();
    } else if (ImGui::Button("Reset all to defaults")) {
        // Clears rather than writes the defaults back: an unset key and a key
        // explicitly holding the default resolve identically today, but only
        // the cleared one reads as "the player never touched it".
        for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
            Combo_ComboSettingClear((ComboSettingId)i);
        }
    }
}

void RegisterComboSettingsWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: the production entry point may be reached
    // from more than one bring-up path, and AddGuiWindow rejects duplicates
    // silently rather than loudly.
    if (gui->GetGuiWindow(kComboSettingsWindowName) != nullptr) {
        return;
    }

    gui->AddGuiWindow(std::make_shared<ComboSettingsWindow>(kComboSettingsVisibilityCVar, kComboSettingsWindowName));
}

} // namespace ComboGui

extern "C" void Combo_ComboSettingsWindow_Init(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    ComboGui::RegisterComboSettingsWindow(gui);
}
