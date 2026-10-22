/**
 * @file test_shared_shields.c
 * @brief Locks for the Shared Shields cross-game rule (#1068, OoTMM
 *        sharedShields): OoT's Hylian Shield with MM's Hero's Shield, and the
 *        two Mirror Shields, behind ONE frozen record bit.
 *
 * Two CTest rows (label "redship"), both display-free and ROM-free:
 *
 *   SharedShieldsPairs ("shared-shields-pairs") - THE CREDITING. Both games'
 *   real registrations (OoT_/MM_RegisterSharedShieldFamilies) put each game's
 *   copy of both pairs into the shared-family registry; the frozen rule arms
 *   both family slots (the rule bit for the Hylian/Hero pair, the Mirror slot for
 *   the Mirror pair); the bag's shared-pair list (Combo_SingleBag_SharedPairs,
 *   what Combo_SingleBag_Run hands the composer) names both pairs; and the real
 *   composer, over both games' REAL item classifications, makes each pair ONE
 *   RSBS_COMBO_BAG_SHARED_PAIR row both solvers credit, with the other game's
 *   REQUIRED copy trimmed. With the rule off nothing is armed and no pair exists.
 *
 *   SharedShieldsArrival ("shared-shields-arrival") - THE ARRIVAL. Through both
 *   games' REAL harvest and apply shims (driven as test_shared_kokiri_sword.c
 *   drives them, one live save at a time): a shield found in one game is given
 *   and worn in the other on arrival. MM's Mirror Shield replaces the Hero's
 *   Shield in its one slot, as MM's own give does, and a Hero's Shield never
 *   replaces a Mirror Shield; OoT owns its own shields (the Hylian/Mirror
 *   distinction) and wears a given one only when its own choice allows. With the
 *   rule off nothing crosses.
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
// Each game's production registration (GameExports_SingleExe.cpp).
void OoT_RegisterSharedShieldFamilies(void);
void MM_RegisterSharedShieldFamilies(void);
// The live-save helpers (games/oot/soh/oot_shared_shields_test.cpp,
// games/mm/2s2h/mm_shared_shields_test.cpp, and the sword row's ArmLive pair).
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_SharedShieldTest_SetOwned(int which, int owned);
int OoT_SharedShieldTest_Owned(int which);
int OoT_SharedShieldTest_Equipped(void);
void OoT_SharedShieldTest_SetAdult(int adult);
void OoT_SharedShieldTest_Wear(int equipValue);
int OoT_SharedShieldTest_PoolId(int which);
void MM_SharedShieldTest_SetShield(int equipValue);
int MM_SharedShieldTest_Shield(void);
int MM_SharedShieldTest_PoolId(int which);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

// The rows' own numbering for the two pairs.
#define SSH_HYLIAN 1
#define SSH_MIRROR 2
// MM's EQUIP_VALUE_SHIELD_* and OoT's EQUIP_VALUE_SHIELD_* (z64item.h of each
// game), restated because this TU includes no game header (ADR 0002).
#define SSH_MM_NONE 0
#define SSH_MM_HERO 1
#define SSH_MM_MIRROR 2
#define SSH_OOT_NONE 0
#define SSH_OOT_DEKU 1
#define SSH_OOT_HYLIAN 2
#define SSH_OOT_MIRROR 3

static int sSshFailures;

#define SSH_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSshFailures++;                           \
        }                                             \
    } while (0)

/** Freeze a paired world the way a creation event does, with the Shared Shields
 *  rule on or off, and register both games' real copies of both pairs. */
static void SshFreezeWorld(bool on) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1068F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x1068CA5Eu;
    gComboCtx.mmProfileDigest = 0x1068D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = on ? (uint32_t)RSBS_XITEM_SHARED_SHIELDS : 0u;
    Combo_FreezeComboSettingsEx(&rec, &rules);
    // Another row may have cleared the registry (the AllTests process runs every
    // row in one process); the production registrations are idempotent.
    OoT_RegisterSharedShieldFamilies();
    MM_RegisterSharedShieldFamilies();
}

