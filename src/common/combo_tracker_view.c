/**
 * @file combo_tracker_view.c
 * @brief The combo tracker's view model (#458). See combo_tracker_view.h.
 *
 * Everything here is a pure read but for two writes. The skip toggle (#458 U5)
 * is made by the check's own game's adapter in its own TU, on that game's live
 * state or, for the far pane, its frozen MM half or suspended OoT heap (#1294);
 * this file only decides when to ask (see Combo_TrackerSetSkipped). The OoT skip
 * journal (#1294) is gComboCtx's, game-neutral u16s kept here for OoT's adapter
 * and save seams. The reads: gComboCtx through the foreign_items.h
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
#include "game.h"           // MM_SAVE_CONTEXT_SIZE

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
        desc->skippedOffset >= desc->checkStride || (desc->seenOffset != 0 && desc->seenOffset >= desc->checkStride) ||
        (desc->priceOffset != 0 && (uint64_t)desc->priceOffset + 2 > (uint64_t)desc->checkStride) ||
        desc->newfLen > sizeof(desc->newf) ||
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
    if (ops->summary == NULL || ops->checkCount == NULL || ops->checkAt == NULL || ops->checkName == NULL ||
        (ops->skipWritable == NULL) != (ops->setSkipped == NULL) ||
        (ops->suspendedSkipWritable == NULL) != (ops->setSuspendedSkipped == NULL)) {
        fprintf(stderr, "[ComboTracker] REJECTED OoT tracker vtable: NULL member\n");
        return;
    }
    sOoTOps = ops;
}

// ============================================================================
// Freshness
// ============================================================================

const char* Combo_TrackerFreshnessLabel(uint8_t game, uint8_t freshness) {
    // The operator's rule for our windows (#927): a caption goes unless it is
    // the label of what is drawn, and freshness is not one. A panel is labelled
    // by its game's header alone, as SoH's trackers label theirs; the freshness
    // stays a field, which the windows still act on (skip writes, found state).
    (void)game;
    (void)freshness;
    return "";
}

// ============================================================================
// Reachability of the inactive game: the departure captures (#1129)
// ============================================================================

/** `game`'s capture record in gComboCtx: its bits, its count and its capacity in ids; NULL for any other game. */
static uint8_t* FrozenRecord(uint8_t game, uint16_t** outCount, uint32_t* outCap) {
    if (game == (uint8_t)GAME_OOT) {
        *outCount = &gComboCtx.trackerAvailCountOoT;
        *outCap = RSBS_TRACKER_AVAIL_OOT_CAP;
        return gComboCtx.trackerAvailOoT;
    }
    if (game == (uint8_t)GAME_MM) {
        *outCount = &gComboCtx.trackerAvailCountMM;
        *outCap = RSBS_TRACKER_AVAIL_MM_CAP;
        return gComboCtx.trackerAvailMM;
    }
    return NULL;
}

void Combo_TrackerFreezeAvailability(uint8_t game, const uint8_t* bits, uint32_t checkCount) {
    uint16_t* count = NULL;
    uint32_t cap = 0;
    uint8_t* store = FrozenRecord(game, &count, &cap);
    if (store == NULL) {
        return;
    }
    // Cleared first: every path below that stores nothing leaves "no capture".
    memset(store, 0, cap / 8);
    *count = 0;
    if (bits == NULL || checkCount == 0) {
        return;
    }
    if (checkCount > cap) {
        fprintf(stderr, "[ComboTracker] REFUSED %s availability capture: %u check ids past the record's %u\n",
                game == (uint8_t)GAME_OOT ? "OoT" : "MM", (unsigned)checkCount, (unsigned)cap);
        return;
    }
    memcpy(store, bits, (checkCount + 7u) / 8u);
    if ((checkCount & 7u) != 0) {
        // The last byte's bits past the count are not part of the capture.
        store[checkCount >> 3] &= (uint8_t)((1u << (checkCount & 7u)) - 1u);
    }
    *count = (uint16_t)checkCount;
}

int Combo_TrackerFrozenAvailability(uint8_t game, uint16_t checkId) {
    uint16_t* count = NULL;
    uint32_t cap = 0;
    const uint8_t* store = FrozenRecord(game, &count, &cap);
    if (store == NULL || checkId >= *count) {
        return -1;
    }
    return (store[checkId >> 3] >> (checkId & 7u)) & 1;
}

