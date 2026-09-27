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
#include "foreign_items.h" // Combo_ForeignGiveCaps, RSBS_GIVECAP_* (src/common; game-header-free like this TU)
#include "shared_resources.h" // Combo_SharedResourceKindArmed, RSBS_SHARED_RES_MAX_HEALTH_QUARTERS (pool policy)
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

// ============================================================================
// The single-owner item classification table (ADR 0010 answer O8)
//
// See shared_items.h for the model. Storage is fixed and static: one class byte
// and one arming word per id per origin, RSBS_ITEM_CLASS_ID_CAP ids each — a few
// KB, and no allocation that a failed build could leak or a test could forget to
// free. Game thread only, like everything else in this TU.
// ============================================================================

// The low half of the arming word IS the give-capability word, so a published
// frozen profile arms rows with no translation table to drift.
RSBS_CTX_STATIC_ASSERT(RSBS_FILL_ARM_SOULS == RSBS_GIVECAP_SOULS &&
                           RSBS_FILL_ARM_OCARINA_BUTTONS == RSBS_GIVECAP_OCARINA_BUTTONS &&
                           RSBS_FILL_ARM_SWIM == RSBS_GIVECAP_SWIM && RSBS_FILL_ARM_CLOCKS == RSBS_GIVECAP_CLOCKS &&
                           (RSBS_GIVECAP_ALL_V1 & ~RSBS_FILL_ARM_GIVECAPS_MASK) == 0u,
                       "the give-capability half of RSBS_FILL_ARM_* must be bit-identical to RSBS_GIVECAP_*");
RSBS_CTX_STATIC_ASSERT(RSBS_FILL_CLASS_NONE == 0u && RSBS_FILL_CLASS_PROGRESSION == 1u &&
                           RSBS_FILL_CLASS_JUNK == 2u && RSBS_FILL_CLASS_RENEWABLE == 3u &&
                           RSBS_FILL_CLASS_TRAP == 4u && RSBS_FILL_CLASS_COUNT == 5u,
                       "RSBS_FILL_CLASS_* values are pinned and append-only");

#define ITEM_CLASS_ORIGINS 3 /* indexed by GameId: GAME_NONE (unused), GAME_OOT, GAME_MM */

typedef struct {
    const ComboItemClassSource* source;
    bool built;
    int fillItems;
    uint8_t fillClass[RSBS_ITEM_CLASS_ID_CAP]; // the SOURCE's answer, unreconciled
    uint32_t armedBy[RSBS_ITEM_CLASS_ID_CAP];
    uint8_t sharedKind[RSBS_ITEM_CLASS_ID_CAP];
    uint8_t sharedUnits[RSBS_ITEM_CLASS_ID_CAP]; // normalized: >= 1 where sharedKind != 0, else 0
} ItemClassTable;

static ItemClassTable sItemClass[ITEM_CLASS_ORIGINS];

// The reconciled class per shared kind (#731), folded over every registered
// origin's stored rows. Rebuilt lazily; invalidated whenever a table is built or
// dropped, so it can never answer for a source set that no longer exists.
static bool sKindClassValid = false;
static uint8_t sKindClass[RSBS_ITEM_CLASS_SHARED_KIND_CAP];
static uint32_t sItemClassRefused = 0;
static uint32_t sItemClassUnregistered = 0;

static ItemClassTable* ItemClassTableFor(uint8_t originGame) {
    if (!IsRealGame((GameId)originGame)) {
        return NULL;
    }
    return &sItemClass[originGame];
}

// A row a source may legally return: "not a fill item" carries NONE and feeds no
// kind, "a fill item" carries exactly one real class. A shared kind must be in
// range, and a TRAP never feeds one (a trap is a per-game punishment, not a
// quantity — shared_items.h, "ONE CLASS PER CROSS-GAME SHARED QUANTITY").
static bool ItemClassRowValid(int rv, const ComboItemClassRow* row) {
    if (rv == 0) {
        return row->fillClass == RSBS_FILL_CLASS_NONE && row->sharedKind == 0u;
    }
    if (row->sharedKind >= RSBS_ITEM_CLASS_SHARED_KIND_CAP ||
        (row->sharedKind != 0u && row->fillClass == RSBS_FILL_CLASS_TRAP)) {
        return false;
    }
    return rv == 1 && row->fillClass != RSBS_FILL_CLASS_NONE && row->fillClass < RSBS_FILL_CLASS_COUNT;
}

