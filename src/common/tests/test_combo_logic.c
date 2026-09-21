/**
 * @file test_combo_logic.c
 * @brief ROM-free locks for the combo-logic coordinator (ADR 0010 increment 3,
 *        #645; O4 = composition).
 *
 * Every claim here is driven over TWO SYNTHETIC STUB ENGINES authored in this
 * file, which is the whole reason the engine surface is a registered vtable
 * rather than two sets of fixed `extern "C"` symbols (combo_logic.h says so at
 * the top). A stub world is a bitmask: an item id maps to a bit, a host is
 * reached when its required bits are held, the crossing and the half-goal are
 * each a required mask. That is small enough to reason about exactly and rich
 * enough to carry every property the coordinator claims.
 *
 * Three rows, split by what each one can prove:
 *
 *   combo-logic-engine-surface — the CONTRACT. Registration refuses a bad ABI,
 *   a hole in the vtable, and a snapshot without its restore; un-registration
 *   restores the registry. The GOAL truth table, including triforce-hunt
 *   REFUSING rather than being approximated (answer O10's shared piece count
 *   does not exist). A fill with one engine refuses instead of half-filling.
 *   ADR 0002: after every scenario, no foreign id ever reached an engine's
 *   `assumeOwnItem` and no foreign id ever entered an engine's own table. And
 *   the two premise watchdogs: an engine whose reached set SHRINKS is reported
 *   NON_MONOTONE, an engine that reports change forever is reported NO_FIXPOINT
 *   at the bound — detected, bounded, not looped on.
 *
 *   combo-logic-fixpoint — one ROUND. It terminates in a handful of
 *   alternations; it is order-independent (the same world assembled with the
 *   assumed set and the placements in a different order reports the same
 *   facts); crossing exchange works in BOTH directions in one world; the
 *   ARRIVAL GATE holds (with OoT's crossing closed, MM's hosts are not
 *   candidates and MM's goal does not count); and THE PAIR-LEVEL LOCK WITH
 *   REMOVAL — an OoT-origin progression item hosted only in the MM stub makes
 *   beat-both provable, and removing that host flips it unprovable. Both halves,
 *   because without the removal the lock is theatre. The MM bracket's
 *   re-apply-after-restore is locked the same way: a second round over a
 *   restore that drops the engine's table still proves the goal, which it can
 *   only do because the coordinator put the placements back.
 *
 *   combo-logic-fill — the FILL. Same seed, same placement digest; a different
 *   seed, a different one (the sensitivity control, without which the first half
 *   is satisfied by a constant). The `none` rung places the whole bag with the
 *   proof skipped, where `beatable` on the identical world refuses. `beatable`
 *   and `beat-either` produce BYTE-IDENTICAL placements on a world where both
 *   halves prove — the no-bias lock, since a fill that ever short-circuited on
 *   one half would diverge there — and on a world where MM's half cannot prove,
 *   `beat-either` succeeds while `beat-both` refuses, with MM's hosts still
 *   receiving items (the permitted unbeatable half is not starved). The
 *   `all-reachable` rung refuses a world whose placed host is never reached
 *   where `beatable` accepts it.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE and therefore
 * compiled as C++, like every other file in this directory; every symbol under
 * test is C-linkage through combo_logic.h.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

#define CL_ASSERT(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                             \
        }                                                                 \
    } while (0)

namespace {

// ============================================================================
// The stub engine
// ============================================================================

const int kClMaxHosts = 16;
const int kClMaxItems = 8;

// OoT-side ids. Disjoint from the MM ids below ON PURPOSE: that disjointness is
// what turns a misrouted id into a detected ADR 0002 violation instead of a
// plausible-looking wrong answer (the #356 class).
const uint16_t kOotSword = 101;
const uint16_t kOotHook = 102;
const uint16_t kOotLens = 103;
const uint16_t kOotBoots = 104;
const uint16_t kOotJunk = 190; // the junk cover an OoT host gets for a foreign item

const uint16_t kMmOcarina = 201;
const uint16_t kMmBow = 202;
const uint16_t kMmRemains = 203;
const uint16_t kMmMask = 204;
const uint16_t kMmJunk = 290;

enum ClFault {
    CL_FAULT_NONE = 0,
    /** The reached set collapses on the second expand of a round, and the expand
     *  still reports "changed" — a monotone observable falling, which is the
     *  violation ERR_NON_MONOTONE names. */
    CL_FAULT_SHRINK = 1,
    /** `expand` reports change forever with nothing growing — the lie that would
     *  hang world generation, and which ERR_NO_FIXPOINT bounds. */
    CL_FAULT_ALWAYS_CHANGED = 2
};

struct ClEngine {
    uint8_t game;

    // --- the world -------------------------------------------------------
    int hostCount;
    uint16_t hostId[kClMaxHosts];      // ASCENDING: the order contract
    uint32_t hostRequires[kClMaxHosts];
    uint32_t alwaysOffer;              // host indices offered even when unreached
    int itemCount;
    uint16_t itemId[kClMaxItems];      // index k <-> bit (1u << k)
    uint32_t crossingRequires;
    uint32_t goalRequires;
    uint16_t junkId;
    bool overReport;                   // offer hosts it has already placed on
    bool restoreDropsPlacements;       // a harsh restore: the table comes back empty
    bool withSnapshot;
    ClFault fault;