/** How many listed, unobtained, unskipped OoT checks the departure capture marks available, or -1 with no capture. */
static int OoTStaleAvailableCount(void) {
    if (gComboCtx.trackerAvailCountOoT == 0 || sOoTOps == NULL) {
        return -1;
    }
    const int count = sOoTOps->checkCount();
    int n = 0;
    for (int i = 0; i < count; i++) {
        ComboTrackerCheckRow row;
        if (!sOoTOps->checkAt(i, &row)) {
            break;
        }
        if (row.shuffled && !row.obtained && !row.skipped &&
            Combo_TrackerFrozenAvailability((uint8_t)GAME_OOT, row.checkId) == 1) {
            n++;
        }
    }
    return n;
}

// ============================================================================
// MM shadow reads
// ============================================================================

static uint32_t MMBlobReadU32(const uint8_t* blob, uint32_t offset) {
    uint32_t v;
    memcpy(&v, blob + offset, sizeof(v)); // offsets may be unaligned in principle
    return v;
}

static uint16_t MMRowReadU16(const uint8_t* row, uint32_t offset) {
    uint16_t v;
    memcpy(&v, row + offset, sizeof(v));
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

/**
 * MM's answer for check `id` on a panel of `freshness` (#1129): the adapter's
 * in-logic set on the LIVE panel, the departure capture on the STALE one, never
 * the other way round; -1 when there is no answer.
 */
static int MMAvailability(uint8_t freshness, uint16_t id) {
    if (freshness == COMBO_TRACKER_FRESH_LIVE) {
        return (sMMDesc.liveAvailable != NULL) ? sMMDesc.liveAvailable(id) : -1;
    }
    if (freshness == COMBO_TRACKER_FRESH_STALE) {
        return Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, id);
    }
    return -1;
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
    // The answer for one id says whether the set is known at all (#1129).
    out->availabilityKnown = out->hasWorld && MMAvailability(freshness, 0) >= 0;

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
        } else if (out->availabilityKnown && MMAvailability(freshness, (uint16_t)i) == 1) {
            out->available++;
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
        if (out->freshness == COMBO_TRACKER_FRESH_STALE) {
            // The snapshot's reachability is the departure capture (#1129), not
            // whatever the suspended heap's bits say: the adapter answers only live.
            const int captured = OoTStaleAvailableCount();
            out->availabilityKnown = captured >= 0;
            out->available = captured >= 0 ? captured : 0;
        }
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
        uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
        const uint8_t* blob = MMBlobIfPresent(&freshness);
        if (blob == NULL || (uint32_t)index >= sMMDesc.checkCount) {
            return false;
        }
        const uint8_t* row = blob + sMMDesc.checkTableOffset + (size_t)index * sMMDesc.checkStride;
        const uint8_t seen = (sMMDesc.seenOffset != 0) ? row[sMMDesc.seenOffset] : 0;
        out->checkId = (uint16_t)index;
        out->name = (sMMDesc.checkName != NULL) ? sMMDesc.checkName((uint16_t)index) : NULL;
        out->shuffled = row[sMMDesc.shuffledOffset] != 0;
        out->obtained = row[sMMDesc.obtainedOffset] != 0;
        out->skipped = row[sMMDesc.skippedOffset] != 0;
        // #458 U4. MM's status is its obtained, skipped and seen bytes (the
        // header's ComboTrackerCheckStatus): obtained wins, then skipped, then seen.
        out->status = out->obtained  ? (uint8_t)COMBO_TRACKER_CHECK_COLLECTED
                      : out->skipped ? (uint8_t)COMBO_TRACKER_CHECK_SKIPPED
                      : seen != 0    ? (uint8_t)COMBO_TRACKER_CHECK_SEEN
                                     : (uint8_t)COMBO_TRACKER_CHECK_UNCHECKED;
        if (sMMDesc.areaName != NULL) {
            out->areaName = sMMDesc.areaName((uint16_t)index, &out->areaKey);
        }
        // A found or seen check names its item from this same save; an identified
        // purchasable check (seen >= 2) also prints its price.
        if (out->obtained && sMMDesc.placedItemName != NULL) {
            uint8_t itemGame = (uint8_t)GAME_NONE;
            out->placedItemName = sMMDesc.placedItemName(blob, (uint16_t)index, &itemGame);
            out->placedItemGame = (out->placedItemName != NULL) ? itemGame : (uint8_t)GAME_NONE;
        } else if (seen != 0 && sMMDesc.placedItemName != NULL) {
            ComboTrackerSeenItem seenItem;
            memset(&seenItem, 0, sizeof(seenItem));
            seenItem.game = (uint8_t)GAME_MM;
            seenItem.saleKind = (uint8_t)COMBO_TRACKER_SALE_NONE;
            seenItem.identified = seen >= 2;
            seenItem.listed = out->shuffled;
            seenItem.canBePurchased = sMMDesc.canBePurchased != NULL && sMMDesc.canBePurchased((uint16_t)index);
            seenItem.itemGame = (uint8_t)GAME_NONE;
            seenItem.itemName = sMMDesc.placedItemName(blob, (uint16_t)index, &seenItem.itemGame);
            seenItem.price = (sMMDesc.priceOffset != 0) ? MMRowReadU16(row, sMMDesc.priceOffset) : 0;
            Combo_TrackerNameSeenItem(&seenItem, out);
        }
        // #1129: the live in-logic set or the departure capture, by the pick above.
        if (MMBlobReadU32(blob, sMMDesc.saveTypeOffset) == sMMDesc.saveTypeRando) {
            const int available = MMAvailability(freshness, (uint16_t)index);
            out->availabilityKnown = available >= 0;
            out->available = available == 1;
        }
        return true;
    }
    if (game == (uint8_t)GAME_OOT && sOoTOps != NULL) {
        if (!sOoTOps->checkAt(index, out)) {
            return false;
        }
        if (Context_GetCurrentGame() != GAME_OOT) {
            // The snapshot (#1129): the departure capture, whatever the adapter
            // (which answers only while OoT is played) left in the row.
            const int available = Combo_TrackerFrozenAvailability((uint8_t)GAME_OOT, out->checkId);
            out->availabilityKnown = available >= 0;
            out->available = available == 1;
        }
        return true;
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

const char* Combo_TrackerCheckLogic(uint8_t game, uint16_t checkId) {
    if (game == (uint8_t)GAME_MM) {
        return (sMMRegistered && sMMDesc.logicText != NULL) ? sMMDesc.logicText(checkId) : NULL;
    }
    if (game == (uint8_t)GAME_OOT && sOoTOps != NULL && sOoTOps->logicText != NULL) {
        return sOoTOps->logicText(checkId);
    }
    return NULL;
}

// The current area kept per game (#1131): each game's last live answer.
static bool sAreaKnown[2];
static uint16_t sAreaKey[2];

bool Combo_TrackerCurrentArea(uint8_t game, uint16_t* outKey) {
    if (game != (uint8_t)GAME_OOT && game != (uint8_t)GAME_MM) {
        return false;
    }
    const int slot = game == (uint8_t)GAME_OOT ? 0 : 1;
    if (Context_GetCurrentGame() == (GameId)game) {
        // Only the active game's adapter is asked: the other's state belongs to
        // the game being played (MM's play state is gone, OoT's gSaveContext is
        // MM's), so its pane keeps the answer of its last frame of play.
        bool (*op)(uint16_t*) = NULL;
        if (game == (uint8_t)GAME_MM) {
            op = sMMRegistered ? sMMDesc.currentArea : NULL;
        } else {
            op = sOoTOps != NULL ? sOoTOps->currentArea : NULL;
        }
        uint16_t key = 0;
        sAreaKnown[slot] = op != NULL && op(&key);
        sAreaKey[slot] = sAreaKnown[slot] ? key : 0;
    }
    if (!sAreaKnown[slot]) {
        return false;
    }
    if (outKey != NULL) {
        *outKey = sAreaKey[slot];
    }
    return true;
}

/** `name`, or NULL for NULL or "". */
static const char* NonEmpty(const char* name) {
    return (name != NULL && name[0] != '\0') ? name : NULL;
}

void Combo_TrackerNameSeenItem(const ComboTrackerSeenItem* seen, ComboTrackerCheckRow* row) {
    if (seen == NULL || row == NULL) {
        return;
    }
    // SoH's DrawLocation, RCSHOW_SEEN / RCSHOW_IDENTIFIED (randomizer_check_tracker.cpp).
    const bool hideMerchant =
        seen->saleKind == (uint8_t)COMBO_TRACKER_SALE_MERCHANT && (!seen->merchantTextHint || seen->mystery);
    const bool hideScrub =
        seen->saleKind == (uint8_t)COMBO_TRACKER_SALE_SCRUB && (!seen->scrubTextHint || seen->mystery);
    const bool hideShop = seen->saleKind == (uint8_t)COMBO_TRACKER_SALE_SHOP && seen->mystery && seen->mysteryShopItem;
    const bool reveal = !(hideMerchant || hideScrub || hideShop);

    const char* name = NULL;
    uint8_t game = (uint8_t)GAME_NONE;
    if (reveal && seen->iceTrap) {
        // The disguise once seen, the trick name once identified.
        name = NonEmpty(seen->identified ? seen->trickName : seen->looksLikeName);
        game = seen->game;
    } else if (reveal) {
        name = NonEmpty(seen->itemName);
        game = seen->itemGame;
    }
    row->placedItemName = name;
    row->placedItemGame = (name != NULL) ? game : (uint8_t)GAME_NONE;
    // The price line: an identified check SoH lists and can sell (SoH 2ba368e7e),
    // whether or not its item is named.
    row->hasPrice = seen->identified && seen->canBePurchased && seen->listed;
    row->price = row->hasPrice ? seen->price : 0;
}

// ============================================================================
// The skip toggle (#458 U5)
// ============================================================================

/**
 * Whether Majora's Mask plays a randomizer file now: MM is the active game and
 * MMBlobIfPresent's pick reads the live save LIVE (play state loaded, marked)
 * and it holds a randomized world. Only reads.
 */
static bool MMPlaysRandoFile(void) {
    if (!sMMRegistered || sMMDesc.liveSave == NULL || Context_GetCurrentGame() != GAME_MM) {
        return false;
    }
    const uint8_t* live = (const uint8_t*)sMMDesc.liveSave();
    return MMBlobMarked(live) && MMBlobReadU32(live, sMMDesc.saveTypeOffset) == sMMDesc.saveTypeRando;
}

/**
 * Whether Ocarina of Time plays a randomizer file now: OoT is the active game and
 * its adapter's live skip gate answers (a loaded randomizer save; the OoT TU
 * reads gSaveContext for it, which is OoT's then). A vtable with no skip toggle
 * cannot say, so it reads as no.
 */
static bool OoTPlaysRandoFile(void) {
    return sOoTOps != NULL && sOoTOps->skipWritable != NULL && Context_GetCurrentGame() == GAME_OOT &&
           sOoTOps->skipWritable();
}

/**
 * Whether MM's LIVE pane takes a skip write now: its adapter has the op and MM
 * plays a randomizer file. This only reads; the write is the MM TU's own op
 * (setLiveSkipped), which checks the same conditions in MM's layout.
 */
static bool MMLiveSkipWritable(void) {
    return sMMRegistered && sMMDesc.setLiveSkipped != NULL && MMPlaysRandoFile();
}

/**
 * Whether MM's FAR pane takes a skip write now (#1294): Ocarina of Time plays a
 * randomizer file, the pairing is live, and the MM half is armed (frozen, not
 * yet consumed by an arrival), marked and a randomized world: the shadow the
 * STALE pane reads is then the paired file's MM half, the one the next commit
 * stores and MM's arrival resumes. Only reads; the write is the MM TU's own op
 * (setShadowSkipped), which checks the same conditions in MM's layout.
 */
static bool MMFarSkipWritable(void) {
    if (!sMMRegistered || sMMDesc.setShadowSkipped == NULL || !Combo_ForeignPairingActive() ||
        !Context_HasFrozenState(GAME_MM) || !OoTPlaysRandoFile()) {
        return false;
    }
    const uint8_t* shadow = MMShadowIfPresent();
    return shadow != NULL && MMBlobReadU32(shadow, sMMDesc.saveTypeOffset) == sMMDesc.saveTypeRando;
}

/** OoT's LIVE pane takes skip writes only through a vtable that has them, while OoT plays a randomizer file. */
static bool OoTLiveSkipWritable(void) {
    return OoTPlaysRandoFile();
}

/**
 * OoT's FAR pane (#1294): Majora's Mask plays a randomizer file and OoT's adapter
 * says its suspended heap is the paired world OoT departed from (the pairing
 * live, the departure capture's seed).
 */
static bool OoTFarSkipWritable(void) {
    return sOoTOps != NULL && sOoTOps->suspendedSkipWritable != NULL && MMPlaysRandoFile() &&
           sOoTOps->suspendedSkipWritable();
}

bool Combo_TrackerSkipWritable(uint8_t game) {
    if (game == (uint8_t)GAME_MM) {
        return MMLiveSkipWritable() || MMFarSkipWritable();
    }
    if (game == (uint8_t)GAME_OOT) {
        return OoTLiveSkipWritable() || OoTFarSkipWritable();
    }
    return false;
}

bool Combo_TrackerRowSkippable(uint8_t game, const ComboTrackerCheckRow* row) {
    return row != NULL && row->shuffled && !row->obtained && Combo_TrackerSkipWritable(game);
}

bool Combo_TrackerSetSkipped(uint8_t game, uint16_t checkId, bool skipped) {
    if (game == (uint8_t)GAME_MM) {
        // MM's TU writes the byte MM's own tracker flips (ADR 0008 rule 5: common
        // code only reads MM's save). LIVE: MM's next save persists it and the
        // departure freeze carries it into the shadow. FAR: the frozen half the
        // next commit stores and the arrival resumes.
        if ((uint32_t)checkId >= sMMDesc.checkCount) {
            return false;
        }
        if (MMLiveSkipWritable()) {
            return sMMDesc.setLiveSkipped(checkId, skipped);
        }
        return MMFarSkipWritable() && sMMDesc.setShadowSkipped(checkId, skipped);
    }
    if (game == (uint8_t)GAME_OOT) {
        if (OoTLiveSkipWritable()) {
            return sOoTOps->setSkipped(checkId, skipped);
        }
        return OoTFarSkipWritable() && sOoTOps->setSuspendedSkipped(checkId, skipped);
    }
    return false;
}

// ============================================================================
// The OoT skip journal (#1294)
// ============================================================================

#define CTV_JOURNAL_SKIPPED_BIT 0x8000u
#define CTV_JOURNAL_ID_MASK 0x7FFFu

/** The journal's count, clamped to the capacity (a corrupt record reads as full, never past it). */
static int JournalCount(void) {
    const uint16_t count = gComboCtx.trackerSkipJournalCountOoT;
    return count > RSBS_TRACKER_SKIP_JOURNAL_OOT_CAP ? (int)RSBS_TRACKER_SKIP_JOURNAL_OOT_CAP : (int)count;
}

bool Combo_TrackerOoTSkipJournalPut(uint16_t checkId, bool skipped, bool onlyIfPresent) {
    if (checkId == 0 || checkId > CTV_JOURNAL_ID_MASK) {
        return false;
    }
    const uint16_t entry = (uint16_t)(checkId | (skipped ? CTV_JOURNAL_SKIPPED_BIT : 0u));
    const int count = JournalCount();
    for (int i = 0; i < count; i++) {
        if ((gComboCtx.trackerSkipJournalOoT[i] & CTV_JOURNAL_ID_MASK) == checkId) {
            gComboCtx.trackerSkipJournalOoT[i] = entry;
            return true;
        }
    }
    if (onlyIfPresent || count >= (int)RSBS_TRACKER_SKIP_JOURNAL_OOT_CAP) {
        return false;
    }
    gComboCtx.trackerSkipJournalOoT[count] = entry;
    gComboCtx.trackerSkipJournalCountOoT = (uint16_t)(count + 1);
    return true;
}

int Combo_TrackerOoTSkipJournalCount(void) {
    return JournalCount();
}

bool Combo_TrackerOoTSkipJournalAt(int index, uint16_t* outCheckId, bool* outSkipped) {
    if (index < 0 || index >= JournalCount()) {
        return false;
    }
    const uint16_t entry = gComboCtx.trackerSkipJournalOoT[index];
    if (outCheckId != NULL) {
        *outCheckId = (uint16_t)(entry & CTV_JOURNAL_ID_MASK);
    }
    if (outSkipped != NULL) {
        *outSkipped = (entry & CTV_JOURNAL_SKIPPED_BIT) != 0;
    }
    return true;
}

void Combo_TrackerOoTSkipJournalClear(void) {
    memset(gComboCtx.trackerSkipJournalOoT, 0, sizeof(gComboCtx.trackerSkipJournalOoT));
    gComboCtx.trackerSkipJournalCountOoT = 0;
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
    // #1129: the host check's row's reachability, as its own panel shows it.
    ComboTrackerCheckRow hostRow;
    const bool hostRead = Combo_TrackerCheckAt(hostGame, (int)host, &hostRow) && hostRow.checkId == host;
    out->availabilityKnown = hostRead && hostRow.availabilityKnown;
    out->available = out->availabilityKnown && hostRow.available;
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
