/**
 * @file test_combo_logic_monotonicity.c
 * @brief ADR 0010 answer O6's CI GROW-CHECK: over BOTH real engines, grant the
 *        bag one copy at a time in several deterministic orders and assert the
 *        reached check set and the reached region set NEVER SHRINK (#645, #500).
 *
 * ============================================================================
 * WHY
 * ============================================================================
 *
 * ADR 0010 §2.3: the combo fill's guarantee is the least fixed point of a
 * MONOTONE operator. Assumed (reverse) fill "pretends the player has" every
 * unplaced item, and the trailing no-logic dump places the rest anywhere; both
 * are sound only if gaining an item can never take reachability away. OoTMM
 * enforces it by construction (its builder throws on `!` over has()/event());
 * redship's conditions are C++ lambdas, so answer O6 accepts ALL THREE of: a
 * review rule, a static probe over the condition sources
 * (.github/scripts/check-monotonicity-negations.py), and THIS row — the one of
 * the three that measures the operator instead of reading its source, and so
 * the one that sees what text cannot: helper bodies (OoT's logic.cpp, MM's
 * Logic.h), give-path arithmetic (the wallet wrap the multiplicity clamp exists
 * for), event counters, MM's time join, and anything a macro hides.
 *
 * ============================================================================
 * WHAT IT DOES, PER ENGINE, PER TRICK SET (off, then every trick on)
 * ============================================================================
 *
 *   G1. THE WALK. One round through the coordinator's own vtable
 *       (`Combo_Logic_GetEngine`): open it from the shipped profile's starting
 *       state, expand, then for every row of the bag in a given ORDER grant ONE
 *       copy (`assumeOwnItem`) and expand, reading after each grant the full
 *       reached CHECK set (`checkReached` over the check id space) and the full
 *       reached REGION set (OoT: every region's four age/time bits; MM: every
 *       region's joined time slices). A single lost check, region, age/time bit
 *       or time slice is a failure, reported with the step and the item granted.
 *       Four orders: forward, reverse, and two seeded shuffles.
 *   G2. ORDER-INDEPENDENCE. The four walks' final closures are identical, and
 *       equal to a one-shot round that assumes the whole bag and expands to a
 *       fixpoint. (The contract's "a function of the MULTISET of copies".)
 *   G3. MONOTONE IN TRICKS (ADR 0010 §3.2). Along the forward walk, every step's
 *       tricks-off closure is contained in the same step's tricks-on closure; and
 *       the tricks-on set is not vacuous on EITHER engine (the forcing seam
 *       turned keys on, and at least one step's closure is strictly larger — so
 *       the engine READ the forced bits, not merely received them).
 *   G4. THE COORDINATOR. `Combo_Logic_RunRound` over 257 growing PREFIXES of the
 *       union bag (both halves, forward and reverse): every round returns OK, and
 *       neither side's candidate HOST SET (a set, not a count: a count cannot see
 *       a host lost in the step another is gained), crossing flag nor goal flag
 *       ever drops between consecutive sampled prefixes. This is the operator as
 *       the fill sees it — both engines, the arrival gate, the exchange — not
 *       each engine alone. Sampled, not per-grant: G1 is the per-grant check.
 *
 * THE BAG (the operator's multiplicity ruling, 2026-09-26: "keep in mind that
 * there are usually options here. like plentiful drops is an option ... ice traps
 * are a thing too"). Per game, one row per COPY:
 *   - REQUIRED: every progression-class copy of the game's pool (OoT: the
 *     generated world's placed items over every owned host; MM: its graph's
 *     vanilla pool, `MM_ComboLogic_PoolVanillaItems`);
 *   - SURPLUS: one extra copy of every distinct progression id — the plentiful
 *     shape, which is exactly where a give path past its top tier or counter
 *     maximum could LOWER something (the wallet wrap, the s8 key sentinel);
 *   - FILLER: every junk/renewable copy of the pool, and every TRAP-class copy
 *     the world holds (OoT's ice traps, which the default profile places). MM's
 *     `RI_TRAP` is NOT granted and is not in MM's vanilla pool anyway (traps enter
 *     it only under `RO_SHUFFLE_TRAPS`, off by default): its give reaches
 *     `OfferTrapItem()` outside the save, which no snapshot undoes — the same
 *     exclusion `MM_ComboLogic_PoolVanillaItems` makes, for the same reason. Traps
 *     are per-game filler this increment; none crosses.
 * Classes come from the O8 owner (`Combo_ItemClassOf`), never from an enum here.
 * A pool copy with NO fill class (an event item such as OoT's Triforce) is not a
 * bag row: its host is left holding it, so the GOAL a round reads stays the
 * world's own.
 *
 * THE RED HALVES (O6's "observe the row fail") — one per lock, each run through
 * the SAME comparison the green half asserts with, on both engines. A bad edge
 * is planted at runtime into each real graph (never into a region file; the two
 * game-side probe TUs install it on the single inbound edge of a region reached
 * at the start, and restore the original after):
 *   G1  NOT_HELD "passable only while the player does NOT hold X": the walk must
 *       go red AT EXACTLY THE GRANT OF X, on both the region and the check
 *       observable, and MM's own shrink counter must see it. Disarmed, the walk
 *       is green and identical to the unplanted one again.
 *   G2  ORDER_LATCH "passable once Y was held while X was not, and from then on":
 *       history-dependent, so it never shrinks along a walk (G1 must stay GREEN
 *       on it) yet the forward walk (Y before X) keeps the region and the reverse
 *       walk and the one-shot round do not — G2's comparison must disagree.
 *   G3  TRICK_OFF "passable only while trick K is OFF": G3's containment must
 *       fail at step 0, on the planted region.
 *   G4  NOT_HELD again, through the coordinator: the prefix pair straddling X's
 *       grant in the union order must show a lost host on X's side, while the
 *       same pair unplanted shows none.
 *
 * WHAT THE ROW LEAVES BEHIND: OoT's world digest, OoT's trick options, every
 * emptied host, MM's frozen trick bits, MM's snapshot not live, no coordinator
 * placement, and the unified save buffer byte-identical to the state MM's shipped
 * profile left it in — compared BEFORE the outer restore (combo-logic-measure's S4
 * lesson: after it the comparison cannot fail). On a FAILING run the restore
 * still happens: every step from the first host emptied on runs inside one
 * lambda, so an assert anywhere returns into the restore (hosts, tricks, planted
 * edges, the save buffer), never past it.
 *
 * ============================================================================
 * WHAT IT DOES NOT CLAIM
 * ============================================================================
 *
 *  - ONE PROFILE PER GAME (the shipped default) with tricks all-off and all-on.
 *    Other settings change the operator; they are not walked.
 *  - FOUR ORDERS, not all orders. A non-monotone edge that fires only for an item
 *    combination no walked prefix produces is not seen. G4 samples 257 prefixes of
 *    each union order; a coordinator-only loss that appears and recovers between
 *    two samples is not seen (a persisting one is: the host sets are compared).
 *  - The red halves prove each comparison CAN fail on a planted edge of the
 *    shape it targets. They do not prove the real graphs contain no subtler
 *    shape; G2's planted latch models an engine carrying state across grants. The per-step check makes
 *    each walk sensitive to every prefix it passes through (546 OoT and 2282 MM
 *    prefixes per walk on the shipped profile, measured 2026-09-26), which is the
 *    reason the walk grants one copy at a time. The whole row ran in ~45 s on the
 *    workstation that measured it; it asserts no timing.
 *  - ONE EXPAND PER GRANT, with the fixpoint confirmed at the end of each walk.
 *    Both engines' `expand` computes a full closure per call (OoT re-runs
 *    `ReachabilitySearch` from scratch; MM crawls and harvests to closure), so a
 *    second call per step would only confirm; the final confirmation asserts it.
 *  - OoT's fill-item hosts are EMPTIED for the walk (and restored after), so the
 *    search does not harvest the generated world's own items and the bag is the
 *    only source. MM's engine harvests only coordinator placements, of which there
 *    are none.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE and compiled as C++,
 * like every file in this directory — so everything here is in its own namespace.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../game.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
// OoT engine (games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp).
int OoT_ComboLogic_TestCheckIdSpace(void);
int OoT_ComboLogic_TestItemIdSpace(void);
int OoT_ComboLogic_TestOwnedHostCount(void);
int OoT_ComboLogic_TestOwnedHosts(uint16_t* out, int cap);
int OoT_ComboLogic_TestPlacedItemAt(uint16_t rc, uint16_t* outItemId, int* outAdvancement);
int OoT_ComboLogic_TestSetPlacedItem(uint16_t rc, uint16_t itemId);
uint32_t OoT_ComboLogic_TestWorldDigest(void);
int Rando_HeadlessSeedTest(const char* seedStr);
// OoT probe (games/oot/soh/Enhancements/randomizer/ComboLogicMonotonicityOoT.cpp).
int OoT_ComboMono_RegionBits(uint8_t* out, int cap);
int OoT_ComboMono_ForceAllTricks(int on);
int OoT_ComboMono_NegatableItem(uint16_t itemId);
int OoT_ComboMono_ArmRegionNegation(const uint16_t* startRegions, int count, uint16_t negItem, uint16_t* outParent);
int OoT_ComboMono_ArmEdge(int mode, const uint16_t* startRegions, int count, uint16_t negItem, uint16_t latchItem,
                          uint16_t* outParent);
int OoT_ComboMono_Disarm(void);

// MM engine (games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp) + bring-up.
void MM_Rando_InitCore(void);
int MM_ComboLogic_ApplyShippedProfile(void);
int MM_ComboLogic_PoolVanillaItems(uint16_t* outItems, uint16_t* outHosts, int cap);
int MM_ComboLogic_ShrinkObservations(void);
int MM_ComboLogic_SnapshotLive(void);
int MM_ComboLogic_HeldPlacementCount(void);
void MM_ComboLogic_ResetCounters(void);
int MM_ComboLogic_TestRoundRegions(uint16_t* outRegions, uint64_t* outTimeSlices, int cap);
// MM probe (games/mm/2s2h/Rando/ComboLogicMonotonicitySingleExe.cpp).
int MM_ComboMono_ForceAllTricks(int on);
int MM_ComboMono_NegatableItem(uint16_t riId);
int MM_ComboMono_ArmRegionNegation(const uint16_t* startRegions, int count, uint16_t negRi, uint16_t* outSource);
int MM_ComboMono_ArmEdge(int mode, const uint16_t* startRegions, int count, uint16_t negRi, uint16_t latchRi,
                         uint16_t* outSource);
int MM_ComboMono_Disarm(void);

extern char gSaveContext[];
}

#define CLMONO_ASSERT(cond, msg)                                            \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace clmono {

/** The MM region id space the closure vector covers. MM's RandoRegionId is far
 *  below this (~316 regions); the read asserts every id fits rather than
 *  truncating. */
