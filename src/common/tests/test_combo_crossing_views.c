/**
 * @file test_combo_crossing_views.c
 * @brief The Combo Tracker's and the Cross-Game Spoiler's crossing rows read
 *        the crossing store, in both directions, with names and per-check found
 *        state from each game's own save (#755, #757).
 *
 * ============================================================================
 * WHAT WAS WRONG
 * ============================================================================
 *
 * Under the single bag (ADR 0010 increment 3, PR #743) the crossing store is the
 * only record of which host holds which foreign item. Both panes read the two
 * retired pinned tables instead, so on every single-bag world the spoiler said
 * "no cross-game items" and the tracker's crossing lists were empty (#755). The
 * spoiler also showed one direction only, keyed by a raw hex MM check id (#757),
 * and its "collected" column scanned the shared-item array by (origin, id): an
 * entry there has no host and is recycled once redeemed, so two copies of one
 * item on two hosts could never be told apart.
 *
 * ============================================================================
 * TWO ROWS
 * ============================================================================
 *
 *   combo-crossing-views (redship tier, ROM-free). A SYNTHETIC world through
 *   the production writers: the store through Combo_Crossings_Replace, MM's save
 *   as an authored shadow at the REGISTERED tracker offsets, OoT's through the
 *   OoT adapter's authoring seam. It round-trips the store through the panes'
 *   view model:
 *     1. unpaired: no rows in either direction, crossings resident or not;
 *     2. both directions, store order, item names and articles from the ORIGIN
 *        game's describer, host check names from the HOST game's adapter (MM's
 *        readable, never RC_* and never hex), the spoiler's rows equal to the
 *        tracker's field for field;
 *     3. found per HOST CHECK from that game's save, with two copies of one item
 *        on two MM hosts, one collected: one row YES, the other NO (the old
 *        (origin, id) scan could only answer the same for both), and the
 *        per-direction totals the notes print;
 *     4. a game switch, SIMULATED at the context layer (no Game_Suspend or
 *        Game_Resume runs): OoT's found state is kept and relabelled stale while
 *        MM is current, and an MM departure's freeze (Context_FreezeState of an
 *        authored blob) is what moves MM's found state;
 *     5. a .redsave load with MM never booted: save, wipe every source (the
 *        combo context, the store, both shadows AND OoT's heap world), load.
 *        The rows, their names and MM's found state come back from the
 *        .redsave. OoT's found state does NOT: OoT's check status is not in the
 *        .redsave but in OoT's own .sav ("trackerData", loaded by OoT's
 *        LoadFile before the .redsave hook runs), so right after the .redsave
 *        load every OoT-hosted row reads UNKNOWN, never a stale YES or NO. The
 *        OoT world is then re-authored as the stand-in for OoT's own .sav load
 *        (this ROM-free row cannot drive SoH's SaveManager), and the rows read
 *        what they read before the save.
 *   The MM stale label on a shadow MM has never loaded (the creation event's
 *   armed half: MM's marker, no creation stamp yet, #765) is "As of file
 *   creation", not "the last game switch or save".
 *
 *   combo-crossing-views-world (rando tier). A REAL single-bag world, the
 *   ComboSingleBag pinned seed (RSBSSINGLEBAG1), made by the production creation
 *   event (OoT_Creation_AuthorRandoFile: MM's half, the crossings stored, OoT's
 *   remainder, the MM world armed in the shadow). The one spoiler's combo
 *   section is written by its production builder (MM_Rando_WriteCrossingSpoilerSection
 *   shares CrossingStoreSpoilerSection with MM_Rando_AugmentSpoilerWithPairedHalf)
 *   and parsed back: the tracker and the spoiler list exactly its rows, in its
 *   order, both directions, with its item names, a host check name for every
 *   row, nothing found on the fresh world, and one host per direction marked
 *   collected in its own game's save turns exactly that row found.
 *
 *   The same world row also locks the two NATIVE check trackers (#796). A
 *   crossing host physically holds a cover (OoT: the Blue Rupee the single bag
 *   writes; MM: RI_JUNK), so a tracker that prints its own table's item names the
 *   cover. Every OoT-hosted crossing, marked collected in OoT's own location
 *   table, must print "<item> (MM)"; every MM-hosted crossing of the armed MM
 *   world must print "<item> (OoT)" while MM's table stores the cover there. With
 *   the pairing gone OoT's give degrades to the cover, and so does its row.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++).
 */

