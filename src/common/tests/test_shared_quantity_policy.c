/**
 * @file test_shared_quantity_policy.c
 * @brief THE SHARED-QUANTITY POOL POLICY and THE SHARED-QUANTITY TRIM, over
 *        synthetic classification sources and the real shared carrier (lane K13;
 *        #525 x #645 increment 3; shared_items.h and combo_logic.h).
 *
 * CTest row SharedQuantityPolicy (label "redship"), dispatch
 * "shared-quantity-policy". ROM-free and display-free: the policy table, the
 * budget arithmetic, the composer's trim over a synthetic pool shaped like the two
 * shipped pools, and the play-side check through the REAL shared carrier
 * (shared_resources.h). combo-logic-bag-composition B7 runs the same trim and the
 * same carrier walk over both REAL pools.
 *
 * THE OPERATOR'S QUESTION (2026-09-27), locked directly: "we aren't accidentally
 * reducing heart containers, pieces of heart, ammo/rupees to single count are
 * we? ... we don't want to increase the maximum heart count." Legs:
 *
 *   Q1 THE POLICY TABLE: every #525 kind's policy as shared_items.h states it,
 *      the ocarina's frozen arming resolved (unarmed = keep-all).
 *   Q2 THE BUDGETS: 44 pieces + 6 containers at three starting hearts; the
 *      starting-health and short-grade adjustments; tier budgets from the pools'
 *      own ceilings; unequal ceilings kept whole; double defense capped at 1;
 *      the proportional spread.
 *   Q3 THE TRIM over a pool shaped like both shipped pools (OoT 36 pieces + 8
 *      containers, MM 52 + 4, one double defense each, 3 + 3 quivers, 2 + 1
 *      hookshots, rupees and bombchus): exactly 44 + 6 + 1 kept, quiver 3 of 6
 *      (a kind with N max and 2N copies keeps exactly N), the hookshot whole,
 *      every removed copy counted as filler under its ORIGIN and listed; rupees
 *      (renewable, keep-all) and bombchus (progression, keep-all) untouched; the
 *      bag a stable filter of the pool. Deterministic per seed, different across
 *      seeds (the sensitivity control). RED HALF: RSBS_COMBO_QUANTITY_KEEP_ALL
 *      composes the untrimmed bag (100 heart rows, 2 double defenses).
 *   Q4 PLENTIFUL: surplus of a trimmed family is surplus relative to the trimmed
 *      count (one game's worth of extras), and never touches REQUIRED rows.
 *   Q5 THE PLAY-SIDE CHECK, which is the operator's actual concern: every heart
 *      row of the TRIMMED bag awarded through the real carrier (apply, give,
 *      Combo_MakeHealthQuarters, harvest) ends the shared bar at exactly 320 with
 *      ZERO dead pickups; the UNTRIMMED bag clamps N pickups (printed, > 0). The
 *      same for the quiver tier and double defense; and the unequal-ceiling
 *      hookshot's order hazard, observed (the reason it is kept whole).
 *
 * Linkage note: `#include`d into test_runner.cpp at FILE SCOPE and therefore
 * compiled as C++, like every other file in this directory.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#define SQP_ASSERT(cond, msg)                                                    \
    do {                                                                         \
        if (!(cond)) {                                                           \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);       \
            return TEST_FAIL;                                                    \
        }                                                                        \
    } while (0)

namespace {

/** What a walk observed. */
struct SqpWalk {
    int pickups = 0;
    int dead = 0;       // pickups after which the shared quantity did not grow
    int finalValue = 0; // the shared quantity at the end
};

/** One heart row as the walk sees it: its origin and its size in pieces. */
struct SqpHeartRow {
    uint8_t origin;
    uint8_t units;
};

void SqpClearSharedStore(void) {
    memset(gComboCtx.sharedResources, 0, sizeof(gComboCtx.sharedResources));
    memset(gComboCtx.sharedResourcesExt, 0, sizeof(gComboCtx.sharedResourcesExt));
    Combo_ResetSharedResourceWatermarks();
}

/**
 * Award `rows` in order through the real carrier: each pickup's game applies the
 * shared bar (Combo_ApplySharedResource, cap 320), gives the copy the way its
 * give path does (a piece: +1 piece, four pieces fold into a container; a
 * container: +0x10 capacity; NEITHER clamps), and harvests
 * Combo_MakeHealthQuarters(capacity, pieces), which is where the clamp lives. A
 * pickup that leaves the shared bar unchanged is DEAD. Both games start at
 * `startHealth`. Clears the shared-resource store; the caller restores gComboCtx.
 */
SqpWalk SqpWalkHealth(const std::vector<SqpHeartRow>& rows, uint16_t startHealth) {
    SqpWalk w;
    SqpClearSharedStore();
    uint16_t cap[3] = { 0, startHealth, startHealth };
    uint16_t pieces[3] = { 0, 0, 0 };
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(startHealth, 0));
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(startHealth, 0));
    for (const SqpHeartRow& r : rows) {
        const GameId g = (GameId)r.origin;
        uint16_t live = Combo_MakeHealthQuarters(cap[g], pieces[g]);
        (void)Combo_ApplySharedResource(g, RSBS_SHARED_RES_HEALTH_QUARTERS,
                                        (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS, &live);
        Combo_SplitHealthQuarters(live, &cap[g], &pieces[g]);
        uint16_t before = 0;
        (void)Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_QUARTERS, &before);
        if (r.units == RSBS_SHARED_QTY_UNITS_CONTAINER) {
            cap[g] = (uint16_t)(cap[g] + 0x10);
        } else {
            pieces[g]++;
            if (pieces[g] >= 4) {
                pieces[g] = (uint16_t)(pieces[g] - 4);
                cap[g] = (uint16_t)(cap[g] + 0x10);
            }
        }
        Combo_HarvestSharedResource(g, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(cap[g], pieces[g]));
        uint16_t after = 0;
        (void)Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_QUARTERS, &after);
        w.pickups++;
        if (after == before) {
            w.dead++;
        }
    }
    uint16_t fin = 0;
    (void)Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_QUARTERS, &fin);
    w.finalValue = (int)fin;
    return w;
}

