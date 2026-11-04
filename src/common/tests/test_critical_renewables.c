/**
 * @file test_critical_renewables.c
 * @brief OoTMM's critical-renewables rule under All Locations (R6): the first copy
 *        of each critical renewable lands on a host of its own game that gives
 *        it again (src/common/critical_renewables.h).
 *
 * ROM-free and display-free. FILE SCOPE (compiled as C++ inside test_runner.cpp),
 * included AFTER test_combo_logic.c: the coordinator legs drive the same two stub
 * engines (ClEngine, ClInstall, ClBagItem ...) that file authors.
 *
 *   R1. ARMING. All Locations arms the rule; Beatable Only and No Logic do not.
 *   R2. REGISTRATION. Both games' production sources are registered and answer
 *       their own predicates (each game's test bridge asserts OoTMM's list with
 *       this port's exclusions, by real check and item id); a second, NULL or
 *       malformed source is refused and the slot keeps the first.
 *   R3. THE PREDICATES, over stub sources: pending, serves (never a crossing,
 *       never a host that does not renew), note; the Shared Shields coupling.
 *   R4. THE REMAINDERS' PLAN: the first pending copy of each kind takes the first
 *       renewing host in the pass's order, later copies are left alone, a kind
 *       with no renewing host is reported, and nothing pending plans nothing.
 *   R5. THE COORDINATOR: under All Locations with the rule asked for, a pending
 *       critical row lands on its own game's renewing host on every seed; under
 *       Beatable Only, or with the rule not asked for, the same row spreads over
 *       the hosts (the sensitivity control); a second copy is free once the first
 *       renewed; and with the kind already renewed the fill is the fill it was,
 *       digest for digest, as it is under Beatable Only with the rule asked for.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../critical_renewables.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
// Each game's source asserts its own predicate over real ids
// (ComboLogicEngineOoT.cpp, Rando/ComboLogicEngineSingleExe.cpp).
int OoT_ComboLogic_TestCriticalRenewableKinds(void);
int MM_ComboLogic_TestCriticalRenewables(void);
#ifdef __cplusplus
}
#endif

namespace {

// ----------------------------------------------------------------------------
// Stub sources over the Cl fixture's ids
// ----------------------------------------------------------------------------
//
// OoT: kOotHook is kind 0, host 12 renews. MM: kMmOcarina is kind 0, kMmMask
// kind 1, host 22 renews. Each source's "already renewed" answer is a variable
// the leg sets.

uint32_t sCrSeedOoT;
uint32_t sCrSeedMM;

int CrOoTKindOf(uint16_t id) {
    return id == kOotHook ? 0 : -1;
}
int CrOoTHostRenews(uint16_t host) {
    return host == 12 ? 1 : 0;
}
uint32_t CrOoTRenewedNow(void) {
    return sCrSeedOoT;
}
int CrMMKindOf(uint16_t id) {
    return id == kMmOcarina ? 0 : (id == kMmMask ? 1 : -1);
}
int CrMMHostRenews(uint16_t host) {
    return host == 22 ? 1 : 0;
}
uint32_t CrMMRenewedNow(void) {
    return sCrSeedMM;
}

const ComboCriticalRenewableSource kCrStubOoT = { RSBS_CRITICAL_RENEWABLE_SOURCE_ABI, CrOoTKindOf, CrOoTHostRenews,
                                                  CrOoTRenewedNow };
const ComboCriticalRenewableSource kCrStubMM = { RSBS_CRITICAL_RENEWABLE_SOURCE_ABI, CrMMKindOf, CrMMHostRenews,
                                                 CrMMRenewedNow };

SharedItem CrItem(uint8_t game, uint16_t id) {
    SharedItem s;
    memset(&s, 0, sizeof(s));
    s.originGame = game;
    s.id = id;
    return s;
}

/** Three OoT hosts and four MM hosts, every one free, both goals unconditional:
 *  every row can land anywhere, so where a row lands is the rule's doing alone. */
