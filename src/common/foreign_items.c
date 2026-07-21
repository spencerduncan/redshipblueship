/**
 * @file foreign_items.c
 * @brief Foreign-item placement table accessors (Phase 3.0 Lane C1, #392).
 *
 * See foreign_items.h for the model. Like shared_items.c, this TU is
 * deliberately free of game headers: it manipulates the ADR-0002 tagged types
 * and gComboCtx only, so it compiles into redship_common and both games (and
 * the ROM-free test harness) call it directly. The pinned POOL half of the
 * header (Combo_GetForeignItemPool / Combo_GetForeignItemName /
 * OoT_ForeignItem_Give) is defined OoT-side, where the RG_* enumerators are
 * in scope.
 */

#include "foreign_items.h"
#include <stdio.h>

bool Combo_ForeignPairingActive(void) {
    return gComboCtx.sourceIsRando && gComboCtx.sharedRandoSettingsHash != 0;
}

int Combo_SetForeignPlacement(uint16_t mmCheckId, SharedItem item) {
    if (mmCheckId == 0 || item.originGame == (uint8_t)GAME_NONE) {
        return -1; // RC_UNKNOWN never hosts; an untagged item must not enter the table
    }

    int firstFree = -1;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        ComboForeignPlacement* slot = &gComboCtx.foreignPlacements[i];
        if (slot->item.originGame == (uint8_t)GAME_NONE) {
            if (firstFree < 0) {
                firstFree = i;
            }
            continue;
        }
        if (slot->mmCheckId == mmCheckId) {
            fprintf(stderr, "[ForeignItem] placement rejected: MM check %u already hosts origin=%u id=%u\n",
                    (unsigned)mmCheckId, (unsigned)slot->item.originGame, (unsigned)slot->item.id);
            return -1; // one check hosts at most one foreign item
        }
    }

    if (firstFree < 0) {
        fprintf(stderr, "[ForeignItem] placement dropped: table full (%u slots), check=%u id=%u\n",
                RSBS_FOREIGN_PLACEMENT_CAP, (unsigned)mmCheckId, (unsigned)item.id);
        return -1;
    }

    ComboForeignPlacement* dst = &gComboCtx.foreignPlacements[firstFree];
    dst->mmCheckId = mmCheckId;
    dst->item = item;
    fprintf(stderr, "[ForeignItem] placed origin=%u id=%u at MM check %u (slot %d)\n", (unsigned)item.originGame,
            (unsigned)item.id, (unsigned)mmCheckId, firstFree);
    return firstFree;
}

const SharedItem* Combo_GetForeignPlacementForCheck(uint16_t mmCheckId) {
    if (mmCheckId == 0) {
        return NULL;
    }
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement* slot = &gComboCtx.foreignPlacements[i];
        if (slot->item.originGame != (uint8_t)GAME_NONE && slot->mmCheckId == mmCheckId) {
            return &slot->item;
        }
    }
    return NULL;
}

int Combo_CountForeignPlacements(void) {
    int count = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        if (gComboCtx.foreignPlacements[i].item.originGame != (uint8_t)GAME_NONE) {
            count++;
        }
    }
    return count;
}

void Combo_ClearForeignPlacements(void) {
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        gComboCtx.foreignPlacements[i].mmCheckId = 0;
        gComboCtx.foreignPlacements[i].item.originGame = (uint8_t)GAME_NONE;
        gComboCtx.foreignPlacements[i].item.flags = 0;
        gComboCtx.foreignPlacements[i].item.id = 0;
    }
}
