/**
 * @file combo_tracker_view.c
 * @brief The combo tracker's view model (#458). See combo_tracker_view.h.
 *
 * Everything here is a pure read: gComboCtx through the foreign_items.h
 * accessors, the crossing store through crossing_store.h, the MM shadow blob
 * through the registered offset descriptor, and the OoT heap through the
 * registered vtable. No gSaveContext through either
 * game's layout, no ImGui, no game headers, no caching — the view is
 * recomputed per call so progress made mid-session shows on the next frame
 * and no stale copy can outlive a .redsave Load.
 */

#include "combo_tracker_view.h"

#include "context.h"        // Context_GetMMSaveContext / Context_GetCurrentGame
#include "crossing_store.h" // Combo_Crossings_Count / Combo_Crossings_At (#755)
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
        // Player wording (the window prints it as a gray note): a game switch is
        // what freezes MM's shadow and suspends OoT's heap.
        case COMBO_TRACKER_FRESH_LIVE:
            return "Updated live";
        case COMBO_TRACKER_FRESH_STALE:
            // The stale wording is per game because the mechanism differs: the
            // MM panel reads a shadow written at freeze/save time; the OoT
            // panel reads a heap that simply stopped advancing at suspend.
            return (game == (uint8_t)GAME_MM) ? "As of the last game switch or save" : "As of the last game switch";
        case COMBO_TRACKER_FRESH_UNAVAILABLE:
            return "No data";
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
 *
 * A RANDOMIZED save type is the second proof of a resident MM world (#755). The
 * paired creation event arms MM's half of a new world in this shadow without
 * MM's file-select marker: observed on the ComboSingleBag pinned seed after
 * OoT_Creation_AuthorRandoFile, newf is six zero bytes and saveType is
 * SAVETYPE_RANDO. Gated on the marker alone, a fresh paired world read "no data"
 * for MM and none of its MM-hosted crossings could say whether it was found.
 * SAVETYPE_RANDO is nonzero, so an all-zero shadow still reads as absent.
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
        if (sMMDesc.saveTypeRando == 0 || MMBlobReadU32(blob, sMMDesc.saveTypeOffset) != sMMDesc.saveTypeRando) {
            return NULL;
        }
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
// Identity + cross-game crossings (#755, #757)
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
 * The direction is the accessor (ADR 0009 decision 3): the two pinned tables
 * are separate key spaces, so `hostGame` selects the TABLE and nothing ever
 * looks one up with the other's key.
 */
static const ComboForeignPlacement* PinnedTableFor(uint8_t hostGame) {
    if (hostGame == (uint8_t)GAME_MM) {
        return gComboCtx.foreignPlacements;
    }
    if (hostGame == (uint8_t)GAME_OOT) {
        return gComboCtx.foreignPlacementsOoT;
    }
    return NULL;
}

/** Does `table` hold an occupied pinned slot for `hostCheck`? Occupancy is the
 *  item tag, exactly as Combo_CountForeignPlacements derives it. */
static bool PinnedHasHost(const ComboForeignPlacement* table, uint16_t hostCheck) {
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        if (table[i].item.originGame != (uint8_t)GAME_NONE && table[i].mmCheckId == hostCheck) {
            return true;
        }
    }
    return false;
}

/**
 * The `index`-th crossing of `hostGame` as a (host check, item) pair, in the
 * order the header documents: pinned slots first, then the store's rows whose
 * host has no pinned slot. With `index < 0` nothing is returned and the walk
 * only counts. @return the number of crossings `hostGame` hosts; `*outFound` is
 * set when row `index` exists.
 */
static int CrossingWalk(uint8_t hostGame, int index, uint16_t* outHost, SharedItem* outItem, bool* outFound) {
    *outFound = false;
    const ComboForeignPlacement* table = PinnedTableFor(hostGame);
    if (table == NULL) {
        return 0;
    }
    int seen = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement* slot = &table[i];
        if (slot->item.originGame == (uint8_t)GAME_NONE) {
            continue;
        }
        if (seen++ == index) {
            // The member NAME is mmCheckId; in the OoT table it holds an OoT RC.
            *outHost = slot->mmCheckId;
            *outItem = slot->item;
            *outFound = true;
            return seen;
        }
    }
    const int storeCount = Combo_Crossings_Count((GameId)hostGame);
    for (int i = 0; i < storeCount; i++) {
        ComboCrossing c;
        if (!Combo_Crossings_At((GameId)hostGame, i, &c) || PinnedHasHost(table, c.hostCheck)) {
            continue; // shadowed by a pinned row: the give path never reads it
        }
        if (seen++ == index) {
            *outHost = c.hostCheck;
            *outItem = c.item;
            *outFound = true;
            return seen;
        }
    }
    return seen;
}