constexpr int kMmRegionSpace = 1024;
/** The MM check id space walked through `checkReached`: MM's RandoCheckId ends
 *  near 2258, and the coordinator's host cap (4096) is sized to hold either
 *  game's id space, so it bounds this one too. */
constexpr int kMmCheckSpace = RSBS_COMBO_LOGIC_HOST_CAP;
/** A walk's per-grant bound on `expand` calls when confirming a fixpoint. */
constexpr int kExpandBound = 64;
/** The planted edge shapes (the probe TUs' `mode`). */
constexpr int kEdgeNotHeld = 0;
constexpr int kEdgeOrderLatch = 1;
constexpr int kEdgeTrickOff = 2;
/** G4's sample count per union order (257 prefixes: 0..n in 256 steps, ~11 rows apart). */
constexpr int kPrefixPoints = 256;
/** G3's red half only needs step 0; a short walk keeps it cheap. */
constexpr size_t kTrickRedRows = 32;

using ArmEdgeFn = int (*)(int, const uint16_t*, int, uint16_t, uint16_t, uint16_t*);

double NowMs() {
    using namespace std::chrono;
    return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() / 1000.0;
}

/** One closure: a byte per check id (reached or not) and a word per region id —
 *  OoT: the four age/time bits; MM: the joined time slices with bit 63 marking
 *  "reached" (MM's slice mask uses 46 bits). */
struct Closure {
    std::vector<uint8_t> checks;
    std::vector<uint64_t> regions;
    int checkCount = 0;
    int regionCount = 0; // regions with any bit
    int regionBits = 0;  // total bits (age/time or time slices)
};

/** The first CHECK in `sub` that `sup` lacks, or -1. */
int FirstLostCheck(const Closure& sup, const Closure& sub) {
    for (size_t i = 0; i < sub.checks.size(); ++i) {
        if (sub.checks[i] != 0 && (i >= sup.checks.size() || sup.checks[i] == 0)) {
            return (int)i;
        }
    }
    return -1;
}

/** The first REGION in `sub` with a bit (age/time, time slice, or presence)
 *  that `sup` lacks, or -1; `outLost` receives the lost bits. */
int FirstLostRegion(const Closure& sup, const Closure& sub, uint64_t* outLost) {
    for (size_t i = 0; i < sub.regions.size(); ++i) {
        const uint64_t have = (i < sup.regions.size()) ? sup.regions[i] : 0;
        const uint64_t lost = sub.regions[i] & ~have;
        if (lost != 0) {
            *outLost = lost;
            return (int)i;
        }
    }
    return -1;
}

/** Is `sub` contained in `sup` on BOTH observables? On failure, names the
 *  first loss (a check first, then a region). */
bool Contains(const Closure& sup, const Closure& sub, int* outKind, int* outId, uint64_t* outLost) {
    const int check = FirstLostCheck(sup, sub);
    if (check >= 0) {
        *outKind = 0;
        *outId = check;
        *outLost = 1;
        return false;
    }
    const int region = FirstLostRegion(sup, sub, outLost);
    if (region >= 0) {
        *outKind = 1;
        *outId = region;
        return false;
    }
    return true;
}

bool Equal(const Closure& a, const Closure& b) {
    return a.checks == b.checks && a.regions == b.regions;
}

int PopCount64(uint64_t v) {
    int n = 0;
    while (v != 0) {
        v &= v - 1;
        ++n;
    }
    return n;
}

/** One engine, as the walk sees it. */
struct Side {
    const char* name;
    GameId game;
    const ComboLogicEngine* e;
    int checkSpace;
    bool snapshotted; // this round's snapshot is live
};

bool ReadClosure(Side& s, Closure* out) {
    out->checks.assign((size_t)s.checkSpace, 0);
    out->checkCount = 0;
    for (int id = 0; id < s.checkSpace; ++id) {
        if (s.e->checkReached(s.e->self, (uint16_t)id) != 0) {
            out->checks[(size_t)id] = 1;
            out->checkCount++;
        }
    }
    out->regionCount = 0;
    out->regionBits = 0;
    if (s.game == GAME_OOT) {
        const int n = OoT_ComboMono_RegionBits(nullptr, 0);
        if (n <= 0) {
            return false;
        }
        std::vector<uint8_t> bits((size_t)n, 0);
        OoT_ComboMono_RegionBits(bits.data(), n);
        out->regions.assign((size_t)n, 0);
        for (int i = 0; i < n; ++i) {
            out->regions[(size_t)i] = bits[(size_t)i];
            if (bits[(size_t)i] != 0) {
                out->regionCount++;
                out->regionBits += PopCount64(bits[(size_t)i]);
            }
        }
        return true;
    }
    const int n = MM_ComboLogic_TestRoundRegions(nullptr, nullptr, 0);
    std::vector<uint16_t> ids((size_t)(n > 0 ? n : 1), 0);
    std::vector<uint64_t> slices((size_t)(n > 0 ? n : 1), 0);
    MM_ComboLogic_TestRoundRegions(ids.data(), slices.data(), n);
    out->regions.assign((size_t)kMmRegionSpace, 0);
    for (int i = 0; i < n; ++i) {
        if ((int)ids[(size_t)i] >= kMmRegionSpace) {
            return false;
        }
        out->regions[ids[(size_t)i]] = (1ull << 63) | slices[(size_t)i];
        out->regionCount++;
        out->regionBits += PopCount64(slices[(size_t)i]);
    }
    return true;
}

