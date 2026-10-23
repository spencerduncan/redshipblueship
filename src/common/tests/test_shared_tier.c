/**
 * @file test_shared_tier.c
 * @brief Lock for #1120: a far game's capacity-tier item found on a host (MM's
 *        Progressive Bomb Bag on an OoT check, OoT's Progressive Wallet on an
 *        MM check) STEPS the shared pool at its pickup and raises the finder's
 *        live tier at once, never crossing, so the far game reads the step from
 *        the pool at its arrival and nothing is given twice.
 *
 * CTest row SharedTierPickup (label "redship"), dispatch "shared-tier-pickup".
 * Display-free and ROM-free: gComboCtx, the RAM watermark table, both games'
 * REAL harvest and apply shims, the REAL pickup cores
 * (OoT_Rando_Foreign_RecordPickup, MM_Rando_Foreign_RecordPickup) and both REAL
 * arrival consumers, driven through helpers that only set each game's live save
 * up and read it back (games/oot/soh/oot_shared_tier_test.cpp,
 * games/mm/2s2h/mm_shared_tier_test.cpp, plus the shared-wallet and
 * shared-bottles rows' helpers).
 *
 * THE DEFECT. A capacity tier is a MONOTONIC shared resource, and the pickup
 * credit (#1061) admitted only the counted bombchu kind: a far game's tier item
 * fell through to Combo_RecordSharedItemCrossing, the finder's inventory did
 * not move, the far game redeemed the crossing at its next arrival, harvested
 * the tier into the pool, and the finder's NEXT arrival applied it — the bag
 * reached the operator two game switches after the chest.
 *
 *   (A) the primitive: Combo_StepSharedTier refuses a consumable, a disarmed
 *       and a zero-unit step and never touches Combo_CreditSharedResource's
 *       refusal of monotonic kinds (the discipline pin); it creates the slot,
 *       steps by one copy, absorbs a copy at the cap, raises the finder's cap to
 *       the other game's ceiling (an OoT longshot found in Termina) and steps
 *       health by four quarters a piece; the allowlist names the kinds;
 *   (B) MM's bomb bag picked up in Hyrule: OoT tier 1 holding 10 bombs, 3
 *       thrown, then the pickup: OoT tier 2 at once, the pool at 2, the 7 bombs
 *       kept (the harvest runs first), NO MM crossing; MM's bag-less arrival
 *       then holds tier 2 from the pool with nothing queued in its give path;
 *   (C) at the cap (OoT tier 3) a further copy is absorbed: pool 3, no crossing;
 *   (D) an older save's recorded bag crossing is redeemed ONCE through the
 *       credit branch: MM tier 2 -> 3, the pool 3, nothing queued, and OoT's
 *       following arrival reads exactly 3;
 *   (E) the mirror: OoT's bag picked up in Termina raises MM's tier at once with
 *       no OoT crossing, and OoT's next arrival applies exactly one step;
 *   (F) a counted pack (#1061) still credits the bombchu count and moves no tier;
 *   (G) the wallet, whose apply orders the ceiling before the rupees it bounds:
 *       MM's wallet found in Hyrule raises OoT's tier, keeps the flag, and the
 *       pooled rupees the old cap cut now land (130 under a 200 cap);
 *   (H) the wallet's mirror in Termina;
 *   (I) the Shared Bottles rule OFF: MM's bottle on an OoT check still CROSSES
 *       (nothing is lost to a disarmed kind); ON: it steps the bottle count.
 */

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedTierTest_ArmLive(void);
void OoT_SharedTierTest_SetBombBag(int tier, int bombs);
void OoT_SharedTierTest_SetBombs(int bombs);
int OoT_SharedTierTest_BombBagTier(void);
int OoT_SharedTierTest_Bombs(void);
int OoT_SharedTierTest_HasBombItem(void);
int OoT_SharedTierTest_BombBagId(void);
int OoT_SharedTierTest_WalletId(void);
void MM_SharedTierTest_ArmLive(void);
void MM_SharedTierTest_SetBombBag(int tier, int bombs);
void MM_SharedTierTest_SetBombs(int bombs);
int MM_SharedTierTest_BombBagTier(void);
int MM_SharedTierTest_Bombs(void);
int MM_SharedTierTest_HasBombItem(void);
void MM_SharedTierTest_SetWallet(int tier, int rupees);
int MM_SharedTierTest_WalletTier(void);
int MM_SharedTierTest_Rupees(void);
int MM_SharedTierTest_BombBagId(void);
int MM_SharedTierTest_WalletId(void);
int MM_SharedTierTest_BottleId(void);
int MM_SharedTierTest_HostCheck(int which);
// The shared-wallet row's OoT helpers (oot_shared_wallet_test.cpp): a RANDOMIZER
// live file with the wallet possession flag, the one world the wallet's
// flag-and-cap apply arithmetic exists for.
void OoT_SharedWalletTest_ArmLive(int tier, int rupees, int hasWallet);
void OoT_SharedWalletTest_SetRupees(int rupees);
int OoT_SharedWalletTest_HasWalletFlag(void);
int OoT_SharedWalletTest_Tier(void);
int OoT_SharedWalletTest_Rupees(void);
// The shared-bottles row's OoT helpers (oot_shared_bottles_test.cpp).
int OoT_SharedBottleTest_SlotCount(void);
void OoT_SharedBottleTest_SetSlot(int slot, int code);
int OoT_SharedBottleTest_Slot(int slot);
// The #1061 row's MM pack id (mm_shared_bombchu_test.cpp).
int MM_SharedBombchuTest_PackId(void);
int OoT_SharedBombchuTest_Count(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void OoT_ConsumeSharedItems(void);
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

static int sStFailures;

#define ST_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sStFailures++;                            \
        }                                             \
    } while (0)