// A row's units as stored: a source's 0 reads as ONE unit (every tier copy and a
// heart piece), and a row feeding no kind stores 0 whatever the source wrote.
static uint8_t ItemClassNormalizedUnits(const ComboItemClassRow* row) {
    if (row->sharedKind == 0u) {
        return 0u;
    }
    return (row->sharedUnits == 0u) ? 1u : row->sharedUnits;
}

// The reconciliation rank: PROGRESSION > RENEWABLE > JUNK. NONE and TRAP rank 0
// (neither can feed a kind — see ItemClassRowValid).
static int ItemClassKindRank(uint8_t fillClass) {
    switch (fillClass) {
        case RSBS_FILL_CLASS_PROGRESSION:
            return 3;
        case RSBS_FILL_CLASS_RENEWABLE:
            return 2;
        case RSBS_FILL_CLASS_JUNK:
            return 1;
        default:
            return 0;
    }
}

bool Combo_RegisterItemClassSource(uint8_t originGame, const ComboItemClassSource* source) {
    ItemClassTable* t = ItemClassTableFor(originGame);
    if (t == NULL) {
        sItemClassRefused++;
        fprintf(stderr, "[ItemClass] registration REFUSED: origin %u is not a real game\n", (unsigned)originGame);
        return false;
    }
    if (source == NULL) {
        // Not an un-registration: that door is test-only and logged
        // (Combo_TestUnregisterItemClassSource), so a replacement can never go
        // through here in two silent steps.
        sItemClassRefused++;
        fprintf(stderr, "[ItemClass] registration REFUSED for %s: NULL source (refusals: %u)\n",
                Game_ToString((GameId)originGame), sItemClassRefused);
        return false;
    }
    if (source->abiVersion != RSBS_ITEM_CLASS_SOURCE_ABI || source->classify == NULL || source->idSpace == 0u ||
        source->idSpace > RSBS_ITEM_CLASS_ID_CAP) {
        sItemClassRefused++;
        fprintf(stderr, "[ItemClass] registration REFUSED for %s: abi=%u classify=%s idSpace=%u (cap %u)\n",
                Game_ToString((GameId)originGame), (unsigned)source->abiVersion,
                source->classify != NULL ? "set" : "NULL", (unsigned)source->idSpace, RSBS_ITEM_CLASS_ID_CAP);
        return false;
    }
    if (t->source != NULL) {
        // ONE SOURCE PER ORIGIN (O8). Refused, never replaced: two sources for
        // one id space is the duplicate that can disagree with itself, and there
        // is no principled winner. Even the SAME source registering twice is
        // refused — a registrar that runs twice is a bug worth a line in the log.
        sItemClassRefused++;
        fprintf(stderr, "[ItemClass] registration REFUSED for %s: a source is already registered (refusals: %u)\n",
                Game_ToString((GameId)originGame), sItemClassRefused);
        return false;
    }
    memset(t, 0, sizeof(*t));
    t->source = source;
    sKindClassValid = false;
    return true;
}

bool Combo_TestUnregisterItemClassSource(uint8_t originGame) {
    ItemClassTable* t = ItemClassTableFor(originGame);
    if (t == NULL || t->source == NULL) {
        return false;
    }
    sItemClassUnregistered++;
    fprintf(stderr, "[ItemClass] TEST un-registration of %s's source (un-registrations: %u)\n",
            Game_ToString((GameId)originGame), sItemClassUnregistered);
    memset(t, 0, sizeof(*t)); // drop the source AND every row built from it
    sKindClassValid = false;
    return true;
}

uint32_t Combo_ItemClassUnregistrations(void) {
    return sItemClassUnregistered;
}

const ComboItemClassSource* Combo_GetItemClassSource(uint8_t originGame) {
    const ItemClassTable* t = ItemClassTableFor(originGame);
    return t != NULL ? t->source : NULL;
}

uint32_t Combo_ItemClassRefusedRegistrations(void) {
    return sItemClassRefused;
}

