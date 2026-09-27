/**
 * @file test_combo_spoiler_view.c
 * @brief ROM-free lock for the cross-game spoiler VIEW MODEL (#496; #755, #757).
 *
 * The model is the thing that turns "a JSON file on disk the operator has to
 * be told the path to" into something the running game can render. This test
 * does NOT stub it. It populates the two sources the give path reads — the
 * legacy pinned table through the REAL Combo_SetForeignPlacement, and the
 * crossing store through the REAL Combo_Crossings_Replace — with REAL items
 * (looked up by name through the origin describer), and asserts on the
 * model's output:
 *
 *  1. With Combo_ForeignPairingActive() false, ZERO rows in both directions and
 *     a summary with paired == false — the assertion that separates "no
 *     crossings" from "no pairing". An unpaired world must never render as an
 *     empty crossing list.
 *  2. BOTH DIRECTIONS, FROM THE STORE (#755). Store rows are listed for the
 *     game that hosts them, in insertion order, each with its describer name
 *     and article. Before #755 the view read the forward pinned table only, so
 *     every single-bag world (whose crossings live in the store alone) showed
 *     none, and the MM-items-in-OoT direction was never shown at all.
 *  3. THE GIVE PATH'S PRECEDENCE. Legacy pinned rows come first, in slot order,
 *     and a store row whose host also has a pinned row is NOT listed: the give
 *     path reads the pinned row there, so listing both would show a crossing
 *     the world cannot yield.
 *  4. MM HOST CHECKS ARE NAMED (#757) once MM's tracker adapter is registered
 *     (the production state from Combo_TrackerWindow_Init), not hex ids.
 *  5. The model round-trips a .redsave Save/Load unchanged, so a reloaded
 *     session shows the same crossings (the pinned table rides Tier-1, the
 *     store rides Tier-4).
 *
 * Found state (per host check, from each game's own save) is locked by
 * ComboTrackerView and ComboCrossingViews, whose authored MM shadow and OoT
 * heap it needs; this row keeps to what the spoiler's rows are.
 *
 * Deliberately absent: any assertion about pixels. The model is pure C with no
 * ImGui; the window that renders it is locked separately for registration and
 * game-agnosticism, and its APPEARANCE is judged from the UiSnapshot captures.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_foreign_items.c) for the rsbs::SaveManager half; every model
 * symbol it drives is C-linkage.
 */

#include "../combo_spoiler_view.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>

#define CSV_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {
// Real MM RandoCheckIds (below RC_MAX), stored opaquely by the common layer.
const uint16_t kSpoilerCheckA = 0x0311;
const uint16_t kSpoilerCheckB = 0x0312;
const uint16_t kSpoilerCheckC = 0x0313;
const uint16_t kSpoilerStoreMM = 0x0401;
// OoT check ids hosting MM items (store rows).
const uint16_t kSpoilerStoreOoT1 = 0x0055;
const uint16_t kSpoilerStoreOoT2 = 0x0056;
const char* const kSpoilerSaveDir = "rsbs_test_saves_spoiler_view";

ComboCrossing SpoilerCrossing(uint16_t host, SharedItem item) {
    ComboCrossing c;
    c.hostCheck = host;
    c.itemClass = 0x0001;
    c.item = item;
    return c;
}

/** Row `index` of `hostGame` has this host and item, a name and an article. */
bool SpoilerRowIs(uint8_t hostGame, int index, uint16_t host, SharedItem item, const char* name) {
    ComboSpoilerRow row;
    memset(&row, 0, sizeof(row));
    if (!Combo_SpoilerRowAt(hostGame, index, &row)) {
        printf("[TEST] combo-spoiler-view: no row %d for host game %u\n", index, (unsigned)hostGame);
        return false;
    }
    const bool ok = row.hostGame == hostGame && row.hostCheckId == host && row.originGame == item.originGame &&
                    row.itemId == item.id && row.itemName != NULL && strcmp(row.itemName, name) == 0 &&
                    row.itemArticle != NULL;
    if (!ok) {
        printf("[TEST] combo-spoiler-view: row %d of host game %u is host 0x%04X item %u:%u '%s', expected host "
               "0x%04X item %u:%u '%s'\n",
               index, (unsigned)hostGame, (unsigned)row.hostCheckId, (unsigned)row.originGame, (unsigned)row.itemId,
               row.itemName != NULL ? row.itemName : "(null)", (unsigned)host, (unsigned)item.originGame,
               (unsigned)item.id, name);
    }
    return ok;
}
} // namespace

