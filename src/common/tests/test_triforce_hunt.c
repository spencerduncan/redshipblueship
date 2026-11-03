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
 *   T10 THE .redsave LOAD REFUSAL, through the real SaveManager::LoadSlot: a
 *      file whose stored triforce record contradicts the goal beside it (a
 *      beat-both world carrying a hunt record) is refused as damage and
 *      quarantined, and nothing is committed; the same world with the four
 *      zero bytes loads.
 *
 *   T11 THE [Both Games] SPLIT (#1389): OoTMM's triforcePieces / triforceGoal
 *      split into the record's two halves (OoT the larger half inside its own
 *      1..100 range), for every pair the rows can author.
 *
 * The real give arms themselves are locked in the rando tier
 * (rando-triforce-hunt-win), because both need a booted game.
 *
 * Two more rows live here (#1389):
 *   combo-triforce-rows (redship tier, needs the CVar store): the two keys and
 *     their clamp, a hunt frozen from them that either game wins at the goal,
 *     the counts divergence and its restore from the file's record (directly
 *     and through a real SaveManager::Load), and MM's own hunt rows following
 *     the [Both Games] rows only under the hunt goal.
 *   paired-triforce-bag (rando tier): a real paired generation under the hunt
 *     succeeds, its frozen hunt is the rows', and the single bag places exactly
 *     the rows' pieces, each game's half on its own hosts, proving the goal.
 */

#include "../combo_logic.h"
#include "../combo_mm_options_page.h" // #1389: MM's hunt rows under the [Both Games] goal
#include "../combo_mm_options_view.h" // #1389: MM_Rando_ResolveTriforceHalf, the MM option table
#include "../combo_settings_view.h"   // #1389: the [Both Games] Triforce Hunt keys and their restore
#include "../combo_single_bag.h"      // #1389: the paired-triforce-bag row's fill report
#include "../context.h"
#include "../cvar_shared_keys.h" // #1389: RSBS_CVAR_COMBO_RANDO_TRIFORCE_*
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_items.h" // #1389: Combo_ItemClassArmedBy, which names a piece in either game
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../triforce_hunt.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

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

// T10's own save directory: RsbsSave_HasQuarantine reports ANY .bak beside the
// slot, so an earlier run's evidence must not be able to satisfy the leg.
inline const char* kTfLoadTestDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_triforce_hunt_load");
}

/** Write slot 0 for a paired world whose combo record is what THIS session
 *  resolves (so no field or fingerprint divergence can fire) and whose triforce
 *  record is `rec`. Returns false when the save could not be written. */
bool TfWriteSlot(const ComboTriforceRecord& rec) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    RsbsSave_ResetSlotSessionState();
    mgr.DeleteSave(0); // arms the write latch (#533)
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, 0x3Cu);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, 0xC3u);
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x7F0CE0A1u;
    gComboCtx.sharedRandoSettingsHash = 0x7F0C5EA1u;
    gComboCtx.mmProfileDigest = 0x7F0C4DA1u;
    ComboSettingsRecord settings;
    Combo_ResolveComboSettings(&settings);
    Combo_FreezeComboSettings(&settings);
    gComboCtx.comboTriforce = rec;
    return mgr.Save(0);
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

