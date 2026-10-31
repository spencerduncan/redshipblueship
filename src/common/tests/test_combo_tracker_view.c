/**
 * @file test_combo_tracker_view.c
 * @brief ROM-free lock for the combo tracker's per-game adapters (#458).
 *
 * What this proves, and how each claim would fail without the code under
 * test:
 *
 * 1. NULL-SAFETY OF THE UNREGISTERED/NEVER-BOOTED STATE. With both adapters
 *    un-registered every read answers UNAVAILABLE/0/false; nothing
 *    dereferences a missing descriptor or vtable. This is the state every
 *    window frame draws in until Combo_TrackerWindow_Init runs.
 *
 * 2. THE MM ADAPTER RECOVERS AUTHORED SHADOW BYTES. The lock authors a
 *    synthetic MM shadow blob at the offsets the REAL registered descriptor
 *    carries (offsetof values computed in the MM TU against z64save.h),
 *    commits it through the production Context_UpdateShadowCopy, and asserts
 *    the summary/rows recover exactly the authored world: seed, shuffled/
 *    obtained/skipped counts, per-row flags, and the never-LIVE freshness.
 *    The offset-correctness half of the tripwire is the MM TU's
 *    static_asserts — this half proves the registration ran and the reader
 *    walks the registered geometry, which is what goes RED if the descriptor
 *    registration is dropped from the link or the reader regresses.
 *
 * 3. AN ALL-ZERO SHADOW IS "NO DATA", NOT "A VANILLA SAVE". The 'newf'
 *    marker gate: before MM ever runs, the shadow is zeros, and the panel
 *    must say "no data" rather than "0 of 0 checks".
 *
 * 4. MM CHECK NAMES RESOLVE IN SINGLE-EXE. Combo_TrackerCheckName(GAME_MM, x)
 *    returns a real non-empty name — the #489-cause-2 class (CheckNames was
 *    RC_MAX empty strings in every single-exe binary) stays fixed on this
 *    surface.
 *
 * 5. THE OoT ADAPTER READS THE HEAP AND HONOURS THE SUSPEND CONTRACT. With
 *    no heap Rando::Context the panel is UNAVAILABLE; after the OoT-side
 *    seam authors a context (three placed checks: collected / untouched /
 *    skipped) the counts and per-row statuses recover, freshness is LIVE
 *    under GAME_OOT and STALE under GAME_MM — the headline claim of #458:
 *    OoT progress visible while MM runs. Releasing the world returns the
 *    adapter to UNAVAILABLE, proving liveness is re-checked per call.
 *
 * 6. FOREIGN ROWS RESOLVE BOTH DIRECTIONS WITH NAMES. One placement per
 *    table; rows carry the describer item name and article, the host-game
 *    check name (MM side), the found state read from the HOST game's save
 *    (#755: the MM host's obtained byte in the authored shadow, flipped both
 *    ways; the OoT host is UNKNOWN once its world is released), and vanish
 *    when the world is unpaired ("not paired" must never render as "no
 *    crossings"). The crossing-store half is ComboCrossingViews.
 *
 * 7. NO FRESHNESS CAPTION (#927). A panel is labelled by its game's header
 *    alone, so every label, for both games and every freshness, is empty.
 *
 * 8. MM READS LIVE WHILE MM IS PLAYED (#799). MM's arrival consumes the shadow
 *    and zeroes it, so a shadow-only reader showed "No data yet" in Termina
 *    until MM's first save. A test descriptor (the production one, liveSave
 *    pointed at an authored buffer) drives the source pick: LIVE totals, found
 *    state and note from a marked live save under GAME_MM with a zeroed
 *    shadow; UNAVAILABLE for the same buffers under GAME_OOT; STALE from the
 *    shadow when liveSave answers NULL (and the production adapter does answer
 *    NULL with no MM play state); the refused-arrival bootstrap. See
 *    CtvLiveLegsBody.
 *
 * 9. ROWS CARRY STATUS, AREA AND THE FOUND ITEM (#458 U4). Each game's rows
 *    carry the projected status the window colours by, the area its own check
 *    tracker lists the check under, and, once found, the item that tracker
 *    names there: MM's from the save the view picked (2b), OoT's from the heap
 *    (5b). A crossing host names the crossed item, not its cover, and the
 *    spelling is held to the native trackers' own test bridges (#796), so the
 *    window and the native trackers cannot disagree. See CtvMMRowsU4 and
 *    CtvOoTRowsU4.
 *
 * 10. THE SKIP TOGGLE WRITES BOTH PANES (#458 U5, #1294). The pair is one game,
 *    so the pane of the game not being played takes the toggle too. MM: the
 *    skipped byte of the live save while MM is played; while OoT plays the paired
 *    file, the same byte in the frozen MM half, which MM's arrival carries into
 *    the live save; nothing from MM's title or file select, unpaired, or with no
 *    armed half. OoT: the heap flag, persisted once per change, while OoT is
 *    played with a loaded save; while MM plays the paired file, the suspended
 *    heap of the world OoT departed from, journalled in Tier-1 with no OoT save
 *    section written and SoH's area order left to the arrival. Neither writes a
 *    found check or one outside the seed. A .redsave commit and load carry both
 *    far skips: MM's in the MM half, OoT's in the journal the load replays. See
 *    CtvSkipLegsMM, CtvSkipLegsOoT and CtvFarSkipRedsave. (Before #1294 these
 *    legs locked the opposite: a pane that was not LIVE refused every skip.)
 *
 * 11. THE OoT PANEL LISTS WHAT SoH'S CHECK TRACKER LISTS (#1126). Once SoH's
 *    tracker has loaded its settings, the rows and the summary follow its
 *    predicate (a stand-in here: the real one needs OTRGlobals), and while MM is
 *    played the panel keeps the list it read under OoT; before that load, or
 *    with no randomizer file loaded (a vanilla file, the title screen), the
 *    placed checks, with the predicate never asked. See CtvOoTListLegs.
 *
 * 12. REACHABILITY (#1129, #993 option 1). Each row says whether its check can be
 *    reached now, and whether that is known at all. LIVE: the OoT row's bit is
 *    the heap's ItemLocation::IsAvailable (authored through a seam), only under
 *    the list's live gate and once SoH's set is current; the MM row's bit is
 *    MM's in-logic set (a stand-in for ComputeChecksInLogic, which needs the
 *    region graph this tier lacks), computed once per refresh rather than per
 *    row, only while Enable Available Checks is on and MM plays a file. STALE:
 *    both panes read the set captured at the departure freeze, never a live
 *    computation; a context with no capture (a .redsave written before #1129)
 *    reads "not known". See CtvAvailMM and CtvAvailOoT.
 *
 * 13. A SEEN CHECK'S ITEM AND PRICE (#1166). OoT rows name what SoH's
 *    DrawLocation names beside a seen or identified check: a shop slot's item,
 *    an ice trap's disguise once seen and its trick name once identified, and the
 *    price after an identified check that sells something (CanBePurchased, SoH
 *    2ba368e7e), never after a Skulltula House reward. See CtvOoTSeenRows.
 *
 * 14. THE CURRENT AREA (#1131). Each game's adapter names the area its player is
 *    in, as an area key of its own rows: OoT's is SoH's Check Tracker's
 *    currentArea (authored through a seam) while OoT plays a randomizer file; MM's
 *    is the scene MM is played in (a stand-in for MM's play state), and a grotto
 *    keeps the scene it was entered from, where MM's tracker lists its checks.
 *    The active game's answer is live; the inactive game answers the area kept
 *    from its last live answer, never its adapter. See CtvNavOoT and CtvNavMM.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++), but every symbol it drives is extern-C. It needs the display-free
 * shared bring-up (the OoT-side authoring seam constructs Rando::Context),
 * so the entry point is a plain function the Test_ComboTrackerView wrapper
 * calls after CreateHarnessStyleContext.
 */

#include "../combo_tracker_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../save.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

// The OoT-side authoring seam (games/oot/soh/Enhancements/randomizer/
// TrackerAdapterSingleExe.cpp): places three checks — collected, untouched,
// skipped, ids returned flat — and holds/releases the heap context.
extern "C" int OoT_TrackerAdapter_TestAuthorWorld(uint32_t seed, uint16_t outIds[3]);
extern "C" void OoT_TrackerAdapter_TestReleaseWorld(void);

#define CTV_ASSERT(cond)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

// ---- #799: the live source ------------------------------------------------
//
// A test descriptor is the production one with liveSave pointed at an authored
// buffer, so the legs below drive the view's source pick with MM's real
// offsets and no MM play state. The buffer is whatever sCtvLive points at;
// NULL is "MM's play state is not loaded".
static uint8_t* sCtvLive = NULL;

static const void* CtvLiveSave(void) {
    return sCtvLive;
}

// MM's skip write (#458 U5), the code the production setLiveSkipped runs on
// &gSaveContext, here run on the same authored buffer liveSave answers.
extern "C" int MM_TrackerAdapter_TestSetSkippedIn(void* save, uint16_t checkId, int skipped);

static bool CtvSetLiveSkipped(uint16_t checkId, bool skipped) {
    return sCtvLive != NULL && MM_TrackerAdapter_TestSetSkippedIn(sCtvLive, checkId, skipped ? 1 : 0) != 0;
}

static void CtvSetObtained(std::vector<uint8_t>& buf, const ComboMMTrackerDesc* desc, uint16_t check, bool obtained) {
    buf[desc->checkTableOffset + (size_t)check * desc->checkStride + desc->obtainedOffset] = obtained ? 1 : 0;
}

/**
 * The legs of #799's lock, against `desc` (the REAL registered descriptor) and
 * `shadowWorld` (the marked rando shadow section 2 authored: seed 0x5EEDF00D,
 * checks 3/5/7 shuffled, 5 obtained, 7 skipped).
 *
 *   A. zeroed shadow + marked live world under GAME_MM -> LIVE totals, LIVE
 *      crossing found state (and it follows the live byte per call), LIVE note.
 *      Before #799 every one of these read UNAVAILABLE / UNKNOWN.
 *   B. the same buffers under GAME_OOT -> UNAVAILABLE: live is for the active
 *      game only.
 *   C. liveSave NULL (no MM play state) + marked shadow under GAME_MM -> STALE,
 *      the shadow's values.
 *   D. refused arrival: the shadow stays armed (never consumed) and MM plays
 *      the boot chain's bootstrap. An unmarked bootstrap (Sram_InitNewSave's
 *      empty newf, which is what the code authors) falls back to the armed
 *      shadow, STALE; a marked non-rando one
 *      would read LIVE as "not a randomized world" with found UNKNOWN.
 */
