/**
 * @file combo_tracker_view.c
 * @brief The combo tracker's view model (#458). See combo_tracker_view.h.
 *
 * Everything here is a pure read: gComboCtx through the foreign_items.h
 * accessors, the MM shadow blob through the registered offset descriptor, and
 * the OoT heap through the registered vtable. No gSaveContext through either
 * game's layout, no ImGui, no game headers, no caching — the view is
 * recomputed per call so progress made mid-session shows on the next frame
 * and no stale copy can outlive a .redsave Load.
 */

#include "combo_tracker_view.h"

#include "context.h" // Context_GetMMSaveContext / Context_GetCurrentGame
#include "game.h"    // MM_SAVE_CONTEXT_SIZE

#include <stdio.h>
#include <string.h>

// ============================================================================
// Adapter registries
// ============================================================================

// MM descriptor is stored BY VALUE (the registrant may pass a stack struct);
// the OoT vtable is stored by pointer (the OoT TU passes a static, and copying
// would not change the lifetime of what the pointers point at).
static ComboMMTrackerDesc sMMDesc;
static bool sMMRegistered = false;
static const ComboOoTTrackerOps* sOoTOps = NULL;

void Combo_Tracker_RegisterMM(const ComboMMTrackerDesc* desc) {
    if (desc == NULL) {
        sMMRegistered = false;
        memset(&sMMDesc, 0, sizeof(sMMDesc));
        return;
    }

    // Belt and braces under the MM TU's static_asserts: refuse geometry that
    // would read outside the shadow blob or outside a row. A rejected
    // descriptor leaves the adapter unregistered — UNAVAILABLE, never OOB.
    const uint64_t tableEnd =
        (uint64_t)desc->checkTableOffset + (uint64_t)desc->checkCount * (uint64_t)desc->checkStride;
    if (desc->checkStride == 0 || desc->checkCount == 0 || tableEnd > (uint64_t)MM_SAVE_CONTEXT_SIZE ||
        desc->shuffledOffset >= desc->checkStride || desc->obtainedOffset >= desc->checkStride ||
        desc->skippedOffset >= desc->checkStride || desc->newfLen > sizeof(desc->newf) ||
        (uint64_t)desc->newfOffset + desc->newfLen > (uint64_t)MM_SAVE_CONTEXT_SIZE ||
        (uint64_t)desc->saveTypeOffset + 4 > (uint64_t)MM_SAVE_CONTEXT_SIZE ||
        (uint64_t)desc->finalSeedOffset + 4 > (uint64_t)MM_SAVE_CONTEXT_SIZE) {
        fprintf(stderr, "[ComboTracker] REJECTED MM tracker descriptor: geometry reads outside the shadow blob\n");
        return;
    }

    sMMDesc = *desc;
    sMMRegistered = true;
}

const ComboMMTrackerDesc* Combo_Tracker_GetMMDesc(void) {
    return sMMRegistered ? &sMMDesc : NULL;
}

void Combo_Tracker_RegisterOoT(const ComboOoTTrackerOps* ops) {
    if (ops == NULL) {
        sOoTOps = NULL;
        return;
    }
    if (ops->summary == NULL || ops->checkCount == NULL || ops->checkAt == NULL || ops->checkName == NULL) {
        fprintf(stderr, "[ComboTracker] REJECTED OoT tracker vtable: NULL member\n");
        return;
    }
    sOoTOps = ops;
}

// ============================================================================
// Freshness
// ============================================================================

const char* Combo_TrackerFreshnessLabel(uint8_t game, uint8_t freshness) {
    switch (freshness) {
        case COMBO_TRACKER_FRESH_LIVE:
            return "live";
        case COMBO_TRACKER_FRESH_STALE:
            // The stale wording is per game because the mechanism differs: the
            // MM panel reads a shadow written at freeze/save time; the OoT
            // panel reads a heap that simply stopped advancing at suspend.
            return (game == (uint8_t)GAME_MM) ? "as of last freeze/save" : "as of suspend";
        case COMBO_TRACKER_FRESH_UNAVAILABLE:
            return "no data";
        default:
            return "(bad freshness)";
    }
}

// ============================================================================
// MM shadow reads
// ============================================================================

static uint32_t MMBlobReadU32(const uint8_t* blob, uint32_t offset) {
    uint32_t v;
    memcpy(&v, blob + offset, sizeof(v)); // offsets may be unaligned in principle
    return v;
}