bool OpenRound(Side& s) {
    s.snapshotted = false;
    if (s.e->snapshot != nullptr) {
        if (s.e->snapshot(s.e->self) == 0) {
            return false;
        }
        s.snapshotted = true;
    }
    if (s.e->beginQuery(s.e->self) == 0) {
        if (s.snapshotted) {
            s.e->restore(s.e->self);
        }
        s.e->endQuery(s.e->self);
        return false;
    }
    return true;
}

void CloseRound(Side& s) {
    if (s.snapshotted) {
        s.e->restore(s.e->self);
        s.snapshotted = false;
    }
    s.e->endQuery(s.e->self);
}

/** Expand until the engine reports nothing new; the number of calls, or -1 if
 *  the bound was hit (an engine reporting change forever). */
int ExpandToFixpoint(Side& s) {
    for (int i = 1; i <= kExpandBound; ++i) {
        if (s.e->expand(s.e->self) == 0) {
            return i;
        }
    }
    return -1;
}

struct Walk {
    bool opened = false;
    bool fixpointConfirmed = false;
    int steps = 0;
    // The first loss, if any.
    bool lost = false;
    int lossStep = -1; // index into the order of the grant after which it was seen
    int lossKind = -1; // 0 check, 1 region
    int lossId = -1;
    uint64_t lossBits = 0;
    uint16_t lossItem = 0;
    int losses = 0; // how many steps lost something
    // Per observable, so the red half can require that BOTH saw it.
    int firstCheckLossStep = -1;
    int firstCheckLost = -1;
    int firstRegionLossStep = -1;
    int firstRegionLost = -1;
    Closure start;
    Closure final_;
    std::vector<Closure> trace; // every step's closure, when requested
    double wallMs = 0.0;
};

Walk RunWalk(Side& s, const std::vector<uint16_t>& order, bool keepTrace) {
    Walk w;
    const double t0 = NowMs();
    if (!OpenRound(s)) {
        return w;
    }
    w.opened = true;
    (void)ExpandToFixpoint(s);
    ReadClosure(s, &w.start);
    Closure prev = w.start;
    if (keepTrace) {
        w.trace.reserve(order.size() + 1);
        w.trace.push_back(prev);
    }
    for (size_t i = 0; i < order.size(); ++i) {
        s.e->assumeOwnItem(s.e->self, order[i]);
        (void)s.e->expand(s.e->self);
        Closure cur;
        ReadClosure(s, &cur);
        int kind = -1;
        int id = -1;
        uint64_t bits = 0;
        if (w.firstCheckLossStep < 0) {
            const int c = FirstLostCheck(cur, prev);
            if (c >= 0) {
                w.firstCheckLossStep = (int)i;
                w.firstCheckLost = c;
            }
        }
        if (w.firstRegionLossStep < 0) {
            uint64_t lostBits = 0;
            const int rg = FirstLostRegion(cur, prev, &lostBits);
            if (rg >= 0) {
                w.firstRegionLossStep = (int)i;
                w.firstRegionLost = rg;
            }
        }
        if (!Contains(cur, prev, &kind, &id, &bits)) {
            w.losses++;
            if (!w.lost) {
                w.lost = true;
                w.lossStep = (int)i;
                w.lossKind = kind;
                w.lossId = id;
                w.lossBits = bits;
                w.lossItem = order[i];
            }
        }
        if (keepTrace) {
            w.trace.push_back(cur);
        }
        prev = cur;
        w.steps++;
    }
    // The fixpoint, confirmed rather than assumed: one expand per grant is a full
    // closure for both engines, so the confirming call must report nothing new.
    w.fixpointConfirmed = (ExpandToFixpoint(s) == 1);
    ReadClosure(s, &w.final_);
    CloseRound(s);
    w.wallMs = NowMs() - t0;
    return w;
}

/** The whole bag assumed at once, expanded to a fixpoint: G2's reference. */
bool OneShot(Side& s, const std::vector<uint16_t>& bag, Closure* out) {
    if (!OpenRound(s)) {
        return false;
    }
    for (const uint16_t id : bag) {
        s.e->assumeOwnItem(s.e->self, id);
    }
    const int n = ExpandToFixpoint(s);
    ReadClosure(s, out);
    CloseRound(s);
    return n > 0;
}

uint32_t XorShift(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

std::vector<uint16_t> Shuffled(const std::vector<uint16_t>& src, uint32_t seed) {
    std::vector<uint16_t> out = src;
    uint32_t state = seed;
    for (size_t i = out.size(); i > 1; --i) {
        const size_t j = (size_t)(XorShift(&state) % (uint32_t)i);
        const uint16_t t = out[i - 1];
        out[i - 1] = out[j];
        out[j] = t;
    }
    return out;
}

struct Bag {
    std::vector<uint16_t> rows; // one per COPY, in bag order: required, surplus, filler
    int required = 0;
    int surplus = 0;
    int filler = 0;
    int traps = 0;
    int trapsSkipped = 0;
    int unclassified = 0; // pool copies with no fill class: not bag rows
};

/** Build one game's bag from its pool (one entry per pool copy, in pool order). */
Bag BuildBag(GameId game, const std::vector<uint16_t>& pool) {
    Bag bag;
    std::vector<uint16_t> progression;
    std::vector<uint16_t> junk;
    std::vector<uint16_t> traps;
    for (const uint16_t id : pool) {
        SharedItem item;
        memset(&item, 0, sizeof(item));
        item.originGame = (uint8_t)game;
        item.id = id;
        const uint8_t cls = Combo_ItemClassOf(item);
        if (cls == RSBS_FILL_CLASS_PROGRESSION) {
            progression.push_back(id);
        } else if (cls == RSBS_FILL_CLASS_TRAP) {
            traps.push_back(id);
        } else if (cls == RSBS_FILL_CLASS_JUNK || cls == RSBS_FILL_CLASS_RENEWABLE) {
            junk.push_back(id);
        } else {
            bag.unclassified++;
        }
    }
    // REQUIRED: every progression copy.
    bag.rows = progression;
    bag.required = (int)progression.size();
    // SURPLUS: one extra copy of every distinct progression id, in first-seen order.
    std::vector<bool> seen(1u << 16, false);
    for (const uint16_t id : progression) {
        if (!seen[id]) {
            seen[id] = true;
            bag.rows.push_back(id);
            bag.surplus++;
        }
    }
    // FILLER: every junk/renewable copy of the pool — every filler copy is a give
    // the walk must survive. (The walk is host-agnostic: "more checks than
    // items" is a fill-side shape, the coordinator's leftover hosts, and is not
    // modelled here.)...
    for (const uint16_t id : junk) {
        bag.rows.push_back(id);
        bag.filler++;
    }
    // ...and every trap copy, except MM's, whose give leaves the save.
    for (const uint16_t id : traps) {
        if (game == GAME_MM) {
            bag.trapsSkipped++;
            continue;
        }
        bag.rows.push_back(id);
        bag.traps++;
    }
    return bag;
}

void PrintLoss(const char* label, const Walk& w) {
    printf("[TEST] combo-logic-monotonicity: %s LOST %s %d (bits 0x%llX) after granting item %u at step %d "
           "(%d step(s) lost something)\n",
           label, w.lossKind == 0 ? "check" : "region", w.lossId, (unsigned long long)w.lossBits,
           (unsigned)w.lossItem, w.lossStep, w.losses);
}

/** The ids of the regions reached in `c` (any bit), for the red-half arming. */
std::vector<uint16_t> ReachedRegionIds(const Closure& c) {
    std::vector<uint16_t> ids;
    for (size_t i = 0; i < c.regions.size(); ++i) {
        if (c.regions[i] != 0) {
            ids.push_back((uint16_t)i);
        }
    }
    return ids;
}

/** G2's comparison, shared by the green and the red half: 0 when every order's
 *  final closure equals the first's and the one-shot's; otherwise the index of
 *  the first disagreeing order (1..n-1), or n for a one-shot mismatch. */
int G2Disagreement(const Closure* finals, int n, const Closure& oneShot) {
    for (int o = 1; o < n; ++o) {
        if (!Equal(finals[o], finals[0])) {
            return o;
        }
    }
    return Equal(oneShot, finals[0]) ? 0 : n;
}

/** G3's comparison, shared by the green and the red half: the first step at
 *  which `off`'s closure is NOT contained in `on`'s (or -1), and how many steps
 *  `on` is strictly larger at. */
int G3FirstLoss(const Walk& on, const Walk& off, int* outStrictlyLarger, int* outKind, int* outId, uint64_t* outBits) {
    int first = -1;
    int strictly = 0;
    for (size_t k = 0; k < on.trace.size() && k < off.trace.size(); ++k) {
        int kind = -1;
        int id = -1;
        uint64_t bits = 0;
        if (first < 0 && !Contains(on.trace[k], off.trace[k], &kind, &id, &bits)) {
            first = (int)k;
            *outKind = kind;
            *outId = id;
            *outBits = bits;
        }
        if (!Equal(on.trace[k], off.trace[k])) {
            strictly++;
        }
    }
    *outStrictlyLarger = strictly;
    return first;
}

int FirstIndexOf(const std::vector<uint16_t>& v, uint16_t id) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == id) {
            return (int)i;
        }
    }
    return -1;
}