/**
 * The same walk for a MONOTONIC TIER kind: each pickup's game applies the shared
 * tier (capped at ITS OWN ceiling), increments its live tier by one clamped at
 * that ceiling (both ports' progressive gives), and harvests.
 */
SqpWalk SqpWalkTier(uint8_t kind, const std::vector<uint8_t>& origins, uint16_t ceilingOoT, uint16_t ceilingMM) {
    SqpWalk w;
    SqpClearSharedStore();
    uint16_t tier[3] = { 0, 0, 0 };
    const uint16_t ceiling[3] = { 0, ceilingOoT, ceilingMM };
    for (const uint8_t o : origins) {
        const GameId g = (GameId)o;
        (void)Combo_ApplySharedResource(g, kind, ceiling[g], &tier[g]);
        uint16_t before = 0;
        (void)Combo_GetSharedResource(kind, &before);
        if (tier[g] < ceiling[g]) {
            tier[g]++;
        }
        Combo_HarvestSharedResource(g, kind, tier[g]);
        uint16_t after = 0;
        (void)Combo_GetSharedResource(kind, &after);
        w.pickups++;
        if (after == before) {
            w.dead++;
        }
    }
    uint16_t fin = 0;
    (void)Combo_GetSharedResource(kind, &fin);
    w.finalValue = (int)fin;
    return w;
}

/** The heart rows of `bag`, OoT and MM alternated in bag order (the longer
 *  list's tail last): the order a player who alternates games meets them. */
std::vector<SqpHeartRow> SqpHeartRowsInterleaved(const ComboLogicBagItem* bag, int count) {
    std::vector<SqpHeartRow> perGame[3];
    for (int i = 0; i < count; ++i) {
        if (Combo_ItemClassSharedKind(bag[i].item) != RSBS_SHARED_RES_HEALTH_QUARTERS) {
            continue;
        }
        SqpHeartRow r;
        r.origin = bag[i].item.originGame;
        r.units = Combo_ItemClassSharedUnits(bag[i].item);
        if (r.origin == (uint8_t)GAME_OOT || r.origin == (uint8_t)GAME_MM) {
            perGame[r.origin].push_back(r);
        }
    }
    std::vector<SqpHeartRow> out;
    size_t a = 0;
    size_t b = 0;
    while (a < perGame[GAME_OOT].size() || b < perGame[GAME_MM].size()) {
        if (a < perGame[GAME_OOT].size()) {
            out.push_back(perGame[GAME_OOT][a++]);
        }
        if (b < perGame[GAME_MM].size()) {
            out.push_back(perGame[GAME_MM][b++]);
        }
    }
    return out;
}

} // namespace

/**
 * THE PLAY-SIDE CHECK over any bag (combo-logic-bag-composition B7 calls it over
 * the real composed bag): award every heart row of `bag`, interleaved by origin,
 * through the real carrier from a `startHealth` bar. Returns the DEAD pickups and
 * writes the pickups and the final bar. Leaves gComboCtx exactly as it found it.
 */
int SqpDeadHeartPickups(const ComboLogicBagItem* bag, int count, uint16_t startHealth, int* outPickups,
                        int* outFinal) {
    std::unique_ptr<unsigned char[]> saved(new unsigned char[sizeof(gComboCtx)]);
    memcpy(saved.get(), &gComboCtx, sizeof(gComboCtx));
    const SqpWalk w = SqpWalkHealth(SqpHeartRowsInterleaved(bag, count), startHealth);
    memcpy(&gComboCtx, saved.get(), sizeof(gComboCtx));
    Combo_ResetSharedResourceWatermarks();
    if (outPickups != nullptr) {
        *outPickups = w.pickups;
    }
    if (outFinal != nullptr) {
        *outFinal = w.finalValue;
    }
    return w.dead;
}