/**
 * The shadow blob, or NULL when it holds no MM save. Context_GetMMSaveContext
 * never returns NULL (the storage is zero-padded at startup), so absence is
 * detected the way the .redsave slot list detects it: the 'ZELDA3' new-file
 * marker. An all-zero shadow — MM never entered this session — fails the
 * compare and reads as UNAVAILABLE rather than as a vanilla save with zero
 * progress.
 */
static const uint8_t* MMBlobIfPresent(void) {
    if (!sMMRegistered) {
        return NULL;
    }
    const uint8_t* blob = (const uint8_t*)Context_GetMMSaveContext();
    if (blob == NULL) {
        return NULL;
    }
    if (sMMDesc.newfLen > 0 && memcmp(blob + sMMDesc.newfOffset, sMMDesc.newf, sMMDesc.newfLen) != 0) {
        return NULL;
    }
    return blob;
}

static void MMSummary(ComboTrackerGameSummary* out) {
    const uint8_t* blob = MMBlobIfPresent();
    if (blob == NULL) {
        return; // caller pre-zeroed: UNAVAILABLE
    }

    // Never LIVE, even while MM is the active game: the shadow is written at
    // freeze/save time and lags the live gSaveContext (see the header).
    out->freshness = COMBO_TRACKER_FRESH_STALE;
    out->hasWorld = MMBlobReadU32(blob, sMMDesc.saveTypeOffset) == sMMDesc.saveTypeRando;
    out->seed = out->hasWorld ? MMBlobReadU32(blob, sMMDesc.finalSeedOffset) : 0;
    out->totalChecks = (int)sMMDesc.checkCount;

    const uint8_t* table = blob + sMMDesc.checkTableOffset;
    for (uint32_t i = 0; i < sMMDesc.checkCount; i++) {
        const uint8_t* row = table + (size_t)i * sMMDesc.checkStride;
        if (row[sMMDesc.shuffledOffset] == 0) {
            continue;
        }
        out->shuffled++;
        if (row[sMMDesc.obtainedOffset] != 0) {
            out->obtained++;
        } else if (row[sMMDesc.skippedOffset] != 0) {
            out->skipped++;
        }
    }
}

// ============================================================================
// The per-game reads
// ============================================================================

void Combo_TrackerGameSummary(uint8_t game, ComboTrackerGameSummary* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out)); // freshness == UNAVAILABLE unless data lands

    if (game == (uint8_t)GAME_MM) {
        MMSummary(out);
        return;
    }
    if (game == (uint8_t)GAME_OOT) {
        if (sOoTOps == NULL || !sOoTOps->summary(out)) {
            memset(out, 0, sizeof(*out)); // adapter must not half-fill an unavailable summary
            return;
        }
        // Freshness is the VIEW's call, not the adapter's: live only while
        // OoT is the running game; otherwise the suspended heap.
        out->freshness = (Context_GetCurrentGame() == GAME_OOT) ? COMBO_TRACKER_FRESH_LIVE : COMBO_TRACKER_FRESH_STALE;
        return;
    }
    // GAME_NONE / out of range: stays UNAVAILABLE zeros.
}

int Combo_TrackerCheckCount(uint8_t game) {
    if (game == (uint8_t)GAME_MM) {
        return (MMBlobIfPresent() != NULL) ? (int)sMMDesc.checkCount : 0;
    }
    if (game == (uint8_t)GAME_OOT && sOoTOps != NULL) {
        return sOoTOps->checkCount();
    }
    return 0;
}

bool Combo_TrackerCheckAt(uint8_t game, int index, ComboTrackerCheckRow* out) {
    if (out == NULL || index < 0) {
        return false;
    }
    if (game == (uint8_t)GAME_MM) {
        const uint8_t* blob = MMBlobIfPresent();
        if (blob == NULL || (uint32_t)index >= sMMDesc.checkCount) {
            return false;
        }
        const uint8_t* row = blob + sMMDesc.checkTableOffset + (size_t)index * sMMDesc.checkStride;
        out->checkId = (uint16_t)index;
        out->name = (sMMDesc.checkName != NULL) ? sMMDesc.checkName((uint16_t)index) : NULL;
        out->shuffled = row[sMMDesc.shuffledOffset] != 0;
        out->obtained = row[sMMDesc.obtainedOffset] != 0;
        out->skipped = row[sMMDesc.skippedOffset] != 0;
        return true;
    }
    if (game == (uint8_t)GAME_OOT && sOoTOps != NULL) {
        return sOoTOps->checkAt(index, out);
    }
    return false;
}

