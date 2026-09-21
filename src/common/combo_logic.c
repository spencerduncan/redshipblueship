/**
 * @file combo_logic.c
 * @brief The combo-logic coordinator: union bag, two placement tables, round
 *        loop, single-bag assumed fill (ADR 0010 increment 3, #645).
 *
 * See combo_logic.h for the contract — this TU implements it and nothing else.
 * Like foreign_items.c and shared_items.c it is deliberately free of game
 * headers: every game-specific fact arrives through a registered
 * ComboLogicEngine vtable, so this file compiles into redship_common and the
 * ROM-free test harness drives it with stub engines.
 *
 * THREADING: game thread only, like every other src/common coordinator. The
 * working buffers below are file-static rather than stack-allocated (a bag of
 * 512 items plus two 1024-host candidate lists is ~6 KB, and the fill recurses
 * nowhere), which is only safe because of that. A future off-thread caller must
 * marshal, exactly as shared_items.h's sourced-grant seam requires.
 */

#include "combo_logic.h"

#include <stdio.h>
#include <string.h>

// ============================================================================
// The registry
// ============================================================================

// Indexed by GameId, so the bound is the origin-id space's size, not a pool
// property — RSBS_FOREIGN_POOL_ORIGIN_COUNT is reused because it already IS
// "one past the highest origin id" (foreign_items.h) and a second constant for
// the same fact could drift against it.
static const ComboLogicEngine* sEngines[RSBS_FOREIGN_POOL_ORIGIN_COUNT];

static bool ComboLogicIsGame(uint8_t game) {
    return game == (uint8_t)GAME_OOT || game == (uint8_t)GAME_MM;
}

static const ComboLogicEngine* ComboLogicEngineFor(uint8_t game) {
    if (!ComboLogicIsGame(game)) {
        return NULL;
    }
    return sEngines[game];
}

bool Combo_Logic_RegisterEngine(GameId originGame, const ComboLogicEngine* engine) {
    const uint8_t g = (uint8_t)originGame;

    if (!ComboLogicIsGame(g)) {
        fprintf(stderr, "[ComboLogic] engine registration rejected: origin %u is not a game id-space\n", (unsigned)g);
        return false;
    }

    if (engine == NULL) {
        // Un-register, so a test can restore the registry instead of leaving
        // process-global state behind.
        sEngines[g] = NULL;
        return true;
    }

    if (engine->abiVersion != RSBS_COMBO_LOGIC_ENGINE_ABI) {
        fprintf(stderr, "[ComboLogic] engine registration rejected: %s engine ABI %u, this build speaks %u\n",
                Game_ToString((GameId)g), (unsigned)engine->abiVersion, (unsigned)RSBS_COMBO_LOGIC_ENGINE_ABI);
        return false;
    }

    // A hole in the vtable is refused here rather than dereferenced in the
    // middle of a round, where the crash would name the coordinator.
    if (engine->beginQuery == NULL || engine->assumeOwnItem == NULL || engine->expand == NULL ||
        engine->crossingOpen == NULL || engine->checkReached == NULL || engine->reachedEmptyHosts == NULL ||
        engine->goalReached == NULL || engine->place == NULL || engine->clearPlacements == NULL ||
        engine->endQuery == NULL) {
        fprintf(stderr, "[ComboLogic] engine registration rejected: %s engine has a NULL required entry point\n",
                Game_ToString((GameId)g));
        return false;
    }

    // Both or neither: a snapshot with no restore silently corrupts the save,
    // a restore with no snapshot silently reverts to nothing.
    if ((engine->snapshot == NULL) != (engine->restore == NULL)) {
        fprintf(stderr, "[ComboLogic] engine registration rejected: %s engine has snapshot without restore (or vice "
                        "versa)\n",
                Game_ToString((GameId)g));
        return false;
    }

    if (sEngines[g] != NULL && sEngines[g] != engine) {
        fprintf(stderr, "[ComboLogic] %s engine re-registered; the previous engine is replaced\n",
                Game_ToString((GameId)g));
    }
    sEngines[g] = engine;
    return true;
}

const ComboLogicEngine* Combo_Logic_GetEngine(GameId originGame) {
    return ComboLogicEngineFor((uint8_t)originGame);
}

const char* Combo_Logic_StatusName(int status) {
    switch (status) {
        case RSBS_COMBO_LOGIC_OK: return "ok";
        case RSBS_COMBO_LOGIC_ERR_NO_ENGINE: return "no-engine";
        case RSBS_COMBO_LOGIC_ERR_BAD_REQUEST: return "bad-request";
        case RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL: return "unsupported-goal";
        case RSBS_COMBO_LOGIC_ERR_NON_MONOTONE: return "non-monotone";
        case RSBS_COMBO_LOGIC_ERR_NO_FIXPOINT: return "no-fixpoint";
        case RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE: return "no-candidate";
        case RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE: return "goal-unprovable";
        case RSBS_COMBO_LOGIC_ERR_NOT_ALL_REACHED: return "not-all-reached";
        case RSBS_COMBO_LOGIC_ERR_CAPACITY: return "capacity";
        case RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED: return "engine-refused";
        default: return "(unknown)";
    }
}