static int CtvLiveLegsBody(const ComboMMTrackerDesc* desc, std::vector<uint8_t>& shadowWorld) {
    const uint32_t kLiveSeed = 0x11FE0799u;
    const uint16_t kLiveA = 11, kLiveHost = 13, kLiveC = 17;

    // The live world: three shuffled checks, the crossing host collected.
    std::vector<uint8_t> live((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(live.data() + desc->newfOffset, desc->newf, desc->newfLen);
    memcpy(live.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
    memcpy(live.data() + desc->finalSeedOffset, &kLiveSeed, sizeof(uint32_t));
    for (uint16_t check : { kLiveA, kLiveHost, kLiveC }) {
        live[desc->checkTableOffset + (size_t)check * desc->checkStride + desc->shuffledOffset] = 1;
    }
    CtvSetObtained(live, desc, kLiveHost, true);

    // One crossing hosted by kLiveHost, so the found state and the note have
    // something to read.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kLiveSeed;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0799u;
    SharedItem ootItem;
    CTV_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootItem));
    CTV_ASSERT(Combo_SetForeignPlacement(kLiveHost, ootItem) >= 0);

    ComboTrackerGameSummary summary;
    ComboTrackerCheckRow row;
    ComboTrackerForeignRow foreignRow;
    ComboTrackerForeignProgress progress;

    // ---- A. arrived in Termina: shadow consumed and zeroed, MM played -------
    std::vector<uint8_t> zeros((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_UpdateShadowCopy(GAME_MM, zeros.data(), zeros.size());
    sCtvLive = live.data();
    Context_SetCurrentGame(GAME_MM);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    printf("[TEST] combo-tracker-view #799 A (GAME_MM, zeroed shadow, marked live): freshness=%u hasWorld=%d "
           "seed=0x%08X shuffled=%d obtained=%d\n",
           (unsigned)summary.freshness, (int)summary.hasWorld, (unsigned)summary.seed, summary.shuffled,
           summary.obtained);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE);
    CTV_ASSERT(summary.hasWorld && summary.seed == kLiveSeed);
    CTV_ASSERT(summary.totalChecks == (int)desc->checkCount);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 0);
    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_MM) == (int)desc->checkCount);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kLiveHost, &row));
    CTV_ASSERT(row.shuffled && row.obtained && !row.skipped);
    CTV_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, summary.freshness), "") == 0); // #927
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.hostCheckId == kLiveHost && foreignRow.found == COMBO_TRACKER_FOUND_YES);
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &progress);
    printf("[TEST] combo-tracker-view #799 A crossing: found=%u progress total=%d found=%d freshness=%u\n",
           (unsigned)foreignRow.found, progress.total, progress.found, (unsigned)progress.freshness);
    CTV_ASSERT(progress.total == 1 && progress.found == 1 && progress.freshness == COMBO_TRACKER_FRESH_LIVE);
    // Per call, no latch: the live byte flips and the row follows at once.
    CtvSetObtained(live, desc, kLiveHost, false);
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.found == COMBO_TRACKER_FOUND_NO);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && summary.obtained == 0);
    CtvSetObtained(live, desc, kLiveHost, true);

    // ---- B. the same buffers while OoT is the active game --------------------
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_MM) == 0);
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.found == COMBO_TRACKER_FOUND_UNKNOWN);
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &progress);
    CTV_ASSERT(progress.total == 1 && progress.found == 0 && progress.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);

    // ---- C. no MM play state (liveSave NULL) + marked shadow: STALE ---------
    Context_SetCurrentGame(GAME_MM);
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    sCtvLive = NULL;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.seed == 0x5EEDF00Du && summary.shuffled == 3 && summary.obtained == 1);
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &progress);
    CTV_ASSERT(progress.freshness == COMBO_TRACKER_FRESH_STALE);

    // ---- D. refused arrival: the armed shadow stays, MM plays the bootstrap --
    // The armed half: marked, rando, never loaded by MM.
    std::vector<uint8_t> armed = shadowWorld;
    Context_UpdateShadowCopy(GAME_MM, armed.data(), armed.size());
    // D1. What the code authors: Sram_InitNewSave's bootstrap carries an empty
    // newf and SAVETYPE_VANILLA, so the live read is refused and the armed
    // shadow is shown, stale.
    std::vector<uint8_t> bootstrap((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    sCtvLive = bootstrap.data();
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.hasWorld && summary.seed == 0x5EEDF00Du && summary.shuffled == 3);
    // D2. Were the bootstrap ever marked, it is MM's running save and is shown
    // live as what it is: not a randomized world, crossings' found not known.
    memcpy(bootstrap.data() + desc->newfOffset, desc->newf, desc->newfLen);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE);
    CTV_ASSERT(!summary.hasWorld && summary.seed == 0 && summary.shuffled == 0);
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.found == COMBO_TRACKER_FOUND_UNKNOWN);
    return TEST_PASS;
}

// ---- #458 U4: what a row carries beyond the three flags ----------------------
//
// The native trackers' own item-name functions (#796's test bridges): what MM's
// and OoT's check trackers print beside a found check, so the lock can hold the
// combo row to agreeing with them rather than to a copy of their rule.
extern "C" int MM_CheckTracker_TestItemName(const void* mmSave, uint16_t randoCheckId, char* out, int cap,
                                            uint16_t* outStoredItem);
extern "C" int OoT_CheckTracker_TestItemName(uint16_t rc, char* out, int cap);
// MM-side authoring: store the MM item whose display name is `name` at `checkId`
// of the SaveContext image `save` (games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp).
extern "C" int MM_TrackerAdapter_TestSetStoredItem(void* save, uint16_t checkId, const char* name);

/** The suffix the native trackers put on the other game's item (" (MM)" / " (OoT)"). */
static std::string CtvNativeSpelling(const ComboTrackerCheckRow& row, uint8_t rowGame) {
    std::string s = row.placedItemName != NULL ? row.placedItemName : "";
    if (row.placedItemGame != rowGame) {
        s += (row.placedItemGame == (uint8_t)GAME_MM) ? " (MM)" : " (OoT)";
    }
    return s;
}

/**
 * MM rows over the authored shadow (`blob`, already committed: 3 shuffled, 5
 * obtained, 7 skipped). Status is the flags' projection; every row has a real
 * scene name; only the obtained row names its item, which is the item MM's table
 * stores in THIS save, spelled as MM's own check tracker spells it.
 */
static int CtvMMRowsU4(const ComboMMTrackerDesc* desc, std::vector<uint8_t>& blob, uint16_t shuffledA,
                       uint16_t obtainedB, uint16_t skippedC) {
    CTV_ASSERT(desc->areaName != NULL);       // the MM TU names areas
    CTV_ASSERT(desc->placedItemName != NULL); // ...and placed items
    CTV_ASSERT(MM_TrackerAdapter_TestSetStoredItem(blob.data(), obtainedB, "Lens of Truth") == 1);
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());

    ComboTrackerCheckRow row;
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)obtainedB, &row));
    printf("[TEST] combo-tracker-view U4 MM obtained row: status=%u area=%s key=%u item=%s game=%u\n",
           (unsigned)row.status, row.areaName != NULL ? row.areaName : "(null)", (unsigned)row.areaKey,
           row.placedItemName != NULL ? row.placedItemName : "(null)", (unsigned)row.placedItemGame);
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_COLLECTED);
    CTV_ASSERT(row.areaName != NULL && row.areaName[0] != '\0' && strcmp(row.areaName, "Unknown") != 0);
    CTV_ASSERT(row.placedItemName != NULL && strcmp(row.placedItemName, "Lens of Truth") == 0);
    CTV_ASSERT(row.placedItemGame == (uint8_t)GAME_MM);
    // MM's tracker prints the full check name under its scene headers.
    CTV_ASSERT(row.shortName == NULL);
    // Agrees with MM's own check tracker, reading the same save.
    char native[96];
    uint16_t stored = 0;
    CTV_ASSERT(
        MM_CheckTracker_TestItemName(Context_GetMMSaveContext(), obtainedB, native, (int)sizeof(native), &stored) == 1);
    CTV_ASSERT(CtvNativeSpelling(row, (uint8_t)GAME_MM) == native);
    // The area is a property of the check, the same on every read.
    ComboTrackerCheckRow again;
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)obtainedB, &again));
    CTV_ASSERT(again.areaKey == row.areaKey && strcmp(again.areaName, row.areaName) == 0);

    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)skippedC, &row));
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_SKIPPED);
    CTV_ASSERT(row.areaName != NULL && row.areaName[0] != '\0');
    // Not found: nothing revealed, as MM's tracker prints no item there.
    CTV_ASSERT(row.placedItemName == NULL && row.placedItemGame == (uint8_t)GAME_NONE);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)shuffledA, &row));
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_UNCHECKED);
    CTV_ASSERT(row.placedItemName == NULL);
    return TEST_PASS;
}

/**
 * OoT rows over the seam's authored heap world (ootIds: saved, untouched,
 * skipped). The seam brings OoT's location and item tables up, so names and
 * areas are real here whatever the tier. Status follows SoH's precedence; the
 * three Kokiri Forest chests share one area; only the saved row names its item,
 * spelled as SoH's check tracker spells it, and a crossing host names the
 * crossed MM item rather than the cover it holds (#796). Leaves the context
 * paired: the caller resets it.
 */
static int CtvOoTRowsU4(const uint16_t ootIds[3]) {
    ComboTrackerCheckRow saved;
    ComboTrackerCheckRow open;
    ComboTrackerCheckRow skipped;
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &saved));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &open));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[2], &skipped));
    printf("[TEST] combo-tracker-view U4 OoT saved row: status=%u area=%s key=%u item=%s game=%u\n",
           (unsigned)saved.status, saved.areaName != NULL ? saved.areaName : "(null)", (unsigned)saved.areaKey,
           saved.placedItemName != NULL ? saved.placedItemName : "(null)", (unsigned)saved.placedItemGame);
    CTV_ASSERT(saved.status == COMBO_TRACKER_CHECK_SAVED);
    CTV_ASSERT(open.status == COMBO_TRACKER_CHECK_UNCHECKED);
    CTV_ASSERT(skipped.status == COMBO_TRACKER_CHECK_SKIPPED);
    CTV_ASSERT(saved.areaName != NULL && strcmp(saved.areaName, "Kokiri Forest") == 0);
    CTV_ASSERT(open.areaName != NULL && strcmp(open.areaName, "Kokiri Forest") == 0);
    CTV_ASSERT(saved.areaKey == open.areaKey && open.areaKey == skipped.areaKey);
    // The name SoH's tracker prints under the area header: the location's short
    // name (DrawLocation, GetShortName(); location_list.cpp's RC_KF_KOKIRI_SWORD_CHEST
    // entry), beside the full name the row keeps for the crossing table.
    printf("[TEST] combo-tracker-view U4 OoT saved row: name=%s shortName=%s\n",
           saved.name != NULL ? saved.name : "(null)", saved.shortName != NULL ? saved.shortName : "(null)");
    CTV_ASSERT(saved.name != NULL && strcmp(saved.name, "KF Kokiri Sword Chest") == 0);
    CTV_ASSERT(saved.shortName != NULL && strcmp(saved.shortName, "Kokiri Sword Chest") == 0);
    CTV_ASSERT(saved.placedItemName != NULL && saved.placedItemGame == (uint8_t)GAME_OOT);
    CTV_ASSERT(open.placedItemName == NULL && skipped.placedItemName == NULL);
    char native[96];
    CTV_ASSERT(OoT_CheckTracker_TestItemName(ootIds[0], native, (int)sizeof(native)) == 1);
    CTV_ASSERT(CtvNativeSpelling(saved, (uint8_t)GAME_OOT) == native);

    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
    SharedItem crossed;
    CTV_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &crossed));
    CTV_ASSERT(Combo_SetForeignPlacementOoT(ootIds[0], crossed) >= 0);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &saved));
    printf("[TEST] combo-tracker-view U4 OoT crossing host: item=%s game=%u\n",
           saved.placedItemName != NULL ? saved.placedItemName : "(null)", (unsigned)saved.placedItemGame);
    CTV_ASSERT(saved.placedItemName != NULL && strcmp(saved.placedItemName, "Lens of Truth") == 0);
    CTV_ASSERT(saved.placedItemGame == (uint8_t)GAME_MM);
    CTV_ASSERT(OoT_CheckTracker_TestItemName(ootIds[0], native, (int)sizeof(native)) == 1);
    CTV_ASSERT(CtvNativeSpelling(saved, (uint8_t)GAME_OOT) == native);
    return TEST_PASS;
}

// ---- #1166: a seen check's item and price --------------------------------------

// The OoT-side authoring seam (TrackerAdapterSingleExe.cpp): five seen checks, or
// (author 0) the same five back to empty.
extern "C" int OoT_TrackerAdapter_TestAuthorSeen(int author, uint16_t outIds[5]);

/** One OoT row's item and price as the window prints them, "(none)" for no item. */
static std::string CtvSeenSpelling(uint16_t id) {
    ComboTrackerCheckRow row;
    if (!Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)id, &row)) {
        return "(no row)";
    }
    std::string s = row.placedItemName != NULL ? row.placedItemName : "(none)";
    if (row.hasPrice) {
        s += " - " + std::to_string((unsigned)row.price);
    }
    if (row.status != COMBO_TRACKER_CHECK_SEEN || !row.shuffled) {
        s += " [not a listed seen row]";
    }
    return s;
}

/**
 * OoT rows SoH's tracker has seen (the seam's five), read from the suspended
 * heap with Mysterious Shuffle off: what SoH's DrawLocation prints beside each.
 */
static int CtvOoTSeenRows(void) {
    uint16_t ids[5] = {};
    CTV_ASSERT(OoT_TrackerAdapter_TestAuthorSeen(1, ids) == 5);
    const std::string shopIdentified = CtvSeenSpelling(ids[0]);
    const std::string shopSeen = CtvSeenSpelling(ids[1]);
    const std::string trapSeen = CtvSeenSpelling(ids[2]);
    const std::string trapIdentified = CtvSeenSpelling(ids[3]);
    const std::string reward = CtvSeenSpelling(ids[4]);
    printf("[TEST] combo-tracker-view #1166 OoT seen rows: shop identified \"%s\", shop seen \"%s\", ice trap seen "
           "\"%s\", ice trap identified \"%s\", Skulltula reward identified \"%s\"\n",
           shopIdentified.c_str(), shopSeen.c_str(), trapSeen.c_str(), trapIdentified.c_str(), reward.c_str());
    (void)OoT_TrackerAdapter_TestAuthorSeen(0, ids);
    CTV_ASSERT(shopIdentified == "Kokiri Sword - 40");
    CTV_ASSERT(shopSeen == "Kokiri Sword");
    CTV_ASSERT(trapSeen == "Fairy Ocarina");
    CTV_ASSERT(trapIdentified == "Kokiri Sord - 15");
    CTV_ASSERT(reward == "Kokiri Sword"); // SoH 2ba368e7e: no "- 0"
    return TEST_PASS;
}

