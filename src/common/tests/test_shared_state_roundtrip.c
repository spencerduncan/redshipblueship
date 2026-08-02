/**
 * @file test_shared_state_roundtrip.c
 * @brief Smoke test for cross-game shared-state plumbing (issue #264, Phase 2 T5)
 *
 * Validates the WIRING of gComboCtx.sharedFlags and gComboCtx.sharedRandoSeed
 * across an OoT -> MM game switch. In the single-executable build both games
 * read and write the SAME gComboCtx instance, so any flag/seed written while
 * "in OoT" before the switch must still be observable once execution is "in
 * MM". This is the shared-state contract Phase 3 (full rando) will rely on.
 *
 * Headless: this test only touches the shared ComboContext struct via its
 * public C API. It does NOT boot either game, allocate a SaveContext, or call
 * the game-port freeze/resume hooks (MM_FreezeState / MM_ResumeFromContext),
 * which are unavailable in the headless --test path. (OoT's counterparts used
 * to be named here too; they were deleted by #598 — never compiled, never
 * called, and a freeze-time writer of the seed stamp that the creation event
 * is now the sole author of.)
 *
 * Included into test_runner.cpp inside an extern "C" block (mirrors
 * test_game_lifecycle.c). Only C-linkage ComboContext_* symbols and gComboCtx
 * are referenced, so this placement is correct.
 */

#include "../context.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Freeze/consume policy under test (src/common/switch.cpp). Declared locally
 * for the same reason the game TUs and test_hotswap_freeze.c declare it
 * locally: context.h is owned elsewhere this wave, so the switch policy has no
 * header of its own. Duplicate prototypes across the #included test files are
 * harmless (declarations, not definitions). */
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

/* A representative cross-game flag: word 5 of the 64-word sharedFlags array,
 * bit 3. Both indices are comfortably in range (sharedFlags[64]). */
#define SHARED_TEST_FLAG_WORD 5
#define SHARED_TEST_FLAG_BIT (1u << 3)

/* A recognizable, non-zero seed value to track across the switch. */
#define SHARED_TEST_SEED 0xC0FFEE01u

TestResult Test_SharedStateRoundtrip(void) {
    printf("[TEST] shared-roundtrip: Shared flag/seed survive OoT -> MM switch (issue #264)\n");

    /* Clean slate: zero the shared context and stamp the magic/version. */
    ComboContext_Init();

    /* ------------------------------------------------------------------
     * Phase A: "in OoT", before the switch.
     * Set a shared flag and the shared rando seed, exactly as OoT rando
     * code would before handing off to MM.
     * ------------------------------------------------------------------ */
    gComboCtx.sourceGame = GAME_OOT;
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = SHARED_TEST_SEED;
    gComboCtx.sharedFlags[SHARED_TEST_FLAG_WORD] |= SHARED_TEST_FLAG_BIT;

    /* Capture the OoT-side view of the seed so we can prove the MM-side view
     * matches the same source value (not just the literal constant). */
    uint32_t ootSeed = gComboCtx.sharedRandoSeed;

    /* Request the switch to MM, then clear it the way the switch coordinator
     * does (ComboContext_ClearSwitch). The key invariant under test: clearing
     * the pending switch must NOT disturb sharedFlags / sharedRandoSeed.
     * 0xD800 = the OoT->MM arrival (South Clock Town tower exit); the value
     * is plumbing here, never applied as an entrance. */
    ComboContext_RequestSwitch(GAME_MM, 0xD800);
    if (!ComboContext_IsSwitchPending()) {
        printf("[TEST] FAIL: switch to MM was not registered as pending\n");
        return TEST_FAIL;
    }
    ComboContext_ClearSwitch();

    /* ------------------------------------------------------------------
     * Phase B: "in MM", after the switch.
     * Read the shared state back through the SAME gComboCtx.
     * ------------------------------------------------------------------ */

    /* Criterion 1: the flag set in OoT is readable from sharedFlags in MM. */
    if ((gComboCtx.sharedFlags[SHARED_TEST_FLAG_WORD] & SHARED_TEST_FLAG_BIT) == 0) {
        printf("[TEST] FAIL: shared flag set in OoT not readable after switch to MM\n");
        return TEST_FAIL;
    }

    /* Criterion 2: sharedRandoSeed is equal in both the OoT and MM views. */
    uint32_t mmSeed = gComboCtx.sharedRandoSeed;
    if (mmSeed != ootSeed) {
        printf("[TEST] FAIL: sharedRandoSeed mismatch across switch (oot=0x%08X mm=0x%08X)\n", ootSeed, mmSeed);
        return TEST_FAIL;
    }
    if (mmSeed != SHARED_TEST_SEED) {
        printf("[TEST] FAIL: sharedRandoSeed corrupted across switch (got 0x%08X, expected 0x%08X)\n", mmSeed,
               SHARED_TEST_SEED);
        return TEST_FAIL;
    }

    /* The rando-mode propagation fields must also be untouched by clearing the
     * pending switch -- Phase 3 reads these alongside the seed. (This makes the
     * sourceGame / sourceIsRando writes in Phase A load-bearing rather than
     * decorative.) */
    if (!gComboCtx.sourceIsRando) {
        printf("[TEST] FAIL: sourceIsRando did not survive the switch\n");
        return TEST_FAIL;
    }
    if (gComboCtx.sourceGame != GAME_OOT) {
        printf("[TEST] FAIL: sourceGame changed across switch (got %d, expected GAME_OOT)\n", gComboCtx.sourceGame);
        return TEST_FAIL;
    }

    printf("[TEST] PASS: shared flag + sharedRandoSeed propagate OoT -> MM\n");
    return TEST_PASS;
}