int Combo_ItemClassBuild(uint8_t originGame) {
    ItemClassTable* t = ItemClassTableFor(originGame);
    if (t == NULL || t->source == NULL) {
        return -1;
    }
    if (t->built) {
        return t->fillItems;
    }
    const ComboItemClassSource* src = t->source;
    int fillItems = 0;
    for (uint32_t id = 0; id < src->idSpace; id++) {
        ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u, 0u, 0u };
        const int rv = src->classify((uint16_t)id, &row);
        if (rv < 0) {
            // Not ready: keep NOTHING. A half-built table would be cached as the
            // answer for the rest of the process.
            memset(t->fillClass, 0, sizeof(t->fillClass));
            memset(t->armedBy, 0, sizeof(t->armedBy));
            memset(t->sharedKind, 0, sizeof(t->sharedKind));
            memset(t->sharedUnits, 0, sizeof(t->sharedUnits));
            return -1;
        }
        if (!ItemClassRowValid(rv, &row)) {
            // "A fill item" with no class, or a class the enum does not have, is a
            // source refusing to classify. Stored as NONE so the lock that counts
            // unclassified fill items sees it, and logged so the id is named.
            fprintf(stderr,
                    "[ItemClass] %s id %u: invalid source row (rv=%d class=%u kind=%u) stored as unclassified\n",
                    Game_ToString((GameId)originGame), (unsigned)id, rv, (unsigned)row.fillClass,
                    (unsigned)row.sharedKind);
            row.fillClass = RSBS_FILL_CLASS_NONE;
            row.armedBy = 0u;
            row.sharedKind = 0u;
        }
        t->fillClass[id] = row.fillClass;
        t->armedBy[id] = row.armedBy;
        t->sharedKind[id] = row.sharedKind;
        t->sharedUnits[id] = ItemClassNormalizedUnits(&row);
        if (row.fillClass != RSBS_FILL_CLASS_NONE) {
            fillItems++;
        }
    }
    t->fillItems = fillItems;
    t->built = true;
    sKindClassValid = false;
    return fillItems;
}

// Fold every registered origin's rows into one class per shared kind. False (and
// nothing cached) while any registered table cannot be built: a fold over half
// the rows would be a different answer once the other half arrives.
static bool ItemClassKindsReady(void) {
    if (sKindClassValid) {
        return true;
    }
    uint8_t folded[RSBS_ITEM_CLASS_SHARED_KIND_CAP];
    memset(folded, 0, sizeof(folded));
    for (uint8_t origin = 0; origin < ITEM_CLASS_ORIGINS; origin++) {
        ItemClassTable* t = ItemClassTableFor(origin);
        if (t == NULL || t->source == NULL) {
            continue;
        }
        if (Combo_ItemClassBuild(origin) < 0) {
            return false;
        }
        for (uint32_t id = 0; id < t->source->idSpace; id++) {
            const uint8_t kind = t->sharedKind[id];
            if (kind != 0u && ItemClassKindRank(t->fillClass[id]) > ItemClassKindRank(folded[kind])) {
                folded[kind] = t->fillClass[id];
            }
        }
    }
    memcpy(sKindClass, folded, sizeof(sKindClass));
    sKindClassValid = true;
    return true;
}

uint8_t Combo_ItemClassSourceOf(SharedItem item) {
    ItemClassTable* t = ItemClassTableFor(item.originGame);
    if (t == NULL || Combo_ItemClassBuild(item.originGame) < 0 || item.id >= t->source->idSpace) {
        return RSBS_FILL_CLASS_NONE;
    }
    return t->fillClass[item.id];
}

uint8_t Combo_ItemClassSharedKind(SharedItem item) {
    ItemClassTable* t = ItemClassTableFor(item.originGame);
    if (t == NULL || Combo_ItemClassBuild(item.originGame) < 0 || item.id >= t->source->idSpace) {
        return 0u;
    }
    return t->sharedKind[item.id];
}

uint8_t Combo_ItemClassSharedUnits(SharedItem item) {
    ItemClassTable* t = ItemClassTableFor(item.originGame);
    if (t == NULL || Combo_ItemClassBuild(item.originGame) < 0 || item.id >= t->source->idSpace) {
        return 0u;
    }
    return t->sharedUnits[item.id];
}

uint8_t Combo_ItemClassKindClass(uint8_t kind) {
    if (kind == 0u || kind >= RSBS_ITEM_CLASS_SHARED_KIND_CAP || !ItemClassKindsReady()) {
        return RSBS_FILL_CLASS_NONE;
    }
    return sKindClass[kind];
}