// ============================================================================
// The two placement tables (RAM only — see the header on the O7 carve)
// ============================================================================
//
// The ordered array is the world-visible record (insertion order is the fill's
// own order). The bitmap beside it answers occupancy in O(1), which matters:
// the candidate filter asks "is this host assigned" once per candidate per
// round, and the fill runs a round per bag item — a linear scan there is
// O(bag * hosts * placements) and would dominate the creation budget (#582).

#define COMBO_LOGIC_OCCUPANCY_BYTES (65536 / 8)

static ComboLogicPlacement sPlacements[RSBS_FOREIGN_POOL_ORIGIN_COUNT][RSBS_COMBO_LOGIC_PLACEMENT_CAP];
static int sPlacementCount[RSBS_FOREIGN_POOL_ORIGIN_COUNT];
static uint8_t sOccupied[RSBS_FOREIGN_POOL_ORIGIN_COUNT][COMBO_LOGIC_OCCUPANCY_BYTES];
/** Per-round bookkeeping: which placement indices have already had their
 *  foreign item handed across the crossing this round. Reset per round, not per
 *  fill — an exchange is a fact about one round's granted set. */
static uint8_t sExchanged[RSBS_FOREIGN_POOL_ORIGIN_COUNT][RSBS_COMBO_LOGIC_PLACEMENT_CAP / 8];

static bool ComboLogicBitGet(const uint8_t* bits, int index) {
    return (bits[index >> 3] & (uint8_t)(1u << (index & 7))) != 0;
}

static void ComboLogicBitSet(uint8_t* bits, int index) {
    bits[index >> 3] |= (uint8_t)(1u << (index & 7));
}

