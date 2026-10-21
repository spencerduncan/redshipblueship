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
 *   returns the defaults). The key lives in the CVar store on the Ship::Context
 *   singleton, whose C bridge dereferences that singleton unchecked, so the row
 *   runs behind test_runner.cpp's display-free shared bring-up
 *   (Test_SharedBottlesRule there; SharedBottlesRule_Run here).
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
 *   both games' REAL item classifications, trims the pair's SLOT-BOUND bottle
 *   items to FOUR in total with the rule on, and trims nothing with it off.
 *   Ruto's Letter is never a trimmed copy but takes one of the four slots (OoT
 *   gives it only into an empty bottle slot), so a Zora's-Fountain-closed pool
 *   keeps three bottles beside it; plentiful (SURPLUS) bottle copies count
 *   against the same four; and each game that has bottles keeps at least one.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h" // Combo_ComboSettingStoreAvailable
#include "../context.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

#include "../cvar_shared_keys.h" // RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES, the rule's key

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

/** The record row's body. Run only by Test_SharedBottlesRule (test_runner.cpp),
 *  after the shared bring-up has put a CVar store on the Ship::Context. */
static TestResult SharedBottlesRule_Run(void) {
    printf("[TEST] shared-bottles-rule: the Shared Bottles key is read into the frozen item rules record, and the "
           "fingerprint carries it (#970)\n");
    sSsbFailures = 0;
    // Without a store every leg below would read the defaults and pass vacuously.
    SSB_CHECK(Combo_ComboSettingStoreAvailable(), "the bring-up gave the resolver no CVar store");
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

/** What one compose kept and trimmed of a bottle pool. */
typedef struct {
    int rc;
    int keptBottles[3]; // indexed by GameId; Ruto's Letter not counted
    int trimmedBottles;
    int letterRows; // Ruto's Letter rows in the pool
    bool letterKept;
    bool trimmedLetter;
} SsbCompose;

/** Is pool row @p r one of the bottle items this row puts in the pool (not Ruto's Letter)? */
static bool SsbIsBottleRow(const ComboLogicPoolRow* r, uint16_t ootLetter) {
    return !(r->item.originGame == (uint8_t)GAME_OOT && r->item.id == ootLetter);
}

/** Append @p count rows of (@p game, @p id, @p poolFlags) to @p pool. */
static void SsbAdd(ComboLogicPoolRow* pool, int* n, uint8_t game, int id, int count, uint16_t poolFlags) {
    for (int i = 0; i < count; ++i, ++*n) {
        pool[*n].item.originGame = game;
        pool[*n].item.id = (uint16_t)id;
        pool[*n].poolFlags = poolFlags;
    }
}

/** Freeze a world with the rule @p on and compose @p pool through the REAL composer. */
static SsbCompose SsbRun(const char* leg, const ComboLogicPoolRow* pool, int n, bool on, uint16_t ootLetter) {
    SsbCompose c;
    memset(&c, 0, sizeof(c));
    SsbFreezeWorld(on);
    ComboLogicComposeRequest req;
    memset(&req, 0, sizeof(req));
    req.rows = pool;
    req.rowCount = n;
    req.quantityFlags = 0u; // the production trim
    req.startingHealthOoT = (uint16_t)RSBS_SHARED_QTY_DEFAULT_START_HEALTH;
    req.startingHealthMM = (uint16_t)RSBS_SHARED_QTY_DEFAULT_START_HEALTH;
    req.trimSeed = 0x0970u;
    ComboLogicBagItem bag[32];
    int index[32];
    ComboLogicComposeResult res;
    memset(&res, 0, sizeof(res));
    c.rc = Combo_Logic_ComposeBag(&req, bag, 32, index, &res);
    for (int i = 0; i < n; ++i) {
        c.letterRows += SsbIsBottleRow(&pool[i], ootLetter) ? 0 : 1;
    }
    for (int i = 0; i < res.bagCount && i < 32; ++i) {
        const ComboLogicPoolRow* r = &pool[index[i]];
        if (SsbIsBottleRow(r, ootLetter)) {
            c.keptBottles[r->item.originGame]++;
        } else {
            c.letterKept = true;
        }
    }
    for (int t = 0; t < Combo_Logic_ComposeTrimmedCount(); ++t) {
        int at = -1;
        if (Combo_Logic_ComposeTrimmedAt(t, &at) && at >= 0 && at < n) {
            if (SsbIsBottleRow(&pool[at], ootLetter)) {
                c.trimmedBottles++;
            } else {
                c.trimmedLetter = true;
            }
        }
    }
    printf("[TEST] (%s, %s) compose rc %d: %d bag row(s), kept bottles OoT %d + MM %d, letter %s, %d trimmed\n", leg,
           on ? "on" : "off", c.rc, res.bagCount, c.keptBottles[GAME_OOT], c.keptBottles[GAME_MM],
           c.letterRows == 0 ? "absent" : (c.letterKept ? "kept" : "MISSING"), c.trimmedBottles);
    SSB_CHECK(c.rc == RSBS_COMBO_LOGIC_OK, "%s: the composer refused the pool (rc %d)", leg, c.rc);
    if (c.letterRows != 0) {
        SSB_CHECK(c.letterKept && !c.trimmedLetter,
                  "%s: Ruto's Letter left the bag: it is OoT's quest item, never one of the shared bottle copies", leg);
    }
    return c;
}

TestResult Test_SharedBottlesTrim(void) {
    printf("[TEST] shared-bottles-trim: with the Shared Bottles rule on, the composer keeps the pair's slot-bound "
           "bottle items (Ruto's Letter counted, never trimmed) at four, at least one bottle per game, plentiful "
           "copies included; off, it trims nothing (#970)\n");
    sSsbFailures = 0;
    SSB_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table did not come up");

    const uint16_t ootLetter = (uint16_t)OoT_SharedBottleTest_PoolId(3);
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    const uint16_t P = (uint16_t)RSBS_COMBO_POOL_PLENTIFUL;

    // LEG 1, Zora's Fountain closed (SoH's default): OoT's three generic bottles
    // and Ruto's Letter (item_pool.cpp's four), MM its six bottle checks' items.
    ComboLogicPoolRow pool[32];
    memset(pool, 0, sizeof(pool));
    int n = 0;
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(0), 1, 0u);
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(1), 1, 0u);
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(2), 1, 0u);
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(3), 1, 0u);
    SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(0), 2, 0u);
    for (int w = 1; w <= 4; ++w) {
        SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(w), 1, 0u);
    }

    // The premise: every one is progression in its own game, so REQUIRED.
    for (int i = 0; i < n; ++i) {
        const int d = Combo_Logic_ComposeDisposition(pool[i].item, 0u, 0u);
        SSB_CHECK(d == RSBS_COMBO_COMPOSE_REQUIRED,
                  "pool row %d (%s id %u) is disposition %d, not REQUIRED; the row's "
                  "premise moved",
                  i, pool[i].item.originGame == O ? "OoT" : "MM", (unsigned)pool[i].item.id, d);
    }

    {
        const SsbCompose off = SsbRun("zf-closed", pool, n, false, ootLetter);
        SSB_CHECK(off.trimmedBottles == 0 && off.keptBottles[GAME_OOT] == 3 && off.keptBottles[GAME_MM] == 6,
                  "OFF: the composer trimmed %d bottle item(s) (kept OoT %d, MM %d); a world that never asked "
                  "keeps both games' bottles",
                  off.trimmedBottles, off.keptBottles[GAME_OOT], off.keptBottles[GAME_MM]);
    }
    {
        // Ruto's Letter takes a bottle slot when it is given (OoT's Item_Give
        // needs an ITEM_NONE slot for it, and the rando hands a blue rupee when
        // there is none), so it is one of the four slot-bound items: the bag
        // keeps THREE bottles beside it. Four bottles plus the letter would let
        // the shared count fill OoT's four slots before the letter is found.
        const SsbCompose on = SsbRun("zf-closed", pool, n, true, ootLetter);
        const int kept = on.keptBottles[GAME_OOT] + on.keptBottles[GAME_MM];
        SSB_CHECK(kept == 3,
                  "ON: the bag keeps %d bottle items beside Ruto's Letter (OoT %d, MM %d); with the letter's slot "
                  "the pair holds %d slot-bound items against a shared count of four",
                  kept, on.keptBottles[GAME_OOT], on.keptBottles[GAME_MM], kept + 1);
        SSB_CHECK(on.keptBottles[GAME_OOT] >= 1 && on.keptBottles[GAME_MM] >= 1,
                  "ON: a game kept no bottle of its own (OoT %d, MM %d); each engine assumes only its own copies",
                  on.keptBottles[GAME_OOT], on.keptBottles[GAME_MM]);
        SSB_CHECK(on.trimmedBottles == 6, "ON: %d bottle item(s) trimmed, expected 6 (nine copies, three kept)",
                  on.trimmedBottles);
    }

    // LEG 2, Zora's Fountain open: no letter in OoT's pool (four generic
    // bottles), so the bag keeps four bottle items.
    n = 0;
    memset(pool, 0, sizeof(pool));
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(0), 2, 0u);
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(1), 1, 0u);
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(2), 1, 0u);
    SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(0), 2, 0u);
    for (int w = 1; w <= 4; ++w) {
        SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(w), 1, 0u);
    }
    {
        const SsbCompose on = SsbRun("zf-open", pool, n, true, ootLetter);
        SSB_CHECK(on.keptBottles[GAME_OOT] + on.keptBottles[GAME_MM] == 4,
                  "ON (no letter): the bag keeps %d bottle items (OoT %d, MM %d); the shared count is four",
                  on.keptBottles[GAME_OOT] + on.keptBottles[GAME_MM], on.keptBottles[GAME_OOT],
                  on.keptBottles[GAME_MM]);
        SSB_CHECK(on.keptBottles[GAME_OOT] >= 1 && on.keptBottles[GAME_MM] >= 1,
                  "ON (no letter): a game kept no bottle of its own (OoT %d, MM %d)", on.keptBottles[GAME_OOT],
                  on.keptBottles[GAME_MM]);
    }

    // LEG 3, plentiful: MM's plentiful copies of its bottles are SURPLUS rows
    // (GeneratePools duplicates RITYPE_MAJOR items). They take slots exactly as
    // the required ones do, so the four-slot budget covers them too: with the
    // letter and three required bottles kept, every surplus bottle is trimmed.
    n = 0;
    memset(pool, 0, sizeof(pool));
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(0), 3, 0u);
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(3), 1, 0u);
    SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(0), 6, 0u);
    SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(1), 3, P);
    SSB_CHECK(Combo_Logic_ComposeDisposition(pool[n - 1].item, P, 0u) == RSBS_COMBO_COMPOSE_SURPLUS,
              "a plentiful MM bottle row is not SURPLUS; the leg's premise moved");
    {
        const SsbCompose on = SsbRun("plentiful", pool, n, true, ootLetter);
        SSB_CHECK(on.keptBottles[GAME_OOT] + on.keptBottles[GAME_MM] == 3,
                  "ON (plentiful): the bag keeps %d bottle items beside Ruto's Letter (OoT %d, MM %d); surplus "
                  "bottles take slots too, so the pair keeps three",
                  on.keptBottles[GAME_OOT] + on.keptBottles[GAME_MM], on.keptBottles[GAME_OOT],
                  on.keptBottles[GAME_MM]);
    }

    // LEG 4, lopsided pools: one OoT bottle against ten MM ones. A largest-
    // remainder split of four would keep none of OoT's; the trim keeps at least
    // one bottle for each game that has any, so each engine's own HasBottle
    // proof still has a copy of its own to stand on.
    n = 0;
    memset(pool, 0, sizeof(pool));
    SsbAdd(pool, &n, O, OoT_SharedBottleTest_PoolId(0), 1, 0u);
    SsbAdd(pool, &n, M, MM_SharedBottleTest_PoolId(0), 10, 0u);
    {
        const SsbCompose on = SsbRun("lopsided", pool, n, true, ootLetter);
        SSB_CHECK(on.keptBottles[GAME_OOT] == 1 && on.keptBottles[GAME_MM] == 3,
                  "ON (lopsided): kept OoT %d + MM %d, expected 1 + 3 (at least one per game, four in total)",
                  on.keptBottles[GAME_OOT], on.keptBottles[GAME_MM]);
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