    // --- live state ------------------------------------------------------
    uint32_t have;
    uint32_t reached;
    bool inQuery;
    int expands;
    int beginCalls;
    int endCalls;
    int snapshots;
    int restores;
    int placeCalls;
    uint32_t snapHave;
    uint32_t snapReached;
    int snapPlaced;

    int placedCount;
    uint16_t placedHost[kClMaxHosts];
    SharedItem placedItem[kClMaxHosts];
    bool placedIsCover[kClMaxHosts];

    // --- contract traps --------------------------------------------------
    bool sawForeignAssume;      // an id this engine's table does not name
    bool sawForeignInOwnTable;  // a foreign-origin SharedItem stored verbatim
    bool sawQueryOutsideRound;  // a query call outside begin/end
    bool sawDoubleBegin;
};

ClEngine gClOoT;
ClEngine gClMM;
ComboLogicEngine gClVtOoT;
ComboLogicEngine gClVtMM;

int ClBitIndex(const ClEngine* e, uint16_t id) {
    for (int k = 0; k < e->itemCount; ++k) {
        if (e->itemId[k] == id) {
            return k;
        }
    }
    return -1;
}

int ClHostIndex(const ClEngine* e, uint16_t host) {
    for (int i = 0; i < e->hostCount; ++i) {
        if (e->hostId[i] == host) {
            return i;
        }
    }
    return -1;
}

int ClOwnPlacementIndex(const ClEngine* e, uint16_t host) {
    for (int p = 0; p < e->placedCount; ++p) {
        if (e->placedHost[p] == host) {
            return p;
        }
    }
    return -1;
}

/** The stub's own local fixpoint: which hosts are reached, and which of its own
 *  items it harvests from them. The own-origin harvest lives HERE and not in the
 *  coordinator on purpose — picking up your own item from your own check is the
 *  engine's own search doing what OoT's ReachabilitySearch already does. */
void ClRecompute(ClEngine* e) {
    for (;;) {
        uint32_t nextReached = 0;
        for (int i = 0; i < e->hostCount; ++i) {
            if ((e->hostRequires[i] & e->have) == e->hostRequires[i]) {
                nextReached |= (1u << i);
            }
        }
        uint32_t nextHave = e->have;
        for (int i = 0; i < e->hostCount; ++i) {
            if ((nextReached & (1u << i)) == 0u) {
                continue;
            }
            const int p = ClOwnPlacementIndex(e, e->hostId[i]);
            if (p < 0 || e->placedIsCover[p]) {
                continue;
            }
            const int k = ClBitIndex(e, e->placedItem[p].id);
            if (k >= 0) {
                nextHave |= (1u << k);
            }
        }
        if (nextReached == e->reached && nextHave == e->have) {
            return;
        }
        e->reached = nextReached;
        e->have = nextHave;
    }
}

int ClBeginQuery(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (e->inQuery) {
        e->sawDoubleBegin = true;
    }
    e->beginCalls++;
    e->inQuery = true;
    e->expands = 0;
    // The world's STARTING inventory. This is the call whose omission would make
    // every round after the first answer a question about a richer world than it
    // was asked about (combo_logic.h on beginQuery).
    e->have = 0u;
    e->reached = 0u;
    return 1;
}

void ClEndQuery(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->endCalls++;
    e->inQuery = false;
}

void ClAssumeOwnItem(void* self, uint16_t ownItemId) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    const int k = ClBitIndex(e, ownItemId);
    if (k < 0) {
        // ADR 0002: an id this engine's id-space does not name got routed here.
        e->sawForeignAssume = true;
        return;
    }
    e->have |= (1u << k); // may only ADD
}

int ClExpand(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    e->expands++;
    const uint32_t haveBefore = e->have;
    const uint32_t reachedBefore = e->reached;
    ClRecompute(e);
    if (e->fault == CL_FAULT_SHRINK && e->expands >= 2) {
        e->reached = 0u;
        e->have = 0u;
        return 1;
    }
    if (e->fault == CL_FAULT_ALWAYS_CHANGED) {
        return 1;
    }
    return (e->have != haveBefore || e->reached != reachedBefore) ? 1 : 0;
}

int ClCrossingOpen(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    return ((e->crossingRequires & e->have) == e->crossingRequires) ? 1 : 0;
}

int ClCheckReached(void* self, uint16_t hostCheck) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    const int i = ClHostIndex(e, hostCheck);
    if (i < 0) {
        return 0;
    }
    return ((e->reached & (1u << i)) != 0u) ? 1 : 0;
}

int ClReachedEmptyHosts(void* self, uint16_t* out, int cap) {
    ClEngine* e = (ClEngine*)self;
    int total = 0;
    for (int i = 0; i < e->hostCount; ++i) {
        const bool reached = (e->reached & (1u << i)) != 0u;
        const bool forced = (e->alwaysOffer & (1u << i)) != 0u;
        if (!reached && !forced) {
            continue;
        }
        if (!e->overReport && ClOwnPlacementIndex(e, e->hostId[i]) >= 0) {
            continue;
        }
        if (out != NULL && total < cap) {
            out[total] = e->hostId[i]; // ascending, by construction
        }
        total++;
    }
    return total;
}