namespace {

// ---------------------------------------------------------------------------
// The synthetic sources: the same id space for both origins, each id one family.
// ---------------------------------------------------------------------------
constexpr uint16_t kSqpPiece = 1;
constexpr uint16_t kSqpContainer = 2;
constexpr uint16_t kSqpDoubleDefense = 3;
constexpr uint16_t kSqpQuiver = 4;
constexpr uint16_t kSqpHookshot = 5;
constexpr uint16_t kSqpRupee = 6;
constexpr uint16_t kSqpBombchu = 7;
constexpr uint16_t kSqpOcarina = 8;

ComboItemClassRow SqpRow(uint8_t fillClass, uint8_t kind, uint8_t units) {
    ComboItemClassRow row;
    memset(&row, 0, sizeof(row));
    row.fillClass = fillClass;
    row.armedBy = 0u;
    row.sharedKind = kind;
    row.sharedUnits = units;
    return row;
}

int SqpClassify(uint16_t id, ComboItemClassRow* out) {
    const uint8_t P = (uint8_t)RSBS_FILL_CLASS_PROGRESSION;
    switch (id) {
        case kSqpPiece:
            *out = SqpRow(P, RSBS_SHARED_RES_HEALTH_QUARTERS, RSBS_SHARED_QTY_UNITS_PIECE);
            return 1;
        case kSqpContainer:
            *out = SqpRow(P, RSBS_SHARED_RES_HEALTH_QUARTERS, RSBS_SHARED_QTY_UNITS_CONTAINER);
            return 1;
        case kSqpDoubleDefense:
            *out = SqpRow(P, RSBS_SHARED_RES_DOUBLE_DEFENSE, 0u);
            return 1;
        case kSqpQuiver:
            *out = SqpRow(P, RSBS_SHARED_RES_QUIVER_TIER, 0u);
            return 1;
        case kSqpHookshot:
            *out = SqpRow(P, RSBS_SHARED_RES_HOOKSHOT_TIER, 0u);
            return 1;
        case kSqpRupee:
            *out = SqpRow((uint8_t)RSBS_FILL_CLASS_RENEWABLE, RSBS_SHARED_RES_RUPEES, 0u);
            return 1;
        case kSqpBombchu:
            *out = SqpRow(P, RSBS_SHARED_RES_BOMBCHU_COUNT, 0u);
            return 1;
        case kSqpOcarina:
            *out = SqpRow(P, RSBS_SHARED_RES_OCARINA_TIER, 0u);
            return 1;
        default:
            *out = SqpRow((uint8_t)RSBS_FILL_CLASS_NONE, 0u, 0u);
            return 0;
    }
}

const ComboItemClassSource kSqpOoTSource = { RSBS_ITEM_CLASS_SOURCE_ABI, 16u, SqpClassify };
const ComboItemClassSource kSqpMMSource = { RSBS_ITEM_CLASS_SOURCE_ABI, 16u, SqpClassify };

/** Installs the synthetic pair and snapshots gComboCtx; puts both back on EVERY
 *  exit (an early SQP_ASSERT return included). */
struct SqpGuard {
    const ComboItemClassSource* realOoT;
    const ComboItemClassSource* realMM;
    std::unique_ptr<unsigned char[]> ctx;
    bool installed = false;
    SqpGuard()
        : realOoT(Combo_GetItemClassSource((uint8_t)GAME_OOT)), realMM(Combo_GetItemClassSource((uint8_t)GAME_MM)),
          ctx(new unsigned char[sizeof(gComboCtx)]) {
        memcpy(ctx.get(), &gComboCtx, sizeof(gComboCtx));
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_OOT);
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM);
        installed = Combo_RegisterItemClassSource((uint8_t)GAME_OOT, &kSqpOoTSource) &&
                    Combo_RegisterItemClassSource((uint8_t)GAME_MM, &kSqpMMSource);
    }
    ~SqpGuard() {
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_OOT);
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM);
        if (realOoT != nullptr) {
            Combo_RegisterItemClassSource((uint8_t)GAME_OOT, realOoT);
        }
        if (realMM != nullptr) {
            Combo_RegisterItemClassSource((uint8_t)GAME_MM, realMM);
        }
        memcpy(&gComboCtx, ctx.get(), sizeof(gComboCtx));
        Combo_ResetSharedResourceWatermarks();
    }
};

/** Freeze a combo record with the shared ocarina on or off (the frozen record is
 *  what Combo_SharedResourceKindArmed reads once a world exists). */
void SqpFreeze(bool sharedOcarina) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x5A0B7133u;
    gComboCtx.sharedRandoSettingsHash = 0x5A0B7144u;
    gComboCtx.mmProfileDigest = 0x5A0B7155u;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.comboFlags = sharedOcarina ? (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA : 0u;
    Combo_FreezeComboSettings(&rec);
}

struct SqpPool {
    std::vector<ComboLogicPoolRow> rows;
    void Add(uint8_t origin, uint16_t id, int n, uint16_t flags = 0u) {
        for (int i = 0; i < n; ++i) {
            ComboLogicPoolRow r;
            memset(&r, 0, sizeof(r));
            r.item.originGame = origin;
            r.item.id = id;
            r.poolFlags = flags;
            rows.push_back(r);
        }
    }
};

/** Both shipped pools' SHAPE for these families (the real counts are printed over
 *  the real exports by combo-logic-bag-composition B7). */
SqpPool SqpShippedShape() {
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    SqpPool p;
    p.Add(O, kSqpPiece, 36);
    p.Add(O, kSqpContainer, 8);
    p.Add(O, kSqpDoubleDefense, 1);
    p.Add(O, kSqpQuiver, 3);
    p.Add(O, kSqpHookshot, 2);
    p.Add(O, kSqpRupee, 20);
    p.Add(O, kSqpBombchu, 5);
    p.Add(O, kSqpOcarina, 2);
    p.Add(M, kSqpPiece, 52);
    p.Add(M, kSqpContainer, 4);
    p.Add(M, kSqpDoubleDefense, 1);
    p.Add(M, kSqpQuiver, 3);
    p.Add(M, kSqpHookshot, 1);
    p.Add(M, kSqpRupee, 20);
    p.Add(M, kSqpBombchu, 5);
    p.Add(M, kSqpOcarina, 1);
    return p;
}

struct SqpComposed {
    int status = -1;
    ComboLogicComposeResult res;
    std::vector<ComboLogicBagItem> bag;
    std::vector<int> poolIndex;
    std::vector<int> trimmed;
};

SqpComposed SqpCompose(const SqpPool& pool, uint32_t seed, uint16_t quantityFlags = 0u, uint16_t startOoT = 0u,
                       uint16_t startMM = 0u) {
    SqpComposed c;
    memset(&c.res, 0, sizeof(c.res));
    ComboLogicComposeRequest req;
    memset(&req, 0, sizeof(req));
    req.rows = pool.rows.data();
    req.rowCount = (int)pool.rows.size();
    req.trimSeed = seed;
    req.quantityFlags = quantityFlags;
    req.startingHealthOoT = startOoT;
    req.startingHealthMM = startMM;
    c.bag.assign(pool.rows.size() + 1, ComboLogicBagItem());
    c.poolIndex.assign(pool.rows.size() + 1, -1);
    c.status = Combo_Logic_ComposeBag(&req, c.bag.data(), (int)c.bag.size(), c.poolIndex.data(), &c.res);
    const int kept = (c.status == RSBS_COMBO_LOGIC_OK) ? c.res.bagCount : 0;
    c.bag.resize((size_t)kept);
    c.poolIndex.resize((size_t)kept);
    for (int i = 0; i < Combo_Logic_ComposeTrimmedCount(); ++i) {
        int idx = -1;
        if (Combo_Logic_ComposeTrimmedAt(i, &idx)) {
            c.trimmed.push_back(idx);
        }
    }
    return c;
}