/** Drop both games' shield members, and only those: a whole-registry clear
 *  would also drop the other families' static-init members (#1065's lens) for
 *  every later row of an AllTests process. */
static void SshUnregisterShields(void) {
    const GameId games[2] = { GAME_OOT, GAME_MM };
    for (int g = 0; g < 2; g++) {
        Combo_RegisterSharedFamilyMember(games[g], (uint32_t)RSBS_XITEM_SHARED_SHIELDS, NULL);
        Combo_RegisterSharedFamilyMember(games[g], (uint32_t)RSBS_XFAMILY_SLOT_MIRROR_SHIELD, NULL);
    }
}

/** An OoT stint: Link at @p adult, owning the Hylian / Mirror Shield as given,
 *  then OoT departs. */
static void SshOoTDeparts(int adult, int hylian, int mirror) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedShieldTest_SetAdult(adult);
    OoT_SharedShieldTest_SetOwned(SSH_HYLIAN, hylian);
    OoT_SharedShieldTest_SetOwned(SSH_MIRROR, mirror);
    OoT_HarvestSharedResources();
}

/** An MM stint holding the shield at @p equipValue, then MM departs. */
static void SshMMDeparts(int equipValue) {
    MM_SharedSwordTest_ArmLive();
    MM_SharedShieldTest_SetShield(equipValue);
    MM_HarvestSharedResources();
}

/** MM arrives holding the shield at @p equipValue; leaves MM live to read. */
static void SshMMArrives(int equipValue) {
    MM_SharedSwordTest_ArmLive();
    MM_SharedShieldTest_SetShield(equipValue);
    MM_ApplySharedResources();
}

/** OoT arrives with Link at @p adult, owning nothing but @p worn, which he
 *  wears (an OoT EQUIP_VALUE_SHIELD_*); leaves OoT live to read. */
static void SshOoTArrives(int adult, int worn) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedShieldTest_SetAdult(adult);
    OoT_SharedShieldTest_Wear(worn);
    OoT_ApplySharedResources();
}