#include "../combo_single_bag.h"
#include "../combo_spoiler_view.h"
#include "../combo_tracker_view.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../save.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
int OoT_TrackerAdapter_TestAuthorWorld(uint32_t seed, uint16_t outIds[3]);
void OoT_TrackerAdapter_TestReleaseWorld(void);
int OoT_TrackerAdapter_TestSetCollected(uint16_t rc, int collected);
int MM_Rando_WriteCrossingSpoilerSection(const char* path);
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_Creation_AuthorRandoFile(int slot);
void Randomizer_TestClearOoTSave(void);
// #796: the native check trackers' found-item names, and the collected write
// the RC-queue drain makes on a crossing host.
int OoT_CheckTracker_TestItemName(uint16_t rc, char* out, int cap);
int OoT_Rando_Foreign_TestSetObtained(uint16_t rc, int obtained);
int MM_CheckTracker_TestItemName(const void* mmSave, uint16_t randoCheckId, char* out, int cap,
                                 uint16_t* outStoredItem);
void MM_Rando_Foreign_TestItemSentinels(uint16_t* outJunk, uint16_t* outNone, uint16_t* outUnknown);
}

namespace {

#define CXV_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL (combo-crossing-views): %s  [%s:%d]\n", #cond, __FILE__, __LINE__);                    \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

const char* const kCxvSaveDir = "rsbs_test_saves_crossing_views";

ComboCrossing CxvCrossing(uint16_t host, SharedItem item) {
    ComboCrossing c;
    c.hostCheck = host;
    c.itemClass = 0x0001;
    c.item = item;
    return c;
}

/** Everything a pane prints for one row, copied (names by value). */
struct CxvRow {
    uint16_t host = 0;
    uint8_t origin = 0;
    uint16_t item = 0;
    std::string hostName; // "" when NULL
    bool hostNamed = false;
    std::string itemName;
    std::string article;
    uint8_t found = 0;
    bool operator==(const CxvRow& o) const {
        return host == o.host && origin == o.origin && item == o.item && hostName == o.hostName &&
               hostNamed == o.hostNamed && itemName == o.itemName && article == o.article && found == o.found;
    }
};

CxvRow CxvCopy(const ComboTrackerForeignRow& r) {
    CxvRow c;
    c.host = r.hostCheckId;
    c.origin = r.originGame;
    c.item = r.itemId;
    c.hostNamed = r.hostCheckName != nullptr;
    c.hostName = r.hostCheckName != nullptr ? r.hostCheckName : "";
    c.itemName = r.itemName != nullptr ? r.itemName : "(null)";
    c.article = r.itemArticle != nullptr ? r.itemArticle : "(null)";
    c.found = r.found;
    return c;
}

/**
 * Both panes' rows for `host`, checked to agree: the tracker's rows, the
 * spoiler's rows, field for field, and the count each reports. `*ok` is false
 * on any disagreement.
 */
std::vector<CxvRow> CxvPaneRows(uint8_t host, bool* ok) {
    std::vector<CxvRow> rows;
    *ok = false;
    const int n = Combo_TrackerForeignCount(host);
    if (n != Combo_SpoilerRowCount(host)) {
        printf("[TEST] combo-crossing-views: tracker lists %d crossings in game %u, spoiler %d\n", n, (unsigned)host,
               Combo_SpoilerRowCount(host));
        return rows;
    }
    for (int i = 0; i < n; i++) {
        ComboTrackerForeignRow t;
        ComboSpoilerRow s;
        if (!Combo_TrackerForeignRowAt(host, i, &t) || !Combo_SpoilerRowAt(host, i, &s)) {
            printf("[TEST] combo-crossing-views: row %d of game %u is missing\n", i, (unsigned)host);
            return rows;
        }
        if (t.hostGame != host || s.hostGame != host || t.itemName == nullptr || t.itemArticle == nullptr ||
            !(CxvCopy(t) == CxvCopy(s))) {
            printf("[TEST] combo-crossing-views: row %d of game %u differs between the panes (or is malformed)\n", i,
                   (unsigned)host);
            return rows;
        }
        rows.push_back(CxvCopy(t));
    }
    *ok = true;
    return rows;
}

void CxvSetMMObtained(std::vector<uint8_t>& blob, const ComboMMTrackerDesc* desc, uint16_t check, bool obtained) {
    uint8_t* row = blob.data() + desc->checkTableOffset + (size_t)check * desc->checkStride;
    row[desc->shuffledOffset] = 1;
    row[desc->obtainedOffset] = obtained ? 1 : 0;
}

} // namespace