/** Arbitrary OoT RandomizerChecks to host MM items (the placement table stores
 *  them opaquely; one per leg so no leg reads another's host). */
static const uint16_t kStOoTHost[6] = { 0x0131, 0x0132, 0x0133, 0x0134, 0x0135, 0x0136 };

/** The pool's value for @p kind, or -1 when it has never been shared. */
static int StPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

/** OoT's live bottle count through the shared-bottles row's slot reader. */
static int StOoTBottles(void) {
    int held = 0;
    for (int slot = 0; slot < OoT_SharedBottleTest_SlotCount(); slot++) {
        if (OoT_SharedBottleTest_Slot(slot) > 0) {
            held++;
        }
    }
    return held;
}

/** Freeze a paired world the way a creation event does, with the Shared
 *  Bottles rule on or off, and drop every watermark, outbox entry and queued
 *  MM give. */
static void StFreezeWorld(bool sharedBottles) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1120F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x1120CA5Eu;
    gComboCtx.mmProfileDigest = 0x1120D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = sharedBottles ? (uint32_t)RSBS_XITEM_SHARED_BOTTLES : 0u;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

static SharedItem StItem(GameId origin, int id) {
    SharedItem item;
    item.originGame = (uint8_t)origin;
    item.flags = 0;
    item.id = (uint16_t)id;
    return item;
}

