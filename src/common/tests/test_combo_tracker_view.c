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
 * 7. FRESHNESS LABELS ARE PLAYER WORDING (UI parity M8). The window prints the
 *    label as a gray note with a closing period, so every label, for both
 *    games and every freshness, starts upper case, carries no closing
 *    punctuation, and names no mechanism a player never sees ("freeze",
 *    "shadow", "suspend", "heap"): the stale wording is "As of the last game
 *    switch", with MM adding "or save".
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
 * 10. THE SKIP TOGGLE WRITES THE LIVE PANEL ONLY (#458 U5). MM: the skipped
 *    byte of the live save, never the shadow, and nothing at all while the panel
 *    is a snapshot. OoT: the heap flag, persisted once per change, only while OoT
 *    is played with a loaded save. Neither writes a found check or one outside
 *    the seed. See CtvSkipLegsMM and CtvSkipLegsOoT.
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
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The OoT-side authoring seam (games/oot/soh/Enhancements/randomizer/
// TrackerAdapterSingleExe.cpp): places three checks — collected, untouched,
// skipped, ids returned flat — and holds/releases the heap context.
extern "C" int OoT_TrackerAdapter_TestAuthorWorld(uint32_t seed, uint16_t outIds[3]);
extern "C" void OoT_TrackerAdapter_TestReleaseWorld(void);

#define CTV_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
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
 *      shadow, STALE, labelled "As of file creation"; a marked non-rando one
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
    const uint64_t kCreatedAt = 1759190400ull; // MM loaded it: OnSaveLoad stamped it
    memcpy(live.data() + desc->createdAtOffset, &kCreatedAt, sizeof(kCreatedAt));
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
    CTV_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, summary.freshness), "Updated live") == 0);
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
    // The armed half: marked, rando, never loaded by MM (creation stamp zero).
    std::vector<uint8_t> armed = shadowWorld;
    memset(armed.data() + desc->createdAtOffset, 0, sizeof(uint64_t));
    Context_UpdateShadowCopy(GAME_MM, armed.data(), armed.size());
    // D1. What the code authors: Sram_InitNewSave's bootstrap carries an empty
    // newf and SAVETYPE_VANILLA, so the live read is refused and the armed
    // shadow is shown, labelled as the file-creation snapshot it is.
    std::vector<uint8_t> bootstrap((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    sCtvLive = bootstrap.data();
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    CTV_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE);
    CTV_ASSERT(summary.hasWorld && summary.seed == 0x5EEDF00Du && summary.shuffled == 3);
    CTV_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, summary.freshness), "As of file creation") == 0);
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
    CTV_ASSERT(MM_CheckTracker_TestItemName(Context_GetMMSaveContext(), obtainedB, native, (int)sizeof(native),
                                            &stored) == 1);
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

// ---- #458 U5: the skip toggle, live panel only --------------------------------

// Failed U5 halves (MM, OoT), reported together at the end of the row.
static int sCtvU5Failures = 0;

static uint8_t CtvSkipByte(const uint8_t* save, const ComboMMTrackerDesc* desc, uint16_t check) {
    return save[desc->checkTableOffset + (size_t)check * desc->checkStride + desc->skippedOffset];
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
 *   B. STALE (no MM play state: the shadow): nothing is skippable and nothing is
 *      written: the snapshot panel never writes.
 *   C. The same live buffer while OoT is the active game: MM's panel is not
 *      live, so refused, and the live buffer is untouched.
 */
static int CtvSkipLegsMM(const ComboMMTrackerDesc* desc, std::vector<uint8_t>& shadowWorld) {
    const uint16_t kOpen = 21, kFound = 23, kOutside = 25, kShadowOpen = 3;
    std::vector<uint8_t> live((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(live.data() + desc->newfOffset, desc->newf, desc->newfLen);
    memcpy(live.data() + desc->saveTypeOffset, &desc->saveTypeRando, sizeof(uint32_t));
    const uint64_t kCreatedAt = 1759190400ull;
    memcpy(live.data() + desc->createdAtOffset, &kCreatedAt, sizeof(kCreatedAt));
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

    // ---- C. MM's live buffer while OoT is played: not MM's live panel -----------
    sCtvLive = live.data();
    Context_SetCurrentGame(GAME_OOT);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_MM));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_MM, kOpen, true));
    CTV_ASSERT(CtvSkipByte(live.data(), desc, kOpen) == 0);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_NONE));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_NONE, kOpen, true));
    sCtvLive = NULL;
    return TEST_PASS;
}

// The OoT-side skip seam (TrackerAdapterSingleExe.cpp): -1 production, 0/1 the
// answer to "is a save loaded" with persists counted; returns the count since the
// previous call.
extern "C" int OoT_TrackerAdapter_TestSkipSeam(int saveLoaded);

/**
 * OoT's half of the toggle (#458 U5), over the seam's authored heap world
 * (ootIds: saved, untouched, skipped).
 *
 *   A. MM played: OoT's panel is the suspended heap. Refused, heap unchanged,
 *      even when the seam says a save is loaded.
 *   B. OoT played with no loaded save (the production predicate: this tier has
 *      no play state): the panel is LIVE but refused, as SoH's tracker shows no
 *      list before a file loads.
 *   C. OoT played with a loaded save: the untouched check is skipped on the heap,
 *      the row and summary follow, and the tracker-data section is persisted
 *      once per change (not for a no-op); the skipped check unskips. The saved
 *      check and a check outside the seed are refused with nothing persisted.
 */