int ClGoalReached(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    return ((e->goalRequires & e->have) == e->goalRequires) ? 1 : 0;
}

int ClPlace(void* self, uint16_t hostCheck, SharedItem item) {
    ClEngine* e = (ClEngine*)self;
    e->placeCalls++;

    const bool foreign = (item.originGame != e->game);
    const int existing = ClOwnPlacementIndex(e, hostCheck);
    if (existing >= 0) {
        // IDEMPOTENT for the same (host, item): the coordinator re-applies the
        // whole table after every restore.
        if (e->placedIsCover[existing] == foreign &&
            (foreign || e->placedItem[existing].id == item.id)) {
            return 1;
        }
        return 0; // a different item on an assigned host is a coordinator defect
    }
    if (e->placedCount >= kClMaxHosts) {
        return 0;
    }
    e->placedHost[e->placedCount] = hostCheck;
    e->placedIsCover[e->placedCount] = foreign;
    if (foreign) {
        // JUNK COVER. The engine's own table gets a legal LOCAL item; the
        // foreign identity stays in the coordinator's table (ADR 0002). If this
        // stored `item` verbatim, the trap below would fire.
        SharedItem cover;
        cover.originGame = e->game;
        cover.flags = 0u;
        cover.id = e->junkId;
        e->placedItem[e->placedCount] = cover;
    } else {
        e->placedItem[e->placedCount] = item;
    }
    if (e->placedItem[e->placedCount].originGame != e->game) {
        e->sawForeignInOwnTable = true;
    }
    e->placedCount++;
    // NOT granted: placing is not holding.
    return 1;
}

void ClClearPlacements(void* self) {
    ClEngine* e = (ClEngine*)self;
    // Only what the coordinator put here. A real engine drops its
    // coordinator-assigned shuffled checks and leaves its graph and its own
    // restricted-pool placements alone.
    e->placedCount = 0;
    e->snapPlaced = 0;
}

int ClSnapshot(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->snapshots++;
    e->snapHave = e->have;
    e->snapReached = e->reached;
    e->snapPlaced = e->placedCount;
    return 1;
}

void ClRestore(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->restores++;
    e->have = e->snapHave;
    e->reached = e->snapReached;
    // The harsh variant models a restore whose captured blob does not carry the
    // placement writes. The coordinator's re-apply is what makes the coordinator
    // correct against it either way (audit amendment 1).
    e->placedCount = e->restoreDropsPlacements ? 0 : e->snapPlaced;
}

void ClFillVtable(ComboLogicEngine* vt, ClEngine* e) {
    memset(vt, 0, sizeof(*vt));
    vt->abiVersion = RSBS_COMBO_LOGIC_ENGINE_ABI;
    vt->self = e;
    vt->beginQuery = ClBeginQuery;
    vt->assumeOwnItem = ClAssumeOwnItem;
    vt->expand = ClExpand;
    vt->crossingOpen = ClCrossingOpen;
    vt->checkReached = ClCheckReached;
    vt->reachedEmptyHosts = ClReachedEmptyHosts;
    vt->goalReached = ClGoalReached;
    vt->place = ClPlace;
    vt->clearPlacements = ClClearPlacements;
    vt->endQuery = ClEndQuery;
    if (e->withSnapshot) {
        vt->snapshot = ClSnapshot;
        vt->restore = ClRestore;
    }
}

void ClResetEngine(ClEngine* e, uint8_t game) {
    memset(e, 0, sizeof(*e));
    e->game = game;
    e->junkId = (game == (uint8_t)GAME_OOT) ? kOotJunk : kMmJunk;
}

// `reqMask`, not `requires`: the latter is a C++20 keyword and this file is
// compiled as C++ (the linkage note in the file header).
void ClAddHost(ClEngine* e, uint16_t id, uint32_t reqMask) {
    e->hostId[e->hostCount] = id;
    e->hostRequires[e->hostCount] = reqMask;
    e->hostCount++;
}

void ClAddItem(ClEngine* e, uint16_t id) {
    e->itemId[e->itemCount++] = id;
}

/** Item bit by id, for authoring requirement masks in the scenarios below. */
uint32_t ClBit(const ClEngine* e, uint16_t id) {
    const int k = ClBitIndex(e, id);
    return (k >= 0) ? (1u << k) : 0u;
}

SharedItem ClItem(uint8_t origin, uint16_t id) {
    SharedItem s;
    s.originGame = origin;
    s.flags = 0u;
    s.id = id;
    return s;
}

ComboLogicBagItem ClBagItem(uint8_t origin, uint16_t id, uint16_t itemClass) {
    ComboLogicBagItem b;
    b.item = ClItem(origin, id);
    b.itemClass = itemClass;
    return b;
}

/** Register both stubs and clear the tables. Call after every world edit: the
 *  vtable carries the `self` pointer and the snapshot pair. */
void ClInstall() {
    ClFillVtable(&gClVtOoT, &gClOoT);
    ClFillVtable(&gClVtMM, &gClMM);
    Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT);
    Combo_Logic_RegisterEngine(GAME_MM, &gClVtMM);
    Combo_Logic_ResetPlacements();
}

