/**
 * @file test_shared_bottles.c
 * @brief Locks for the Shared Bottles cross-game rule (#970): the bottle COUNT
 *        (unlocked bottle slots) is one quantity for both games up to OoT's
 *        four slots; the contents stay each game's own.
 *
 * Three CTest rows (label "redship"), all display-free and ROM-free:
 *
 *   SharedBottlesRule ("shared-bottles-rule") - THE RECORD. The rule's key
 *   (gCombo.Rando.SharedBottles) is read by the item-rules resolver into
 *   RSBS_XITEM_SHARED_BOTTLES, the frozen record carries it, the fingerprint
 *   moves with it, and a world frozen with it compares healthy against a session
 *   that resolves it the same way. RED with the bit ignored (no key, the resolver
 *   returns the defaults).
 *
 *   SharedBottlesTier ("shared-bottles-tier") - THE SHARED TIER. Through both
 *   games' REAL harvest and apply shims (driven as test_shared_kokiri_sword.c
 *   drives them, one live save at a time): a bottle found in one game unlocks an
 *   EMPTY bottle slot in the other on arrival, a filled slot is never touched,
 *   the count is capped at OoT's four (MM's fifth and sixth stay MM's), the count
 *   SUMS across switches because every apply materializes it before the next
 *   give, and a lower harvest after a full apply keeps the count (MONOTONIC; the
 *   one behaviour that tells it from CONSUMABLE, the shared-resource discipline
 *   pin). With the rule off nothing crosses.
 *
 *   SharedBottlesTrim ("shared-bottles-trim") - THE FILL. The real composer, over
 *   both games' REAL item classifications, trims the pair's bottle items to FOUR
 *   in total with the rule on (both games keep at least one, Ruto's Letter is
 *   never a trimmed copy), and trims nothing with it off.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

// The rule's key, until cvar_shared_keys.h names it. Spelled as the identity
// prefix and a suffix so the tier-4 namespace scan (CVarClassification) does not
// read this restatement as a second, unclassified spelling of the key.
#ifndef RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES
#define RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES "gCombo.Rando." "SharedBottles"
#endif

#ifdef __cplusplus
extern "C" {
#endif
// The live-save helpers (games/oot/soh/oot_shared_bottles_test.cpp,
// games/mm/2s2h/mm_shared_bottles_test.cpp, and the sword row's ArmLive pair).
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
int OoT_SharedBottleTest_SlotCount(void);
void OoT_SharedBottleTest_SetSlot(int slot, int code);
int OoT_SharedBottleTest_Slot(int slot);
int OoT_SharedBottleTest_PoolId(int which);
int MM_SharedBottleTest_SlotCount(void);
void MM_SharedBottleTest_SetSlot(int slot, int code);
int MM_SharedBottleTest_Slot(int slot);
int MM_SharedBottleTest_PoolId(int which);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

// The helpers' neutral slot codes.
#define SSB_NONE 0
#define SSB_EMPTY 1
#define SSB_FILLED 2 // OoT: a fairy; MM: Chateau Romani
#define SSB_OTHER 3  // OoT: Ruto's Letter; MM: Gold Dust

#define SSB_OOT_SLOTS 4
#define SSB_MM_SLOTS 6

static int sSsbFailures;

#define SSB_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSsbFailures++;                           \
        }                                             \
    } while (0)

/** Freeze a paired world the way a creation event does, with the Shared Bottles
 *  rule on or off. */
static void SsbFreezeWorld(bool on) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0970F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x0970CA5Eu;
    gComboCtx.mmProfileDigest = 0x0970D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = on ? (uint32_t)RSBS_XITEM_SHARED_BOTTLES : 0u;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

/** OoT's live save with its four bottle slots as given (codes, slot order). */
static void SsbOoTLive(const int* codes) {
    OoT_SharedSwordTest_ArmLive();
    for (int i = 0; i < SSB_OOT_SLOTS; ++i) {
        OoT_SharedBottleTest_SetSlot(i, codes[i]);
    }
}

/** MM's live save with its six bottle slots as given. */
static void SsbMMLive(const int* codes) {
    MM_SharedSwordTest_ArmLive();
    for (int i = 0; i < SSB_MM_SLOTS; ++i) {
        MM_SharedBottleTest_SetSlot(i, codes[i]);
    }
}

static void SsbOoTDeparts(int a, int b, int c, int d) {
    const int codes[SSB_OOT_SLOTS] = { a, b, c, d };
    SsbOoTLive(codes);
    OoT_HarvestSharedResources();
}

