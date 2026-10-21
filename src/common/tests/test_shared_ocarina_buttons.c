/**
 * @file test_shared_ocarina_buttons.c
 * @brief Locks for the Shared Ocarina Buttons cross-game rule (#1067, OoTMM
 *        sharedOcarinaButtons): each game's five ocarina buttons are ONE set
 *        for the pair, behind ONE frozen record bit.
 *
 * Two CTest rows (label "redship"), both display-free and ROM-free:
 *
 *   SharedOcarinaButtonsPairs ("shared-ocarina-buttons-pairs") - THE CREDITING.
 *   Both games' real registrations (OoT_/MM_RegisterSharedOcarinaButtonFamilies)
 *   put each game's copy of all five buttons into the shared-family registry;
 *   the frozen rule arms all five family slots (the rule bit for the A buttons,
 *   the four extra slots for the C buttons); the bag's shared-pair list
 *   (Combo_SingleBag_SharedPairs, what Combo_SingleBag_Run hands the composer)
 *   names all five pairs; and the real composer, over both games' REAL item
 *   classifications, makes each pair ONE RSBS_COMBO_BAG_SHARED_PAIR row both
 *   solvers credit, with the other game's REQUIRED copy trimmed. The pairs need
 *   BOTH games' own button shuffles on (OoT's seed option, MM's frozen
 *   RSBS_GIVECAP_OCARINA_BUTTONS): a shared row may be hosted in either game,
 *   and a game without the shuffle never sees a button given there, so with one
 *   shuffle off no pair is built and each game's buttons stay its own items.
 *   With the rule off nothing is armed and no pair exists.
 *
 *   SharedOcarinaButtonsArrival ("shared-ocarina-buttons-arrival") - THE
 *   ARRIVAL. Through both games' REAL harvest and apply shims (driven as
 *   test_shared_shields.c drives them, one live save at a time): a button found
 *   in one game is owned in the other on arrival, and only that button. A game
 *   whose own shuffle is off starts with all five and finds none, so it shares
 *   none. With the rule off nothing crosses.
 *
 * Buttons are numbered in OoT's order for both games: 0 A, 1 C-Up, 2 C-Down,
 * 3 C-Left, 4 C-Right (the helpers map MM's own order).
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
void OoT_RegisterSharedOcarinaButtonFamilies(void);
void MM_RegisterSharedOcarinaButtonFamilies(void);
// The live-save helpers (games/oot/soh/oot_shared_ocarina_buttons_test.cpp,
// games/mm/2s2h/mm_shared_ocarina_buttons_test.cpp, and the sword row's
// ArmLive pair).
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_SharedButtonsTest_Begin(void);
void OoT_SharedButtonsTest_End(void);
void OoT_SharedButtonsTest_Set(int shuffle, unsigned heldMask);
unsigned OoT_SharedButtonsTest_Held(void);
int OoT_SharedButtonsTest_PoolId(int button);
void MM_SharedButtonsTest_Set(int shuffle, unsigned heldMask);
unsigned MM_SharedButtonsTest_Held(void);
int MM_SharedButtonsTest_PoolId(int button);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
#ifdef __cplusplus
}
#endif

#define SOB_BUTTONS 5
#define SOB_ALL 0x1Fu
#define SOB_A 0x01u
#define SOB_C_UP 0x02u
#define SOB_C_DOWN 0x04u
#define SOB_C_LEFT 0x08u
#define SOB_C_RIGHT 0x10u

static const char* const kSobNames[SOB_BUTTONS] = { "A", "C-Up", "C-Down", "C-Left", "C-Right" };

/** The family slot of button i: the rule bit for A, an extra slot per C button. */
static uint32_t SobSlot(int button) {
    static const uint32_t kSlots[SOB_BUTTONS] = { (uint32_t)RSBS_XITEM_SHARED_OCARINA_BUTTONS,
                                                  (uint32_t)RSBS_XFAMILY_SLOT_OCARINA_C_UP,
                                                  (uint32_t)RSBS_XFAMILY_SLOT_OCARINA_C_DOWN,
                                                  (uint32_t)RSBS_XFAMILY_SLOT_OCARINA_C_LEFT,
                                                  (uint32_t)RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT };
    return kSlots[button];
}

