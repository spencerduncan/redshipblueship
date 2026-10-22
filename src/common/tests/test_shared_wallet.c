/**
 * @file test_shared_wallet.c
 * @brief Lock for #991 and #992: a wallet found in Termina IS the pair's wallet
 *        (operator ruling R2, 2026-10-19), so OoT's arrival apply sets the wallet
 *        possession flag whenever the shared tier is 1 or more, and an arrival
 *        with no wallet at all never debits the shared rupee pool.
 *
 * CTest row SharedWallet (label "redship"), dispatch "shared-wallet". Display-free
 * and ROM-free: gComboCtx, the RAM watermark table, OoT's REAL harvest and apply
 * shims, logic's REAL GetWalletCapacity over OoT's live save and the REAL
 * per-frame rupee handler, driven through small helpers that only set OoT's live
 * save up and read it back (games/oot/soh/oot_shared_wallet_test.cpp). MM's side
 * is the pool: Combo_HarvestSharedResource(GAME_MM, ...) is exactly what MM's
 * harvest offers (MM always owns its wallet).
 *
 * "Shuffle Child's Wallet on, child wallet not found" is OoT's live save with
 * RAND_INF_HAS_WALLET clear: the setting is read once, at file creation.
 *
 *   (1) #991: MM's tier-1 wallet and 50 rupees in the pool, OoT arrives with no
 *       wallet. The apply sets the flag, logic's child-wallet row agrees with
 *       the tier, and GetWalletCapacity reads the tier's capacity (200);
 *   (2) #992: the frame handler no longer zeroes the 50 and the next harvest
 *       finds nothing to debit, so the pool still holds 50, on every visit;
 *   (3) #992 at tier 0: 80 pooled rupees and no wallet anywhere. The flag stays
 *       clear (no wallet was found), OoT holds 0, the pool keeps its 80 across
 *       the frame and the harvest. Finding OoT's own child wallet afterwards,
 *       then earning 30, harvests +30 (the apply moved the watermark to OoT);
 *   (4) the reverse direction: OoT's own child wallet (flag set, tier 0) shares
 *       its rupees with the pool the way the kind always has, creates no wallet
 *       slot (MM owns that wallet already) and survives a following apply;
 *   (5) higher tiers: pool tier 2 and 3 raise OoT's tier, set the flag, and the
 *       capacity logic reads follows the tier (500, 999);
 *   (6) a VANILLA (non-randomizer) file never has the possession flag, yet the
 *       wallet kinds are armed for every world: its rupees must still apply
 *       (the zero cap is a randomizer-only rule), and a pooled wallet tier
 *       writes no randomizer-inf bit into it.
 */

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedWalletTest_ArmLive(int tier, int rupees, int hasWallet);
void OoT_SharedWalletTest_ArmLiveVanilla(int tier, int rupees);
void OoT_SharedWalletTest_GiveChildWallet(void);
void OoT_SharedWalletTest_SetRupees(int rupees);
int OoT_SharedWalletTest_HasWalletFlag(void);
int OoT_SharedWalletTest_Tier(void);
int OoT_SharedWalletTest_Rupees(void);
void OoT_SharedWalletTest_Frame(void);
int OoT_SharedWalletTest_LogicCapacity(int* childRow);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

static int sSwFailures;

#define SW_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSwFailures++;                            \
        }                                             \
    } while (0)

/** The pool's value for @p kind, or -1 when it has never been shared. */
static int SwPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

/** Freeze a paired world the way a creation event does, and drop every watermark. */
static void SwFreezeWorld(void) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0991F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x0991CA5Eu;
    gComboCtx.mmProfileDigest = 0x0991D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettings(&rec);
}