/**
 * MM seen (`RCSEEN_SEEN` = 1) and identified (`RCSEEN_IDENTIFIED` = 2) rows over
 * the authored shadow: a shop slot seen on its shelf names its item with no
 * price; once identified (inspected), a purchasable check (RCTYPE_SHOP /
 * RCTYPE_TINGLE_SHOP) also prints its price, a non-purchasable check does not,
 * a crossing host names the crossed OoT item, and skipping a seen check turns
 * its status to SKIPPED while keeping its revealed item and price.
 */
static int CtvMMSeenRows(const ComboMMTrackerDesc* desc, const std::vector<uint8_t>& baseBlob) {
    CTV_ASSERT(desc->seenOffset != 0 && desc->seenOffset < desc->checkStride);
    CTV_ASSERT(desc->priceOffset != 0 && desc->priceOffset + 2 <= desc->checkStride);
    CTV_ASSERT(desc->checkStride == 12); // RandoSaveCheck.seen occupies offset 9 without growing the 12-byte row
    CTV_ASSERT(desc->canBePurchased != NULL);

    uint16_t shopId = 0;
    uint16_t nonShopId = 0;
    for (uint32_t id = 10; id < desc->checkCount && (shopId == 0 || nonShopId == 0); id++) {
        if (desc->checkName((uint16_t)id) == NULL) {
            continue;
        }
        if (desc->canBePurchased((uint16_t)id)) {
            if (shopId == 0) {
                shopId = (uint16_t)id;
            }
        } else if (nonShopId == 0) {
            nonShopId = (uint16_t)id;
        }
    }
    CTV_ASSERT(shopId != 0 && nonShopId != 0);

    std::vector<uint8_t> work = baseBlob;
    uint8_t* shopRow = work.data() + desc->checkTableOffset + (size_t)shopId * desc->checkStride;
    shopRow[desc->shuffledOffset] = 1;
    shopRow[desc->seenOffset] = 1; // RCSEEN_SEEN
    const uint16_t kShopPrice = 80;
    memcpy(shopRow + desc->priceOffset, &kShopPrice, sizeof(kShopPrice));
    CTV_ASSERT(MM_TrackerAdapter_TestSetStoredItem(work.data(), shopId, "Lens of Truth") == 1);

    uint8_t* nonShopRow = work.data() + desc->checkTableOffset + (size_t)nonShopId * desc->checkStride;
    nonShopRow[desc->shuffledOffset] = 1;
    nonShopRow[desc->seenOffset] = 2; // RCSEEN_IDENTIFIED on a non-purchasable check
    CTV_ASSERT(MM_TrackerAdapter_TestSetStoredItem(work.data(), nonShopId, "Lens of Truth") == 1);
    Context_UpdateShadowCopy(GAME_MM, work.data(), work.size());

    ComboTrackerCheckRow row;
    // 1. Seen shop slot: status is SEEN, item is named, price is not shown yet.
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)shopId, &row));
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_SEEN && !row.obtained && !row.skipped);
    CTV_ASSERT(row.placedItemName != NULL && strcmp(row.placedItemName, "Lens of Truth") == 0);
    CTV_ASSERT(row.placedItemGame == (uint8_t)GAME_MM);
    CTV_ASSERT(!row.hasPrice && row.price == 0);

    // 2. Identified shop slot: status is SEEN, item and price are both shown.
    shopRow[desc->seenOffset] = 2; // RCSEEN_IDENTIFIED
    Context_UpdateShadowCopy(GAME_MM, work.data(), work.size());
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)shopId, &row));
    printf("[TEST] combo-tracker-view MM seen rows: shopId=%u status=%u item=\"%s\" hasPrice=%d price=%u\n",
           (unsigned)shopId, (unsigned)row.status, row.placedItemName != NULL ? row.placedItemName : "(null)",
           (int)row.hasPrice, (unsigned)row.price);
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_SEEN);
    CTV_ASSERT(row.placedItemName != NULL && strcmp(row.placedItemName, "Lens of Truth") == 0);
    CTV_ASSERT(row.placedItemGame == (uint8_t)GAME_MM);
    CTV_ASSERT(row.hasPrice && row.price == kShopPrice);

    // 3. Crossing host on an identified MM shop slot: names the crossed OoT item + price.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
    SharedItem crossedOoT;
    CTV_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Kokiri Sword", &crossedOoT));
    CTV_ASSERT(Combo_SetForeignPlacement(shopId, crossedOoT) >= 0);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)shopId, &row));
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_SEEN);
    CTV_ASSERT(row.placedItemName != NULL && strcmp(row.placedItemName, "Kokiri Sword") == 0);
    CTV_ASSERT(row.placedItemGame == (uint8_t)GAME_OOT);
    CTV_ASSERT(row.hasPrice && row.price == kShopPrice);
    ComboContext_Init();

    // 4. Skipping an identified shop slot: status becomes SKIPPED, item + price stay revealed.
    shopRow[desc->skippedOffset] = 1;
    Context_UpdateShadowCopy(GAME_MM, work.data(), work.size());
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)shopId, &row));
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_SKIPPED && row.skipped);
    CTV_ASSERT(row.placedItemName != NULL && strcmp(row.placedItemName, "Lens of Truth") == 0);
    CTV_ASSERT(row.hasPrice && row.price == kShopPrice);

    // 5. Non-purchasable check even at seen=2: names item, never prints "- 0".
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)nonShopId, &row));
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_SEEN);
    CTV_ASSERT(row.placedItemName != NULL && strcmp(row.placedItemName, "Lens of Truth") == 0);
    CTV_ASSERT(!row.hasPrice && row.price == 0);

    Context_UpdateShadowCopy(GAME_MM, baseBlob.data(), baseBlob.size());
    return TEST_PASS;
}

// ---- #458 U5, #1294: the skip toggle, on both panes ---------------------------

// Failed U5 halves (MM, OoT), reported together at the end of the row.
static int sCtvU5Failures = 0;

// switch.cpp: an arrival's consumption of its game's frozen half (#1294's MM arrival leg).
extern "C" int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

static uint8_t CtvSkipByte(const uint8_t* save, const ComboMMTrackerDesc* desc, uint16_t check) {
    return save[desc->checkTableOffset + (size_t)check * desc->checkStride + desc->skippedOffset];
}

// ---- #1294's stand-ins for "the other game plays a randomizer file" ----------------
//
// MM's far pane is written only while OoT plays a randomizer file, which the view
// asks OoT's adapter (its live skip gate). Where the real OoT adapter is not
// registered yet, a stub vtable answers it: sCtvOoTPlays.
static bool sCtvOoTPlays = false;
static bool CtvStubSummary(ComboTrackerGameSummary*) {
    return false;
}
static int CtvStubCount(void) {
    return 0;
}
static bool CtvStubCheckAt(int, ComboTrackerCheckRow*) {
    return false;
}
static const char* CtvStubCheckName(uint16_t) {
    return NULL;
}
static bool CtvStubOoTPlays(void) {
    return sCtvOoTPlays;
}
static bool CtvStubSetSkipped(uint16_t, bool) {
    return false;
}
static const ComboOoTTrackerOps kCtvOoTPlaysOps = { CtvStubSummary,   CtvStubCount,    CtvStubCheckAt,
                                                    CtvStubCheckName, CtvStubOoTPlays, CtvStubSetSkipped };

// OoT's far pane is written only while MM plays a randomizer file, which the view
// reads from MM's live save; this tier has no MM play state, so the production
// descriptor is registered with liveSave pointed at a marked, randomized buffer.
static std::vector<uint8_t> sCtvPlayedMM;