void CrBuildOpenWorld() {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, 0u);
    ClAddHost(&gClOoT, 12, 0u);
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, 0u);
    ClAddHost(&gClMM, 22, 0u);
    ClAddHost(&gClMM, 23, 0u);
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

int CrRunFill(const ComboLogicBagItem* bag, int bagCount, uint8_t rung, uint32_t seed, bool rule,
              ComboLogicFillResult* out) {
    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = bagCount;
    req.goal = RSBS_COMBO_GOAL_BEAT_BOTH;
    req.logicRung = rung;
    req.seed = seed;
    req.maxAttempts = 0;
    req.criticalRenewables = rule ? 1u : 0u;
    return Combo_Logic_RunFill(&req, out);
}

/** Where does the (first) placement carrying `id` of `origin` sit? */
bool CrFind(uint8_t origin, uint16_t id, uint8_t* outGame, uint16_t* outHost) {
    const GameId both[2] = { GAME_OOT, GAME_MM };
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < Combo_Logic_PlacementCount(both[s]); ++i) {
            ComboLogicPlacement p;
            if (Combo_Logic_PlacementAt(both[s], i, &p) && p.item.originGame == origin && p.item.id == id) {
                *outGame = (uint8_t)both[s];
                *outHost = p.hostCheck;
                return true;
            }
        }
    }
    return false;
}

const int kCrSeeds = 16;