/* ==========================================================================
 * Origin-tagged shared-item round trip through the REAL hook functions
 * (ADR 0002 / Lane A1). This is the non-negotiable lock: a recorded cross-game
 * item survives suspend -> switch -> resume -> switch -> resume, visible in
 * BOTH directions, and is awarded exactly once per crossing.
 *
 * "Real hook functions" means the exact src/common entry points the game hooks
 * call — Combo_StageSharedItem / Combo_CommitStagedSharedItems (the producer at
 * OoT_Game_Suspend / MM_Game_Suspend), Combo_RedeemSharedItemsForGame (the
 * consumer at OoT_Play_Init / MM_Play_ConsumeStartupEntrance), and
 * Switch_PrepareHotSwap / Combo_ConsumeFrozenState (the F10 freeze/consume,
 * src/common/switch.cpp). test_hotswap_freeze.c is the precedent for driving
 * these headlessly rather than booting a game. The F10 path is exercised
 * directly: this test does NOT go through Combo_CheckEntranceSwitch, so a
 * producer that lived only there (dropping F10 switches) would fail here.
 * ========================================================================== */

/* Recognizable, non-zero ids in each game's id-space. Zero is "none"
 * (RG_NONE / RI_UNKNOWN) and marks an empty slot, so real crossings use
 * non-zero values. Plain integers on purpose — the point of the tagged struct
 * is that the origin GAME is what disambiguates these, not the numbers. */
#define SHARED_ITEM_MM_ID 0x0042u /* an MM RandoItemId, collected while in OoT */
#define SHARED_ITEM_OOT_ID 0x0135u /* an OoT RandomizerGet, collected while in MM */

/* A slice of a SaveContext is all the freeze path needs (it clamps to the
 * per-game capacity), matching test_hotswap_freeze.c. */
#define SIR_BUF 256

typedef struct {
    int awardCount;
    uint16_t lastId;
    uint8_t lastOrigin;
} SharedAwardCtx;

/* Mock of the game-side award callback (Lane C wires the real give). Records
 * what it was handed so the test can prove the RIGHT item was awarded, not just
 * that something was. */
static void SharedItemTestAward(const SharedItem* item, void* ctx) {
    SharedAwardCtx* c = (SharedAwardCtx*)ctx;
    c->awardCount++;
    c->lastId = item->id;
    c->lastOrigin = item->originGame;
}

/* Count occupied entries (any origin), redeemed or not — the "still visible"
 * check: a redeemed entry stays as the record of the crossing. */
static int SharedItem_OccupiedTotal(void) {
    return Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) +
           Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
}

#define SIR_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                            \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

