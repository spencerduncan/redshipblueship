/**
 * @file test_roundtrip_integrity.c
 * @brief Roundtrip SaveContext byte-integrity unit test (issue #262, Phase 2 T3)
 *
 * Extends the Test_Roundtrip pattern (test_runner.cpp) from a marker-byte spot
 * check into a FULL byte-identical comparison of a populated OoT SaveContext
 * before vs after an OoT -> MM -> OoT roundtrip.
 *
 * This file is #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * NOT inside the extern "C" block used for test_game_lifecycle.c. That
 * placement is required: the body calls Entrance_Init() and
 * Entrance_RegisterDefaultLinks(), which entrance.h declares only in its C++
 * section and entrance.cpp defines with C++ (mangled) linkage. Under extern "C"
 * the unmangled Entrance_Init would instead bind to OoT's randomizer
 * Entrance_Init (randomizer_entrance.c). Compiled as C++ it correctly resolves
 * to the combo entrance system. The Combo_, Context_, and ComboContext_ symbols
 * and gComboCtx are genuine C-linkage and resolve identically either way.
 *
 * Headless: no SDL, no real game boot. The roundtrip is driven entirely through
 * the production C API (Combo_CheckCrossGameEntrance / Combo_FreezeState /
 * Combo_RestoreState) that z_play.c uses, plus direct manipulation of gComboCtx.
 *
 * --------------------------------------------------------------------------
 * Exclusion list (fields the switch is allowed to mutate)
 * --------------------------------------------------------------------------
 * The acceptance criterion allows an explicitly enumerated set of fields that a
 * cross-game switch may touch. In this architecture those fields are the
 * cross-game shared state, which lives in the ComboContext (gComboCtx) struct,
 * NOT inside either game's SaveContext buffer:
 *
 *   - gComboCtx.sharedFlags[64]   (cross-game event flags)
 *   - gComboCtx.sharedItems[32]    (cross-game shared inventory)
 *   - gComboCtx.sharedRandoSeed    (shared randomizer seed)
 *   - gComboCtx.sourceIsRando      (rando-mode propagation)
 *   - gComboCtx.switchRequested / targetGame / targetEntrance /
 *     sourceGame / sourceEntrance (switch routing bookkeeping)
 *
 * Because every excluded field lives OUTSIDE the OoT SaveContext buffer, the
 * OoT SaveContext itself must come back byte-for-byte identical after the
 * roundtrip. To prove the exclusion is real and not vacuous, this test
 * deliberately mutates those gComboCtx fields between freeze and restore and
 * then asserts the restored OoT SaveContext is still byte-identical. If a
 * future change ever stores any of those shared fields inside the SaveContext
 * buffer, this test will fail at the offending offset and the exclusion list
 * above must be revisited.
 */

#include "../context.h"
#include "../entrance.h"
#include "../test_runner.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Local pass/fail helper, matching the style of test_game_lifecycle.c. */
#define RT_ASSERT(cond)                                                       \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            return 1;                                                         \
        }                                                                     \
    } while (0)

/**
 * Fill a buffer with deterministic, fully non-zero bytes.
 *
 * A simple LCG keeps the test reproducible across runs/platforms. We force the
 * low bit so no byte is ever 0x00 -- a zero-filled "populated" buffer would let
 * a buggy memcpy that drops bytes pass by accident.
 */
static void RoundtripIntegrity_FillDeterministic(uint8_t* buf, size_t size) {
    uint32_t state = 0x1BADB002u;
    for (size_t i = 0; i < size; i++) {
        state = state * 1664525u + 1013904223u;
        uint8_t b = (uint8_t)((state >> 24) & 0xFF);
        buf[i] = (uint8_t)(b | 0x01); /* guarantee non-zero */
    }
}

/**
 * Report the first byte offset at which two buffers differ (for diagnostics).
 * Returns the offset, or `size` if the buffers are byte-identical.
 */
static size_t RoundtripIntegrity_FirstDiff(const uint8_t* a, const uint8_t* b, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    return size;
}

/**
 * Roundtrip integrity test body.
 * @return 0 on pass, 1 on fail (matches the lifecycle test convention).
 */
