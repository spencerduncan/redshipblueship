/**
 * @file test_combo_spoiler_view.c
 * @brief ROM-free lock for the cross-game spoiler VIEW MODEL (#496).
 *
 * The model is the thing that turns "a JSON file on disk the operator has to
 * be told the path to" into something the running game can render. This test
 * does NOT stub it. It populates gComboCtx through the REAL
 * Combo_SetForeignPlacement with entries drawn from the REAL pinned pool,
 * records a crossing through the REAL give path (the same
 * MM_Rando_Foreign_RecordPickup -> Combo_RedeemSharedItemsForGame pair
 * test_foreign_items.c drives), and asserts on the model's output:
 *
 *  1. One row per occupied placement slot, in slot order, each carrying its
 *     pinned-pool display name — not a placeholder, not an id rendered as text.
 *  2. The crossed-and-awarded entry reports redeemed == true and the others
 *     false. This is the assertion that makes the view worth having: a spoiler
 *     that cannot distinguish "hosted" from "already collected" is a file dump.
 *  3. With Combo_ForeignPairingActive() false, ZERO rows and a summary with
 *     paired == false — the assertion that separates "no crossings" from "no
 *     pairing". An unpaired world must never render as an empty crossing list.
 *  4. The model round-trips a .redsave Save/Load unchanged, so a reloaded
 *     session shows the same crossings (same Save/Load pair
 *     test_foreign_items.c uses).
 *
 * Deliberately absent: any assertion about pixels. The model is pure C with no
 * ImGui; the window that renders it is locked separately for registration and
 * game-agnosticism, and its APPEARANCE is operator verification.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_foreign_items.c) for the rsbs::SaveManager half; every model
 * symbol it drives is C-linkage.
 */

#include "../combo_spoiler_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

extern "C" {
int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId);
}

#define CSV_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {
// Arbitrary nonzero MM RandoCheckIds — the common layer stores them opaquely.
const uint16_t kSpoilerCheckA = 0x0311;
const uint16_t kSpoilerCheckB = 0x0312;
const uint16_t kSpoilerCheckC = 0x0313;
const char* const kSpoilerSaveDir = "rsbs_test_saves_spoiler_view";

void SpoilerNoopAward(const SharedItem* item, void* ctx) {
    (void)item;
    (void)ctx;
}
} // namespace

