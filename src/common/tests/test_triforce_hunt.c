/**
 * @file test_triforce_hunt.c
 * @brief combo-triforce-hunt: ONE shared triforce piece count across both
 *        worlds (ADR 0010 answer O10). redship tier: display-free, ROM-free.
 *
 * WHAT IS LOCKED, leg by leg:
 *
 *   T1 THE FORMAT. The frozen record is 4 bytes at .redsave offset 896, the
 *      shared kind is 19, and the watermark table covers it.
 *   T2 THE RULE. Each half's pieces stay in its own pool; the combo total is
 *      their sum; the combo requirement is the sum of the halves' requirements.
 *      No pieces anywhere, an incoherent half, and a combo total past OoT's
 *      8-bit counter are refused — including MM's own 1000-piece slider value,
 *      which a byte would have truncated to a plausible 232.
 *   T3 THE FREEZE. Only a world whose frozen goal is triforce-hunt stores a
 *      record; every other world (the shipped default among them) stores four
 *      zero bytes; an indescribable hunt stores nothing and reports why.
 *   T4 DIVERGENCE IS REFUSED. A stored record that contradicts its goal (a hunt
 *      with no valid record, a record under another goal, bytes the rule could
 *      never write) is the damage bit RSBS_COMBO_DIVERGE_TRIFORCE on the arrival
 *      refusal; a half that is not what its game's frozen settings produce
 *      diverges.
 *   T5 THE DISCIPLINE PIN (shared-resource-discipline-pin): only a LOWER harvest
 *      after a FULL apply tells MONOTONIC from CONSUMABLE. A consumable kind
 *      would take the lower value's delta and lose pieces; this kind must keep
 *      the count.
 *   T6 ARMING. A world that is not a triforce hunt grows no slot and applies
 *      nothing: its pool is byte-identical to one written before the kind.
 *   T7 THE CROSS-GAME SUM through BOTH GAMES' REAL SHIMS: collect k in OoT and m
 *      in MM, and after the switch both counters read k+m (and again after more
 *      collects), capped at the combo total.
 *   T8 THE WIN DECISION both ports' give arms call: unarmed, each game's own
 *      `==`; armed, only the combo requirement, once.
 *   T9 THE GOAL PREDICATE over stub engines: the coordinator's triforce-hunt
 *      expression is the SUM of both engines' `triforcePieces` (the MM half
 *      behind the arrival gate) against the frozen requirement — never either
 *      half's goalReached — and a fill proves it, or reports it unprovable, or
 *      refuses a pair of engines without the query.
 *
 * The real give arms themselves are locked in the rando tier
 * (rando-triforce-hunt-win), because both need a booted game.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../triforce_hunt.h"

#include <cstdio>
#include <cstring>

// The two games' shim-driving helpers (games/oot/soh/oot_triforce_hunt_test.cpp,
// games/mm/2s2h/mm_triforce_hunt_test.cpp) and the real shims they drive
// (each game's GameExports_SingleExe.cpp).
extern "C" {
void OoT_TriforceHuntTest_ArmLive(void);
void OoT_TriforceHuntTest_SetCount(uint16_t count);
int OoT_TriforceHuntTest_Count(void);
void MM_TriforceHuntTest_ArmLive(void);
void MM_TriforceHuntTest_SetCount(uint16_t count);
int MM_TriforceHuntTest_Count(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
}

#define TFH_ASSERT(cond, msg)                                               \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            TfRestoreEngines();                                             \
            ComboContext_Init();                                            \
            Combo_ResetSharedResourceWatermarks();                          \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

// Whatever the process registered before this row (the real engines, from
// their file-scope registrars) is put back on every exit, so a later row in the
// same `--test all` process meets the registry it would have met without us.
const ComboLogicEngine* gTfSavedOoT = NULL;
const ComboLogicEngine* gTfSavedMM = NULL;

void TfRestoreEngines() {
    Combo_Logic_ResetPlacements();
    Combo_Logic_RegisterEngine(GAME_OOT, gTfSavedOoT);
    Combo_Logic_RegisterEngine(GAME_MM, gTfSavedMM);
}

// ============================================================================
// Fixtures
// ============================================================================

/** Freeze a paired world with combo goal `goal` (the ocarina row's shape). */
void TfFreezeWorld(uint8_t goal) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x7F0CE010u;
    gComboCtx.sharedRandoSettingsHash = 0x7F0C5E77u;
    gComboCtx.mmProfileDigest = 0x7F0C4D4Du;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.goal = goal;
    Combo_FreezeComboSettings(&rec);
}