const char* Combo_TrackerCheckName(uint8_t game, uint16_t checkId) {
    if (game == (uint8_t)GAME_MM) {
        return (sMMRegistered && sMMDesc.checkName != NULL) ? sMMDesc.checkName(checkId) : NULL;
    }
    if (game == (uint8_t)GAME_OOT && sOoTOps != NULL) {
        return sOoTOps->checkName(checkId);
    }
    return NULL;
}

// ============================================================================
// Identity + cross-game placements
// ============================================================================

void Combo_TrackerIdentity(ComboTrackerIdentity* out) {
    if (out == NULL) {
        return;
    }
    out->paired = Combo_ForeignPairingActive();
    out->sharedRandoSeed = gComboCtx.sharedRandoSeed;
    out->sharedRandoSettingsHash = gComboCtx.sharedRandoSettingsHash;
    out->mmProfileDigest = gComboCtx.mmProfileDigest;
    out->mmHostedForeign = Combo_TrackerForeignCount((uint8_t)GAME_MM);
    out->ootHostedForeign = Combo_TrackerForeignCount((uint8_t)GAME_OOT);
}

/**
 * Same redeemed derivation the spoiler view uses: a linear (originGame, id)
 * scan of the tagged array, flags deliberately not part of the match so
 * SOURCED entries (ADR 0005) report their redeemed bit honestly.
 */
static bool TrackerItemRedeemed(uint8_t originGame, uint16_t itemId) {
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_CAP; i++) {
        const SharedItem* slot = &gComboCtx.sharedItemsTagged[i];
        if (slot->originGame == originGame && slot->id == itemId && (slot->flags & RSBS_SHARED_ITEM_REDEEMED) != 0) {
            return true;
        }
    }
    return false;
}

/**
 * The direction is the accessor (ADR 0009 decision 3): the two placement
 * tables are separate key spaces, so `hostGame` selects the TABLE and nothing
 * ever looks one up with the other's key.
 */
static const ComboForeignPlacement* ForeignTableFor(uint8_t hostGame) {
    if (hostGame == (uint8_t)GAME_MM) {
        return gComboCtx.foreignPlacements;
    }
    if (hostGame == (uint8_t)GAME_OOT) {
        return gComboCtx.foreignPlacementsOoT;
    }
    return NULL;
}

int Combo_TrackerForeignCount(uint8_t hostGame) {
    if (!Combo_ForeignPairingActive()) {
        return 0; // "not paired", not "no crossings" — same rule as the spoiler view
    }
    if (hostGame == (uint8_t)GAME_MM) {
        return Combo_CountForeignPlacements();
    }
    if (hostGame == (uint8_t)GAME_OOT) {
        return Combo_CountForeignPlacementsOoT();
    }
    return 0;
}

bool Combo_TrackerForeignRowAt(uint8_t hostGame, int index, ComboTrackerForeignRow* out) {
    if (out == NULL || index < 0 || !Combo_ForeignPairingActive()) {
        return false;
    }
    const ComboForeignPlacement* table = ForeignTableFor(hostGame);
    if (table == NULL) {
        return false;
    }

    // Slot order, counting only occupied slots — row N is the Nth crossing as
    // serialized, and occupancy is the item tag (no count field to disagree).
    int seen = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement* slot = &table[i];
        if (slot->item.originGame == (uint8_t)GAME_NONE) {
            continue;
        }
        if (seen++ != index) {
            continue;
        }

        const char* itemName = Combo_GetForeignItemName(slot->item);
        out->hostGame = hostGame;
        out->hostCheckId = slot->mmCheckId; // the member NAME is mmCheckId; in the
                                            // OoT table it holds an OoT RC (context.h)
        out->hostCheckName = Combo_TrackerCheckName(hostGame, slot->mmCheckId);
        out->originGame = slot->item.originGame;
        out->itemId = slot->item.id;
        out->itemName = (itemName != NULL) ? itemName : RSBS_TRACKER_UNKNOWN_ITEM_NAME;
        out->redeemed = TrackerItemRedeemed(slot->item.originGame, slot->item.id);
        return true;
    }
    return false;
}