static void CtvMMPlaysBegin(const ComboMMTrackerDesc& real) {
    sCtvPlayedMM.assign((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(sCtvPlayedMM.data() + real.newfOffset, real.newf, real.newfLen);
    memcpy(sCtvPlayedMM.data() + real.saveTypeOffset, &real.saveTypeRando, sizeof(uint32_t));
    ComboMMTrackerDesc played = real;
    played.liveSave = CtvLiveSave;
    Combo_Tracker_RegisterMM(&played);
    sCtvLive = sCtvPlayedMM.data();
}

static void CtvMMPlaysEnd(const ComboMMTrackerDesc& real) {
    sCtvLive = NULL;
    Combo_Tracker_RegisterMM(&real);
}

/**
 * MM's half of the toggle (#458 U5), against the test descriptor (the production
 * one, liveSave pointed at sCtvLive and setLiveSkipped at MM's own write code run
 * on that buffer) and the marked shadow world section 2 authored (check 3
 * shuffled and open there). The production setLiveSkipped's refusal with no MM
 * play state is section 8's.
 *
 *   A. LIVE (MM played, marked live save): an open check is skippable; the write
 *      lands in the LIVE save's skipped byte, the row and the summary follow, the
 *      shadow is untouched; unskip clears it. A found check, a check outside the
 *      seed and an out-of-range id are refused with nothing written.
 *   B. STALE while MM is the active game with no play state (its title screen or
 *      file select): nothing is skippable and nothing is written. The far write
 *      is for the pane of the game not being played.
 *   C. FAR (#1294): OoT is the active game. With the MM half not armed, the
 *      pairing not live, or OoT not playing a randomizer file (its title, file
 *      select, a vanilla file), refused with nothing written. With the paired
 *      half armed and OoT playing it, an open check of the SHADOW is skippable: the write lands in the
 *      frozen half's skipped byte (the one RsbsSave_StageCommit stores as
 *      Tier-3), the STALE row and summary follow, MM's live buffer is untouched,
 *      unskip clears it, and a found check, a check outside the seed and an
 *      out-of-range id are refused. MM's arrival (Combo_ConsumeFrozenState)
 *      carries the skip into the live save, and the consumed half takes no
 *      further far write.
 */
static int CtvSkipLegsMM(const ComboMMTrackerDesc* desc, std::vector<uint8_t>& shadowWorld) {
    const uint16_t kOpen = 21, kFound = 23, kOutside = 25, kShadowOpen = 3;
    std::vector<uint8_t> live((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(live.data() + desc->newfOffset, desc->newf, desc->newfLen);
    memcpy(live.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
    for (uint16_t check : { kOpen, kFound }) {
        live[desc->checkTableOffset + (size_t)check * desc->checkStride + desc->shuffledOffset] = 1;
    }
    CtvSetObtained(live, desc, kFound, true);
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());

    ComboTrackerCheckRow row;
    ComboTrackerGameSummary summary;

    // ---- A. LIVE ---------------------------------------------------------------
    sCtvLive = live.data();
    Context_SetCurrentGame(GAME_MM);
    CTV_ASSERT(Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kOpen, &row));
    CTV_ASSERT(Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    const bool wrote = Combo_TrackerSetSkipped((uint8_t)GAME_MM, kOpen, true);
    printf("[TEST] combo-tracker-view U5 MM live skip: returned=%d liveByte=%u shadowByte=%u\n", (int)wrote,
           (unsigned)CtvSkipByte(live.data(), desc, kOpen),
           (unsigned)CtvSkipByte((const uint8_t*)Context_GetMMSaveContext(), desc, kOpen));
    CTV_ASSERT(wrote);
    CTV_ASSERT(CtvSkipByte(live.data(), desc, kOpen) == 1);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kOpen, &row));
    CTV_ASSERT(row.skipped && row.status == COMBO_TRACKER_CHECK_SKIPPED);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && summary.skipped == 1);
    // The write went to the live save only: the shadow is byte-for-byte what was committed.
    CTV_ASSERT(memcmp(Context_GetMMSaveContext(), shadowWorld.data(), shadowWorld.size()) == 0);
    // Setting what is already set succeeds and changes nothing; unskip clears it.
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_MM, kOpen, true));
    CTV_ASSERT(CtvSkipByte(live.data(), desc, kOpen) == 1);
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_MM, kOpen, false));
    CTV_ASSERT(CtvSkipByte(live.data(), desc, kOpen) == 0);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kOpen, &row));
    CTV_ASSERT(!row.skipped && row.status == COMBO_TRACKER_CHECK_UNCHECKED);
    // A found check and a check outside the seed are not offered and not written.
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kFound, &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kFound, true));
    CTV_ASSERT(CtvSkipByte(live.data(), desc, kFound) == 0);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kOutside, &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kOutside, true));
    CTV_ASSERT(CtvSkipByte(live.data(), desc, kOutside) == 0);
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, (uint16_t)desc->checkCount, true));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_MM, NULL));

    // ---- B. STALE: the shadow panel never writes --------------------------------
    sCtvLive = NULL;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kShadowOpen, &row));
    CTV_ASSERT(row.shuffled && !row.obtained && !row.skipped); // a check the live panel would offer
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true));
    CTV_ASSERT(memcmp(Context_GetMMSaveContext(), shadowWorld.data(), shadowWorld.size()) == 0);

    // ---- C. FAR: OoT played, the MM half frozen (#1294) --------------------------
    sCtvLive = live.data();
    const std::vector<uint8_t> liveBefore = live;
    Context_SetCurrentGame(GAME_OOT);
    Combo_Tracker_RegisterOoT(&kCtvOoTPlaysOps); // OoT's adapter is registered in section 5
    sCtvOoTPlays = true;
    // C1. Not armed: the shadow is resident but no freeze, load or creation armed it.
    Context_ClearFrozenState(GAME_MM);
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    CTV_ASSERT(Combo_ForeignPairingActive()); // CtvLiveLegsBody's pairing
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true));
    CTV_ASSERT(memcmp(Context_GetMMSaveContext(), shadowWorld.data(), shadowWorld.size()) == 0);
    // C2. Armed but unpaired: no paired world, no far write.
    CTV_ASSERT(Context_ArmShadowAsFrozen(GAME_MM, 0) == 1);
    const uint32_t pairHash = gComboCtx.sharedRandoSettingsHash;
    gComboCtx.sharedRandoSettingsHash = 0;
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true));
    CTV_ASSERT(memcmp(Context_GetMMSaveContext(), shadowWorld.data(), shadowWorld.size()) == 0);
    gComboCtx.sharedRandoSettingsHash = pairHash;
    // C2b. Armed and paired, but OoT plays no randomizer file: refused.
    sCtvOoTPlays = false;
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true));
    CTV_ASSERT(memcmp(Context_GetMMSaveContext(), shadowWorld.data(), shadowWorld.size()) == 0);
    sCtvOoTPlays = true;
    // C3. Armed, paired, OoT playing: the far pane takes the toggle.
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE && summary.skipped == 1);
    CTV_ASSERT(Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kShadowOpen, &row));
    CTV_ASSERT(Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    const bool farWrote = Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true);
    const uint8_t* shadow = (const uint8_t*)Context_GetMMSaveContext();
    printf("[TEST] combo-tracker-view #1294 MM far skip (OoT played): returned=%d shadowByte=%u liveByte=%u\n",
           (int)farWrote, (unsigned)CtvSkipByte(shadow, desc, kShadowOpen),
           (unsigned)CtvSkipByte(live.data(), desc, kShadowOpen));
    CTV_ASSERT(farWrote);
    CTV_ASSERT(CtvSkipByte(shadow, desc, kShadowOpen) == 1);
    CTV_ASSERT(live == liveBefore); // MM's live buffer is not the far pane's
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kShadowOpen, &row));
    CTV_ASSERT(row.skipped && row.status == COMBO_TRACKER_CHECK_SKIPPED);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE && summary.skipped == 2);
    // Only that byte moved.
    std::vector<uint8_t> expect = shadowWorld;
    expect[desc->checkTableOffset + (size_t)kShadowOpen * desc->checkStride + desc->skippedOffset] = 1;
    CTV_ASSERT(memcmp(shadow, expect.data(), expect.size()) == 0);
    // Already set: success, nothing changes; unskip clears it; skip again.
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true));
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, false));
    CTV_ASSERT(CtvSkipByte(shadow, desc, kShadowOpen) == 0);
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true));
    // The authored found check (5), a check outside the seed (4) and an id past the table: refused.
    const uint16_t kShadowFound = 5, kShadowOutside = 4;
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kShadowFound, &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowFound, true));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kShadowOutside, &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_MM, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOutside, true));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, (uint16_t)desc->checkCount, true));
    CTV_ASSERT(memcmp(shadow, expect.data(), expect.size()) == 0);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_NONE));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_NONE, kOpen, true));
    // C4. MM's arrival consumes the half into the live save: the skip is MM's own.
    std::vector<uint8_t> arrived((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    CTV_ASSERT(Combo_ConsumeFrozenState("mm", arrived.data(), arrived.size()) == 1);
    printf("[TEST] combo-tracker-view #1294 MM arrival: consumed skip byte=%u\n",
           (unsigned)CtvSkipByte(arrived.data(), desc, kShadowOpen));
    CTV_ASSERT(CtvSkipByte(arrived.data(), desc, kShadowOpen) == 1);
    CTV_ASSERT(memcmp(arrived.data(), expect.data(), expect.size()) == 0);
    // A consumed half is not armed: no far write lands in it.
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    sCtvLive = NULL;
    sCtvOoTPlays = false;
    Combo_Tracker_RegisterOoT(NULL);
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    return TEST_PASS;
}

// The OoT-side skip seam (TrackerAdapterSingleExe.cpp): -1 production, 0/1 the
// answer to "is a save loaded" with persists counted; returns the count since the
// previous call.
extern "C" int OoT_TrackerAdapter_TestSkipSeam(int saveLoaded);
// The OoT-side reachability seam (#1129), declared here for the far legs' departure.
extern "C" void OoT_TrackerAdapter_TestAvailableSeam(int current);
// #1294: the arrival's tracker notification (OTRGlobals.cpp, z_play.c calls it) and
// whether a far-pane skip left SoH's area order to it (randomizer_check_tracker.cpp).
extern "C" void CheckTracker_ArmAvailableChecksRecalc(void);
extern "C" int RandoTest_CheckTrackerOrderingDeferred(void);
extern "C" void RandoTest_CheckTrackerClearRecalcPending(void);

/** The journal's entry for OoT check `id`: 1 skipped, 0 unskipped, -1 none. */
static int CtvJournalValue(uint16_t id) {
    for (int i = 0; i < Combo_TrackerOoTSkipJournalCount(); i++) {
        uint16_t at = 0;
        bool skipped = false;
        if (Combo_TrackerOoTSkipJournalAt(i, &at, &skipped) && at == id) {
            return skipped ? 1 : 0;
        }
    }
    return -1;
}

/** OoT departs (OoT_Game_Suspend's capture) playing a randomizer file (1) or not (0). */
static void CtvOoTDepart(int playingRandoFile) {
    Context_SetCurrentGame(GAME_OOT);
    (void)OoT_TrackerAdapter_TestSkipSeam(playingRandoFile);
    OoT_TrackerAdapter_TestAvailableSeam(0); // the set is not current: the capture clears its record
    OoT_TrackerAdapter_CaptureAvailability();
    OoT_TrackerAdapter_TestAvailableSeam(-1);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    Context_SetCurrentGame(GAME_MM);
}

/**
 * OoT's half of the toggle (#458 U5, #1294), over the seam's authored heap world
 * (ootIds: saved, untouched, skipped).
 *
 *   A. MM played: OoT's panel is the suspended heap (#1294). Refused while that
 *      heap is not a world OoT departed from playing it, while unpaired, and
 *      while MM plays no randomizer file (its title, file select). After
 *      a departure from the randomizer file, paired: the untouched check is
 *      skippable; the write sets the heap flag, journals (id, skipped) in Tier-1,
 *      issues no OoT save section (gSaveContext is MM's) and leaves SoH's area
 *      order to the arrival, which takes it. A no-op writes nothing; unskip
 *      rewrites the same entry; the saved check and a check outside the seed are
 *      refused with nothing journalled.
 *   B. OoT played with no loaded save (the production predicate: this tier has
 *      no play state): the panel is LIVE but refused, as SoH's tracker shows no
 *      list before a file loads.
 *   C. OoT played with a loaded save: the untouched check is skipped on the heap,
 *      the row and summary follow, and the tracker-data section is persisted
 *      once per change (not for a no-op); the skipped check unskips. The saved
 *      check and a check outside the seed are refused with nothing persisted. A
 *      live toggle of a journalled check rewrites its entry (a load's replay must
 *      not bring the far pane's older answer back) and journals no other check.
 */
static int CtvSkipLegsOoT(const uint16_t ootIds[3]) {
    ComboTrackerCheckRow row;
    ComboTrackerGameSummary summary;
    const uint16_t kOutside = (uint16_t)(ootIds[2] + 1); // not placed by the seam

    // ---- A. MM played: the far pane (#1294) --------------------------------------
    const ComboMMTrackerDesc* mmDesc = Combo_Tracker_GetMMDesc();
    CTV_ASSERT(mmDesc != NULL);
    const ComboMMTrackerDesc realMM = *mmDesc;
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFE129u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED1294u;
    Combo_TrackerOoTSkipJournalClear();
    RandoTest_CheckTrackerClearRecalcPending();
    CheckTracker_ArmAvailableChecksRecalc(); // drop any order pass an earlier row deferred
    RandoTest_CheckTrackerClearRecalcPending();
    // A1. OoT departed from no randomizer file (its title, a vanilla file): the
    // heap is not the played world, so refused even paired, with MM playing.
    CtvOoTDepart(0);
    CtvMMPlaysBegin(realMM);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.skipped);
    // A2. Departed from the randomizer file, but unpaired: refused.
    CtvOoTDepart(1);
    gComboCtx.sharedRandoSettingsHash = 0;
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    gComboCtx.sharedRandoSettingsHash = 0x5EED1294u;
    // A2b. Paired, but MM plays no randomizer file (no play state): refused.
    sCtvLive = NULL;
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    sCtvLive = sCtvPlayedMM.data();
    // A3. Paired, MM playing: the far pane takes the toggle.
    CTV_ASSERT(Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    CTV_ASSERT(Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    (void)OoT_TrackerAdapter_TestSkipSeam(1); // count any section save the write might issue
    Context_SetCurrentGame(GAME_MM);
    const bool farWrote = Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    const int farPersists = OoT_TrackerAdapter_TestSkipSeam(1);
    printf("[TEST] combo-tracker-view #1294 OoT far skip (MM played): returned=%d heapSkipped=%d persists=%d "
           "journal=%d (entry %d) orderDeferred=%d\n",
           (int)farWrote, (int)row.skipped, farPersists, Combo_TrackerOoTSkipJournalCount(), CtvJournalValue(ootIds[1]),
           RandoTest_CheckTrackerOrderingDeferred());
    CTV_ASSERT(farWrote);
    CTV_ASSERT(row.skipped && row.status == COMBO_TRACKER_CHECK_SKIPPED);
    CTV_ASSERT(farPersists == 0); // no OoT save section while MM owns gSaveContext
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1 && CtvJournalValue(ootIds[1]) == 1);
    CTV_ASSERT(RandoTest_CheckTrackerOrderingDeferred() == 1);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE && summary.skipped == 2);
    // A no-op succeeds and journals nothing new; unskip rewrites the same entry.
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1);
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], false));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.skipped);
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1 && CtvJournalValue(ootIds[1]) == 0);
    // The saved check and a check outside the seed: no button, nothing journalled.
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[0], true));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, kOutside, true));
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1);
    CTV_ASSERT(OoT_TrackerAdapter_TestSkipSeam(-1) == 0);
    CtvMMPlaysEnd(realMM);
    // OoT's arrival takes the deferred order pass.
    CheckTracker_ArmAvailableChecksRecalc();
    CTV_ASSERT(RandoTest_CheckTrackerOrderingDeferred() == 0);
    RandoTest_CheckTrackerClearRecalcPending();

    // ---- B. OoT played, no loaded save -----------------------------------------
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.skipped);

    // ---- C. OoT played, a loaded save ---------------------------------------------
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    CTV_ASSERT(Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    CTV_ASSERT(Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    const bool wrote = Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    int persists = OoT_TrackerAdapter_TestSkipSeam(1);
    printf("[TEST] combo-tracker-view U5 OoT live skip: returned=%d heapSkipped=%d status=%u persists=%d\n", (int)wrote,
           (int)row.skipped, (unsigned)row.status, persists);
    CTV_ASSERT(wrote);
    CTV_ASSERT(row.skipped && row.status == COMBO_TRACKER_CHECK_SKIPPED);
    CTV_ASSERT(persists == 1);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.skipped == 2);
    // A no-op write succeeds without persisting.
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    CTV_ASSERT(OoT_TrackerAdapter_TestSkipSeam(1) == 0);
    // The authored skipped check unskips (the button's other face) and back.
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[2], false));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[2], &row) && !row.skipped);
    CTV_ASSERT(row.status == COMBO_TRACKER_CHECK_UNCHECKED);
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[2], true));
    CTV_ASSERT(Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], false));
    CTV_ASSERT(OoT_TrackerAdapter_TestSkipSeam(1) == 3);
    // #1294: the journalled check's entry followed each live toggle of it (last:
    // unskipped), and the live toggles of the other check journalled nothing.
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1 && CtvJournalValue(ootIds[1]) == 0);
    CTV_ASSERT(CtvJournalValue(ootIds[2]) == -1);
    // The saved check and a check outside the seed: no button, no write.
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[0], true));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row) && !row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kOutside, &row) && !row.shuffled);
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, kOutside, true));
    CTV_ASSERT(OoT_TrackerAdapter_TestSkipSeam(-1) == 0);
    // Back to the authored world: one skipped, one untouched.
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.skipped == 1 && summary.obtained == 1);
    Combo_TrackerOoTSkipJournalClear();
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 0);
    ComboContext_Init(); // unpaired again, as the sections after this one expect
    return TEST_PASS;
}

