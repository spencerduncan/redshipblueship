/**
 * @file shared_items.c
 * @brief Cross-game shared-item producers and consumers (ADR 0002, Lane A1).
 *
 * See shared_items.h for the model. Everything here operates on the
 * process-global gComboCtx.sharedItemsTagged array (the durable, serialized
 * store) plus a small RAM-only outbox that the producer hook drains at suspend.
 *
 * This TU is intentionally free of game headers: it manipulates the ADR-0002
 * SharedItem type and gComboCtx only, so it compiles into the shared
 * redship_common library and both games' C TUs call it directly.
 */

#include "shared_items.h"
#include <stdio.h>

// ============================================================================
// RAM outbox for the deferred (stage -> commit-at-suspend) producer path.
//
// Kept small: this only buffers pickups between a foreign-item give and the
// imminent switch. Committed items land in gComboCtx.sharedItemsTagged, which
// is the array that actually crosses the switch and serializes.
// ============================================================================

#define SHARED_ITEM_OUTBOX_CAP RSBS_SHARED_ITEM_CAP

static SharedItem sOutbox[SHARED_ITEM_OUTBOX_CAP];
static int sOutboxCount = 0;

static bool IsRealGame(GameId game) {
    return game == GAME_OOT || game == GAME_MM;
}

// ============================================================================
// Producer
// ============================================================================

int Combo_RecordSharedItem(GameId originGame, uint16_t id) {
    if (!IsRealGame(originGame)) {
        return -1;
    }

    // De-dup against an existing un-redeemed entry so a re-fired producer
    // cannot create doubles (see the header). An already-redeemed match does
    // NOT block a fresh record — the same item crossing a second time is a
    // real, distinct hand-off.
    int firstFree = -1;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame == (uint8_t)GAME_NONE) {
            if (firstFree < 0) {
                firstFree = i;
            }
            continue;
        }
        if (slot->originGame == (uint8_t)originGame && slot->id == id &&
            (slot->flags & RSBS_SHARED_ITEM_REDEEMED) == 0) {
            return i; // already pending — leave it exactly as-is
        }
    }

    if (firstFree < 0) {
        fprintf(stderr, "[SharedItem] record dropped: array full (%u slots), origin=%s id=%u\n",
                RSBS_SHARED_ITEM_CAP, Game_ToString(originGame), (unsigned)id);
        return -1;
    }

    SharedItem* dst = &gComboCtx.sharedItemsTagged[firstFree];
    dst->originGame = (uint8_t)originGame;
    dst->flags = 0;
    dst->id = id;
    fprintf(stderr, "[SharedItem] recorded origin=%s id=%u in slot %d\n", Game_ToString(originGame), (unsigned)id,
            firstFree);
    return firstFree;
}

bool Combo_StageSharedItem(GameId originGame, uint16_t id) {
    if (!IsRealGame(originGame)) {
        return false;
    }
    if (sOutboxCount >= SHARED_ITEM_OUTBOX_CAP) {
        fprintf(stderr, "[SharedItem] stage dropped: outbox full (%d), origin=%s id=%u\n", SHARED_ITEM_OUTBOX_CAP,
                Game_ToString(originGame), (unsigned)id);
        return false;
    }
    sOutbox[sOutboxCount].originGame = (uint8_t)originGame;
    sOutbox[sOutboxCount].flags = 0;
    sOutbox[sOutboxCount].id = id;
    sOutboxCount++;
    return true;
}

int Combo_CommitStagedSharedItems(void) {
    int committed = 0;
    for (int i = 0; i < sOutboxCount; i++) {
        if (Combo_RecordSharedItem((GameId)sOutbox[i].originGame, sOutbox[i].id) >= 0) {
            committed++;
        }
        // A -1 here means the durable array is full; the staged entry is
        // dropped (already logged by Combo_RecordSharedItem) rather than left
        // to leak forward into a later, unrelated switch.
    }
    sOutboxCount = 0;
    return committed;
}

// ============================================================================
// Consumer
// ============================================================================

int Combo_RedeemSharedItemsForGame(GameId arrivingGame, ComboSharedItemAward award, void* ctx) {
    if (!IsRealGame(arrivingGame)) {
        return 0;
    }

    int redeemed = 0;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame != (uint8_t)arrivingGame) {
            continue; // empty slot or an item bound for the other game
        }
        if ((slot->flags & RSBS_SHARED_ITEM_REDEEMED) != 0) {
            continue; // already awarded on an earlier arrival
        }
        if (award != NULL) {
            award(slot, ctx);
        }
        slot->flags |= RSBS_SHARED_ITEM_REDEEMED;
        redeemed++;
    }
    if (redeemed > 0) {
        fprintf(stderr, "[SharedItem] redeemed %d item(s) for %s\n", redeemed, Game_ToString(arrivingGame));
    }
    return redeemed;
}

// ============================================================================
// Read-only helpers
// ============================================================================

int Combo_CountSharedItems(GameId game, bool includeRedeemed) {
    if (!IsRealGame(game)) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        const SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame != (uint8_t)game) {
            continue;
        }
        if (!includeRedeemed && (slot->flags & RSBS_SHARED_ITEM_REDEEMED) != 0) {
            continue;
        }
        count++;
    }
    return count;
}

void Combo_ClearSharedItemOutbox(void) {
    sOutboxCount = 0;
}