int TfRound(const ComboLogicBagItem* assumed, int count, uint8_t goal, uint16_t required, ComboLogicRoundResult* out) {
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
        // #1389: a half holding pieces with a ZERO share of the requirement is
        // coherent now (the [Both Games] split gives MM's half that share when the
        // goal is 1); a hunt whose halves require nothing at all is not.
        oot = TfHalf(10, 0);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_OK && Combo_TriforceRecordRequired(&r) == 5,
                   "a half with pieces and a zero share of the requirement must resolve (#1389)");
        ComboTriforceHalf noGoal = TfHalf(5, 0);
        TFH_ASSERT(Combo_TriforceResolve(&oot, &noGoal, &r) == RSBS_TRIFORCE_ERR_NO_GOAL && TfRecordZero(&r),
                   "a hunt that requires no piece at all must be refused (it is won at its start)");
        TFH_ASSERT(strcmp(Combo_TriforceStatusName(RSBS_TRIFORCE_ERR_NO_GOAL), "no-goal") == 0,
                   "the no-goal status is unnamed");
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
        TFH_ASSERT(
            Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_TRIFORCE_PIECES, Combo_TriforceHuntTotal(), &mmLive) &&
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
        TFH_ASSERT(
            Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_TRIFORCE_PIECES, Combo_TriforceHuntTotal(), &capped) &&
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
        TFH_ASSERT(TfRound(assumed, 5, hunt, 6, &r) == RSBS_COMBO_LOGIC_OK && r.goalExpression == 0 && r.goalOoT == 1 &&
                       r.goalMM == 1,
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

    // ---- T10: the .redsave load refusal (SaveManager::LoadSlot) -----------------
    {
        std::error_code ec;
        std::filesystem::remove_all(kTfLoadTestDir(), ec); // no stale .bak from an earlier run
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kTfLoadTestDir());

        // Counter-leg first: the same world with the four zero bytes every
        // non-hunt world stores loads, so the refusal below is the record's.
        ComboTriforceRecord zero;
        memset(&zero, 0, sizeof(zero));
        TFH_ASSERT(TfWriteSlot(zero), "fixture: Save(0) failed for the healthy world");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        TFH_ASSERT(mgr.Load(0), "a beat-both .redsave with a zero triforce record was refused - the load check is not "
                                "the record's");
        TFH_ASSERT(Combo_ComboSettingsFrozen() && TfRecordZero(&gComboCtx.comboTriforce),
                   "the healthy world did not load its record");

        // A beat-both world carrying a hunt record: a hunt nobody froze.
        ComboTriforceRecord stray;
        stray.totalOoT = 5;
        stray.requiredOoT = 3;
        stray.totalMM = 4;
        stray.requiredMM = 3;
        TFH_ASSERT(TfWriteSlot(stray), "fixture: Save(0) failed for the torn world");
        TFH_ASSERT(RsbsSave_HasQuarantine(0) == 0, "fixture: a quarantine exists before the refused load");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        TFH_ASSERT(!mgr.Load(0), "a .redsave whose triforce record contradicts its goal was COMMITTED - the load "
                                 "check does not read the O10 record (save.cpp)");
        TFH_ASSERT(RsbsSave_GetSlotRefuseReason(0) == (int)RSBS_REFUSE_IDENTITY,
                   "the triforce-record refusal must be RSBS_REFUSE_IDENTITY");
        TFH_ASSERT(RsbsSave_IsSlotWritable(0) == 0, "a refused load must latch the slot against writes (#533)");
        TFH_ASSERT(RsbsSave_HasQuarantine(0) == 1,
                   "a contradicting triforce record is DAMAGE to the stored identity and must be quarantined");
        TFH_ASSERT(!Combo_ComboSettingsFrozen() && TfRecordZero(&gComboCtx.comboTriforce),
                   "a refused load committed the torn record over the resident context");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        std::filesystem::remove_all(kTfLoadTestDir(), ec);
    }

    // ---- T11: THE [Both Games] SPLIT (#1389) -----------------------------------
    // OoTMM's triforcePieces / triforceGoal become the record's two halves: OoT
    // the larger half of each number inside its own 1..100 range, MM the rest.
    // Every split is a record the rule accepts, sums back to the two numbers, and
    // gives OoT at least one piece and one requirement (OoT's options store
    // value - 1).
    {
        struct SplitCase {
            uint16_t pieces, goal;
            uint16_t ootTotal, ootRequired, mmTotal, mmRequired;
        };
        const SplitCase kCases[] = {
            { 30, 20, 15, 10, 15, 10 },       // OoTMM's defaults
            { 1, 1, 1, 1, 0, 0 },             // one piece: OoT holds it, MM's hunt is off
            { 2, 1, 1, 1, 1, 0 },             // goal 1: MM holds a piece and requires none
            { 3, 3, 2, 2, 1, 1 },             // odd numbers: OoT takes the larger half
            { 12, 8, 6, 4, 6, 4 },            // the rando-tier row's world
            { 201, 201, 100, 100, 101, 101 }, // OoT's half capped at its option range
            { 255, 255, 100, 100, 155, 155 }, // the cap: OoT's 8-bit counter
            { 255, 1, 100, 1, 155, 0 },
        };
        for (const SplitCase& c : kCases) {
            ComboTriforceHalf oot;
            ComboTriforceHalf mm;
            TFH_ASSERT(Combo_TriforceSplit(c.pieces, c.goal, &oot, &mm) == RSBS_TRIFORCE_OK,
                       "a valid [Both Games] pair did not split");
            if (oot.total != c.ootTotal || oot.required != c.ootRequired || mm.total != c.mmTotal ||
                mm.required != c.mmRequired) {
                printf("[TEST] FAIL: %u of %u split OoT %u of %u, MM %u of %u; expected OoT %u of %u, MM %u of %u\n",
                       (unsigned)c.goal, (unsigned)c.pieces, (unsigned)oot.required, (unsigned)oot.total,
                       (unsigned)mm.required, (unsigned)mm.total, (unsigned)c.ootRequired, (unsigned)c.ootTotal,
                       (unsigned)c.mmRequired, (unsigned)c.mmTotal);
                TFH_ASSERT(false, "the [Both Games] split drifted (#1389)");
            }
            ComboTriforceRecord r;
            TFH_ASSERT(Combo_TriforceResolve(&oot, &mm, &r) == RSBS_TRIFORCE_OK &&
                           Combo_TriforceRecordTotal(&r) == c.pieces && Combo_TriforceRecordRequired(&r) == c.goal,
                       "a split is not a record the rule accepts, or does not sum back to the two numbers");
        }
        // Every pair the rows can author splits into a coherent record.
        for (uint32_t pieces = 1; pieces <= RSBS_TRIFORCE_COMBO_MAX; pieces++) {
            for (uint32_t goal = 1; goal <= pieces; goal++) {
                ComboTriforceHalf oot;
                ComboTriforceHalf mm;
                ComboTriforceRecord r;
                if (Combo_TriforceSplit((uint16_t)pieces, (uint16_t)goal, &oot, &mm) != RSBS_TRIFORCE_OK ||
                    Combo_TriforceResolve(&oot, &mm, &r) != RSBS_TRIFORCE_OK || oot.total < 1 || oot.required < 1 ||
                    oot.total > RSBS_TRIFORCE_OOT_HALF_MAX || Combo_TriforceRecordTotal(&r) != pieces ||
                    Combo_TriforceRecordRequired(&r) != goal) {
                    printf("[TEST] FAIL: %u of %u does not split into a coherent record\n", (unsigned)goal,
                           (unsigned)pieces);
                    TFH_ASSERT(false, "the split leaves an authorable pair without a record (#1389)");
                }
            }
        }
        ComboTriforceHalf oot;
        ComboTriforceHalf mm;
        TFH_ASSERT(Combo_TriforceSplit(0, 0, &oot, &mm) == RSBS_TRIFORCE_ERR_NO_PIECES &&
                       Combo_TriforceSplit(5, 0, &oot, &mm) == RSBS_TRIFORCE_ERR_NO_GOAL &&
                       Combo_TriforceSplit(5, 6, &oot, &mm) == RSBS_TRIFORCE_ERR_GOAL_OVER_PIECES &&
                       Combo_TriforceSplit(256, 20, &oot, &mm) == RSBS_TRIFORCE_ERR_OVER_CAP &&
                       Combo_TriforceSplit(5, 3, NULL, &mm) == RSBS_TRIFORCE_ERR_BAD_REQUEST,
                   "the split must refuse what no [Both Games] row can author");
        TFH_ASSERT(oot.total == 0 && oot.required == 0 && mm.total == 0 && mm.required == 0,
                   "a refused split must leave both halves zero");
        TFH_ASSERT(RSBS_TRIFORCE_COMBO_DEFAULT_PIECES == 30u && RSBS_TRIFORCE_COMBO_DEFAULT_GOAL == 20u,
                   "the [Both Games] defaults are not OoTMM's 30 and 20");
        TFH_ASSERT(RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS == 0x20000u &&
                       strcmp(Combo_ComboSettingsDivergenceFieldName(RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS),
                              "triforceCounts") == 0 &&
                       !Combo_ComboSettingsDivergenceIsDamage(RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS),
                   "the counts divergence must be the next contiguous bit, named, and a SESSION divergence");
        printf("[TEST] T11: [Both Games] pieces / goal split into a coherent record for every authorable pair "
               "(#1389)\n");
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] PASS: combo-triforce-hunt\n");
    return TEST_PASS;
}