/** Bag rows of (origin, id); `surplus` 1 = only SURPLUS, 0 = only REQUIRED, -1 = both. */
int SqpCount(const SqpComposed& c, uint8_t origin, uint16_t id, int surplus = -1) {
    int n = 0;
    for (const ComboLogicBagItem& b : c.bag) {
        const int isSurplus = ((b.bagFlags & RSBS_COMBO_BAG_SURPLUS) != 0u) ? 1 : 0;
        if (b.item.originGame == origin && b.item.id == id && (surplus < 0 || surplus == isSurplus)) {
            n++;
        }
    }
    return n;
}

int SqpBoth(const SqpComposed& c, uint16_t id, int surplus = -1) {
    return SqpCount(c, (uint8_t)GAME_OOT, id, surplus) + SqpCount(c, (uint8_t)GAME_MM, id, surplus);
}

bool SqpSameBag(const SqpComposed& a, const SqpComposed& b) {
    if (a.bag.size() != b.bag.size() || a.trimmed != b.trimmed || a.poolIndex != b.poolIndex) {
        return false;
    }
    for (size_t i = 0; i < a.bag.size(); ++i) {
        if (a.bag[i].item.originGame != b.bag[i].item.originGame || a.bag[i].item.id != b.bag[i].item.id ||
            a.bag[i].bagFlags != b.bag[i].bagFlags) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> SqpOrigins(const SqpComposed& c, uint16_t id) {
    std::vector<uint8_t> out;
    for (const ComboLogicBagItem& b : c.bag) {
        if (b.item.id == id) {
            out.push_back(b.item.originGame);
        }
    }
    return out;
}

} // namespace

TestResult Test_SharedQuantityPolicy(void) {
    printf("[TEST] shared-quantity-policy: trim capacity-like shared families to the shared maximum (lane K13)\n");
    const SqpGuard guard;
    SQP_ASSERT(guard.installed, "both synthetic classification sources installed");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;

    // ---- Q1: the policy table --------------------------------------------------
    SqpFreeze(false);
    {
        const uint8_t trimTier[] = { RSBS_SHARED_RES_WALLET_TIER,  RSBS_SHARED_RES_MAGIC_LEVEL,
                                     RSBS_SHARED_RES_QUIVER_TIER,  RSBS_SHARED_RES_BOMB_BAG_TIER,
                                     RSBS_SHARED_RES_STICK_TIER,   RSBS_SHARED_RES_NUT_TIER,
                                     RSBS_SHARED_RES_HOOKSHOT_TIER };
        const uint8_t keepAll[] = { RSBS_SHARED_RES_RUPEES,        RSBS_SHARED_RES_HEALTH_CURRENT,
                                    RSBS_SHARED_RES_MAGIC_CURRENT, RSBS_SHARED_RES_ARROW_COUNT,
                                    RSBS_SHARED_RES_BOMB_COUNT,    RSBS_SHARED_RES_BOMBCHU_COUNT,
                                    RSBS_SHARED_RES_STICK_COUNT,   RSBS_SHARED_RES_NUT_COUNT,
                                    RSBS_SHARED_RES_TRIFORCE_PIECES };
        ComboSharedQuantityPolicy p;
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_HEALTH_QUARTERS, &p) &&
                       p.policy == RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX && p.budget == RSBS_SHARED_QTY_BUDGET_HEALTH,
                   "Q1: health is TRIM_TO_SHARED_MAX by the health budget");
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_DOUBLE_DEFENSE, &p) &&
                       p.policy == RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX && p.budget == RSBS_SHARED_QTY_BUDGET_TIER &&
                       p.fixedCap == 1u,
                   "Q1: double defense is a TIER trim capped at one copy");
        for (const uint8_t k : trimTier) {
            SQP_ASSERT(Combo_SharedQuantityPolicyOf(k, &p) && p.policy == RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX &&
                           p.budget == RSBS_SHARED_QTY_BUDGET_TIER && p.fixedCap == 0u,
                       "Q1: every always-shared capacity tier is a TIER trim");
        }
        for (const uint8_t k : keepAll) {
            SQP_ASSERT(Combo_SharedQuantityPolicyOf(k, &p) && p.policy == RSBS_SHARED_QTY_KEEP_ALL,
                       "Q1: rupees, current health/magic, every ammo count, bombchus and triforce pieces are KEEP_ALL");
        }
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_OCARINA_TIER, &p) &&
                       p.policy == RSBS_SHARED_QTY_KEEP_ALL,
                   "Q1: an UNARMED shared ocarina keeps each game's own copies");
        SQP_ASSERT(!Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_NONE, &p) && p.policy == RSBS_SHARED_QTY_KEEP_ALL &&
                       !Combo_SharedQuantityPolicyOf((uint8_t)RSBS_SHARED_RES_KIND_COUNT, &p),
                   "Q1: a kind that is not a real shared resource is refused as KEEP_ALL");
        SqpFreeze(true);
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_OCARINA_TIER, &p) &&
                       p.policy == RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX && p.budget == RSBS_SHARED_QTY_BUDGET_TIER,
                   "Q1: an ARMED shared ocarina is a TIER trim");
        SqpFreeze(false);
    }

    // ---- Q2: the budgets ---------------------------------------------------------
    {
        int pc = -1;
        int hc = -1;
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0u, -1, -1, &pc, &hc) == 68 && pc == 44 && hc == 6,
                   "Q2: three starting hearts (the unpublished default) is OoTMM's 44 pieces + 6 containers");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x30u, 88, 12, &pc, &hc) == 68 && pc == 44 && hc == 6,
                   "Q2: 0x30 is the same budget");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x40u, -1, -1, &pc, &hc) == 64 && pc == 44 && hc == 5,
                   "Q2: four starting hearts is 44 + 5");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x20u, -1, -1, &pc, &hc) == 72 && pc == 44 && hc == 7,
                   "Q2: two starting hearts is 44 + 7");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x130u, -1, -1, &pc, &hc) == 4 && pc == 4 && hc == 0,
                   "Q2: nineteen starting hearts leaves one heart of pieces");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x140u, -1, -1, &pc, &hc) == 0 && pc == 0 && hc == 0,
                   "Q2: twenty starting hearts leaves nothing to place");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x30u, 30, 20, &pc, &hc) == 68 && pc == 30 && hc == 9,
                   "Q2: fourteen missing pieces move three whole hearts to containers (never past the budget)");
        SQP_ASSERT(Combo_SharedQuantityHealthBudget(0x30u, 88, 4, &pc, &hc) == 68 && pc == 52 && hc == 4,
                   "Q2: two missing containers move eight pieces the other way");

        const ComboSharedQuantityPolicy tier = { RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX, RSBS_SHARED_QTY_BUDGET_TIER, 0u };
        const ComboSharedQuantityPolicy dd = { RSBS_SHARED_QTY_TRIM_TO_SHARED_MAX, RSBS_SHARED_QTY_BUDGET_TIER, 1u };
        const ComboSharedQuantityPolicy keep = { RSBS_SHARED_QTY_KEEP_ALL, RSBS_SHARED_QTY_BUDGET_NONE, 0u };
        SQP_ASSERT(Combo_SharedQuantityTierBudget(&tier, 3, 3) == 3, "Q2: equal ceilings 3/3 keep 3");
        SQP_ASSERT(Combo_SharedQuantityTierBudget(&tier, 2, 0) == 2, "Q2: a family only one game holds keeps it");
        SQP_ASSERT(Combo_SharedQuantityTierBudget(&tier, 2, 1) == -1 && Combo_SharedQuantityTierBudget(&tier, 1, 2) == -1,
                   "Q2: UNEQUAL ceilings keep every copy");
        SQP_ASSERT(Combo_SharedQuantityTierBudget(&dd, 1, 1) == 1 && Combo_SharedQuantityTierBudget(&dd, 2, 2) == 1,
                   "Q2: double defense is capped at one copy");
        SQP_ASSERT(Combo_SharedQuantityTierBudget(&keep, 3, 3) == -1, "Q2: a KEEP_ALL policy has no tier budget");
        // The real rows' ceilings (the give paths' tops): MM's pool holds two bomb
        // bags (its first is a vanilla shop's) but its ceiling is 3 like OoT's, so
        // 3/2 is an EQUAL-ceiling family trimmed to 3; the hookshot 2/1 is not; the
        // wallet is 2/2 until OoT's pool carries the tycoon's copy.
        ComboSharedQuantityPolicy real;
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_BOMB_BAG_TIER, &real) &&
                       Combo_SharedQuantityTierBudget(&real, 3, 2) == 3,
                   "Q2: bomb bags 3 (OoT) / 2 (MM) share ceiling 3 and keep 3");
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_HOOKSHOT_TIER, &real) &&
                       Combo_SharedQuantityTierBudget(&real, 2, 1) == -1,
                   "Q2: the hookshot's ceilings (2 vs 1) differ: every copy is kept");
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_WALLET_TIER, &real) &&
                       Combo_SharedQuantityTierBudget(&real, 2, 2) == 2 &&
                       Combo_SharedQuantityTierBudget(&real, 3, 2) == -1,
                   "Q2: wallets 2/2 keep 2; a tycoon OoT pool (3) raises OoT's ceiling past MM's and keeps all");
        SQP_ASSERT(Combo_SharedQuantityPolicyOf(RSBS_SHARED_RES_MAGIC_LEVEL, &real) &&
                       Combo_SharedQuantityTierBudget(&real, 2, 2) == 2,
                   "Q2: magic 2/2 keeps 2");

        int a = -1;
        int b = -1;
        Combo_SharedQuantitySplit(44, 36, 52, 0u, &a, &b);
        SQP_ASSERT(a == 18 && b == 26, "Q2: 44 pieces over 36/52 split 18/26 (proportional)");
        Combo_SharedQuantitySplit(6, 8, 4, 0u, &a, &b);
        SQP_ASSERT(a == 4 && b == 2, "Q2: 6 containers over 8/4 split 4/2");
        Combo_SharedQuantitySplit(1, 1, 1, 0u, &a, &b);
        SQP_ASSERT(a == 1 && b == 0, "Q2: an exact tie goes to OoT on an even tie seed");
        Combo_SharedQuantitySplit(1, 1, 1, 1u, &a, &b);
        SQP_ASSERT(a == 0 && b == 1, "Q2: ... and to MM on an odd one");
        Combo_SharedQuantitySplit(2, 10, 1, 0u, &a, &b);
        SQP_ASSERT(a == 1 && b == 1, "Q2: every holder keeps a copy when the budget allows one each");
        Combo_SharedQuantitySplit(9, 3, 3, 0u, &a, &b);
        SQP_ASSERT(a == 3 && b == 3, "Q2: a budget covering every copy keeps them all");
    }

    // ---- Q3: the trim over the shipped shape ---------------------------------------
    const SqpPool shipped = SqpShippedShape();
    const SqpComposed full = SqpCompose(shipped, 0x0A11u, RSBS_COMBO_QUANTITY_KEEP_ALL);
    const SqpComposed trim = SqpCompose(shipped, 0x0A11u);
    SQP_ASSERT(full.status == RSBS_COMBO_LOGIC_OK && trim.status == RSBS_COMBO_LOGIC_OK, "Q3: both composes accepted");
    printf("[TEST] shared-quantity-policy: Q3 untrimmed bag %d rows (pieces %d, containers %d, double defense %d, "
           "quiver %d, hookshot %d, bombchu %d); trimmed bag %d rows (pieces %d = OoT %d + MM %d, containers %d = OoT "
           "%d + MM %d, double defense %d, quiver %d = OoT %d + MM %d, hookshot %d, bombchu %d); trimmed rows OoT %d "
           "MM %d\n",
           full.res.bagCount, SqpBoth(full, kSqpPiece), SqpBoth(full, kSqpContainer), SqpBoth(full, kSqpDoubleDefense),
           SqpBoth(full, kSqpQuiver), SqpBoth(full, kSqpHookshot), SqpBoth(full, kSqpBombchu), trim.res.bagCount,
           SqpBoth(trim, kSqpPiece), SqpCount(trim, O, kSqpPiece), SqpCount(trim, M, kSqpPiece),
           SqpBoth(trim, kSqpContainer), SqpCount(trim, O, kSqpContainer), SqpCount(trim, M, kSqpContainer),
           SqpBoth(trim, kSqpDoubleDefense), SqpBoth(trim, kSqpQuiver), SqpCount(trim, O, kSqpQuiver),
           SqpCount(trim, M, kSqpQuiver), SqpBoth(trim, kSqpHookshot), SqpBoth(trim, kSqpBombchu),
           trim.res.perGame[O].rows[RSBS_COMBO_COMPOSE_TRIMMED], trim.res.perGame[M].rows[RSBS_COMBO_COMPOSE_TRIMMED]);
    // The red half first: the untrimmed bag is the defect.
    SQP_ASSERT(SqpBoth(full, kSqpPiece) == 88 && SqpBoth(full, kSqpContainer) == 12 &&
                   SqpBoth(full, kSqpDoubleDefense) == 2 && SqpBoth(full, kSqpQuiver) == 6 && full.trimmed.empty() &&
                   full.res.perGame[O].rows[RSBS_COMBO_COMPOSE_TRIMMED] == 0 &&
                   full.res.perGame[M].rows[RSBS_COMBO_COMPOSE_TRIMMED] == 0,
               "Q3 RED HALF: KEEP_ALL composes every copy: 88 pieces, 12 containers, 2 double defenses, 6 quivers");
    SQP_ASSERT(SqpBoth(trim, kSqpPiece) == 44 && SqpBoth(trim, kSqpContainer) == 6,
               "Q3: the trimmed bag holds exactly 44 pieces + 6 containers across both games");
    SQP_ASSERT(SqpCount(trim, O, kSqpPiece) == 18 && SqpCount(trim, M, kSqpPiece) == 26 &&
                   SqpCount(trim, O, kSqpContainer) == 4 && SqpCount(trim, M, kSqpContainer) == 2,
               "Q3: the spread is proportional, so both worlds keep heart checks");
    SQP_ASSERT(SqpBoth(trim, kSqpDoubleDefense) == 1, "Q3: exactly one double defense");
    SQP_ASSERT(SqpBoth(trim, kSqpQuiver) == 3 && SqpCount(trim, O, kSqpQuiver) >= 1 &&
                   SqpCount(trim, M, kSqpQuiver) >= 1,
               "Q3: a kind with N max and 2N copies keeps exactly N, at least one per game");
    SQP_ASSERT(SqpBoth(trim, kSqpHookshot) == 3 && SqpCount(trim, O, kSqpHookshot) == 2 &&
                   SqpCount(trim, M, kSqpHookshot) == 1,
               "Q3: the unequal-ceiling hookshot (2 vs 1) keeps every copy");
    SQP_ASSERT(SqpBoth(trim, kSqpOcarina) == 3, "Q3: the unarmed shared ocarina keeps every copy");
    SQP_ASSERT(SqpBoth(trim, kSqpBombchu) == 10 && SqpBoth(full, kSqpBombchu) == 10,
               "Q3: bombchus (a KEEP_ALL kind, PROGRESSION) are untouched");
    SQP_ASSERT(trim.res.perGame[O].rows[RSBS_COMBO_COMPOSE_RENEWABLE] == 20 &&
                   trim.res.perGame[M].rows[RSBS_COMBO_COMPOSE_RENEWABLE] == 20 &&
                   full.res.perGame[O].rows[RSBS_COMBO_COMPOSE_RENEWABLE] == 20 &&
                   full.res.perGame[M].rows[RSBS_COMBO_COMPOSE_RENEWABLE] == 20,
               "Q3: rupees (KEEP_ALL, renewable) are untouched: every one still goes to its own game's junk pass");
    for (const int idx : trim.trimmed) {
        SQP_ASSERT(shipped.rows[(size_t)idx].item.id != kSqpRupee && shipped.rows[(size_t)idx].item.id != kSqpBombchu,
                   "Q3: no KEEP_ALL row is ever trimmed");
    }
    // Filler accounting: per origin, what the trim removed == what the untrimmed
    // bag held beyond the trimmed one == the listed trimmed rows of that origin.
    const uint8_t games[2] = { O, M };
    for (const uint8_t g : games) {
        int untrimmedG = 0;
        int trimmedG = 0;
        int listedG = 0;
        for (const ComboLogicBagItem& b : full.bag) {
            untrimmedG += (b.item.originGame == g) ? 1 : 0;
        }
        for (const ComboLogicBagItem& b : trim.bag) {
            trimmedG += (b.item.originGame == g) ? 1 : 0;
        }
        for (const int idx : trim.trimmed) {
            listedG += (shipped.rows[(size_t)idx].item.originGame == g) ? 1 : 0;
        }
        SQP_ASSERT(trim.res.perGame[g].rows[RSBS_COMBO_COMPOSE_TRIMMED] == untrimmedG - trimmedG &&
                       listedG == untrimmedG - trimmedG,
                   "Q3: every removed copy is counted as filler under its ORIGIN game and listed");
        int sum = 0;
        for (int d = 0; d < RSBS_COMBO_COMPOSE_COUNT; ++d) {
            sum += trim.res.perGame[g].rows[d];
        }
        int poolG = 0;
        for (const ComboLogicPoolRow& r : shipped.rows) {
            poolG += (r.item.originGame == g) ? 1 : 0;
        }
        SQP_ASSERT(sum == poolG, "Q3: every pool row keeps exactly one disposition");
    }
    SQP_ASSERT((int)trim.trimmed.size() == 44 + 6 + 1 + 3, "Q3: 44 pieces + 6 containers + 1 DD + 3 quivers trimmed");
    for (size_t i = 1; i < trim.poolIndex.size(); ++i) {
        SQP_ASSERT(trim.poolIndex[i] > trim.poolIndex[i - 1], "Q3: the bag is a STABLE filter of the pool");
    }
    for (size_t i = 1; i < trim.trimmed.size(); ++i) {
        SQP_ASSERT(trim.trimmed[i] > trim.trimmed[i - 1], "Q3: the trimmed list is in pool order");
    }
    // Determinism and the sensitivity control.
    const SqpComposed again = SqpCompose(shipped, 0x0A11u);
    SQP_ASSERT(SqpSameBag(trim, again), "Q3: the same seed composes the same bag and trims the same rows");
    int differing = 0;
    for (uint32_t s = 1; s <= 8; ++s) {
        const SqpComposed other = SqpCompose(shipped, 0x0A11u + s * 0x1000193u);
        SQP_ASSERT(other.status == RSBS_COMBO_LOGIC_OK && SqpBoth(other, kSqpPiece) == 44 &&
                       SqpBoth(other, kSqpContainer) == 6 && SqpBoth(other, kSqpDoubleDefense) == 1 &&
                       SqpBoth(other, kSqpQuiver) == 3,
                   "Q3: every seed keeps the same counts");
        differing += (other.trimmed != trim.trimmed) ? 1 : 0;
    }
    printf("[TEST] shared-quantity-policy: Q3 sensitivity: %d of 8 other seeds trimmed a different set of rows\n",
           differing);
    SQP_ASSERT(differing > 0, "Q3: a different seed trims different copies (the sensitivity control)");
    // Starting health.
    const SqpComposed start4 = SqpCompose(shipped, 0x0A11u, 0u, 0x30u, 0x40u);
    SQP_ASSERT(SqpBoth(start4, kSqpPiece) == 44 && SqpBoth(start4, kSqpContainer) == 5,
               "Q3: a four-heart starting bar (the larger of the two) keeps 44 + 5");
    // An unknown quantity flag is refused.
    {
        ComboLogicComposeRequest bad;
        memset(&bad, 0, sizeof(bad));
        bad.rows = shipped.rows.data();
        bad.rowCount = (int)shipped.rows.size();
        bad.quantityFlags = 0x8000u;
        ComboLogicBagItem sink[1];
        SQP_ASSERT(Combo_Logic_ComposeBag(&bad, sink, 1, nullptr, nullptr) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                   "Q3: an unknown quantity flag is refused, never ignored");
    }

    // ---- Q4: plentiful surplus is relative to the trimmed count ----------------------
    {
        SqpPool plentiful = SqpShippedShape();
        plentiful.Add(O, kSqpPiece, 4, RSBS_COMBO_POOL_PLENTIFUL);
        plentiful.Add(M, kSqpPiece, 2, RSBS_COMBO_POOL_PLENTIFUL);
        plentiful.Add(O, kSqpQuiver, 1, RSBS_COMBO_POOL_PLENTIFUL);
        plentiful.Add(M, kSqpQuiver, 1, RSBS_COMBO_POOL_PLENTIFUL);
        plentiful.Add(O, kSqpHookshot, 1, RSBS_COMBO_POOL_PLENTIFUL);
        plentiful.Add(O, kSqpDoubleDefense, 1, RSBS_COMBO_POOL_PLENTIFUL);
        const SqpComposed pc = SqpCompose(plentiful, 0x0B22u);
        printf("[TEST] shared-quantity-policy: Q4 plentiful: required pieces %d, surplus pieces %d; required quivers "
               "%d, surplus quivers %d; hookshots %d (surplus %d); double defense required %d surplus %d\n",
               SqpBoth(pc, kSqpPiece, 0), SqpBoth(pc, kSqpPiece, 1), SqpBoth(pc, kSqpQuiver, 0),
               SqpBoth(pc, kSqpQuiver, 1), SqpBoth(pc, kSqpHookshot), SqpBoth(pc, kSqpHookshot, 1),
               SqpBoth(pc, kSqpDoubleDefense, 0), SqpBoth(pc, kSqpDoubleDefense, 1));
        SQP_ASSERT(pc.status == RSBS_COMBO_LOGIC_OK, "Q4: accepted");
        SQP_ASSERT(SqpBoth(pc, kSqpPiece, 0) == 44 && SqpBoth(pc, kSqpContainer, 0) == 6 &&
                       SqpBoth(pc, kSqpQuiver, 0) == 3 && SqpBoth(pc, kSqpDoubleDefense, 0) == 1,
                   "Q4: the REQUIRED trim is unchanged by plentiful");
        SQP_ASSERT(SqpBoth(pc, kSqpPiece, 1) == 4, "Q4: surplus pieces keep one game's worth (max(4, 2))");
        SQP_ASSERT(SqpBoth(pc, kSqpQuiver, 1) == 1, "Q4: surplus quivers keep one game's worth (max(1, 1))");
        SQP_ASSERT(SqpBoth(pc, kSqpDoubleDefense, 1) == 1, "Q4: the one-sided surplus double defense stays");
        SQP_ASSERT(SqpBoth(pc, kSqpHookshot) == 4 && SqpBoth(pc, kSqpHookshot, 1) == 1,
                   "Q4: a family kept whole for unequal ceilings keeps its surplus too");
    }

    // ---- Q5: the play-side check through the real shared carrier ---------------------
    {
        const std::vector<SqpHeartRow> trimmedHearts = SqpHeartRowsInterleaved(trim.bag.data(), (int)trim.bag.size());
        const std::vector<SqpHeartRow> fullHearts = SqpHeartRowsInterleaved(full.bag.data(), (int)full.bag.size());
        const SqpWalk wt = SqpWalkHealth(trimmedHearts, 0x30u);
        const SqpWalk wf = SqpWalkHealth(fullHearts, 0x30u);
        // A second order: every OoT row first, then MM's.
        std::vector<SqpHeartRow> ootFirst;
        for (const SqpHeartRow& r : trimmedHearts) {
            if (r.origin == O) {
                ootFirst.push_back(r);
            }
        }
        for (const SqpHeartRow& r : trimmedHearts) {
            if (r.origin == M) {
                ootFirst.push_back(r);
            }
        }
        const SqpWalk wt2 = SqpWalkHealth(ootFirst, 0x30u);
        printf("[TEST] shared-quantity-policy: Q5 hearts: TRIMMED %d pickups, %d dead, bar 0x%X (OoT-first order: %d "
               "dead, bar 0x%X); UNTRIMMED %d pickups, %d dead, bar 0x%X\n",
               wt.pickups, wt.dead, (unsigned)wt.finalValue, wt2.dead, (unsigned)wt2.finalValue, wf.pickups, wf.dead,
               (unsigned)wf.finalValue);
        SQP_ASSERT(wt.pickups == 50 && wt.dead == 0 && wt.finalValue == (int)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS,
                   "Q5: the trimmed pool's 50 heart pickups end the bar at exactly 320 with ZERO dead pickups");
        SQP_ASSERT(wt2.dead == 0 && wt2.finalValue == (int)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS,
                   "Q5: ... in a second pickup order too");
        SQP_ASSERT(wf.pickups == 100 && wf.dead > 0 && wf.finalValue == (int)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS,
                   "Q5 RED HALF: the untrimmed pool clamps: pickups past the 20-heart bar are dead");

        // The quiver (equal ceilings 3/3): the trimmed copies all count, in both orders.
        const std::vector<uint8_t> qTrim = SqpOrigins(trim, kSqpQuiver);
        const std::vector<uint8_t> qFull = SqpOrigins(full, kSqpQuiver);
        const std::vector<uint8_t> qTrimRev(qTrim.rbegin(), qTrim.rend());
        const SqpWalk qt = SqpWalkTier(RSBS_SHARED_RES_QUIVER_TIER, qTrim, 3, 3);
        const SqpWalk qt2 = SqpWalkTier(RSBS_SHARED_RES_QUIVER_TIER, qTrimRev, 3, 3);
        const SqpWalk qf = SqpWalkTier(RSBS_SHARED_RES_QUIVER_TIER, qFull, 3, 3);
        printf("[TEST] shared-quantity-policy: Q5 quiver: TRIMMED %d pickups %d dead tier %d (reversed: %d dead); "
               "UNTRIMMED %d pickups %d dead tier %d\n",
               qt.pickups, qt.dead, qt.finalValue, qt2.dead, qf.pickups, qf.dead, qf.finalValue);
        SQP_ASSERT(qt.dead == 0 && qt.finalValue == 3 && qt2.dead == 0 && qt2.finalValue == 3,
                   "Q5: the trimmed quivers reach tier 3 with no dead pickup, in either order");
        SQP_ASSERT(qf.dead == 3 && qf.finalValue == 3, "Q5 RED HALF: the untrimmed six quivers waste three");

        const SqpWalk dt = SqpWalkTier(RSBS_SHARED_RES_DOUBLE_DEFENSE, SqpOrigins(trim, kSqpDoubleDefense), 1, 1);
        const SqpWalk df = SqpWalkTier(RSBS_SHARED_RES_DOUBLE_DEFENSE, SqpOrigins(full, kSqpDoubleDefense), 1, 1);
        SQP_ASSERT(dt.pickups == 1 && dt.dead == 0 && dt.finalValue == 1 && df.pickups == 2 && df.dead == 1 &&
                       df.finalValue == 1,
                   "Q5: the one double defense is live; the untrimmed second is dead (red half)");

        // WHY THE HOOKSHOT IS KEPT WHOLE: its ceilings are 2 (OoT) and 1 (MM).
        const SqpWalk hMmFirst = SqpWalkTier(RSBS_SHARED_RES_HOOKSHOT_TIER, { M, O, O }, 2, 1);
        const SqpWalk hOoTFirst = SqpWalkTier(RSBS_SHARED_RES_HOOKSHOT_TIER, { O, O, M }, 2, 1);
        const SqpWalk hTrimBad = SqpWalkTier(RSBS_SHARED_RES_HOOKSHOT_TIER, { O, M }, 2, 1);
        printf("[TEST] shared-quantity-policy: Q5 hookshot kept whole: MM-first %d dead tier %d, OoT-first %d dead "
               "tier %d; a naive trim to one copy each, OoT first: tier %d\n",
               hMmFirst.dead, hMmFirst.finalValue, hOoTFirst.dead, hOoTFirst.finalValue, hTrimBad.finalValue);
        SQP_ASSERT(hMmFirst.finalValue == 2 && hOoTFirst.finalValue == 2 && hMmFirst.dead == 1 && hOoTFirst.dead == 1,
                   "Q5: the whole hookshot family reaches the longshot in every order, at exactly one dead pickup");
        SQP_ASSERT(hTrimBad.finalValue == 1,
                   "Q5: a trim to one OoT + one MM hookshot loses the longshot when OoT's comes first (the hazard the "
                   "unequal-ceiling rule exists for)");
    }

    printf("[TEST] PASS: shared-quantity-policy\n");
    return TEST_PASS;
}