// ============================================================================
// combo-crossing-views (redship tier)
// ============================================================================
TestResult ComboCrossingViews_RunSynthetic(void) {
    printf("[TEST] combo-crossing-views: both panes read the crossing store in both directions, named, with found "
           "state from each game's save, across a game switch and a .redsave load (#755, #757)\n");

    const GameId prevGame = Context_GetCurrentGame();
    Context_InitFrozenStates();
    std::vector<uint8_t> ootBackup((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mmBackup((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    if (const void* p = Context_GetOoTSaveContext()) {
        memcpy(ootBackup.data(), p, ootBackup.size());
    }
    if (const void* p = Context_GetMMSaveContext()) {
        memcpy(mmBackup.data(), p, mmBackup.size());
    }
    ComboContext_Init();
    Combo_Crossings_Clear();
    Context_ClearAllFrozenStates();

    // The production registrations (Combo_TrackerWindow_Init makes both).
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    CXV_ASSERT(desc != nullptr);

    // OoT's world: [collected, untouched, skipped] OoT checks, on the heap.
    uint16_t ootIds[3] = { 0, 0, 0 };
    CXV_ASSERT(OoT_TrackerAdapter_TestAuthorWorld(0x0A11CE01u, ootIds) == 3);

    // MM's world: a rando save in the shadow, three shuffled host checks, the
    // first collected.
    const uint32_t kSeed = 0xC0FFEE55u;
    const uint16_t mmA = 0x0401, mmB = 0x0402, mmC = 0x0311;
    std::vector<uint8_t> blob((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(blob.data() + desc->newfOffset, desc->newf, desc->newfLen);
    memcpy(blob.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
    memcpy(blob.data() + desc->finalSeedOffset, &kSeed, sizeof(uint32_t));
    // A save MM has loaded: its OnSaveLoad stamped the creation time (#765).
    const uint64_t kCreatedAt = 1760000000ull;
    memcpy(blob.data() + desc->createdAtOffset, &kCreatedAt, sizeof(kCreatedAt));
    CxvSetMMObtained(blob, desc, mmA, true);
    CxvSetMMObtained(blob, desc, mmB, false);
    CxvSetMMObtained(blob, desc, mmC, false);
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());

    SharedItem lens, hammer, mmLens, mmHookshot;
    CXV_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &lens));
    CXV_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer));
    CXV_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmLens));
    CXV_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Hookshot", &mmHookshot));

    // Two copies of the OoT Lens on two MM hosts (mmA collected, mmC not): the
    // single bag's multiplicity, which only a per-host found state tells apart.
    const ComboCrossing ootHosted[2] = { CxvCrossing(ootIds[0], mmLens), CxvCrossing(ootIds[1], mmHookshot) };
    const ComboCrossing mmHosted[3] = { CxvCrossing(mmA, lens), CxvCrossing(mmB, hammer), CxvCrossing(mmC, lens) };
    CXV_ASSERT(Combo_Crossings_Replace(ootHosted, 2, mmHosted, 3) == 5);

    // ---- 1. unpaired: nothing listed, though the store holds five rows ----
    CXV_ASSERT(!Combo_ForeignPairingActive());
    CXV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0 && Combo_TrackerForeignCount((uint8_t)GAME_OOT) == 0);
    CXV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) == 0 && Combo_SpoilerRowCount((uint8_t)GAME_OOT) == 0);

    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kSeed;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0755u;
    CXV_ASSERT(Combo_ForeignPairingActive());
    Context_SetCurrentGame(GAME_OOT);

    // ---- 2. both directions, store order, names and articles --------------
    bool ok = false;
    const std::vector<CxvRow> inMM = CxvPaneRows((uint8_t)GAME_MM, &ok);
    CXV_ASSERT(ok);
    const std::vector<CxvRow> inOoT = CxvPaneRows((uint8_t)GAME_OOT, &ok);
    CXV_ASSERT(ok);
    CXV_ASSERT(inMM.size() == 3 && inOoT.size() == 2);
    for (size_t i = 0; i < inMM.size(); i++) {
        CXV_ASSERT(inMM[i].host == mmHosted[i].hostCheck && inMM[i].origin == (uint8_t)GAME_OOT &&
                   inMM[i].item == mmHosted[i].item.id);
        CXV_ASSERT(inMM[i].itemName == Combo_GetForeignItemName(mmHosted[i].item));
        const char* article = Combo_GetForeignItemArticle(mmHosted[i].item);
        CXV_ASSERT(inMM[i].article == (article != nullptr ? article : ""));
        // #757: the host check's readable MM name, the one the MM panel prints.
        CXV_ASSERT(inMM[i].hostNamed && !inMM[i].hostName.empty());
        CXV_ASSERT(inMM[i].hostName.rfind("RC_", 0) != 0 && inMM[i].hostName.rfind("0x", 0) != 0);
        CXV_ASSERT(inMM[i].hostName == Combo_TrackerCheckName((uint8_t)GAME_MM, mmHosted[i].hostCheck));
    }
    CXV_ASSERT(inMM[0].itemName == "Lens of Truth" && inMM[1].itemName == "Megaton Hammer");
    for (size_t i = 0; i < inOoT.size(); i++) {
        CXV_ASSERT(inOoT[i].host == ootHosted[i].hostCheck && inOoT[i].origin == (uint8_t)GAME_MM &&
                   inOoT[i].item == ootHosted[i].item.id);
        CXV_ASSERT(inOoT[i].itemName == Combo_GetForeignItemName(ootHosted[i].item));
        // OoT's location names are filled at OTR bring-up; in this ROM-free tier
        // they may be absent, and the row then says so (NULL) exactly as the OoT
        // panel's own name does. The world row names them for real.
        const char* ootName = Combo_TrackerCheckName((uint8_t)GAME_OOT, ootHosted[i].hostCheck);
        CXV_ASSERT(inOoT[i].hostNamed == (ootName != nullptr));
        CXV_ASSERT(!inOoT[i].hostNamed || inOoT[i].hostName == ootName);
    }
    CXV_ASSERT(inOoT[0].itemName == "Lens of Truth" && inOoT[1].itemName == "Hookshot");

    // ---- 3. found, per host check, from each game's save -----------------
    CXV_ASSERT(inMM[0].found == COMBO_TRACKER_FOUND_YES); // mmA obtained
    CXV_ASSERT(inMM[1].found == COMBO_TRACKER_FOUND_NO);
    CXV_ASSERT(inMM[2].found == COMBO_TRACKER_FOUND_NO);  // the SAME item as row 0; its own host is not collected
    CXV_ASSERT(inOoT[0].found == COMBO_TRACKER_FOUND_YES); // ootIds[0] is the collected OoT check
    CXV_ASSERT(inOoT[1].found == COMBO_TRACKER_FOUND_NO);
    ComboTrackerForeignProgress mmProgress, ootProgress;
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &mmProgress);
    Combo_TrackerForeignProgress((uint8_t)GAME_OOT, &ootProgress);
    CXV_ASSERT(mmProgress.total == 3 && mmProgress.found == 1 && mmProgress.freshness == COMBO_TRACKER_FRESH_STALE);
    CXV_ASSERT(ootProgress.total == 2 && ootProgress.found == 1 && ootProgress.freshness == COMBO_TRACKER_FRESH_LIVE);
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);
    CXV_ASSERT(identity.mmHostedForeign == 3 && identity.ootHostedForeign == 2);
    ComboSpoilerSummary summary;
    Combo_SpoilerPairingSummary(&summary);
    CXV_ASSERT(summary.paired && summary.mmHosted == 3 && summary.ootHosted == 2);
    // The creation event's shape: MM's half armed with MM's file-select marker
    // but never loaded by MM, so no creation stamp yet (#765; the world row runs
    // the real creation). MM's found state is read.
    {
        std::vector<uint8_t> unloaded = blob;
        memset(unloaded.data() + desc->createdAtOffset, 0, sizeof(uint64_t));
        Context_UpdateShadowCopy(GAME_MM, unloaded.data(), unloaded.size());
        const std::vector<CxvRow> unloadedRows = CxvPaneRows((uint8_t)GAME_MM, &ok);
        CXV_ASSERT(ok && unloadedRows == inMM);
        Combo_TrackerForeignProgress((uint8_t)GAME_MM, &mmProgress);
        CXV_ASSERT(mmProgress.found == 1 && mmProgress.freshness == COMBO_TRACKER_FRESH_STALE);
        // MM has never loaded this half: its data is as of file creation, and
        // the note must not name a switch or save that never happened.
        CXV_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE),
                          "As of file creation") == 0);
        // ...but an all-zero shadow (MM never entered) is still no data at all.
        std::vector<uint8_t> zero((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        Context_UpdateShadowCopy(GAME_MM, zero.data(), zero.size());
        Combo_TrackerForeignProgress((uint8_t)GAME_MM, &mmProgress);
        CXV_ASSERT(mmProgress.found == 0 && mmProgress.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
        ComboTrackerForeignRow unknownRow;
        CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &unknownRow) &&
                   unknownRow.found == COMBO_TRACKER_FOUND_UNKNOWN);
        Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());
        CXV_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE),
                          "As of the last game switch or save") == 0);
    }

    // ---- 4. a game switch --------------------------------------------------
    // Into MM: OoT's heap is suspended, not gone; its found state stays and is
    // labelled stale.
    Context_SetCurrentGame(GAME_MM);
    Combo_TrackerForeignProgress((uint8_t)GAME_OOT, &ootProgress);
    CXV_ASSERT(ootProgress.total == 2 && ootProgress.found == 1 && ootProgress.freshness == COMBO_TRACKER_FRESH_STALE);
    // Collecting mmB and leaving MM: the departure's freeze is the MM write the
    // panes read.
    CxvSetMMObtained(blob, desc, mmB, true);
    Context_FreezeState(GAME_MM, 0, blob.data(), blob.size());
    Context_SetCurrentGame(GAME_OOT);
    const std::vector<CxvRow> inMMAfter = CxvPaneRows((uint8_t)GAME_MM, &ok);
    CXV_ASSERT(ok && inMMAfter.size() == 3);
    CXV_ASSERT(inMMAfter[0].found == COMBO_TRACKER_FOUND_YES && inMMAfter[1].found == COMBO_TRACKER_FOUND_YES &&
               inMMAfter[2].found == COMBO_TRACKER_FOUND_NO);
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &mmProgress);
    CXV_ASSERT(mmProgress.found == 2);

    // ---- 5. a .redsave load with MM never booted ---------------------------
    const std::vector<CxvRow> savedMM = CxvPaneRows((uint8_t)GAME_MM, &ok);
    CXV_ASSERT(ok);
    const std::vector<CxvRow> savedOoT = CxvPaneRows((uint8_t)GAME_OOT, &ok);
    CXV_ASSERT(ok);
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kCxvSaveDir);
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0); // an erase is what unlatches the slot for this session's write
    CXV_ASSERT(mgr.Save(0));

    ComboContext_Init();
    Combo_Crossings_Clear();
    Context_ClearAllFrozenStates(); // zeroes MM's shadow: MM has nothing to read
    OoT_TrackerAdapter_TestReleaseWorld(); // OoT's heap world goes too: nothing survives but the files
    CXV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0);
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &mmProgress);
    CXV_ASSERT(mmProgress.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);

    Context_InvalidateSessionOnSlotLoad();
    CXV_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK);
    const std::vector<CxvRow> loadedMM = CxvPaneRows((uint8_t)GAME_MM, &ok);
    CXV_ASSERT(ok);
    std::vector<CxvRow> loadedOoT = CxvPaneRows((uint8_t)GAME_OOT, &ok);
    CXV_ASSERT(ok);
    CXV_ASSERT(loadedMM == savedMM);
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &mmProgress);
    CXV_ASSERT(mmProgress.total == 3 && mmProgress.found == 2 && mmProgress.freshness == COMBO_TRACKER_FRESH_STALE);
    // OoT's rows come back from the .redsave's crossing block, named; their
    // found state is not the .redsave's to give, so it reads UNKNOWN until
    // OoT's own save is loaded.
    CXV_ASSERT(loadedOoT.size() == savedOoT.size());
    for (size_t i = 0; i < loadedOoT.size(); i++) {
        CxvRow expect = savedOoT[i];
        expect.found = COMBO_TRACKER_FOUND_UNKNOWN;
        CXV_ASSERT(loadedOoT[i] == expect);
    }
    Combo_TrackerForeignProgress((uint8_t)GAME_OOT, &ootProgress);
    CXV_ASSERT(ootProgress.total == 2 && ootProgress.found == 0 &&
               ootProgress.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    // OoT's own .sav load, stood in for by re-authoring the same world: the
    // rows read exactly what they read before the save.
    CXV_ASSERT(OoT_TrackerAdapter_TestAuthorWorld(0x0A11CE01u, ootIds) == 3);
    loadedOoT = CxvPaneRows((uint8_t)GAME_OOT, &ok);
    CXV_ASSERT(ok);
    CXV_ASSERT(loadedOoT == savedOoT);
    Combo_TrackerForeignProgress((uint8_t)GAME_OOT, &ootProgress);
    CXV_ASSERT(ootProgress.total == 2 && ootProgress.found == 1);
    printf("[TEST] combo-crossing-views: after the load, %d crossings in MM (%d found), %d in OoT (%d found)\n",
           (int)loadedMM.size(), mmProgress.found, (int)loadedOoT.size(), ootProgress.found);

    // ---- leave no trace ----------------------------------------------------
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    OoT_TrackerAdapter_TestReleaseWorld();
    Combo_Crossings_Clear();
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    Context_UpdateShadowCopy(GAME_OOT, ootBackup.data(), ootBackup.size());
    Context_UpdateShadowCopy(GAME_MM, mmBackup.data(), mmBackup.size());
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: both panes list the crossing store's rows in both directions, named, found per host "
           "check from each game's save, across a game switch and a .redsave load with MM never booted\n");
    return TEST_PASS;
}

