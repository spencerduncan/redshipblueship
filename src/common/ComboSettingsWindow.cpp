/**
 * @file ComboSettingsWindow.cpp
 * @brief Renders the five tier-4 combo settings (ADR 0011 increment 2).
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
        ImGui::TextWrapped("No paired world yet. These rules freeze into the next paired world's identity when a "
                           "randomized Ocarina of Time world is generated.");
        ImGui::Separator();
        return;
    }

    ComboMMProfileSummary profile;
    Combo_MMProfileSummary(&profile);
    ImGui::Text("Paired seed: %u", (unsigned)profile.sharedRandoSeed);
    if (summary.frozen) {
        ImGui::Text("Rules fingerprint: %08X (frozen at creation)", (unsigned)summary.comboSettingsHash);
    } else {
        // formatVersion == 0 on a PAIRED world means a legacy pre-carve pair
        // (ADR 0011 decision 4.2): the shipped defaults freeze at its first
        // crossing (accepted answer O5). For a post-carve created file this
        // state is unreachable, so it is named rather than rendered as a
        // benign default.
        ImGui::TextUnformatted("Rules: not frozen (this pair predates combo rules; the shipped defaults freeze at "
                               "its first crossing)");
    }
    ImGui::Separator();
}

/**
 * The FOURTH presentation state (ADR 0004 §6 state 4): frozen-at-creation,
 * read-only, with the reason stated where the rows are.
 *
 * The copy names the ACTUAL escape, which is not "create a new paired world":
 * the stamp lands at GENERATION (Playthrough_Init), and the only unfreezing
 * events are the DROP paths in Context_InvalidateSessionState — returning to
 * the title screen, starting a vanilla file, or loading a slot whose .redsave
 * is unfrozen. The same escape the MM options pane names, for the same
 * reason: advice the player cannot act on is ADR 0004 §5's vacuous gate
 * wearing a help string.
 */
void DrawDecidedBanner(const ComboSettingsSummary& summary) {
    DecidedHeading();
    ImGui::TextWrapped("This paired world's cross-game rules were frozen into its identity when the world was "
                       "created. The values below are read from the save, not from the settings store, and are "
                       "shown read-only; changing them is no longer possible for this pair. To play under different "
                       "rules, return to the title screen — these unlock there — then set them and generate a new "
                       "seed.");
    if (!summary.paired) {
        // A frozen record with no live pairing is a state no created combo
        // file may be in (decision 4.2). Say so rather than render zeros as
        // rules.
        ImGui::TextColored(kWarnColor, "Rules are frozen but no paired world is live — corrupt session state.");
    }
    ImGui::Separator();
}

/** Pre-creation: what these are, and — for a legacy pair — what they are NOT. */
void DrawAuthoringNotice(const ComboSettingsSummary& summary) {
    ImGui::TextColored(kWarnColor, "These freeze into the paired world's identity.");
    ImGui::TextWrapped("They govern the crossing between both games and are decided once, when a randomized Ocarina "
                       "of Time world is generated. After that the record in the save is the authority, and a "
                       "crossing or a load whose authored rules no longer match it is refused by name.");
    if (summary.paired && !summary.frozen) {
        ImGui::Spacing();
        ImGui::TextColored(kWarnColor, "The current paired world predates these rules.");
        ImGui::TextWrapped("It was generated when there was only one rule set — the shipped defaults — and those are "
                           "recorded into it at its first crossing. The values below describe the NEXT world you "
                           "create; leave them at the defaults before crossing into this one.");
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

void DrawPoolSizeRow(ComboSettingId id, uint8_t value, bool decided) {
    int v = (int)value;
    if (decided) {
        ImGui::BeginDisabled();
    }
    if (ImGui::SliderInt(Combo_ComboSettingLabel(id), &v, 1, (int)RSBS_FOREIGN_PLACEMENT_CAP)) {
        Combo_ComboSettingSet(id, (int32_t)v);
    }
    if (decided) {
        ImGui::EndDisabled();
        DecidedTag();
    }
}

void DrawItemClassRows(ComboSettingId id, uint16_t mask, bool decided) {
    // The allocated bits, in bit order (foreign_items.h). Appending a class is
    // a new row here; re-pointing one is forbidden there.
    static const uint16_t kClassBits[] = {
        (uint16_t)RSBS_ITEMCLASS_PROGRESSION,    (uint16_t)RSBS_ITEMCLASS_SONGS,
        (uint16_t)RSBS_ITEMCLASS_MASKS,          (uint16_t)RSBS_ITEMCLASS_DUNGEON_ITEMS,
        (uint16_t)RSBS_ITEMCLASS_DUNGEON_REWARD, (uint16_t)RSBS_ITEMCLASS_SIDEQUEST,
    };

    ImGui::TextUnformatted(Combo_ComboSettingLabel(id));
    if (decided) {
        DecidedTag();
        ImGui::BeginDisabled();
    }
    // Two directions draw the same six labels; the ID scope keeps ImGui from
    // conflating the OoT "songs" box with the MM one.
    ImGui::PushID((int)id);
    for (uint16_t bit : kClassBits) {
        bool on = (mask & bit) != 0;
        if (ImGui::Checkbox(Combo_ForeignItemClassName(bit), &on)) {
            const uint16_t next = on ? (uint16_t)(mask | bit) : (uint16_t)(mask & (uint16_t)~bit);
            Combo_ComboSettingSet(id, (int32_t)next);
        }
    }
    ImGui::PopID();
    if (decided) {
        ImGui::EndDisabled();
    }
    if (mask == 0) {
        // A legitimate world (ADR 0011 decision 3.3), but one worth naming: the
        // pass logs "no crossings" and places nothing, and a player who did
        // not mean that would otherwise read it as a broken pool.
        ImGui::TextDisabled("No classes armed: this direction places nothing (the same world as that direction "
                            "being off).");
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
    DrawPoolSizeRow(COMBO_SETTING_POOL_SIZE_OOT, shown.poolSizeOoT, decided);
    DrawPoolSizeRow(COMBO_SETTING_POOL_SIZE_MM, shown.poolSizeMM, decided);
    ImGui::Spacing();
    DrawItemClassRows(COMBO_SETTING_ITEM_CLASS_OOT, shown.itemClassOoT, decided);
    ImGui::Spacing();
    DrawItemClassRows(COMBO_SETTING_ITEM_CLASS_MM, shown.itemClassMM, decided);

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