int Combo_ItemClassKindRows(uint8_t originGame, uint8_t kind) {
    ItemClassTable* t = ItemClassTableFor(originGame);
    if (t == NULL || Combo_ItemClassBuild(originGame) < 0) {
        return -1;
    }
    int rows = 0;
    for (uint32_t id = 0; id < t->source->idSpace; id++) {
        if (kind != 0u && t->sharedKind[id] == kind && t->fillClass[id] != RSBS_FILL_CLASS_NONE) {
            rows++;
        }
    }
    return rows;
}

uint8_t Combo_ItemClassOf(SharedItem item) {
    ItemClassTable* t = ItemClassTableFor(item.originGame);
    if (t == NULL || Combo_ItemClassBuild(item.originGame) < 0 || item.id >= t->source->idSpace) {
        return RSBS_FILL_CLASS_NONE;
    }
    const uint8_t kind = t->sharedKind[item.id];
    if (kind == 0u || t->fillClass[item.id] == RSBS_FILL_CLASS_NONE) {
        return t->fillClass[item.id];
    }
    // A row feeding a shared kind answers the KIND's class (#731), which needs
    // every registered table; NONE until they can all be built.
    return Combo_ItemClassKindClass(kind);
}

uint32_t Combo_ItemClassArmedBy(SharedItem item) {
    ItemClassTable* t = ItemClassTableFor(item.originGame);
    if (t == NULL || Combo_ItemClassBuild(item.originGame) < 0 || item.id >= t->source->idSpace) {
        return 0u;
    }
    return t->armedBy[item.id];
}

int Combo_ItemClassCount(uint8_t originGame, uint8_t fillClass) {
    ItemClassTable* t = ItemClassTableFor(originGame);
    if (t == NULL || Combo_ItemClassBuild(originGame) < 0) {
        return -1;
    }
    int count = 0;
    for (uint32_t id = 0; id < t->source->idSpace; id++) {
        SharedItem item;
        item.originGame = originGame;
        item.flags = 0;
        item.id = (uint16_t)id;
        if (Combo_ItemClassOf(item) == fillClass) {
            count++;
        }
    }
    return count;
}

int Combo_ItemClassVerify(uint8_t originGame) {
    ItemClassTable* t = ItemClassTableFor(originGame);
    if (t == NULL || Combo_ItemClassBuild(originGame) < 0) {
        return -1;
    }
    int diverging = 0;
    for (uint32_t id = 0; id < t->source->idSpace; id++) {
        ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u, 0u, 0u };
        const int rv = t->source->classify((uint16_t)id, &row);
        if (rv < 0) {
            return -1;
        }
        if (!ItemClassRowValid(rv, &row)) {
            row.fillClass = RSBS_FILL_CLASS_NONE;
            row.armedBy = 0u;
            row.sharedKind = 0u;
        }
        if (row.fillClass != t->fillClass[id] || row.armedBy != t->armedBy[id] ||
            row.sharedKind != t->sharedKind[id] || ItemClassNormalizedUnits(&row) != t->sharedUnits[id]) {
            if (diverging < 8) {
                fprintf(stderr, "[ItemClass] %s id %u DIVERGES: owner=(%s, 0x%08X) source=(%s, 0x%08X)\n",
                        Game_ToString((GameId)originGame), (unsigned)id, Combo_ItemClassName(t->fillClass[id]),
                        (unsigned)t->armedBy[id], Combo_ItemClassName(row.fillClass), (unsigned)row.armedBy);
            }
            diverging++;
        }
    }
    return diverging;
}

const char* Combo_ItemClassName(uint8_t fillClass) {
    switch (fillClass) {
        case RSBS_FILL_CLASS_NONE:
            return "(none)";
        case RSBS_FILL_CLASS_PROGRESSION:
            return "progression";
        case RSBS_FILL_CLASS_JUNK:
            return "junk";
        case RSBS_FILL_CLASS_RENEWABLE:
            return "renewable";
        case RSBS_FILL_CLASS_TRAP:
            return "trap";
        default:
            return "(unknown)";
    }
}

bool Combo_ItemClassMayCrossUnder(SharedItem item, uint32_t armed) {
    if (Combo_ItemClassOf(item) != RSBS_FILL_CLASS_PROGRESSION) {
        return false; // junk, renewable, trap and non-items never cross (shared_items.h)
    }
    const uint32_t needs = Combo_ItemClassArmedBy(item);
    return (needs & ~armed) == 0u;
}

uint32_t Combo_ItemClassArmedFromFrozen(uint8_t originGame) {
    if (!IsRealGame((GameId)originGame) || !Combo_ForeignGiveCapsPublished(originGame)) {
        return 0u; // nothing frozen is published: nothing conditional is armed
    }
    return Combo_ForeignGiveCaps(originGame) & RSBS_FILL_ARM_GIVECAPS_MASK;
}