static uint32_t SobAllSlots(void) {
    uint32_t slots = 0u;
    for (int i = 0; i < SOB_BUTTONS; ++i) {
        slots |= SobSlot(i);
    }
    return slots;
}

static int sSobFailures;

#define SOB_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSobFailures++;                           \
        }                                             \
    } while (0)

/** Freeze a paired world the way a creation event does, with the Shared Ocarina
 *  Buttons rule on or off, and register both games' real copies of all five. */
static void SobFreezeWorld(bool on) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1067F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x1067CA5Eu;
    gComboCtx.mmProfileDigest = 0x1067D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = on ? (uint32_t)RSBS_XITEM_SHARED_OCARINA_BUTTONS : 0u;
    Combo_FreezeComboSettingsEx(&rec, &rules);
    // Another row may have cleared the registry (the AllTests process runs every
    // row in one process); the production registrations are idempotent.
    OoT_RegisterSharedOcarinaButtonFamilies();
    MM_RegisterSharedOcarinaButtonFamilies();
}

/** An OoT stint under OoT's button shuffle @p shuffle owning @p held, then OoT
 *  departs. */
static void SobOoTDeparts(int shuffle, unsigned held) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedButtonsTest_Set(shuffle, held);
    OoT_HarvestSharedResources();
}

/** An MM stint under MM's button shuffle @p shuffle owning @p held, then MM
 *  departs. */
static void SobMMDeparts(int shuffle, unsigned held) {
    MM_SharedSwordTest_ArmLive();
    MM_SharedButtonsTest_Set(shuffle, held);
    MM_HarvestSharedResources();
}

/** OoT arrives owning @p held; leaves OoT live to read. */
static void SobOoTArrives(int shuffle, unsigned held) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedButtonsTest_Set(shuffle, held);
    OoT_ApplySharedResources();
}

/** MM arrives owning @p held; leaves MM live to read. */
static void SobMMArrives(int shuffle, unsigned held) {
    MM_SharedSwordTest_ArmLive();
    MM_SharedButtonsTest_Set(shuffle, held);
    MM_ApplySharedResources();
}

/** Both games' own button shuffles as the bag reads them at generation: OoT's
 *  from its seed's option (the context OoT_SharedButtonsTest_Begin holds), MM's
 *  from its frozen published give caps (@p mm < 0: MM published nothing). */
static void SobSetShuffles(int oot, int mm) {
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedButtonsTest_Set(oot, 0u);
    Combo_ClearForeignGiveCaps();
    if (mm >= 0) {
        Combo_PublishForeignGiveCaps((uint8_t)GAME_MM, mm != 0 ? (uint32_t)RSBS_GIVECAP_OCARINA_BUTTONS : 0u);
    }
}

/** The process's published give caps, put back when a row ends (the AllTests
 *  process runs every row in one process). */
typedef struct {
    bool published[2];
    uint32_t caps[2];
} SobGiveCaps;

static SobGiveCaps SobSaveGiveCaps(void) {
    SobGiveCaps g;
    for (int i = 0; i < 2; ++i) {
        const uint8_t game = i == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
        g.published[i] = Combo_ForeignGiveCapsPublished(game);
        g.caps[i] = Combo_ForeignGiveCaps(game);
    }
    return g;
}

static void SobRestoreGiveCaps(const SobGiveCaps* g) {
    Combo_ClearForeignGiveCaps();
    for (int i = 0; i < 2; ++i) {
        if (g->published[i]) {
            Combo_PublishForeignGiveCaps(i == 0 ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM, g->caps[i]);
        }
    }
}

/** Compose @p rowCount pool rows through the real composer with MM's frozen
 *  armed word holding the button shuffle (what MM's profile publishes when its
 *  shuffle is on, RSBS_GIVECAP_OCARINA_BUTTONS). */