/**
 * #1294: both far skips survive a .redsave commit and load. MM's lands in the
 * frozen MM half the commit stores as Tier-3 and the load brings back; OoT's
 * rides the journal in Tier-1, and OoT's load seam replays it over the .sav's
 * older tracker data (re-authoring the world stands in for that .sav load).
 * The OoT journal also refuses a far skip once full, writing nothing.
 */
static int CtvFarSkipRedsaveBody(const ComboMMTrackerDesc* desc, const std::vector<uint8_t>& shadowWorld,
                                 uint32_t ootSeed, uint16_t ootIds[3]) {
    const uint16_t kShadowOpen = 3;
    const ComboMMTrackerDesc realMM = *desc; // by value: the stand-in below re-registers the descriptor
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFE12Au;
    gComboCtx.sharedRandoSettingsHash = 0x5EED129Au;
    Combo_TrackerOoTSkipJournalClear();
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    CTV_ASSERT(Context_ArmShadowAsFrozen(GAME_MM, 0) == 1);

    // MM's far skip, OoT playing its randomizer file (the OoT adapter's skip seam).
    Context_SetCurrentGame(GAME_OOT);
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    const bool mmFar = Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShadowOpen, true);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    CTV_ASSERT(mmFar);
    // OoT's far skip, MM playing, after OoT departed from its randomizer file.
    CtvOoTDepart(1);
    CtvMMPlaysBegin(realMM);
    const bool ootFar = Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true);
    CtvMMPlaysEnd(realMM);
    CTV_ASSERT(ootFar);
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1);

    // The commit (an MM-side save), then nothing survives but the file.
    mgr.SetSaveDirectory(rsbs::test::ScratchDir("rsbs_test_combo_tracker_view_far_skip").string());
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0); // an erase is what unlatches the slot for this session's write
    CTV_ASSERT(mgr.Save(0));
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    OoT_TrackerAdapter_TestReleaseWorld();
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 0);

    // The load.
    Context_InvalidateSessionOnSlotLoad();
    CTV_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK);
    const uint8_t* mmHalf = (const uint8_t*)Context_GetMMSaveContext();
    const int mmByte = mmHalf != NULL ? (int)CtvSkipByte(mmHalf, desc, kShadowOpen) : -1;
    CTV_ASSERT(OoT_TrackerAdapter_TestAuthorWorld(ootSeed, ootIds) == 3); // OoT's .sav: without the far skip
    ComboTrackerCheckRow row;
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.skipped);
    Context_SetCurrentGame(GAME_OOT); // OoT's load seam runs while OoT is played
    const int replayed = OoT_TrackerAdapter_ReplaySkipJournal();
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    printf("[TEST] combo-tracker-view #1294 redsave round trip: MM half skip byte=%d, OoT journal=%d, replayed=%d, "
           "heapSkipped=%d\n",
           mmByte, Combo_TrackerOoTSkipJournalCount(), replayed, (int)row.skipped);
    CTV_ASSERT(mmByte == 1);
    CTV_ASSERT(Combo_TrackerOoTSkipJournalCount() == 1 && CtvJournalValue(ootIds[1]) == 1);
    CTV_ASSERT(replayed == 1 && row.skipped);
    CTV_ASSERT(OoT_TrackerAdapter_ReplaySkipJournal() == 0); // already applied: nothing changes

    // A full journal refuses a far skip and writes nothing.
    Combo_TrackerOoTSkipJournalClear();
    for (uint16_t i = 0; i < (uint16_t)RSBS_TRACKER_SKIP_JOURNAL_OOT_CAP; i++) {
        CTV_ASSERT(Combo_TrackerOoTSkipJournalPut((uint16_t)(0x7000u + i), true, false));
    }
    CTV_ASSERT(!Combo_TrackerOoTSkipJournalPut(0x6FFFu, true, false));
    CTV_ASSERT(Combo_TrackerOoTSkipJournalPut(0x7000u, false, false)); // a present entry is still rewritten
    CtvOoTDepart(1);                                                   // paired by the load's Tier-1
    CtvMMPlaysBegin(realMM);
    CTV_ASSERT(Combo_TrackerSkipWritable((uint8_t)GAME_OOT)); // the pane takes writes...
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && row.skipped);
    const bool overFull = Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], false); // ...but not this one
    CtvMMPlaysEnd(realMM);
    CTV_ASSERT(!overFull);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && row.skipped);
    Combo_TrackerOoTSkipJournalClear();
    return TEST_PASS;
}

static int CtvFarSkipRedsave(const ComboMMTrackerDesc* desc, const std::vector<uint8_t>& shadowWorld, uint32_t ootSeed,
                             uint16_t ootIds[3]) {
    const GameId prevGame = Context_GetCurrentGame();
    const ComboMMTrackerDesc realMM = *desc;
    // The commit stores, and the load restores, the OoT half too: put it back after.
    std::vector<uint8_t> ootBackup((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    const uint8_t* ootResident = (const uint8_t*)Context_GetOoTSaveContext();
    if (ootResident != NULL) {
        memcpy(ootBackup.data(), ootResident, ootBackup.size());
    }
    const int result = CtvFarSkipRedsaveBody(&realMM, shadowWorld, ootSeed, ootIds);
    sCtvLive = NULL;
    Combo_Tracker_RegisterMM(&realMM); // whatever a failed leg left registered
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    OoT_TrackerAdapter_TestAvailableSeam(-1);
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    Context_UpdateShadowCopy(GAME_OOT, ootBackup.data(), ootBackup.size());
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    CTV_ASSERT(OoT_TrackerAdapter_TestAuthorWorld(ootSeed, ootIds) == 3); // the authored world the later legs read
    Context_SetCurrentGame(prevGame);
    return result;
}

// The OoT-side listing seam (TrackerAdapterSingleExe.cpp, #1126): -1 production,
// 0/1 the answer to "are SoH's Check Tracker settings loaded", with 1 a stand-in
// for SoH's predicate that lists exactly `visible`; returns the questions put to
// the predicate since the previous call.
extern "C" int OoT_TrackerAdapter_TestListSeam(int settingsLoaded, const uint16_t* visible, int count);

/**
 * Which OoT checks the panel lists (#1126), over the seam's authored heap world
 * (ootIds: saved, untouched, skipped; kOutside is not placed).
 *
 *   A. Production in this tier: SoH's Check Tracker never loaded its settings
 *      (its OnLoadGame hook is not registered here), so the panel lists the
 *      placed checks and never asks SoH's predicate. That predicate dereferences
 *      OTRGlobals::Instance, which this tier does not have: a question would
 *      crash the row, and the count proves none was put.
 *   B. The seam says the settings are not loaded: the same.
 *   B2. The settings are loaded but no randomizer file is (the skip seam's
 *      answer: a vanilla file, or the title screen with SoH's `initialized` left
 *      over from a file MM returned to title from): the same, never asked, so a
 *      vanilla file is not listed through the predicate's vanilla branch.
 *   C. The settings are loaded and OoT is playing a randomizer file: the panel
 *      lists what SoH's predicate (the stand-in) lists, placed or not: the saved
 *      check and an unplaced one, not the untouched or skipped placed checks;
 *      the summary counts those rows, and the rows read after it are served
 *      from the list the summary read (no second question per row).
 *   D. MM is played: the panel keeps the list it read while OoT ran and asks
 *      nothing, even though the stand-in now lists nothing.
 *   E. The settings are unloaded again (a file exit): the placed checks once more.
 */
static int CtvOoTListLegs(const uint16_t ootIds[3]) {
    ComboTrackerCheckRow row;
    ComboTrackerGameSummary summary;
    const uint16_t kOutside = (uint16_t)(ootIds[2] + 1); // not placed by the seam

    // ---- A. production: the placed checks, no question asked --------------------
    (void)OoT_TrackerAdapter_TestListSeam(-1, NULL, 0);
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && summary.hasWorld);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 1);
    for (int i = 0; i < 3; i++) {
        CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[i], &row) && row.shuffled);
    }
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kOutside, &row) && !row.shuffled);
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(-1, NULL, 0) == 0);

    // ---- B. the seam says not loaded: the same ----------------------------------
    (void)OoT_TrackerAdapter_TestListSeam(0, NULL, 0);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 1);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && row.shuffled);
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(0, NULL, 0) == 0);

    // ---- B2. loaded, but no randomizer file loaded: the same ------------------------
    const uint16_t visible[2] = { ootIds[0], kOutside };
    (void)OoT_TrackerAdapter_TestListSeam(1, visible, 2);
    (void)OoT_TrackerAdapter_TestSkipSeam(0);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    printf("[TEST] combo-tracker-view #1126 OoT list, settings loaded, no randomizer file: hasWorld=%d shuffled=%d "
           "obtained=%d skipped=%d\n",
           summary.hasWorld ? 1 : 0, summary.shuffled, summary.obtained, summary.skipped);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && summary.hasWorld);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 1);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && row.shuffled);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kOutside, &row) && !row.shuffled);
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(1, visible, 2) == 0);

    // ---- C. loaded, OoT playing a randomizer file: SoH's list ----------------------
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    printf("[TEST] combo-tracker-view #1126 OoT list, settings loaded: freshness=%u shuffled=%d obtained=%d "
           "skipped=%d\n",
           (unsigned)summary.freshness, summary.shuffled, summary.obtained, summary.skipped);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && summary.hasWorld);
    CTV_ASSERT(summary.shuffled == 2 && summary.obtained == 1 && summary.skipped == 0);
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(1, visible, 2) > 0); // the summary asked
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row) && row.shuffled && row.obtained);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.shuffled);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[2], &row) && !row.shuffled && row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kOutside, &row) && row.shuffled);
    // The rows asked nothing (they read the summary's list); the stand-in now lists nothing.
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(1, NULL, 0) == 0);

    // ---- D. MM played: the list as of OoT, no question asked -----------------------
    Context_SetCurrentGame(GAME_MM);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.shuffled == 2 && summary.obtained == 1 && summary.skipped == 0);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.shuffled);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kOutside, &row) && row.shuffled);
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(1, NULL, 0) == 0);

    // ---- E. unloaded again: the placed checks, under either game ----------------------
    (void)OoT_TrackerAdapter_TestListSeam(-1, NULL, 0);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 1);
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 1);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kOutside, &row) && !row.shuffled);
    CTV_ASSERT(OoT_TrackerAdapter_TestListSeam(-1, NULL, 0) == 0);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    Context_SetCurrentGame(GAME_MM); // as the caller had it
    return TEST_PASS;
}

// ---- #1129: reachability -------------------------------------------------------

// The reachability seams: MM's (TrackerAdapterSingleExe.cpp in 2s2h/Rando) stand in
// for MM's play state and for ComputeChecksInLogic and count its computations;
// OoT's answer "is SoH's available set current" and author IsAvailable.
extern "C" void MM_TrackerAdapter_TestLiveSeam(int live);
extern "C" int MM_TrackerAdapter_TestInLogicSeam(int authored, const uint16_t* ids, int count);
extern "C" void OoT_TrackerAdapter_TestAvailableSeam(int current);
extern "C" int OoT_TrackerAdapter_TestSetAvailable(uint16_t rc, int available);

// SoH's Enable Available Checks, which both panes' availability follows (one set
// of settings, ruling D3).
static const char* const kCtvAvailableKey = "gTrackers.CheckTracker.EnableAvailableChecks";

static int sCtvAvailFailures = 0;

/** `game`'s row `id` as one comparable value: -1 not known, else its available bit (-2: no row). */
static int CtvAvail(uint8_t game, uint16_t id) {
    ComboTrackerCheckRow row;
    if (!Combo_TrackerCheckAt(game, (int)id, &row)) {
        return -2;
    }
    return row.availabilityKnown ? (row.available ? 1 : 0) : -1;
}

/**
 * MM's half (#1129), over a test descriptor (the production one, liveSave pointed at
 * an authored live world: checks 11, 13, 17 shuffled, 13 obtained, 17 hosting a
 * crossing) with the PRODUCTION liveAvailable.
 *
 *   A. LIVE: the rows follow the in-logic set; the summary counts it; the
 *      computation runs once for the summary and every row (a cached set, MM's
 *      20-frame cadence), and again only when the set is dropped; the crossing
 *      row carries its host's bit. Enable Available Checks off, or no MM play
 *      state: not known, and nothing computed.
 *   B. STALE: the departure capture (MM_TrackerAdapter_CaptureAvailability) is
 *      what the snapshot pane shows once OoT is played, with no computation at
 *      all; a departure with no file played leaves it, one with the option off
 *      clears it; a context with no capture (pre-#1129) reads not known.
 */
