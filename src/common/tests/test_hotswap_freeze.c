/**
 * @file test_hotswap_freeze.c
 * @brief Hot-swap (F10) freeze/consume contract — issue #364
 *
 * Locks the two halves of the #364 fix:
 *
 *   1. A hot swap freezes the DEPARTING game with a valid return entrance, or
 *      it refuses. It never leaves the launcher to restore someone else's blob.
 *   2. A frozen blob is single-use. Consuming it retires it, so it can never be
 *      re-applied on a later entry.
 *
 * The bug this replaces was silent. F10 froze nothing, but rsbs/src/main.cpp
 * still keyed the return-leg restore off Context_HasFrozenState(target), and
 * nothing ever cleared that flag. So after one entrance switch, every later F10
 * return re-applied the FIRST departure's snapshot — the player kept playing,
 * kept collecting, and kept losing it, with no error and no visible symptom.
 * Assertion 5 below is the direct regression: freeze fresh progress, consume,
 * and require the NEW bytes rather than the old ones.
 *
 * Headless: no SDL, no game boot, no ROM archives. It drives the exact
 * production entry points — Switch_PrepareHotSwap (what the launcher calls via
 * Combo_FreezeActiveGameForHotSwap) and Combo_ConsumeFrozenState (what both
 * games' Play_Init startup-entrance consumption calls).
 *
 * #included into test_runner.cpp inside its extern "C" block: every symbol used
 * here is genuine C linkage (Context_*, Combo_*, Switch_*), and the entrance
 * ids are plain macros.
 *
 * The "../entrance.h" include below resolves to nothing in practice —
 * test_runner.cpp already includes entrance.h at FILE scope, so the guard is
 * set by the time we get here. That ordering matters: entrance.h's second half
 * declares C++-linkage Entrance_* functions, and pulling it in for the first
 * time from inside an extern "C" block would give those C linkage and collide
 * with OoT's randomizer Entrance_Init. Do not move this file's inclusion above
 * test_runner.cpp's own includes.
 */

#include "../context.h"
#include "../entrance.h"
#include "../test_runner.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Freeze/consume policy under test (src/common/switch.cpp). Declared locally
 * for the same reason the game TUs declare it locally: context.h is owned
 * elsewhere this wave, so the switch policy has no header of its own yet. */
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
uint16_t Switch_GetHotSwapReturnEntrance(GameId departing);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

#define HSF_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                  \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

/* A slice of a SaveContext is enough: Context_FreezeState/RestoreState clamp to
 * min(size, per-game capacity), so a short buffer round-trips exactly. */
#define HSF_BUF 512