/** What the G1 red half chose, reused by G4's red half through the coordinator. */
struct RedPlan {
    uint16_t negItem = 0;
    int firstGrant = -1;
    std::vector<uint16_t> startIds;
};

/**
 * G1-G3 for one engine: four walks (and a one-shot) under tricks off, the same
 * under tricks on, the trick containment along the forward walk, and the red
 * halves of G1, G2 and G3. Returns TEST_PASS / TEST_FAIL.
 */
TestResult WalkEngine(Side& s, const Bag& bag, int (*forceTricks)(int), int (*negatable)(uint16_t), ArmEdgeFn armEdge,
                      int (*disarm)(void), RedPlan* plan) {
    const std::vector<uint16_t>& fwd = bag.rows;
    std::vector<uint16_t> rev(fwd.rbegin(), fwd.rend());
    const std::vector<uint16_t> sh1 = Shuffled(fwd, 0x9E3779B9u);
    const std::vector<uint16_t> sh2 = Shuffled(fwd, 0x85EBCA6Bu);
    const std::vector<uint16_t>* orders[4] = { &fwd, &rev, &sh1, &sh2 };
    const char* orderName[4] = { "forward", "reverse", "shuffle-A", "shuffle-B" };

    Walk forwardOff;
    for (int tricks = 0; tricks < 2; ++tricks) {
        const char* trickName = tricks ? "tricks-ON" : "tricks-off";
        if (tricks) {
            const int turnedOn = forceTricks(1);
            printf("[TEST] combo-logic-monotonicity: %s: forcing every trick on turned %d key(s) on\n", s.name,
                   turnedOn);
            CLMONO_ASSERT(turnedOn > 0, "the tricks-on seam turned no key on - the tricks-on walks would be the "
                                        "tricks-off walks again");
        }
        Closure finals[4];
        for (int o = 0; o < 4; ++o) {
            const bool keep = (o == 0);
            Walk w = RunWalk(s, *orders[o], keep);
            CLMONO_ASSERT(w.opened, "the engine refused to open the walk's round");
            printf("[TEST] combo-logic-monotonicity: %s %s %-9s: %d grants in %.0fms; start %d checks / %d regions "
                   "(%d bits) -> final %d checks / %d regions (%d bits); fixpoint confirmed=%d\n",
                   s.name, trickName, orderName[o], w.steps, w.wallMs, w.start.checkCount, w.start.regionCount,
                   w.start.regionBits, w.final_.checkCount, w.final_.regionCount, w.final_.regionBits,
                   w.fixpointConfirmed ? 1 : 0);
            if (w.lost) {
                PrintLoss(s.name, w);
            }
            CLMONO_ASSERT(!w.lost, "G1: the reached check or region set SHRANK after a grant - reachability is not "
                                   "monotone in items (ADR 0010 section 2.3); the line above names the step");
            CLMONO_ASSERT(w.fixpointConfirmed, "a walk's final expand still reported change - one expand per grant "
                                               "was not a full closure, so the per-step reads were partial");
            CLMONO_ASSERT(w.steps == (int)orders[o]->size(), "the walk did not grant every row");
            CLMONO_ASSERT(w.final_.checkCount > w.start.checkCount && w.final_.regionBits > w.start.regionBits,
                          "the bag grew nothing - the walk would be vacuous (hosts not emptied, or the bag holds "
                          "nothing the graph asks for)");
            finals[o] = w.final_;
            if (o == 0 && tricks == 0) {
                forwardOff = w;
                // DIAGNOSTIC, not asserted: a FILLER or TRAP copy (forward order
                // puts them after every progression and surplus row) that still
                // GREW the closure is an item the O8 owner files as filler but a
                // logic term reads. The fill places filler with no logic at all, so
                // each one printed here is worth a reading.
                const int fillerFrom = bag.required + bag.surplus;
                int grewOnFiller = 0;
                for (int k = fillerFrom; k + 1 < (int)w.trace.size(); ++k) {
                    const Closure& before = w.trace[(size_t)k];
                    const Closure& after = w.trace[(size_t)k + 1];
                    if (after.checkCount != before.checkCount || after.regionBits != before.regionBits) {
                        SharedItem si;
                        memset(&si, 0, sizeof(si));
                        si.originGame = (uint8_t)s.game;
                        si.id = fwd[(size_t)k];
                        printf("[TEST] combo-logic-monotonicity: %s FILLER GREW THE CLOSURE: item %u (class %s) at step "
                               "%d: +%d checks, +%d region bits\n",
                               s.name, (unsigned)si.id, Combo_ItemClassName(Combo_ItemClassOf(si)), k,
                               after.checkCount - before.checkCount, after.regionBits - before.regionBits);
                        grewOnFiller++;
                    }
                }
                printf("[TEST] combo-logic-monotonicity: %s: %d filler/trap grant(s) grew the closure\n", s.name,
                       grewOnFiller);
            }
            if (o == 0 && tricks == 1) {
                // G3: step-wise containment, tricks-off within tricks-on.
                int strictlyLarger = 0;
                int kind = -1;
                int id = -1;
                uint64_t bits = 0;
                const int lossStep = G3FirstLoss(w, forwardOff, &strictlyLarger, &kind, &id, &bits);
                if (lossStep >= 0) {
                    printf("[TEST] combo-logic-monotonicity: %s step %d: the tricks-off closure has %s %d "
                           "(bits 0x%llX) that the tricks-on closure lacks\n",
                           s.name, lossStep, kind == 0 ? "check" : "region", id, (unsigned long long)bits);
                }
                CLMONO_ASSERT(lossStep < 0, "G3: turning tricks on LOST reachability - tricks are not a monotone "
                                            "parameter of the operator (ADR 0010 section 3.2)");
                printf("[TEST] combo-logic-monotonicity: %s G3: tricks-off contained in tricks-ON at every one of "
                       "%d forward steps; strictly larger at %d of them\n",
                       s.name, (int)w.trace.size(), strictlyLarger);
                // Both engines: the forced bits were not merely WRITTEN (turnedOn
                // above) but READ — some step's closure is strictly larger.
                CLMONO_ASSERT(strictlyLarger > 0, "G3: every trick on changed no step's closure - the engine does "
                                                  "not read the forced trick set, so the tricks-on walk is not "
                                                  "measuring a different operator");
            }
        }
        // G2: order-independence, and agreement with the one-shot round.
        Closure oneShot;
        CLMONO_ASSERT(OneShot(s, fwd, &oneShot), "the one-shot round did not open or reach a fixpoint");
        const int g2 = G2Disagreement(finals, 4, oneShot);
        CLMONO_ASSERT(g2 == 0 || g2 == 4,
                      "G2: two grant orders of the same multiset closed on DIFFERENT sets - the round is not a "
                      "function of the multiset");
        CLMONO_ASSERT(g2 == 0,
                      "G2: granting one copy at a time closed on a different set than assuming the whole bag at once");
        printf("[TEST] combo-logic-monotonicity: %s %s G2: all four orders and the one-shot round close on the same "
               "%d checks / %d regions\n",
               s.name, trickName, oneShot.checkCount, oneShot.regionCount);
        if (tricks) {
            forceTricks(0);
        }
    }

    // ------------------------------------------------------------------
    // THE RED HALF: one planted negated edge must turn the same walk red at
    // exactly the grant of the negated item.
    // ------------------------------------------------------------------
    int negIndex = -1;
    for (size_t i = fwd.size() / 2; i < fwd.size() && negIndex < 0; ++i) {
        if (negatable(fwd[i]) == 1) {
            negIndex = (int)i;
        }
    }
    for (size_t i = 1; i < fwd.size() / 2 && negIndex < 0; ++i) {
        if (negatable(fwd[i]) == 1) {
            negIndex = (int)i;
        }
    }
    CLMONO_ASSERT(negIndex > 0, "no bag row can key the planted negation - the red half cannot run");
    const uint16_t negItem = fwd[(size_t)negIndex];
    int firstGrant = -1;
    for (size_t i = 0; i < fwd.size(); ++i) {
        if (fwd[i] == negItem) {
            firstGrant = (int)i;
            break;
        }
    }
    const std::vector<uint16_t> startIds = ReachedRegionIds(forwardOff.start);
    uint16_t parent = 0;
    const int target = armEdge(kEdgeNotHeld, startIds.data(), (int)startIds.size(), negItem, 0, &parent);
    CLMONO_ASSERT(target >= 0, "the probe found no region to plant the negated edge on");
    const int shrinkBefore = (s.game == GAME_MM) ? MM_ComboLogic_ShrinkObservations() : 0;
    Walk red = RunWalk(s, fwd, true);
    const int shrinkAfter = (s.game == GAME_MM) ? MM_ComboLogic_ShrinkObservations() : 0;
    CLMONO_ASSERT(disarm() == 1, "the planted edge was not armed when the red walk ended");
    printf("[TEST] combo-logic-monotonicity: %s RED HALF: planted `NOT holding item %u` on the one edge %d -> %d; the "
           "item is first granted at step %d\n",
           s.name, (unsigned)negItem, (int)parent, target, firstGrant);
    if (red.lost) {
        PrintLoss(s.name, red);
    }
    CLMONO_ASSERT(red.opened && red.lost, "RED HALF: the planted negation did NOT turn the walk red - the grow-check "
                                          "is vacuous");
    CLMONO_ASSERT(red.lossStep == firstGrant,
                  "RED HALF: the walk went red, but not at the grant of the negated item");
    // BOTH observables must see it, at that grant: the planted target region is
    // the first region lost, and a check is lost too (the probe prefers a target
    // that owns a check, so a region-only red would mean the check read is blind).
    printf("[TEST] combo-logic-monotonicity: %s RED HALF: first region lost %d at step %d; first check lost %d at "
           "step %d\n",
           s.name, red.firstRegionLost, red.firstRegionLossStep, red.firstCheckLost, red.firstCheckLossStep);
    CLMONO_ASSERT(red.firstRegionLossStep == firstGrant,
                  "RED HALF: the REGION observable did not go red at the negated item's grant");
    // trace[k] is the closure after k grants: the target is held just before the
    // negated item's grant and gone just after it.
    CLMONO_ASSERT(red.trace.size() > (size_t)firstGrant + 1 &&
                      red.trace[(size_t)firstGrant].regions[(size_t)target] != 0 &&
                      red.trace[(size_t)firstGrant + 1].regions[(size_t)target] == 0,
                  "RED HALF: the planted target region was not the one lost at the negated item's grant");
    CLMONO_ASSERT(red.firstCheckLossStep == firstGrant,
                  "RED HALF: the CHECK observable did not go red at the negated item's grant");
    if (s.game == GAME_MM) {
        CLMONO_ASSERT(shrinkAfter > shrinkBefore,
                      "RED HALF: MM's own in-round shrink counter did not see the planted negation");
    }
    // Disarmed, the walk is green again and identical to the unplanted one.
    Walk again = RunWalk(s, fwd, false);
    CLMONO_ASSERT(again.opened && !again.lost, "after disarming, the walk is still red - the edge was not restored");
    CLMONO_ASSERT(Equal(again.final_, forwardOff.final_) && Equal(again.start, forwardOff.start),
                  "after disarming, the walk differs from the unplanted walk - the planted edge was not restored "
                  "exactly");
    printf("[TEST] combo-logic-monotonicity: %s RED HALF observed at step %d and disarmed; the walk is green and "
           "identical to the unplanted one again\n",
           s.name, red.lossStep);
    plan->negItem = negItem;
    plan->firstGrant = firstGrant;
    plan->startIds = startIds;

    // ------------------------------------------------------------------
    // G2's RED HALF: an ORDER-LATCHED edge. Pick Y before X in the forward
    // order and X before Y in the reverse order (both negatable, distinct).
    // ------------------------------------------------------------------
    std::vector<uint16_t> negIds;
    for (const uint16_t id : fwd) {
        if (negatable(id) == 1 && FirstIndexOf(negIds, id) < 0) {
            negIds.push_back(id);
        }
    }
    uint16_t latchY = 0;
    uint16_t latchX = 0;
    bool havePair = false;
    for (size_t i = 0; i < negIds.size() && !havePair; ++i) {
        for (size_t j = i + 1; j < negIds.size() && !havePair; ++j) {
            const uint16_t y = negIds[i];
            const uint16_t x = negIds[j];
            if (FirstIndexOf(fwd, y) < FirstIndexOf(fwd, x) && FirstIndexOf(rev, x) < FirstIndexOf(rev, y)) {
                latchY = y;
                latchX = x;
                havePair = true;
            }
        }
    }
    CLMONO_ASSERT(havePair, "G2 RED HALF: no pair of negatable items is granted Y-before-X forward and X-before-Y "
                            "in reverse - the order latch cannot be planted");
    Walk latchFwd;
    Walk latchRev;
    Closure latchOne;
    int latchTarget = -1;
    for (int run = 0; run < 3; ++run) {
        const int t = armEdge(kEdgeOrderLatch, startIds.data(), (int)startIds.size(), latchX, latchY, &parent);
        CLMONO_ASSERT(t >= 0, "G2 RED HALF: the probe could not plant the order-latched edge");
        latchTarget = t;
        bool oneShotOk = true;
        if (run == 0) {
            latchFwd = RunWalk(s, fwd, false);
        } else if (run == 1) {
            latchRev = RunWalk(s, rev, false);
        } else {
            oneShotOk = OneShot(s, fwd, &latchOne);
        }
        CLMONO_ASSERT(disarm() == 1, "G2 RED HALF: the latched edge was not armed when its run ended");
        CLMONO_ASSERT(oneShotOk, "G2 RED HALF: the one-shot round did not open or reach a fixpoint");
    }
    CLMONO_ASSERT(latchFwd.opened && latchRev.opened, "G2 RED HALF: a latched walk did not open");
    const Closure latchFinals[2] = { latchFwd.final_, latchRev.final_ };
    const int g2Red = G2Disagreement(latchFinals, 2, latchOne);
    const size_t lt = (size_t)latchTarget;
    printf("[TEST] combo-logic-monotonicity: %s G2 RED HALF: latch Y=%u (forward step %d) before X=%u (forward step "
           "%d); region %d final: forward=%d reverse=%d one-shot=%d; G1 on those walks: forward lost=%d reverse "
           "lost=%d; G2 comparison says order %d disagrees\n",
           s.name, (unsigned)latchY, FirstIndexOf(fwd, latchY), (unsigned)latchX, FirstIndexOf(fwd, latchX),
           latchTarget, latchFwd.final_.regions[lt] != 0 ? 1 : 0, latchRev.final_.regions[lt] != 0 ? 1 : 0,
           latchOne.regions[lt] != 0 ? 1 : 0, latchFwd.lost ? 1 : 0, latchRev.lost ? 1 : 0, g2Red);
    // The latch never shrinks along a walk, so G1 stays green: only G2 can see it.
    CLMONO_ASSERT(!latchFwd.lost && !latchRev.lost,
                  "G2 RED HALF: the order-latched edge made a walk SHRINK - it was meant to be invisible to G1");
    CLMONO_ASSERT(latchFwd.final_.regions[lt] != 0 && latchRev.final_.regions[lt] == 0 && latchOne.regions[lt] == 0,
                  "G2 RED HALF: the latched region was not kept by the forward walk alone");
    CLMONO_ASSERT(g2Red == 1, "G2 RED HALF: G2's comparison did not see two orders close on different sets");
    CLMONO_ASSERT(!Equal(latchOne, latchFwd.final_), "G2 RED HALF: G2's one-shot comparison did not see it");
    printf("[TEST] combo-logic-monotonicity: %s G2 RED HALF observed: G1 green, G2 red (orders and one-shot)\n",
           s.name);

    // ------------------------------------------------------------------
    // G3's RED HALF: a TRICK-NEGATED edge. Tricks-on must LOSE the region at
    // step 0 through G3's own comparison.
    // ------------------------------------------------------------------
    const size_t shortRows = fwd.size() < kTrickRedRows ? fwd.size() : kTrickRedRows;
    const std::vector<uint16_t> shortFwd(fwd.begin(), fwd.begin() + (long)shortRows);
    const int trickTarget = armEdge(kEdgeTrickOff, startIds.data(), (int)startIds.size(), 0, 0, &parent);
    CLMONO_ASSERT(trickTarget >= 0, "G3 RED HALF: the probe could not plant the trick-negated edge");
    Walk trickOff = RunWalk(s, shortFwd, true);
    const int forcedOn = forceTricks(1);
    Walk trickOn = RunWalk(s, shortFwd, true);
    forceTricks(0);
    CLMONO_ASSERT(disarm() == 1, "G3 RED HALF: the trick-negated edge was not armed when its runs ended");
    CLMONO_ASSERT(trickOff.opened && trickOn.opened && forcedOn > 0, "G3 RED HALF: a walk did not open");
    int g3Strictly = 0;
    int g3Kind = -1;
    int g3Id = -1;
    uint64_t g3Bits = 0;
    const int g3Red = G3FirstLoss(trickOn, trickOff, &g3Strictly, &g3Kind, &g3Id, &g3Bits);
    printf("[TEST] combo-logic-monotonicity: %s G3 RED HALF: planted `trick OFF` on region %d; G3's comparison "
           "first fails at step %d (%s %d)\n",
           s.name, trickTarget, g3Red, g3Kind == 0 ? "check" : "region", g3Id);
    CLMONO_ASSERT(g3Red == 0, "G3 RED HALF: G3's containment did not fail at step 0 on a trick-negated edge");
    CLMONO_ASSERT(trickOff.trace[0].regions[(size_t)trickTarget] != 0 &&
                      trickOn.trace[0].regions[(size_t)trickTarget] == 0,
                  "G3 RED HALF: the planted region is not the one tricks-on lost");
    printf("[TEST] combo-logic-monotonicity: %s G3 RED HALF observed at step 0\n", s.name);
    return TEST_PASS;
}