TestResult Test_SharedWallet(void) {
    printf("[TEST] shared-wallet: a wallet found in Termina is the pair's wallet; an arrival without one never debits "
           "the rupee pool (#991, #992)\n");
    sSwFailures = 0;

    // ---- (1) #991: the apply sets the flag ------------------------------------
    {
        SwFreezeWorld();
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, 1);
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 50);
        OoT_SharedWalletTest_ArmLive(0, 0, 0); // Shuffle Child's Wallet on, none found
        OoT_ApplySharedResources();
        int childRow = -1;
        const int capacity = OoT_SharedWalletTest_LogicCapacity(&childRow);
        const int flag = OoT_SharedWalletTest_HasWalletFlag();
        const int tier = OoT_SharedWalletTest_Tier();
        printf("[TEST] (1) pool tier 1 + 50 rupees, OoT arrives with no wallet: tier %d, flag %d, logic child row %d, "
               "logic capacity %d (expected 1, 1, 1, 200)\n",
               tier, flag, childRow, capacity);
        SW_CHECK(tier == 1, "the arrival apply left OoT's wallet tier at %d, expected the pool's 1", tier);
        SW_CHECK(flag == 1,
                 "OoT's wallet flag is %d after a tier-1 apply: tier 1 without the flag is a state SoH alone cannot "
                 "reach (#991)",
                 flag);
        SW_CHECK(childRow == 1, "logic reads the child wallet row as %d while the tier is 1 (#991)", childRow);
        SW_CHECK(capacity == 200, "logic's GetWalletCapacity reads %d for tier 1, expected 200", capacity);

        // ---- (2) #992: the frame handler leaves the rupees, the harvest debits nothing
        const int appliedRupees = OoT_SharedWalletTest_Rupees();
        OoT_SharedWalletTest_Frame();
        const int afterFrame = OoT_SharedWalletTest_Rupees();
        OoT_HarvestSharedResources();
        const int pool = SwPool((uint8_t)RSBS_SHARED_RES_RUPEES);
        printf("[TEST] (2) rupees applied %d, after a frame %d, pool after the harvest %d (expected 50, 50, 50)\n",
               appliedRupees, afterFrame, pool);
        SW_CHECK(appliedRupees == 50, "the apply wrote %d rupees, expected the pool's 50", appliedRupees);
        SW_CHECK(afterFrame == 50, "the frame handler zeroed the applied rupees (%d), expected 50 (#992)", afterFrame);
        SW_CHECK(pool == 50, "the pool holds %d rupees after the harvest, expected 50: the visit debited it (#992)",
                 pool);

        // A second visit repeats the arrival; the pool must not decay with it.
        OoT_SharedWalletTest_ArmLive(1, 0, 1);
        OoT_ApplySharedResources();
        OoT_SharedWalletTest_Frame();
        OoT_HarvestSharedResources();
        const int pool2 = SwPool((uint8_t)RSBS_SHARED_RES_RUPEES);
        SW_CHECK(pool2 == 50, "a second visit left %d rupees in the pool, expected 50 (#992)", pool2);
    }

    // ---- (3) #992 at tier 0: no wallet anywhere -------------------------------
    {
        SwFreezeWorld();
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 80); // MM's base wallet: no tier slot
        OoT_SharedWalletTest_ArmLive(0, 0, 0);
        OoT_ApplySharedResources();
        const int flag = OoT_SharedWalletTest_HasWalletFlag();
        const int applied = OoT_SharedWalletTest_Rupees();
        int childRow = -1;
        const int capacity = OoT_SharedWalletTest_LogicCapacity(&childRow);
        OoT_SharedWalletTest_Frame();
        OoT_HarvestSharedResources();
        const int pool = SwPool((uint8_t)RSBS_SHARED_RES_RUPEES);
        printf("[TEST] (3) pool 80 rupees, no wallet found: flag %d, OoT rupees %d, logic child row %d, capacity %d, "
               "pool after a visit %d (expected 0, 0, 0, 0, 80)\n",
               flag, applied, childRow, capacity, pool);
        SW_CHECK(flag == 0, "tier 0 set the wallet flag (%d): no wallet was found in either game", flag);
        SW_CHECK(applied == 0, "OoT holds %d rupees with no wallet, expected 0", applied);
        SW_CHECK(childRow == 0 && capacity == 0, "logic reads a wallet (row %d, capacity %d) with none found", childRow,
                 capacity);
        SW_CHECK(pool == 80, "the pool holds %d rupees after a wallet-less visit, expected 80 (#992)", pool);

        // OoT's own child wallet is found, 30 rupees are earned, OoT departs.
        OoT_SharedWalletTest_GiveChildWallet();
        OoT_SharedWalletTest_SetRupees(30);
        OoT_SharedWalletTest_Frame();
        OoT_HarvestSharedResources();
        const int pool2 = SwPool((uint8_t)RSBS_SHARED_RES_RUPEES);
        printf("[TEST] (3) then OoT's child wallet and 30 rupees earned: pool %d (expected 110)\n", pool2);
        SW_CHECK(pool2 == 110, "the pool holds %d after OoT earned 30 with its own wallet, expected 110", pool2);
    }

    // ---- (4) The reverse direction: OoT's own child wallet --------------------
    {
        SwFreezeWorld();
        OoT_SharedWalletTest_ArmLive(0, 30, 1); // child wallet found, tier 0
        OoT_HarvestSharedResources();
        const int rupees = SwPool((uint8_t)RSBS_SHARED_RES_RUPEES);
        const int tierSlot = SwPool((uint8_t)RSBS_SHARED_RES_WALLET_TIER);
        OoT_SharedWalletTest_ArmLive(0, 0, 1);
        OoT_ApplySharedResources();
        const int flag = OoT_SharedWalletTest_HasWalletFlag();
        const int back = OoT_SharedWalletTest_Rupees();
        const int capacity = OoT_SharedWalletTest_LogicCapacity(NULL);
        printf("[TEST] (4) OoT's own child wallet: pool rupees %d, tier slot %d, after the next apply flag %d, "
               "rupees %d, capacity %d (expected 30, -1, 1, 30, 99)\n",
               rupees, tierSlot, flag, back, capacity);
        SW_CHECK(rupees == 30, "OoT's own rupees reached the pool as %d, expected 30", rupees);
        SW_CHECK(tierSlot < 0, "OoT's tier-0 child wallet created a wallet-tier slot (%d)", tierSlot);
        SW_CHECK(flag == 1 && back == 30, "the next apply lost OoT's child wallet or its rupees (flag %d, rupees %d)",
                 flag, back);
        SW_CHECK(capacity == 99, "logic reads capacity %d for OoT's own child wallet, expected 99", capacity);
    }

    // ---- (5) Higher tiers -----------------------------------------------------
    {
        const int tiers[2] = { 2, 3 };
        const int expectCap[2] = { 500, 999 };
        for (int i = 0; i < 2; i++) {
            SwFreezeWorld();
            Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, (uint16_t)tiers[i]);
            OoT_SharedWalletTest_ArmLive(0, 0, 0);
            OoT_ApplySharedResources();
            int childRow = -1;
            const int capacity = OoT_SharedWalletTest_LogicCapacity(&childRow);
            const int flag = OoT_SharedWalletTest_HasWalletFlag();
            const int tier = OoT_SharedWalletTest_Tier();
            printf("[TEST] (5) pool tier %d: OoT tier %d, flag %d, child row %d, capacity %d (expected %d, 1, 1, %d)\n",
                   tiers[i], tier, flag, childRow, capacity, tiers[i], expectCap[i]);
            SW_CHECK(tier == tiers[i] && flag == 1 && childRow == 1,
                     "pool tier %d applied as tier %d, flag %d, child row %d", tiers[i], tier, flag, childRow);
            SW_CHECK(capacity == expectCap[i], "pool tier %d: logic capacity %d, expected %d", tiers[i], capacity,
                     expectCap[i]);
        }
    }

    // ---- (6) A vanilla file: the zero cap and the flag write are randomizer-only
    {
        SwFreezeWorld();
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 50);
        OoT_SharedWalletTest_ArmLiveVanilla(0, 0); // QUEST_NORMAL: flag clear, tier 0
        OoT_ApplySharedResources();
        const int applied = OoT_SharedWalletTest_Rupees();
        const int flag = OoT_SharedWalletTest_HasWalletFlag();
        printf("[TEST] (6) vanilla file, 50 pooled rupees, flag clear, tier 0: rupees applied %d, flag %d "
               "(expected 50, 0)\n",
               applied, flag);
        SW_CHECK(applied == 50,
                 "a vanilla file lost its rupees on arrival (%d applied, expected 50): the zero cap hit a "
                 "file that never has the wallet flag",
                 applied);
        SW_CHECK(flag == 0, "a vanilla file got the randomizer wallet flag (%d)", flag);

        SwFreezeWorld();
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, 1);
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 50);
        OoT_SharedWalletTest_ArmLiveVanilla(0, 0);
        OoT_ApplySharedResources();
        const int tier = OoT_SharedWalletTest_Tier();
        const int flag2 = OoT_SharedWalletTest_HasWalletFlag();
        const int applied2 = OoT_SharedWalletTest_Rupees();
        printf("[TEST] (6) vanilla file, pooled tier 1 + 50 rupees: tier %d, flag %d, rupees %d (expected 1, 0, 50)\n",
               tier, flag2, applied2);
        SW_CHECK(tier == 1, "the vanilla file's tier is %d after a pooled tier-1 apply, expected 1", tier);
        SW_CHECK(flag2 == 0, "a pooled tier wrote the randomizer wallet flag (%d) into a vanilla file", flag2);
        SW_CHECK(applied2 == 50, "the vanilla file holds %d rupees after the apply, expected 50", applied2);
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSwFailures != 0) {
        printf("[TEST] FAIL: shared-wallet (%d failure(s))\n", sSwFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-wallet\n");
    return TEST_PASS;
}