static int SobCompose(ComboLogicPoolRow* pool, int rowCount, const ComboLogicSharedPair* pairs, int pairCount,
                      ComboLogicBagItem* bag, int bagCap, ComboLogicComposeResult* res) {
    ComboLogicComposeRequest req;
    memset(&req, 0, sizeof(req));
    req.rows = pool;
    req.rowCount = rowCount;
    req.armedMM = (uint32_t)RSBS_FILL_ARM_OCARINA_BUTTONS;
    req.quantityFlags = RSBS_COMBO_QUANTITY_KEEP_ALL; // no button is a quantity; keep the trim out of it
    req.sharedPairs = pairCount > 0 ? pairs : NULL;
    req.sharedPairCount = pairCount;
    int index[16];
    memset(res, 0, sizeof(*res));
    return Combo_Logic_ComposeBag(&req, bag, bagCap, index, res);
}

TestResult Test_SharedOcarinaButtonsPairs(void) {
    printf("[TEST] shared-ocarina-buttons-pairs: the Shared Ocarina Buttons rule arms five shared families (A and the "
           "four C buttons) that both games register, and the bag makes each one row both solvers credit (#1067)\n");
    sSobFailures = 0;
    const SobGiveCaps priorCaps = SobSaveGiveCaps();
    OoT_SharedButtonsTest_Begin();
    const uint32_t R = (uint32_t)RSBS_XITEM_SHARED_OCARINA_BUTTONS;
    uint16_t ootId[SOB_BUTTONS];
    uint16_t mmId[SOB_BUTTONS];
    for (int i = 0; i < SOB_BUTTONS; ++i) {
        ootId[i] = (uint16_t)OoT_SharedButtonsTest_PoolId(i);
        mmId[i] = (uint16_t)MM_SharedButtonsTest_PoolId(i);
    }

    // ---- (0) The format -------------------------------------------------------
    SOB_CHECK(R == 0x01000000u && RSBS_XFAMILY_SLOT_OCARINA_C_UP == 0x20u &&
                  RSBS_XFAMILY_SLOT_OCARINA_C_DOWN == 0x40u && RSBS_XFAMILY_SLOT_OCARINA_C_LEFT == 0x80u &&
                  RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT == 0x100u,
              "the Shared Ocarina Buttons bit is %08X and its C slots %08X %08X %08X %08X, expected 01000000 and 20 40 "
              "80 100 (.redsave format, append-only)",
              (unsigned)R, (unsigned)RSBS_XFAMILY_SLOT_OCARINA_C_UP, (unsigned)RSBS_XFAMILY_SLOT_OCARINA_C_DOWN,
              (unsigned)RSBS_XFAMILY_SLOT_OCARINA_C_LEFT, (unsigned)RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT);
    SOB_CHECK((RSBS_XITEM_FAMILY_MASK & R) == R && (RSBS_XITEM_ALL_V1 & R) == R,
              "RSBS_XITEM_SHARED_OCARINA_BUTTONS is not an allocated family bit (ALL_V1 %08X, FAMILY_MASK %08X)",
              (unsigned)RSBS_XITEM_ALL_V1, (unsigned)RSBS_XITEM_FAMILY_MASK);
    SOB_CHECK((SobAllSlots() & ~R & (uint32_t)RSBS_XITEM_FAMILY_MASK) == 0u &&
                  (SobAllSlots() & ~R & ~(uint32_t)RSBS_XFAMILY_SLOTS_EXTRA) == 0u,
              "a C-button slot is a family rule's bit or not an extra slot (extra slots %08X)",
              (unsigned)RSBS_XFAMILY_SLOTS_EXTRA);

    // ---- (1) THE REGISTRATIONS: both games, all five buttons --------------------
    {
        Combo_ClearSharedFamilyMembers();
        OoT_RegisterSharedOcarinaButtonFamilies();
        MM_RegisterSharedOcarinaButtonFamilies();
        for (int i = 0; i < SOB_BUTTONS; ++i) {
            uint16_t o = 0;
            uint16_t m = 0;
            const bool okO = Combo_SharedFamilyMemberId(GAME_OOT, SobSlot(i), &o);
            const bool okM = Combo_SharedFamilyMemberId(GAME_MM, SobSlot(i), &m);
            printf("[TEST] (1) %s at %08X: OoT %d/%u, MM %d/%u (expected 1/%u, 1/%u)\n", kSobNames[i],
                   (unsigned)SobSlot(i), okO ? 1 : 0, (unsigned)o, okM ? 1 : 0, (unsigned)m, (unsigned)ootId[i],
                   (unsigned)mmId[i]);
            SOB_CHECK(okO && o == ootId[i], "OoT did not register its %s button at its Shared Ocarina Buttons slot",
                      kSobNames[i]);
            SOB_CHECK(okM && m == mmId[i], "MM did not register its %s button at its Shared Ocarina Buttons slot",
                      kSobNames[i]);
        }
    }

    // ---- (2) OFF: nothing armed, no pair ---------------------------------------
    {
        SobFreezeWorld(false);
        SobSetShuffles(1, 1);
        ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
        const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (2) off: armed %08X, %d pair(s) (expected 00000000, 0)\n", (unsigned)Combo_SharedFamiliesArmed(),
               n);
        SOB_CHECK(Combo_SharedFamiliesArmed() == 0u && n == 0,
                  "OFF: the rule off armed %08X and built %d pair(s); a world that never asked shares nothing",
                  (unsigned)Combo_SharedFamiliesArmed(), n);
    }

    // ---- (3) ON, both shuffles on: all five slots armed, all five pairs built ---
    ComboLogicSharedPair pairs[RSBS_XITEM_BIT_COUNT];
    int pairCount = 0;
    {
        SobFreezeWorld(true);
        SobSetShuffles(1, 1);
        pairCount = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XITEM_BIT_COUNT);
        printf("[TEST] (3) on: armed %08X, %d pair(s):", (unsigned)Combo_SharedFamiliesArmed(), pairCount);
        for (int i = 0; i < pairCount; ++i) {
            printf(" (%u, %u)", (unsigned)pairs[i].ootId, (unsigned)pairs[i].mmId);
        }
        printf(" (expected %08X, 5)\n", (unsigned)SobAllSlots());
        SOB_CHECK(Combo_SharedFamiliesArmed() == SobAllSlots(),
                  "ON: the rule armed %08X; it must arm exactly the five button slots %08X",
                  (unsigned)Combo_SharedFamiliesArmed(), (unsigned)SobAllSlots());
        SOB_CHECK(pairCount == SOB_BUTTONS, "ON: the bag built %d shared pair(s), expected five", pairCount);
        for (int b = 0; b < SOB_BUTTONS; ++b) {
            bool found = false;
            for (int i = 0; i < pairCount; ++i) {
                found = found || (pairs[i].ootId == ootId[b] && pairs[i].mmId == mmId[b]);
            }
            SOB_CHECK(found, "ON: the bag has no shared pair (OoT %u, MM %u) for the %s buttons", (unsigned)ootId[b],
                      (unsigned)mmId[b], kSobNames[b]);
        }
    }

    // ---- (3b) ON, ONE SHUFFLE OFF: no pair --------------------------------------
    // A shared row may be hosted at a check in either game, and that game gives
    // its own copy. A game without the shuffle never sees a button given there
    // (its HELD reads nothing), so the button would never reach the game that
    // needs it, while both solvers had credited it: an unbeatable seed. So with
    // either shuffle off (or MM's profile unpublished) the bag builds no pair,
    // though the rule still arms all five slots.
    {
        static const struct {
            int oot;
            int mm;
            const char* what;
        } kOneOff[] = {
            { 1, 0, "OoT on, MM off" },
            { 0, 1, "OoT off, MM on" },
            { 0, 0, "both off" },
            { 1, -1, "OoT on, MM unpublished" },
        };
        for (int k = 0; k < (int)(sizeof(kOneOff) / sizeof(kOneOff[0])); ++k) {
            SobFreezeWorld(true);
            SobSetShuffles(kOneOff[k].oot, kOneOff[k].mm);
            ComboLogicSharedPair none[RSBS_XITEM_BIT_COUNT];
            const int n = Combo_SingleBag_SharedPairs(none, (int)RSBS_XITEM_BIT_COUNT);
            printf("[TEST] (3b) on, %s: armed %08X, %d pair(s) (expected %08X, 0)\n", kOneOff[k].what,
                   (unsigned)Combo_SharedFamiliesArmed(), n, (unsigned)SobAllSlots());
            SOB_CHECK(Combo_SharedFamiliesArmed() == SobAllSlots(), "ON, %s: the rule armed %08X, expected %08X",
                      kOneOff[k].what, (unsigned)Combo_SharedFamiliesArmed(), (unsigned)SobAllSlots());
            SOB_CHECK(n == 0,
                      "ON, %s: the bag built %d shared button pair(s); a pair hosted in the game without the "
                      "shuffle is never delivered",
                      kOneOff[k].what, n);
        }
    }

    // ---- (4) THE CREDITING: the real composer over both real classifications ----
    {
        SOB_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table did not come up");
        ComboLogicPoolRow pool[2 * SOB_BUTTONS];
        memset(pool, 0, sizeof(pool));
        for (int i = 0; i < SOB_BUTTONS; ++i) {
            pool[i].item.originGame = (uint8_t)GAME_OOT;
            pool[i].item.id = ootId[i];
            pool[SOB_BUTTONS + i].item.originGame = (uint8_t)GAME_MM;
            pool[SOB_BUTTONS + i].item.id = mmId[i];
        }
        // The premise the expectations rest on: every button is progression in
        // its own game, and MM's are REQUIRED once MM's profile arms its button
        // shuffle (they are CONFINED without it).
        for (int i = 0; i < SOB_BUTTONS; ++i) {
            const int dO = Combo_Logic_ComposeDisposition(pool[i].item, 0u, 0u);
            const int dM =
                Combo_Logic_ComposeDisposition(pool[SOB_BUTTONS + i].item, 0u, (uint32_t)RSBS_FILL_ARM_OCARINA_BUTTONS);
            printf("[TEST] (4) %s dispositions: OoT %d, MM %d (expected %d, %d)\n", kSobNames[i], dO, dM,
                   RSBS_COMBO_COMPOSE_REQUIRED, RSBS_COMBO_COMPOSE_REQUIRED);
            SOB_CHECK(dO == RSBS_COMBO_COMPOSE_REQUIRED && dM == RSBS_COMBO_COMPOSE_REQUIRED,
                      "the %s buttons are not REQUIRED in both games; the row's premise moved", kSobNames[i]);
        }

        ComboLogicBagItem bag[16];
        ComboLogicComposeResult res;
        const int rc = SobCompose(pool, 2 * SOB_BUTTONS, pairs, pairCount, bag, 16, &res);
        printf("[TEST] (4) compose: rc %d, %d bag row(s), %d pair(s), %d trimmed (expected 0, 5, 5, 5)\n", rc,
               res.bagCount, res.sharedPairs, Combo_Logic_ComposeTrimmedCount());
        SOB_CHECK(rc == RSBS_COMBO_LOGIC_OK && res.sharedPairs == SOB_BUTTONS && res.bagCount == SOB_BUTTONS,
                  "the composer made %d shared row(s) in %d bag row(s) (rc %d); each button pair must be ONE shared "
                  "row",
                  res.sharedPairs, res.bagCount, rc);
        for (int b = 0; b < SOB_BUTTONS; ++b) {
            // OoT's row is the earlier pool position, so it is the shared row,
            // naming MM's half; MM's own copy is trimmed.
            bool shared = false;
            for (int i = 0; i < res.bagCount && i < 16; ++i) {
                shared = shared || ((bag[i].bagFlags & RSBS_COMBO_BAG_SHARED_PAIR) != 0u &&
                                    bag[i].item.originGame == (uint8_t)GAME_OOT && bag[i].item.id == ootId[b] &&
                                    bag[i].pairId == mmId[b]);
            }
            SOB_CHECK(shared, "the two %s buttons are not ONE shared row (OoT's copy, naming MM's) both solvers credit",
                      kSobNames[b]);
        }
        bool mmTrimmed = Combo_Logic_ComposeTrimmedCount() == SOB_BUTTONS;
        for (int t = 0; t < Combo_Logic_ComposeTrimmedCount(); ++t) {
            int at = -1;
            mmTrimmed = mmTrimmed && Combo_Logic_ComposeTrimmedAt(t, &at) && at >= SOB_BUTTONS;
        }
        SOB_CHECK(mmTrimmed,
                  "MM's own five button rows were not the trimmed ones (%d trimmed): the pair holds ONE copy",
                  Combo_Logic_ComposeTrimmedCount());

        // ---- (4b) ONE SHUFFLE OFF: only MM's buttons are in a pool ------------
        // OoT starts with all five, so its buttons are in no pool, and (3b) built
        // no pair: MM's five rows are MM's own items, none shared, none trimmed.
        SobFreezeWorld(true);
        SobSetShuffles(0, 1);
        ComboLogicSharedPair oneOff[RSBS_XITEM_BIT_COUNT];
        const int oneOffCount = Combo_SingleBag_SharedPairs(oneOff, (int)RSBS_XITEM_BIT_COUNT);
        const int rcMM = SobCompose(pool + SOB_BUTTONS, SOB_BUTTONS, oneOff, oneOffCount, bag, 16, &res);
        int sharedMM = 0;
        for (int i = 0; i < res.bagCount && i < 16; ++i) {
            sharedMM += ((bag[i].bagFlags & RSBS_COMBO_BAG_SHARED_PAIR) != 0u) ? 1 : 0;
        }
        printf("[TEST] (4b) MM's buttons alone: rc %d, %d bag row(s), %d shared, %d trimmed (expected 0, 5, 0, 0)\n",
               rcMM, res.bagCount, sharedMM, Combo_Logic_ComposeTrimmedCount());
        SOB_CHECK(rcMM == RSBS_COMBO_LOGIC_OK && res.bagCount == SOB_BUTTONS && sharedMM == 0 &&
                      Combo_Logic_ComposeTrimmedCount() == 0,
                  "with OoT's buttons in no pool, MM's five rows must stay MM's own items, none shared, none trimmed");
    }

    Combo_ClearSharedFamilyMembers();
    OoT_RegisterSharedOcarinaButtonFamilies();
    MM_RegisterSharedOcarinaButtonFamilies();
    OoT_SharedButtonsTest_End();
    // Leave OoT's live save a plain, non-randomizer file for the next row.
    OoT_SharedSwordTest_ArmLive();
    SobRestoreGiveCaps(&priorCaps);
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSobFailures != 0) {
        printf("[TEST] FAIL: shared-ocarina-buttons-pairs (%d failure(s))\n", sSobFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-ocarina-buttons-pairs\n");
    return TEST_PASS;
}

TestResult Test_SharedOcarinaButtonsArrival(void) {
    printf("[TEST] shared-ocarina-buttons-arrival: with the Shared Ocarina Buttons rule on, a button found in one game "
           "is owned in the other on arrival; a game whose own shuffle is off shares nothing; off, nothing crosses "
           "(#1067)\n");
    sSobFailures = 0;
    OoT_SharedButtonsTest_Begin();

    // ---- (1) OFF: nothing crosses -------------------------------------------------
    {
        SobFreezeWorld(false);
        SobOoTDeparts(1, SOB_C_UP);
        SobMMArrives(1, 0u);
        const unsigned mm = MM_SharedButtonsTest_Held();
        printf("[TEST] (1) off, OoT C-Up -> MM: MM holds %02X, pool %08X (expected 00, 00000000)\n", mm,
               (unsigned)Combo_SharedFamiliesOwned());
        SOB_CHECK(mm == 0u, "OFF: OoT's C-Up button reached MM (MM holds %02X)", mm);
        SOB_CHECK(Combo_SharedFamiliesOwned() == 0u, "OFF: a harvest wrote the family bitset (%08X)",
                  (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (2) ON, OoT -> MM: the C-Up found in Hyrule is MM's on arrival ----------
    {
        SobFreezeWorld(true);
        SobOoTDeparts(1, SOB_C_UP);
        SobMMArrives(1, 0u);
        const unsigned mm = MM_SharedButtonsTest_Held();
        printf("[TEST] (2) on, OoT C-Up -> MM: MM holds %02X (expected %02X)\n", mm, SOB_C_UP);
        SOB_CHECK(mm == SOB_C_UP,
                  "ON: MM arrived holding %02X after OoT found the C-Up button; it must hold C-Up, and "
                  "only C-Up",
                  mm);
    }

    // ---- (3) ON, MM -> OoT: the A and C-Left found in Termina are OoT's ----------
    {
        SobFreezeWorld(true);
        SobMMDeparts(1, SOB_A | SOB_C_LEFT);
        SobOoTArrives(1, SOB_C_RIGHT);
        const unsigned oot = OoT_SharedButtonsTest_Held();
        printf("[TEST] (3) on, MM A + C-Left -> OoT holding C-Right: OoT holds %02X (expected %02X)\n", oot,
               SOB_A | SOB_C_LEFT | SOB_C_RIGHT);
        SOB_CHECK(oot == (SOB_A | SOB_C_LEFT | SOB_C_RIGHT),
                  "ON: OoT arrived holding %02X after MM found A and C-Left; it must hold them beside its own C-Right",
                  oot);
    }

    // ---- (4) ON: a game whose shuffle is off shares nothing --------------------------
    // OoT's file starts with all five when its own shuffle is off (savefile.cpp);
    // none of them was found, so none may reach an MM that shuffles its buttons.
    {
        SobFreezeWorld(true);
        SobOoTDeparts(0, SOB_ALL);
        SobMMArrives(1, SOB_C_DOWN);
        const unsigned mm = MM_SharedButtonsTest_Held();
        printf("[TEST] (4) on, OoT shuffle off (holds all five) -> MM: MM holds %02X, pool %08X (expected %02X, "
               "00000000)\n",
               mm, (unsigned)Combo_SharedFamiliesOwned(), SOB_C_DOWN);
        SOB_CHECK(mm == SOB_C_DOWN, "ON: an OoT without the button shuffle gave MM buttons it never found (MM %02X)",
                  mm);
        SOB_CHECK(Combo_SharedFamiliesOwned() == 0u,
                  "ON: an OoT without the button shuffle harvested %08X into the pool",
                  (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (5) ON, MONOTONIC: an MM stint holding less never takes a button back ----
    {
        SobFreezeWorld(true);
        SobMMDeparts(1, SOB_C_DOWN);
        SobMMDeparts(1, 0u); // a later MM stint; the pool keeps what was found
        SobOoTArrives(1, 0u);
        const unsigned oot = OoT_SharedButtonsTest_Held();
        printf("[TEST] (5) on, MM C-Down then an MM stint without it -> OoT: OoT holds %02X (expected %02X)\n", oot,
               SOB_C_DOWN);
        SOB_CHECK(oot == SOB_C_DOWN, "ON: the shared C-Down button was lost from the pair (OoT holds %02X)", oot);
    }

    OoT_SharedButtonsTest_End();
    // Leave both live saves plain, non-randomizer files for the next row (the
    // AllTests process runs every row in one process).
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sSobFailures != 0) {
        printf("[TEST] FAIL: shared-ocarina-buttons-arrival (%d failure(s))\n", sSobFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: shared-ocarina-buttons-arrival\n");
    return TEST_PASS;
}
