/**
 * @file test_shared_song_of_time.c
 * @brief Locks for the Shared Song of Time cross-game rule (#1069, OoTMM
 *        sharedSongTime): OoT's RG_SONG_OF_TIME and MM's RI_SONG_TIME as ONE
 *        shared family on the #976 mechanism (PR #1002's P1), behind one
 *        frozen record bit.
 *
 * Two CTest rows (label "redship"), both display-free and ROM-free:
 *
 *   SharedSongOfTimePairs ("shared-song-of-time-pairs") - THE CREDITING. The
 *   bit is allocated (0x04000000, the next free RSBS_XITEM_* bit after #1068's
 *   Shared Shields), a family and named sharedSongTime; both games' real
 *   static-init registrations put their own Song of Time into the shared-family
 *   registry under it; the frozen rule arms it and the bag's shared-pair list
 *   (Combo_SingleBag_SharedPairs, what Combo_SingleBag_Run hands the composer)
 *   names the pair; the real composer, over both games' REAL item
 *   classifications, makes the two copies ONE RSBS_COMBO_BAG_SHARED_PAIR row both
 *   solvers credit and trims the other copy. With the rule off nothing is armed
 *   and no pair exists.
 *
 *   SharedSongOfTimeArrival ("shared-song-of-time-arrival") - THE ARRIVAL.
 *   Through both games' REAL harvest and apply shims, one live save at a time: a
 *   Song of Time learnt in one game is known (its quest bit, which is the whole
 *   of "the ocarina can play it" in both games) in the other at the arrival.
 *   With the rule off nothing crosses.
 *
 * The bit is spelled as a literal: it is .redsave format, and the lock states it
 * rather than reading it back from the header under test.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_logic.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
// The live-save helpers (the sword row's ArmLive pair, and this rule's own
// seams: games/oot/soh/oot_shared_song_of_time_test.cpp,
// games/mm/2s2h/mm_shared_song_of_time_test.cpp).
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_SharedSongOfTimeTest_SetHeld(int held);
int OoT_SharedSongOfTimeTest_Held(void);
int OoT_SharedSongOfTimeTest_PoolId(void);
void MM_SharedSongOfTimeTest_SetHeld(int held);
int MM_SharedSongOfTimeTest_Held(void);
int MM_SharedSongOfTimeTest_PoolId(void);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

/** The rule's record bit (.redsave format, append-only). */
static const uint32_t kSotBit = 0x04000000u;

static int sSotFailures;

#define SOT_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSotFailures++;                           \
        }                                             \
    } while (0)

/** Freeze a paired world the way a creation event does, with the Shared Song of
 *  Time rule on or off. */
static void SotFreezeWorld(bool on) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1069F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x1069CA5Eu;
    gComboCtx.mmProfileDigest = 0x1069D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = on ? kSotBit : 0u;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

/** Both games forget the Song of Time, on armed live saves. */
static void SotForgetAll(void) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedSongOfTimeTest_SetHeld(0);
    MM_SharedSwordTest_ArmLive();
    MM_SharedSongOfTimeTest_SetHeld(0);
}

/** Does @p pairs (of @p n) hold the Song of Time pair? */
static bool SotHasPair(const ComboLogicSharedPair* pairs, int n, uint16_t ootId, uint16_t mmId) {
    for (int i = 0; i < n; ++i) {
        if (pairs[i].ootId == ootId && pairs[i].mmId == mmId) {
            return true;
        }
    }
    return false;
}

