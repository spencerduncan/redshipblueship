/**
 * @file combo_tracker_view.c
 * @brief The combo tracker's view model (#458). See combo_tracker_view.h.
 *
 * Everything here is a pure read. The one write, the skip toggle (#458 U5), is
 * made by the ACTIVE game's adapter in its own TU, on that game's live state
 * only; this file only decides when to ask (see Combo_TrackerSetSkipped). The
 * reads: gComboCtx through the foreign_items.h
 * accessors, the crossing store through crossing_store.h, the MM shadow blob
 * (or, while MM is played, the live save the MM adapter hands out, #799)
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
        (uint64_t)desc->createdAtOffset + 8 > (uint64_t)MM_SAVE_CONTEXT_SIZE ||
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
    if (ops->summary == NULL || ops->checkCount == NULL || ops->checkAt == NULL || ops->checkName == NULL ||
        (ops->skipWritable == NULL) != (ops->setSkipped == NULL)) {
        fprintf(stderr, "[ComboTracker] REJECTED OoT tracker vtable: NULL member\n");
        return;
    }
    sOoTOps = ops;
}

// ============================================================================
// Freshness
// ============================================================================

static bool MMShadowNeverEntered(void);

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
            // MM's half of a paired file that MM has never loaded: the creation
            // event armed it and nothing has written it since, so "the last game
            // switch or save" would name an event that never happened. Keyed on
            // the save's creation stamp, which MM writes on its first load
            // (#765; see MMShadowNeverEntered).
            if (game == (uint8_t)GAME_MM && MMShadowNeverEntered()) {
                return "As of file creation";
            }
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
 * The marker alone, again (#765). #755 widened this gate to also accept a
 * randomized save type, because the paired creation event armed MM's half
 * without the marker. The creation event now stamps it the way MM's own
 * file-select new-file path does (MM_Creation_StampNewFileFields), so every MM
 * half that exists carries it and the second proof has nothing left to prove.
 * A .redsave whose MM half was created before that stamp still reads as no
 * data here (pre-release saves; stated in the PR that removed the widening).
 */
static bool MMBlobMarked(const uint8_t* blob) {
    return blob != NULL &&
           (sMMDesc.newfLen == 0 || memcmp(blob + sMMDesc.newfOffset, sMMDesc.newf, sMMDesc.newfLen) == 0);
}

static const uint8_t* MMShadowIfPresent(void) {
    if (!sMMRegistered) {
        return NULL;
    }
    const uint8_t* blob = (const uint8_t*)Context_GetMMSaveContext();
    return MMBlobMarked(blob) ? blob : NULL;
}

/**
 * THE SOURCE PICK (#799): every MM read goes through here, so the summary, the
 * check rows, the crossings' found state and their note all switch source
 * together. Live for the active game, the snapshot for the other:
 *
 *   - LIVE: MM is the active game, its adapter hands out the live save (it
 *     does only while MM's play state is loaded; ADR 0008 rule 5's amendment),
 *     and that save carries the marker. This is the window the shadow cannot
 *     cover: MM's arrival consumes the shadow into gSaveContext and zeroes it
 *     (Combo_ConsumeFrozenState), and nothing refills it before MM's first save
 *     or the departure freeze, so a shadow-only reader said "No data yet"
 *     while MM was being played.
 *   - STALE: otherwise, a marked shadow. A refused arrival never consumes the
 *     shadow, and MM then plays the boot chain's unmarked bootstrap save, so
 *     the pick lands here and still shows the armed world.
 *   - UNAVAILABLE: neither (NULL returned).
 *
 * `outFreshness` may be NULL.
 */
static const uint8_t* MMBlobIfPresent(uint8_t* outFreshness) {
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const uint8_t* blob = NULL;
    if (sMMRegistered) {
        if (Context_GetCurrentGame() == GAME_MM && sMMDesc.liveSave != NULL) {
            const uint8_t* live = (const uint8_t*)sMMDesc.liveSave();
            if (MMBlobMarked(live)) {
                blob = live;
                freshness = COMBO_TRACKER_FRESH_LIVE;
            }
        }
        if (blob == NULL) {
            blob = MMShadowIfPresent();
            freshness = (blob != NULL) ? COMBO_TRACKER_FRESH_STALE : COMBO_TRACKER_FRESH_UNAVAILABLE;
        }
    }
    if (outFreshness != NULL) {
        *outFreshness = freshness;
    }
    return blob;
}

static uint64_t MMBlobReadU64(const uint8_t* blob, uint32_t offset) {
    uint64_t v;
    memcpy(&v, blob + offset, sizeof(v));
    return v;
}

/**
 * A resident MM save that MM itself has never loaded: its creation stamp
 * (ShipSaveInfo.fileCreatedAt) is still zero. MM writes that stamp from
 * OnSaveLoad, which MM's file select dispatches as it creates or loads a file
 * and a paired half first sees at its first arrival (z_play.c), so a zero
 * stamp is exactly the half the creation event armed and nothing has loaded
 * since. (#755 keyed this on the missing marker, which MM's arrival and
 * departure never restore, so after a first visit the label still said "As of
 * file creation".) It is a question about the SHADOW, the only source a stale
 * label describes, so it reads the shadow and never the live save (#799).
 */
static bool MMShadowNeverEntered(void) {
    const uint8_t* blob = MMShadowIfPresent();
    return blob != NULL && MMBlobReadU64(blob, sMMDesc.createdAtOffset) == 0;
}