/** One coordinator round over a prefix, with both candidate host SETS read back
 *  (sorted) through the coordinator's read-only accessor. */
struct PrefixObs {
    int status = -1;
    ComboLogicRoundResult res;
    std::vector<uint16_t> hosts[2]; // [0] OoT, [1] MM
};

PrefixObs RunPrefix(const std::vector<ComboLogicBagItem>& order, int k) {
    PrefixObs o;
    memset(&o.res, 0, sizeof(o.res));
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = (k > 0) ? order.data() : nullptr;
    req.assumedCount = k;
    req.goal = RSBS_COMBO_GOAL_BEAT_BOTH;
    o.status = Combo_Logic_RunRound(&req, &o.res);
    for (int side = 0; side < 2; ++side) {
        const GameId g = side == 0 ? GAME_OOT : GAME_MM;
        const int n = Combo_Logic_TestLastCandidates(g, nullptr, 0);
        o.hosts[side].assign((size_t)(n > 0 ? n : 0), 0);
        if (n > 0) {
            Combo_Logic_TestLastCandidates(g, o.hosts[side].data(), n);
        }
        std::sort(o.hosts[side].begin(), o.hosts[side].end());
    }
    return o;
}

/** G4's comparison, shared by the green and the red half: what the LONGER prefix
 *  `cur` lost relative to `prev` — nullptr if nothing; else what, with the side
 *  (0 OoT, 1 MM) and, for a host, its id. */
