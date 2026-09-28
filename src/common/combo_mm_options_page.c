/**
 * @file combo_mm_options_page.c
 * @brief The page view model for Combo > MM Randomizer and Combo > MM Tricks.
 *
 * See combo_mm_options_page.h for the contract. Everything here is a decision
 * the retired window used to make inline while it drew; moved out so the pages'
 * locks can drive it with no ImGui context and no SoH header.
 */

#include "combo_mm_options_page.h"

#include "ComboSettingsWindow.h" // Combo_ComboSettingsWindow_Init: the tier-4 twin's bring-up
#include "context.h"             // Context_GetCurrentGame, for the suspended note

#include <stdio.h>

void Combo_MMOptionsPages_Init(void) {
    // Both tables are built MM-side from namespace-scope std::maps in other
    // translation units, which is why they are published by a call and not a
    // file-scope registrar (static initialization order across TUs is
    // unspecified). Both calls are idempotent: they publish one function-local
    // table each, and re-registering the same table is silent.
    MM_RandoOptionsUi_Register();
    MM_RandoTricksUi_Register();
    // The tier-4 combo settings model freezes at the same creation event as
    // this profile, so the two share a bring-up point, as they did when both
    // were windows. Idempotent and headless-safe on its own.
    Combo_ComboSettingsWindow_Init();
}

int Combo_MMOptionsPage_GroupColumn(uint8_t group) {
    switch ((ComboMMOptionGroup)group) {
        case COMBO_MM_GROUP_LOGIC:
        case COMBO_MM_GROUP_SHUFFLE:
            return 0;
        case COMBO_MM_GROUP_ITEMS:
        case COMBO_MM_GROUP_STARTING:
        case COMBO_MM_GROUP_HINTS:
            return 1;
        default:
            return -1;
    }
}

ComboMMRowState Combo_MMOptionsPage_OptionState(const ComboMMOptionDesc* desc, const char** reason) {
    const char* why = "";
    ComboMMRowState state = COMBO_MM_ROW_LIVE;
    if (Combo_MMProfileFrozen()) {
        state = COMBO_MM_ROW_FROZEN;
        why = COMBO_MM_OPTIONS_FROZEN_REASON;
    } else if (desc != NULL &&
               (desc->liveness == COMBO_MM_LIVENESS_PARTIAL || desc->liveness == COMBO_MM_LIVENESS_DORMANT)) {
        // ADR 0004 section 5: the behaviour is absent, so the control would flip
        // a CVar and change nothing. Disabled, with the table's own reason.
        state = COMBO_MM_ROW_BLOCKED;
        why = desc->disabledReason != NULL ? desc->disabledReason : "";
    }
    if (reason != NULL) {
        *reason = why;
    }
    return state;
}

ComboMMRowState Combo_MMOptionsPage_TrickState(const ComboMMTrickDesc* desc, const char** reason) {
    const char* why = "";
    ComboMMRowState state = COMBO_MM_ROW_LIVE;
    if (Combo_MMProfileFrozen()) {
        state = COMBO_MM_ROW_FROZEN;
        why = COMBO_MM_OPTIONS_FROZEN_REASON;
    } else if (desc != NULL && (desc->reserved || !desc->bound)) {
        // The writer refuses these rows too (Combo_MMTrickSetValue); the page
        // says why instead of drawing a control that silently does nothing.
        state = COMBO_MM_ROW_BLOCKED;
        why = desc->disabledReason != NULL ? desc->disabledReason : "";
    }
    if (reason != NULL) {
        *reason = why;
    }
    return state;
}

const char* Combo_MMOptionsPage_StatusNote(void) {
    if (Combo_MMProfileFrozen()) {
        // The stamp lands at generation, so re-generating re-stamps the same
        // profile: the real escape is the title screen, where
        // Context_InvalidateSessionState drops the pair's identity.
        return "Already decided when this world was created. Return to the title screen to choose options for a "
               "new world.";
    }
    ComboMMProfileSummary summary;
    Combo_MMProfileSummary(&summary);
    if (summary.paired) {
        // A legacy pre-freeze pair (#564 V8): its profile freezes at its first crossing.
        return "Your paired world predates saved Majora's Mask options. These are saved into it when you first "
               "cross into Majora's Mask.";
    }
    // No paired file is loaded, so the rows author the NEXT world; loading an
    // existing paired file puts that file's own options back (#781).
    return "These options apply to the next paired world you create. Loading a paired file restores its own "
           "options.";
}

bool Combo_MMOptionsPage_Suspended(void) {
    return !Combo_MMProfileFrozen() && Context_GetCurrentGame() != GAME_MM;
}

const char* Combo_MMOptionsPage_SuspendedNote(void) {
    // Not SohMenu's default INACTIVE_GAME sentence ("...these take effect when
    // you return."): a randomizer option takes effect at generation, not when
    // Majora's Mask resumes, so that sentence would be false here.
    return "Majora's Mask is suspended; these options stay editable.";
}

const char* Combo_MMOptionsPage_FallbackWarning(void) {
    return "If these options make generation fail, Majora's Mask falls back to a vanilla file; Glitchless logic is "
           "the most likely cause.";
}

int Combo_MMOptionsPage_SettableTrickCount(void) {
    int settable = 0;
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc != NULL && desc->bound && !desc->reserved) {
            settable++;
        }
    }
    return settable;
}

const char* Combo_MMOptionsPage_TricksNote(char* buf, size_t size) {
    if (buf == NULL || size == 0) {
        return buf;
    }
    const int count = Combo_MMTrickCount();
    if (count == 0) {
        // Distinct from "MM has no tricks": say the table is missing.
        snprintf(buf, size, "The Majora's Mask trick table is not available in this build.");
    } else if (Combo_MMProfileFrozen()) {
        snprintf(buf, size, "Already decided when this world was created. Return to the title screen to choose "
                            "tricks for a new world.");
    } else {
        // ONE line at the page's width (the reference page has no note at all
        // above its filter): the trick headline, then what a load does (#781).
        // A trick that cannot be turned on says why in its own disabled row.
        snprintf(buf, size,
                 "%d of %d tricks are supported by the randomizer logic so far. Loading a paired file restores its "
                 "own tricks.",
                 Combo_MMOptionsPage_SettableTrickCount(), count);
    }
    return buf;
}

void Combo_MMOptionsPage_ResetAll(void) {
    // Tricks reset with the options: a reset that left a trick armed would leave
    // the profile, and so the frozen identity, in a state the button claims it
    // cleared.
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        Combo_MMOptionClear(Combo_MMOptionAt(i));
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        Combo_MMTrickClear(Combo_MMTrickAt(i));
    }
}