static void MMSummary(ComboTrackerGameSummary* out) {
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const uint8_t* blob = MMBlobIfPresent(&freshness);
    if (blob == NULL) {
        return; // caller pre-zeroed: UNAVAILABLE
    }

    // LIVE while MM is played (its live save), STALE from the shadow otherwise
    // (MMBlobIfPresent's source pick; see the header).
    out->freshness = freshness;
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
        return (MMBlobIfPresent(NULL) != NULL) ? (int)sMMDesc.checkCount : 0;
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
    memset(out, 0, sizeof(*out));
    if (game == (uint8_t)GAME_MM) {
        const uint8_t* blob = MMBlobIfPresent(NULL);
        if (blob == NULL || (uint32_t)index >= sMMDesc.checkCount) {
            return false;
        }
        const uint8_t* row = blob + sMMDesc.checkTableOffset + (size_t)index * sMMDesc.checkStride;
        out->checkId = (uint16_t)index;
        out->name = (sMMDesc.checkName != NULL) ? sMMDesc.checkName((uint16_t)index) : NULL;
        out->shuffled = row[sMMDesc.shuffledOffset] != 0;
        out->obtained = row[sMMDesc.obtainedOffset] != 0;
        out->skipped = row[sMMDesc.skippedOffset] != 0;
        // #458 U4. MM's status is its two flags and nothing more (the header's
        // ComboTrackerCheckStatus): obtained wins, as in MM's own tracker.
        out->status = out->obtained  ? (uint8_t)COMBO_TRACKER_CHECK_COLLECTED
                      : out->skipped ? (uint8_t)COMBO_TRACKER_CHECK_SKIPPED
                                     : (uint8_t)COMBO_TRACKER_CHECK_UNCHECKED;
        if (sMMDesc.areaName != NULL) {
            out->areaName = sMMDesc.areaName((uint16_t)index, &out->areaKey);
        }
        // Only a found check names its item, as MM's tracker prints one only
        // beside an obtained check; the MM TU reads it from this same save.
        if (out->obtained && sMMDesc.placedItemName != NULL) {
            uint8_t itemGame = (uint8_t)GAME_NONE;
            out->placedItemName = sMMDesc.placedItemName(blob, (uint16_t)index, &itemGame);
            out->placedItemGame = (out->placedItemName != NULL) ? itemGame : (uint8_t)GAME_NONE;
        }
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
// The skip toggle (#458 U5)
// ============================================================================

/**
 * Whether MM takes a skip write now: its adapter has the op, and
 * MMBlobIfPresent's pick reads the live save LIVE (MM active, play state
 * loaded, marked) and it holds a randomized world. This only reads; the write
 * is the MM TU's own op (setLiveSkipped), which checks the same conditions in
 * MM's layout. A STALE panel (the shadow) has no write at all.
 */
static bool MMSkipWritable(void) {
    if (!sMMRegistered || sMMDesc.setLiveSkipped == NULL || sMMDesc.liveSave == NULL ||
        Context_GetCurrentGame() != GAME_MM) {
        return false;
    }
    const uint8_t* live = (const uint8_t*)sMMDesc.liveSave();
    return MMBlobMarked(live) && MMBlobReadU32(live, sMMDesc.saveTypeOffset) == sMMDesc.saveTypeRando;
}

/** OoT takes skip writes only through a vtable that has them, while OoT is played. */
static bool OoTSkipWritable(void) {
    return sOoTOps != NULL && sOoTOps->skipWritable != NULL && Context_GetCurrentGame() == GAME_OOT &&
           sOoTOps->skipWritable();
}

bool Combo_TrackerSkipWritable(uint8_t game) {
    if (game == (uint8_t)GAME_MM) {
        return MMSkipWritable();
    }
    if (game == (uint8_t)GAME_OOT) {
        return OoTSkipWritable();
    }
    return false;
}

bool Combo_TrackerRowSkippable(uint8_t game, const ComboTrackerCheckRow* row) {
    return row != NULL && row->shuffled && !row->obtained && Combo_TrackerSkipWritable(game);
}

bool Combo_TrackerSetSkipped(uint8_t game, uint16_t checkId, bool skipped) {
    if (game == (uint8_t)GAME_MM) {
        // MM's TU writes the byte MM's own tracker flips (ADR 0008 rule 5: common
        // code only reads MM's save); MM's next save persists it and the
        // departure freeze carries it into the shadow.
        return MMSkipWritable() && (uint32_t)checkId < sMMDesc.checkCount && sMMDesc.setLiveSkipped(checkId, skipped);
    }
    if (game == (uint8_t)GAME_OOT) {
        return OoTSkipWritable() && sOoTOps->setSkipped(checkId, skipped);
    }
    return false;
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
        const uint8_t* blob = MMBlobIfPresent(NULL);
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
    // The host game's freshness, by the per-game panel's own rule: each game's
    // data is live only while that game is played (MM: MMBlobIfPresent's pick).
    if (hostGame == (uint8_t)GAME_MM) {
        (void)MMBlobIfPresent(&out->freshness);
    } else if (hostGame == (uint8_t)GAME_OOT && sOoTOps != NULL && sOoTOps->checkCount() > 0) {
        out->freshness = (Context_GetCurrentGame() == GAME_OOT) ? COMBO_TRACKER_FRESH_LIVE : COMBO_TRACKER_FRESH_STALE;
    } else {
        out->freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    }
}
