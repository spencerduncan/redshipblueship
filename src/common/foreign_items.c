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
#include <string.h>

bool Combo_ForeignPairingActive(void) {
    return gComboCtx.sourceIsRando && gComboCtx.sharedRandoSettingsHash != 0;
}

// ============================================================================
// Pool registry, indexed by origin game (ADR 0009 decision 3)
// ============================================================================
//
// Each pool's table is defined in the TU where its enum is in scope and lands
// here through a file-scope registration, so neither game has to be linkable
// from the other and a build with only one pool present still resolves. See
// foreign_items.h for why (originGame, name) is the key and a bare name is not.

static const ComboForeignItemDef* sForeignPools[RSBS_FOREIGN_POOL_ORIGIN_COUNT];
static int sForeignPoolCounts[RSBS_FOREIGN_POOL_ORIGIN_COUNT];

void Combo_RegisterForeignItemPool(uint8_t originGame, const ComboForeignItemDef* pool, int count) {
    if (originGame == (uint8_t)GAME_NONE || originGame >= RSBS_FOREIGN_POOL_ORIGIN_COUNT) {
        fprintf(stderr, "[ForeignItem] pool registration rejected: origin %u is not a game id-space\n",
                (unsigned)originGame);
        return;
    }
    if (pool == NULL && count == 0) {
        // Explicit un-register. Exists so a test can install a synthetic pool
        // for an origin whose real pool TU is not linked yet and then put the
        // registry back, rather than leaving process-global state behind for
        // whichever test runs next.
        sForeignPools[originGame] = NULL;
        sForeignPoolCounts[originGame] = 0;
        return;
    }
    if (pool == NULL || count <= 0) {
        fprintf(stderr, "[ForeignItem] pool registration rejected: empty pool for origin %u\n", (unsigned)originGame);
        return;
    }
    if (sForeignPools[originGame] != NULL) {
        // Two tables claiming one id-space is exactly the ambiguity this
        // surface exists to prevent; it must not pass silently even though the
        // last writer wins.
        fprintf(stderr, "[ForeignItem] pool for origin %u re-registered (%d entries replace %d)\n",
                (unsigned)originGame, count, sForeignPoolCounts[originGame]);
    }
    sForeignPools[originGame] = pool;
    sForeignPoolCounts[originGame] = count;
}

int Combo_GetForeignItemPoolFor(uint8_t originGame, const ComboForeignItemDef** outPool) {
    if (originGame == (uint8_t)GAME_NONE || originGame >= RSBS_FOREIGN_POOL_ORIGIN_COUNT) {
        return 0;
    }
    if (sForeignPools[originGame] == NULL) {
        return 0; // that origin's pool TU is not linked into this build
    }
    if (outPool != NULL) {
        *outPool = sForeignPools[originGame];
    }
    return sForeignPoolCounts[originGame];
}

int Combo_GetForeignItemPool(const ComboForeignItemDef** outPool) {
    return Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, outPool);
}

const char* Combo_GetForeignItemName(SharedItem item) {
    // Origin-aware by construction: the SharedItem carries its own tag, so the
    // only thing the origin dimension changes is WHICH pool gets walked. An
    // untagged item resolves to no pool and therefore to no name, which is the
    // correct answer rather than a lucky match in whichever table came first.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(item.originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].item.originGame == item.originGame && pool[i].item.id == item.id) {
            return pool[i].name;
        }
    }
    return NULL;
}

bool Combo_GetForeignItemByNameFor(uint8_t originGame, const char* name, SharedItem* outItem) {
    // The spoiler-LOAD inverse. Reused (not re-derived) so reconstruction shares
    // one source of truth with generation, and so a raw RG_*/RI_* is never
    // fabricated on the far side (ADR 0002). Scoped to ONE origin's pool: the
    // display names are not unique across pools ("Bomb Bag" is in both), so a
    // merged scan would resolve to a wrong origin tag rather than to nothing.
    if (name == NULL) {
        return false;
    }
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].name != NULL && strcmp(pool[i].name, name) == 0) {
            if (outItem != NULL) {
                *outItem = pool[i].item;
            }
            return true;
        }
    }
    return false;
}

bool Combo_GetForeignItemByName(const char* name, SharedItem* outItem) {
    return Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, name, outItem);
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