// ============================================================================
// combo-triforce-rows (#1389): the [Both Games] Triforce Hunt rows' keys, the
// load restore that puts them back, and MM's read-only rows. Needs the CVar
// store (test_runner.cpp brings one up, as for combo-settings-authoring).
// ============================================================================

namespace {

/** The three keys this row writes, cleared on every exit. */
void TfrClearKeys() {
    CVarClear(RSBS_CVAR_COMBO_RANDO_GOAL);
    CVarClear(RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES);
    CVarClear(RSBS_CVAR_COMBO_RANDO_TRIFORCE_GOAL);
}

/** The MM option row named @p name (its StaticData name), or NULL. */
const ComboMMOptionDesc* TfrMMOption(const char* name) {
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
        if (d != NULL && d->name != NULL && strcmp(d->name, name) == 0) {
            return d;
        }
    }
    return NULL;
}

} // namespace

#define TFR_ASSERT(cond, msg)                                               \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            ComboContext_Init();                                            \
            TfrClearKeys();                                                 \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

TestResult ComboTriforceRows_Run(void) {
    printf("[TEST] combo-triforce-rows: [Both Games] Triforce Pieces / Triforce Goal author the frozen hunt, a load "
           "restores them, and MM's own hunt rows follow them under the hunt goal (#1389)\n");
    TFR_ASSERT(Combo_ComboSettingStoreAvailable(), "fixture: no CVar store");
    ComboContext_Init();
    TfrClearKeys();

    // ---- R1: the keys and their resolution ------------------------------------
    {
        uint16_t pieces = 0;
        uint16_t goal = 0;
        Combo_ResolveComboTriforce(&pieces, &goal);
        TFR_ASSERT(pieces == 30 && goal == 20, "unset keys must resolve to OoTMM's 30 and 20");
        TFR_ASSERT(strcmp(Combo_ComboSettingKey(COMBO_SETTING_TRIFORCE_PIECES), "gCombo.Rando.TriforcePieces") == 0 &&
                       strcmp(Combo_ComboSettingKey(COMBO_SETTING_TRIFORCE_GOAL), "gCombo.Rando.TriforceGoal") == 0 &&
                       strcmp(Combo_ComboSettingLabel(COMBO_SETTING_TRIFORCE_PIECES), "Triforce Pieces") == 0 &&
                       strcmp(Combo_ComboSettingLabel(COMBO_SETTING_TRIFORCE_GOAL), "Triforce Goal") == 0,
                   "the keys or labels are not OoTMM's names");
        TFR_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_PIECES, 12) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_GOAL, 40) == 1,
                   "the writers refused a pre-creation hunt");
        Combo_ResolveComboTriforce(&pieces, &goal);
        TFR_ASSERT(pieces == 12 && goal == 12, "a goal past the pieces must resolve clamped to them, as OoTMM's");
        TFR_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_PIECES, 256) == 0 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_GOAL, 0) == 0,
                   "a value outside 1..255 was written");
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES, 999); // out of band: OoTMM's own maximum
        Combo_ResolveComboTriforce(&pieces, &goal);
        TFR_ASSERT(pieces == 30 && goal == 30,
                   "an out-of-space pieces key must resolve to its default (the goal then clamps to it)");
        TfrClearKeys();
    }

    // ---- R2: a frozen hunt from the rows, and the win from either game ----------
    ComboTriforceRecord fileRecord;
    ComboSettingsRecord fileSettings;
    {
        TFR_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_PIECES, 12) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_GOAL, 8) == 1,
                   "fixture: the hunt keys");
        TfFreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
        uint16_t pieces = 0;
        uint16_t goal = 0;
        Combo_ResolveComboTriforce(&pieces, &goal);
        ComboTriforceHalf oot;
        ComboTriforceHalf mm;
        TFR_ASSERT(Combo_TriforceSplit(pieces, goal, &oot, &mm) == RSBS_TRIFORCE_OK &&
                       Combo_TriforceFreezeAtCreation(&oot, &mm) == RSBS_TRIFORCE_OK,
                   "the rows' split did not freeze");
        TFR_ASSERT(Combo_TriforceHuntArmed() && Combo_TriforceHuntTotal() == 12 && Combo_TriforceHuntRequired() == 8,
                   "the frozen hunt is not the rows' 8 of 12");
        uint16_t shownPieces = 0;
        uint16_t shownGoal = 0;
        Combo_TriforceCountsShown(&shownPieces, &shownGoal);
        TFR_ASSERT(shownPieces == 12 && shownGoal == 8, "the rows do not show the frozen hunt");
        // The one decision both give arms call: the combo requirement, from
        // either game; neither game's own share is a threshold.
        TFR_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_OOT, 8, oot.required) == RSBS_TRIFORCE_WIN_COMBO &&
                       Combo_TriforceHuntOnPieceGiven(GAME_MM, 8, mm.required) == RSBS_TRIFORCE_WIN_COMBO,
                   "reaching the [Both Games] goal did not win in one of the games");
        TFR_ASSERT(Combo_TriforceHuntOnPieceGiven(GAME_OOT, oot.required, oot.required) == RSBS_TRIFORCE_WIN_NONE &&
                       Combo_TriforceHuntOnPieceGiven(GAME_MM, 7, mm.required) == RSBS_TRIFORCE_WIN_NONE,
                   "a count short of the [Both Games] goal won");
        TFR_ASSERT(Combo_TriforceCountsDivergence(&gComboCtx.comboSettings, &gComboCtx.comboTriforce) == 0,
                   "a world frozen from the session's own keys reads as divergent");
        // A session whose keys walked away from the world (the writers refuse
        // while frozen, so out of band here).
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES, 40);
        TFR_ASSERT(Combo_TriforceCountsDivergence(&gComboCtx.comboSettings, &gComboCtx.comboTriforce) ==
                       RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS,
                   "keys that differ from the frozen hunt must diverge as triforceCounts");
        ComboSettingsRecord beatBoth = gComboCtx.comboSettings;
        beatBoth.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
        ComboTriforceRecord zero;
        memset(&zero, 0, sizeof(zero));
        TFR_ASSERT(Combo_TriforceCountsDivergence(&beatBoth, &zero) == 0,
                   "a world under another goal must never diverge on the hunt's keys");
        fileRecord = gComboCtx.comboTriforce;
        fileSettings = gComboCtx.comboSettings;

        // The restore: only with the file's triforce record, and exactly the
        // key that differs.
        const uint32_t bits = RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS;
        TFR_ASSERT(
            Combo_ComboSettingsUnrestorableBits(&gComboCtx.comboItemRules, bits) == 0u &&
                Combo_ComboSettingsCanRestoreLiveEx(&fileSettings, &gComboCtx.comboItemRules, &fileRecord, bits) == 1 &&
                Combo_ComboSettingsCanRestoreLive(&fileSettings, &gComboCtx.comboItemRules, bits) == 0,
            "the counts must restore from the file's triforce record, and only with it");
        char names[128];
        ComboSettingsKeyUndo undo;
        TFR_ASSERT(Combo_ComboSettingsRestoreLiveEx(&fileSettings, &gComboCtx.comboItemRules, &fileRecord, bits, names,
                                                    sizeof(names), &undo) == 1 &&
                       strcmp(names, "Triforce Pieces") == 0,
                   "the restore did not put back exactly the Triforce Pieces key");
        TFR_ASSERT(Combo_TriforceCountsDivergence(&fileSettings, &fileRecord) == 0,
                   "after the restore the keys still diverge from the file");
        Combo_ComboSettingsRestoreUndo(&undo);
        TFR_ASSERT(Combo_ComboSettingResolved(COMBO_SETTING_TRIFORCE_PIECES) == 40,
                   "the undo did not put the session's own key back");
        printf("[TEST] R2: the rows freeze 8 of 12 (OoT %u of %u, MM %u of %u), either game wins at 8, and the "
               "counts restore from the file's record\n",
               (unsigned)oot.required, (unsigned)oot.total, (unsigned)mm.required, (unsigned)mm.total);
    }

    // ---- R3: MM's own hunt rows follow the [Both Games] rows under the goal -----
    {
        ComboContext_Init();
        TfrClearKeys();
        Combo_MMOptionsPages_Init();
        const ComboMMOptionDesc* shuffleRow = TfrMMOption(COMBO_MM_OPTION_NAME_SHUFFLE_TRIFORCE);
        const ComboMMOptionDesc* maxRow = TfrMMOption(COMBO_MM_OPTION_NAME_TRIFORCE_MAX);
        const ComboMMOptionDesc* requiredRow = TfrMMOption(COMBO_MM_OPTION_NAME_TRIFORCE_REQUIRED);
        TFR_ASSERT(shuffleRow != NULL && maxRow != NULL && requiredRow != NULL,
                   "MM's three Triforce Hunt rows are not named");
        // MM's own hunt off (its keys at their defaults), whatever a row before
        // this one left there.
        CVarClear(shuffleRow->cvar);
        CVarClear(maxRow->cvar);
        CVarClear(requiredRow->cvar);
        TFR_ASSERT(!Combo_MMOptionsPage_OptionSetByCombo(shuffleRow) && !Combo_MMOptionsPage_OptionSetByCombo(maxRow) &&
                       !Combo_MMOptionsPage_OptionSetByCombo(requiredRow),
                   "under the shipped goal MM's own hunt rows must stay MM's (#768)");
        TFR_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_PIECES, 12) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_GOAL, 8) == 1,
                   "fixture: the hunt keys");
        const char* reason = NULL;
        TFR_ASSERT(Combo_MMOptionsPage_OptionState(maxRow, &reason) == COMBO_MM_ROW_SET_BY_COMBO &&
                       strcmp(reason, COMBO_TRIFORCE_SET_BY_COMBO_REASON) == 0,
                   "under the hunt goal MM's Triforce Pieces Shuffled row is not set by [Both Games] Triforce Hunt");
        TFR_ASSERT(Combo_MMOptionsPage_OptionShownValue(shuffleRow) == 1 &&
                       Combo_MMOptionsPage_OptionShownValue(maxRow) == 6 &&
                       Combo_MMOptionsPage_OptionShownValue(requiredRow) == 4,
                   "MM's hunt rows do not show MM's half of 8 of 12 (on, 6 pieces, 4 required)");
        uint16_t mmTotal = 0;
        uint16_t mmRequired = 0;
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/0, &mmTotal, &mmRequired);
        TFR_ASSERT(mmTotal == 6 && mmRequired == 4,
                   "MM's paired resolution does not generate MM's half of the [Both Games] split");
        // Under another goal MM's own rows are MM's hunt again.
        TFR_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH) == 1,
                   "fixture: the goal");
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/0, &mmTotal, &mmRequired);
        TFR_ASSERT(mmTotal == 0 && mmRequired == 0,
                   "under a boss goal MM's half moved with the [Both Games] rows (MM's own hunt is off)");
        TfrClearKeys();
        printf("[TEST] R3: MM's three hunt rows are set by [Both Games] Triforce Hunt under its goal and show MM's "
               "half; under a boss goal they are MM's own\n");
    }

    // ---- R4: a .redsave load restores the file's counts ------------------------
    // The same world, met by a session staged at other counts: frozen wins at
    // load (#781), so the file's 8 of 12 go back into the keys instead of the
    // file being refused, and the resident hunt is the file's.
    {
        std::error_code ec;
        std::filesystem::remove_all(kTfLoadTestDir(), ec);
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kTfLoadTestDir());
        TFR_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) == 1,
                   "fixture: the goal");
        TFR_ASSERT(TfWriteSlot(fileRecord), "fixture: Save(0) failed for the hunt world");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES, 50);
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_TRIFORCE_GOAL, 30);
        const bool loaded = mgr.Load(0);
        const int32_t piecesAfter = Combo_ComboSettingResolved(COMBO_SETTING_TRIFORCE_PIECES);
        const int32_t goalAfter = Combo_ComboSettingResolved(COMBO_SETTING_TRIFORCE_GOAL);
        printf("[TEST] R4: load %s; keys after it %d of %d (file 8 of 12)\n", loaded ? "accepted" : "REFUSED",
               (int)goalAfter, (int)piecesAfter);
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        std::filesystem::remove_all(kTfLoadTestDir(), ec);
        TFR_ASSERT(loaded, "a hunt world met by a session at other counts was refused instead of restored (#781)");
        TFR_ASSERT(piecesAfter == 12 && goalAfter == 8, "the load did not put the file's counts back into the keys");
        TFR_ASSERT(Combo_TriforceHuntArmed() && Combo_TriforceHuntRequired() == 8 && Combo_TriforceHuntTotal() == 12,
                   "the loaded world's hunt is not the file's");
        (void)fileSettings;
    }

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    TfrClearKeys();
    printf("[TEST] PASS: combo-triforce-rows\n");
    return TEST_PASS;
}