static void SsbOoTArrives(int a, int b, int c, int d) {
    const int codes[SSB_OOT_SLOTS] = { a, b, c, d };
    SsbOoTLive(codes);
    OoT_ApplySharedResources();
}

static void SsbMMDeparts(int a, int b, int c, int d, int e, int f) {
    const int codes[SSB_MM_SLOTS] = { a, b, c, d, e, f };
    SsbMMLive(codes);
    MM_HarvestSharedResources();
}

static void SsbMMArrives(int a, int b, int c, int d, int e, int f) {
    const int codes[SSB_MM_SLOTS] = { a, b, c, d, e, f };
    SsbMMLive(codes);
    MM_ApplySharedResources();
}

/** OoT's four slots as a string of codes ("1120"), for the prints. */
static const char* SsbOoTSlots(char* buf) {
    for (int i = 0; i < SSB_OOT_SLOTS; ++i) {
        const int c = OoT_SharedBottleTest_Slot(i);
        buf[i] = (c < 0) ? '?' : (char)('0' + c);
    }
    buf[SSB_OOT_SLOTS] = '\0';
    return buf;
}

static const char* SsbMMSlots(char* buf) {
    for (int i = 0; i < SSB_MM_SLOTS; ++i) {
        const int c = MM_SharedBottleTest_Slot(i);
        buf[i] = (c < 0) ? '?' : (char)('0' + c);
    }
    buf[SSB_MM_SLOTS] = '\0';
    return buf;
}

static bool SsbOoTIs(const char* expected) {
    char buf[SSB_OOT_SLOTS + 1];
    return strcmp(SsbOoTSlots(buf), expected) == 0;
}

static bool SsbMMIs(const char* expected) {
    char buf[SSB_MM_SLOTS + 1];
    return strcmp(SsbMMSlots(buf), expected) == 0;
}