static int CtvAvailMMBody(std::vector<uint8_t>& live) {
    const uint16_t kA = 11, kObtained = 13, kC = 17;
    ComboTrackerGameSummary summary;
    ComboTrackerForeignRow foreignRow;

    // ---- A. LIVE -------------------------------------------------------------------
    Context_SetCurrentGame(GAME_MM);
    sCtvLive = live.data();
    CVarSetInteger(kCtvAvailableKey, 1);
    MM_TrackerAdapter_TestLiveSeam(1);
    const uint16_t inLogicA[1] = { kA };
    (void)MM_TrackerAdapter_TestInLogicSeam(1, inLogicA, 1);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    const int a = CtvAvail((uint8_t)GAME_MM, kA);
    const int c = CtvAvail((uint8_t)GAME_MM, kC);
    const int obtained = CtvAvail((uint8_t)GAME_MM, kObtained);
    const int calls = MM_TrackerAdapter_TestInLogicSeam(1, inLogicA, 1);
    printf("[TEST] combo-tracker-view #1129 MM A (LIVE, in logic {11}): freshness=%u known=%d available=%d; rows 11=%d "
           "17=%d 13=%d; computations %d\n",
           (unsigned)summary.freshness, summary.availabilityKnown ? 1 : 0, summary.available, a, c, obtained, calls);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE);
    CTV_ASSERT(summary.availabilityKnown && summary.available == 1);
    CTV_ASSERT(a == 1 && c == 0 && obtained == 0);
    CTV_ASSERT(calls == 1); // one computation for the summary and the three rows
    // The rows follow the set: dropped by the seam, recomputed with 17 in logic.
    const uint16_t inLogicC[1] = { kC };
    (void)MM_TrackerAdapter_TestInLogicSeam(1, inLogicC, 1);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_MM, kA) == 0 && CtvAvail((uint8_t)GAME_MM, kC) == 1);
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.hostCheckId == kC && foreignRow.availabilityKnown && foreignRow.available);
    // Enable Available Checks off: not known, nothing computed.
    (void)MM_TrackerAdapter_TestInLogicSeam(1, inLogicC, 1);
    CVarClear(kCtvAvailableKey);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && !summary.availabilityKnown && summary.available == 0);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_MM, kC) == -1);
    CTV_ASSERT(MM_TrackerAdapter_TestInLogicSeam(1, inLogicC, 1) == 0);
    // No MM play state (the seam's 0): not known, nothing computed.
    CVarSetInteger(kCtvAvailableKey, 1);
    MM_TrackerAdapter_TestLiveSeam(0);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_MM, kC) == -1);
    CTV_ASSERT(MM_TrackerAdapter_TestInLogicSeam(1, inLogicC, 1) == 0);

    // ---- B. STALE: the departure capture --------------------------------------------
    MM_TrackerAdapter_TestLiveSeam(1);
    const uint16_t inLogicAC[2] = { kA, kC };
    (void)MM_TrackerAdapter_TestInLogicSeam(1, inLogicAC, 2);
    MM_TrackerAdapter_CaptureAvailability();
    const int captureCalls = MM_TrackerAdapter_TestInLogicSeam(1, nullptr, 0); // the live set is empty from here
    // The freeze copies the live save into the shadow; OoT is played next.
    Context_UpdateShadowCopy(GAME_MM, live.data(), live.size());
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    const int sa = CtvAvail((uint8_t)GAME_MM, kA);
    const int sc = CtvAvail((uint8_t)GAME_MM, kC);
    const int so = CtvAvail((uint8_t)GAME_MM, kObtained);
    const int staleCalls = MM_TrackerAdapter_TestInLogicSeam(1, nullptr, 0);
    printf("[TEST] combo-tracker-view #1129 MM B (STALE after a capture of {11, 17}): freshness=%u known=%d "
           "available=%d; rows 11=%d 17=%d 13=%d; computations at the capture %d, while stale %d\n",
           (unsigned)summary.freshness, summary.availabilityKnown ? 1 : 0, summary.available, sa, sc, so, captureCalls,
           staleCalls);
    CTV_ASSERT(captureCalls == 1); // the capture computes afresh
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.availabilityKnown && summary.available == 2);
    CTV_ASSERT(sa == 1 && sc == 1 && so == 0);
    CTV_ASSERT(staleCalls == 0); // the stale pane never crawls
    // No file played at the departure (the seam's 0): the capture stands.
    Context_SetCurrentGame(GAME_MM);
    MM_TrackerAdapter_TestLiveSeam(0);
    MM_TrackerAdapter_CaptureAvailability();
    Context_SetCurrentGame(GAME_OOT);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_MM, kA) == 1);
    // A departure with Enable Available Checks off clears it.
    Context_SetCurrentGame(GAME_MM);
    MM_TrackerAdapter_TestLiveSeam(1);
    CVarClear(kCtvAvailableKey);
    MM_TrackerAdapter_CaptureAvailability();
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(!summary.availabilityKnown && CtvAvail((uint8_t)GAME_MM, kA) == -1);
    // A context with no capture at all: a .redsave written before #1129.
    CVarSetInteger(kCtvAvailableKey, 1);
    Context_SetCurrentGame(GAME_MM);
    MM_TrackerAdapter_CaptureAvailability();
    Context_SetCurrentGame(GAME_OOT);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_MM, kA) == 0); // captured from the empty live set
    ComboContext_Init();
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE && !summary.availabilityKnown);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_MM, kA) == -1 && CtvAvail((uint8_t)GAME_MM, kC) == -1);

    // The record itself: an id past the capture is not known, and a capture past
    // the record's capacity is refused (cleared), never truncated.
    const uint8_t bits[2] = { 0x02, 0x00 }; // id 1
    Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, bits, 9);
    CTV_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, 1) == 1);
    CTV_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, 2) == 0);
    CTV_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, 9) == -1);
    CTV_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_OOT, 1) == -1);
    std::vector<uint8_t> huge(8192, 0xFF);
    Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, huge.data(), 65535);
    CTV_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, 1) == -1);
    return TEST_PASS;
}

static int CtvAvailMM(const ComboMMTrackerDesc* real, std::vector<uint8_t>& shadowWorld) {
    const ComboMMTrackerDesc production = *real; // `real` points into the registry, which the legs re-register
    ComboMMTrackerDesc testDesc = production;
    testDesc.liveSave = CtvLiveSave;
    Combo_Tracker_RegisterMM(&testDesc);
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    const GameId prevGame = Context_GetCurrentGame();
    const bool hadKey = CVarExists(kCtvAvailableKey); // put back as found: never lose or leave a player value
    const int32_t keyValue = CVarGetInteger(kCtvAvailableKey, 0);

    int result = TEST_FAIL;
    if (desc != NULL && desc->liveAvailable != NULL) { // the MM TU installs the op
        std::vector<uint8_t> live((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        const uint32_t kSeed = 0x11290001u;
        memcpy(live.data() + desc->newfOffset, desc->newf, desc->newfLen);
        memcpy(live.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
        memcpy(live.data() + desc->finalSeedOffset, &kSeed, sizeof(uint32_t));
        for (uint16_t check : { (uint16_t)11, (uint16_t)13, (uint16_t)17 }) {
            live[desc->checkTableOffset + (size_t)check * desc->checkStride + desc->shuffledOffset] = 1;
        }
        CtvSetObtained(live, desc, 13, true);
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = kSeed;
        gComboCtx.sharedRandoSettingsHash = 0x5EED1129u;
        SharedItem ootItem;
        if (TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootItem) &&
            Combo_SetForeignPlacement(17, ootItem) >= 0) {
            result = CtvAvailMMBody(live);
        }
    } else {
        printf("[TEST] FAIL: combo-tracker-view #1129: the MM descriptor carries no liveAvailable op\n");
    }

    // Production again, whatever the legs did.
    if (hadKey) {
        CVarSetInteger(kCtvAvailableKey, keyValue);
    } else {
        CVarClear(kCtvAvailableKey);
    }
    MM_TrackerAdapter_TestLiveSeam(-1);
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    sCtvLive = NULL;
    Combo_Tracker_RegisterMM(&production);
    ComboContext_Init();
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    Context_SetCurrentGame(prevGame);
    return result;
}

/**
 * OoT's half (#1129), over the seam's authored heap world (ootIds: saved,
 * untouched, skipped; all three listed through the list seam).
 *
 *   A. LIVE: the rows' bits are the heap's IsAvailable and the summary counts
 *      the listed ones, while OoT plays a randomizer file and SoH's set is
 *      current; not known when the set is not current (Enable Available Checks
 *      off, or no recalculation yet) or no randomizer file is loaded.
 *   B. STALE: MM played, the pane shows the departure capture
 *      (OoT_TrackerAdapter_CaptureAvailability), not the heap's later bits; a
 *      departure with no randomizer file played leaves it, one with the set not
 *      current clears it; no capture reads not known.
 */
static int CtvAvailOoTBody(const uint16_t ootIds[3]) {
    ComboTrackerGameSummary summary;
    const uint16_t saved = ootIds[0], untouched = ootIds[1], skipped = ootIds[2];
    (void)OoT_TrackerAdapter_TestListSeam(1, ootIds, 3);
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    OoT_TrackerAdapter_TestAvailableSeam(1);
    CTV_ASSERT(OoT_TrackerAdapter_TestSetAvailable(saved, 0) >= 0);
    CTV_ASSERT(OoT_TrackerAdapter_TestSetAvailable(untouched, 1) >= 0);
    CTV_ASSERT(OoT_TrackerAdapter_TestSetAvailable(skipped, 1) >= 0);

    // ---- A. LIVE -------------------------------------------------------------------
    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    printf("[TEST] combo-tracker-view #1129 OoT A (LIVE): known=%d available=%d; rows saved=%d untouched=%d "
           "skipped=%d\n",
           summary.availabilityKnown ? 1 : 0, summary.available, CtvAvail((uint8_t)GAME_OOT, saved),
           CtvAvail((uint8_t)GAME_OOT, untouched), CtvAvail((uint8_t)GAME_OOT, skipped));
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE && summary.availabilityKnown && summary.available == 2);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, saved) == 0);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, untouched) == 1 && CtvAvail((uint8_t)GAME_OOT, skipped) == 1);
    // The bit follows the heap.
    CTV_ASSERT(OoT_TrackerAdapter_TestSetAvailable(skipped, 0) == 1);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.available == 1 && CtvAvail((uint8_t)GAME_OOT, skipped) == 0);
    // SoH's set not current: not known.
    OoT_TrackerAdapter_TestAvailableSeam(0);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(!summary.availabilityKnown && summary.available == 0 && CtvAvail((uint8_t)GAME_OOT, untouched) == -1);
    // No randomizer file loaded (the list's live gate): not known.
    OoT_TrackerAdapter_TestAvailableSeam(1);
    (void)OoT_TrackerAdapter_TestSkipSeam(0);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(!summary.availabilityKnown && CtvAvail((uint8_t)GAME_OOT, untouched) == -1);
    (void)OoT_TrackerAdapter_TestSkipSeam(1);

    // ---- B. STALE: the departure capture --------------------------------------------
    OoT_TrackerAdapter_CaptureAvailability(); // untouched available, the other two not
    Context_SetCurrentGame(GAME_MM);
    CTV_ASSERT(OoT_TrackerAdapter_TestSetAvailable(skipped, 1) == 0); // the heap moves on; the pane must not
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    printf("[TEST] combo-tracker-view #1129 OoT B (STALE after a capture): freshness=%u known=%d available=%d; rows "
           "untouched=%d skipped=%d\n",
           (unsigned)summary.freshness, summary.availabilityKnown ? 1 : 0, summary.available,
           CtvAvail((uint8_t)GAME_OOT, untouched), CtvAvail((uint8_t)GAME_OOT, skipped));
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE && summary.availabilityKnown && summary.available == 1);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, untouched) == 1 && CtvAvail((uint8_t)GAME_OOT, skipped) == 0);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, saved) == 0);
    // A departure with no randomizer file played leaves the capture...
    Context_SetCurrentGame(GAME_OOT);
    (void)OoT_TrackerAdapter_TestSkipSeam(0);
    OoT_TrackerAdapter_CaptureAvailability();
    Context_SetCurrentGame(GAME_MM);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, untouched) == 1);
    // ...one with SoH's set not current clears it.
    Context_SetCurrentGame(GAME_OOT);
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    OoT_TrackerAdapter_TestAvailableSeam(0);
    OoT_TrackerAdapter_CaptureAvailability();
    Context_SetCurrentGame(GAME_MM);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(!summary.availabilityKnown && CtvAvail((uint8_t)GAME_OOT, untouched) == -1);
    // No capture at all (a .redsave written before #1129).
    Context_SetCurrentGame(GAME_OOT);
    OoT_TrackerAdapter_TestAvailableSeam(1);
    OoT_TrackerAdapter_CaptureAvailability();
    Context_SetCurrentGame(GAME_MM);
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, untouched) == 1);
    ComboContext_Init();
    CTV_ASSERT(CtvAvail((uint8_t)GAME_OOT, untouched) == -1);
    return TEST_PASS;
}