TestResult Test_ComboSpoilerView(void) {
    printf("[TEST] combo-spoiler-view: the in-game view model reports crossings, their names and their collected "
           "state, and distinguishes unpaired from empty (#496)\n");

    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    CSV_ASSERT(pool != NULL);
    CSV_ASSERT(poolCount >= 3); // this test places three distinct pool entries

    // ------------------------------------------------------------------
    // 3 (first, while the state is honestly unpaired): NOT PAIRED must not
    // look like NO CROSSINGS.
    // ------------------------------------------------------------------
    ComboContext_Init();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    CSV_ASSERT(!Combo_ForeignPairingActive());

    ComboSpoilerSummary summary;
    memset(&summary, 0xA5, sizeof(summary));
    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(!summary.paired);
    CSV_ASSERT(summary.placementCount == 0);
    CSV_ASSERT(Combo_SpoilerRowCount() == 0);

    ComboSpoilerRow row;
    memset(&row, 0xA5, sizeof(row));
    CSV_ASSERT(!Combo_SpoilerRowAt(0, &row));
    CSV_ASSERT(row.mmCheckId == 0xA5A5); // untouched on failure

    // Placements EXIST but pairing does not: the model must still report zero
    // rows. This is the leg that would pass vacuously if the model simply
    // counted the table.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE96u;
    CSV_ASSERT(!Combo_ForeignPairingActive()); // seed without settings digest
    gComboCtx.sharedRandoSettingsHash = 0x5EED0496u;
    CSV_ASSERT(Combo_ForeignPairingActive());
    CSV_ASSERT(Combo_SetForeignPlacement(kSpoilerCheckA, pool[0].item) >= 0);
    CSV_ASSERT(Combo_SpoilerRowCount() == 1);
    gComboCtx.sharedRandoSettingsHash = 0; // un-pair, leaving the table populated
    CSV_ASSERT(Combo_CountForeignPlacements() == 1);
    CSV_ASSERT(Combo_SpoilerRowCount() == 0);
    CSV_ASSERT(!Combo_SpoilerRowAt(0, &row));
    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(!summary.paired && summary.placementCount == 0);
    gComboCtx.sharedRandoSettingsHash = 0x5EED0496u; // re-pair for the rest

    // ------------------------------------------------------------------
    // 1: one row per occupied slot, in slot order, with pinned-pool names.
    // ------------------------------------------------------------------
    CSV_ASSERT(Combo_SetForeignPlacement(kSpoilerCheckB, pool[1].item) >= 0);
    CSV_ASSERT(Combo_SetForeignPlacement(kSpoilerCheckC, pool[2].item) >= 0);
    CSV_ASSERT(Combo_SpoilerRowCount() == 3);

    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(summary.paired);
    CSV_ASSERT(summary.sharedRandoSeed == 0xC0FFEE96u);
    CSV_ASSERT(summary.sharedRandoSettingsHash == 0x5EED0496u);
    CSV_ASSERT(summary.placementCount == 3);

    const uint16_t expectedChecks[3] = { kSpoilerCheckA, kSpoilerCheckB, kSpoilerCheckC };
    for (int i = 0; i < 3; i++) {
        memset(&row, 0, sizeof(row));
        CSV_ASSERT(Combo_SpoilerRowAt(i, &row));
        CSV_ASSERT(row.mmCheckId == expectedChecks[i]);
        CSV_ASSERT(row.originGame == (uint8_t)GAME_OOT);
        CSV_ASSERT(row.itemId == pool[i].item.id);
        // The pinned-pool display name, not a placeholder and not the fallback.
        CSV_ASSERT(row.itemName != NULL);
        CSV_ASSERT(strcmp(row.itemName, pool[i].name) == 0);
        CSV_ASSERT(strcmp(row.itemName, RSBS_SPOILER_UNKNOWN_ITEM_NAME) != 0);
        CSV_ASSERT(!row.redeemed); // nothing collected yet
    }
    CSV_ASSERT(!Combo_SpoilerRowAt(3, &row));  // one past the end
    CSV_ASSERT(!Combo_SpoilerRowAt(-1, &row)); // negative index
    CSV_ASSERT(!Combo_SpoilerRowAt(0, NULL));  // NULL out

    // ------------------------------------------------------------------
    // 2: the collected crossing reports redeemed; its neighbours do not.
    //    Driven through the REAL give path, not by setting the flag.
    // ------------------------------------------------------------------
    CSV_ASSERT(MM_Rando_Foreign_RecordPickup(kSpoilerCheckB) == 1);
    // Picked up but not yet awarded on the OoT side: the crossing is pending,
    // not redeemed. The view must not call that "collected".
    CSV_ASSERT(Combo_SpoilerRowAt(1, &row) && !row.redeemed);

    CSV_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, SpoilerNoopAward, NULL) == 1);
    for (int i = 0; i < 3; i++) {
        memset(&row, 0, sizeof(row));
        CSV_ASSERT(Combo_SpoilerRowAt(i, &row));
        CSV_ASSERT(row.redeemed == (i == 1));
    }
    // The placement survives redemption — the world still hosts the item, so
    // the spoiler stays truthful; only the crossing is marked done.
    CSV_ASSERT(Combo_SpoilerRowCount() == 3);

    // ------------------------------------------------------------------
    // 4: the whole view round-trips a .redsave Save/Load unchanged.
    // ------------------------------------------------------------------
    ComboSpoilerRow expectedRows[3];
    for (int i = 0; i < 3; i++) {
        CSV_ASSERT(Combo_SpoilerRowAt(i, &expectedRows[i]));
    }

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSpoilerSaveDir);
    mgr.DeleteSave(0);
    CSV_ASSERT(mgr.Save(0));

    ComboContext_Init(); // wipe live state...
    CSV_ASSERT(Combo_SpoilerRowCount() == 0);
    memset(gComboCtx.foreignPlacements, 0x5A, sizeof(gComboCtx.foreignPlacements)); // ...then scribble
    CSV_ASSERT(mgr.Load(0));

    CSV_ASSERT(Combo_SpoilerRowCount() == 3);
    for (int i = 0; i < 3; i++) {
        memset(&row, 0, sizeof(row));
        CSV_ASSERT(Combo_SpoilerRowAt(i, &row));
        CSV_ASSERT(row.mmCheckId == expectedRows[i].mmCheckId);
        CSV_ASSERT(row.originGame == expectedRows[i].originGame);
        CSV_ASSERT(row.itemId == expectedRows[i].itemId);
        CSV_ASSERT(strcmp(row.itemName, expectedRows[i].itemName) == 0);
        // The redeemed bit rides sharedItemsTagged, a different carve than the
        // placements — a reloaded session must not forget what was collected.
        CSV_ASSERT(row.redeemed == expectedRows[i].redeemed);
    }
    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(summary.paired);
    CSV_ASSERT(summary.sharedRandoSeed == 0xC0FFEE96u);
    CSV_ASSERT(summary.sharedRandoSettingsHash == 0x5EED0496u);
    mgr.DeleteSave(0);

    // Leave global state clean for any subsequent test.
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    printf("[TEST] PASS: spoiler view reports named crossings in slot order with their collected state, survives a "
           "save round trip, and reports unpaired distinctly from empty\n");
    return TEST_PASS;
}