const char* PrefixLoss(const PrefixObs& prev, const PrefixObs& cur, int* outSide, int* outHost) {
    for (int side = 0; side < 2; ++side) {
        const std::vector<uint16_t>& a = prev.hosts[side];
        const std::vector<uint16_t>& b = cur.hosts[side];
        for (const uint16_t h : a) {
            if (!std::binary_search(b.begin(), b.end(), h)) {
                *outSide = side;
                *outHost = h;
                return "a longer prefix REACHED FEWER hosts (a host left the candidate set)";
            }
        }
    }
    *outHost = -1;
    if (cur.res.crossingOpenOoT < prev.res.crossingOpenOoT || cur.res.crossingOpenMM < prev.res.crossingOpenMM) {
        *outSide = cur.res.crossingOpenOoT < prev.res.crossingOpenOoT ? 0 : 1;
        return "a longer prefix CLOSED a crossing";
    }
    if (cur.res.goalOoT < prev.res.goalOoT || cur.res.goalMM < prev.res.goalMM) {
        *outSide = cur.res.goalOoT < prev.res.goalOoT ? 0 : 1;
        return "a longer prefix LOST a half's goal";
    }
    return nullptr;
}

/** G4 over the coordinator: prefixes of the union order, both engines at once. */
TestResult CoordinatorPrefixes(const char* label, const std::vector<ComboLogicBagItem>& unionOrder) {
    const int n = (int)unionOrder.size();
    PrefixObs prev;
    PrefixObs first;
    bool havePrev = false;
    const double t0 = NowMs();
    for (int p = 0; p <= kPrefixPoints; ++p) {
        const int k = (int)(((long long)n * p) / kPrefixPoints);
        PrefixObs cur = RunPrefix(unionOrder, k);
        if (p % 32 == 0) {
            printf("[TEST] combo-logic-monotonicity: G4 %s prefix %4d/%d: status=%s iters=%d candidates OoT=%d MM=%d "
                   "crossing OoT=%d MM=%d goal OoT=%d MM=%d\n",
                   label, k, n, Combo_Logic_StatusName(cur.status), cur.res.iterations, cur.res.candidatesOoT,
                   cur.res.candidatesMM, cur.res.crossingOpenOoT, cur.res.crossingOpenMM, cur.res.goalOoT,
                   cur.res.goalMM);
        }
        CLMONO_ASSERT(cur.status == RSBS_COMBO_LOGIC_OK, "G4: a coordinator round over a bag prefix did not return OK");
        CLMONO_ASSERT((int)cur.hosts[0].size() == cur.res.candidatesOoT &&
                          (int)cur.hosts[1].size() == cur.res.candidatesMM,
                      "G4: the candidate-set accessor disagrees with the round's own counts");
        if (havePrev) {
            int side = -1;
            int host = -1;
            const char* loss = PrefixLoss(prev, cur, &side, &host);
            if (loss != nullptr) {
                printf("[TEST] combo-logic-monotonicity: G4 %s prefix %d -> %d: %s side, host %d: %s\n", label,
                       (int)(((long long)n * (p - 1)) / kPrefixPoints), k, side == 0 ? "OoT" : "MM", host, loss);
            }
            CLMONO_ASSERT(loss == nullptr, "G4: a longer bag prefix lost a host, a crossing or a goal (line above)");
        } else {
            first = cur;
        }
        prev = cur;
        havePrev = true;
    }
    printf("[TEST] combo-logic-monotonicity: G4 %s: %d prefixes in %.0fms, no host, crossing or goal lost\n", label,
           kPrefixPoints + 1, NowMs() - t0);
    // Anti-vacuity: every observable the comparison holds non-decreasing actually
    // MOVED over the prefixes. (On the shipped default profile the whole union
    // bag proves both halves; the empty prefix proves neither and has OoT's
    // crossing closed.) That the comparison can FAIL is G4's red half.
    CLMONO_ASSERT(prev.res.candidatesOoT > first.res.candidatesOoT && prev.res.candidatesMM > first.res.candidatesMM,
                  "G4: a side's candidate count never grew over the prefixes - the prefix walk is vacuous");
    CLMONO_ASSERT(first.res.crossingOpenOoT == 0 && prev.res.crossingOpenOoT == 1,
                  "G4: OoT's crossing did not go from closed to open over the prefixes");
    CLMONO_ASSERT(first.res.goalOoT == 0 && prev.res.goalOoT == 1 && first.res.goalMM == 0 && prev.res.goalMM == 1,
                  "G4: a half's goal did not go from unprovable to provable over the prefixes");
    return TEST_PASS;
}