static int TestHotSwapFreeze_Run(void) {
    uint8_t live[HSF_BUF];
    uint8_t scratch[HSF_BUF];

    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();

    /* ---- 1. Refusals produce nothing ------------------------------------
     * The launcher treats a 0 return as "keep running the current game". That
     * only stays safe if a refused freeze leaves no blob behind for a later
     * return leg to pick up. */
    memset(live, 0xA1, sizeof(live));
    HSF_ASSERT(Switch_PrepareHotSwap(GAME_NONE, live, sizeof(live)) == 0);
    HSF_ASSERT(Switch_PrepareHotSwap(GAME_OOT, NULL, sizeof(live)) == 0);
    HSF_ASSERT(Switch_PrepareHotSwap(GAME_OOT, live, 0) == 0);
    HSF_ASSERT(Context_HasFrozenState(GAME_OOT) == 0);
    HSF_ASSERT(Context_HasFrozenState(GAME_MM) == 0);

    /* ---- 2. A hot swap freezes the departing game ------------------------
     * The return entrance must be the game's own portal arrival. It lands in
     * gSaveContext.entranceIndex, a direct linear index into the entrance
     * table, so an id from the WRONG game reads out of bounds (#356 class). */
    HSF_ASSERT(Switch_PrepareHotSwap(GAME_OOT, live, sizeof(live)) == 1);
    HSF_ASSERT(Context_HasFrozenState(GAME_OOT) == 1);
    HSF_ASSERT(Context_GetFrozenReturnEntrance(GAME_OOT) == OOT_ENTR_MARKET_FROM_MASK_SHOP);
    HSF_ASSERT(Switch_GetHotSwapReturnEntrance(GAME_MM) == MM_ENTR_SOUTH_CLOCK_TOWN_0);
    HSF_ASSERT(Switch_GetHotSwapReturnEntrance(GAME_NONE) == 0);
    /* Freezing one side must not fabricate state for the other. */
    HSF_ASSERT(Context_HasFrozenState(GAME_MM) == 0);

    /* ---- 3. The return leg consumes it ----------------------------------- */
    memset(scratch, 0x00, sizeof(scratch));
    HSF_ASSERT(Combo_ConsumeFrozenState("oot", scratch, sizeof(scratch)) == 1);
    HSF_ASSERT(scratch[0] == 0xA1);
    HSF_ASSERT(scratch[HSF_BUF - 1] == 0xA1);
    /* ...and retires it in the same step. */
    HSF_ASSERT(Context_HasFrozenState(GAME_OOT) == 0);

    /* ---- 4. A consumed blob cannot be re-applied -------------------------
     * The second consume must report "nothing" and must not write. This is the
     * property that makes Context_HasFrozenState(target) in main.cpp mean "left
     * and not yet returned to" instead of "left at some point, ever". */
    memset(scratch, 0x3C, sizeof(scratch));
    HSF_ASSERT(Combo_ConsumeFrozenState("oot", scratch, sizeof(scratch)) == 0);
    HSF_ASSERT(scratch[0] == 0x3C);
    HSF_ASSERT(scratch[HSF_BUF - 1] == 0x3C);

    /* ---- 5. The #364 regression: the next trip carries the NEW state ------
     * Play on after the first round trip, hot swap again, come back. Before the
     * fix the return leg re-applied the step-2 snapshot (0xA1) and the 0xB2
     * progress vanished without a trace. */
    memset(live, 0xB2, sizeof(live));
    HSF_ASSERT(Switch_PrepareHotSwap(GAME_OOT, live, sizeof(live)) == 1);
    memset(scratch, 0x00, sizeof(scratch));
    HSF_ASSERT(Combo_ConsumeFrozenState("oot", scratch, sizeof(scratch)) == 1);
    HSF_ASSERT(scratch[0] != 0xA1); /* the silent rollback */
    HSF_ASSERT(scratch[0] == 0xB2);
    HSF_ASSERT(scratch[HSF_BUF - 1] == 0xB2);
    HSF_ASSERT(Context_HasFrozenState(GAME_OOT) == 0);

    /* ---- 6. The MM side behaves identically -------------------------------
     * F10 from MM freezes MM, not OoT. The launcher's departing-game lookup and
     * the freeze target have to agree or the two blobs cross over. */
    memset(live, 0xC3, sizeof(live));
    HSF_ASSERT(Switch_PrepareHotSwap(GAME_MM, live, sizeof(live)) == 1);
    HSF_ASSERT(Context_HasFrozenState(GAME_MM) == 1);
    HSF_ASSERT(Context_HasFrozenState(GAME_OOT) == 0);
    HSF_ASSERT(Context_GetFrozenReturnEntrance(GAME_MM) == MM_ENTR_SOUTH_CLOCK_TOWN_0);
    memset(scratch, 0x00, sizeof(scratch));
    HSF_ASSERT(Combo_ConsumeFrozenState("mm", scratch, sizeof(scratch)) == 1);
    HSF_ASSERT(scratch[0] == 0xC3);
    HSF_ASSERT(Context_HasFrozenState(GAME_MM) == 0);
    HSF_ASSERT(Combo_ConsumeFrozenState("mm", scratch, sizeof(scratch)) == 0);

    printf("[TEST] PASS: hot swap freezes the departing game or refuses; "
           "a consumed frozen state is retired and cannot be re-applied\n");

    /* Leave global state clean for any subsequent test. */
    Context_ClearAllFrozenStates();
    return 0;
}
