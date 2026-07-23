/**
 * @file combo_spoiler_view.c
 * @brief Read-only view model over gComboCtx's cross-game placements (#496).
 *
 * See combo_spoiler_view.h for the contract. Everything here is a pure read of
 * gComboCtx through the foreign_items.h accessors: no gSaveContext, no ImGui,
 * no game headers, no caching. The view is recomputed per call so a crossing
 * collected mid-frame shows up immediately, and so no stale copy can outlive a
 * .redsave Load.
 */

#include "combo_spoiler_view.h"

// RSBS_SHARED_ITEM_REDEEMED / RSBS_SHARED_ITEM_CAP come through context.h
// (pulled in by combo_spoiler_view.h -> foreign_items.h); shared_items.h is
// included for the documented meaning of the tagged-array contract this file
// reads, not for a symbol.
#include "shared_items.h"

/**
 * Has the origin game already awarded this crossing?
 *
 * gComboCtx.sharedItemsTagged has no index — matching is the same linear
 * (originGame, id) scan the give path and Combo_CountSharedItems use. Flags
 * are deliberately NOT part of the match: RSBS_SHARED_ITEM_SOURCED entries
 * (ADR 0005) describe the same crossing and must still report their redeemed
 * bit honestly.
 *
 * A crossing that was never picked up has no tagged entry at all, which reads
 * as false — correct: the item is still sitting in the MM check.
 */
static bool SpoilerItemRedeemed(uint8_t originGame, uint16_t itemId) {
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        const SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame == originGame && slot->id == itemId) {
            if ((slot->flags & RSBS_SHARED_ITEM_REDEEMED) != 0) {
                return true;
            }
        }
    }
    return false;
}

int Combo_SpoilerRowCount(void) {
    if (!Combo_ForeignPairingActive()) {
        return 0; // "not paired", not "no crossings" — see the header
    }
    return Combo_CountForeignPlacements();
}

bool Combo_SpoilerRowAt(int index, ComboSpoilerRow* out) {
    if (out == NULL || index < 0 || !Combo_ForeignPairingActive()) {
        return false;
    }

    // Walk the table in SLOT order, counting only occupied slots, so row N is
    // the Nth crossing as serialized rather than the Nth slot. Occupancy is
    // derived from the item tag exactly the way Combo_CountForeignPlacements
    // derives it — there is no count field to disagree with.
    int seen = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement* slot = &gComboCtx.foreignPlacements[i];
        if (slot->item.originGame == (uint8_t)GAME_NONE) {
            continue;
        }
        if (seen++ != index) {
            continue;
        }

        const char* name = Combo_GetForeignItemName(slot->item);
        out->mmCheckId = slot->mmCheckId;
        out->originGame = slot->item.originGame;
        out->itemId = slot->item.id;
        out->itemName = (name != NULL) ? name : RSBS_SPOILER_UNKNOWN_ITEM_NAME;
        out->redeemed = SpoilerItemRedeemed(slot->item.originGame, slot->item.id);
        return true;
    }
    return false; // index past the last occupied slot
}

void Combo_SpoilerPairingSummary(ComboSpoilerSummary* out) {
    if (out == NULL) {
        return;
    }
    out->paired = Combo_ForeignPairingActive();
    out->sharedRandoSeed = gComboCtx.sharedRandoSeed;
    out->sharedRandoSettingsHash = gComboCtx.sharedRandoSettingsHash;
    out->placementCount = Combo_SpoilerRowCount();
}