ComboTriforceHalf TfHalf(uint16_t total, uint16_t required) {
    ComboTriforceHalf h;
    h.total = total;
    h.required = required;
    return h;
}

int TfPool(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

bool TfRecordZero(const ComboTriforceRecord* r) {
    return r->totalOoT == 0 && r->requiredOoT == 0 && r->totalMM == 0 && r->requiredMM == 0;
}

// ---- The stub engine (T9) --------------------------------------------------
//
// A world with no graph: every host is reached, and the crossing is a fixed
// flag. What the stub DOES model is the only thing T9 is about — which copies of
// the piece this half holds in a round: one per assumed copy, plus one per own
// piece the coordinator placed on this side (harvested by `expand`, once per
// host per round). goalReached is a fixed ANSWER the test sets, so a round that
// read it instead of the count would be seen.

const uint16_t kTfOotPiece = 111; // OoT-side id space
const uint16_t kTfMmPiece = 222;  // MM-side id space, disjoint on purpose (ADR 0002)
const int kTfMaxHosts = 8;

struct TfEngine {
    uint8_t game;
    uint16_t piece;
    int crossing;
    int goalAnswer;
    uint16_t hosts[kTfMaxHosts];
    int hostCount;
    uint16_t placedHost[kTfMaxHosts];
    SharedItem placedItem[kTfMaxHosts];
    bool harvested[kTfMaxHosts];
    int placedCount;
    bool inRound;
    int pieces;
    int pieceQueries;
};

TfEngine gTfOoT;
TfEngine gTfMM;
ComboLogicEngine gTfVtOoT;
ComboLogicEngine gTfVtMM;

int TfBegin(void* self) {
    TfEngine* e = (TfEngine*)self;
    e->inRound = true;
    e->pieces = 0;
    for (int i = 0; i < kTfMaxHosts; ++i) {
        e->harvested[i] = false;
    }
    return 1;
}
void TfAssume(void* self, uint16_t id) {
    TfEngine* e = (TfEngine*)self;
    if (id == e->piece) {
        e->pieces++;
    }
}
int TfExpand(void* self) {
    TfEngine* e = (TfEngine*)self;
    int changed = 0;
    for (int i = 0; i < e->placedCount; ++i) {
        if (!e->harvested[i] && e->placedItem[i].originGame == e->game) {
            e->harvested[i] = true;
            if (e->placedItem[i].id == e->piece) {
                e->pieces++;
            }
            changed = 1;
        }
    }
    return changed;
}
int TfCrossing(void* self) {
    return ((TfEngine*)self)->crossing;
}
bool TfIsHost(const TfEngine* e, uint16_t host) {
    for (int i = 0; i < e->hostCount; ++i) {
        if (e->hosts[i] == host) {
            return true;
        }
    }
    return false;
}
bool TfIsPlaced(const TfEngine* e, uint16_t host) {
    for (int i = 0; i < e->placedCount; ++i) {
        if (e->placedHost[i] == host) {
            return true;
        }
    }
    return false;
}
int TfCheckReached(void* self, uint16_t host) {
    return TfIsHost((TfEngine*)self, host) ? 1 : 0;
}
int TfEmptyHosts(void* self, uint16_t* out, int cap) {
    const TfEngine* e = (const TfEngine*)self;
    int total = 0;
    for (int i = 0; i < e->hostCount; ++i) {
        if (TfIsPlaced(e, e->hosts[i])) {
            continue;
        }
        if (out != NULL && total < cap) {
            out[total] = e->hosts[i];
        }
        total++;
    }
    return total;
}
int TfGoal(void* self) {
    return ((TfEngine*)self)->goalAnswer;
}
int TfPlace(void* self, uint16_t host, SharedItem item) {
    TfEngine* e = (TfEngine*)self;
    for (int i = 0; i < e->placedCount; ++i) {
        if (e->placedHost[i] == host) {
            return (e->placedItem[i].originGame == item.originGame && e->placedItem[i].id == item.id) ? 1 : 0;
        }
    }
    if (e->placedCount >= kTfMaxHosts || !TfIsHost(e, host)) {
        return 0;
    }
    e->placedHost[e->placedCount] = host;
    e->placedItem[e->placedCount] = item;
    e->harvested[e->placedCount] = false;
    e->placedCount++;
    return 1;
}
void TfClear(void* self) {
    ((TfEngine*)self)->placedCount = 0;
}
void TfEnd(void* self) {
    ((TfEngine*)self)->inRound = false;
}
int TfPieces(void* self) {
    TfEngine* e = (TfEngine*)self;
    e->pieceQueries++;
    return e->inRound ? e->pieces : 0;
}

void TfResetEngine(TfEngine* e, uint8_t game, uint16_t piece, uint16_t hostBase, int hosts) {
    memset(e, 0, sizeof(*e));
    e->game = game;
    e->piece = piece;
    e->crossing = 1;
    for (int i = 0; i < hosts && i < kTfMaxHosts; ++i) {
        e->hosts[i] = (uint16_t)(hostBase + i);
    }
    e->hostCount = hosts < kTfMaxHosts ? hosts : kTfMaxHosts;
}

void TfFillVtable(ComboLogicEngine* vt, TfEngine* e, bool withPieces) {
    memset(vt, 0, sizeof(*vt));
    vt->abiVersion = RSBS_COMBO_LOGIC_ENGINE_ABI;
    vt->self = e;
    vt->beginQuery = TfBegin;
    vt->assumeOwnItem = TfAssume;
    vt->expand = TfExpand;
    vt->crossingOpen = TfCrossing;
    vt->checkReached = TfCheckReached;
    vt->reachedEmptyHosts = TfEmptyHosts;
    vt->allEmptyHosts = TfEmptyHosts;
    vt->goalReached = TfGoal;
    vt->place = TfPlace;
    vt->clearPlacements = TfClear;
    vt->endQuery = TfEnd;
    vt->snapshot = NULL;
    vt->restore = NULL;
    vt->triforcePieces = withPieces ? TfPieces : NULL;
}

bool TfInstall(bool ootPieces, bool mmPieces) {
    TfFillVtable(&gTfVtOoT, &gTfOoT, ootPieces);
    TfFillVtable(&gTfVtMM, &gTfMM, mmPieces);
    return Combo_Logic_RegisterEngine(GAME_OOT, &gTfVtOoT) && Combo_Logic_RegisterEngine(GAME_MM, &gTfVtMM);
}

ComboLogicBagItem TfRow(uint8_t origin, uint16_t id) {
    ComboLogicBagItem row;
    memset(&row, 0, sizeof(row));
    row.item.originGame = origin;
    row.item.id = id;
    row.itemClass = 0;
    row.bagFlags = 0;
    return row;
}

int TfRound(const ComboLogicBagItem* assumed, int count, uint8_t goal, uint16_t required,
            ComboLogicRoundResult* out) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = assumed;
    req.assumedCount = count;
    req.goal = goal;
    req.triforceRequired = required;
    return Combo_Logic_RunRound(&req, out);
}