/**
 * G4's RED HALF for one side: the prefix pair straddling that side's G1 negated
 * item's first grant in the union order. Unplanted, G4's comparison must see no
 * loss on it; with G1's NOT_HELD edge planted again, it must see a lost host on
 * that side.
 */
TestResult CoordinatorRedHalf(const char* name, int side, const std::vector<ComboLogicBagItem>& unionOrder, int offset,
                              const RedPlan& plan, ArmEdgeFn armEdge, int (*disarm)(void)) {
    const int k = offset + plan.firstGrant;
    CLMONO_ASSERT(k >= 0 && k + 1 <= (int)unionOrder.size() && unionOrder[(size_t)k].item.id == plan.negItem,
                  "G4 RED HALF: the union order does not grant the negated item where the plan says");
    int lossSide = -1;
    int lossHost = -1;
    const PrefixObs g0 = RunPrefix(unionOrder, k);
    const PrefixObs g1 = RunPrefix(unionOrder, k + 1);
    CLMONO_ASSERT(g0.status == RSBS_COMBO_LOGIC_OK && g1.status == RSBS_COMBO_LOGIC_OK, "G4 RED HALF: a round failed");
    CLMONO_ASSERT(PrefixLoss(g0, g1, &lossSide, &lossHost) == nullptr,
                  "G4 RED HALF: the unplanted prefix pair already loses something");
    uint16_t parent = 0;
    const int target = armEdge(kEdgeNotHeld, plan.startIds.data(), (int)plan.startIds.size(), plan.negItem, 0, &parent);
    CLMONO_ASSERT(target >= 0, "G4 RED HALF: the probe could not plant the negated edge");
    const PrefixObs r0 = RunPrefix(unionOrder, k);
    const PrefixObs r1 = RunPrefix(unionOrder, k + 1);
    CLMONO_ASSERT(disarm() == 1, "G4 RED HALF: the planted edge was not armed when its rounds ended");
    CLMONO_ASSERT(r0.status == RSBS_COMBO_LOGIC_OK && r1.status == RSBS_COMBO_LOGIC_OK, "G4 RED HALF: a round failed");
    const char* loss = PrefixLoss(r0, r1, &lossSide, &lossHost);
    const int c0 = side == 0 ? r0.res.candidatesOoT : r0.res.candidatesMM;
    const int c1 = side == 0 ? r1.res.candidatesOoT : r1.res.candidatesMM;
    printf("[TEST] combo-logic-monotonicity: G4 RED HALF %s: prefix %d -> %d (grants item %u) with `NOT holding` "
           "planted on region %d: %s (side %s, host %d); %s candidates %d -> %d, so a count-only comparison would "
           "%s\n",
           name, k, k + 1, (unsigned)plan.negItem, target, loss != nullptr ? loss : "NO LOSS SEEN",
           lossSide == 0 ? "OoT" : (lossSide == 1 ? "MM" : "-"), lossHost, name, c0, c1,
           c1 < c0 ? "also have seen it" : "NOT have seen it");
    CLMONO_ASSERT(loss != nullptr && lossSide == side && lossHost >= 0,
                  "G4 RED HALF: G4's comparison did not see the planted negation lose a host on its side - G4 is "
                  "vacuous");
    return TEST_PASS;
}

} // namespace clmono

