/**
 * @file combo_spoiler_view.c
 * @brief Read-only view model over the paired world's crossings (#496, #755).
 *
 * See combo_spoiler_view.h for the contract. The rows ARE the combo tracker's
 * crossing rows (combo_tracker_view.c), so the two panes cannot disagree about
 * which crossings a world has, what they are called, or which were found. No
 * gSaveContext, no ImGui, no game headers, no caching: the view is recomputed
 * per call, so a crossing collected mid-session shows up on the next frame and
 * no stale copy can outlive a .redsave Load.
 */

#include "combo_spoiler_view.h"

int Combo_SpoilerRowCount(uint8_t hostGame) {
    // Zero when unpaired: "not paired", not "no crossings" — see the header.
    return Combo_TrackerForeignCount(hostGame);
}

bool Combo_SpoilerRowAt(uint8_t hostGame, int index, ComboSpoilerRow* out) {
    return Combo_TrackerForeignRowAt(hostGame, index, out);
}

void Combo_SpoilerPairingSummary(ComboSpoilerSummary* out) {
    if (out == NULL) {
        return;
    }
    out->paired = Combo_ForeignPairingActive();
    out->sharedRandoSeed = gComboCtx.sharedRandoSeed;
    out->sharedRandoSettingsHash = gComboCtx.sharedRandoSettingsHash;
    out->mmHosted = Combo_SpoilerRowCount((uint8_t)GAME_MM);
    out->ootHosted = Combo_SpoilerRowCount((uint8_t)GAME_OOT);
}
