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
 *    table; rows carry the pinned-pool item name, the host-game check name
 *    (MM side), the redeemed bit from the tagged array, and vanish when the
 *    world is unpaired ("not paired" must never render as "no crossings").
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

#include <cstdio>
#include <cstring>
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

extern "C" int Combo_TrackerView_RunHeadless(void) {
    printf("[TEST] combo-tracker-view: per-game adapters recover authored shadow/heap worlds, staleness-labelled "
           "(#458)\n");

    const GameId prevGame = Context_GetCurrentGame();
    ComboContext_Init();

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
    // Never LIVE — not even while MM is the active game (the shadow lags the
    // live save; #458's staleness contract).
    Context_SetCurrentGame(GAME_MM);
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

    // ---- 4. MM check names resolve (the #489 class) -----------------------
    const char* mmName = Combo_TrackerCheckName((uint8_t)GAME_MM, kShuffledA);
    CTV_ASSERT(mmName != NULL && mmName[0] != '\0');

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
    // skipped, in that order). Names may be NULL (static data never
    // initialized in this tier) — the call must simply not crash.
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[0], &row));
    CTV_ASSERT(row.shuffled && row.obtained && !row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[1], &row));
    CTV_ASSERT(row.shuffled && !row.obtained && !row.skipped);
    CTV_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)ootIds[2], &row));
    CTV_ASSERT(row.shuffled && !row.obtained && row.skipped);
    (void)Combo_TrackerCheckName((uint8_t)GAME_OOT, ootIds[0]);

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

    const ComboForeignItemDef* ootPool = NULL;
    const int ootPoolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, &ootPool);
    CTV_ASSERT(ootPoolCount >= 1);
    const ComboForeignItemDef* mmPool = NULL;
    const int mmPoolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &mmPool);
    CTV_ASSERT(mmPoolCount >= 1);

    // One crossing per direction: MM check kObtainedB hosts an OoT item, and
    // an arbitrary OoT check hosts an MM item.
    CTV_ASSERT(Combo_SetForeignPlacement(kObtainedB, ootPool[0].item) >= 0);
    const uint16_t kOoTHostCheck = 0x0123;
    CTV_ASSERT(Combo_SetForeignPlacementOoT(kOoTHostCheck, mmPool[0].item) >= 0);

    Combo_TrackerIdentity(&identity);
    CTV_ASSERT(identity.paired);
    CTV_ASSERT(identity.sharedRandoSeed == 0xC0FFEE97u);
    CTV_ASSERT(identity.mmHostedForeign == 1 && identity.ootHostedForeign == 1);

    ComboTrackerForeignRow foreignRow;
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.hostGame == (uint8_t)GAME_MM);
    CTV_ASSERT(foreignRow.hostCheckId == kObtainedB);
    CTV_ASSERT(foreignRow.originGame == (uint8_t)GAME_OOT);
    CTV_ASSERT(strcmp(foreignRow.itemName, ootPool[0].name) == 0);
    // The MM adapter is registered, so the host check resolves to a name.
    CTV_ASSERT(foreignRow.hostCheckName != NULL && foreignRow.hostCheckName[0] != '\0');
    CTV_ASSERT(!foreignRow.redeemed);

    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 0, &foreignRow));
    CTV_ASSERT(foreignRow.hostGame == (uint8_t)GAME_OOT);
    CTV_ASSERT(foreignRow.hostCheckId == kOoTHostCheck);
    CTV_ASSERT(foreignRow.originGame == (uint8_t)GAME_MM);
    CTV_ASSERT(strcmp(foreignRow.itemName, mmPool[0].name) == 0);
    CTV_ASSERT(!Combo_TrackerForeignRowAt((uint8_t)GAME_OOT, 1, &foreignRow)); // only one crossing

    // The redeemed bit reads through from the tagged array.
    gComboCtx.sharedItemsTagged[0].originGame = ootPool[0].item.originGame;
    gComboCtx.sharedItemsTagged[0].id = ootPool[0].item.id;
    gComboCtx.sharedItemsTagged[0].flags = RSBS_SHARED_ITEM_REDEEMED;
    CTV_ASSERT(Combo_TrackerForeignRowAt((uint8_t)GAME_MM, 0, &foreignRow));
    CTV_ASSERT(foreignRow.redeemed);

    // Unpaired again: the rows must vanish ("not paired" != "no crossings").
    ComboContext_Init();
    CTV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0);
    CTV_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_OOT) == 0);

    // ---- Leave global state clean -----------------------------------------
    // Adapters stay registered (the production state); the authored shadow is
    // re-zeroed so later tests inherit the cold-boot shape.
    std::vector<uint8_t> zeros((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_UpdateShadowCopy(GAME_MM, zeros.data(), zeros.size());
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: adapters recover authored MM shadow + OoT heap worlds, label staleness honestly, and "
           "answer unavailable states without dereferencing (#458)\n");
    return TEST_PASS;
}