/**
 * Has `hostGame`'s own save collected `hostCheck`? Read through the same
 * adapters the per-game panels use, so the answer carries their freshness.
 */
static uint8_t HostCheckFound(uint8_t hostGame, uint16_t hostCheck) {
    if (hostGame == (uint8_t)GAME_MM) {
        const uint8_t* blob = MMBlobIfPresent();
        if (blob == NULL || (uint32_t)hostCheck >= sMMDesc.checkCount ||
            MMBlobReadU32(blob, sMMDesc.saveTypeOffset) != sMMDesc.saveTypeRando) {
            return COMBO_TRACKER_FOUND_UNKNOWN;
        }
        const uint8_t* row = blob + sMMDesc.checkTableOffset + (size_t)hostCheck * sMMDesc.checkStride;
        return row[sMMDesc.obtainedOffset] != 0 ? COMBO_TRACKER_FOUND_YES : COMBO_TRACKER_FOUND_NO;
    }
    if (hostGame == (uint8_t)GAME_OOT && sOoTOps != NULL) {
        // OoT's own adapter indexes rows by check id; any other registrant (the
        // UI snapshot's synthetic world) is searched, so the answer never
        // depends on that coincidence.
        ComboTrackerCheckRow row;
        if (sOoTOps->checkAt((int)hostCheck, &row) && row.checkId == hostCheck) {
            return row.obtained ? COMBO_TRACKER_FOUND_YES : COMBO_TRACKER_FOUND_NO;
        }
        const int count = sOoTOps->checkCount();
        for (int i = 0; i < count; i++) {
            if (sOoTOps->checkAt(i, &row) && row.checkId == hostCheck) {
                return row.obtained ? COMBO_TRACKER_FOUND_YES : COMBO_TRACKER_FOUND_NO;
            }
        }
    }
    return COMBO_TRACKER_FOUND_UNKNOWN;
}

int Combo_TrackerForeignCount(uint8_t hostGame) {
    if (!Combo_ForeignPairingActive()) {
        return 0; // "not paired", not "no crossings" — same rule as the spoiler view
    }
    uint16_t host = 0;
    SharedItem item;
    bool found = false;
    return CrossingWalk(hostGame, -1, &host, &item, &found);
}

bool Combo_TrackerForeignRowAt(uint8_t hostGame, int index, ComboTrackerForeignRow* out) {
    if (out == NULL || index < 0 || !Combo_ForeignPairingActive()) {
        return false;
    }
    uint16_t host = 0;
    SharedItem item;
    bool found = false;
    (void)CrossingWalk(hostGame, index, &host, &item, &found);
    if (!found) {
        return false;
    }

    const char* itemName = Combo_GetForeignItemName(item);
    const char* article = Combo_GetForeignItemArticle(item);
    out->hostGame = hostGame;
    out->hostCheckId = host;
    out->hostCheckName = Combo_TrackerCheckName(hostGame, host);
    out->originGame = item.originGame;
    out->itemId = item.id;
    out->itemName = (itemName != NULL) ? itemName : RSBS_TRACKER_UNKNOWN_ITEM_NAME;
    out->itemArticle = (article != NULL) ? article : "";
    out->found = HostCheckFound(hostGame, host);
    return true;
}

void Combo_TrackerForeignProgress(uint8_t hostGame, ComboTrackerForeignProgress* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->total = Combo_TrackerForeignCount(hostGame);
    for (int i = 0; i < out->total; i++) {
        ComboTrackerForeignRow row;
        if (Combo_TrackerForeignRowAt(hostGame, i, &row) && row.found == COMBO_TRACKER_FOUND_YES) {
            out->found++;
        }
    }
    // The host game's freshness, by the per-game panel's own rule: MM's data is
    // never live, OoT's is live only while OoT runs.
    if (hostGame == (uint8_t)GAME_MM) {
        out->freshness = (MMBlobIfPresent() != NULL) ? COMBO_TRACKER_FRESH_STALE : COMBO_TRACKER_FRESH_UNAVAILABLE;
    } else if (hostGame == (uint8_t)GAME_OOT && sOoTOps != NULL && sOoTOps->checkCount() > 0) {
        out->freshness = (Context_GetCurrentGame() == GAME_OOT) ? COMBO_TRACKER_FRESH_LIVE : COMBO_TRACKER_FRESH_STALE;
    } else {
        out->freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    }
}
