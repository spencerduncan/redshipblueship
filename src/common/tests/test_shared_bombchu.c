/**
 * @file test_shared_bombchu.c
 * @brief Lock for #1061: a bombchu pack found in the other game is CREDITED to
 *        the shared bombchu count at its pickup, never crossed, so the count
 *        rises where it is found and the other game reads it from the pool.
 *
 * CTest row SharedBombchuPickup (label "redship"), dispatch
 * "shared-bombchu-pickup". Display-free and ROM-free: gComboCtx, the RAM
 * watermark table, both games' REAL harvest and apply shims, the REAL pickup
 * cores (OoT_Rando_Foreign_RecordPickup, MM_Rando_Foreign_RecordPickup) and MM's
 * REAL arrival consumer, driven through helpers that only set each game's live
 * save up and read it back (games/oot/soh/oot_shared_bombchu_test.cpp,
 * games/mm/2s2h/mm_shared_bombchu_test.cpp).
 *
 * THE DEFECT. MM caps bombchus at its BOMB BAG capacity (0 with no bag). An MM
 * bombchu pack found in Hyrule only recorded a crossing; MM's arrival applied
 * the pool against that cap and then ran MM's own give for the pack, which
 * capped the sum again: with no bag the pack's 10 vanished and the item sat in
 * its slot with 0, and once the pool filled MM's bag every crossed pack was lost.
 *
 *   (1) OoT holds 20 (bag owned), spends 5, then picks up MM's 10-pack on an OoT
 *       check: the pool and OoT's count both read 25 at once (the harvest runs
 *       before the credit, so the 5 spent are not refunded) and no crossing is
 *       recorded;
 *   (2) the report's arrival: MM with no bomb bag holds 0 and no bombchu item,
 *       and the pool keeps its 25 through MM's harvest;
 *   (3) MM with a tier-2 bomb bag (cap 30) arrives and holds 25;
 *   (4) a crossing an older save already recorded is credited when MM redeems
 *       it, not given: the pool reads 35, MM holds its cap of 30, nothing waits
 *       in MM's give queue, and OoT's next arrival holds all 35;
 *   (5) the mirror: MM holds 30, spends 10, then picks up OoT's 10-pack on an
 *       MM check: pool 30, MM 30, and no crossing for OoT.
 */

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedBombchuTest_ArmLive(int count);
void OoT_SharedBombchuTest_SetCount(int count);
int OoT_SharedBombchuTest_Count(void);
int OoT_SharedBombchuTest_PackId(void);
void MM_SharedBombchuTest_ArmLive(int bombBagTier, int count);
void MM_SharedBombchuTest_SetCount(int count);
int MM_SharedBombchuTest_Count(void);
int MM_SharedBombchuTest_HasItem(void);
int MM_SharedBombchuTest_Capacity(void);
int MM_SharedBombchuTest_PackId(void);
int MM_SharedBombchuTest_HostCheck(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
void MM_ConsumeSharedItems(void);
int OoT_Rando_Foreign_RecordPickup(uint16_t rc);
int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId);
int MM_ForeignItem_TestPendingCount(void);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void MM_ForeignItem_TestResetPending(void);
#ifdef __cplusplus
}
#endif

static int sSbFailures;

#define SB_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSbFailures++;                            \
        }                                             \
    } while (0)

/** An arbitrary OoT RandomizerCheck to host MM's pack (the placement table
 *  stores it opaquely; the reverse carve row uses the same kind of value). */
static const uint16_t kSbOoTHostCheck = 0x0123;

/** The pool's bombchu count, or -1 when it has never been shared. */
static int SbPool(void) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource((uint8_t)RSBS_SHARED_RES_BOMBCHU_COUNT, &value)) {
        return -1;
    }
    return (int)value;
}

/** Freeze a paired world the way a creation event does, and drop every watermark. */
static void SbFreezeWorld(void) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1061F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x1061CA5Eu;
    gComboCtx.mmProfileDigest = 0x1061D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettings(&rec);
}