// ============================================================================
// combo-crossing-views-world (rando tier)
// ============================================================================
TestResult ComboCrossingViews_RunWorld(void) {
    printf("[TEST] combo-crossing-views-world: over a real single-bag world, both panes list exactly the one "
           "spoiler's combo.crossingStore rows, named (#755, #757)\n");

    CXV_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAG1") == 0);
    Randomizer_TestClearOoTSave();
    Context_ClearFrozenState(GAME_MM);
    CXV_ASSERT(OoT_Creation_AuthorRandoFile(0) == 1);
    CXV_ASSERT(Combo_ForeignPairingActive());
    CXV_ASSERT(Context_HasFrozenState(GAME_MM) != 0); // MM's world is in the armed shadow; MM never booted
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    Context_SetCurrentGame(GAME_OOT);

    const int storeMM = Combo_Crossings_Count(GAME_MM);
    const int storeOoT = Combo_Crossings_Count(GAME_OOT);
    printf("[TEST] combo-crossing-views-world: the store holds %d OoT items in MM checks, %d MM items in OoT checks; "
           "pinned tables %d / %d\n",
           storeMM, storeOoT, Combo_CountForeignPlacements(), Combo_CountForeignPlacementsOoT());
    CXV_ASSERT(storeMM > 0 && storeOoT > 0);
    CXV_ASSERT(Combo_CountForeignPlacements() == 0 && Combo_CountForeignPlacementsOoT() == 0);

    // The one spoiler's combo section, by its production builder.
    std::error_code ec;
    std::filesystem::create_directories("rsbs_test_crossing_views_world", ec);
    const std::string path = "rsbs_test_crossing_views_world/spoiler.json";
    std::filesystem::remove(path, ec);
    CXV_ASSERT(MM_Rando_WriteCrossingSpoilerSection(path.c_str()) == 0);
    nlohmann::json doc;
    {
        std::ifstream in(path);
        CXV_ASSERT(in.is_open());
        in >> doc;
    }
    CXV_ASSERT(doc.contains("combo") && doc["combo"].contains("crossingStore"));
    const nlohmann::json& section = doc["combo"]["crossingStore"];

    const struct {
        const char* key;
        uint8_t host;
        const char* hostKey;
        const char* originKey;
    } kLists[2] = { { "ootItemsInMM", (uint8_t)GAME_MM, "mm", "oot" },
                    { "mmItemsInOoT", (uint8_t)GAME_OOT, "oot", "mm" } };
    for (const auto& list : kLists) {
        CXV_ASSERT(section.contains(list.key) && section[list.key].is_array());
        const nlohmann::json& rows = section[list.key];
        bool ok = false;
        const std::vector<CxvRow> pane = CxvPaneRows(list.host, &ok);
        CXV_ASSERT(ok);
        printf("[TEST] combo-crossing-views-world: %s: spoiler section %d rows, panes %d rows\n", list.key,
               (int)rows.size(), (int)pane.size());
        CXV_ASSERT(pane.size() == rows.size());
        CXV_ASSERT((int)pane.size() == Combo_Crossings_Count((GameId)list.host));
        for (size_t i = 0; i < pane.size(); i++) {
            const nlohmann::json& r = rows[i];
            CXV_ASSERT(r["host"].get<std::string>() == list.hostKey &&
                       r["origin"].get<std::string>() == list.originKey);
            CXV_ASSERT(pane[i].host == r["hostCheck"].get<uint32_t>());
            CXV_ASSERT(pane[i].item == r["itemId"].get<uint32_t>());
            CXV_ASSERT(pane[i].origin != list.host);
            CXV_ASSERT(r["itemName"].is_string() && pane[i].itemName == r["itemName"].get<std::string>());
            CXV_ASSERT(pane[i].itemName != RSBS_TRACKER_UNKNOWN_ITEM_NAME);
            // Every host check is named: MM's by its readable tracker name (the
            // section prints MM's describer spelling, RC_*), OoT's by the same
            // location name the section prints.
            CXV_ASSERT(pane[i].hostNamed && !pane[i].hostName.empty());
            CXV_ASSERT(pane[i].hostName.rfind("RC_", 0) != 0);
            if (list.host == (uint8_t)GAME_OOT) {
                CXV_ASSERT(r["hostCheckName"].is_string() &&
                           pane[i].hostName == r["hostCheckName"].get<std::string>());
            }
            // A fresh world: its own game's save has collected none of the hosts.
            // For MM that save is the shadow the creation event armed, with
            // MM's file-select marker since #765: NO, not UNKNOWN.
            CXV_ASSERT(pane[i].found == COMBO_TRACKER_FOUND_NO);
            if (i < 3) {
                printf("[TEST] combo-crossing-views-world:   %s -> %s%s\n", pane[i].hostName.c_str(),
                       pane[i].article.c_str(), pane[i].itemName.c_str());
            }
        }
        ComboTrackerForeignProgress progress;
        Combo_TrackerForeignProgress(list.host, &progress);
        CXV_ASSERT(progress.total == (int)rows.size() && progress.found == 0);
        CXV_ASSERT(progress.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE);
    }

    // One host per direction collected in its own game's save: exactly that
    // row turns found, and the count follows.
    {
        ComboTrackerForeignRow row;
        CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &row));
        const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
        CXV_ASSERT(desc != nullptr);
        std::vector<uint8_t> shadow((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        memcpy(shadow.data(), Context_GetMMSaveContext(), shadow.size());
        const std::vector<uint8_t> original = shadow;
        shadow[desc->checkTableOffset + (size_t)row.hostCheckId * desc->checkStride + desc->obtainedOffset] = 1;
        Context_UpdateShadowCopy(GAME_MM, shadow.data(), shadow.size());
        ComboTrackerForeignProgress progress;
        Combo_TrackerForeignProgress((uint8_t)GAME_MM, &progress);
        CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &row) && row.found == COMBO_TRACKER_FOUND_YES);
        CXV_ASSERT(progress.found == 1);
        Context_UpdateShadowCopy(GAME_MM, original.data(), original.size());
    }
    {
        ComboTrackerForeignRow row;
        CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 0, &row));
        CXV_ASSERT(OoT_TrackerAdapter_TestSetCollected(row.hostCheckId, 1) == 0);
        ComboTrackerForeignProgress progress;
        Combo_TrackerForeignProgress((uint8_t)GAME_OOT, &progress);
        CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 0, &row) && row.found == COMBO_TRACKER_FOUND_YES);
        CXV_ASSERT(progress.found == 1 && progress.freshness == COMBO_TRACKER_FRESH_LIVE);
        CXV_ASSERT(OoT_TrackerAdapter_TestSetCollected(row.hostCheckId, 0) == 1);
    }

    // #796: the native trackers name the crossed item, never the cover.
    {
        char name[128];
        const int ootRows = Combo_TrackerForeignCount((uint8_t)GAME_OOT);
        for (int i = 0; i < ootRows; i++) {
            ComboTrackerForeignRow row;
            CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, i, &row));
            // Found, the way the RC-queue drain marks a crossing host.
            CXV_ASSERT(OoT_Rando_Foreign_TestSetObtained(row.hostCheckId, 1) == 1);
            name[0] = '\0';
            const int wrote = OoT_CheckTracker_TestItemName(row.hostCheckId, name, (int)sizeof(name));
            OoT_Rando_Foreign_TestSetObtained(row.hostCheckId, 0);
            CXV_ASSERT(wrote == 1);
            const std::string expected = std::string(row.itemName) + " (MM)";
            if (i < 3 || expected != name) {
                printf("[TEST] combo-crossing-views-world: OoT check tracker row for %s reads (%s), expected (%s)\n",
                       row.hostCheckName != nullptr ? row.hostCheckName : "?", name, expected.c_str());
            }
            CXV_ASSERT(expected == name);
        }
        // Unpaired, OoT's pickup refuses the crossing and gives the cover the host
        // holds (OoT_Rando_Foreign_RecordPickup), so the row names the cover.
        {
            ComboTrackerForeignRow row;
            CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 0, &row));
            const std::string foreign = std::string(row.itemName) + " (MM)";
            gComboCtx.sourceIsRando = false;
            CXV_ASSERT(!Combo_ForeignPairingActive());
            const int wrote = OoT_CheckTracker_TestItemName(row.hostCheckId, name, (int)sizeof(name));
            gComboCtx.sourceIsRando = true;
            CXV_ASSERT(Combo_ForeignPairingActive());
            printf("[TEST] combo-crossing-views-world: unpaired, the same OoT row reads (%s)\n", name);
            CXV_ASSERT(wrote == 1 && name[0] != '\0' && foreign != name);
        }

        const void* mmSave = Context_GetMMSaveContext();
        CXV_ASSERT(mmSave != nullptr);
        uint16_t mmJunk = 0, mmNone = 0, mmUnknown = 0;
        MM_Rando_Foreign_TestItemSentinels(&mmJunk, &mmNone, &mmUnknown);
        const int mmRows = Combo_TrackerForeignCount((uint8_t)GAME_MM);
        for (int i = 0; i < mmRows; i++) {
            ComboTrackerForeignRow row;
            CXV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, i, &row));
            uint16_t stored = 0;
            name[0] = '\0';
            CXV_ASSERT(MM_CheckTracker_TestItemName(mmSave, row.hostCheckId, name, (int)sizeof(name), &stored) == 1);
            const std::string expected = std::string(row.itemName) + " (OoT)";
            if (i < 3 || expected != name) {
                printf("[TEST] combo-crossing-views-world: MM check tracker row for %s (MM table stores item %u) "
                       "reads (%s), expected (%s)\n",
                       row.hostCheckName != nullptr ? row.hostCheckName : "?", (unsigned)stored, name,
                       expected.c_str());
            }
            // The premise: MM's own table holds the cover at a crossing host.
            CXV_ASSERT(stored == mmJunk);
            CXV_ASSERT(expected == name);
        }
    }

    std::filesystem::remove(path, ec);
    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();
    printf("[TEST] PASS: combo-crossing-views-world\n");
    return TEST_PASS;
}