// ============================================================================
// THE SHARED-QUANTITY POOL POLICY (lane K13) — see shared_items.h
// ============================================================================

bool Combo_SharedQuantityPolicyOf(uint8_t kind, ComboSharedQuantityPolicy* out) {
    ComboSharedQuantityPolicy p = { RSBS_SHARED_QTY_KEEP_ALL, RSBS_SHARED_QTY_BUDGET_NONE, 0u, 0u, 0u };
    const bool real = (kind != (uint8_t)RSBS_SHARED_RES_NONE && kind < RSBS_SHARED_RES_KIND_COUNT);
    if (real) {
        switch (kind) {
            case RSBS_SHARED_RES_HEALTH_QUARTERS:
                p.policy = RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX;
                p.budget = RSBS_SHARED_QTY_BUDGET_HEALTH;
                break;
            // The per-game ceilings are the give paths' tops, the same numbers the
            // carrier's shims apply with (OOT_/MM_MAX_*_TIER, GameExports_SingleExe.cpp;
            // restated here because this TU names no game header).
            case RSBS_SHARED_RES_DOUBLE_DEFENSE:
                p.policy = RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX;
                p.budget = RSBS_SHARED_QTY_BUDGET_TIER;
                p.fixedCap = 1u; // one flag, one copy (OoTMM: "one Double Defense for both games")
                p.ceilingOoT = 1u;
                p.ceilingMM = 1u;
                break;
            case RSBS_SHARED_RES_WALLET_TIER:
                // Both progressive wallets stop at the giant's wallet (MM's
                // ConvertItem: CUR_UPG_VALUE(UPG_WALLET) >= 2); OoT's reaches the
                // tycoon's only when its pool carries that copy, which the pool
                // count raises the ceiling to.
            case RSBS_SHARED_RES_MAGIC_LEVEL:
                p.policy = RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX;
                p.budget = RSBS_SHARED_QTY_BUDGET_TIER;
                p.ceilingOoT = 2u;
                p.ceilingMM = 2u;
                break;
            case RSBS_SHARED_RES_QUIVER_TIER:
            case RSBS_SHARED_RES_BOMB_BAG_TIER:
            case RSBS_SHARED_RES_STICK_TIER:
            case RSBS_SHARED_RES_NUT_TIER:
                p.policy = RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX;
                p.budget = RSBS_SHARED_QTY_BUDGET_TIER;
                p.ceilingOoT = 3u; // OOT_MAX_AMMO_TIER
                p.ceilingMM = 3u;  // MM_MAX_AMMO_TIER
                break;
            case RSBS_SHARED_RES_HOOKSHOT_TIER:
            case RSBS_SHARED_RES_OCARINA_TIER:
                p.policy = RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX;
                p.budget = RSBS_SHARED_QTY_BUDGET_TIER;
                p.ceilingOoT = 2u; // OOT_MAX_HOOKSHOT_TIER / OOT_MAX_OCARINA_TIER
                p.ceilingMM = 1u;  // MM_MAX_HOOKSHOT_TIER / MM_MAX_OCARINA_TIER
                break;
            default:
                // Rupees, current health / magic, the five ammo counts and the
                // triforce count: renewable or counted, never a capacity.
                break;
        }
        // The one arming gate, read from the one place it lives: a kind the
        // frozen record does not share keeps each game's own copies untrimmed.
        if (p.policy != RSBS_SHARED_QTY_KEEP_ALL && !Combo_SharedResourceKindArmed(kind)) {
            p.policy = RSBS_SHARED_QTY_KEEP_ALL;
            p.budget = RSBS_SHARED_QTY_BUDGET_NONE;
            p.fixedCap = 0u;
            p.ceilingOoT = 0u;
            p.ceilingMM = 0u;
        }
    }
    if (out != NULL) {
        *out = p;
    }
    return real;
}

const char* Combo_SharedQuantityPolicyName(uint8_t policy) {
    switch (policy) {
        case RSBS_SHARED_QTY_KEEP_ALL:
            return "keep-all";
        case RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX:
            return "trim-to-shared-max";
        default:
            return "(unknown)";
    }
}