TestResult Test_SharedTierPickup(void) {
    printf("[TEST] shared-tier-pickup: a far game's capacity tier found on a host steps the shared pool at its pickup "
           "and raises the finder's tier at once, never crossing (#1120)\n");
    sStFailures = 0;

    // OoT's item table answers the class owner for OoT's items. Transient: no
    // Rando::Context is left behind for a later row of `--test all`.
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        printf("[TEST] FAIL: shared-tier-pickup could not bring OoT's item table up\n");
        return TEST_FAIL;
    }
    if (MM_SharedTierTest_BombBagId() < 0 || MM_SharedTierTest_HostCheck(0) <= 0) {
        printf("[TEST] FAIL: shared-tier-pickup needs the single-exe MM seams\n");
        return TEST_FAIL;
    }
    const SharedItem mmBag = StItem(GAME_MM, MM_SharedTierTest_BombBagId());
    const SharedItem ootBag = StItem(GAME_OOT, OoT_SharedTierTest_BombBagId());
    const SharedItem mmWallet = StItem(GAME_MM, MM_SharedTierTest_WalletId());
    const SharedItem ootWallet = StItem(GAME_OOT, OoT_SharedTierTest_WalletId());
    const SharedItem mmBottle = StItem(GAME_MM, MM_SharedTierTest_BottleId());
    const SharedItem mmPack = StItem(GAME_MM, MM_SharedBombchuTest_PackId());
    const uint16_t mmHost0 = (uint16_t)MM_SharedTierTest_HostCheck(0);
    const uint16_t mmHost1 = (uint16_t)MM_SharedTierTest_HostCheck(1);
    const uint8_t kBag = (uint8_t)RSBS_SHARED_RES_BOMB_BAG_TIER;
    const uint8_t kBombs = (uint8_t)RSBS_SHARED_RES_BOMB_COUNT;
    const uint8_t kWallet = (uint8_t)RSBS_SHARED_RES_WALLET_TIER;
    const uint8_t kRupees = (uint8_t)RSBS_SHARED_RES_RUPEES;
    const uint8_t kBottles = (uint8_t)RSBS_SHARED_RES_BOTTLE_COUNT;

    // Both bags, both wallets and the bottle feed their kinds (true before the fix too).
    ST_CHECK(Combo_ItemClassSharedKind(mmBag) == kBag, "MM's bag feeds kind %u",
             (unsigned)Combo_ItemClassSharedKind(mmBag));
    ST_CHECK(Combo_ItemClassSharedKind(ootBag) == kBag, "OoT's bag feeds kind %u",
             (unsigned)Combo_ItemClassSharedKind(ootBag));
    ST_CHECK(Combo_ItemClassSharedKind(mmWallet) == kWallet, "MM's wallet feeds kind %u",
             (unsigned)Combo_ItemClassSharedKind(mmWallet));
    ST_CHECK(Combo_ItemClassSharedKind(ootWallet) == kWallet, "OoT's wallet feeds kind %u",
             (unsigned)Combo_ItemClassSharedKind(ootWallet));
    ST_CHECK(Combo_ItemClassSharedKind(mmBottle) == kBottles, "MM's bottle feeds kind %u",
             (unsigned)Combo_ItemClassSharedKind(mmBottle));

    // ---- (A) The primitive, over the bare pool --------------------------------
    StFreezeWorld(false);
    {
        ST_CHECK(!Combo_StepSharedTier(kBombs, 1, 40) && StPool(kBombs) == -1,
                 "the step accepted a CONSUMABLE kind (the bomb count); a step is monotonic only");
        ST_CHECK(!Combo_StepSharedTier((uint8_t)RSBS_SHARED_RES_OCARINA_TIER, 1, 2) &&
                     StPool((uint8_t)RSBS_SHARED_RES_OCARINA_TIER) == -1,
                 "the step accepted the never-armed ocarina kind");
        ST_CHECK(!Combo_StepSharedTier(kBottles, 1, 4) && StPool(kBottles) == -1,
                 "the step accepted the bottle count with the Shared Bottles rule off");
        ST_CHECK(!Combo_StepSharedTier(kBag, 0, 3) && StPool(kBag) == -1, "a zero-unit step wrote a slot");
        ST_CHECK(!Combo_CreditSharedResource(kBag, 1) && StPool(kBag) == -1,
                 "Combo_CreditSharedResource ADDED to a monotonic kind: the discipline pin is gone");

        ST_CHECK(Combo_StepSharedTier(kBag, 1, 3) && StPool(kBag) == 1,
                 "the first step did not create the slot at 1 (%d)", StPool(kBag));
        Combo_HarvestSharedResource(GAME_OOT, kBag, 2); // OoT found its own second bag meanwhile
        ST_CHECK(Combo_StepSharedTier(kBag, 1, 3) && StPool(kBag) == 3,
                 "a step over a harvested 2 reads %d, expected 3", StPool(kBag));
        ST_CHECK(Combo_StepSharedTier(kBag, 1, 3) && StPool(kBag) == 3,
                 "a step at the cap must be absorbed (true, pool unchanged): %d", StPool(kBag));

        // The pool's ceiling is the higher of the two games': MM applies the
        // hookshot with a cap of 1, but the pool must reach OoT's longshot (2)
        // from a copy found in Termina.
        const uint8_t kHook = (uint8_t)RSBS_SHARED_RES_HOOKSHOT_TIER;
        ST_CHECK(Combo_StepSharedTier(kHook, 1, 1) && StPool(kHook) == 1, "hookshot step 1 reads %d", StPool(kHook));
        ST_CHECK(Combo_StepSharedTier(kHook, 1, 1) && StPool(kHook) == 2,
                 "a second hookshot copy found under MM's cap of 1 left the pool at %d, expected OoT's 2",
                 StPool(kHook));
        ST_CHECK(Combo_StepSharedTier(kHook, 1, 1) && StPool(kHook) == 2, "a third hookshot copy moved the pool to %d",
                 StPool(kHook));

        // Health: one piece is four quarters, a container sixteen.
        const uint8_t kHealth = (uint8_t)RSBS_SHARED_RES_HEALTH_QUARTERS;
        ST_CHECK(Combo_StepSharedTier(kHealth, (uint16_t)RSBS_SHARED_QTY_UNITS_PIECE,
                                      (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS) &&
                     StPool(kHealth) == 4,
                 "a heart piece stepped the health pool to %d, expected 4 quarters", StPool(kHealth));
        ST_CHECK(Combo_StepSharedTier(kHealth, (uint16_t)RSBS_SHARED_QTY_UNITS_CONTAINER,
                                      (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS) &&
                     StPool(kHealth) == 20,
                 "a heart container stepped the health pool to %d, expected 20 quarters", StPool(kHealth));

        // The allowlist: every monotonic kind some pool item feeds, minus the
        // stick and nut tiers (OoT's grant block at tier 0 under the bag shuffles
        // is opened only by OoT's own give, so those items keep crossing).
        static const uint8_t kSteps[] = {
            RSBS_SHARED_RES_WALLET_TIER,   RSBS_SHARED_RES_HEALTH_QUARTERS, RSBS_SHARED_RES_DOUBLE_DEFENSE,
            RSBS_SHARED_RES_MAGIC_LEVEL,   RSBS_SHARED_RES_QUIVER_TIER,     RSBS_SHARED_RES_BOMB_BAG_TIER,
            RSBS_SHARED_RES_HOOKSHOT_TIER, RSBS_SHARED_RES_BOTTLE_COUNT,
        };
        static const uint8_t kNoSteps[] = {
            RSBS_SHARED_RES_STICK_TIER,    RSBS_SHARED_RES_NUT_TIER,       RSBS_SHARED_RES_OCARINA_TIER,
            RSBS_SHARED_RES_BOMBCHU_COUNT, RSBS_SHARED_RES_RUPEES,         RSBS_SHARED_RES_TRIFORCE_PIECES,
            RSBS_SHARED_RES_KOKIRI_SWORD,  RSBS_SHARED_RES_HEALTH_CURRENT,
        };
        for (size_t i = 0; i < sizeof(kSteps) / sizeof(kSteps[0]); i++) {
            ST_CHECK(Combo_SharedResourceStepsAtPickup(kSteps[i]), "kind %u is not stepped at pickup",
                     (unsigned)kSteps[i]);
        }
        for (size_t i = 0; i < sizeof(kNoSteps) / sizeof(kNoSteps[0]); i++) {
            ST_CHECK(!Combo_SharedResourceStepsAtPickup(kNoSteps[i]), "kind %u is stepped at pickup, expected not",
                     (unsigned)kNoSteps[i]);
        }
        ST_CHECK(Combo_SharedResourceCreditsAtPickup((uint8_t)RSBS_SHARED_RES_BOMBCHU_COUNT) &&
                     !Combo_SharedResourceCreditsAtPickup(kBag),
                 "the counted-pack allowlist moved");
    }

    // ---- (B) MM's bag picked up in Hyrule --------------------------------------
    StFreezeWorld(false);
    ST_CHECK(Combo_SetForeignPlacementOoT(kStOoTHost[0], mmBag) >= 0, "could not place MM's bag on an OoT check");
    OoT_SharedTierTest_ArmLive();
    OoT_SharedTierTest_SetBombBag(1, 10);
    OoT_HarvestSharedResources();   // OoT's own first bag and its 10 bombs reach the pool
    OoT_ApplySharedResources();     // an arrival: OoT owns the watermarks
    OoT_SharedTierTest_SetBombs(7); // 3 thrown
    {
        const int recorded = OoT_Rando_Foreign_RecordPickup(kStOoTHost[0]);
        const int tier = OoT_SharedTierTest_BombBagTier();
        const int bombs = OoT_SharedTierTest_Bombs();
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (B) OoT bag tier 1, 10 bombs, 3 thrown, MM's bag picked up in Hyrule: pickup %d, OoT tier %d, "
               "bombs %d, pool tier %d, pool bombs %d, MM crossings %d (expected 1, 2, 7, 2, 7, 0)\n",
               recorded, tier, bombs, StPool(kBag), StPool(kBombs), crossings);
        ST_CHECK(recorded == 1, "the pickup core answered %d for a hosted tier item in a paired world", recorded);
        ST_CHECK(tier == 2,
                 "OoT's bomb bag is tier %d after finding MM's bag, expected 2: the tier must rise where "
                 "it is found, not after a round trip through Termina",
                 tier);
        ST_CHECK(StPool(kBag) == 2, "the pool holds tier %d after the pickup, expected 2", StPool(kBag));
        ST_CHECK(bombs == 7 && StPool(kBombs) == 7,
                 "OoT holds %d bombs and the pool %d after the pickup, expected 7 and 7: the harvest runs first, so "
                 "the 3 thrown are neither refunded nor lost",
                 bombs, StPool(kBombs));
        ST_CHECK(crossings == 0, "the pickup recorded %d MM crossing(s): a credited tier must not cross", crossings);
    }
    OoT_HarvestSharedResources(); // OoT departs
    ST_CHECK(StPool(kBag) == 2, "OoT's departure moved the pool to %d, expected 2", StPool(kBag));
    MM_SharedTierTest_ArmLive(); // MM arrives with no bomb bag
    MM_ApplySharedResources();
    MM_ConsumeSharedItems();
    {
        const int tier = MM_SharedTierTest_BombBagTier();
        const int item = MM_SharedTierTest_HasBombItem();
        const int bombs = MM_SharedTierTest_Bombs();
        const int pending = MM_ForeignItem_TestPendingCount();
        const int unredeemed = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false);
        printf("[TEST] (B) MM's bag-less arrival: MM tier %d, bomb item %d, bombs %d, queued gives %d, unredeemed %d "
               "(expected 2, 1, 7, 0, 0)\n",
               tier, item, bombs, pending, unredeemed);
        ST_CHECK(tier == 2 && item == 1, "MM arrives at bag tier %d (item %d), expected the pool's 2 with the item",
                 tier, item);
        ST_CHECK(bombs == 7, "MM holds %d bombs on arrival, expected the pool's 7", bombs);
        ST_CHECK(pending == 0 && unredeemed == 0,
                 "MM's arrival queued %d give(s) with %d crossing(s) unredeemed: a credited tier reaches MM through "
                 "the pool alone, so a crossing on top would be a second step",
                 pending, unredeemed);
    }
    MM_HarvestSharedResources();
    ST_CHECK(StPool(kBag) == 2, "MM's departure moved the pool to %d, expected 2", StPool(kBag));

    // ---- (C) At the cap, a further copy is absorbed ---------------------------
    StFreezeWorld(false);
    ST_CHECK(Combo_SetForeignPlacementOoT(kStOoTHost[1], mmBag) >= 0, "could not place MM's bag on an OoT check");
    OoT_SharedTierTest_ArmLive();
    OoT_SharedTierTest_SetBombBag(3, 40);
    OoT_HarvestSharedResources();
    OoT_ApplySharedResources();
    {
        const int recorded = OoT_Rando_Foreign_RecordPickup(kStOoTHost[1]);
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (C) OoT at bag tier 3 picks MM's bag up: pickup %d, OoT tier %d, pool %d, MM crossings %d "
               "(expected 1, 3, 3, 0)\n",
               recorded, OoT_SharedTierTest_BombBagTier(), StPool(kBag), crossings);
        ST_CHECK(recorded == 1, "the pickup core answered %d at the cap", recorded);
        ST_CHECK(OoT_SharedTierTest_BombBagTier() == 3 && StPool(kBag) == 3,
                 "a copy at the cap moved tier %d / pool %d", OoT_SharedTierTest_BombBagTier(), StPool(kBag));
        ST_CHECK(crossings == 0, "a copy at the cap recorded %d crossing(s): nothing can hold it in Termina either",
                 crossings);
    }

    // ---- (D) An older save's recorded crossing is credited once ---------------
    StFreezeWorld(false);
    MM_SharedTierTest_ArmLive();
    MM_SharedTierTest_SetBombBag(2, 0);
    MM_HarvestSharedResources(); // the pool learns MM's tier 2
    ST_CHECK(Combo_RecordSharedItemCrossing(GAME_MM, mmBag.id) >= 0, "could not record the older save's crossing");
    MM_ApplySharedResources(); // MM's next arrival: apply, then the consumer
    MM_ConsumeSharedItems();
    {
        const int tier = MM_SharedTierTest_BombBagTier();
        const int pending = MM_ForeignItem_TestPendingCount();
        const int unredeemed = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false);
        printf("[TEST] (D) an older save's crossed MM bag redeemed at an MM arrival: MM tier %d, pool %d, queued gives "
               "%d, unredeemed %d (expected 3, 3, 0, 0)\n",
               tier, StPool(kBag), pending, unredeemed);
        ST_CHECK(tier == 3, "MM's tier is %d after redeeming the crossing, expected 3 (exactly one step)", tier);
        ST_CHECK(StPool(kBag) == 3, "the pool holds %d after the redemption, expected 3", StPool(kBag));
        ST_CHECK(pending == 0, "the redemption queued %d give(s): it must credit the pool, not give", pending);
        ST_CHECK(unredeemed == 0, "%d crossing(s) left unredeemed", unredeemed);
    }
    MM_ForeignItem_TestResetPending();
    MM_HarvestSharedResources();
    OoT_SharedTierTest_ArmLive();
    OoT_SharedTierTest_SetBombBag(2, 0);
    OoT_ApplySharedResources();
    OoT_ConsumeSharedItems();
    ST_CHECK(OoT_SharedTierTest_BombBagTier() == 3, "OoT's following arrival reads tier %d, expected the pool's 3",
             OoT_SharedTierTest_BombBagTier());

    // ---- (E) The mirror: OoT's bag picked up in Termina -----------------------
    StFreezeWorld(false);
    ST_CHECK(Combo_SetForeignPlacement(mmHost0, ootBag) >= 0, "could not place OoT's bag on an MM check");
    MM_SharedTierTest_ArmLive();
    MM_SharedTierTest_SetBombBag(1, 10);
    MM_HarvestSharedResources();
    MM_ApplySharedResources();
    MM_SharedTierTest_SetBombs(4); // 6 thrown
    {
        const int recorded = MM_Rando_Foreign_RecordPickup(mmHost0);
        const int tier = MM_SharedTierTest_BombBagTier();
        const int bombs = MM_SharedTierTest_Bombs();
        const int crossings = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
        printf("[TEST] (E) MM bag tier 1, 10 bombs, 6 thrown, OoT's bag picked up in Termina: pickup %d, MM tier %d, "
               "bombs %d, pool tier %d, pool bombs %d, OoT crossings %d (expected 1, 2, 4, 2, 4, 0)\n",
               recorded, tier, bombs, StPool(kBag), StPool(kBombs), crossings);
        ST_CHECK(recorded == 1, "MM's pickup core answered %d for a hosted tier item", recorded);
        ST_CHECK(tier == 2 && StPool(kBag) == 2, "MM's tier %d / pool %d after finding OoT's bag, expected 2 / 2", tier,
                 StPool(kBag));
        ST_CHECK(bombs == 4 && StPool(kBombs) == 4, "MM holds %d bombs and the pool %d, expected 4 and 4", bombs,
                 StPool(kBombs));
        ST_CHECK(crossings == 0, "MM's pickup recorded %d OoT crossing(s): a credited tier must not cross", crossings);
    }
    MM_HarvestSharedResources(); // MM departs
    OoT_SharedTierTest_ArmLive();
    OoT_SharedTierTest_SetBombBag(1, 0);
    OoT_ApplySharedResources();
    OoT_ConsumeSharedItems();
    {
        const int tier = OoT_SharedTierTest_BombBagTier();
        const int bombs = OoT_SharedTierTest_Bombs();
        printf("[TEST] (E) OoT's next arrival at tier 1: tier %d, bombs %d (expected 2, 4)\n", tier, bombs);
        ST_CHECK(tier == 2, "OoT's arrival reads tier %d, expected exactly one step (2), with no crossing to redeem",
                 tier);
        ST_CHECK(bombs == 4, "OoT's arrival holds %d bombs, expected the pool's 4", bombs);
    }

    // ---- (F) A counted pack (#1061) is unchanged ------------------------------
    ST_CHECK(Combo_SetForeignPlacementOoT(kStOoTHost[2], mmPack) >= 0, "could not place MM's pack on an OoT check");
    {
        const int recorded = OoT_Rando_Foreign_RecordPickup(kStOoTHost[2]);
        const int chus = OoT_SharedBombchuTest_Count();
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (F) MM's 10-pack picked up in Hyrule after (E): pickup %d, OoT bombchus %d, pool bombchus %d, "
               "OoT bag tier %d, MM crossings %d (expected 1, 10, 10, 2, 0)\n",
               recorded, chus, StPool((uint8_t)RSBS_SHARED_RES_BOMBCHU_COUNT), OoT_SharedTierTest_BombBagTier(),
               crossings);
        ST_CHECK(recorded == 1 && chus == 10 && StPool((uint8_t)RSBS_SHARED_RES_BOMBCHU_COUNT) == 10,
                 "the counted-pack credit changed: pickup %d, OoT %d, pool %d", recorded, chus,
                 StPool((uint8_t)RSBS_SHARED_RES_BOMBCHU_COUNT));
        ST_CHECK(OoT_SharedTierTest_BombBagTier() == 2 && StPool(kBag) == 2, "a counted pack moved a tier");
        ST_CHECK(crossings == 0, "the pack recorded %d crossing(s)", crossings);
    }

    // ---- (G) The wallet: the ceiling before the rupees it bounds --------------
    StFreezeWorld(false);
    ST_CHECK(Combo_SetForeignPlacementOoT(kStOoTHost[3], mmWallet) >= 0, "could not place MM's wallet on an OoT check");
    Combo_HarvestSharedResource(GAME_MM, kRupees, 150); // MM pooled 150 rupees
    OoT_SharedWalletTest_ArmLive(0, 50, 1);             // OoT's child wallet, cap 99, 50 held
    OoT_HarvestSharedResources();                       // an occupied slot: seeded, delta 0
    OoT_ApplySharedResources();                         // OoT holds its cap of 99; 51 stay pooled
    OoT_SharedWalletTest_SetRupees(79);                 // 20 spent
    {
        const int recorded = OoT_Rando_Foreign_RecordPickup(kStOoTHost[3]);
        const int tier = OoT_SharedWalletTest_Tier();
        const int flag = OoT_SharedWalletTest_HasWalletFlag();
        const int rupees = OoT_SharedWalletTest_Rupees();
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (G) pool 150 rupees, OoT child wallet holds 99 then spends 20, MM's wallet picked up in Hyrule: "
               "pickup %d, OoT tier %d, flag %d, rupees %d, pool tier %d, pool rupees %d, MM crossings %d (expected "
               "1, 1, 1, 130, 1, 130, 0)\n",
               recorded, tier, flag, rupees, StPool(kWallet), StPool(kRupees), crossings);
        ST_CHECK(recorded == 1, "the pickup core answered %d for MM's wallet", recorded);
        ST_CHECK(tier == 1 && StPool(kWallet) == 1,
                 "OoT's wallet tier %d / pool %d after finding MM's wallet, expected "
                 "1 / 1",
                 tier, StPool(kWallet));
        ST_CHECK(flag == 1, "the wallet possession flag is %d after the credit", flag);
        ST_CHECK(StPool(kRupees) == 130, "the rupee pool reads %d, expected 130 (150 less the 20 spent)",
                 StPool(kRupees));
        ST_CHECK(rupees == 130,
                 "OoT holds %d rupees after the credit, expected 130: the tier (cap 200) applies BEFORE the rupees it "
                 "bounds, so the pooled rupees the 99 cap cut now land",
                 rupees);
        ST_CHECK(crossings == 0, "the wallet pickup recorded %d crossing(s)", crossings);
    }

    // ---- (H) The wallet's mirror in Termina ----------------------------------
    StFreezeWorld(false);
    ST_CHECK(Combo_SetForeignPlacement(mmHost1, ootWallet) >= 0, "could not place OoT's wallet on an MM check");
    MM_SharedTierTest_ArmLive();
    MM_SharedTierTest_SetWallet(0, 30);
    MM_HarvestSharedResources();
    MM_ApplySharedResources();
    {
        const int recorded = MM_Rando_Foreign_RecordPickup(mmHost1);
        const int tier = MM_SharedTierTest_WalletTier();
        const int rupees = MM_SharedTierTest_Rupees();
        const int crossings = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
        printf("[TEST] (H) MM wallet tier 0 with 30 rupees, OoT's wallet picked up in Termina: pickup %d, MM tier %d, "
               "rupees %d, pool tier %d, OoT crossings %d (expected 1, 1, 30, 1, 0)\n",
               recorded, tier, rupees, StPool(kWallet), crossings);
        ST_CHECK(recorded == 1, "MM's pickup core answered %d for OoT's wallet", recorded);
        ST_CHECK(tier == 1 && StPool(kWallet) == 1, "MM's wallet tier %d / pool %d, expected 1 / 1", tier,
                 StPool(kWallet));
        ST_CHECK(rupees == 30, "MM holds %d rupees after the credit, expected its 30 kept", rupees);
        ST_CHECK(crossings == 0, "MM's wallet pickup recorded %d OoT crossing(s)", crossings);
    }

    // ---- (I) A disarmed kind still crosses; an armed one steps ----------------
    StFreezeWorld(false); // Shared Bottles off
    ST_CHECK(Combo_SetForeignPlacementOoT(kStOoTHost[4], mmBottle) >= 0, "could not place MM's bottle on an OoT check");
    OoT_SharedTierTest_ArmLive();
    {
        const int recorded = OoT_Rando_Foreign_RecordPickup(kStOoTHost[4]);
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (I) Shared Bottles OFF, MM's bottle picked up in Hyrule: pickup %d, OoT bottles %d, pool %d, MM "
               "crossings %d (expected 1, 0, -1, 1)\n",
               recorded, StOoTBottles(), StPool(kBottles), crossings);
        ST_CHECK(recorded == 1, "the pickup core answered %d for a disarmed kind's item", recorded);
        ST_CHECK(StOoTBottles() == 0 && StPool(kBottles) == -1,
                 "a disarmed kind's item moved OoT's bottles to %d / the pool to %d", StOoTBottles(), StPool(kBottles));
        ST_CHECK(crossings == 1, "a disarmed kind's item recorded %d crossing(s), expected 1: it must still cross",
                 crossings);
    }
    StFreezeWorld(true); // Shared Bottles on
    ST_CHECK(Combo_SetForeignPlacementOoT(kStOoTHost[5], mmBottle) >= 0, "could not place MM's bottle on an OoT check");
    OoT_SharedTierTest_ArmLive();
    OoT_SharedBottleTest_SetSlot(0, 1); // one empty bottle
    OoT_HarvestSharedResources();
    OoT_ApplySharedResources();
    {
        const int recorded = OoT_Rando_Foreign_RecordPickup(kStOoTHost[5]);
        const int crossings = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
        printf("[TEST] (I) Shared Bottles ON, OoT holds one, MM's bottle picked up in Hyrule: pickup %d, OoT bottles "
               "%d, pool %d, MM crossings %d (expected 1, 2, 2, 0)\n",
               recorded, StOoTBottles(), StPool(kBottles), crossings);
        ST_CHECK(recorded == 1, "the pickup core answered %d for an armed bottle", recorded);
        ST_CHECK(StOoTBottles() == 2 && StPool(kBottles) == 2, "OoT's bottles %d / pool %d, expected 2 / 2",
                 StOoTBottles(), StPool(kBottles));
        ST_CHECK(crossings == 0, "an armed bottle recorded %d crossing(s)", crossings);
    }

    MM_ForeignItem_TestResetPending();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sStFailures != 0) {
        printf("[TEST] FAIL: shared-tier-pickup (%d failure(s))\n", sStFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-tier-pickup\n");
    return TEST_PASS;
}
