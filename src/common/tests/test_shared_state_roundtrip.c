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
 * the game-port freeze/resume hooks (OoT_FreezeState / MM_ResumeFromContext),
 * which are unavailable in the headless --test path.
 *
 * Included into test_runner.cpp inside an extern "C" block (mirrors
 * test_game_lifecycle.c). Only C-linkage ComboContext_* symbols and gComboCtx
 * are referenced, so this placement is correct.
 */

#include "../context.h"
#include "../test_runner.h"
#include <stdint.h>
#include <stdio.h>

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
     * the pending switch must NOT disturb sharedFlags / sharedRandoSeed. */
    ComboContext_RequestSwitch(GAME_MM, 0xC010);
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