TestResult Test_SharedSongOfTimePairs(void) {
    printf("[TEST] shared-song-of-time-pairs: the Shared Song of Time rule arms one shared family both games "
           "register, and the bag makes the two copies one row both solvers credit (#1069)\n");
    sSotFailures = 0;
    const uint16_t ootSot = (uint16_t)OoT_SharedSongOfTimeTest_PoolId();
    const uint16_t mmSot = (uint16_t)MM_SharedSongOfTimeTest_PoolId();

    // ---- (0) The format ---------------------------------------------------------
    printf("[TEST] (0) bit %08X: ALL_V1 %08X, FAMILY_MASK %08X, name '%s' (expected allocated, a family, "
           "sharedSongTime)\n",
           (unsigned)kSotBit, (unsigned)RSBS_XITEM_ALL_V1, (unsigned)RSBS_XITEM_FAMILY_MASK,
           Combo_ComboItemRuleName(kSotBit));
    SOT_CHECK((RSBS_XITEM_ALL_V1 & kSotBit) == kSotBit && (RSBS_XITEM_FAMILY_MASK & kSotBit) == kSotBit,
              "0x04000000 is not an allocated shared-family bit (ALL_V1 %08X, FAMILY_MASK %08X)",
              (unsigned)RSBS_XITEM_ALL_V1, (unsigned)RSBS_XITEM_FAMILY_MASK);
    SOT_CHECK(strcmp(Combo_ComboItemRuleName(kSotBit), "sharedSongTime") == 0,
              "the rule at 0x04000000 is named '%s', expected OoTMM's sharedSongTime",
              Combo_ComboItemRuleName(kSotBit));

    // ---- (1) THE REGISTRATIONS: each game's own Song of Time ---------------------
    {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamilyMemberId(GAME_OOT, kSotBit, &ootId);
        const bool mmReg = Combo_SharedFamilyMemberId(GAME_MM, kSotBit, &mmId);
        printf("[TEST] (1) registered: OoT %d (id %u), MM %d (id %u) (expected 1/%u, 1/%u)\n", ootReg ? 1 : 0,
               (unsigned)ootId, mmReg ? 1 : 0, (unsigned)mmId, (unsigned)ootSot, (unsigned)mmSot);
        SOT_CHECK(ootReg && ootId == ootSot,
                  "OoT did not register its Song of Time (RG_SONG_OF_TIME) as the Shared Song of Time family");
        SOT_CHECK(mmReg && mmId == mmSot,
                  "MM did not register its Song of Time (RI_SONG_TIME) as the Shared Song of Time family");
    }

    // ---- (2) OFF: nothing armed, no pair ------------------------------------------
    {
        SotFreezeWorld(false);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (2) off: armed %08X, Song of Time pair %d (expected 0, 0)\n",
               (unsigned)(Combo_SharedFamiliesArmed() & kSotBit), SotHasPair(pairs, n, ootSot, mmSot) ? 1 : 0);
        SOT_CHECK((Combo_SharedFamiliesArmed() & kSotBit) == 0u && !SotHasPair(pairs, n, ootSot, mmSot),
                  "OFF: the rule off armed the Song of Time or paired it; a world that never asked shares nothing");
    }

    // ---- (3) ON: the family armed, the pair built ----------------------------------
    ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
    int pairCount = 0;
    {
        SotFreezeWorld(true);
        pairCount = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (3) on: frozen rules %08X, armed %08X, %d pair(s), Song of Time pair %d (expected %08X, %08X, "
               "1, 1)\n",
               (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)Combo_SharedFamiliesArmed(), pairCount,
               SotHasPair(pairs, pairCount, ootSot, mmSot) ? 1 : 0, (unsigned)kSotBit, (unsigned)kSotBit);
        SOT_CHECK(Combo_SharedFamiliesArmed() == kSotBit,
                  "ON: the rule armed %08X; it must arm exactly the Shared Song of Time family",
                  (unsigned)Combo_SharedFamiliesArmed());
        SOT_CHECK(pairCount == 1 && SotHasPair(pairs, pairCount, ootSot, mmSot),
                  "ON: the bag's shared pairs are not exactly (RG_SONG_OF_TIME, RI_SONG_TIME)");
    }

    // ---- (4) THE CREDITING: the real composer over both real classifications --------
    {
        SOT_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table did not come up");
        ComboLogicPoolRow pool[2];
        memset(pool, 0, sizeof(pool));
        pool[0].item.originGame = (uint8_t)GAME_OOT;
        pool[0].item.id = ootSot;
        pool[1].item.originGame = (uint8_t)GAME_MM;
        pool[1].item.id = mmSot;
        const int ootDisp = Combo_Logic_ComposeDisposition(pool[0].item, 0u, 0u);
        const int mmDisp = Combo_Logic_ComposeDisposition(pool[1].item, 0u, 0u);
        // The premise: both songs are progression in their own game (OoT's song
        // is an advancement item, MM's RI_SONG_TIME is RITYPE_MAJOR), so both are
        // REQUIRED rows the composer can pair (pass 2b pairs only those).
        printf("[TEST] (4) dispositions: OoT %d, MM %d (expected %d, %d)\n", ootDisp, mmDisp,
               RSBS_COMBO_COMPOSE_REQUIRED, RSBS_COMBO_COMPOSE_REQUIRED);
        SOT_CHECK(ootDisp == RSBS_COMBO_COMPOSE_REQUIRED && mmDisp == RSBS_COMBO_COMPOSE_REQUIRED,
                  "a Song of Time is not a REQUIRED row; the row's premise moved");

        ComboLogicComposeRequest req;
        memset(&req, 0, sizeof(req));
        req.rows = pool;
        req.rowCount = 2;
        req.quantityFlags = RSBS_COMBO_QUANTITY_KEEP_ALL; // a song is not a quantity
        req.sharedPairs = pairCount > 0 ? pairs : NULL;
        req.sharedPairCount = pairCount;
        ComboLogicBagItem bag[4];
        int index[4];
        ComboLogicComposeResult res;
        memset(&res, 0, sizeof(res));
        const int rc = Combo_Logic_ComposeBag(&req, bag, 4, index, &res);
        printf("[TEST] (4) compose: rc %d, %d bag row(s), %d pair(s):", rc, res.bagCount, res.sharedPairs);
        for (int i = 0; i < res.bagCount && i < 4; ++i) {
            printf(" [%u:%u flags %04X pair %u]", (unsigned)bag[i].item.originGame, (unsigned)bag[i].item.id,
                   (unsigned)bag[i].bagFlags, (unsigned)bag[i].pairId);
        }
        printf(" (expected 0, 1, 1: [OoT Song of Time pair MM Song of Time])\n");
        const bool oneRow = rc == RSBS_COMBO_LOGIC_OK && res.sharedPairs == 1 && res.bagCount == 1 &&
                            (bag[0].bagFlags & RSBS_COMBO_BAG_SHARED_PAIR) != 0u &&
                            bag[0].item.originGame == (uint8_t)GAME_OOT && bag[0].item.id == ootSot &&
                            bag[0].pairId == mmSot;
        SOT_CHECK(oneRow, "the two Songs of Time are not ONE shared row (OoT's copy, naming MM's) both solvers credit");
        int trimmedAt = -1;
        SOT_CHECK(Combo_Logic_ComposeTrimmedCount() == 1 && Combo_Logic_ComposeTrimmedAt(0, &trimmedAt) &&
                      trimmedAt == 1,
                  "MM's own Song of Time row was not trimmed (count %d, at %d): the pair holds ONE copy",
                  Combo_Logic_ComposeTrimmedCount(), trimmedAt);
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSotFailures != 0) {
        printf("[TEST] FAIL: shared-song-of-time-pairs (%d failure(s))\n", sSotFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-song-of-time-pairs\n");
    return TEST_PASS;
}

TestResult Test_SharedSongOfTimeArrival(void) {
    printf("[TEST] shared-song-of-time-arrival: with the Shared Song of Time rule on, the song learnt in one game is "
           "known in the other at its arrival through the real shims; off, nothing crosses (#1069)\n");
    sSotFailures = 0;

    // ---- (1) OFF: nothing crosses ---------------------------------------------------
    {
        SotFreezeWorld(false);
        SotForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongOfTimeTest_SetHeld(1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        const int mmKnows = MM_SharedSongOfTimeTest_Held();
        printf("[TEST] (1) off, OoT -> MM: MM knows it %d, pool %08X (expected 0, 00000000)\n", mmKnows,
               (unsigned)(Combo_SharedFamiliesOwned() & kSotBit));
        SOT_CHECK(mmKnows == 0, "OFF: OoT's Song of Time reached MM");
        SOT_CHECK((Combo_SharedFamiliesOwned() & kSotBit) == 0u, "OFF: a harvest put the Song of Time in the pool");
    }

    // ---- (2) ON, learnt in Hyrule, known in Termina ---------------------------------
    {
        SotFreezeWorld(true);
        SotForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongOfTimeTest_SetHeld(1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamiliesOwned();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        const int mmKnows = MM_SharedSongOfTimeTest_Held();
        printf("[TEST] (2) on, OoT -> MM: pool %08X, MM knows it %d (expected %08X, 1)\n", (unsigned)afterOoT, mmKnows,
               (unsigned)kSotBit);
        SOT_CHECK((afterOoT & kSotBit) == kSotBit, "ON: OoT's harvest shim did not put its Song of Time in the pool");
        SOT_CHECK(mmKnows == 1, "ON: MM's apply shim did not teach MM the Song of Time at the arrival");
        MM_ApplySharedResources();
        SOT_CHECK(MM_SharedSongOfTimeTest_Held() == 1, "ON: a second MM arrival lost the Song of Time");
    }

    // ---- (3) ON, learnt in Termina, known in Hyrule ---------------------------------
    {
        SotFreezeWorld(true);
        SotForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSongOfTimeTest_SetHeld(1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        const int ootKnows = OoT_SharedSongOfTimeTest_Held();
        printf("[TEST] (3) on, MM -> OoT: pool %08X, OoT knows it %d (expected %08X, 1)\n",
               (unsigned)Combo_SharedFamiliesOwned(), ootKnows, (unsigned)kSotBit);
        SOT_CHECK((Combo_SharedFamiliesOwned() & kSotBit) == kSotBit,
                  "ON: MM's harvest shim did not put its Song of Time in the pool");
        SOT_CHECK(ootKnows == 1, "ON: OoT's apply shim did not teach OoT the Song of Time at the arrival");
    }

    SotForgetAll();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSotFailures != 0) {
        printf("[TEST] FAIL: shared-song-of-time-arrival (%d failure(s))\n", sSotFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-song-of-time-arrival\n");
    return TEST_PASS;
}
