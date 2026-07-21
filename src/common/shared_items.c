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
#include <string.h>

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
// Slot management (ADR 0005)
//
// The occupied entries always form a PREFIX of the array: records append at
// the first free slot, entries are never cleared individually, and
// reclamation compacts. Slot order is therefore acceptance order, which is
// what makes Combo_RedeemSharedItemsForGame's slot-order walk a received-order
// guarantee.
// ============================================================================

static int FindFirstFreeSlot(void) {
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        if (gComboCtx.sharedItemsTagged[i].originGame == (uint8_t)GAME_NONE) {
            return i;
        }
    }
    return -1;
}

// Evict the OLDEST redeemed entry by compacting everything after it down one
// slot, freeing the tail slot for the caller. Relative order of the surviving
// entries is preserved, so received-order redemption is unaffected. Only
// redeemed entries are evictable: they are informational records of completed
// crossings, while an un-redeemed entry is an undelivered item and must never
// be dropped. Returns the freed tail slot, or -1 if nothing is redeemed.
static int ReclaimOldestRedeemedSlot(void) {
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame != (uint8_t)GAME_NONE && (slot->flags & RSBS_SHARED_ITEM_REDEEMED) != 0) {
            memmove(&gComboCtx.sharedItemsTagged[i], &gComboCtx.sharedItemsTagged[i + 1],
                    ((size_t)RSBS_SHARED_ITEM_CAP - 1u - (size_t)i) * sizeof(SharedItem));
            memset(&gComboCtx.sharedItemsTagged[RSBS_SHARED_ITEM_CAP - 1], 0, sizeof(SharedItem));
            return (int)RSBS_SHARED_ITEM_CAP - 1;
        }
    }
    return -1;
}

// Append an entry with NO content de-dup (the sourced-grant path depends on
// that: a second grant of the same item is a real second item). `flags` is 0
// for an in-process record or RSBS_SHARED_ITEM_SOURCED for a sourced grant —
// never RSBS_SHARED_ITEM_REDEEMED (only the consumer sets that). On a full
// array, reclaim the oldest redeemed entry; if every entry is un-redeemed,
// refuse LOUDLY — increment the durable overflow count and log — rather than
// drop silently. Returns the slot written, or -1 on refusal.
static int AppendSharedItem(GameId originGame, uint16_t id, uint8_t flags) {
    int slot = FindFirstFreeSlot();
    if (slot < 0) {
        slot = ReclaimOldestRedeemedSlot();
        if (slot >= 0) {
            fprintf(stderr, "[SharedItem] array full: reclaimed oldest redeemed entry (origin=%s id=%u incoming)\n",
                    Game_ToString(originGame), (unsigned)id);
        }
    }
    if (slot < 0) {
        gComboCtx.sharedItemOverflowCount++;
        fprintf(stderr,
                "[SharedItem] record REFUSED: all %u slots hold un-redeemed items (overflow count now %u), "
                "origin=%s id=%u\n",
                RSBS_SHARED_ITEM_CAP, gComboCtx.sharedItemOverflowCount, Game_ToString(originGame), (unsigned)id);
        return -1;
    }

    SharedItem* dst = &gComboCtx.sharedItemsTagged[slot];
    dst->originGame = (uint8_t)originGame;
    dst->flags = flags;
    dst->id = id;
    return slot;
}

// ============================================================================
// In-process producer
// ============================================================================

int Combo_RecordSharedItem(GameId originGame, uint16_t id) {
    if (!IsRealGame(originGame)) {
        return -1;
    }

    // De-dup BY CONTENT against an existing un-redeemed IN-PROCESS entry so a
    // re-fired in-process producer cannot create doubles (see the header). An
    // already-redeemed match does NOT block a fresh record — the same item
    // crossing a second time is a real, distinct hand-off. SOURCED entries are
    // skipped (RSBS_SHARED_ITEM_SOURCED): a peer's pending gift of the same
    // item must not swallow a genuine local pickup — the two producer classes'
    // idempotency domains are disjoint by design (ADR 0005).
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame == (uint8_t)originGame && slot->id == id &&
            (slot->flags & (RSBS_SHARED_ITEM_REDEEMED | RSBS_SHARED_ITEM_SOURCED)) == 0) {
            return i; // already pending — leave it exactly as-is
        }
    }

    int slot = AppendSharedItem(originGame, id, 0);
    if (slot >= 0) {
        fprintf(stderr, "[SharedItem] recorded origin=%s id=%u in slot %d\n", Game_ToString(originGame), (unsigned)id,
                slot);
    }
    return slot;
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
        // A -1 here means the durable array is full of un-redeemed items even
        // after reclamation; the staged entry is dropped (already logged AND
        // counted in the durable overflow count by the record path) rather
        // than left to leak forward into a later, unrelated switch.
    }
    sOutboxCount = 0;
    return committed;
}