TestResult ComboLogicMonotonicity_Run(void) {
    using namespace clmono;
    printf("[TEST] combo-logic-monotonicity: ADR 0010 O6's grow-check - grant the bag one copy at a time over both "
           "real engines and assert the reached check and region sets never shrink (#645, #500)\n");

    const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mm = Combo_Logic_GetEngine(GAME_MM);
    CLMONO_ASSERT(oot != nullptr && mm != nullptr, "both real engines must be registered");
    CLMONO_ASSERT(oot->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI && mm->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI,
                  "a registered engine carries the wrong ABI");

    CLMONO_ASSERT(Rando_HeadlessSeedTest("RSBSCOMBOMONO1") == 0, "headless OoT seed generation failed");
    MM_Rando_InitCore();

    std::unique_ptr<unsigned char[]> saveBefore(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveBefore.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    const uint32_t worldDigest0 = OoT_ComboLogic_TestWorldDigest();
    CLMONO_ASSERT(worldDigest0 != 0u, "OoT's world digest is zero - no fill result is visible");

    MM_ComboLogic_ResetCounters();
    (void)MM_ComboLogic_ApplyShippedProfile();
    std::unique_ptr<unsigned char[]> saveAfterProfile(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    CLMONO_ASSERT(memcmp(saveBefore.get(), saveAfterProfile.get(), OOT_SAVE_CONTEXT_SIZE) != 0,
                  "applying MM's shipped profile changed no byte - the save comparison below would be vacuous");

    // ------------------------------------------------------------------
    // OoT's pool: every owned host's placed FILL item. Those hosts are EMPTIED
    // for the walks (so the search harvests nothing the bag does not grant) and
    // restored; a host holding a non-fill item (the Triforce, an event) keeps it.
    // ------------------------------------------------------------------
    const int owned = OoT_ComboLogic_TestOwnedHostCount();
    CLMONO_ASSERT(owned > 0, "the OoT engine owns no host over a generated world");
    std::vector<uint16_t> ootHosts((size_t)owned, 0);
    CLMONO_ASSERT(OoT_ComboLogic_TestOwnedHosts(ootHosts.data(), owned) == owned, "owned-host bridge disagreed");
    std::vector<uint16_t> ootPool;
    std::vector<uint16_t> ootPrior((size_t)owned, 0);
    std::vector<bool> ootEmpty((size_t)owned, false);
    int ootKept = 0;
    // READ phase: nothing is mutated until every host has been read, so a failure
    // here leaves the world as found.
    for (int i = 0; i < owned; ++i) {
        uint16_t item = 0;
        int adv = 0;
        CLMONO_ASSERT(OoT_ComboLogic_TestPlacedItemAt(ootHosts[(size_t)i], &item, &adv) == 1, "unreadable host");
        ootPrior[(size_t)i] = item;
        if (item == 0) {
            continue;
        }
        SharedItem si;
        memset(&si, 0, sizeof(si));
        si.originGame = (uint8_t)GAME_OOT;
        si.id = item;
        if (Combo_ItemClassOf(si) == RSBS_FILL_CLASS_NONE) {
            ootKept++;
            continue;
        }
        ootPool.push_back(item);
        ootEmpty[(size_t)i] = true;
    }

    // Everything from the first mutation on runs inside `body`, so EVERY failure
    // path — an assert anywhere below returns TEST_FAIL from the lambda — reaches
    // the restore after it (hosts, tricks, planted edges, the save buffer).
    RedPlan ootPlan;
    RedPlan mmPlan;
    int ootRows = 0;
    int mmRows = 0;
    auto body = [&]() -> TestResult {
        for (int i = 0; i < owned; ++i) {
            if (ootEmpty[(size_t)i]) {
                CLMONO_ASSERT(OoT_ComboLogic_TestSetPlacedItem(ootHosts[(size_t)i], 0) == 1, "could not empty an OoT host");
            }
        }
        printf("[TEST] combo-logic-monotonicity: OoT: %d owned hosts, %d fill copies emptied into the bag, %d non-fill "
               "item(s) left in place\n",
               owned, (int)ootPool.size(), ootKept);

        // MM's pool: the graph's vanilla multiset.
        const int mmTotal = MM_ComboLogic_PoolVanillaItems(nullptr, nullptr, 0);
        CLMONO_ASSERT(mmTotal > 0, "MM's graph yields no giveable vanilla item");
        std::vector<uint16_t> mmPool((size_t)mmTotal, 0);
        MM_ComboLogic_PoolVanillaItems(mmPool.data(), nullptr, mmTotal);

        const Bag ootBag = BuildBag(GAME_OOT, ootPool);
        const Bag mmBag = BuildBag(GAME_MM, mmPool);
        printf("[TEST] combo-logic-monotonicity: OoT bag %d rows = %d required + %d surplus + %d filler + %d trap (pool of "
               "%d copies over %d owned hosts)\n",
               (int)ootBag.rows.size(), ootBag.required, ootBag.surplus, ootBag.filler, ootBag.traps, (int)ootPool.size(),
               owned);
        printf("[TEST] combo-logic-monotonicity: MM  bag %d rows = %d required + %d surplus + %d filler + %d trap (%d trap "
               "copies NOT granted: RI_TRAP's give leaves the save) (pool of %d copies, %d with no fill class)\n",
               (int)mmBag.rows.size(), mmBag.required, mmBag.surplus, mmBag.filler, mmBag.traps, mmBag.trapsSkipped,
               mmTotal, mmBag.unclassified);
        CLMONO_ASSERT(ootBag.required > 100 && mmBag.required > 100, "a bag half is implausibly small - the grow-check "
                                                                     "would walk almost nothing");
        CLMONO_ASSERT(ootBag.surplus > 0 && mmBag.surplus > 0 && ootBag.filler > 0 && mmBag.filler > 0,
                      "a bag shape (surplus or filler) is empty");

        Side ootSide = { "OoT", GAME_OOT, oot, OoT_ComboLogic_TestCheckIdSpace(), false };
        Side mmSide = { "MM", GAME_MM, mm, kMmCheckSpace, false };

        ootRows = (int)ootBag.rows.size();
        mmRows = (int)mmBag.rows.size();
        TestResult r = WalkEngine(ootSide, ootBag, OoT_ComboMono_ForceAllTricks, OoT_ComboMono_NegatableItem,
                                  OoT_ComboMono_ArmEdge, OoT_ComboMono_Disarm, &ootPlan);
        if (r == TEST_PASS) {
            r = WalkEngine(mmSide, mmBag, MM_ComboMono_ForceAllTricks, MM_ComboMono_NegatableItem, MM_ComboMono_ArmEdge,
                           MM_ComboMono_Disarm, &mmPlan);
        }

        // G4: the coordinator, both engines at once, over prefixes of the union bag.
        if (r == TEST_PASS) {
            std::vector<ComboLogicBagItem> unionOrder;
            for (int g = 0; g < 2; ++g) {
                const Bag& b = (g == 0) ? ootBag : mmBag;
                for (const uint16_t id : b.rows) {
                    ComboLogicBagItem row;
                    memset(&row, 0, sizeof(row));
                    row.item.originGame = (uint8_t)(g == 0 ? GAME_OOT : GAME_MM);
                    row.item.id = id;
                    unionOrder.push_back(row);
                }
            }
            std::vector<ComboLogicBagItem> unionReverse(unionOrder.rbegin(), unionOrder.rend());
            for (int tricks = 0; tricks < 2 && r == TEST_PASS; ++tricks) {
                if (tricks) {
                    OoT_ComboMono_ForceAllTricks(1);
                    MM_ComboMono_ForceAllTricks(1);
                }
                r = CoordinatorPrefixes(tricks ? "tricks-ON forward" : "tricks-off forward", unionOrder);
                if (r == TEST_PASS) {
                    r = CoordinatorPrefixes(tricks ? "tricks-ON reverse" : "tricks-off reverse", unionReverse);
                }
                if (tricks) {
                    OoT_ComboMono_ForceAllTricks(0);
                    MM_ComboMono_ForceAllTricks(0);
                }
            }
            // G4's red half, one side at a time (tricks off): OoT's negated item in
            // the OoT half of the union order, MM's after every OoT row.
            if (r == TEST_PASS) {
                r = CoordinatorRedHalf("OoT", 0, unionOrder, 0, ootPlan, OoT_ComboMono_ArmEdge, OoT_ComboMono_Disarm);
            }
            if (r == TEST_PASS) {
                r = CoordinatorRedHalf("MM", 1, unionOrder, (int)ootBag.rows.size(), mmPlan, MM_ComboMono_ArmEdge,
                                       MM_ComboMono_Disarm);
            }
        }
        return r;
    };
    TestResult r = body();

    // ------------------------------------------------------------------
    // Leave the process as found. Every check below runs BEFORE the outer
    // restore, against the inner baseline.
    // ------------------------------------------------------------------
    OoT_ComboMono_Disarm();
    MM_ComboMono_Disarm();
    OoT_ComboMono_ForceAllTricks(0);
    MM_ComboMono_ForceAllTricks(0);
    int restoreFailures = 0;
    for (int i = 0; i < owned; ++i) {
        if (OoT_ComboLogic_TestSetPlacedItem(ootHosts[(size_t)i], ootPrior[(size_t)i]) != 1) {
            restoreFailures++;
        }
    }
    if (restoreFailures != 0) {
        printf("[TEST] FAIL: could not restore %d emptied OoT host(s)\n", restoreFailures);
        r = TEST_FAIL;
    }
    if (r != TEST_PASS) {
        memcpy(gSaveContext, saveBefore.get(), OOT_SAVE_CONTEXT_SIZE);
        return r;
    }
    CLMONO_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0, "OoT's world digest moved");
    CLMONO_ASSERT(MM_ComboLogic_SnapshotLive() == 0, "MM's snapshot is still live");
    CLMONO_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0 && Combo_Logic_PlacementCount(GAME_OOT) == 0 &&
                      Combo_Logic_PlacementCount(GAME_MM) == 0,
                  "a coordinator placement survived the row");
    CLMONO_ASSERT(memcmp(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
                  "the unified save buffer differs from the profile-applied state - a round, the trick forcing or "
                  "the red half leaked into the live save");
    memcpy(gSaveContext, saveBefore.get(), OOT_SAVE_CONTEXT_SIZE); // construction, not a claim

    printf("[TEST] PASS: reachability never shrank over %d OoT and %d MM grants per walk, four orders, tricks off and "
           "on; the orders agree; the coordinator's prefixes never lost a host; and every lock (G1-G4) went red on its "
           "planted edge on both engines\n",
           ootRows, mmRows);
    return TEST_PASS;
}
