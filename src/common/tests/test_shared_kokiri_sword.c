/**
 * @file test_shared_kokiri_sword.c
 * @brief Lock for the Shared Kokiri Sword option (#886): with the frozen combo
 *        bit on, finding either game's Kokiri Sword lights both; with it off,
 *        neither crosses.
 *
 * CTest row SharedKokiriSword (label "redship"), dispatch "shared-kokiri-sword".
 * Display-free and ROM-free: gComboCtx, the RAM watermark table, and both
 * games' REAL harvest and apply shims (OoT_/MM_HarvestSharedResources,
 * OoT_/MM_ApplySharedResources), driven through small helpers that only set a
 * live save up and read the sword back (games/oot/soh/oot_shared_kokiri_sword_test.cpp,
 * games/mm/2s2h/mm_shared_kokiri_sword_test.cpp).
 *
 * ONE LIVE SAVE AT A TIME. Both ports overlay the same unified storage, so a
 * switch is modelled as test_triforce_hunt.c's T7 models it: the departing
 * game's harvest, then the arriving game's own save (ArmLive plus what that
 * game held), then the arriving game's apply. Each sword is read while its game
 * is the live one.
 *
 *   (0) the format: the bit is 0x02 and the kind is 20, both append-only;
 *   (1) OFF: OoT's sword does not reach MM, MM's does not reach OoT, and no
 *       slot is created (a world that never asked for the option keeps a
 *       `.redsave` byte-identical to one written before it existed);
 *   (2) ON, OoT -> MM: MM, holding no sword, gets the Kokiri Sword on B;
 *   (3) ON, MM -> OoT: MM's RAZOR Sword gives OoT the Kokiri Sword's owned bit
 *       (any MM sword counts; the pool holds 1, never MM's tier);
 *   (4) ON, MONOTONIC and never taking away: an MM apply leaves a Razor Sword
 *       alone, and an MM harvest with no sword (Takkuri stole it) does not lower
 *       the pool, so OoT keeps its sword;
 *   (5) the arming predicate follows the FROZEN bit, and nothing is armed
 *       before a world exists.
 */

#include <stdio.h>
#include <string.h>

// The two games' shim-driving helpers and the real shims they drive (each
// game's GameExports_SingleExe.cpp).
#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedSwordTest_ArmLive(void);
void OoT_SharedSwordTest_SetOwned(int owned);
int OoT_SharedSwordTest_Owned(void);
void MM_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_SetSword(int equipValue);
int MM_SharedSwordTest_Sword(void);
int MM_SharedSwordTest_BButton(void);
int MM_SharedSwordTest_ItemFor(int equipValue);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

// MM's EQUIP_VALUE_SWORD_* (games/mm/include/z64item.h), restated because this
// TU includes no game header (ADR 0002).
#define SKS_MM_SWORD_NONE 0
#define SKS_MM_SWORD_KOKIRI 1
#define SKS_MM_SWORD_RAZOR 2

static int sSksFailures;

#define SKS_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSksFailures++;                           \
        }                                             \
    } while (0)

/** The pool's value for the sword, or -1 when it has never been shared. */
static int SksPool(void) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource((uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD, &value)) {
        return -1;
    }
    return (int)value;
}

/** Freeze a paired world the way a creation event does, with the Shared Kokiri
 *  Sword bit set or clear, and drop every watermark. */
static void SksFreezeWorld(bool on) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0886F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x0886CA5Eu;
    gComboCtx.mmProfileDigest = 0x0886D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.comboFlags = on ? (uint8_t)RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD : 0u;
    Combo_FreezeComboSettings(&rec);
}

/** OoT stint holding (or not) the Kokiri Sword, then OoT departs. */
static void SksOoTDeparts(int owned) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedSwordTest_SetOwned(owned);
    OoT_HarvestSharedResources();
}

/** MM stint holding the sword at @p equipValue, then MM departs. */
static void SksMMDeparts(int equipValue) {
    MM_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_SetSword(equipValue);
    MM_HarvestSharedResources();
}

/** OoT arrives with no Kokiri Sword of its own; returns whether it holds one. */
static int SksOoTArrives(void) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedSwordTest_SetOwned(0);
    OoT_ApplySharedResources();
    return OoT_SharedSwordTest_Owned();
}

/** MM arrives holding the sword at @p equipValue; leaves MM live to read. */
static void SksMMArrives(int equipValue) {
    MM_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_SetSword(equipValue);
    MM_ApplySharedResources();
}