void Combo_Logic_ResetPlacements(void) {
    memset(sPlacementCount, 0, sizeof(sPlacementCount));
    memset(sOccupied, 0, sizeof(sOccupied));
    memset(sExchanged, 0, sizeof(sExchanged));

    // Both sides must forget, or a retry is not a roll-back: the next attempt
    // would find the previous one's hosts assigned inside the engines and report
    // a dead end that belongs to the retry rather than to the world.
    const uint8_t order[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
    for (int s = 0; s < 2; ++s) {
        const ComboLogicEngine* e = sEngines[order[s]];
        if (e != NULL) {
            e->clearPlacements(e->self);
        }
    }
}

int Combo_Logic_PlacementCount(GameId hostGame) {
    const uint8_t g = (uint8_t)hostGame;
    return ComboLogicIsGame(g) ? sPlacementCount[g] : 0;
}

bool Combo_Logic_PlacementAt(GameId hostGame, int index, ComboLogicPlacement* out) {
    const uint8_t g = (uint8_t)hostGame;
    if (!ComboLogicIsGame(g) || index < 0 || index >= sPlacementCount[g]) {
        return false;
    }
    if (out != NULL) {
        *out = sPlacements[g][index];
    }
    return true;
}

bool Combo_Logic_GetPlacement(GameId hostGame, uint16_t hostCheck, ComboLogicPlacement* out) {
    const uint8_t g = (uint8_t)hostGame;
    if (!ComboLogicIsGame(g) || !ComboLogicBitGet(sOccupied[g], (int)hostCheck)) {
        return false;
    }
    for (int i = 0; i < sPlacementCount[g]; ++i) {
        if (sPlacements[g][i].hostCheck == hostCheck) {
            if (out != NULL) {
                *out = sPlacements[g][i];
            }
            return true;
        }
    }
    return false;
}

/** Append one placement. False on capacity, or on a host already assigned (the
 *  fill never offers one, so this is a defect trap, not a flow-control path). */
static bool ComboLogicAddPlacement(uint8_t hostGame, uint16_t hostCheck, SharedItem item, uint16_t itemClass) {
    if (!ComboLogicIsGame(hostGame)) {
        return false;
    }
    if (ComboLogicBitGet(sOccupied[hostGame], (int)hostCheck)) {
        fprintf(stderr, "[ComboLogic] placement rejected: %s check %u is already assigned\n",
                Game_ToString((GameId)hostGame), (unsigned)hostCheck);
        return false;
    }
    if (sPlacementCount[hostGame] >= RSBS_COMBO_LOGIC_PLACEMENT_CAP) {
        fprintf(stderr, "[ComboLogic] placement rejected: %s table full (%d)\n", Game_ToString((GameId)hostGame),
                RSBS_COMBO_LOGIC_PLACEMENT_CAP);
        return false;
    }
    ComboLogicPlacement* p = &sPlacements[hostGame][sPlacementCount[hostGame]++];
    p->hostCheck = hostCheck;
    p->itemClass = itemClass;
    p->item = item;
    ComboLogicBitSet(sOccupied[hostGame], (int)hostCheck);
    return true;
}

bool Combo_Logic_Place(GameId hostGame, uint16_t hostCheck, SharedItem item, uint16_t itemClass) {
    const uint8_t g = (uint8_t)hostGame;

    if (!ComboLogicIsGame(g) || !ComboLogicIsGame(item.originGame)) {
        return false;
    }
    const ComboLogicEngine* e = ComboLogicEngineFor(g);
    if (e == NULL) {
        fprintf(stderr, "[ComboLogic] authored placement refused: no %s engine\n", Game_ToString(hostGame));
        return false;
    }
    if (!ComboLogicAddPlacement(g, hostCheck, item, itemClass)) {
        return false;
    }
    if (!e->place(e->self, hostCheck, item)) {
        // Undo the table entry: the two must never disagree about what the host
        // holds, and a half-recorded placement is the worse of the two states.
        sPlacementCount[g]--;
        sOccupied[g][hostCheck >> 3] &= (uint8_t)~(1u << (hostCheck & 7));
        fprintf(stderr, "[ComboLogic] authored placement refused by the %s engine on check %u\n",
                Game_ToString(hostGame), (unsigned)hostCheck);
        return false;
    }
    return true;
}

static void ComboLogicDigestByte(uint32_t* h, uint8_t b) {
    *h ^= (uint32_t)b;
    *h *= 16777619u;
}

uint32_t Combo_Logic_PlacementDigest(void) {
    uint32_t h = 2166136261u; // FNV-1a offset basis
    const uint8_t order[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };

    for (int s = 0; s < 2; ++s) {
        const uint8_t g = order[s];
        ComboLogicDigestByte(&h, g);
        ComboLogicDigestByte(&h, (uint8_t)(sPlacementCount[g] & 0xFF));
        ComboLogicDigestByte(&h, (uint8_t)((sPlacementCount[g] >> 8) & 0xFF));
        for (int i = 0; i < sPlacementCount[g]; ++i) {
            const ComboLogicPlacement* p = &sPlacements[g][i];
            ComboLogicDigestByte(&h, (uint8_t)(p->hostCheck & 0xFF));
            ComboLogicDigestByte(&h, (uint8_t)((p->hostCheck >> 8) & 0xFF));
            ComboLogicDigestByte(&h, p->item.originGame);
            ComboLogicDigestByte(&h, (uint8_t)(p->item.id & 0xFF));
            ComboLogicDigestByte(&h, (uint8_t)((p->item.id >> 8) & 0xFF));
            ComboLogicDigestByte(&h, (uint8_t)(p->itemClass & 0xFF));
            ComboLogicDigestByte(&h, (uint8_t)((p->itemClass >> 8) & 0xFF));
        }
    }
    return h;
}

// ============================================================================
// GOAL evaluation
// ============================================================================

int Combo_Logic_EvaluateGoal(uint8_t goal, int ootGoalReached, int mmGoalReached) {
    const int o = (ootGoalReached != 0) ? 1 : 0;
    const int m = (mmGoalReached != 0) ? 1 : 0;

    switch (goal) {
        case RSBS_COMBO_GOAL_BEAT_BOTH: return (o && m) ? 1 : 0;
        // A PLAIN OR. Both halves provable is a welcome outcome and is never
        // constrained away, perturbed or re-rolled to restore asymmetry (ADR
        // 0010 §1.2 / answer O1) — which is why there is nothing here but the
        // disjunction.
        case RSBS_COMBO_GOAL_BEAT_EITHER: return (o || m) ? 1 : 0;
        // Answer O10 rules ONE shared piece count across both worlds, carried
        // shared-resource-style. That carrier does not exist (epic #645 item 5),
        // and per-half composition is exactly what O10 rejected — so this is
        // refused rather than approximated.
        case RSBS_COMBO_GOAL_TRIFORCE_HUNT: return -1;
        default: return -1;
    }
}

// ============================================================================
// The private RNG
// ============================================================================
//
// splitmix32. Private and injected (ComboLogicFillRequest.seed), never either
// game's RNG: a query runs a variable number of times per fill, so drawing from
// a game's stream would couple the world to the search's shape instead of to
// the frozen identity. Rolled here rather than reused from a port for the same
// reason the boundary is game-header-free.

static uint32_t ComboLogicRngNext(uint32_t* state) {
    uint32_t z = (*state += 0x9E3779B9u);
    z = (z ^ (z >> 16)) * 0x21F0AAADu;
    z = (z ^ (z >> 15)) * 0x735A2D97u;
    return z ^ (z >> 15);
}

/** Uniform in [0, bound), by rejection — a plain modulo would bias the low ids
 *  and the bias would be invisible in every determinism row. */
static uint32_t ComboLogicRngBelow(uint32_t* state, uint32_t bound) {
    if (bound <= 1u) {
        return 0u;
    }
    const uint32_t threshold = (0u - bound) % bound; // 2^32 mod bound
    for (;;) {
        const uint32_t r = ComboLogicRngNext(state);
        if (r >= threshold) {
            return r % bound;
        }
    }
}

/** The per-attempt stream. Derived deterministically from the request seed and
 *  the attempt index, so a retry explores a genuinely different order while the
 *  whole fill stays a pure function of (seed, inputs). */
static uint32_t ComboLogicAttemptSeed(uint32_t seed, int attempt) {
    uint32_t s = seed ^ (0x85EBCA6Bu * (uint32_t)(attempt + 1));
    (void)ComboLogicRngNext(&s);
    return s;
}

static void ComboLogicShuffle(int* order, int count, uint32_t* rng) {
    for (int i = count - 1; i > 0; --i) {
        const uint32_t j = ComboLogicRngBelow(rng, (uint32_t)(i + 1));
        const int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
}

// ============================================================================
// One round
// ============================================================================

/** Candidate hosts collected from one engine, filtered by our own occupancy. */
typedef struct {
    uint16_t host[RSBS_COMBO_LOGIC_PLACEMENT_CAP];
    int count;
} ComboLogicHostBuf;

static ComboLogicHostBuf sCandidates[RSBS_FOREIGN_POOL_ORIGIN_COUNT];
static uint16_t sHostScratch[RSBS_COMBO_LOGIC_PLACEMENT_CAP];

/**
 * Ask one engine for its candidate hosts and keep the ones our tables do not
 * already hold. Occupancy has exactly one authority (ours), so an engine that
 * over-reports costs nothing here.
 *
 * @return RSBS_COMBO_LOGIC_OK, or ERR_CAPACITY when the engine reports more
 *         hosts than the scratch buffer holds — refused rather than truncated,
 *         because a truncated candidate set silently narrows the world to a
 *         prefix of one engine's table.
 */
static int ComboLogicCollectCandidates(uint8_t game) {
    ComboLogicHostBuf* buf = &sCandidates[game];
    buf->count = 0;

    const ComboLogicEngine* e = ComboLogicEngineFor(game);
    if (e == NULL) {
        return RSBS_COMBO_LOGIC_ERR_NO_ENGINE;
    }

    const int total = e->reachedEmptyHosts(e->self, sHostScratch, RSBS_COMBO_LOGIC_PLACEMENT_CAP);
    if (total < 0) {
        return RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED;
    }
    if (total > RSBS_COMBO_LOGIC_PLACEMENT_CAP) {
        fprintf(stderr, "[ComboLogic] %s engine offered %d candidate hosts; the buffer holds %d\n",
                Game_ToString((GameId)game), total, RSBS_COMBO_LOGIC_PLACEMENT_CAP);
        return RSBS_COMBO_LOGIC_ERR_CAPACITY;
    }

    for (int i = 0; i < total; ++i) {
        if (!ComboLogicBitGet(sOccupied[game], (int)sHostScratch[i])) {
            buf->host[buf->count++] = sHostScratch[i];
        }
    }
    return RSBS_COMBO_LOGIC_OK;
}

/**
 * Hand every not-yet-exchanged FOREIGN item hosted in `hostGame`'s reached
 * checks to the engine whose id-space it belongs to.
 *
 * `gateOpen` is the travel the crossing models, computed by the caller:
 *   hostGame == OOT (an MM-origin item found in OoT): OoT's crossing open, so
 *       the player can get to MM and spend it.
 *   hostGame == MM  (an OoT-origin item found in MM): OoT's crossing open to
 *       get there at all AND MM's crossing open to get back.
 *
 * Own-origin placements are deliberately skipped: harvesting an item from a
 * reached check in its OWN game is the engine's own `expand` (that is what
 * OoT's `ReachabilitySearch` already does), and doing it here too would grant
 * items the engine has its own authored reasons not to.
 *
 * @return nonzero if anything was granted (the round must iterate again).
 */
static int ComboLogicExchangeFrom(uint8_t hostGame, int gateOpen, int* exchanged) {
    const ComboLogicEngine* host = ComboLogicEngineFor(hostGame);
    int changed = 0;

    if (host == NULL || !gateOpen) {
        return 0;
    }

    for (int i = 0; i < sPlacementCount[hostGame]; ++i) {
        const ComboLogicPlacement* p = &sPlacements[hostGame][i];
        if (ComboLogicBitGet(sExchanged[hostGame], i)) {
            continue;
        }
        if (p->item.originGame == hostGame || !ComboLogicIsGame(p->item.originGame)) {
            continue;
        }
        if (!host->checkReached(host->self, p->hostCheck)) {
            continue;
        }
        const ComboLogicEngine* owner = ComboLogicEngineFor(p->item.originGame);
        if (owner == NULL) {
            continue;
        }
        owner->assumeOwnItem(owner->self, p->item.id);
        ComboLogicBitSet(sExchanged[hostGame], i);
        if (exchanged != NULL) {
            (*exchanged)++;
        }
        changed = 1;
    }
    return changed;
}

static void ComboLogicResetRoundResult(ComboLogicRoundResult* res) {
    memset(res, 0, sizeof(*res));
    res->goalExpression = -1;
    res->allHostsReached = -1;
}

/**
 * Run one round. On return the engines have been torn down, so every fact the
 * caller needs is in `res` and in sCandidates[].
 */
static int ComboLogicRoundRun(const ComboLogicBagItem* assumed, int assumedCount, uint8_t goal,
                              ComboLogicRoundResult* res) {
    const uint8_t order[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
    bool began[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = { false, false, false };
    bool snapped[RSBS_FOREIGN_POOL_ORIGIN_COUNT] = { false, false, false };
    int status = RSBS_COMBO_LOGIC_OK;
    uint32_t prevObs[4] = { 0u, 0u, 0u, 0u };
    bool haveObs = false;

    ComboLogicResetRoundResult(res);
    sCandidates[(uint8_t)GAME_OOT].count = 0;
    sCandidates[(uint8_t)GAME_MM].count = 0;
    memset(sExchanged, 0, sizeof(sExchanged));

    if (ComboLogicEngineFor((uint8_t)GAME_OOT) == NULL || ComboLogicEngineFor((uint8_t)GAME_MM) == NULL) {
        res->status = RSBS_COMBO_LOGIC_ERR_NO_ENGINE;
        return res->status;
    }

    // --- open the bracket -------------------------------------------------
    // snapshot BEFORE beginQuery on purpose: the restore has to undo whatever
    // beginQuery did to the live save too.
    for (int s = 0; s < 2 && status == RSBS_COMBO_LOGIC_OK; ++s) {
        const uint8_t g = order[s];
        const ComboLogicEngine* e = sEngines[g];
        if (e->snapshot != NULL) {
            if (!e->snapshot(e->self)) {
                fprintf(stderr, "[ComboLogic] %s engine refused snapshot\n", Game_ToString((GameId)g));
                status = RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED;
                break;
            }
            snapped[g] = true;
        }
        if (!e->beginQuery(e->self)) {
            fprintf(stderr, "[ComboLogic] %s engine refused beginQuery\n", Game_ToString((GameId)g));
            status = RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED;
            break;
        }
        began[g] = true;
    }

    // --- seed the assumed set --------------------------------------------
    if (status == RSBS_COMBO_LOGIC_OK) {
        for (int i = 0; i < assumedCount; ++i) {
            const uint8_t og = assumed[i].item.originGame;
            if (!ComboLogicIsGame(og)) {
                status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
                break;
            }
            const ComboLogicEngine* owner = sEngines[og];
            owner->assumeOwnItem(owner->self, assumed[i].item.id);
        }
    }

    // --- the alternating expansion to a fixpoint --------------------------
    //
    // TERMINATION ARGUMENT. Each engine's expansion is a monotone operator on
    // its own finite join-semilattice (reachable regions x reached checks x
    // granted items); the facts exchanged between them — a granted item, a
    // crossing being open — are themselves monotone, because nothing in a round
    // is ever withdrawn (`assumeOwnItem` may only add, `place` grants nothing,
    // and no placement is made inside the bracket). The composite operator is
    // therefore monotone on the product lattice, which is finite, so the
    // iteration reaches a least fixed point in at most (|regions| + |checks| +
    // |items|) steps. That is the argument; the audit expects two or three
    // alternations in practice because the crossing opens at sphere zero.
    //
    // The bound below is NOT that argument. It is a watchdog on the PREMISE: an
    // engine that reports `changed` unconditionally (a non-monotone `expand`, a
    // cache that reports a fresh answer every call) would otherwise spin
    // forever inside world generation, with no diagnosis and no way out. Two
    // detectors, because they catch different lies:
    //   - a monotone observable that DECREASED  -> ERR_NON_MONOTONE, caught the
    //     iteration it happens;
    //   - `changed` forever with nothing growing -> ERR_NO_FIXPOINT at the cap.
    // Neither is worked around: assumed fill and the trailing per-game junk
    // dump are unsound without monotonicity (ADR 0010 §2.3), so a violated
    // premise fails the fill.
    if (status == RSBS_COMBO_LOGIC_OK) {
        const ComboLogicEngine* eo = sEngines[(uint8_t)GAME_OOT];
        const ComboLogicEngine* em = sEngines[(uint8_t)GAME_MM];
        int iteration = 0;
        bool converged = false;

        while (iteration < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS) {
            int changed = 0;

            changed |= (eo->expand(eo->self) != 0) ? 1 : 0;
            const int crossOoT = (eo->crossingOpen(eo->self) != 0) ? 1 : 0;
            changed |= ComboLogicExchangeFrom((uint8_t)GAME_OOT, crossOoT, &res->exchanged);

            changed |= (em->expand(em->self) != 0) ? 1 : 0;
            const int crossMM = (em->crossingOpen(em->self) != 0) ? 1 : 0;
            changed |= ComboLogicExchangeFrom((uint8_t)GAME_MM, crossOoT && crossMM, &res->exchanged);

            const int rawOoT = eo->reachedEmptyHosts(eo->self, NULL, 0);
            const int rawMM = em->reachedEmptyHosts(em->self, NULL, 0);
            const uint32_t obs[4] = { (uint32_t)(rawOoT < 0 ? 0 : rawOoT), (uint32_t)(rawMM < 0 ? 0 : rawMM),
                                      (uint32_t)crossOoT, (uint32_t)crossMM };
            if (haveObs) {
                for (int k = 0; k < 4; ++k) {
                    if (obs[k] < prevObs[k]) {
                        fprintf(stderr,
                                "[ComboLogic] monotonicity violated: observable %d fell from %u to %u inside one "
                                "round\n",
                                k, (unsigned)prevObs[k], (unsigned)obs[k]);
                        status = RSBS_COMBO_LOGIC_ERR_NON_MONOTONE;
                        break;
                    }
                }
            }
            memcpy(prevObs, obs, sizeof(prevObs));
            haveObs = true;
            ++iteration;

            if (status != RSBS_COMBO_LOGIC_OK) {
                break;
            }
            if (!changed) {
                converged = true;
                break;
            }
        }
        res->iterations = iteration;

        if (status == RSBS_COMBO_LOGIC_OK && !converged) {
            fprintf(stderr, "[ComboLogic] no fixpoint after %d alternations; an engine reports change forever\n",
                    RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS);
            status = RSBS_COMBO_LOGIC_ERR_NO_FIXPOINT;
        }
    }

    // --- read the round's facts, before any teardown ----------------------
    if (status == RSBS_COMBO_LOGIC_OK) {
        const ComboLogicEngine* eo = sEngines[(uint8_t)GAME_OOT];
        const ComboLogicEngine* em = sEngines[(uint8_t)GAME_MM];

        res->crossingOpenOoT = (eo->crossingOpen(eo->self) != 0) ? 1 : 0;
        res->crossingOpenMM = (em->crossingOpen(em->self) != 0) ? 1 : 0;
        res->goalOoT = (eo->goalReached(eo->self) != 0) ? 1 : 0;
        // THE ARRIVAL GATE. MM's engine cannot know that Termina is entered
        // through OoT's crossing — its root is unconditional in its own dialect
        // — so the coordinator applies the gate the engine cannot. Without it a
        // world whose crossing never opens would count MM's half as proved.
        res->goalMM = (em->goalReached(em->self) != 0 && res->crossingOpenOoT) ? 1 : 0;
        res->goalExpression = Combo_Logic_EvaluateGoal(goal, res->goalOoT, res->goalMM);

        status = ComboLogicCollectCandidates((uint8_t)GAME_OOT);
        if (status == RSBS_COMBO_LOGIC_OK) {
            status = ComboLogicCollectCandidates((uint8_t)GAME_MM);
        }
        if (status == RSBS_COMBO_LOGIC_OK) {
            // The same arrival gate on hosts: while the crossing is closed, MM's
            // reached checks are not places the player can be.
            if (!res->crossingOpenOoT) {
                sCandidates[(uint8_t)GAME_MM].count = 0;
            }
            res->candidatesOoT = sCandidates[(uint8_t)GAME_OOT].count;
            res->candidatesMM = sCandidates[(uint8_t)GAME_MM].count;

            // Evaluated here because `checkReached` is only valid before
            // teardown — which is why the all-reachable rung cannot be a
            // post-pass over the tables.
            int allReached = 1;
            for (int s = 0; s < 2 && allReached; ++s) {
                const uint8_t g = order[s];
                const ComboLogicEngine* e = sEngines[g];
                const int gated = (g == (uint8_t)GAME_MM) ? res->crossingOpenOoT : 1;
                for (int i = 0; i < sPlacementCount[g]; ++i) {
                    if (!gated || !e->checkReached(e->self, sPlacements[g][i].hostCheck)) {
                        allReached = 0;
                        break;
                    }
                }
            }
            res->allHostsReached = allReached;
        }
    }

    // --- close the bracket, in reverse --------------------------------------
    for (int s = 1; s >= 0; --s) {
        const uint8_t g = order[s];
        const ComboLogicEngine* e = sEngines[g];
        if (snapped[g]) {
            e->restore(e->self);
            // Amendment 1: the restore undid the round's writes, so the
            // coordinator re-applies its whole table for THIS side. `place` is
            // contractually idempotent, which is what makes that safe. A side
            // that did not restore lost nothing and is skipped — the re-apply
            // is one `place` per placement per round and the fill runs a round
            // per bag item.
            for (int i = 0; i < sPlacementCount[g]; ++i) {
                const ComboLogicPlacement* p = &sPlacements[g][i];
                if (!e->place(e->self, p->hostCheck, p->item) && status == RSBS_COMBO_LOGIC_OK) {
                    fprintf(stderr, "[ComboLogic] %s engine refused re-applying its own placement on check %u\n",
                            Game_ToString((GameId)g), (unsigned)p->hostCheck);
                    status = RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED;
                }
            }
        }
        if (began[g]) {
            e->endQuery(e->self);
        }
    }

    res->status = status;
    return status;
}

int Combo_Logic_RunRound(const ComboLogicRoundRequest* req, ComboLogicRoundResult* out) {
    ComboLogicRoundResult local;
    int status;

    if (req == NULL || req->assumedCount < 0 || (req->assumedCount > 0 && req->assumed == NULL)) {
        if (out != NULL) {
            ComboLogicResetRoundResult(out);
            out->status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        }
        return RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
    }

    status = ComboLogicRoundRun(req->assumed, req->assumedCount, req->goal, &local);
    if (out != NULL) {
        *out = local;
    }
    return status;
}

// ============================================================================
// The single-bag assumed fill
// ============================================================================

static ComboLogicBagItem sAssumedBuf[RSBS_COMBO_LOGIC_BAG_CAP];
static int sBagOrder[RSBS_COMBO_LOGIC_BAG_CAP];

static void ComboLogicResetFillResult(ComboLogicFillResult* out) {
    memset(out, 0, sizeof(*out));
    out->status = RSBS_COMBO_LOGIC_OK;
}

static bool ComboLogicRungIsPinned(uint8_t rung) {
    return rung == RSBS_COMBO_RUNG_NONE || rung == RSBS_COMBO_RUNG_BEATABLE || rung == RSBS_COMBO_RUNG_ALL_REACHABLE;
}

int Combo_Logic_RunFill(const ComboLogicFillRequest* req, ComboLogicFillResult* out) {
    ComboLogicFillResult res;
    ComboLogicRoundResult round;
    int maxAttempts;
    int lastFailure = RSBS_COMBO_LOGIC_OK;

    int status = RSBS_COMBO_LOGIC_OK;

    ComboLogicResetFillResult(&res);

    if (req == NULL || req->bagCount < 0 || (req->bagCount > 0 && req->bag == NULL) || req->maxAttempts < 0) {
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }
    if (!ComboLogicRungIsPinned(req->logicRung)) {
        fprintf(stderr, "[ComboLogic] fill refused: logic rung %u is outside the pinned space\n",
                (unsigned)req->logicRung);
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }
    if (req->bagCount > RSBS_COMBO_LOGIC_BAG_CAP) {
        fprintf(stderr, "[ComboLogic] fill refused: bag of %d exceeds the cap of %d\n", req->bagCount,
                RSBS_COMBO_LOGIC_BAG_CAP);
        status = RSBS_COMBO_LOGIC_ERR_CAPACITY;
        goto finish;
    }
    for (int i = 0; i < req->bagCount; ++i) {
        if (!ComboLogicIsGame(req->bag[i].item.originGame)) {
            fprintf(stderr, "[ComboLogic] fill refused: bag entry %d carries no origin game\n", i);
            status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
            goto finish;
        }
    }
    // The goal is validated BEFORE any placement, so an unsupported goal cannot
    // leave a half-filled world behind.
    if (Combo_Logic_EvaluateGoal(req->goal, 1, 1) < 0) {
        fprintf(stderr, "[ComboLogic] fill refused: this build has no evaluator for GOAL %u\n", (unsigned)req->goal);
        status = RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL;
        goto finish;
    }
    if (Combo_Logic_GetEngine(GAME_OOT) == NULL || Combo_Logic_GetEngine(GAME_MM) == NULL) {
        fprintf(stderr, "[ComboLogic] fill refused: a paired fill needs both engines registered\n");
        status = RSBS_COMBO_LOGIC_ERR_NO_ENGINE;
        goto finish;
    }

    maxAttempts = (req->maxAttempts > 0) ? req->maxAttempts : RSBS_COMBO_LOGIC_FILL_RETRIES;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        uint32_t rng = ComboLogicAttemptSeed(req->seed, attempt);
        bool deadEnd = false;

        res.attempts = attempt + 1;
        Combo_Logic_ResetPlacements();

        for (int i = 0; i < req->bagCount; ++i) {
            sBagOrder[i] = i;
        }
        ComboLogicShuffle(sBagOrder, req->bagCount, &rng);

        for (int k = 0; k < req->bagCount; ++k) {
            // The assumed set: every bag item NOT yet placed, EXCLUDING the one
            // being placed. That exclusion is the whole of assumed fill — the
            // item must land somewhere reachable WITHOUT itself, or the world
            // contains a self-justifying cycle.
            int assumedCount = 0;
            for (int j = k + 1; j < req->bagCount; ++j) {
                sAssumedBuf[assumedCount++] = req->bag[sBagOrder[j]];
            }

            const int st = ComboLogicRoundRun(sAssumedBuf, assumedCount, req->goal, &round);
            res.rounds++;
            if (st != RSBS_COMBO_LOGIC_OK) {
                // An engine-contract failure is not a dead end and retrying it
                // would just repeat the violation: report it.
                status = st;
                goto finish;
            }

            const int nOoT = sCandidates[(uint8_t)GAME_OOT].count;
            const int nMM = sCandidates[(uint8_t)GAME_MM].count;
            const int total = nOoT + nMM;
            if (total == 0) {
                // Dead end: roll the whole batch back and re-shuffle, which is
                // OoT's own assumed-fill discipline. The ATTEMPT LADDER (a new
                // seed) is the layer above and is not invoked here.
                deadEnd = true;
                break;
            }

            // ONE uniform draw over the UNION of both sides' candidates. Not
            // "pick a side, then a host": weighting by side would bias the
            // world toward the smaller game, and under `beat-either` a side
            // preference is exactly the XOR bias ADR 0010 §1.2 forbids.
            const uint32_t pick = ComboLogicRngBelow(&rng, (uint32_t)total);
            const uint8_t hostGame =
                (pick < (uint32_t)nOoT) ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
            const int hostIndex = (pick < (uint32_t)nOoT) ? (int)pick : (int)(pick - (uint32_t)nOoT);
            const uint16_t host = sCandidates[hostGame].host[hostIndex];
            const ComboLogicBagItem* item = &req->bag[sBagOrder[k]];

            if (!ComboLogicAddPlacement(hostGame, host, item->item, item->itemClass)) {
                status = RSBS_COMBO_LOGIC_ERR_CAPACITY;
                goto finish;
            }

            const ComboLogicEngine* e = Combo_Logic_GetEngine((GameId)hostGame);
            if (!e->place(e->self, host, item->item)) {
                fprintf(stderr, "[ComboLogic] %s engine refused a placement on check %u it had offered\n",
                        Game_ToString((GameId)hostGame), (unsigned)host);
                status = RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED;
                goto finish;
            }
        }

        if (deadEnd) {
            lastFailure = RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE;
            continue;
        }

        // --- the exit condition ------------------------------------------
        //
        // The guarantee is the fill's exit condition, never a check bolted on
        // after it (ADR 0010 §2.3). The three rungs are this one code path with
        // that condition parametrized — `none` is the operator's "bag, then
        // randomly distribute" base mode, reached by skipping the proof and
        // nothing else.
        if (req->logicRung == RSBS_COMBO_RUNG_NONE) {
            res.proofSkipped = true;
            res.goalProven = false;
            status = RSBS_COMBO_LOGIC_OK;
            goto finish;
        }

        {
            const int st = ComboLogicRoundRun(NULL, 0, req->goal, &round);
            res.rounds++;
            if (st != RSBS_COMBO_LOGIC_OK) {
                status = st;
                goto finish;
            }
        }
        if (round.goalExpression != 1) {
            lastFailure = RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE;
            continue;
        }
        if (req->logicRung == RSBS_COMBO_RUNG_ALL_REACHABLE && round.allHostsReached != 1) {
            lastFailure = RSBS_COMBO_LOGIC_ERR_NOT_ALL_REACHED;
            continue;
        }

        res.goalProven = true;
        res.allHostsReached = (round.allHostsReached == 1);
        status = RSBS_COMBO_LOGIC_OK;
        goto finish;
    }

    status = (lastFailure != RSBS_COMBO_LOGIC_OK) ? lastFailure : RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE;

finish:
    res.status = status;
    res.placed = Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM);
    res.placementDigest = Combo_Logic_PlacementDigest();
    if (out != NULL) {
        *out = res;
    }
    return status;
}