TestResult CrBody() {
    // No frozen world: the Shared Shields coupling must not join the stubs' kinds
    // (MM's stub kind 0 is the Hero's Shield's index) unless a leg freezes it.
    ComboContext_Init();

    // --- R1 ----------------------------------------------------------------
    CL_ASSERT(Combo_CriticalRenewablesArmed((uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE),
              "All Locations must arm the rule (OoTMM's logic === 'allLocations')");
    CL_ASSERT(!Combo_CriticalRenewablesArmed((uint8_t)RSBS_COMBO_RUNG_BEATABLE), "Beatable Only must not arm the rule");
    CL_ASSERT(!Combo_CriticalRenewablesArmed((uint8_t)RSBS_COMBO_RUNG_NONE), "No Logic must not arm the rule");

    // --- R3: the predicates over the stubs -----------------------------------
    {
        uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = { 0u, 0u, 0u };
        const SharedItem ocarina = CrItem((uint8_t)GAME_MM, kMmOcarina);
        const SharedItem bow = CrItem((uint8_t)GAME_MM, kMmBow);
        CL_ASSERT(Combo_CriticalRenewableKind(ocarina) == 0 && Combo_CriticalRenewableKind(bow) == -1,
                  "the kind is the game source's answer");
        CL_ASSERT(Combo_CriticalRenewablePending(ocarina, renewed), "a critical kind nothing renews yet is pending");
        CL_ASSERT(!Combo_CriticalRenewablePending(bow, renewed), "a non-critical item is never pending");
        CL_ASSERT(Combo_CriticalRenewableServes((uint8_t)GAME_MM, 22, ocarina), "its own game's renewing host serves");
        CL_ASSERT(!Combo_CriticalRenewableServes((uint8_t)GAME_MM, 21, ocarina),
                  "a host that does not renew never serves");
        CL_ASSERT(!Combo_CriticalRenewableServes((uint8_t)GAME_OOT, 12, ocarina),
                  "a CROSSING never serves: a foreign item is delivered once, whatever the host");
        Combo_CriticalRenewablesNote(renewed, (uint8_t)GAME_MM, 21, ocarina);
        CL_ASSERT(Combo_CriticalRenewablePending(ocarina, renewed), "a copy on a non-renewing host renews nothing");
        Combo_CriticalRenewablesNote(renewed, (uint8_t)GAME_MM, 22, ocarina);
        CL_ASSERT(!Combo_CriticalRenewablePending(ocarina, renewed) && renewed[GAME_MM] == 1u &&
                      renewed[GAME_OOT] == 0u,
                  "a copy on a renewing host renews its kind, in its own game only");
    }
    // The Shared Shields coupling (#1068): with the rule frozen on, OoT's Hylian
    // Shield and MM's Hero's Shield renew each other; off, they do not.
    {
        for (int on = 0; on <= 1; ++on) {
            ComboContext_Init();
            ComboSettingsRecord rec;
            Combo_ComboSettingsDefaults(&rec);
            ComboItemRulesRecord rules;
            memset(&rules, 0, sizeof(rules));
            rules.crossItemFlags = on ? (uint32_t)RSBS_XITEM_SHARED_SHIELDS : 0u;
            Combo_FreezeComboSettingsEx(&rec, &rules);
            uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = { 0u, 0u, 0u };
            renewed[GAME_OOT] = 1u << RSBS_CRIT_OOT_HYLIAN_SHIELD;
            Combo_CriticalRenewablesCouple(renewed);
            const bool heroRenewed = (renewed[GAME_MM] & (1u << RSBS_CRIT_MM_HERO_SHIELD)) != 0u;
            ComboContext_Init();
            CL_ASSERT(heroRenewed == (on != 0),
                      "Shared Shields on: a renewed Hylian Shield renews the Hero's Shield; off: it does not");
        }
    }

    // --- R4: the remainders' plan -------------------------------------------
    {
        // The pass's rows and hosts, in the pass's (shuffled) order.
        const uint16_t items[] = { kMmRemains, kMmMask, kMmOcarina, kMmMask, kMmOcarina };
        const uint16_t hostsOne[] = { 20, 22, 21, 23 };
        int itemIdx[8];
        int hostIdx[8];
        uint32_t unhosted = 0u;
        uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = { 0u, 0u, 0u };
        int pairs = Combo_CriticalRenewablesPlanFirstCopies((uint8_t)GAME_MM, items, 5, hostsOne, 4, renewed, itemIdx,
                                                            hostIdx, 8, &unhosted);
        CL_ASSERT(pairs == 1 && itemIdx[0] == 1 && hostIdx[0] == 1,
                  "the first pending copy in the pass's order (the mask) takes the first renewing host (22)");
        CL_ASSERT(
            unhosted == (1u << 0),
            "the ocarina's first copy found no renewing host left, and the plan says so (OoTMM's LogicSeedError)");
        CL_ASSERT((renewed[GAME_MM] & (1u << 1)) != 0u, "the planned copy renews its kind");

        // A pass whose one renewing host comes first: the first pending kind takes
        // it and the next kind is reported.
        const uint16_t hostsTwo[] = { 22, 20 };
        memset(renewed, 0, sizeof(renewed));
        unhosted = 0u;
        pairs = Combo_CriticalRenewablesPlanFirstCopies((uint8_t)GAME_MM, items, 5, hostsTwo, 2, renewed, itemIdx,
                                                        hostIdx, 8, &unhosted);
        CL_ASSERT(pairs == 1 && unhosted == (1u << 0), "one renewing host serves one kind, whichever comes first");

        // Already renewed: nothing pending, nothing planned.
        memset(renewed, 0, sizeof(renewed));
        renewed[GAME_MM] = 0x3u;
        pairs = Combo_CriticalRenewablesPlanFirstCopies((uint8_t)GAME_MM, items, 5, hostsOne, 4, renewed, itemIdx,
                                                        hostIdx, 8, &unhosted);
        CL_ASSERT(pairs == 0 && unhosted == 0u, "a pass whose kinds are all renewed plans nothing and reports nothing");
    }

    // --- R5: the coordinator --------------------------------------------------
    ComboLogicBagItem bag[5];
    int bagCount = 0;
    bag[bagCount++] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
    bag[bagCount++] = ClBagItem((uint8_t)GAME_OOT, kOotHook, RSBS_ITEMCLASS_PROGRESSION);
    bag[bagCount++] = ClBagItem((uint8_t)GAME_MM, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
    bag[bagCount++] = ClBagItem((uint8_t)GAME_MM, kMmBow, RSBS_ITEMCLASS_PROGRESSION);
    ComboLogicFillResult res;

    // The MM kind pending, OoT's already renewed: the ocarina must take MM 22.
    sCrSeedOoT = 1u;
    sCrSeedMM = 0u;
    int beatableElsewhere = 0;
    int offElsewhere = 0;
    for (int s = 0; s < kCrSeeds; ++s) {
        const uint32_t seed = 0xC217A000u + (uint32_t)s;
        uint8_t game = 0;
        uint16_t host = 0;

        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, seed, true, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "an All Locations fill with a pending first copy and a free renewing host must succeed");
        CL_ASSERT(CrFind((uint8_t)GAME_MM, kMmOcarina, &game, &host), "the ocarina must be placed");
        CL_ASSERT(game == (uint8_t)GAME_MM && host == 22,
                  "All Locations: the pending first copy lands on its own game's renewing host, every seed");

        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, seed, true, &res) == RSBS_COMBO_LOGIC_OK,
                  "the Beatable Only fill must succeed");
        CL_ASSERT(CrFind((uint8_t)GAME_MM, kMmOcarina, &game, &host), "the ocarina must be placed (beatable)");
        beatableElsewhere += (game != (uint8_t)GAME_MM || host != 22) ? 1 : 0;

        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, seed, false, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "an All Locations fill that does not ask for the rule must succeed");
        CL_ASSERT(CrFind((uint8_t)GAME_MM, kMmOcarina, &game, &host), "the ocarina must be placed (rule off)");
        offElsewhere += (game != (uint8_t)GAME_MM || host != 22) ? 1 : 0;
    }
    CL_ASSERT(beatableElsewhere > 0, "SENSITIVITY: under Beatable Only the ocarina must land elsewhere on some seed");
    CL_ASSERT(offElsewhere > 0,
              "SENSITIVITY: with the rule not asked for the ocarina must land elsewhere on some seed");

    // OoT's kind pending, MM's renewed: the hook must take OoT 12.
    sCrSeedOoT = 0u;
    sCrSeedMM = 1u;
    for (int s = 0; s < kCrSeeds; ++s) {
        uint8_t game = 0;
        uint16_t host = 0;
        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, 0xC217B000u + (uint32_t)s, true,
                            &res) == RSBS_COMBO_LOGIC_OK,
                  "the OoT-side All Locations fill must succeed");
        CL_ASSERT(CrFind((uint8_t)GAME_OOT, kOotHook, &game, &host) && game == (uint8_t)GAME_OOT && host == 12,
                  "the rule holds for OoT's kinds too: the hook's first copy lands on OoT's renewing host");
    }

    // Only the FIRST copy: a second ocarina is free once the first renewed. Over
    // the seeds, some second copy must land off MM 22's game (a crossing) or on
    // another MM host — which it always does, 22 being taken — and some must cross.
    {
        ComboLogicBagItem two[5];
        int n = 0;
        two[n++] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        two[n++] = ClBagItem((uint8_t)GAME_MM, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
        two[n++] = ClBagItem((uint8_t)GAME_MM, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
        two[n++] = ClBagItem((uint8_t)GAME_MM, kMmBow, RSBS_ITEMCLASS_PROGRESSION);
        sCrSeedOoT = 1u;
        sCrSeedMM = 0u;
        int crossed = 0;
        for (int s = 0; s < kCrSeeds; ++s) {
            CrBuildOpenWorld();
            CL_ASSERT(CrRunFill(two, n, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, 0xC217C000u + (uint32_t)s, true,
                                &res) == RSBS_COMBO_LOGIC_OK,
                      "a fill with two copies of a critical renewable must succeed");
            ComboLogicPlacement p;
            CL_ASSERT(Combo_Logic_GetPlacement(GAME_MM, 22, &p) && p.item.originGame == (uint8_t)GAME_MM &&
                          p.item.id == kMmOcarina,
                      "one copy holds the renewing host");
            for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
                if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM &&
                    p.item.id == kMmOcarina) {
                    crossed++;
                }
            }
        }
        CL_ASSERT(crossed > 0, "the second copy is unrestricted: on some seed it crosses into OoT");
    }

    // INERT when nothing is pending: the rule asked for, the kinds renewed, is the
    // fill without the rule, digest for digest; and so is Beatable Only asked.
    sCrSeedOoT = 1u;
    sCrSeedMM = 1u;
    for (int s = 0; s < kCrSeeds; ++s) {
        const uint32_t seed = 0xC217D000u + (uint32_t)s;
        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, seed, true, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the renewed All Locations fill must succeed");
        const uint32_t withRule = res.placementDigest;
        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, seed, false, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the All Locations fill without the rule must succeed");
        CL_ASSERT(res.placementDigest == withRule,
                  "nothing pending, nothing moves: the rule must not change a world whose kinds are renewed");
    }
    sCrSeedOoT = 0u;
    sCrSeedMM = 0u;
    for (int s = 0; s < kCrSeeds; ++s) {
        const uint32_t seed = 0xC217E000u + (uint32_t)s;
        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, seed, true, &res) == RSBS_COMBO_LOGIC_OK,
                  "the Beatable Only fill asked for the rule must succeed");
        const uint32_t withRule = res.placementDigest;
        CrBuildOpenWorld();
        CL_ASSERT(CrRunFill(bag, bagCount, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, seed, false, &res) == RSBS_COMBO_LOGIC_OK,
                  "the Beatable Only fill must succeed");
        CL_ASSERT(res.placementDigest == withRule, "Beatable Only is unchanged: the rule is All Locations' alone");
    }
    CL_ASSERT(ClContractClean(), "neither stub engine saw a contract violation");
    return TEST_PASS;
}

} // namespace