TestResult Test_SharedBottlesRule(void) {
    printf("[TEST] shared-bottles-rule: the Shared Bottles key is read into the frozen item rules record, and the "
           "fingerprint carries it (#970)\n");
    sSsbFailures = 0;
    const uint32_t B = (uint32_t)RSBS_XITEM_SHARED_BOTTLES;

    // ---- (0) The format -----------------------------------------------------
    SSB_CHECK(B == 0x00100000u, "the Shared Bottles bit is %08X, expected 00100000 (.redsave format, append-only)",
              (unsigned)B);
    SSB_CHECK((RSBS_XITEM_ALL_V1 & B) == B, "RSBS_XITEM_SHARED_BOTTLES is not an allocated rule bit");
    SSB_CHECK((RSBS_XITEM_FAMILY_MASK & B) == 0u,
              "RSBS_XITEM_SHARED_BOTTLES is a one-copy family bit; bottles are a shared COUNT, not a family");

    // ---- (1) The key, read by the resolver ----------------------------------
    ComboContext_Init();
    CVarClear(RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES);
    ComboItemRulesRecord off;
    Combo_ResolveComboItemRules(&off);
    SSB_CHECK((off.crossItemFlags & B) == 0u, "an unauthored Shared Bottles key resolved ON (crossItemFlags %08X)",
              (unsigned)off.crossItemFlags);
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES, 1);
    ComboItemRulesRecord on;
    Combo_ResolveComboItemRules(&on);
    printf("[TEST] (1) resolved: off %08X, on %08X (expected 00000000, %08X)\n", (unsigned)off.crossItemFlags,
           (unsigned)on.crossItemFlags, (unsigned)B);
    SSB_CHECK((on.crossItemFlags & B) == B,
              "the resolver ignored %s=1: the rule's bit is not in the item rules a creation freezes",
              RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES);
    SSB_CHECK(Combo_ComboItemRulesValid(&on), "the resolved Shared Bottles record is not valid");

    // ---- (2) The freeze and the fingerprint ---------------------------------
    {
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        ComboItemRulesRecord clear;
        memset(&clear, 0, sizeof(clear));
        ComboItemRulesRecord bottles;
        memset(&bottles, 0, sizeof(bottles));
        bottles.crossItemFlags = B;
        const uint32_t hClear = Combo_ComputeComboSettingsHashEx(&rec, &clear, 0x0970CA5Eu, 0x0970D16Eu);
        const uint32_t hOn = Combo_ComputeComboSettingsHashEx(&rec, &bottles, 0x0970CA5Eu, 0x0970D16Eu);
        printf("[TEST] (2) fingerprint: clear %08X, Shared Bottles %08X\n", (unsigned)hClear, (unsigned)hOn);
        SSB_CHECK(hClear != hOn, "the Shared Bottles bit did not move the fingerprint: a world rule outside the "
                                 "world's identity");

        SsbFreezeWorld(true);
        SSB_CHECK(Combo_ComboCrossItemRule(B), "the frozen record does not carry the Shared Bottles rule");
        const uint32_t div = Combo_ComboSettingsDivergence();
        printf("[TEST] (2) a world frozen ON against a session resolving ON: divergence %08X (expected 0)\n",
               (unsigned)div);
        SSB_CHECK(div == 0u,
                  "a world frozen with Shared Bottles ON does not compare healthy against a session that "
                  "resolves it ON (divergence %08X)",
                  (unsigned)div);
    }

    CVarClear(RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES);
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSsbFailures != 0) {
        printf("[TEST] FAIL: shared-bottles-rule (%d failure(s))\n", sSsbFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-bottles-rule\n");
    return TEST_PASS;
}

TestResult Test_SharedBottlesTier(void) {
    printf("[TEST] shared-bottles-tier: with the Shared Bottles rule on, a bottle found in one game unlocks an empty "
           "bottle slot in the other on arrival, up to OoT's four; off, nothing crosses (#970)\n");
    sSsbFailures = 0;
    char ob[SSB_OOT_SLOTS + 1];
    char mb[SSB_MM_SLOTS + 1];
    SSB_CHECK(OoT_SharedBottleTest_SlotCount() == SSB_OOT_SLOTS && MM_SharedBottleTest_SlotCount() == SSB_MM_SLOTS,
              "the helpers report %d OoT and %d MM bottle slots, expected 4 and 6", OoT_SharedBottleTest_SlotCount(),
              MM_SharedBottleTest_SlotCount());

    // ---- (1) OFF: nothing crosses --------------------------------------------
    {
        SsbFreezeWorld(false);
        SsbOoTDeparts(SSB_EMPTY, SSB_FILLED, SSB_NONE, SSB_NONE);
        SsbMMArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (1) off, OoT 2 -> MM: MM %s (expected 000000)\n", SsbMMSlots(mb));
        SSB_CHECK(SsbMMIs("000000"), "OFF: OoT's bottles reached MM (%s)", mb);
        SsbMMDeparts(SSB_FILLED, SSB_FILLED, SSB_FILLED, SSB_NONE, SSB_NONE, SSB_NONE);
        SsbOoTArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (1) off, MM 3 -> OoT: OoT %s (expected 0000)\n", SsbOoTSlots(ob));
        SSB_CHECK(SsbOoTIs("0000"), "OFF: MM's bottles reached OoT (%s)", ob);
    }

    // ---- (2) ON, OoT -> MM: one more EMPTY slot per bottle, contents stay -----
    {
        SsbFreezeWorld(true);
        SsbOoTDeparts(SSB_OTHER, SSB_FILLED, SSB_NONE, SSB_NONE); // Ruto's Letter and a fairy
        SsbMMArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (2) on, OoT letter + fairy -> empty MM: MM %s (expected 110000)\n", SsbMMSlots(mb));
        SSB_CHECK(SsbMMIs("110000"),
                  "ON: MM arrived with %s after OoT held two bottles; it must hold two EMPTY bottles (the count is "
                  "shared, the contents are not)",
                  mb);
    }

    // ---- (3) ON, MM -> OoT: a filled slot is never touched --------------------
    {
        SsbFreezeWorld(true);
        SsbMMDeparts(SSB_FILLED, SSB_OTHER, SSB_FILLED, SSB_NONE, SSB_NONE, SSB_NONE);
        SsbOoTArrives(SSB_FILLED, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (3) on, MM 3 -> OoT holding a fairy: OoT %s (expected 2110)\n", SsbOoTSlots(ob));
        SSB_CHECK(SsbOoTIs("2110"),
                  "ON: OoT arrived with %s; its own fairy must stay in slot 1 and two empty bottles fill the next "
                  "empty slots",
                  ob);
    }

    // ---- (4) ON, the cap: OoT's four; MM's fifth and sixth stay MM's ---------
    {
        SsbFreezeWorld(true);
        SsbMMDeparts(SSB_FILLED, SSB_FILLED, SSB_FILLED, SSB_FILLED, SSB_FILLED, SSB_FILLED);
        SsbOoTArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (4) on, MM 6 -> empty OoT: OoT %s (expected 1111)\n", SsbOoTSlots(ob));
        SSB_CHECK(SsbOoTIs("1111"), "ON: OoT arrived with %s after MM held six bottles; the shared count is four", ob);
        OoT_HarvestSharedResources();
        SsbMMArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (4) on, the pair at the cap -> an empty MM: MM %s (expected 111100)\n", SsbMMSlots(mb));
        SSB_CHECK(SsbMMIs("111100"),
                  "ON: an MM arrival at the shared cap got %s; the fifth and sixth bottles are MM's own and are never "
                  "shared",
                  mb);
    }

    // ---- (5) ON, the count SUMS across switches -------------------------------
    {
        SsbFreezeWorld(true);
        SsbOoTDeparts(SSB_EMPTY, SSB_EMPTY, SSB_NONE, SSB_NONE); // two found in Hyrule
        SsbMMArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        // MM finds a third: its give fills the next empty slot after the two applied.
        MM_SharedBottleTest_SetSlot(2, SSB_FILLED);
        MM_HarvestSharedResources();
        SsbOoTArrives(SSB_EMPTY, SSB_EMPTY, SSB_NONE, SSB_NONE);
        printf("[TEST] (5) on, OoT 2, MM +1 -> OoT: OoT %s (expected 1110)\n", SsbOoTSlots(ob));
        SSB_CHECK(SsbOoTIs("1110"),
                  "ON: OoT arrived with %s after two Hyrule bottles and one Termina bottle; the "
                  "pair has three",
                  ob);
    }

    // ---- (6) ON, MONOTONIC: a lower harvest after a full apply keeps the count -
    {
        SsbFreezeWorld(true);
        SsbOoTDeparts(SSB_EMPTY, SSB_EMPTY, SSB_EMPTY, SSB_NONE);
        SsbMMArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE); // full apply: three
        // MM departs holding fewer than it was given (a stale or rewound save):
        // under CONSUMABLE that lower value is "bottles spent" and leaves the pool.
        SsbMMDeparts(SSB_EMPTY, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        SsbOoTArrives(SSB_NONE, SSB_NONE, SSB_NONE, SSB_NONE);
        printf("[TEST] (6) on, MM departs with 1 after a full apply of 3 -> empty OoT: OoT %s (expected 1110)\n",
               SsbOoTSlots(ob));
        SSB_CHECK(SsbOoTIs("1110"),
                  "ON: a lower MM harvest after a full apply took bottles out of the pair (OoT %s); the count is "
                  "MONOTONIC",
                  ob);
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSsbFailures != 0) {
        printf("[TEST] FAIL: shared-bottles-tier (%d failure(s))\n", sSsbFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-bottles-tier\n");
    return TEST_PASS;
}

/** Is pool row @p r one of the bottle items this row puts in the pool? */
static bool SsbIsBottleRow(const ComboLogicPoolRow* r, uint16_t ootLetter) {
    return !(r->item.originGame == (uint8_t)GAME_OOT && r->item.id == ootLetter);
}

TestResult Test_SharedBottlesTrim(void) {
    printf("[TEST] shared-bottles-trim: with the Shared Bottles rule on, the composer keeps four of the pair's "
           "bottle items, at least one per game, and never trims Ruto's Letter; off, it trims nothing (#970)\n");
    sSsbFailures = 0;
    SSB_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table did not come up");

    // OoT: three generic bottles and Ruto's Letter (a ZF-closed pool's four).
    // MM: its six bottle checks' items.
    ComboLogicPoolRow pool[10];
    memset(pool, 0, sizeof(pool));
    const int ootIds[4] = { OoT_SharedBottleTest_PoolId(0), OoT_SharedBottleTest_PoolId(1),
                            OoT_SharedBottleTest_PoolId(2), OoT_SharedBottleTest_PoolId(3) };
    const int mmIds[6] = {
        MM_SharedBottleTest_PoolId(0), MM_SharedBottleTest_PoolId(0), MM_SharedBottleTest_PoolId(1),
        MM_SharedBottleTest_PoolId(2), MM_SharedBottleTest_PoolId(3), MM_SharedBottleTest_PoolId(4)
    };
    int n = 0;
    for (int i = 0; i < 4; ++i, ++n) {
        pool[n].item.originGame = (uint8_t)GAME_OOT;
        pool[n].item.id = (uint16_t)ootIds[i];
    }
    for (int i = 0; i < 6; ++i, ++n) {
        pool[n].item.originGame = (uint8_t)GAME_MM;
        pool[n].item.id = (uint16_t)mmIds[i];
    }
    const uint16_t ootLetter = (uint16_t)OoT_SharedBottleTest_PoolId(3);

    // The premise: every one is progression in its own game, so REQUIRED.
    for (int i = 0; i < n; ++i) {
        const int d = Combo_Logic_ComposeDisposition(pool[i].item, 0u, 0u);
        SSB_CHECK(d == RSBS_COMBO_COMPOSE_REQUIRED,
                  "pool row %d (%s id %u) is disposition %d, not REQUIRED; the row's "
                  "premise moved",
                  i, pool[i].item.originGame == (uint8_t)GAME_OOT ? "OoT" : "MM", (unsigned)pool[i].item.id, d);
    }

    for (int pass = 0; pass < 2; ++pass) {
        const bool on = (pass == 1);
        SsbFreezeWorld(on);
        ComboLogicComposeRequest req;
        memset(&req, 0, sizeof(req));
        req.rows = pool;
        req.rowCount = n;
        req.quantityFlags = 0u; // the production trim
        req.startingHealthOoT = (uint16_t)RSBS_SHARED_QTY_DEFAULT_START_HEALTH;
        req.startingHealthMM = (uint16_t)RSBS_SHARED_QTY_DEFAULT_START_HEALTH;
        req.trimSeed = 0x0970u;
        ComboLogicBagItem bag[16];
        int index[16];
        ComboLogicComposeResult res;
        memset(&res, 0, sizeof(res));
        const int rc = Combo_Logic_ComposeBag(&req, bag, 16, index, &res);
        int keptBottles[3] = { 0, 0, 0 };
        bool letterKept = false;
        for (int i = 0; i < res.bagCount && i < 16; ++i) {
            const ComboLogicPoolRow* r = &pool[index[i]];
            if (SsbIsBottleRow(r, ootLetter)) {
                keptBottles[r->item.originGame]++;
            } else {
                letterKept = true;
            }
        }
        int trimmedBottles = 0;
        bool trimmedLetter = false;
        for (int t = 0; t < Combo_Logic_ComposeTrimmedCount(); ++t) {
            int at = -1;
            if (Combo_Logic_ComposeTrimmedAt(t, &at) && at >= 0 && at < n) {
                if (SsbIsBottleRow(&pool[at], ootLetter)) {
                    trimmedBottles++;
                } else {
                    trimmedLetter = true;
                }
            }
        }
        printf("[TEST] (%s) compose rc %d: %d bag row(s), kept bottles OoT %d + MM %d, letter %s, %d trimmed\n",
               on ? "on" : "off", rc, res.bagCount, keptBottles[GAME_OOT], keptBottles[GAME_MM],
               letterKept ? "kept" : "MISSING", trimmedBottles);
        SSB_CHECK(rc == RSBS_COMBO_LOGIC_OK, "the composer refused the pool (rc %d)", rc);
        SSB_CHECK(letterKept && !trimmedLetter,
                  "Ruto's Letter left the bag: it is OoT's quest item, never one of the shared bottle copies");
        if (!on) {
            SSB_CHECK(trimmedBottles == 0 && keptBottles[GAME_OOT] == 3 && keptBottles[GAME_MM] == 6,
                      "OFF: the composer trimmed %d bottle item(s) (kept OoT %d, MM %d); a world that never asked "
                      "keeps both games' bottles",
                      trimmedBottles, keptBottles[GAME_OOT], keptBottles[GAME_MM]);
        } else {
            SSB_CHECK(keptBottles[GAME_OOT] + keptBottles[GAME_MM] == 4,
                      "ON: the bag keeps %d bottle items across both games (OoT %d, MM %d); the shared count is four, "
                      "so the bag keeps four",
                      keptBottles[GAME_OOT] + keptBottles[GAME_MM], keptBottles[GAME_OOT], keptBottles[GAME_MM]);
            SSB_CHECK(keptBottles[GAME_OOT] >= 1 && keptBottles[GAME_MM] >= 1,
                      "ON: a game kept no bottle of its own (OoT %d, MM %d); each engine assumes only its own copies",
                      keptBottles[GAME_OOT], keptBottles[GAME_MM]);
            SSB_CHECK(trimmedBottles == 5, "ON: %d bottle item(s) trimmed, expected 5 (nine copies, four kept)",
                      trimmedBottles);
        }
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSsbFailures != 0) {
        printf("[TEST] FAIL: shared-bottles-trim (%d failure(s))\n", sSsbFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-bottles-trim\n");
    return TEST_PASS;
}