TestResult Test_SharedKokiriSword(void) {
    printf("[TEST] shared-kokiri-sword: with the frozen bit on, finding either game's Kokiri Sword lights both; "
           "with it off, neither crosses (#886)\n");
    sSksFailures = 0;

    // ---- (0) The format ------------------------------------------------------
    SKS_CHECK(RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD == 0x02u,
              "RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD is 0x%02X, expected 0x02 (comboFlags bits are .redsave format, "
              "append-only)",
              (unsigned)RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD);
    SKS_CHECK((unsigned)RSBS_SHARED_RES_KOKIRI_SWORD == 20u,
              "RSBS_SHARED_RES_KOKIRI_SWORD is %u, expected 20 (kinds are .redsave format, append-only)",
              (unsigned)RSBS_SHARED_RES_KOKIRI_SWORD);

    // ---- (1) OFF: neither crosses, and no slot is created --------------------
    {
        SksFreezeWorld(false);
        SksOoTDeparts(1);
        const int poolAfterOoT = SksPool();
        SksMMArrives(SKS_MM_SWORD_NONE);
        const int mmSword = MM_SharedSwordTest_Sword();
        printf("[TEST] (1) off, OoT -> MM: pool %d, MM sword %d (expected -1, 0)\n", poolAfterOoT, mmSword);
        SKS_CHECK(poolAfterOoT < 0, "OFF: OoT's harvest created a Kokiri Sword slot (value %d)", poolAfterOoT);
        SKS_CHECK(mmSword == SKS_MM_SWORD_NONE, "OFF: MM arrived holding sword %d; OoT's sword crossed", mmSword);

        SksFreezeWorld(false);
        SksMMDeparts(SKS_MM_SWORD_KOKIRI);
        const int poolAfterMM = SksPool();
        const int ootOwned = SksOoTArrives();
        printf("[TEST] (1) off, MM -> OoT: pool %d, OoT owns %d (expected -1, 0)\n", poolAfterMM, ootOwned);
        SKS_CHECK(poolAfterMM < 0, "OFF: MM's harvest created a Kokiri Sword slot (value %d)", poolAfterMM);
        SKS_CHECK(ootOwned == 0, "OFF: OoT arrived owning the Kokiri Sword; MM's sword crossed");
    }

    // ---- (2) ON, OoT -> MM ---------------------------------------------------
    {
        SksFreezeWorld(true);
        SksOoTDeparts(1);
        const int pool = SksPool();
        SksMMArrives(SKS_MM_SWORD_NONE);
        const int mmSword = MM_SharedSwordTest_Sword();
        const int mmB = MM_SharedSwordTest_BButton();
        printf("[TEST] (2) on, OoT -> MM: pool %d, MM sword %d, B 0x%02X (expected 1, 1, 0x%02X)\n", pool, mmSword,
               (unsigned)mmB, (unsigned)MM_SharedSwordTest_ItemFor(SKS_MM_SWORD_KOKIRI));
        SKS_CHECK(pool == 1, "ON: OoT's Kokiri Sword did not reach the pool (pool %d)", pool);
        SKS_CHECK(mmSword == SKS_MM_SWORD_KOKIRI, "ON: MM arrived holding sword %d, expected the Kokiri Sword (1)",
                  mmSword);
        SKS_CHECK(mmB == MM_SharedSwordTest_ItemFor(SKS_MM_SWORD_KOKIRI),
                  "ON: MM's B button holds 0x%02X, not the Kokiri Sword MM's own give equips", (unsigned)mmB);
    }

    // ---- (3) ON, MM -> OoT, from a Razor Sword --------------------------------
    {
        SksFreezeWorld(true);
        SksMMDeparts(SKS_MM_SWORD_RAZOR);
        const int pool = SksPool();
        const int ootOwned = SksOoTArrives();
        printf("[TEST] (3) on, MM (Razor) -> OoT: pool %d, OoT owns %d (expected 1, 1)\n", pool, ootOwned);
        SKS_CHECK(pool == 1, "ON: MM's Razor Sword reached the pool as %d; any MM sword is the shared 1", pool);
        SKS_CHECK(ootOwned == 1, "ON: OoT arrived without the Kokiri Sword after MM held a sword");
    }

    // ---- (4) ON, MONOTONIC and never taking away ------------------------------
    {
        SksFreezeWorld(true);
        SksOoTDeparts(1);
        SksMMArrives(SKS_MM_SWORD_RAZOR);
        const int mmSword = MM_SharedSwordTest_Sword();
        const int mmB = MM_SharedSwordTest_BButton();
        SKS_CHECK(mmSword == SKS_MM_SWORD_RAZOR && mmB == MM_SharedSwordTest_ItemFor(SKS_MM_SWORD_RAZOR),
                  "ON: an MM apply replaced the Razor Sword (sword %d, B 0x%02X)", mmSword, (unsigned)mmB);
        // Takkuri steals the sword: MM departs holding none.
        MM_SharedSwordTest_SetSword(SKS_MM_SWORD_NONE);
        MM_HarvestSharedResources();
        const int pool = SksPool();
        const int ootOwned = SksOoTArrives();
        printf("[TEST] (4) on: Razor kept %d, pool after a swordless MM harvest %d, OoT owns %d (expected 1, 1, 1)\n",
               mmSword == SKS_MM_SWORD_RAZOR ? 1 : 0, pool, ootOwned);
        SKS_CHECK(pool == 1, "ON: a swordless MM harvest lowered the pool to %d; the sword is MONOTONIC", pool);
        SKS_CHECK(ootOwned == 1, "ON: OoT lost the shared Kokiri Sword after MM's was stolen");
    }

    // ---- (5) The arming predicate ---------------------------------------------
    {
        ComboContext_Init();
        const bool unfrozen = Combo_SharedResourceKindArmed((uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD);
        SksFreezeWorld(false);
        const bool off = Combo_SharedResourceKindArmed((uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD);
        SksFreezeWorld(true);
        const bool on = Combo_SharedResourceKindArmed((uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD);
        printf("[TEST] (5) armed: unfrozen %d, bit clear %d, bit set %d (expected 0, 0, 1)\n", unfrozen ? 1 : 0,
               off ? 1 : 0, on ? 1 : 0);
        SKS_CHECK(!unfrozen && !off && on, "the Kokiri Sword kind is not armed by exactly the frozen bit");
        SKS_CHECK(Combo_SharedResourceKindArmed((uint8_t)RSBS_SHARED_RES_HOOKSHOT_TIER),
                  "arming the sword disarmed an unconditional kind");
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSksFailures != 0) {
        printf("[TEST] FAIL: shared-kokiri-sword (%d failure(s))\n", sSksFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-kokiri-sword\n");
    return TEST_PASS;
}