void ClUninstall() {
    // Reset BEFORE un-registering, so the engines are told to forget too.
    Combo_Logic_ResetPlacements();
    Combo_Logic_RegisterEngine(GAME_OOT, NULL);
    Combo_Logic_RegisterEngine(GAME_MM, NULL);
}

/** True when neither stub saw a contract violation — the ADR 0002 routing trap
 *  included. Checked at the end of every scenario. */
bool ClContractClean() {
    const ClEngine* both[2] = { &gClOoT, &gClMM };
    for (int i = 0; i < 2; ++i) {
        const ClEngine* e = both[i];
        if (e->sawForeignAssume || e->sawForeignInOwnTable || e->sawQueryOutsideRound || e->sawDoubleBegin) {
            printf("[TEST]   %s stub: foreignAssume=%d foreignInTable=%d outsideRound=%d doubleBegin=%d\n",
                   Game_ToString((GameId)e->game), (int)e->sawForeignAssume, (int)e->sawForeignInOwnTable,
                   (int)e->sawQueryOutsideRound, (int)e->sawDoubleBegin);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Worlds
// ============================================================================

/**
 * THE CROSSING WORLD. One OoT host and one MM host, each hosting the OTHER
 * game's progression item, so one round has to exchange in BOTH directions:
 *   OoT host 10 (sphere zero) holds MM's Ocarina  -> unlocks MM host 21
 *   MM  host 20 (sphere zero) holds OoT's Lens    -> proves OoT's goal
 * OoT's goal needs the Lens; MM's goal needs the Ocarina.
 */
void ClBuildCrossingWorld(uint32_t ootCrossingItemMask) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, ClBit(&gClOoT, kOotSword));
    gClOoT.crossingRequires = ootCrossingItemMask;
    gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, ClBit(&gClMM, kMmOcarina));
    gClMM.crossingRequires = 0u; // unconditional from MM's root (audit §4.5)
    gClMM.goalRequires = ClBit(&gClMM, kMmOcarina);
    gClMM.overReport = true; // exercise the coordinator's own occupancy filter
    ClInstall();
}

/** A six-host-per-side world where both halves prove, for the fill rows. */
void ClBuildFillWorld(bool mmHalfProvable) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddItem(&gClOoT, kOotBoots);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, 0u);
    ClAddHost(&gClOoT, 12, 0u);
    ClAddHost(&gClOoT, 13, ClBit(&gClOoT, kOotSword));
    ClAddHost(&gClOoT, 14, ClBit(&gClOoT, kOotHook));
    ClAddHost(&gClOoT, 15, 0u);
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddItem(&gClMM, kMmMask);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, 0u);
    ClAddHost(&gClMM, 22, 0u);
    ClAddHost(&gClMM, 23, ClBit(&gClMM, kMmOcarina));
    ClAddHost(&gClMM, 24, ClBit(&gClMM, kMmBow));
    ClAddHost(&gClMM, 25, 0u);
    gClMM.crossingRequires = 0u;
    // The unprovable variant needs an item that is in NO bag and in NO host, so
    // the half is unbeatable by its own authored parameters — exactly the
    // configuration answer O1 says `beat-either` must permit.
    gClMM.goalRequires = mmHalfProvable ? ClBit(&gClMM, kMmRemains) : ClBit(&gClMM, kMmMask);
    ClInstall();
}

/**
 * A world whose OoT engine OFFERS a host it never reaches (`alwaysOffer` over a
 * requirement no item in the world can meet), and whose two halves both prove
 * unconditionally. `beatable` cannot see the unreached host; `all-reachable`
 * must refuse it. MM has no hosts, so the single bag item has exactly one place
 * to go and the scenario is deterministic rather than probable.
 */
void ClBuildOfferUnreachedWorld() {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddHost(&gClOoT, 10, 1u << 7); // a requirement no item in this world can meet
    gClOoT.alwaysOffer = 1u << 0;    // ... and it is offered anyway
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

/** The bag both fill worlds use. Five advancement items, both origins. */
int ClBuildFillBag(ComboLogicBagItem* bag) {
    int n = 0;
    bag[n++] = ClBagItem((uint8_t)GAME_OOT, kOotLens, RSBS_ITEMCLASS_PROGRESSION);
    bag[n++] = ClBagItem((uint8_t)GAME_MM, kMmRemains, RSBS_ITEMCLASS_DUNGEON_REWARD);
    bag[n++] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
    bag[n++] = ClBagItem((uint8_t)GAME_MM, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
    bag[n++] = ClBagItem((uint8_t)GAME_OOT, kOotHook, RSBS_ITEMCLASS_PROGRESSION);
    return n;
}

int ClRunRound(uint8_t goal, const ComboLogicBagItem* assumed, int assumedCount, ComboLogicRoundResult* out) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = assumed;
    req.assumedCount = assumedCount;
    req.goal = goal;
    return Combo_Logic_RunRound(&req, out);
}

int ClRunFill(const ComboLogicBagItem* bag, int bagCount, uint8_t goal, uint8_t rung, uint32_t seed,
              ComboLogicFillResult* out) {
    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = bagCount;
    req.goal = goal;
    req.logicRung = rung;
    req.seed = seed;
    req.maxAttempts = 0; // the default batch roll-back count
    return Combo_Logic_RunFill(&req, out);
}

} // namespace