// ============================================================================
// paired-triforce-bag (#1389, rando tier): a real paired triforce-hunt world.
// ============================================================================

extern "C" {
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int OoT_ComboLogic_ExportPool(int source, uint16_t* outItems, uint16_t* outHosts, uint16_t* outFlags, int cap);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
}

namespace {

/** Is @p id of @p origin a Triforce Piece? Asked game-neutrally: both games tag
 *  their piece, and only it, with the WORLD_EVENT family (shared_items.h). */
bool TfbIsPiece(uint8_t origin, uint16_t id) {
    SharedItem item;
    memset(&item, 0, sizeof(item));
    item.originGame = origin;
    item.id = id;
    return (Combo_ItemClassArmedBy(item) & RSBS_FILL_ARM_WORLD_EVENT) != 0u;
}

} // namespace

TestResult PairedTriforceBag_Run(void) {
    printf("[TEST] paired-triforce-bag: a paired world under the Triforce Hunt goal is generated, freezes the "
           "[Both Games] 8 of 12, and the single bag places exactly 12 pieces, 6 on each game's own hosts, proving "
           "the goal (#1389)\n");
    int failures = 0;
#define TFB_CHECK(cond, ...)                                     \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            failures++;                                          \
        }                                                        \
    } while (0)

    ComboContext_Init();
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_GOAL, (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES, 12);
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_TRIFORCE_GOAL, 8);

    // ---- B1: Generate is no longer refused, and freezes the rows' hunt ---------
    const int gen = Rando_HeadlessSeedTest("RSBSTRIFORCE1389");
    TFB_CHECK(gen == 0,
              "the paired generation under the Triforce Hunt goal failed (rc %d): Generate still refuses "
              "the goal, or the fill could not carry it",
              gen);
    TFB_CHECK(Combo_ComboSettingsFrozen() && gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT,
              "the creation did not freeze the Triforce Hunt goal");
    TFB_CHECK(Combo_TriforceHuntArmed() && Combo_TriforceHuntTotal() == 12 && Combo_TriforceHuntRequired() == 8,
              "the frozen hunt is %u of %u, expected the rows' 8 of 12", (unsigned)Combo_TriforceHuntRequired(),
              (unsigned)Combo_TriforceHuntTotal());
    TFB_CHECK(gComboCtx.comboTriforce.totalOoT == 6 && gComboCtx.comboTriforce.requiredOoT == 4 &&
                  gComboCtx.comboTriforce.totalMM == 6 && gComboCtx.comboTriforce.requiredMM == 4,
              "the frozen halves are OoT %u of %u, MM %u of %u; expected the split, 4 of 6 each",
              (unsigned)gComboCtx.comboTriforce.requiredOoT, (unsigned)gComboCtx.comboTriforce.totalOoT,
              (unsigned)gComboCtx.comboTriforce.requiredMM, (unsigned)gComboCtx.comboTriforce.totalMM);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    TFB_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world (rc %d)", mmHalf);

    if (gen == 0 && mmHalf == 0) {
        // ---- B2: the fill carried the hunt and proved it -----------------------
        const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
        printf("[TEST] paired-triforce-bag: bag %d rows, %d placed, goal %u, proven %d, %d round(s)\n", bag.bagCount,
               bag.fill.placed, (unsigned)bag.goal, bag.fill.goalProven ? 1 : 0, bag.fill.rounds);
        TFB_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven &&
                      bag.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT,
                  "the single-bag fill did not prove the Triforce Hunt goal (status %d)", bag.status);
        TFB_CHECK(Combo_Logic_UnprovedHalvesForGoal(&bag.fill, bag.goal) == 0u,
                  "a proved triforce-hunt world would warn that a game may be unfinishable");

        // ---- B3: exactly the rows' pieces, each game's on its own hosts ----------
        int placedOoT = 0;
        int placedMM = 0;
        int crossed = 0;
        for (GameId host : { GAME_OOT, GAME_MM }) {
            ComboLogicPlacement p;
            for (int i = 0; i < Combo_Logic_PlacementCount(host); i++) {
                if (!Combo_Logic_PlacementAt(host, i, &p) || !TfbIsPiece(p.item.originGame, p.item.id)) {
                    continue;
                }
                if (p.item.originGame != (uint8_t)host) {
                    crossed++;
                } else if (host == GAME_OOT) {
                    placedOoT++;
                } else {
                    placedMM++;
                }
            }
        }
        printf("[TEST] paired-triforce-bag: the bag placed %d OoT and %d MM piece(s), %d crossed\n", placedOoT,
               placedMM, crossed);
        TFB_CHECK(placedOoT == 6 && placedMM == 6 && crossed == 0,
                  "the bag placed %d OoT and %d MM pieces (%d across games); expected 6 on each game's own hosts",
                  placedOoT, placedMM, crossed);

        // The two worlds hold no other piece: none was left to a junk pass.
        std::vector<uint16_t> items(8192, 0);
        const int ootHosted = OoT_ComboLogic_ExportPool(1, items.data(), nullptr, nullptr, (int)items.size());
        int ootWorld = 0;
        for (int i = 0; i < ootHosted && i < (int)items.size(); i++) {
            ootWorld += TfbIsPiece((uint8_t)GAME_OOT, items[(size_t)i]) ? 1 : 0;
        }
        const int mmShuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());
        int mmWorld = 0;
        for (int i = 0; i < mmShuffled && i < (int)items.size(); i++) {
            mmWorld += TfbIsPiece((uint8_t)GAME_MM, items[(size_t)i]) ? 1 : 0;
        }
        printf("[TEST] paired-triforce-bag: OoT's world holds %d piece(s), MM's %d (expected 6 and 6)\n", ootWorld,
               mmWorld);
        TFB_CHECK(ootWorld == 6 && mmWorld == 6,
                  "OoT's world holds %d pieces and MM's %d; expected exactly the bag's 6 each", ootWorld, mmWorld);

        // ---- B4: the goal fires at 8 from either game ----------------------------
        TFB_CHECK(Combo_TriforceHuntOnPieceGiven(GAME_OOT, 8, 4) == RSBS_TRIFORCE_WIN_COMBO &&
                      Combo_TriforceHuntOnPieceGiven(GAME_MM, 8, 4) == RSBS_TRIFORCE_WIN_COMBO &&
                      Combo_TriforceHuntOnPieceGiven(GAME_OOT, 4, 4) == RSBS_TRIFORCE_WIN_NONE &&
                      Combo_TriforceHuntOnPieceGiven(GAME_MM, 7, 4) == RSBS_TRIFORCE_WIN_NONE,
                  "the generated world does not end at 8 pieces from either game");
    }

    CVarClear(RSBS_CVAR_COMBO_RANDO_TRIFORCE_GOAL);
    CVarClear(RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES);
    CVarClear(RSBS_CVAR_COMBO_RANDO_GOAL);
#undef TFB_CHECK
    printf("[TEST] paired-triforce-bag: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? TEST_PASS : TEST_FAIL;
}