TestResult Test_CriticalRenewablesRule(void) {
    printf("[TEST] critical-renewables-rule: OoTMM's critical renewables under All Locations (R6): the first copy of "
           "each lands on its own game's renewing host\n");

    // --- R2: the production sources (before any stub is swapped in) ---------
    const ComboCriticalRenewableSource* realOoT = Combo_GetCriticalRenewableSource((uint8_t)GAME_OOT);
    const ComboCriticalRenewableSource* realMM = Combo_GetCriticalRenewableSource((uint8_t)GAME_MM);
    if (realOoT == NULL || realMM == NULL) {
        printf("[TEST] FAIL: a game's critical-renewables source is not registered (OoT %s, MM %s): its registrar was "
               "elided\n",
               realOoT != NULL ? "set" : "NULL", realMM != NULL ? "set" : "NULL");
        return TEST_FAIL;
    }
    const int ootBridge = OoT_ComboLogic_TestCriticalRenewableKinds();
    const int mmBridge = MM_ComboLogic_TestCriticalRenewables();
    if (ootBridge != 0 || mmBridge != 0) {
        printf("[TEST] FAIL: a game's source does not answer OoTMM's list with this port's exclusions (OoT step %d, "
               "MM step %d)\n",
               ootBridge, mmBridge);
        return TEST_FAIL;
    }
    if (Combo_RegisterCriticalRenewableSource((uint8_t)GAME_MM, &kCrStubMM) ||
        Combo_RegisterCriticalRenewableSource((uint8_t)GAME_OOT, NULL) ||
        Combo_RegisterCriticalRenewableSource((uint8_t)GAME_NONE, &kCrStubOoT) ||
        Combo_GetCriticalRenewableSource((uint8_t)GAME_MM) != realMM) {
        printf("[TEST] FAIL: a second, NULL or non-game registration was accepted, or replaced the first\n");
        return TEST_FAIL;
    }

    // --- R1, R3-R5 over stub sources; the real ones are put back whatever happens.
    Combo_TestSwapCriticalRenewableSource((uint8_t)GAME_OOT, &kCrStubOoT);
    Combo_TestSwapCriticalRenewableSource((uint8_t)GAME_MM, &kCrStubMM);
    const TestResult result = CrBody();
    ClUninstall();
    ComboContext_Init();
    Combo_TestSwapCriticalRenewableSource((uint8_t)GAME_OOT, realOoT);
    Combo_TestSwapCriticalRenewableSource((uint8_t)GAME_MM, realMM);
    sCrSeedOoT = 0u;
    sCrSeedMM = 0u;
    if (result == TEST_PASS) {
        printf("[TEST] PASS: critical-renewables-rule\n");
    }
    return result;
}