TestResult Test_SharedItemRoundtrip(void) {
    printf("[TEST] shared-item-roundtrip: tagged item survives suspend->switch->resume x2, both dirs (ADR 0002)\n");

    uint8_t ootSave[SIR_BUF];
    uint8_t mmSave[SIR_BUF];
    uint8_t scratch[SIR_BUF];
    SharedAwardCtx award;

    /* Clean slate: empty tagged array + empty outbox + no frozen state. */
    ComboContext_Init();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();

    SIR_ASSERT(SharedItem_OccupiedTotal() == 0);

    /* ------------------------------------------------------------------
     * Leg 1: in OoT, collect a foreign (MM) item, then leave OoT for MM.
     * Uses the STAGE -> COMMIT-AT-SUSPEND producer path.
     * ------------------------------------------------------------------ */
    SIR_ASSERT(Combo_StageSharedItem(GAME_MM, SHARED_ITEM_MM_ID) == true);
    /* Staged, not yet committed: the durable/serialized array is still empty. */
    SIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 0);

    /* Suspend OoT: the F10 freeze of the departing game, then the producer
     * commit (both are what OoT_Game_Suspend + the launcher run). */
    memset(ootSave, 0xA1, sizeof(ootSave));
    SIR_ASSERT(Switch_PrepareHotSwap(GAME_OOT, ootSave, sizeof(ootSave)) == 1);
    SIR_ASSERT(Combo_CommitStagedSharedItems() == 1);
    SIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 1);

    /* Arrive in MM: first MM entry has no frozen MM state to consume, then the
     * consumer awards the MM-tagged item. */
    memset(scratch, 0x00, sizeof(scratch));
    SIR_ASSERT(Combo_ConsumeFrozenState("mm", scratch, sizeof(scratch)) == 0);
    memset(&award, 0, sizeof(award));
    SIR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, SharedItemTestAward, &award) == 1);
    SIR_ASSERT(award.awardCount == 1);
    SIR_ASSERT(award.lastOrigin == (uint8_t)GAME_MM);
    SIR_ASSERT(award.lastId == SHARED_ITEM_MM_ID);
    /* Redeemed, but still present (occupancy = originGame != GAME_NONE). */
    SIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
    SIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);

    /* ------------------------------------------------------------------
     * Leg 2: in MM, collect a foreign (OoT) item, then leave MM for OoT.
     * Uses the DIRECT-RECORD producer path (the other give-path entry).
     * ------------------------------------------------------------------ */
    SIR_ASSERT(Combo_RecordSharedItem(GAME_OOT, SHARED_ITEM_OOT_ID) >= 0);
    /* De-dup: recording the same un-redeemed item again must not duplicate. */
    SIR_ASSERT(Combo_RecordSharedItem(GAME_OOT, SHARED_ITEM_OOT_ID) >= 0);
    SIR_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1);

    /* Suspend MM: F10 freeze of MM + the producer commit (nothing staged this
     * leg — we recorded directly — so the commit is a no-op, which is fine). */
    memset(mmSave, 0xB2, sizeof(mmSave));
    SIR_ASSERT(Switch_PrepareHotSwap(GAME_MM, mmSave, sizeof(mmSave)) == 1);
    SIR_ASSERT(Combo_CommitStagedSharedItems() == 0);

    /* Arrive in OoT: consume the frozen OoT state frozen back on leg 1 (proves
     * the shared-item work and the freeze/restore machinery coexist), then the
     * consumer awards the OoT-tagged item. */
    memset(scratch, 0x00, sizeof(scratch));
    SIR_ASSERT(Combo_ConsumeFrozenState("oot", scratch, sizeof(scratch)) == 1);
    SIR_ASSERT(scratch[0] == 0xA1); /* the leg-1 OoT freeze came back intact */
    memset(&award, 0, sizeof(award));
    SIR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, SharedItemTestAward, &award) == 1);
    SIR_ASSERT(award.awardCount == 1);
    SIR_ASSERT(award.lastOrigin == (uint8_t)GAME_OOT);
    SIR_ASSERT(award.lastId == SHARED_ITEM_OOT_ID);

    /* Both crossings are now recorded and visible — one each direction. */
    SIR_ASSERT(SharedItem_OccupiedTotal() == 2);
    SIR_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) == 1);
    SIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);

    /* ------------------------------------------------------------------
     * Leg 3: switch back to OoT again (F10). The already-redeemed OoT item
     * must NOT be awarded a second time, and both entries must still survive.
     * This is the single-use guarantee (RSBS_SHARED_ITEM_REDEEMED).
     * ------------------------------------------------------------------ */
    memset(mmSave, 0xC3, sizeof(mmSave));
    SIR_ASSERT(Switch_PrepareHotSwap(GAME_MM, mmSave, sizeof(mmSave)) == 1);
    SIR_ASSERT(Combo_CommitStagedSharedItems() == 0);
    memset(scratch, 0x00, sizeof(scratch));
    Combo_ConsumeFrozenState("oot", scratch, sizeof(scratch)); /* no OoT freeze this leg */
    memset(&award, 0, sizeof(award));
    SIR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, SharedItemTestAward, &award) == 0);
    SIR_ASSERT(award.awardCount == 0);

    /* Everything still visible after the full round trip, correctly tagged and
     * marked redeemed. Nothing lost, nothing double-awarded. */
    SIR_ASSERT(SharedItem_OccupiedTotal() == 2);
    SIR_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0);
    SIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);

    /* Leave global state clean for any subsequent test. */
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    printf("[TEST] PASS: tagged item survives the full round trip both directions; awarded once each\n");
    return TEST_PASS;
}