int TfFill(const ComboLogicBagItem* bag, int count, uint16_t required, ComboLogicFillResult* out) {
    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = count;
    req.goal = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
    req.logicRung = (uint8_t)RSBS_COMBO_RUNG_BEATABLE;
    req.seed = 0x7F0C0010u;
    req.triforceRequired = required;
    return Combo_Logic_RunFill(&req, out);
}

} // namespace

TestResult Test_ComboTriforceHunt(void) {
    printf("[TEST] combo-triforce-hunt: one shared triforce piece count across both worlds (ADR 0010 O10)\n");

    // ---- T1: the format -------------------------------------------------------
    TFH_ASSERT(sizeof(ComboTriforceRecord) == 4u, "the triforce record is not 4 bytes");
    TFH_ASSERT(offsetof(ComboContext, comboTriforce) == 896u, "the triforce record moved off .redsave offset 896");
    TFH_ASSERT((uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES == 19u,
               "RSBS_SHARED_RES_TRIFORCE_PIECES moved off 19 - these values are .redsave format, append-only");
    TFH_ASSERT(RSBS_SHARED_RES_KIND_COUNT > (unsigned)RSBS_SHARED_RES_TRIFORCE_PIECES,
               "the watermark table does not cover the triforce kind; its harvest would be dropped");

    // ---- T2: the rule ----------------------------------------------------------
    {
        ComboTriforceRecord r;
        ComboTriforceHalf oot = TfHalf(30, 20);
        ComboTriforceHalf mm = TfHalf(15, 15);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_OK, "two coherent halves must resolve");
        TFH_ASSERT(r.totalOoT == 30 && r.requiredOoT == 20 && r.totalMM == 15 && r.requiredMM == 15,
                   "each half's pieces must stay in its own pool, as its own settings said");
        TFH_ASSERT(Combo_TriforceRecordTotal(&r) == 45 && Combo_TriforceRecordRequired(&r) == 35,
                   "the combo total / requirement is not the sum of the halves");

        mm = TfHalf(0, 0);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_OK && r.totalMM == 0 && r.requiredMM == 0,
                   "a half whose own hunt is off contributes no pieces and no requirement");
        oot = TfHalf(0, 0);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_ERR_NO_PIECES && TfRecordZero(&r),
                   "a triforce hunt with no pieces in either pool must be refused, and store nothing");
        oot = TfHalf(10, 11);
        mm = TfHalf(5, 5);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_ERR_BAD_HALF,
                   "a half requiring more than its own pool holds must be refused");
        oot = TfHalf(10, 0);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_ERR_BAD_HALF,
                   "a hunt that is on with a zero requirement must be refused");
        oot = TfHalf(0, 3);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_ERR_BAD_HALF,
                   "a hunt that is off with a nonzero requirement must be refused");
        oot = TfHalf(100, 100);
        mm = TfHalf(155, 1);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_OK && Combo_TriforceRecordTotal(&r) == 255,
                   "a combo total of exactly 255 fits OoT's 8-bit counter");
        mm = TfHalf(156, 1);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_ERR_OVER_CAP && TfRecordZero(&r),
                   "a combo total of 256 would wrap OoT's counter and must be refused");
        oot = TfHalf(0, 0);
        mm = TfHalf(1000, 20);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_ERR_OVER_CAP,
                   "MM's own 1000-piece setting must be REFUSED, not truncated to a plausible 232");
        TFH_ASSERT(Combo_TriforceResolve(NULL, &mm, &r) == RSBS_TRIFORCE_ERR_BAD_REQUEST,
                   "a NULL half must be refused");
        TFH_ASSERT(strcmp(Combo_TriforceStatusName(RSBS_TRIFORCE_ERR_OVER_CAP), "over-cap") == 0,
                   "the status names drifted");
    }

    // ---- T3: the freeze --------------------------------------------------------
    const ComboTriforceHalf kOot = TfHalf(30, 20);
    const ComboTriforceHalf kMm = TfHalf(15, 15);
    {
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
        TFH_ASSERT(gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, "fixture: goal did not freeze");
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK,
                   "a creation under another goal must succeed");
        TFH_ASSERT(TfRecordZero(&gComboCtx.comboTriforce),
                   "a world that is not a triforce hunt must store four zero bytes, whatever the halves say");
        TFH_ASSERT(!Combo_TriforceHuntArmed() && Combo_TriforceHuntRequired() == 0 && Combo_TriforceHuntTotal() == 0,
                   "a world that is not a triforce hunt must not arm the combo hunt");

        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "a coherent hunt must freeze");
        TFH_ASSERT(gComboCtx.comboTriforce.totalOoT == 30 && gComboCtx.comboTriforce.requiredMM == 15,
                   "the frozen record is not the resolved record");
        TFH_ASSERT(Combo_TriforceHuntArmed() && Combo_TriforceHuntRequired() == 35 && Combo_TriforceHuntTotal() == 45,
                   "a frozen triforce-hunt world must arm the combo hunt with the combo requirement and total");

        // An indescribable hunt stores nothing — not even the previous world's.
        const ComboTriforceHalf off = TfHalf(0, 0);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&off, &off) == RSBS_TRIFORCE_ERR_NO_PIECES,
                   "a triforce-hunt creation with no pieces anywhere must report why");
        TFH_ASSERT(TfRecordZero(&gComboCtx.comboTriforce) && !Combo_TriforceHuntArmed(),
                   "a refused hunt must leave no record behind (not even the previous creation's)");

        // An unfrozen world freezes nothing.
        ComboContext_Init();
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK &&
                       TfRecordZero(&gComboCtx.comboTriforce),
                   "with no frozen combo record there is no hunt to freeze");
    }

    // ---- T4: divergence is refused ----------------------------------------------
    {
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: hunt did not freeze");
        TFH_ASSERT(Combo_TriforceRecordDivergence(&gComboCtx.comboSettings, &gComboCtx.comboTriforce) == 0,
                   "a consistent hunt record reads as divergent");
        TFH_ASSERT((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_TRIFORCE) == 0,
                   "the arrival refusal flags a consistent hunt record");

        // Bytes the rule could never write: OoT requiring more than its pool.
        const ComboTriforceRecord good = gComboCtx.comboTriforce;
        gComboCtx.comboTriforce.requiredOoT = (uint8_t)(gComboCtx.comboTriforce.totalOoT + 1);
        TFH_ASSERT(Combo_TriforceRecordDivergence(&gComboCtx.comboSettings, &gComboCtx.comboTriforce) ==
                       RSBS_COMBO_DIVERGE_TRIFORCE,
                   "a stored record the rule could never write must diverge");
        TFH_ASSERT((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_TRIFORCE) != 0,
                   "the arrival refusal (Combo_ComboSettingsDivergence) does not carry the triforce bit");
        TFH_ASSERT(!Combo_TriforceHuntArmed(), "a damaged record must not arm the hunt");

        // A hunt goal with no record at all.
        memset(&gComboCtx.comboTriforce, 0, sizeof(gComboCtx.comboTriforce));
        TFH_ASSERT(Combo_TriforceRecordDivergence(&gComboCtx.comboSettings, &gComboCtx.comboTriforce) ==
                       RSBS_COMBO_DIVERGE_TRIFORCE,
                   "a frozen triforce-hunt goal with no record must diverge");

        // A record under another goal, and a stray requirement byte alone.
        ComboSettingsRecord beatBoth = gComboCtx.comboSettings;
        beatBoth.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
        TFH_ASSERT(Combo_TriforceRecordDivergence(&beatBoth, &good) == RSBS_COMBO_DIVERGE_TRIFORCE,
                   "a hunt record under a beat-both goal must diverge");
        ComboTriforceRecord stray;
        memset(&stray, 0, sizeof(stray));
        stray.requiredMM = 1;
        TFH_ASSERT(Combo_TriforceRecordDivergence(&beatBoth, &stray) == RSBS_COMBO_DIVERGE_TRIFORCE,
                   "a stray requirement byte with a zero total must diverge - Present() alone would not see it");
        ComboSettingsRecord absent;
        memset(&absent, 0, sizeof(absent));
        TFH_ASSERT(Combo_TriforceRecordDivergence(&absent, &good) == RSBS_COMBO_DIVERGE_TRIFORCE,
                   "a hunt record with no combo record at all must diverge");
        memset(&stray, 0, sizeof(stray));
        TFH_ASSERT(Combo_TriforceRecordDivergence(&absent, &stray) == 0 &&
                       Combo_TriforceRecordDivergence(&beatBoth, &stray) == 0,
                   "the all-zero record every non-hunt and legacy world stores must NOT diverge");

        TFH_ASSERT(Combo_ComboSettingsDivergenceIsDamage(RSBS_COMBO_DIVERGE_TRIFORCE),
                   "a self-contradicting triforce record is damage to the stored identity, not a session change");
        TFH_ASSERT(strcmp(Combo_ComboSettingsDivergenceFieldName(RSBS_COMBO_DIVERGE_TRIFORCE), "triforceHunt") == 0,
                   "the refusal must name the rule");

        // A half re-derived from its own game's frozen settings.
        const ComboTriforceHalf mmSame = TfHalf(15, 15);
        const ComboTriforceHalf mmRequiredMoved = TfHalf(15, 14);
        const ComboTriforceHalf mmTotalMoved = TfHalf(16, 15);
        const ComboTriforceHalf ootSame = TfHalf(30, 20);
        TFH_ASSERT(!Combo_TriforceHalfDiverges(&good, GAME_MM, &mmSame) &&
                       !Combo_TriforceHalfDiverges(&good, GAME_OOT, &ootSame),
                   "a half equal to what the record froze reads as divergent");
        TFH_ASSERT(Combo_TriforceHalfDiverges(&good, GAME_MM, &mmRequiredMoved),
                   "a half whose REQUIREMENT moved since creation must diverge");
        TFH_ASSERT(Combo_TriforceHalfDiverges(&good, GAME_MM, &mmTotalMoved),
                   "a half whose TOTAL moved since creation must diverge");
        TFH_ASSERT(!Combo_TriforceHalfDiverges(&stray, GAME_MM, &mmRequiredMoved),
                   "an absent record diverges from nothing");
    }

    // ---- T5: the discipline pin (lower harvest after a FULL apply) --------------
    {
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: hunt did not freeze");
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES, 9u);
        TFH_ASSERT(TfPool(RSBS_SHARED_RES_TRIFORCE_PIECES) == 9, "an armed harvest did not reach the pool");
        uint16_t mmLive = 0;
        TFH_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_TRIFORCE_PIECES, Combo_TriforceHuntTotal(),
                                             &mmLive) &&
                       mmLive == 9,
                   "the FULL apply did not materialize the whole count");
        // Now a LOWER live value is harvested (a counter reset, a stale save).
        // A consumable kind would take the delta (9 -> 4) against the apply
        // watermark and the pool would LOSE five pieces; a monotonic kind keeps 9.
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_TRIFORCE_PIECES, 4u);
        TFH_ASSERT(TfPool(RSBS_SHARED_RES_TRIFORCE_PIECES) == 9,
                   "a lower harvest after a full apply LOWERED the piece count - the kind is not MONOTONIC (the only "
                   "test that tells the two disciplines apart)");
        // And the slot says so.
        bool flagged = false;
        for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_CAP; ++i) {
            if (gComboCtx.sharedResources[i].kind == (uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES) {
                flagged = (gComboCtx.sharedResources[i].flags & RSBS_SHARED_RES_F_MONOTONIC) != 0;
            }
        }
        for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_EXT_CAP; ++i) {
            if (gComboCtx.sharedResourcesExt[i].kind == (uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES) {
                flagged = (gComboCtx.sharedResourcesExt[i].flags & RSBS_SHARED_RES_F_MONOTONIC) != 0;
            }
        }
        TFH_ASSERT(flagged, "the triforce slot is not flagged MONOTONIC");
        // The cap is the combo total.
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES, 60u);
        uint16_t capped = 0;
        TFH_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_TRIFORCE_PIECES, Combo_TriforceHuntTotal(),
                                             &capped) &&
                       capped == 45,
                   "the apply must cap the count at the combo total");
    }

    // ---- T6: arming ------------------------------------------------------------
    {
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: freeze failed");
        const int slotsBefore = Combo_CountSharedResources();
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES, 7u);
        TFH_ASSERT(TfPool(RSBS_SHARED_RES_TRIFORCE_PIECES) < 0 && Combo_CountSharedResources() == slotsBefore,
                   "a world that is not a triforce hunt grew a triforce slot - its .redsave would no longer be "
                   "byte-identical to one written before the kind existed");
        uint16_t live = 3;
        TFH_ASSERT(!Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_TRIFORCE_PIECES, 45u, &live) && live == 3,
                   "a disarmed apply must answer as a never-shared resource does and leave the counter alone");
        TFH_ASSERT(Combo_SharedResourceKindArmed(RSBS_SHARED_RES_HOOKSHOT_TIER),
                   "arming the triforce kind disarmed an unconditional kind");
    }

    // ---- T7: the cross-game sum, through both games' REAL shims ------------------
    {
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: hunt did not freeze");

        // ONE LIVE SAVE AT A TIME. Both ports overlay the same unified storage
        // (unified_save.c), so a switch is: the departing game's harvest, then
        // the arriving game's OWN save restored — modelled here as ArmLive plus
        // the counter that game held when it left — then the arriving game's
        // apply. Each counter is read while its game is the live one.
        int ootSaved = 0;
        int mmSaved = 0;

        // OoT stint: collect k = 4, depart.
        OoT_TriforceHuntTest_ArmLive();
        OoT_TriforceHuntTest_SetCount((uint16_t)ootSaved);
        OoT_ApplySharedResources();
        OoT_TriforceHuntTest_SetCount((uint16_t)(OoT_TriforceHuntTest_Count() + 4));
        ootSaved = OoT_TriforceHuntTest_Count();
        OoT_HarvestSharedResources();
        TFH_ASSERT(TfPool(RSBS_SHARED_RES_TRIFORCE_PIECES) == 4, "OoT's harvest shim did not carry OoT's collects");

        // MM stint: arrive, collect m = 7, depart.
        MM_TriforceHuntTest_ArmLive();
        MM_TriforceHuntTest_SetCount((uint16_t)mmSaved);
        MM_ApplySharedResources();
        TFH_ASSERT(MM_TriforceHuntTest_Count() == 4, "MM's apply shim did not raise MM's counter to the combo count");
        MM_TriforceHuntTest_SetCount((uint16_t)(MM_TriforceHuntTest_Count() + 7));
        mmSaved = MM_TriforceHuntTest_Count();
        MM_HarvestSharedResources();
        TFH_ASSERT(TfPool(RSBS_SHARED_RES_TRIFORCE_PIECES) == 11, "MM's harvest shim did not carry MM's collects");

        // OoT arrives: k + m in BOTH games — MM left holding it, OoT now reads it.
        OoT_TriforceHuntTest_ArmLive();
        OoT_TriforceHuntTest_SetCount((uint16_t)ootSaved);
        OoT_ApplySharedResources();
        TFH_ASSERT(OoT_TriforceHuntTest_Count() == 11 && mmSaved == 11,
                   "after the switch the two counters do not both read k + m (4 + 7)");

        // More collects on both sides keep summing.
        OoT_TriforceHuntTest_SetCount((uint16_t)(OoT_TriforceHuntTest_Count() + 2));
        ootSaved = OoT_TriforceHuntTest_Count();
        OoT_HarvestSharedResources();
        MM_TriforceHuntTest_ArmLive();
        MM_TriforceHuntTest_SetCount((uint16_t)mmSaved);
        MM_ApplySharedResources();
        TFH_ASSERT(MM_TriforceHuntTest_Count() == 13, "a second OoT stint did not add to the one count");
        MM_TriforceHuntTest_SetCount((uint16_t)(MM_TriforceHuntTest_Count() + 1));
        mmSaved = MM_TriforceHuntTest_Count();
        MM_HarvestSharedResources();
        OoT_TriforceHuntTest_ArmLive();
        OoT_TriforceHuntTest_SetCount((uint16_t)ootSaved);
        OoT_ApplySharedResources();
        TFH_ASSERT(OoT_TriforceHuntTest_Count() == 14 && mmSaved == 14,
                   "a second MM stint did not add to the one count");

        // The same shims in a world that is NOT a hunt: each game keeps its own.
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: freeze failed");
        OoT_TriforceHuntTest_ArmLive();
        OoT_TriforceHuntTest_SetCount(5);
        OoT_HarvestSharedResources();
        MM_TriforceHuntTest_ArmLive();
        MM_TriforceHuntTest_SetCount(2);
        MM_ApplySharedResources();
        TFH_ASSERT(TfPool(RSBS_SHARED_RES_TRIFORCE_PIECES) < 0 && MM_TriforceHuntTest_Count() == 2,
                   "the shims shared a piece count in a world whose goal is not a triforce hunt");
        OoT_TriforceHuntTest_ArmLive();
        MM_TriforceHuntTest_ArmLive();
    }

    // ---- T8: the win decision --------------------------------------------------
    {
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: freeze failed");
        TFH_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_OOT, 20, 20) == RSBS_TRIFORCE_WIN_OWN &&
                       Combo_TriforceHuntOnPieceGiven(GAME_MM, 15, 15) == RSBS_TRIFORCE_WIN_OWN,
                   "unarmed, each game's own requirement must fire its own ending, exactly as upstream");
        TFH_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_OOT, 19, 20) == RSBS_TRIFORCE_WIN_NONE &&
                       Combo_TriforceHuntOnPieceGiven(GAME_OOT, 21, 20) == RSBS_TRIFORCE_WIN_NONE,
                   "unarmed, only the give that reaches the requirement fires (upstream's ==)");

        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
        TFH_ASSERT(Combo_TriforceFreezeAtCreation(&kOot, &kMm) == RSBS_TRIFORCE_OK, "fixture: hunt did not freeze");
        TFH_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_OOT, 20, 20) == RSBS_TRIFORCE_WIN_NONE &&
                       Combo_TriforceHuntOnPieceGiven(GAME_MM, 15, 15) == RSBS_TRIFORCE_WIN_NONE,
                   "armed, a game's OWN requirement must not end the combo");
        TFH_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_OOT, 35, 20) == RSBS_TRIFORCE_WIN_COMBO &&
                       Combo_TriforceHuntOnPieceGiven(GAME_MM, 35, 15) == RSBS_TRIFORCE_WIN_COMBO,
                   "armed, the COMBO requirement must end the combo in WHICHEVER game reaches it");
        TFH_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_MM, 36, 15) == RSBS_TRIFORCE_WIN_NONE,
                   "armed, a piece past the combo requirement must not fire again");
        TFH_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_NONE, 35, 20) == RSBS_TRIFORCE_WIN_NONE,
                   "a non-game never wins");
    }

    // ---- T9: the goal predicate over stub engines ---------------------------------
    {
        ComboContext_Init();
        const uint8_t hunt = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
        ComboLogicBagItem assumed[5];
        assumed[0] = TfRow((uint8_t)GAME_OOT, kTfOotPiece);
        assumed[1] = TfRow((uint8_t)GAME_OOT, kTfOotPiece);
        assumed[2] = TfRow((uint8_t)GAME_OOT, kTfOotPiece);
        assumed[3] = TfRow((uint8_t)GAME_MM, kTfMmPiece);
        assumed[4] = TfRow((uint8_t)GAME_MM, kTfMmPiece);

        TfResetEngine(&gTfOoT, (uint8_t)GAME_OOT, kTfOotPiece, 100, 4);
        TfResetEngine(&gTfMM, (uint8_t)GAME_MM, kTfMmPiece, 200, 4);
        TFH_ASSERT(TfInstall(true, true), "both stub engines must register at ABI 4");
        Combo_Logic_ResetPlacements();

        ComboLogicRoundResult r;
        TFH_ASSERT(TfRound(assumed, 5, hunt, 5, &r) == RSBS_COMBO_LOGIC_OK, "a triforce-hunt round must run");
        TFH_ASSERT(r.triforcePiecesOoT == 3 && r.triforcePiecesMM == 2 && r.triforcePieces == 5,
                   "the round did not read each half's pieces and sum them into ONE count");
        TFH_ASSERT(r.goalExpression == 1, "5 of 5 pieces across both worlds must satisfy the hunt");
        TFH_ASSERT(TfRound(assumed, 5, hunt, 6, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == 0,
                   "5 pieces must not satisfy a requirement of 6");
        TFH_ASSERT(TfRound(assumed, 3, hunt, 3, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == 1 &&
                       r.triforcePiecesMM == 0,
                   "OoT's pieces alone satisfy a requirement they meet: it is one count, not per-half ANDs");
        TFH_ASSERT(TfRound(assumed + 3, 2, hunt, 2, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == 1,
                   "MM's pieces alone satisfy a requirement they meet");

        // The halves' own goalReached is NOT what the hunt reads: both say "reached"
        // and the count still decides.
        gTfOoT.goalAnswer = 1;
        gTfMM.goalAnswer = 1;
        TFH_ASSERT(TfRound(assumed, 5, hunt, 6, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == 0 &&
                       r.goalOoT == 1 && r.goalMM == 1,
                   "the triforce-hunt expression followed goalReached instead of the shared count");
        TFH_ASSERT(TfRound(assumed, 5, (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, 6, &r) == RSBS_COMBO_LOGIC_OK &&
                       r.goalExpression == 1 && r.triforcePieces == -1,
                   "beat-both must be untouched by the triforce fields (and must not read the pieces)");
        gTfOoT.goalAnswer = 0;
        gTfMM.goalAnswer = 0;

        // THE ARRIVAL GATE: with OoT's crossing closed, MM's pieces are in a
        // Termina the player cannot enter.
        gTfOoT.crossing = 0;
        TFH_ASSERT(TfRound(assumed, 5, hunt, 5, &r) == RSBS_COMBO_LOGIC_OK && r.triforcePiecesMM == 0 &&
                       r.triforcePieces == 3 && r.goalExpression == 0,
                   "MM's pieces counted while OoT's crossing is closed - the arrival gate does not hold for the hunt");
        gTfOoT.crossing = 1;

        // A requirement of zero describes no hunt: unevaluated, never "reached".
        TFH_ASSERT(TfRound(assumed, 5, hunt, 0, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == -1,
                   "a zero requirement must leave the expression unevaluated");
        TFH_ASSERT(Combo_Logic_EvaluateTriforceHunt(4, 5) == 0 && Combo_Logic_EvaluateTriforceHunt(5, 5) == 1 &&
                       Combo_Logic_EvaluateTriforceHunt(9, 5) == 1 && Combo_Logic_EvaluateTriforceHunt(-1, 5) == -1 &&
                       Combo_Logic_EvaluateTriforceHunt(5, 0) == -1,
                   "the triforce evaluator's truth table drifted (>= for the proof; -1 for unusable inputs)");
        TFH_ASSERT(Combo_Logic_EvaluateGoal(hunt, 1, 1) == -1,
                   "the boolean evaluator must still have no answer for a COUNT goal");

        // THE FILL proves the hunt: 3 OoT and 2 MM pieces placed anywhere, and a
        // final round with NOTHING assumed that reaches all five.
        ComboLogicFillResult f;
        TFH_ASSERT(TfFill(assumed, 5, 5, &f) == RSBS_COMBO_LOGIC_OK && f.goalProven && f.placed == 5,
                   "a fill whose bag holds the requirement must prove the hunt");
        TFH_ASSERT(TfFill(assumed, 5, 6, &f) == RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE && !f.goalProven,
                   "a fill whose bag holds 5 pieces must NOT prove a requirement of 6");
        TFH_ASSERT(TfFill(assumed, 5, 0, &f) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST && f.attempts == 0,
                   "a triforce-hunt fill with no frozen requirement must be refused before any attempt");

        // Engines without the query cannot answer for their half.
        TFH_ASSERT(TfInstall(true, false), "re-register with MM lacking triforcePieces");
        const int queriesBefore = gTfOoT.pieceQueries;
        TFH_ASSERT(TfFill(assumed, 5, 5, &f) == RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL && f.attempts == 0 &&
                       f.placed == 0,
                   "a triforce-hunt fill over an engine without triforcePieces must refuse before any attempt");
        TFH_ASSERT(gTfOoT.pieceQueries == queriesBefore, "a refused fill must not have run a round");
        TFH_ASSERT(TfRound(assumed, 5, hunt, 5, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == -1 &&
                       r.triforcePiecesMM == -1 && r.triforcePieces == -1,
                   "a round over a half that cannot answer must leave the count and the expression unevaluated");

        TfRestoreEngines();
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] PASS: combo-triforce-hunt\n");
    return TEST_PASS;
}