// ============================================================================
// Sourced producer (ADR 0005) — the seam a transport writes against.
// ============================================================================

ComboGrantResult Combo_SubmitSourcedGrant(uint32_t sourceKey, uint32_t seq, GameId originGame, uint16_t id) {
    if (sourceKey == 0u || seq == 0u || !IsRealGame(originGame)) {
        fprintf(stderr, "[SharedItem] sourced grant REJECTED (malformed): key=%u seq=%u origin=%d id=%u\n",
                sourceKey, seq, (int)originGame, (unsigned)id);
        return RSBS_GRANT_REJECTED;
    }

    // Locate this source's cursor; remember a free slot in case the source is
    // new. The cursor slot must be secured BEFORE the item is recorded: an
    // item recorded without a cursor to remember it would make the source's
    // inevitable retransmit of the same seq record a double.
    ComboGrantSourceCursor* cur = NULL;
    ComboGrantSourceCursor* freeSlot = NULL;
    for (int i = 0; i < (int)RSBS_GRANT_SOURCE_CAP; i++) {
        ComboGrantSourceCursor* c = &gComboCtx.grantCursors[i];
        if (c->sourceKey == sourceKey) {
            cur = c;
            break;
        }
        if (c->sourceKey == 0u && freeSlot == NULL) {
            freeSlot = c;
        }
    }

    const uint32_t lastSeq = (cur != NULL) ? cur->lastSeq : 0u;
    if (seq <= lastSeq) {
        // Retransmit of a grant this save already accepted. Deliberately not
        // logged: this is the expected idempotent path under a resend-happy
        // transport, not an anomaly.
        return RSBS_GRANT_DUPLICATE;
    }
    if (seq != lastSeq + 1u) {
        fprintf(stderr, "[SharedItem] sourced grant GAP: key=%u sent seq=%u but expected %u — resync the source\n",
                sourceKey, seq, lastSeq + 1u);
        return RSBS_GRANT_GAP;
    }
    if (cur == NULL && freeSlot == NULL) {
        fprintf(stderr, "[SharedItem] sourced grant refused: all %u source-cursor slots occupied (key=%u)\n",
                RSBS_GRANT_SOURCE_CAP, sourceKey);
        return RSBS_GRANT_NO_SOURCE_SLOT;
    }

    // In-order and novel: record it. NO content de-dup — a second grant of the
    // same (originGame, id) with its own seq is a real second item. The
    // SOURCED flag keeps this entry out of Combo_RecordSharedItem's content
    // de-dup domain.
    if (AppendSharedItem(originGame, id, RSBS_SHARED_ITEM_SOURCED) < 0) {
        // Backpressure, not loss: the cursor did not advance, so the source
        // still owes this seq. A later retransmit — after redemption frees
        // capacity — is accepted, not treated as a duplicate. AppendSharedItem
        // already counted and logged the refusal.
        return RSBS_GRANT_RETRY_FULL;
    }

    if (cur == NULL) {
        cur = freeSlot;
        cur->sourceKey = sourceKey;
    }
    cur->lastSeq = seq;
    fprintf(stderr, "[SharedItem] sourced grant accepted: key=%u seq=%u origin=%s id=%u\n", sourceKey, seq,
            Game_ToString(originGame), (unsigned)id);
    return RSBS_GRANT_ACCEPTED;
}

uint32_t Combo_GetGrantCursor(uint32_t sourceKey) {
    if (sourceKey == 0u) {
        return 0u;
    }
    for (int i = 0; i < (int)RSBS_GRANT_SOURCE_CAP; i++) {
        if (gComboCtx.grantCursors[i].sourceKey == sourceKey) {
            return gComboCtx.grantCursors[i].lastSeq;
        }
    }
    return 0u;
}

int Combo_CountGrantSources(void) {
    int count = 0;
    for (int i = 0; i < (int)RSBS_GRANT_SOURCE_CAP; i++) {
        if (gComboCtx.grantCursors[i].sourceKey != 0u) {
            count++;
        }
    }
    return count;
}

uint32_t Combo_GetSharedItemOverflowCount(void) {
    return gComboCtx.sharedItemOverflowCount;
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
