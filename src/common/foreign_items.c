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

const char* Combo_GetForeignItemArticle(SharedItem item) {
    // Same origin-keyed walk as the name lookup — deliberately a separate entry
    // point rather than a second out-param, so the spoiler surfaces (which want
    // the bare name) are not forced to think about articles at all.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor(item.originGame, &pool);
    for (int i = 0; i < poolCount; i++) {
        if (pool[i].item.originGame == item.originGame && pool[i].item.id == item.id) {
            return pool[i].article;
        }
    }
    return NULL;
}

bool Combo_GetForeignItemByNameFor(uint8_t originGame, const char* name, SharedItem* outItem) {
    // The spoiler-LOAD inverse. Reused (not re-derived) so reconstruction shares
    // one source of truth with generation, and so a raw RG_*/RI_* is never
    // fabricated on the far side (ADR 0002). Scoped to ONE origin's pool: the
    // display names are not unique across pools ("Lens of Truth" is in both), so a
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

// ============================================================================
// Placement tables — one per DIRECTION (#493, ADR 0009 decision 3)
// ============================================================================
//
// There are two tables and they are separate key spaces, not one array split
// in two:
//
//   gComboCtx.foreignPlacements     keyed by MM   RandoCheckId -> OoT item
//   gComboCtx.foreignPlacementsOoT  keyed by OoT  RandomizerCheck -> MM item
//
// An OoT RC and an MM RC are unrelated enumerations that collide freely as raw
// u16s, which is exactly why one table with no host discriminator would
// false-positive across directions. ComboForeignPlacement cannot gain a host
// byte in place (its size and member offsets are static_asserted .redsave
// format), so the DIRECTION IS THE ACCESSOR: the table you pass is the host
// discriminator, and no lookup ever consults the other one.
//
// The bodies below are shared between directions on purpose. #493 names the
// duplication of five accessors as the accepted cost of the parallel carve;
// taking the table as a parameter pays it once instead of five times, so a fix
// to the duplicate scan cannot land in one direction and miss the other.

// A direction, for the log lines only. Behavior must never branch on this.
static const char* ForeignHostName(const ComboForeignPlacement* table) {
    return (table == gComboCtx.foreignPlacementsOoT) ? "OoT" : "MM";
}

static int ForeignPlaceInto(ComboForeignPlacement* table, uint16_t hostCheckId, SharedItem item) {
    if (hostCheckId == 0 || item.originGame == (uint8_t)GAME_NONE) {
        return -1; // check id 0 is each game's RC_UNKNOWN and never hosts; an untagged item must not enter
    }

    // ONE pass finds both the duplicate and the first free slot. Splitting it
    // into two passes is how a cross-block duplicate escapes when a second
    // block is carved later (see the RSBS_FOREIGN_PLACEMENT_CAP note in
    // context.h) — keep them fused.
    int firstFree = -1;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        ComboForeignPlacement* slot = &table[i];
        if (slot->item.originGame == (uint8_t)GAME_NONE) {
            if (firstFree < 0) {
                firstFree = i;
            }
            continue;
        }
        if (slot->mmCheckId == hostCheckId) {
            fprintf(stderr, "[ForeignItem] placement rejected: %s check %u already hosts origin=%u id=%u\n",
                    ForeignHostName(table), (unsigned)hostCheckId, (unsigned)slot->item.originGame,
                    (unsigned)slot->item.id);
            return -1; // one check hosts at most one foreign item
        }
    }

    if (firstFree < 0) {
        fprintf(stderr, "[ForeignItem] placement dropped: %s table full (%u slots), check=%u id=%u\n",
                ForeignHostName(table), RSBS_FOREIGN_PLACEMENT_CAP, (unsigned)hostCheckId, (unsigned)item.id);
        return -1;
    }

    ComboForeignPlacement* dst = &table[firstFree];
    dst->mmCheckId = hostCheckId;
    dst->item = item;
    fprintf(stderr, "[ForeignItem] placed origin=%u id=%u at %s check %u (slot %d)\n", (unsigned)item.originGame,
            (unsigned)item.id, ForeignHostName(table), (unsigned)hostCheckId, firstFree);
    return firstFree;
}

static const SharedItem* ForeignLookupIn(const ComboForeignPlacement* table, uint16_t hostCheckId) {
    if (hostCheckId == 0) {
        return NULL;
    }
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement* slot = &table[i];
        if (slot->item.originGame != (uint8_t)GAME_NONE && slot->mmCheckId == hostCheckId) {
            return &slot->item;
        }
    }
    return NULL;
}

static int ForeignCountIn(const ComboForeignPlacement* table) {
    int count = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        if (table[i].item.originGame != (uint8_t)GAME_NONE) {
            count++;
        }
    }
    return count;
}

static void ForeignClear(ComboForeignPlacement* table) {
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        table[i].mmCheckId = 0;
        table[i].item.originGame = (uint8_t)GAME_NONE;
        table[i].item.flags = 0;
        table[i].item.id = 0;
    }
}

// ---- Forward direction: MM checks host OoT items -----------------------------

int Combo_SetForeignPlacement(uint16_t mmCheckId, SharedItem item) {
    return ForeignPlaceInto(gComboCtx.foreignPlacements, mmCheckId, item);
}

const SharedItem* Combo_GetForeignPlacementForCheck(uint16_t mmCheckId) {
    return ForeignLookupIn(gComboCtx.foreignPlacements, mmCheckId);
}

int Combo_CountForeignPlacements(void) {
    return ForeignCountIn(gComboCtx.foreignPlacements);
}

void Combo_ClearForeignPlacements(void) {
    ForeignClear(gComboCtx.foreignPlacements);
}

// ---- Reverse direction: OoT checks host MM items -----------------------------

int Combo_SetForeignPlacementOoT(uint16_t ootCheckId, SharedItem item) {
    return ForeignPlaceInto(gComboCtx.foreignPlacementsOoT, ootCheckId, item);
}

const SharedItem* Combo_GetForeignPlacementForOoTCheck(uint16_t ootCheckId) {
    return ForeignLookupIn(gComboCtx.foreignPlacementsOoT, ootCheckId);
}

int Combo_CountForeignPlacementsOoT(void) {
    return ForeignCountIn(gComboCtx.foreignPlacementsOoT);
}

void Combo_ClearForeignPlacementsOoT(void) {
    ForeignClear(gComboCtx.foreignPlacementsOoT);
}