TestResult Test_SharedBombchuPickup(void) {
    printf("[TEST] shared-bombchu-pickup: a bombchu pack found in the other game credits the shared count at its "
           "pickup, and MM's bomb-bag cap no longer eats it (#1061)\n");
    sSbFailures = 0;

    // OoT's item table answers the class owner for OoT's pack. Transient: no
    // Rando::Context is left behind for a later row of `--test all`.
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        printf("[TEST] FAIL: shared-bombchu-pickup could not bring OoT's item table up\n");
        return TEST_FAIL;
    }
    SbFreezeWorld();
    SharedItem mmPack;
    mmPack.originGame = (uint8_t)GAME_MM;
    mmPack.flags = 0;
    mmPack.id = (uint16_t)MM_SharedBombchuTest_PackId();
    SharedItem ootPack;
    ootPack.originGame = (uint8_t)GAME_OOT;
    ootPack.flags = 0;
    ootPack.id = (uint16_t)OoT_SharedBombchuTest_PackId();
    const uint16_t mmHostCheck = (uint16_t)MM_SharedBombchuTest_HostCheck();
    if (MM_SharedBombchuTest_PackId() < 0 || mmHostCheck == 0) {
        printf("[TEST] FAIL: shared-bombchu-pickup needs the single-exe MM seams\n");
        return TEST_FAIL;
    }
    // Both packs feed the shared bombchu count (true before #1061 too).
    SB_CHECK(Combo_ItemClassSharedKind(mmPack) == RSBS_SHARED_RES_BOMBCHU_COUNT,
             "MM's 10-pack feeds kind %u, expected the bombchu count", (unsigned)Combo_ItemClassSharedKind(mmPack));
    SB_CHECK(Combo_ItemClassSharedKind(ootPack) == RSBS_SHARED_RES_BOMBCHU_COUNT,
             "OoT's 10-pack feeds kind %u, expected the bombchu count", (unsigned)Combo_ItemClassSharedKind(ootPack));
    SB_CHECK(Combo_SetForeignPlacementOoT(kSbOoTHostCheck, mmPack) >= 0, "could not place MM's pack on an OoT check");
    SB_CHECK(Combo_SetForeignPlacement(mmHostCheck, ootPack) >= 0, "could not place OoT's pack on an MM check");

    // ---- (1) MM's pack picked up in Hyrule ------------------------------------
    OoT_SharedBombchuTest_ArmLive(20);
    OoT_HarvestSharedResources();       // OoT's bombchu bag gave 20; the pool learns them
    OoT_ApplySharedResources();         // an arrival: OoT owns the watermark
    OoT_SharedBombchuTest_SetCount(15); // 5 thrown
    const int recorded = OoT_Rando_Foreign_RecordPickup(kSbOoTHostCheck);
    {
        const int pool = SbPool();
        const int oot = OoT_SharedBombchuTest_Count();
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (1) OoT 20, 5 thrown, MM 10-pack picked up in Hyrule: pickup %d, pool %d, OoT %d, MM crossings "
               "%d (expected 1, 25, 25, 0)\n",
               recorded, pool, oot, crossings);
        SB_CHECK(recorded == 1, "the pickup core answered %d for a hosted pack in a paired world", recorded);
        SB_CHECK(pool == 25,
                 "the pool holds %d after the pickup, expected 25: the pack must credit the pool at its "
                 "pickup (and the 5 thrown leave it first)",
                 pool);
        SB_CHECK(oot == 25, "OoT holds %d bombchus after the pickup, expected 25: the count must rise in Hyrule", oot);
        SB_CHECK(crossings == 0, "the pickup recorded %d MM crossing(s): a counted pack must not cross", crossings);
    }
    OoT_HarvestSharedResources(); // OoT departs
    SB_CHECK(SbPool() == 25, "OoT's departure moved the pool to %d, expected 25", SbPool());

    // ---- (2) The report's arrival: MM with no bomb bag ------------------------
    MM_SharedBombchuTest_ArmLive(0, 0);
    MM_ApplySharedResources();
    MM_ConsumeSharedItems();
    {
        const int mm = MM_SharedBombchuTest_Count();
        const int item = MM_SharedBombchuTest_HasItem();
        const int pending = MM_ForeignItem_TestPendingCount();
        MM_HarvestSharedResources();
        const int pool = SbPool();
        printf("[TEST] (2) MM arrives with no bomb bag (cap %d): MM %d, item %d, queued gives %d, pool after MM's "
               "harvest %d (expected 0, 0, 0, 25)\n",
               MM_SharedBombchuTest_Capacity(), mm, item, pending, pool);
        SB_CHECK(mm == 0 && item == 0, "MM holds %d bombchus (item %d) with no bomb bag, expected 0 and no item", mm,
                 item);
        SB_CHECK(pending == 0, "MM's arrival queued %d give(s): the pack must not reach MM's capped give", pending);
        SB_CHECK(pool == 25, "the pool holds %d after a bag-less MM visit, expected 25", pool);
    }
    MM_ForeignItem_TestResetPending();

    // ---- (3) MM with a tier-2 bomb bag (cap 30) -------------------------------
    MM_SharedBombchuTest_ArmLive(2, 0);
    MM_ApplySharedResources();
    {
        const int mm = MM_SharedBombchuTest_Count();
        printf("[TEST] (3) MM arrives with a tier-2 bomb bag (cap %d): MM %d (expected 25)\n",
               MM_SharedBombchuTest_Capacity(), mm);
        SB_CHECK(mm == 25 && MM_SharedBombchuTest_HasItem() == 1, "MM holds %d bombchus at cap 30, expected 25", mm);
    }

    // ---- (4) A crossing an older save recorded is credited at redemption ------
    MM_HarvestSharedResources(); // MM departs holding 25
    SB_CHECK(Combo_RecordSharedItemCrossing(GAME_MM, mmPack.id) >= 0, "could not record the older save's crossing");
    MM_SharedBombchuTest_ArmLive(2, 25);
    MM_ApplySharedResources();
    MM_ConsumeSharedItems();
    {
        const int pool = SbPool();
        const int mm = MM_SharedBombchuTest_Count();
        const int pending = MM_ForeignItem_TestPendingCount();
        const int unredeemed = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false);
        printf("[TEST] (4) an older save's crossed MM 10-pack redeemed at an MM arrival: pool %d, MM %d, queued gives "
               "%d, unredeemed %d (expected 35, 30, 0, 0)\n",
               pool, mm, pending, unredeemed);
        SB_CHECK(pool == 35, "the pool holds %d after the redemption, expected 35: the crossed pack must credit it",
                 pool);
        SB_CHECK(mm == 30, "MM holds %d after the redemption, expected its cap of 30", mm);
        SB_CHECK(pending == 0, "the redemption queued %d give(s) into MM's capped give path", pending);
        SB_CHECK(unredeemed == 0, "%d crossing(s) left unredeemed", unredeemed);
    }
    MM_ForeignItem_TestResetPending();
    MM_HarvestSharedResources();
    SB_CHECK(SbPool() == 35, "MM's departure moved the pool to %d, expected 35 (the 5 above MM's cap stay)", SbPool());
    OoT_SharedBombchuTest_ArmLive(25);
    OoT_ApplySharedResources();
    SB_CHECK(OoT_SharedBombchuTest_Count() == 35, "OoT's next arrival holds %d, expected the pool's 35",
             OoT_SharedBombchuTest_Count());

    // ---- (5) The mirror: OoT's pack picked up in Termina ----------------------
    OoT_HarvestSharedResources();
    MM_SharedBombchuTest_ArmLive(2, 0);
    MM_ApplySharedResources();         // MM holds its cap, 30
    MM_SharedBombchuTest_SetCount(20); // 10 thrown
    const int mmRecorded = MM_Rando_Foreign_RecordPickup(mmHostCheck);
    {
        const int pool = SbPool();
        const int mm = MM_SharedBombchuTest_Count();
        const int crossings = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
        printf("[TEST] (5) MM 30, 10 thrown, OoT 10-pack picked up in Termina: pickup %d, pool %d, MM %d, OoT "
               "crossings %d (expected 1, 35, 30, 0)\n",
               mmRecorded, pool, mm, crossings);
        SB_CHECK(mmRecorded == 1, "MM's pickup core answered %d for a hosted pack in a paired world", mmRecorded);
        SB_CHECK(pool == 35, "the pool holds %d after MM's pickup, expected 35 (35 - 10 thrown + 10)", pool);
        SB_CHECK(mm == 30, "MM holds %d after the pickup, expected its cap of 30", mm);
        SB_CHECK(crossings == 0, "MM's pickup recorded %d OoT crossing(s): a counted pack must not cross", crossings);
    }

    MM_ForeignItem_TestResetPending();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSbFailures != 0) {
        printf("[TEST] FAIL: shared-bombchu-pickup (%d failure(s))\n", sSbFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-bombchu-pickup\n");
    return TEST_PASS;
}