// ============================================================================
// Row 1 — the contract
// ============================================================================

TestResult Test_ComboLogicEngineSurface(void) {
    printf("[TEST] combo-logic-engine-surface: registration, GOAL table, ADR 0002 routing, premise watchdogs\n");

    ClUninstall();

    // --- registration validation -----------------------------------------
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClFillVtable(&gClVtOoT, &gClOoT);

    CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_NONE, &gClVtOoT), "GAME_NONE has no id-space and must be refused");

    {
        ComboLogicEngine bad = gClVtOoT;
        bad.abiVersion = RSBS_COMBO_LOGIC_ENGINE_ABI + 7u;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad), "an ABI mismatch must be refused, not called through");
        CL_ASSERT(Combo_Logic_GetEngine(GAME_OOT) == NULL, "a refused registration must leave the registry unchanged");
    }
    {
        ComboLogicEngine bad = gClVtOoT;
        bad.expand = NULL;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad), "a NULL required entry point must be refused");
    }
    {
        ComboLogicEngine bad = gClVtOoT;
        bad.snapshot = ClSnapshot;
        bad.restore = NULL;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad),
                  "a snapshot without its restore must be refused: it would silently corrupt the save");
    }

    CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT), "a well-formed engine must register");
    CL_ASSERT(Combo_Logic_GetEngine(GAME_OOT) == &gClVtOoT, "the registry must serve what was registered");
    CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, NULL), "NULL must un-register");
    CL_ASSERT(Combo_Logic_GetEngine(GAME_OOT) == NULL, "un-registration must clear the slot");

    // --- the GOAL truth table ---------------------------------------------
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_BOTH, 1, 1) == 1, "beat-both: 1 AND 1");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_BOTH, 1, 0) == 0, "beat-both: 1 AND 0");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_BOTH, 0, 1) == 0, "beat-both: 0 AND 1");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 1, 0) == 1, "beat-either: an unbeatable half is OK");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 0, 1) == 1, "beat-either is symmetric");
    // The OR is never an XOR: both halves provable is a welcome outcome.
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 1, 1) == 1,
              "beat-either must not be narrowed to an XOR (ADR 0010 §1.2)");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 0, 0) == 0, "beat-either: neither half");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_TRIFORCE_HUNT, 1, 1) == -1,
              "triforce-hunt has no evaluator until answer O10's shared piece count exists");
    CL_ASSERT(Combo_Logic_EvaluateGoal(0u, 1, 1) == -1, "an unpinned GOAL value has no evaluator");

    // --- the fill's refusals, before any world is authored ----------------
    {
        ComboLogicBagItem bag[1];
        ComboLogicFillResult res;
        bag[0] = ClBagItem((uint8_t)GAME_OOT, kOotLens, RSBS_ITEMCLASS_PROGRESSION);

        ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
        ClAddItem(&gClOoT, kOotLens);
        ClAddHost(&gClOoT, 10, 0u);
        ClFillVtable(&gClVtOoT, &gClOoT);
        Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT);
        Combo_Logic_RegisterEngine(GAME_MM, NULL);

        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_NO_ENGINE,
                  "a paired fill with one engine must refuse, not half-fill");
        CL_ASSERT(res.placed == 0, "a refused fill must place nothing");

        ClResetEngine(&gClMM, (uint8_t)GAME_MM);
        ClAddItem(&gClMM, kMmRemains);
        ClAddHost(&gClMM, 20, 0u);
        ClInstall();

        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_TRIFORCE_HUNT, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL,
                  "triforce-hunt must refuse rather than be evaluated as beat-both");
        CL_ASSERT(res.placed == 0, "an unsupported goal must be caught before anything is placed");
        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_BEAT_BOTH, 0u, 1u, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "an unpinned logic rung must refuse");
        CL_ASSERT(ClRunFill(NULL, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "a NULL bag with a nonzero count must refuse");
        {
            ComboLogicBagItem untagged[1];
            untagged[0] = ClBagItem((uint8_t)GAME_NONE, 5u, RSBS_ITEMCLASS_PROGRESSION);
            CL_ASSERT(ClRunFill(untagged, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                          RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                      "an untagged bag entry must refuse (ADR 0002: the origin is the id-space)");
        }
    }

    // --- ADR 0002: a foreign item is covered, never stored ----------------
    ClBuildCrossingWorld(0u);
    {
        ComboLogicRoundResult round;
        CL_ASSERT(Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION),
                  "authoring an MM-origin item into an OoT host must succeed");
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_OK, "the round must run");
        CL_ASSERT(gClOoT.placedCount == 1 && gClOoT.placedIsCover[0], "the OoT host must hold a junk COVER");
        CL_ASSERT(gClOoT.placedItem[0].originGame == (uint8_t)GAME_OOT && gClOoT.placedItem[0].id == kOotJunk,
                  "the cover must be a legal LOCAL item: a raw foreign id never enters the host's own table");
        CL_ASSERT(ClContractClean(), "no foreign id may reach an engine's assumeOwnItem or its own table");
    }

    // --- premise watchdog 1: a monotone observable that falls -------------
    ClBuildCrossingWorld(0u);
    gClOoT.fault = CL_FAULT_SHRINK;
    ClInstall();
    {
        ComboLogicRoundResult round;
        const int st = ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round);
        CL_ASSERT(st == RSBS_COMBO_LOGIC_ERR_NON_MONOTONE,
                  "a reached set that SHRINKS inside one round must be reported non-monotone");
        CL_ASSERT(round.iterations <= 8, "the violation must be caught where it happens, not looped on");
        CL_ASSERT(gClOoT.endCalls == gClOoT.beginCalls && gClMM.endCalls == gClMM.beginCalls,
                  "a failed round must still close its bracket on every engine it opened");
    }

    // --- premise watchdog 2: `changed` forever ----------------------------
    ClBuildCrossingWorld(0u);
    gClMM.fault = CL_FAULT_ALWAYS_CHANGED;
    ClInstall();
    {
        ComboLogicRoundResult round;
        const int st = ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round);
        CL_ASSERT(st == RSBS_COMBO_LOGIC_ERR_NO_FIXPOINT, "an engine reporting change forever must be bounded");
        CL_ASSERT(round.iterations == RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS,
                  "the bound must be the watchdog's, and the loop must stop AT it");
        CL_ASSERT(gClMM.endCalls == gClMM.beginCalls, "the bracket must still close");
    }

    ClUninstall();
    printf("[TEST] combo-logic-engine-surface: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 2 — one round
// ============================================================================

TestResult Test_ComboLogicFixpoint(void) {
    printf("[TEST] combo-logic-fixpoint: termination, order independence, both crossing directions, the pair-level "
           "lock with removal\n");

    // --- both crossing directions in one world, and termination ----------
    ClBuildCrossingWorld(0u);
    {
        ComboLogicRoundResult round;
        CL_ASSERT(Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION),
                  "author the MM Ocarina into OoT's sphere-zero host");
        CL_ASSERT(Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION),
                  "author the OoT Lens into MM's sphere-zero host");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_OK, "the round must run");
        CL_ASSERT(round.iterations >= 2 && round.iterations <= 8,
                  "the fixpoint must terminate in a handful of alternations (audit §4.3 expects two or three)");
        CL_ASSERT(round.exchanged == 2, "exactly two exchanges: one per direction");
        CL_ASSERT(round.goalMM == 1, "OoT -> MM: the Ocarina found in OoT must prove MM's half");
        CL_ASSERT(round.goalOoT == 1, "MM -> OoT: the Lens found in MM must prove OoT's half");
        CL_ASSERT(round.goalExpression == 1, "beat-both holds when both halves do");
        CL_ASSERT(round.candidatesMM >= 1, "MM's host 21, opened by the exchanged Ocarina, must be a candidate");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- order independence ----------------------------------------------
    // The same world, authored in the OPPOSITE order and with the assumed set
    // permuted, must report the same FACTS. The placement digest deliberately is
    // NOT compared: insertion order is world-visible and part of it.
    ComboLogicRoundResult forward;
    ComboLogicRoundResult reverse;
    {
        ComboLogicBagItem assumedA[2];
        ComboLogicBagItem assumedB[2];
        assumedA[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        assumedA[1] = ClBagItem((uint8_t)GAME_MM, kMmBow, RSBS_ITEMCLASS_PROGRESSION);
        assumedB[0] = assumedA[1];
        assumedB[1] = assumedA[0];

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, assumedA, 2, &forward) == RSBS_COMBO_LOGIC_OK, "round A");

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, assumedB, 2, &reverse) == RSBS_COMBO_LOGIC_OK, "round B");

        CL_ASSERT(forward.goalOoT == reverse.goalOoT && forward.goalMM == reverse.goalMM &&
                      forward.goalExpression == reverse.goalExpression,
                  "the fixpoint's goal facts must not depend on the order facts arrived in");
        CL_ASSERT(forward.candidatesOoT == reverse.candidatesOoT && forward.candidatesMM == reverse.candidatesMM,
                  "nor must the candidate sets");
        CL_ASSERT(forward.crossingOpenOoT == reverse.crossingOpenOoT && forward.crossingOpenMM == reverse.crossingOpenMM,
                  "nor the crossing facts");
        CL_ASSERT(forward.exchanged == reverse.exchanged, "nor the number of exchanges");
    }

    // --- the arrival gate -------------------------------------------------
    // OoT's crossing needs the Sword. Until it is held, Termina is unreachable:
    // MM's hosts are not candidates and MM's half does not count as proved, even
    // though MM's own engine would answer yes.
    {
        ComboLogicRoundResult closed;
        ComboLogicRoundResult open;
        ComboLogicBagItem sword[1];

        ClBuildCrossingWorld(1u << 0); // bit 0 == kOotSword
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, NULL, 0, &closed) == RSBS_COMBO_LOGIC_OK, "gated round");
        CL_ASSERT(closed.crossingOpenOoT == 0, "with no Sword, OoT's crossing must be closed");
        CL_ASSERT(closed.candidatesMM == 0, "a Termina the player cannot enter offers no hosts");
        CL_ASSERT(closed.goalMM == 0, "nor may its half be counted as proved");
        CL_ASSERT(closed.exchanged == 0, "nor may an item cross a closed crossing");

        sword[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, sword, 1, &open) == RSBS_COMBO_LOGIC_OK, "ungated round");
        CL_ASSERT(open.crossingOpenOoT == 1, "the Sword opens OoT's crossing");
        CL_ASSERT(open.candidatesMM >= 1, "and Termina's hosts become candidates");
        CL_ASSERT(open.goalMM == 1, "and MM's half can be proved");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- THE PAIR-LEVEL LOCK, WITH REMOVAL -------------------------------
    // An OoT-origin progression item hosted ONLY in the MM stub. OoT's goal
    // needs the Lens; no OoT host holds it; the only copy is in Termina.
    {
        ComboLogicRoundResult withHost;
        ComboLogicRoundResult withoutHost;

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &withHost) == RSBS_COMBO_LOGIC_OK, "paired round");
        CL_ASSERT(withHost.goalExpression == 1,
                  "with the OoT item hosted in MM, the pair's goal must be provable — this is the half that is easy");

        // REMOVAL. Identical world, minus that one host. Without this half the
        // assertion above is theatre: it would pass over any world at all.
        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &withoutHost) == RSBS_COMBO_LOGIC_OK, "removed round");
        CL_ASSERT(withoutHost.goalOoT == 0, "with the only Lens gone, OoT's half must NOT be provable");
        CL_ASSERT(withoutHost.goalExpression == 0, "so beat-both must flip to unprovable");
        // ... and the same world under beat-either is a legitimate world with one
        // unbeatable half (answer O1), not a failure.
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, NULL, 0, &withoutHost) == RSBS_COMBO_LOGIC_OK, "either round");
        CL_ASSERT(withoutHost.goalMM == 1 && withoutHost.goalOoT == 0,
                  "MM's half still proves and OoT's still does not");
        CL_ASSERT(withoutHost.goalExpression == 1, "beat-either permits the unbeatable half");
    }

    // --- MM's bracket: the re-apply after restore ------------------------
    // A restore that brings the engine's table back EMPTY. The coordinator's
    // re-apply is the only reason the second round still sees the placement —
    // and MM's half depends on harvesting it, so the lock is not vacuous.
    {
        ComboLogicRoundResult first;
        ComboLogicRoundResult second;

        ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
        ClAddItem(&gClOoT, kOotSword);
        ClAddHost(&gClOoT, 10, 0u);
        gClOoT.crossingRequires = 0u;
        gClOoT.goalRequires = 0u;

        ClResetEngine(&gClMM, (uint8_t)GAME_MM);
        ClAddItem(&gClMM, kMmOcarina);
        ClAddItem(&gClMM, kMmBow);
        ClAddItem(&gClMM, kMmRemains);
        ClAddHost(&gClMM, 20, 0u);
        gClMM.crossingRequires = 0u;
        gClMM.goalRequires = ClBit(&gClMM, kMmRemains);
        gClMM.withSnapshot = true;
        gClMM.restoreDropsPlacements = true;
        ClInstall();

        CL_ASSERT(Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_MM, kMmRemains), RSBS_ITEMCLASS_DUNGEON_REWARD),
                  "author MM's own Remains into an MM host");
        CL_ASSERT(gClMM.placedCount == 1, "the engine must hold it");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &first) == RSBS_COMBO_LOGIC_OK, "round 1");
        CL_ASSERT(first.goalMM == 1, "round 1 harvests the Remains from its own reached host");
        CL_ASSERT(gClMM.snapshots == 1 && gClMM.restores == 1, "one snapshot/restore brackets the whole round");
        CL_ASSERT(gClMM.placedCount == 1, "after the restore dropped it, the coordinator must have put it back");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &second) == RSBS_COMBO_LOGIC_OK, "round 2");
        CL_ASSERT(second.goalMM == 1,
                  "round 2 must prove the same half: without the re-apply the table would be empty here");
        CL_ASSERT(gClMM.placedCount == 1, "and the table must still hold it");
        CL_ASSERT(gClOoT.snapshots == 0 && gClOoT.restores == 0,
                  "an engine with pure queries declares no snapshot pair and must not be bracketed");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    ClUninstall();
    printf("[TEST] combo-logic-fixpoint: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 3 — the fill
// ============================================================================

TestResult Test_ComboLogicFill(void) {
    printf("[TEST] combo-logic-fill: determinism with its sensitivity control, the rungs, beat-either's lack of "
           "bias\n");

    ComboLogicBagItem bag[8];
    const int bagCount = ClBuildFillBag(bag);
    ComboLogicFillResult res;

    // --- same seed, same placement; different seed, a different one -------
    uint32_t digestA = 0u;
    uint32_t digestB = 0u;
    uint32_t digestOther = 0u;
    {
        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the fill must succeed on a world where both halves prove");
        CL_ASSERT(res.placed == bagCount, "every bag item must be placed");
        CL_ASSERT(res.goalProven && !res.proofSkipped, "the beatable rung must have evaluated the exit condition");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM) == bagCount,
                  "the two tables together must hold the whole bag");
        digestA = res.placementDigest;
        CL_ASSERT(digestA != 0u, "the digest must cover something");

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the same fill must succeed again");
        digestB = res.placementDigest;
        CL_ASSERT(digestA == digestB, "the same seed must place identically: the world is a function of the identity");

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1235u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "a neighbouring seed must also succeed");
        digestOther = res.placementDigest;
        // THE SENSITIVITY CONTROL. Without it "same seed, same digest" is
        // satisfied by a fill that ignores the seed entirely.
        CL_ASSERT(digestOther != digestA, "a different seed must place differently");
    }

    // --- the `none` rung places without proof; `beatable` refuses --------
    {
        ClBuildFillWorld(false); // MM's goal item is in no bag and no host
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 0x0B00u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the base rung is the same fill with the proof skipped: it must succeed");
        CL_ASSERT(res.proofSkipped, "and say so");
        CL_ASSERT(!res.goalProven, "and claim nothing: `not proven` is not `failed`, and it is not `proven` either");
        CL_ASSERT(res.placed == bagCount, "the whole bag must still be distributed");
        CL_ASSERT(res.attempts == 1, "and with no proof obligation there is nothing to retry");

        ClBuildFillWorld(false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x0B00u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "the identical world under `beatable` must refuse: the GOAL is the fill's exit condition");
        CL_ASSERT(!res.goalProven && !res.proofSkipped, "a refused proof is neither proven nor skipped");
        CL_ASSERT(res.attempts == RSBS_COMBO_LOGIC_FILL_RETRIES, "and the batch roll-backs must all have been spent");
    }

    // --- beat-either: no bias, and no starved half ------------------------
    {
        // (1) On a world where BOTH halves prove, beat-either and beat-both must
        //     place BYTE-IDENTICALLY for one seed. A fill that ever short-circuited
        //     once one half proved would diverge here, which is the XOR bias ADR
        //     0010 §1.2 forbids.
        uint32_t both = 0u;
        uint32_t either = 0u;

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xC0FFEEu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-both on the provable world");
        both = res.placementDigest;

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE, 0xC0FFEEu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-either on the same world");
        either = res.placementDigest;
        CL_ASSERT(both == either, "beat-either must not bias the placement toward an asymmetric outcome");

        // (2) A half that cannot prove is PERMITTED, and its checks are not
        //     starved: MM still hosts items.
        ClBuildFillWorld(false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE, 0xC0FFEEu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-either must accept a world whose MM half its own parameters make unbeatable");
        CL_ASSERT(res.goalProven, "the OR held");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_MM) > 0,
                  "the permitted unbeatable half must still receive items, not be emptied");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) > 0, "and so must the provable one");
    }

    // --- the all-reachable rung adds a real obligation --------------------
    // An engine that OFFERS a host it does not reach. `beatable` cannot see it
    // (the goal still proves); `all-reachable` must refuse it.
    {
        ComboLogicBagItem one[1];
        one[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);

        ClBuildOfferUnreachedWorld();
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 7u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "`beatable` asks only for the GOAL, which this world proves");
        CL_ASSERT(res.goalProven && !res.allHostsReached,
                  "and it reports honestly that a placed host was not reached");

        ClBuildOfferUnreachedWorld();
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 7u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_NOT_ALL_REACHED,
                  "`all-reachable` must refuse the same world: its obligation is strictly stronger");
        CL_ASSERT(res.attempts == RSBS_COMBO_LOGIC_FILL_RETRIES,
                  "and the batch roll-backs must be real roll-backs: every attempt must have re-placed, not "
                  "dead-ended on the previous attempt's leftovers");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- the fill's own bracket bookkeeping ------------------------------
    {
        ClBuildFillWorld(true);
        gClMM.withSnapshot = true;
        gClMM.restoreDropsPlacements = true;
        ClInstall();
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the fill must survive a restore that drops the engine's table every round");
        CL_ASSERT(gClMM.snapshots == res.rounds && gClMM.restores == res.rounds,
                  "exactly one snapshot/restore per round, not per call");
        CL_ASSERT(gClMM.beginCalls == res.rounds && gClMM.endCalls == res.rounds,
                  "and exactly one begin/end per round on each engine");
        CL_ASSERT(gClMM.placedCount == Combo_Logic_PlacementCount(GAME_MM),
                  "at the end the engine's own table must agree with the coordinator's");
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_MM); ++i) {
            ComboLogicPlacement p;
            CL_ASSERT(Combo_Logic_PlacementAt(GAME_MM, i, &p), "read back the coordinator's MM placement");
            CL_ASSERT(ClOwnPlacementIndex(&gClMM, p.hostCheck) >= 0,
                      "every coordinator placement must be present in the engine after the re-apply");
            ComboLogicPlacement byHost;
            CL_ASSERT(Combo_Logic_GetPlacement(GAME_MM, p.hostCheck, &byHost) && byHost.item.id == p.item.id,
                      "and the occupancy lookup must agree with the ordered table");
            CL_ASSERT(byHost.itemClass != 0u, "the bag row's item class must be carried onto the placement");
        }
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    ClUninstall();
    printf("[TEST] combo-logic-fill: PASS\n");
    return TEST_PASS;
}