static int TestRoundtripIntegrity_Run(void) {
    printf("[TEST] roundtrip-integrity: OoT SaveContext byte-identical across OoT->MM->OoT (issue #262)\n");

    /* Fresh, deterministic starting point. */
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Entrance_Init();
    Entrance_RegisterDefaultLinks();

    /* Build a populated (non-zero, deterministic) OoT SaveContext and snapshot
     * it. Static: at the full runtime blob capacity (~136KB each) these three
     * buffers would otherwise put ~400KB on one stack frame. */
    static uint8_t ootSave[OOT_SAVE_CONTEXT_SIZE];
    static uint8_t snapshot[OOT_SAVE_CONTEXT_SIZE];
    RoundtripIntegrity_FillDeterministic(ootSave, sizeof(ootSave));
    memcpy(snapshot, ootSave, sizeof(snapshot));

    /* ----------------------------------------------------------------
     * Leg 1: OoT -> MM via Happy Mask Shop.
     * Drive the production switch API, then freeze the populated OoT save.
     * ---------------------------------------------------------------- */
    Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);
    RT_ASSERT(Combo_IsCrossGameSwitch());
    RT_ASSERT(strcmp(Combo_GetSwitchTargetGameId(), "mm") == 0);

    Combo_FreezeState("oot", Combo_GetSwitchReturnEntrance(), ootSave, sizeof(ootSave));
    RT_ASSERT(Combo_HasFrozenState("oot"));
    Entrance_ClearPendingSwitch();

    /* ----------------------------------------------------------------
     * While "in MM": mutate ONLY the documented exclusion-list fields
     * (the cross-game shared state in gComboCtx). None of these live in
     * the OoT SaveContext buffer, so the restored save must be unaffected.
     * Also scribble over the working OoT buffer to prove the restore
     * actually rewrites it from frozen storage rather than leaving it.
     * ---------------------------------------------------------------- */
    for (int i = 0; i < 64; i++) {
        gComboCtx.sharedFlags[i] = 0xABCD0000u | (uint32_t)i;
    }
    for (int i = 0; i < 32; i++) {
        gComboCtx.sharedItems[i] = (uint16_t)(0x1000 + i);
    }
    gComboCtx.sharedRandoSeed = 0xDEADBEEFu;
    gComboCtx.sourceIsRando = true;
    memset(ootSave, 0x5A, sizeof(ootSave));

    /* ----------------------------------------------------------------
     * Leg 2: MM -> OoT via South Clock Town.
     * ---------------------------------------------------------------- */
    Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);
    RT_ASSERT(Combo_IsCrossGameSwitch());
    RT_ASSERT(strcmp(Combo_GetSwitchTargetGameId(), "oot") == 0);
    Entrance_ClearPendingSwitch();

    /* ----------------------------------------------------------------
     * Leg 3: restore the OoT SaveContext and assert byte-for-byte equality
     * with the pre-switch snapshot (modulo the gComboCtx exclusion list,
     * which is not part of this buffer).
     * ---------------------------------------------------------------- */
    static uint8_t restored[OOT_SAVE_CONTEXT_SIZE];
    memset(restored, 0x00, sizeof(restored));
    RT_ASSERT(Combo_RestoreState("oot", restored, sizeof(restored)) != 0);

    size_t diff = RoundtripIntegrity_FirstDiff(snapshot, restored, sizeof(snapshot));
    if (diff != sizeof(snapshot)) {
        printf("  FAIL: OoT SaveContext diverged at offset 0x%04zX (snapshot=0x%02X restored=0x%02X)\n", diff,
               snapshot[diff], restored[diff]);
        return 1;
    }
    RT_ASSERT(memcmp(snapshot, restored, sizeof(snapshot)) == 0);

    /* Sanity: the excluded shared fields really were mutated (exclusion is not
     * vacuous) yet the SaveContext above still matched byte-for-byte. */
    RT_ASSERT(gComboCtx.sharedRandoSeed == 0xDEADBEEFu);
    RT_ASSERT(gComboCtx.sharedFlags[0] == 0xABCD0000u);

    printf("[TEST] PASS: OoT SaveContext is byte-identical after OoT->MM->OoT\n");
    printf("[TEST]       (excluded shared fields live in gComboCtx, outside the SaveContext buffer)\n");

    /* Leave global test state clean for any subsequent test. */
    Context_ClearAllFrozenStates();
    Entrance_ClearPendingSwitch();
    ComboContext_Init();
    return 0;
}