TestResult Test_ComboSpoilerView(void) {
    printf("[TEST] combo-spoiler-view: the in-game view model lists both directions' crossings from the crossing "
           "store and the legacy pinned table, named, and distinguishes unpaired from empty (#496, #755, #757)\n");

    // Real items, by name (the placement rows below).
    SharedItem oot[4];
    const char* const kOoTNames[4] = { "Lens of Truth", "Megaton Hammer", "Boomerang", "Hookshot" };
    for (int i = 0; i < 4; i++) {
        CSV_ASSERT(TestNamedItem((uint8_t)GAME_OOT, kOoTNames[i], &oot[i]));
    }
    SharedItem mm[2];
    const char* const kMMNames[2] = { "Lens of Truth", "Hookshot" };
    for (int i = 0; i < 2; i++) {
        CSV_ASSERT(TestNamedItem((uint8_t)GAME_MM, kMMNames[i], &mm[i]));
    }

    // ------------------------------------------------------------------
    // 1 (first, while the state is honestly unpaired): NOT PAIRED must not
    // look like NO CROSSINGS.
    // ------------------------------------------------------------------
    ComboContext_Init();
    Combo_Crossings_Clear();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    CSV_ASSERT(!Combo_ForeignPairingActive());

    ComboSpoilerSummary summary;
    memset(&summary, 0xA5, sizeof(summary));
    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(!summary.paired);
    CSV_ASSERT(summary.mmHosted == 0 && summary.ootHosted == 0);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) == 0);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_OOT) == 0);

    ComboSpoilerRow row;
    memset(&row, 0xA5, sizeof(row));
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_MM, 0, &row));
    CSV_ASSERT(row.hostCheckId == 0xA5A5); // untouched on failure

    // Crossings EXIST (both sources) but pairing does not: the model must
    // still report zero rows. This is the leg that would pass vacuously if the
    // model simply counted the tables.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE96u;
    CSV_ASSERT(!Combo_ForeignPairingActive()); // seed without settings digest
    gComboCtx.sharedRandoSettingsHash = 0x5EED0496u;
    CSV_ASSERT(Combo_ForeignPairingActive());
    CSV_ASSERT(Combo_SetForeignPlacement(kSpoilerCheckA, oot[0]) >= 0);
    {
        const ComboCrossing ootHosted[2] = { SpoilerCrossing(kSpoilerStoreOoT1, mm[0]),
                                             SpoilerCrossing(kSpoilerStoreOoT2, mm[1]) };
        // kSpoilerCheckA is ALSO pinned: the store row there is shadowed (leg 3).
        const ComboCrossing mmHosted[2] = { SpoilerCrossing(kSpoilerStoreMM, oot[3]),
                                            SpoilerCrossing(kSpoilerCheckA, oot[2]) };
        CSV_ASSERT(Combo_Crossings_Replace(ootHosted, 2, mmHosted, 2) == 4);
    }
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) > 0);
    gComboCtx.sharedRandoSettingsHash = 0; // un-pair, leaving both sources populated
    CSV_ASSERT(Combo_CountForeignPlacements() == 1 && Combo_Crossings_Count(GAME_OOT) == 2);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) == 0);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_OOT) == 0);
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_OOT, 0, &row));
    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(!summary.paired && summary.mmHosted == 0 && summary.ootHosted == 0);
    gComboCtx.sharedRandoSettingsHash = 0x5EED0496u; // re-pair for the rest

    // ------------------------------------------------------------------
    // 2 + 3: both directions; pinned first, then the store, shadowed hosts
    //        skipped.
    // ------------------------------------------------------------------
    CSV_ASSERT(Combo_SetForeignPlacement(kSpoilerCheckB, oot[1]) >= 0);
    CSV_ASSERT(Combo_SetForeignPlacement(kSpoilerCheckC, oot[2]) >= 0);

    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(summary.paired);
    CSV_ASSERT(summary.sharedRandoSeed == 0xC0FFEE96u);
    CSV_ASSERT(summary.sharedRandoSettingsHash == 0x5EED0496u);
    // MM hosts: 3 pinned + 1 store row (the store row on kSpoilerCheckA is shadowed).
    CSV_ASSERT(summary.mmHosted == 4);
    CSV_ASSERT(summary.ootHosted == 2);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) == 4);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_OOT) == 2);

    CSV_ASSERT(SpoilerRowIs((uint8_t)GAME_MM, 0, kSpoilerCheckA, oot[0], kOoTNames[0]));
    CSV_ASSERT(SpoilerRowIs((uint8_t)GAME_MM, 1, kSpoilerCheckB, oot[1], kOoTNames[1]));
    CSV_ASSERT(SpoilerRowIs((uint8_t)GAME_MM, 2, kSpoilerCheckC, oot[2], kOoTNames[2]));
    CSV_ASSERT(SpoilerRowIs((uint8_t)GAME_MM, 3, kSpoilerStoreMM, oot[3], kOoTNames[3]));
    CSV_ASSERT(SpoilerRowIs((uint8_t)GAME_OOT, 0, kSpoilerStoreOoT1, mm[0], kMMNames[0]));
    CSV_ASSERT(SpoilerRowIs((uint8_t)GAME_OOT, 1, kSpoilerStoreOoT2, mm[1], kMMNames[1]));
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_MM, 4, &row));  // one past the end
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_OOT, 2, &row)); // one past the end
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_MM, -1, &row)); // negative index
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_MM, 0, NULL));  // NULL out
    CSV_ASSERT(!Combo_SpoilerRowAt((uint8_t)GAME_NONE, 0, &row)); // not a game
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_NONE) == 0);
    // The article comes from the origin's describer (OoT: "the " for the Lens).
    CSV_ASSERT(Combo_SpoilerRowAt((uint8_t)GAME_MM, 0, &row));
    {
        const char* article = Combo_GetForeignItemArticle(oot[0]);
        CSV_ASSERT(strcmp(row.itemArticle, article != NULL ? article : "") == 0);
    }

    // ------------------------------------------------------------------
    // 4: MM host checks are NAMED once MM's tracker adapter is registered.
    // ------------------------------------------------------------------
    MM_TrackerAdapter_Register();
    for (int i = 0; i < Combo_SpoilerRowCount((uint8_t)GAME_MM); i++) {
        CSV_ASSERT(Combo_SpoilerRowAt((uint8_t)GAME_MM, i, &row));
        CSV_ASSERT(row.hostCheckName != NULL && row.hostCheckName[0] != '\0');
        CSV_ASSERT(strncmp(row.hostCheckName, "RC_", 3) != 0); // the readable name, not the enum spelling
        CSV_ASSERT(strcmp(row.hostCheckName, Combo_TrackerCheckName((uint8_t)GAME_MM, row.hostCheckId)) == 0);
    }

    // ------------------------------------------------------------------
    // 5: the whole view round-trips a .redsave Save/Load unchanged.
    // ------------------------------------------------------------------
    ComboSpoilerRow expectedMM[4];
    ComboSpoilerRow expectedOoT[2];
    for (int i = 0; i < 4; i++) {
        CSV_ASSERT(Combo_SpoilerRowAt((uint8_t)GAME_MM, i, &expectedMM[i]));
    }
    for (int i = 0; i < 2; i++) {
        CSV_ASSERT(Combo_SpoilerRowAt((uint8_t)GAME_OOT, i, &expectedOoT[i]));
    }

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSpoilerSaveDir);
    mgr.DeleteSave(0);
    CSV_ASSERT(mgr.Save(0));

    ComboContext_Init(); // wipe live state...
    Combo_Crossings_Clear();
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) == 0);
    memset(gComboCtx.foreignPlacements, 0x5A, sizeof(gComboCtx.foreignPlacements)); // ...then scribble
    CSV_ASSERT(mgr.Load(0));

    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_MM) == 4);
    CSV_ASSERT(Combo_SpoilerRowCount((uint8_t)GAME_OOT) == 2);
    for (int i = 0; i < 6; i++) {
        const uint8_t host = i < 4 ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
        const ComboSpoilerRow& want = i < 4 ? expectedMM[i] : expectedOoT[i - 4];
        memset(&row, 0, sizeof(row));
        CSV_ASSERT(Combo_SpoilerRowAt(host, i < 4 ? i : i - 4, &row));
        CSV_ASSERT(row.hostCheckId == want.hostCheckId);
        CSV_ASSERT(row.originGame == want.originGame);
        CSV_ASSERT(row.itemId == want.itemId);
        CSV_ASSERT(strcmp(row.itemName, want.itemName) == 0);
        CSV_ASSERT(strcmp(row.itemArticle, want.itemArticle) == 0);
        CSV_ASSERT(row.found == want.found);
    }
    Combo_SpoilerPairingSummary(&summary);
    CSV_ASSERT(summary.paired);
    CSV_ASSERT(summary.sharedRandoSeed == 0xC0FFEE96u);
    CSV_ASSERT(summary.sharedRandoSettingsHash == 0x5EED0496u);
    mgr.DeleteSave(0);
    mgr.SetSaveDirectory("Save");

    // Leave global state clean for any subsequent test.
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    Combo_Crossings_Clear();

    printf("[TEST] PASS: spoiler view lists both directions from the crossing store after the legacy pinned rows "
           "(shadowed hosts skipped), named and articled, survives a save round trip, and reports unpaired "
           "distinctly from empty\n");
    return TEST_PASS;
}