static int CtvSkipLegsOoT(const uint16_t ootIds[3]) {
    ComboTrackerCheckRow row;
    ComboTrackerGameSummary summary;
    const uint16_t kOutside = (uint16_t)(ootIds[2] + 1); // not placed by the seam

    // ---- A. MM played ---------------------------------------------------------
    Context_SetCurrentGame(GAME_MM);
    (void)OoT_TrackerAdapter_TestSkipSeam(1);
    CTV_ASSERT(!Combo_TrackerSkipWritable((uint8_t)GAME_OOT));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    CTV_ASSERT(!Combo_TrackerRowSkippable((uint8_t)GAME_OOT, &row));
    CTV_ASSERT(!Combo_TrackerSetSkipped((uint8_t)GAME_OOT, ootIds[1], true));
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row) && !row.skipped);
    CTV_ASSERT(OoT_TrackerAdapter_TestSkipSeam(-1) == 0);

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
    printf("[TEST] combo-tracker-view U5 OoT live skip: returned=%d heapSkipped=%d status=%u persists=%d\n",
           (int)wrote, (int)row.skipped, (unsigned)row.status, persists);
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
    return TEST_PASS;
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
    CTV_ASSERT(desc->checkCount > 100);    // a real RC_MAX-sized table, not a stub
    CTV_ASSERT(desc->checkStride >= 8);    // RandoSaveCheck carries an item id + flags + price
    CTV_ASSERT(desc->checkName != NULL);   // names resolve MM-side
    CTV_ASSERT(desc->saveTypeRando != 0);  // SAVETYPE_RANDO is nonzero (vanilla is 0)

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
    CTV_ASSERT(!row.shuffled); // id 4 was never authored
    CTV_ASSERT(!Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)desc->checkCount, &row)); // out of range

    // ---- 2b. #458 U4: status, area and placed item on MM rows ----------------
    // Recorded rather than returned on, so the OoT half (5b) still runs and
    // reports: the row fails at the end either way.
    int u4Failures = 0;
    if (CtvMMRowsU4(desc, blob, kShuffledA, kObtainedB, kSkippedC) != TEST_PASS) {
        u4Failures++;
    }

    // ---- 4. MM check names resolve (the #489 class) -----------------------
    const char* mmName = Combo_TrackerCheckName((uint8_t)GAME_MM, kShuffledA);
    CTV_ASSERT(mmName != NULL && mmName[0] != '\0');

    // ---- 4b. MM reads live while MM is played (#799) ----------------------
    if (CtvLiveLegs(desc, blob) != TEST_PASS) {
        return TEST_FAIL;
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

    // ---- 5b. #458 U4: status, area and placed item on OoT rows --------------
    if (CtvOoTRowsU4(ootIds) != TEST_PASS) {
        u4Failures++;
    }
    ComboContext_Init(); // unpaired again, as section 6 expects

    // ---- 5c. #458 U5: OoT's skip toggle, live panel only --------------------
    const int ootSkip = CtvSkipLegsOoT(ootIds);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1); // production again, whatever the legs did
    if (ootSkip != TEST_PASS) {
        sCtvU5Failures++;
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

    // ---- 7. Freshness labels are player wording ---------------------------
    {
        const uint8_t games[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
        const uint8_t fresh[3] = { COMBO_TRACKER_FRESH_LIVE, COMBO_TRACKER_FRESH_STALE,
                                   COMBO_TRACKER_FRESH_UNAVAILABLE };
        for (uint8_t g : games) {
            for (uint8_t f : fresh) {
                const char* label = Combo_TrackerFreshnessLabel(g, f);
                CTV_ASSERT(label != NULL && label[0] >= 'A' && label[0] <= 'Z');
                const size_t len = strlen(label);
                CTV_ASSERT(label[len - 1] != '.' && label[len - 1] != ')');
                for (const char* jargon : { "freeze", "shadow", "suspend", "heap" }) {
                    CTV_ASSERT(strstr(label, jargon) == NULL);
                }
            }
        }
        CTV_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_OOT, COMBO_TRACKER_FRESH_STALE),
                          Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE)) != 0);
    }

    // ---- Leave global state clean -----------------------------------------
    // Adapters stay registered (the production state); the shadow goes back to
    // exactly the bytes this row inherited.
    Context_UpdateShadowCopy(GAME_MM, shadowBackup.data(), shadowBackup.size());
    Context_SetCurrentGame(prevGame);

    if (u4Failures != 0) {
        printf("[TEST] FAIL: %d of the two #458 U4 row sections (MM 2b, OoT 5b) failed\n", u4Failures);
        return TEST_FAIL;
    }
    if (sCtvU5Failures != 0) {
        printf("[TEST] FAIL: %d of the two #458 U5 skip-toggle halves (MM, OoT 5c) failed\n", sCtvU5Failures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: adapters recover authored MM shadow + OoT heap worlds, label staleness honestly, and "
           "answer unavailable states without dereferencing (#458)\n");
    return TEST_PASS;
}