int Combo_SharedQuantityHealthBudget(uint16_t startingHealth, int piecesAvailable, int containersAvailable,
                                     int* outPieces, int* outContainers) {
    const int start = (startingHealth == 0u) ? (int)RSBS_SHARED_QTY_DEFAULT_START_HEALTH : (int)startingHealth;
    int budget = ((int)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS - start) / 4; // in quarters (heart pieces)
    if (budget < 0) {
        budget = 0;
    }
    int pieces = (budget < RSBS_SHARED_QTY_HEART_PIECES) ? budget : RSBS_SHARED_QTY_HEART_PIECES;
    int containers = (budget - pieces) / 4;
    pieces += (budget - pieces) % 4; // a remainder under one heart stays in pieces
    // Move what a short grade cannot use to the other grade, computed from the
    // ORIGINAL split so the two moves cannot feed each other.
    const int pieceShort = (piecesAvailable >= 0 && piecesAvailable < pieces) ? pieces - piecesAvailable : 0;
    const int containerShort =
        (containersAvailable >= 0 && containersAvailable < containers) ? containers - containersAvailable : 0;
    pieces += 4 * containerShort;
    containers += pieceShort / 4;
    if (piecesAvailable >= 0 && pieces > piecesAvailable) {
        pieces = piecesAvailable;
    }
    if (containersAvailable >= 0 && containers > containersAvailable) {
        containers = containersAvailable;
    }
    if (outPieces != NULL) {
        *outPieces = pieces;
    }
    if (outContainers != NULL) {
        *outContainers = containers;
    }
    return budget;
}

int Combo_SharedQuantityTierBudget(const ComboSharedQuantityPolicy* policy, int ootCopies, int mmCopies) {
    if (policy == NULL || policy->policy != RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX ||
        policy->budget != RSBS_SHARED_QTY_BUDGET_TIER || ootCopies < 0 || mmCopies < 0) {
        return -1;
    }
    const int ceilO = ((int)policy->ceilingOoT > ootCopies) ? (int)policy->ceilingOoT : ootCopies;
    const int ceilM = ((int)policy->ceilingMM > mmCopies) ? (int)policy->ceilingMM : mmCopies;
    if (ootCopies > 0 && mmCopies > 0 && ceilO != ceilM) {
        return -1; // UNEQUAL CEILINGS ARE KEPT WHOLE (shared_items.h)
    }
    int budget = (ootCopies > mmCopies) ? ootCopies : mmCopies;
    if (policy->fixedCap != 0u && budget > (int)policy->fixedCap) {
        budget = (int)policy->fixedCap;
    }
    return budget;
}

void Combo_SharedQuantitySplit(int budget, int ootCopies, int mmCopies, uint32_t tieSeed, int* outOoT, int* outMM) {
    int keepO = 0;
    int keepM = 0;
    if (ootCopies < 0) {
        ootCopies = 0;
    }
    if (mmCopies < 0) {
        mmCopies = 0;
    }
    const int total = ootCopies + mmCopies;
    if (budget < 0) {
        budget = 0;
    }
    if (budget >= total) {
        keepO = ootCopies;
        keepM = mmCopies;
    } else if (budget > 0) {
        // Largest remainder in integers: quota_g = budget * n_g / total.
        keepO = (int)(((long long)budget * ootCopies) / total);
        keepM = (int)(((long long)budget * mmCopies) / total);
        const long long remO = ((long long)budget * ootCopies) % total;
        const long long remM = ((long long)budget * mmCopies) % total;
        int left = budget - keepO - keepM; // 0 or 1 with two games
        while (left > 0) {
            bool toOoT;
            if (keepO >= ootCopies) {
                toOoT = false;
            } else if (keepM >= mmCopies) {
                toOoT = true;
            } else if (remO != remM) {
                toOoT = remO > remM;
            } else {
                toOoT = (tieSeed & 1u) == 0u;
            }
            if (toOoT) {
                keepO++;
            } else {
                keepM++;
            }
            left--;
        }
        // Both worlds keep a copy of a family both hold, when the budget allows
        // one per holder: take it from the larger share.
        if (budget >= 2) {
            if (ootCopies > 0 && keepO == 0 && keepM > 1) {
                keepO++;
                keepM--;
            } else if (mmCopies > 0 && keepM == 0 && keepO > 1) {
                keepM++;
                keepO--;
            }
        }
    }
    if (outOoT != NULL) {
        *outOoT = keepO;
    }
    if (outMM != NULL) {
        *outMM = keepM;
    }
}