TestResult Test_SharedShieldsPairs(void) {
    printf("[TEST] shared-shields-pairs: the Shared Shields rule arms two shared families (Hylian/Hero's, Mirror/"
           "Mirror) that both games register, and the bag makes each one row both solvers credit (#1068)\n");
    sSshFailures = 0;
    const uint32_t S = (uint32_t)RSBS_XITEM_SHARED_SHIELDS;
    const uint32_t M = (uint32_t)RSBS_XFAMILY_SLOT_MIRROR_SHIELD;
    const uint16_t ootHylian = (uint16_t)OoT_SharedShieldTest_PoolId(SSH_HYLIAN);
    const uint16_t ootMirror = (uint16_t)OoT_SharedShieldTest_PoolId(SSH_MIRROR);
    const uint16_t mmHero = (uint16_t)MM_SharedShieldTest_PoolId(SSH_HYLIAN);
    const uint16_t mmMirror = (uint16_t)MM_SharedShieldTest_PoolId(SSH_MIRROR);

    // ---- (0) The format -------------------------------------------------------
    SSH_CHECK(S == 0x02000000u && M == 0x80000000u,
              "the Shared Shields bit is %08X and the Mirror slot %08X, expected 02000000 and 80000000 (.redsave "
              "format, append-only)",
              (unsigned)S, (unsigned)M);
    SSH_CHECK((RSBS_XITEM_FAMILY_MASK & S) == S && (RSBS_XITEM_ALL_V1 & S) == S,
              "RSBS_XITEM_SHARED_SHIELDS is not an allocated family bit (ALL_V1 %08X, FAMILY_MASK %08X)",
              (unsigned)RSBS_XITEM_ALL_V1, (unsigned)RSBS_XITEM_FAMILY_MASK);
    SSH_CHECK((RSBS_XITEM_ALL_V1 & M) == 0u, "the Mirror slot must never be a rule bit");

    // ---- (1) THE REGISTRATIONS: both games, both pairs -------------------------
    {
        SshUnregisterShields();
        OoT_RegisterSharedShieldFamilies();
        MM_RegisterSharedShieldFamilies();
        uint16_t id[4] = { 0, 0, 0, 0 };
        const bool ok[4] = { Combo_SharedFamilyMemberId(GAME_OOT, S, &id[0]),
                             Combo_SharedFamilyMemberId(GAME_MM, S, &id[1]),
                             Combo_SharedFamilyMemberId(GAME_OOT, M, &id[2]),
                             Combo_SharedFamilyMemberId(GAME_MM, M, &id[3]) };
        printf("[TEST] (1) registered: OoT Hylian %d/%u, MM Hero's %d/%u, OoT Mirror %d/%u, MM Mirror %d/%u "
               "(expected 1/%u, 1/%u, 1/%u, 1/%u)\n",
               ok[0] ? 1 : 0, (unsigned)id[0], ok[1] ? 1 : 0, (unsigned)id[1], ok[2] ? 1 : 0, (unsigned)id[2],
               ok[3] ? 1 : 0, (unsigned)id[3], (unsigned)ootHylian, (unsigned)mmHero, (unsigned)ootMirror,
               (unsigned)mmMirror);
        SSH_CHECK(ok[0] && id[0] == ootHylian, "OoT did not register its Hylian Shield as the Shared Shields family");
        SSH_CHECK(ok[1] && id[1] == mmHero, "MM did not register its Hero's Shield as the Shared Shields family");
        SSH_CHECK(ok[2] && id[2] == ootMirror, "OoT did not register its Mirror Shield at the Mirror slot");
        SSH_CHECK(ok[3] && id[3] == mmMirror, "MM did not register its Mirror Shield at the Mirror slot");
    }

    // ---- (2) OFF: nothing armed, no pair ---------------------------------------
    {
        SshFreezeWorld(false);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (2) off: armed %08X, %d pair(s) (expected 00000000, 0)\n", (unsigned)Combo_SharedFamiliesArmed(),
               n);
        SSH_CHECK(Combo_SharedFamiliesArmed() == 0u && n == 0,
                  "OFF: the rule off armed %08X and built %d pair(s); a world that never asked shares nothing",
                  (unsigned)Combo_SharedFamiliesArmed(), n);
    }

    // ---- (3) ON: both slots armed, both pairs built ------------------------------
    ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
    int pairCount = 0;
    {
        SshFreezeWorld(true);
        pairCount = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (3) on: armed %08X, %d pair(s): ", (unsigned)Combo_SharedFamiliesArmed(), pairCount);
        for (int i = 0; i < pairCount; ++i) {
            printf("%s(%u, %u)", i == 0 ? "" : " ", (unsigned)pairs[i].ootId, (unsigned)pairs[i].mmId);
        }
        printf(" (expected %08X, 2: (%u, %u) (%u, %u))\n", (unsigned)(S | M), (unsigned)ootHylian, (unsigned)mmHero,
               (unsigned)ootMirror, (unsigned)mmMirror);
        SSH_CHECK(Combo_SharedFamiliesArmed() == (S | M),
                  "ON: the rule armed %08X; it must arm exactly the Hylian/Hero's family and the Mirror slot",
                  (unsigned)Combo_SharedFamiliesArmed());
        SSH_CHECK(pairCount == 2 && pairs[0].ootId == ootHylian && pairs[0].mmId == mmHero &&
                      pairs[1].ootId == ootMirror && pairs[1].mmId == mmMirror,
                  "ON: the bag's shared pairs are not (Hylian, Hero's) then (Mirror, Mirror)");
    }

    // ---- (4) THE CREDITING: the real composer over both real classifications ----
    {
        SSH_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table did not come up");
        ComboLogicPoolRow pool[4];
        memset(pool, 0, sizeof(pool));
        pool[0].item.originGame = (uint8_t)GAME_OOT;
        pool[0].item.id = ootHylian;
        pool[1].item.originGame = (uint8_t)GAME_OOT;
        pool[1].item.id = ootMirror;
        pool[2].item.originGame = (uint8_t)GAME_MM;
        pool[2].item.id = mmHero;
        pool[3].item.originGame = (uint8_t)GAME_MM;
        pool[3].item.id = mmMirror;
        int disp[4];
        for (int i = 0; i < 4; ++i) {
            disp[i] = Combo_Logic_ComposeDisposition(pool[i].item, 0u, 0u);
        }
        // The premise the expectations rest on: both Mirror Shields are progression
        // in their own game, MM's Hero's Shield too (RITYPE_MAJOR), and OoT's
        // Hylian Shield is renewable (sold in shops; OoT's IsAdvancement is false).
        printf("[TEST] (4) dispositions: OoT Hylian %d, OoT Mirror %d, MM Hero's %d, MM Mirror %d (expected %d, %d, "
               "%d, %d)\n",
               disp[0], disp[1], disp[2], disp[3], RSBS_COMBO_COMPOSE_RENEWABLE, RSBS_COMBO_COMPOSE_REQUIRED,
               RSBS_COMBO_COMPOSE_REQUIRED, RSBS_COMBO_COMPOSE_REQUIRED);
        SSH_CHECK(disp[1] == RSBS_COMBO_COMPOSE_REQUIRED && disp[2] == RSBS_COMBO_COMPOSE_REQUIRED &&
                      disp[3] == RSBS_COMBO_COMPOSE_REQUIRED,
                  "a shield the expectations treat as progression is not REQUIRED; the row's premise moved");

        ComboLogicComposeRequest req;
        memset(&req, 0, sizeof(req));
        req.rows = pool;
        req.rowCount = 4;
        req.quantityFlags = RSBS_COMBO_QUANTITY_KEEP_ALL; // no shield is a quantity; keep the trim out of it
        req.sharedPairs = pairCount > 0 ? pairs : NULL;
        req.sharedPairCount = pairCount;
        ComboLogicBagItem bag[8];
        int index[8];
        ComboLogicComposeResult res;
        memset(&res, 0, sizeof(res));
        const int rc = Combo_Logic_ComposeBag(&req, bag, 8, index, &res);
        printf("[TEST] (4) compose: rc %d, %d bag row(s), %d pair(s):", rc, res.bagCount, res.sharedPairs);
        for (int i = 0; i < res.bagCount && i < 8; ++i) {
            printf(" [%u:%u flags %04X pair %u]", (unsigned)bag[i].item.originGame, (unsigned)bag[i].item.id,
                   (unsigned)bag[i].bagFlags, (unsigned)bag[i].pairId);
        }
        printf(" (expected 0, 2, 2: [OoT Mirror pair MM Mirror] [MM Hero's pair OoT Hylian])\n");
        SSH_CHECK(rc == RSBS_COMBO_LOGIC_OK && res.sharedPairs == 2,
                  "the composer made %d shared row(s) (rc %d); both shield pairs must be one shared row each",
                  res.sharedPairs, rc);
        // The Mirror pair: OoT's row is the earlier pool position, so it is the
        // shared row, naming MM's half; MM's own copy is trimmed.
        bool mirrorRow = false;
        bool heroRow = false;
        bool plainHalf = false;
        for (int i = 0; i < res.bagCount && i < 8; ++i) {
            const bool shared = (bag[i].bagFlags & RSBS_COMBO_BAG_SHARED_PAIR) != 0u;
            if (shared && bag[i].item.originGame == (uint8_t)GAME_OOT && bag[i].item.id == ootMirror &&
                bag[i].pairId == mmMirror) {
                mirrorRow = true;
            } else if (shared && bag[i].item.originGame == (uint8_t)GAME_MM && bag[i].item.id == mmHero &&
                       bag[i].pairId == ootHylian) {
                heroRow = true;
            } else if (!shared) {
                plainHalf = true;
            }
        }
        SSH_CHECK(mirrorRow, "the two Mirror Shields are not ONE shared row (OoT's copy, naming MM's) both solvers "
                             "credit");
        SSH_CHECK(heroRow, "the Hero's Shield is not the shared row of the Hylian/Hero's pair (naming OoT's Hylian "
                           "Shield) both solvers credit");
        SSH_CHECK(!plainHalf && res.bagCount == 2,
                  "a shield half is still a plain bag row (%d row(s)); each pair must credit both games from one row",
                  res.bagCount);
        int trimmedAt = -1;
        SSH_CHECK(Combo_Logic_ComposeTrimmedCount() == 1 && Combo_Logic_ComposeTrimmedAt(0, &trimmedAt) &&
                      trimmedAt == 3,
                  "MM's own Mirror Shield row was not trimmed (count %d, at %d): the pair holds ONE copy",
                  Combo_Logic_ComposeTrimmedCount(), trimmedAt);
    }

    SshUnregisterShields();
    OoT_RegisterSharedShieldFamilies();
    MM_RegisterSharedShieldFamilies();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSshFailures != 0) {
        printf("[TEST] FAIL: shared-shields-pairs (%d failure(s))\n", sSshFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-shields-pairs\n");
    return TEST_PASS;
}

TestResult Test_SharedShieldsArrival(void) {
    printf("[TEST] shared-shields-arrival: with the Shared Shields rule on, a shield found in one game is given and "
           "worn in the other on arrival; off, nothing crosses (#1068)\n");
    sSshFailures = 0;

    // ---- (1) OFF: nothing crosses -------------------------------------------------
    {
        SshFreezeWorld(false);
        SshOoTDeparts(0, 1, 0);
        SshMMArrives(SSH_MM_NONE);
        const int mmShield = MM_SharedShieldTest_Shield();
        SshMMDeparts(SSH_MM_MIRROR);
        SshOoTArrives(1, SSH_OOT_NONE);
        const int ootMirror = OoT_SharedShieldTest_Owned(SSH_MIRROR);
        printf("[TEST] (1) off: MM shield %d, OoT owns Mirror %d, pool %08X (expected 0, 0, 00000000)\n", mmShield,
               ootMirror, (unsigned)Combo_SharedFamiliesOwned());
        SSH_CHECK(mmShield == SSH_MM_NONE, "OFF: OoT's Hylian Shield reached MM as shield %d", mmShield);
        SSH_CHECK(ootMirror == 0, "OFF: MM's Mirror Shield reached OoT");
        SSH_CHECK(Combo_SharedFamiliesOwned() == 0u, "OFF: a harvest wrote the family bitset (%08X)",
                  (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (2) ON, OoT -> MM: the Hylian Shield arrives as the Hero's Shield, worn --
    {
        SshFreezeWorld(true);
        SshOoTDeparts(0, 1, 0);
        SshMMArrives(SSH_MM_NONE);
        const int mmShield = MM_SharedShieldTest_Shield();
        printf("[TEST] (2) on, OoT Hylian -> MM: MM shield %d (expected %d, the Hero's Shield)\n", mmShield,
               SSH_MM_HERO);
        SSH_CHECK(mmShield == SSH_MM_HERO,
                  "ON: MM arrived with shield %d after OoT found the Hylian Shield; it must wear the Hero's Shield",
                  mmShield);
    }

    // ---- (3) ON, MM -> OoT: the Mirror Shield arrives in OoT, worn by an adult ----
    {
        SshFreezeWorld(true);
        SshMMDeparts(SSH_MM_MIRROR);
        SshOoTArrives(1, SSH_OOT_NONE);
        const int owns = OoT_SharedShieldTest_Owned(SSH_MIRROR);
        const int worn = OoT_SharedShieldTest_Equipped();
        printf("[TEST] (3) on, MM Mirror -> adult OoT: owns Mirror %d, wears %d (expected 1, %d)\n", owns, worn,
               SSH_OOT_MIRROR);
        SSH_CHECK(owns == 1, "ON: OoT arrived without the Mirror Shield MM found");
        SSH_CHECK(worn == SSH_OOT_MIRROR,
                  "ON: an adult with no shield of his own choice wears %d, not the Mirror "
                  "Shield the arrival gave",
                  worn);
    }

    // ---- (4) ON, OoT -> MM: the Mirror Shield replaces the Hero's Shield ---------
    {
        SshFreezeWorld(true);
        SshOoTDeparts(1, 0, 1);
        SshMMArrives(SSH_MM_HERO);
        const int mmShield = MM_SharedShieldTest_Shield();
        printf("[TEST] (4) on, OoT Mirror -> MM holding the Hero's Shield: MM shield %d (expected %d)\n", mmShield,
               SSH_MM_MIRROR);
        SSH_CHECK(mmShield == SSH_MM_MIRROR,
                  "ON: MM kept shield %d; OoT's Mirror Shield must replace the Hero's Shield in MM's one slot",
                  mmShield);
    }

    // ---- (5) ON: a Hero's Shield never replaces MM's Mirror Shield ----------------
    {
        SshFreezeWorld(true);
        SshOoTDeparts(0, 1, 0);
        SshMMArrives(SSH_MM_MIRROR);
        const int mmShield = MM_SharedShieldTest_Shield();
        printf("[TEST] (5) on, OoT Hylian -> MM holding the Mirror Shield: MM shield %d (expected %d)\n", mmShield,
               SSH_MM_MIRROR);
        SSH_CHECK(mmShield == SSH_MM_MIRROR, "ON: the shared Hylian Shield downgraded MM's Mirror Shield to %d",
                  mmShield);
    }

    // ---- (6) ON: OoT keeps its own choice; a child is never handed the Mirror ----
    {
        SshFreezeWorld(true);
        SshMMDeparts(SSH_MM_MIRROR);
        SshOoTArrives(0, SSH_OOT_DEKU);
        const int ownsHylian = OoT_SharedShieldTest_Owned(SSH_HYLIAN);
        const int ownsMirror = OoT_SharedShieldTest_Owned(SSH_MIRROR);
        const int worn = OoT_SharedShieldTest_Equipped();
        printf("[TEST] (6) on, MM Mirror -> child OoT wearing the Deku Shield: owns Hylian %d, Mirror %d, wears %d "
               "(expected 1, 1, %d)\n",
               ownsHylian, ownsMirror, worn, SSH_OOT_DEKU);
        SSH_CHECK(ownsMirror == 1, "ON: a child arrival did not own the shared Mirror Shield");
        SSH_CHECK(worn == SSH_OOT_DEKU, "ON: the arrival replaced the child's own Deku Shield with %d", worn);
    }

    // ---- (7) ON, MONOTONIC: losing a shield in MM never takes it from the pair ----
    {
        SshFreezeWorld(true);
        SshOoTDeparts(0, 1, 0);
        SshMMDeparts(SSH_MM_NONE); // a Like Like ate it
        SshMMArrives(SSH_MM_NONE);
        const int mmShield = MM_SharedShieldTest_Shield();
        printf("[TEST] (7) on, MM departs shieldless: pool %08X, MM's next arrival wears %d (expected %08X, %d)\n",
               (unsigned)Combo_SharedFamiliesOwned(), mmShield, (unsigned)RSBS_XITEM_SHARED_SHIELDS, SSH_MM_HERO);
        SSH_CHECK((Combo_SharedFamiliesOwned() & RSBS_XITEM_SHARED_SHIELDS) != 0u,
                  "ON: a shieldless MM harvest cleared the shared Hylian/Hero's Shield");
        SSH_CHECK(mmShield == SSH_MM_HERO, "ON: MM's next arrival did not get the shared Hero's Shield back (%d)",
                  mmShield);
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSshFailures != 0) {
        printf("[TEST] FAIL: shared-shields-arrival (%d failure(s))\n", sSshFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-shields-arrival\n");
    return TEST_PASS;
}