static int CtvAvailOoT(const uint16_t ootIds[3]) {
    const GameId prevGame = Context_GetCurrentGame();
    const int result = CtvAvailOoTBody(ootIds);
    (void)OoT_TrackerAdapter_TestListSeam(-1, NULL, 0);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    OoT_TrackerAdapter_TestAvailableSeam(-1);
    for (int i = 0; i < 3; i++) {
        (void)OoT_TrackerAdapter_TestSetAvailable(ootIds[i], 0);
    }
    ComboContext_Init();
    Context_SetCurrentGame(prevGame);
    return result;
}

// ---- #1131: the current area (header point 14) -----------------------------

extern "C" int OoT_TrackerAdapter_TestSetCurrentArea(int area);
extern "C" void MM_TrackerAdapter_TestSceneSeam(int mode, int sceneId);
extern "C" int MM_TrackerAdapter_TestCheckScene(uint16_t checkId);
extern "C" int MM_TrackerAdapter_TestGrottoScene(void);

static int sCtvNavFailures = 0;

/** `game`'s current area as one value: the key, or -1 for none. */
static int CtvArea(uint8_t game) {
    uint16_t key = 0xFFFF;
    return Combo_TrackerCurrentArea(game, &key) ? (int)key : -1;
}

/**
 * OoT over the authored world (ootIds: three Kokiri Forest chests): SoH's
 * currentArea is the area, as the area key the rows carry, while OoT plays a
 * randomizer file; while MM is played OoT answers the area kept from its last
 * live answer, whatever SoH's variable holds.
 */
static int CtvNavOoTBody(const uint16_t ootIds[3]) {
    ComboTrackerCheckRow row;
    Context_SetCurrentGame(GAME_OOT);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row) && row.areaName != NULL);
    const int kokiriForest = (int)row.areaKey;
    (void)OoT_TrackerAdapter_TestSkipSeam(1); // a randomizer file is loaded
    (void)OoT_TrackerAdapter_TestSetCurrentArea(-1);
    CTV_ASSERT(CtvArea((uint8_t)GAME_OOT) == -1); // no transition yet: none
    (void)OoT_TrackerAdapter_TestSetCurrentArea(kokiriForest);
    printf("[TEST] combo-tracker-view #1131 OoT: currentArea %d (\"%s\") answers %d\n", kokiriForest, row.areaName,
           CtvArea((uint8_t)GAME_OOT));
    CTV_ASSERT(CtvArea((uint8_t)GAME_OOT) == kokiriForest);
    CTV_ASSERT(Combo_TrackerCurrentArea((uint8_t)GAME_OOT, NULL)); // a NULL key is allowed
    (void)OoT_TrackerAdapter_TestSkipSeam(0);                      // no randomizer file: none
    CTV_ASSERT(CtvArea((uint8_t)GAME_OOT) == -1);
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    CTV_ASSERT(CtvArea((uint8_t)GAME_OOT) == kokiriForest);
    // MM is played: OoT's pane keeps the area of its last frame of play.
    Context_SetCurrentGame(GAME_MM);
    (void)OoT_TrackerAdapter_TestSetCurrentArea(kokiriForest + 1);
    CTV_ASSERT(CtvArea((uint8_t)GAME_OOT) == kokiriForest);
    // OoT again: the live answer.
    Context_SetCurrentGame(GAME_OOT);
    CTV_ASSERT(CtvArea((uint8_t)GAME_OOT) == kokiriForest + 1);
    // No game, or a game out of range: none.
    CTV_ASSERT(CtvArea((uint8_t)GAME_NONE) == -1);
    return TEST_PASS;
}

static int CtvNavOoT(const uint16_t ootIds[3]) {
    const GameId prevGame = Context_GetCurrentGame();
    const int prevArea = OoT_TrackerAdapter_TestSetCurrentArea(-1);
    const int result = CtvNavOoTBody(ootIds);
    (void)OoT_TrackerAdapter_TestSetCurrentArea(prevArea);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    Context_SetCurrentGame(prevGame);
    return result;
}

/**
 * MM through the real descriptor: the area of the scene MM is played in is the
 * area key MM's rows carry for a check of that scene; a grotto keeps the scene
 * it was entered from; with no play state there is none; while OoT is played MM
 * answers the area kept from its last live answer.
 */
static int CtvNavMMBody(const ComboMMTrackerDesc* desc) {
    CTV_ASSERT(desc->areaName != NULL);
    const int grotto = MM_TrackerAdapter_TestGrottoScene();
    // Two checks of two scenes, each listed under its own scene (a grotto's or
    // an enemy drop's row may be listed elsewhere, so they are skipped): the
    // row key's low byte is its scene (MMTrackerAreaName).
    int sceneA = -1;
    int sceneB = -1;
    int keyA = -1;
    int keyB = -1;
    for (uint32_t id = 1; id < desc->checkCount && sceneB < 0; id++) {
        const int scene = MM_TrackerAdapter_TestCheckScene((uint16_t)id);
        uint16_t key = 0;
        if (scene < 0 || scene == grotto || desc->areaName((uint16_t)id, &key) == NULL ||
            (key & 0xFFu) != ((unsigned)scene & 0xFFu)) {
            continue;
        }
        if (sceneA < 0) {
            sceneA = scene;
            keyA = (int)key;
        } else if (scene != sceneA) {
            sceneB = scene;
            keyB = (int)key;
        }
    }
    printf("[TEST] combo-tracker-view #1131 MM: scene %d is area %d, scene %d is area %d\n", sceneA, keyA, sceneB,
           keyB);
    CTV_ASSERT(sceneA >= 0 && sceneB >= 0 && keyA != keyB);

    Context_SetCurrentGame(GAME_MM);
    MM_TrackerAdapter_TestSceneSeam(0, 0); // no MM play state
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == -1);
    MM_TrackerAdapter_TestSceneSeam(1, sceneA);
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == keyA);
    MM_TrackerAdapter_TestSceneSeam(1, grotto); // a grotto entered from scene A: still area A
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == keyA);
    MM_TrackerAdapter_TestSceneSeam(1, sceneB);
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == keyB);
    // OoT is played: MM's pane keeps the area of its last frame of play.
    Context_SetCurrentGame(GAME_OOT);
    MM_TrackerAdapter_TestSceneSeam(1, sceneA);
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == keyB);
    Context_SetCurrentGame(GAME_MM);
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == keyA);
    // A grotto with no scene before it (a file loaded inside one): none.
    MM_TrackerAdapter_TestSceneSeam(0, 0);
    MM_TrackerAdapter_TestSceneSeam(1, grotto);
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == -1);
    // The production op with no play state in this tier: none.
    MM_TrackerAdapter_TestSceneSeam(-1, 0);
    CTV_ASSERT(desc->currentArea != NULL);
    CTV_ASSERT(CtvArea((uint8_t)GAME_MM) == -1);
    return TEST_PASS;
}

static int CtvNavMM(const ComboMMTrackerDesc* desc) {
    const GameId prevGame = Context_GetCurrentGame();
    const int result = CtvNavMMBody(desc);
    MM_TrackerAdapter_TestSceneSeam(-1, 0);
    Context_SetCurrentGame(prevGame);
    return result;
}

static int CtvLiveLegs(const ComboMMTrackerDesc* desc, std::vector<uint8_t>& shadowWorld) {
    const ComboMMTrackerDesc real = *desc; // backup: the production descriptor
    ComboMMTrackerDesc testDesc = real;
    testDesc.liveSave = CtvLiveSave;
    testDesc.setLiveSkipped = CtvSetLiveSkipped;
    Combo_Tracker_RegisterMM(&testDesc);
    const GameId prevGame = Context_GetCurrentGame();

    const int result = CtvLiveLegsBody(Combo_Tracker_GetMMDesc(), shadowWorld);
    // #458 U5's MM half rides the same test descriptor. Recorded rather than
    // returned on, so the OoT half (5c) still runs and reports.
    if (result == TEST_PASS && CtvSkipLegsMM(Combo_Tracker_GetMMDesc(), shadowWorld) != TEST_PASS) {
        sCtvU5Failures++;
    }

    // Always put back what the later sections expect: the production
    // descriptor, an unpaired context, the authored shadow world.
    sCtvLive = NULL;
    Combo_Tracker_RegisterMM(&real);
    ComboContext_Init();
    Context_UpdateShadowCopy(GAME_MM, shadowWorld.data(), shadowWorld.size());
    Context_SetCurrentGame(prevGame);
    return result;
}

extern "C" int Combo_TrackerView_RunHeadless(void) {
    printf("[TEST] combo-tracker-view: per-game adapters recover authored shadow/heap worlds, staleness-labelled "
           "(#458)\n");

    const GameId prevGame = Context_GetCurrentGame();
    ComboContext_Init();
    sCtvU5Failures = 0;

    // ---- 1. Unregistered: every read is inert -----------------------------
    Combo_Tracker_RegisterMM(NULL);
    Combo_Tracker_RegisterOoT(NULL);

    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CTV_ASSERT(!summary.hasWorld && summary.shuffled == 0 && summary.totalChecks == 0);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_MM) == 0);
    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_OOT) == 0);
    ComboTrackerCheckRow row;
    CTV_ASSERT(!Combo_TrackerCheckAt((uint8_t)GAME_MM, 0, &row));
    CTV_ASSERT(!Combo_TrackerCheckAt((uint8_t)GAME_OOT, 0, &row));
    CTV_ASSERT(Combo_TrackerCheckName((uint8_t)GAME_MM, 0) == NULL);
    CTV_ASSERT(Combo_TrackerCheckName((uint8_t)GAME_OOT, 0) == NULL);
    // NULL-out and bad-game arguments are ignored, not dereferenced.
    Combo_TrackerGameSummary((uint8_t)GAME_NONE, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    Combo_TrackerGameSummary((uint8_t)GAME_MM, NULL);
    Combo_TrackerIdentity(NULL);

    // ---- 2 + 3. MM adapter over an authored shadow blob -------------------
    MM_TrackerAdapter_Register();
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    // If this fires, the MM registration was dropped (the #516 dead-registrar
    // class) or the descriptor failed the view's geometry validation.
    CTV_ASSERT(desc != NULL);
    CTV_ASSERT(desc->checkCount > 100);   // a real RC_MAX-sized table, not a stub
    CTV_ASSERT(desc->checkStride >= 8);   // RandoSaveCheck carries an item id + flags + price
    CTV_ASSERT(desc->checkName != NULL);  // names resolve MM-side
    CTV_ASSERT(desc->saveTypeRando != 0); // SAVETYPE_RANDO is nonzero (vanilla is 0)

    // Snapshot whatever is resident before authoring over it. Under
    // `--test all` an earlier row (the roundtrip/switch tests) may have left a
    // real frozen MM image in the shadow, and handing the next row a zeroed
    // one instead of what it inherited would make this lock a hidden input to
    // tests it has nothing to do with.
    std::vector<uint8_t> shadowBackup((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    const uint8_t* residentShadow = (const uint8_t*)Context_GetMMSaveContext();
    if (residentShadow != NULL) {
        memcpy(shadowBackup.data(), residentShadow, shadowBackup.size());
    }
    // #1294's legs arm and consume the MM half: put its frozen flag back too.
    const bool mmWasFrozen = Context_HasFrozenState(GAME_MM) != 0;
    const uint16_t mmReturnEntrance = Context_GetFrozenReturnEntrance(GAME_MM);

    // All-zero shadow first: must read as "no data", never as a vanilla save.
    std::vector<uint8_t> blob((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_MM) == 0);

    // Author a world at the REGISTERED offsets: rando save, known seed, three
    // shuffled checks (ids 3, 5, 7), of which 5 is obtained and 7 skipped.
    const uint32_t kMMSeed = 0x5EEDF00Du;
    const uint16_t kShuffledA = 3, kObtainedB = 5, kSkippedC = 7;
    memcpy(blob.data() + desc->newfOffset, desc->newf, desc->newfLen);
    memcpy(blob.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
    memcpy(blob.data() + desc->finalSeedOffset, &kMMSeed, sizeof(uint32_t));
    const uint16_t authored[3] = { kShuffledA, kObtainedB, kSkippedC };
    for (int i = 0; i < 3; i++) {
        uint8_t* checkRow = blob.data() + desc->checkTableOffset + (size_t)authored[i] * desc->checkStride;
        checkRow[desc->shuffledOffset] = 1;
    }
    blob[desc->checkTableOffset + (size_t)kObtainedB * desc->checkStride + desc->obtainedOffset] = 1;
    blob[desc->checkTableOffset + (size_t)kSkippedC * desc->checkStride + desc->skippedOffset] = 1;
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());

    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    // The shadow is never LIVE — not even while MM is the active game, when
    // the real adapter's liveSave answers NULL because no MM play state is
    // loaded in this tier (the title/file-select case; #799's "liveSave NULL +
    // marked shadow -> STALE" leg).
    Context_SetCurrentGame(GAME_MM);
    CTV_ASSERT(desc->liveSave != NULL);   // the MM TU installs the live source
    CTV_ASSERT(desc->liveSave() == NULL); // ...and withholds it with no play state
    // #458 U5: the MM TU installs its own skip write, and with no MM play state
    // it refuses: nothing reaches MM's live save, and the shadow is untouched.
    CTV_ASSERT(desc->setLiveSkipped != NULL);
    CTV_ASSERT(!desc->setLiveSkipped(kShuffledA, true));
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kShuffledA, true));
    CTV_ASSERT(CtvSkipByte((const uint8_t*)Context_GetMMSaveContext(), desc, kShuffledA) == 0);
    ComboTrackerGameSummary summaryWhileMMActive;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summaryWhileMMActive);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summaryWhileMMActive.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.hasWorld);
    CTV_ASSERT(summary.seed == kMMSeed);
    CTV_ASSERT(summary.totalChecks == (int)desc->checkCount);
    CTV_ASSERT(summary.shuffled == 3);
    CTV_ASSERT(summary.obtained == 1);
    CTV_ASSERT(summary.skipped == 1);

    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_MM) == (int)desc->checkCount);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kObtainedB, &row));
    CTV_ASSERT(row.checkId == kObtainedB && row.shuffled && row.obtained && !row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kSkippedC, &row));
    CTV_ASSERT(row.shuffled && !row.obtained && row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)kShuffledA + 1, &row));
    CTV_ASSERT(!row.shuffled);                                                        // id 4 was never authored
    CTV_ASSERT(!Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)desc->checkCount, &row)); // out of range

    // ---- 2b. #458 U4: status, area and placed item on MM rows ----------------
    // Recorded rather than returned on, so the OoT half (5b) still runs and
    // reports: the row fails at the end either way.
    int u4Failures = 0;
    if (CtvMMRowsU4(desc, blob, kShuffledA, kObtainedB, kSkippedC) != TEST_PASS) {
        u4Failures++;
    }
    if (CtvMMSeenRows(desc, blob) != TEST_PASS) {
        u4Failures++;
    }

    // ---- 4. MM check names resolve (the #489 class) -----------------------
    const char* mmName = Combo_TrackerCheckName((uint8_t)GAME_MM, kShuffledA);
    CTV_ASSERT(mmName != NULL && mmName[0] != '\0');

    // ---- 4b. MM reads live while MM is played (#799) ----------------------
    if (CtvLiveLegs(desc, blob) != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- 4c. #1129: MM's reachability, live and captured -------------------
    // Recorded rather than returned on, so the OoT half (5d) still runs.
    sCtvAvailFailures = 0;
    if (CtvAvailMM(desc, blob) != TEST_PASS) {
        sCtvAvailFailures++;
    }
    desc = Combo_Tracker_GetMMDesc(); // re-registered by the legs

    // ---- 4d. #1131: MM's current area ---------------------------------------
    // Recorded rather than returned on, so the OoT half (5e) still runs.
    sCtvNavFailures = 0;
    if (CtvNavMM(desc) != TEST_PASS) {
        sCtvNavFailures++;
    }

    // ---- 5. OoT adapter: never-booted, authored, suspend labelling --------
    OoT_TrackerAdapter_Register();
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    // No heap Rando::Context exists in this tier until the seam below runs.
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CTV_ASSERT(Combo_TrackerCheckCount((uint8_t)GAME_OOT) == 0);

    const uint32_t kOoTSeed = 0x0A11CE00u;
    uint16_t ootIds[3] = { 0, 0, 0 };
    CTV_ASSERT(OoT_TrackerAdapter_TestAuthorWorld(kOoTSeed, ootIds) == 3);

    Context_SetCurrentGame(GAME_OOT);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_LIVE);
    CTV_ASSERT(summary.hasWorld);
    CTV_ASSERT(summary.seed == kOoTSeed);
    CTV_ASSERT(summary.shuffled == 3);
    CTV_ASSERT(summary.obtained == 1);
    CTV_ASSERT(summary.skipped == 1);

    // The headline: with MM active, OoT's suspended heap stays readable and
    // is labelled stale rather than live.
    Context_SetCurrentGame(GAME_MM);
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.shuffled == 3 && summary.obtained == 1 && summary.skipped == 1);

    // Row content at the flat ids the seam returned (collected / untouched /
    // skipped, in that order). The seam fills OoT's static tables when this tier
    // never did (#458 U4), so the names resolve; 5b asserts what they say.
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row));
    CTV_ASSERT(row.shuffled && row.obtained && !row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    CTV_ASSERT(row.shuffled && !row.obtained && !row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[2], &row));
    CTV_ASSERT(row.shuffled && !row.obtained && row.skipped);
    (void)Combo_TrackerCheckName((uint8_t)GAME_OOT, ootIds[0]);

    // ---- 5a. #1126: SoH's Check Tracker's list, once its settings are loaded ---
    const int ootList = CtvOoTListLegs(ootIds);
    (void)OoT_TrackerAdapter_TestListSeam(-1, NULL, 0); // production again, whatever the legs did
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    Context_SetCurrentGame(GAME_MM);
    if (ootList != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- 5b. #458 U4: status, area and placed item on OoT rows --------------
    if (CtvOoTRowsU4(ootIds) != TEST_PASS) {
        u4Failures++;
    }
    ComboContext_Init(); // unpaired again, as section 6 expects
    // #1166: seen rows name their item and price (header point 13).
    if (CtvOoTSeenRows() != TEST_PASS) {
        u4Failures++;
    }

    // ---- 5c. #458 U5: OoT's skip toggle, live panel only --------------------
    const int ootSkip = CtvSkipLegsOoT(ootIds);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1); // production again, whatever the legs did
    sCtvLive = NULL;
    MM_TrackerAdapter_Register(); // #1294's legs stand in for MM's play state through the descriptor
    if (ootSkip != TEST_PASS) {
        sCtvU5Failures++;
    }

    // ---- 5c'. #1294: both far skips through a .redsave commit and load -------
    if (CtvFarSkipRedsave(Combo_Tracker_GetMMDesc(), blob, kOoTSeed, ootIds) != TEST_PASS) {
        sCtvU5Failures++;
    }

    // ---- 5d. #1129: OoT's reachability, live and captured --------------------
    if (CtvAvailOoT(ootIds) != TEST_PASS) {
        sCtvAvailFailures++;
    }

    // ---- 5e. #1131: OoT's current area --------------------------------------
    if (CtvNavOoT(ootIds) != TEST_PASS) {
        sCtvNavFailures++;
    }

    // Releasing the world must return the adapter to UNAVAILABLE — liveness
    // is a per-call check on the weak singleton, not a latched flag.
    OoT_TrackerAdapter_TestReleaseWorld();
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);

    // ---- 6. Identity + foreign rows, both directions ----------------------
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);
    CTV_ASSERT(!identity.paired);
    CTV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0);
    CTV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_OOT) == 0);
    CTV_ASSERT(!Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, NULL));

    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;

    SharedItem ootItem;
    SharedItem mmItem;
    CTV_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootItem));
    CTV_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmItem));

    // One crossing per direction: MM check kObtainedB hosts an OoT item, and
    // an arbitrary OoT check hosts an MM item.
    CTV_ASSERT(Combo_SetForeignPlacement(kObtainedB, ootItem) >= 0);
    const uint16_t kOoTHostCheck = 0x0123;
    CTV_ASSERT(Combo_SetForeignPlacementOoT(kOoTHostCheck, mmItem) >= 0);

    Combo_TrackerIdentity(&identity);
    CTV_ASSERT(identity.paired);
    CTV_ASSERT(identity.sharedRandoSeed == 0xC0FFEE97u);
    CTV_ASSERT(identity.mmHostedForeign == 1 && identity.ootHostedForeign == 1);

    ComboTrackerForeignRow foreignRow;
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.hostGame == (uint8_t)GAME_MM);
    CTV_ASSERT(foreignRow.hostCheckId == kObtainedB);
    CTV_ASSERT(foreignRow.originGame == (uint8_t)GAME_OOT);
    CTV_ASSERT(strcmp(foreignRow.itemName, "Lens of Truth") == 0);
    CTV_ASSERT(foreignRow.itemArticle != NULL);
    // The MM adapter is registered, so the host check resolves to a name.
    CTV_ASSERT(foreignRow.hostCheckName != NULL && foreignRow.hostCheckName[0] != '\0');
    // kObtainedB is obtained in the authored MM shadow: the host game's save
    // says this crossing was found.
    CTV_ASSERT(foreignRow.found == COMBO_TRACKER_FOUND_YES);

    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 0, &foreignRow));
    CTV_ASSERT(foreignRow.hostGame == (uint8_t)GAME_OOT);
    CTV_ASSERT(foreignRow.hostCheckId == kOoTHostCheck);
    CTV_ASSERT(foreignRow.originGame == (uint8_t)GAME_MM);
    CTV_ASSERT(strcmp(foreignRow.itemName, "Lens of Truth") == 0);
    // The OoT world was released above: OoT has nothing to read, so the OoT
    // host's found state is UNKNOWN, never "not found".
    CTV_ASSERT(foreignRow.found == COMBO_TRACKER_FOUND_UNKNOWN);
    CTV_ASSERT(!Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 1, &foreignRow)); // only one crossing

    // Found follows the host check's obtained byte, not the shared-item array:
    // clear the byte and the row reads NO; a redeemed tagged entry for the same
    // item does not bring it back.
    blob[desc->checkTableOffset + (size_t)kObtainedB * desc->checkStride + desc->obtainedOffset] = 0;
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());
    gComboCtx.sharedItemsTagged[0].originGame = ootItem.originGame;
    gComboCtx.sharedItemsTagged[0].id = ootItem.id;
    gComboCtx.sharedItemsTagged[0].flags = RSBS_SHARED_ITEM_REDEEMED;
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.found == COMBO_TRACKER_FOUND_NO);
    ComboTrackerForeignProgress progress;
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &progress);
    CTV_ASSERT(progress.total == 1 && progress.found == 0 && progress.freshness == COMBO_TRACKER_FRESH_STALE);
    blob[desc->checkTableOffset + (size_t)kObtainedB * desc->checkStride + desc->obtainedOffset] = 1;
    Context_UpdateShadowCopy(GAME_MM, blob.data(), blob.size());
    Combo_TrackerForeignProgress((uint8_t)GAME_MM, &progress);
    CTV_ASSERT(progress.total == 1 && progress.found == 1);
    Combo_TrackerForeignProgress((uint8_t)GAME_OOT, &progress);
    CTV_ASSERT(progress.total == 1 && progress.found == 0 && progress.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);

    // Unpaired again: the rows must vanish ("not paired" != "no crossings").
    ComboContext_Init();
    CTV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0);
    CTV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_OOT) == 0);

    // ---- 7. No freshness caption (#927) -------------------------------------
    {
        const uint8_t games[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
        const uint8_t fresh[3] = { COMBO_TRACKER_FRESH_LIVE, COMBO_TRACKER_FRESH_STALE,
                                   COMBO_TRACKER_FRESH_UNAVAILABLE };
        for (uint8_t g : games) {
            for (uint8_t f : fresh) {
                const char* label = Combo_TrackerFreshnessLabel(g, f);
                CTV_ASSERT(label != NULL && label[0] == '\0');
            }
        }
    }

    // ---- Leave global state clean -----------------------------------------
    // Adapters stay registered (the production state); the shadow goes back to
    // exactly the bytes this row inherited.
    Context_ClearFrozenState(GAME_MM);
    Context_UpdateShadowCopy(GAME_MM, shadowBackup.data(), shadowBackup.size());
    if (mmWasFrozen) {
        (void)Context_ArmShadowAsFrozen(GAME_MM, mmReturnEntrance);
    }
    Context_SetCurrentGame(prevGame);

    if (u4Failures != 0) {
        printf("[TEST] FAIL: %d of the row sections (#458 U4: MM 2b, OoT 5b; #1166: OoT seen rows) failed\n",
               u4Failures);
        return TEST_FAIL;
    }
    if (sCtvAvailFailures != 0) {
        printf("[TEST] FAIL: %d of the two #1129 reachability halves (MM 4c, OoT 5d) failed\n", sCtvAvailFailures);
        return TEST_FAIL;
    }
    if (sCtvNavFailures != 0) {
        printf("[TEST] FAIL: %d of the two #1131 current-area halves (MM 4d, OoT 5e) failed\n", sCtvNavFailures);
        return TEST_FAIL;
    }
    if (sCtvU5Failures != 0) {
        printf("[TEST] FAIL: %d of the #458 U5 / #1294 skip-toggle sections (MM, OoT 5c, far round trip 5c') failed\n",
               sCtvU5Failures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: adapters recover authored MM shadow + OoT heap worlds, label staleness honestly, and "
           "answer unavailable states without dereferencing (#458)\n");
    return TEST_PASS;
}
