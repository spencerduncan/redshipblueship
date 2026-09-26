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
 *       the tricks-on set is not vacuous (the forcing seam turned keys on, and on
 *       OoT at least one step's closure is strictly larger).
 *   G4. THE COORDINATOR. `Combo_Logic_RunRound` over growing PREFIXES of the
 *       union bag (both halves, forward and reverse): every round returns OK, and
 *       neither side's candidate count, crossing flag nor goal flag ever drops.
 *       This is the operator as the fill sees it — both engines, the arrival
 *       gate, the exchange — not each engine alone.
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
 * THE RED HALF (O6's "observe the row fail"). One NEGATED EDGE — "passable only
 * while the player does NOT hold X" — is planted at runtime into each real graph
 * (never into a region file; the two game-side probe TUs install and remove it),
 * and the same walk must go red AT EXACTLY THE GRANT OF X, on both the region and
 * the check observable, and MM's own shrink counter must see it. Then the edge is
 * removed and the walk must be green and identical to the unplanted one.
 *
 * WHAT THE ROW LEAVES BEHIND: OoT's world digest, OoT's trick options, every
 * emptied host, MM's frozen trick bits, MM's snapshot not live, no coordinator
 * placement, and the unified save buffer byte-identical to the state MM's shipped
 * profile left it in — compared BEFORE the outer restore (combo-logic-measure's S4
 * lesson: after it the comparison cannot fail).
 *
 * ============================================================================
 * WHAT IT DOES NOT CLAIM
 * ============================================================================
 *
 *  - ONE PROFILE PER GAME (the shipped default) with tricks all-off and all-on.
 *    Other settings change the operator; they are not walked.
 *  - FOUR ORDERS, not all orders. A non-monotone edge that fires only for an item
 *    combination no walked prefix produces is not seen. The per-step check makes
 *    each walk sensitive to every prefix it passes through (546 OoT and 2282 MM
 *    prefixes per walk on the shipped profile, measured 2026-09-26), which is the
 *    reason the walk grants one copy at a time. The whole row ran in ~30 s on the
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
    // FILLER: every junk/renewable copy of the pool (more checks than items is the
    // normal case, and every one of these copies is a give the walk must survive)...
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

/**
 * G1-G3 for one engine: four walks (and a one-shot) under tricks off, the same
 * under tricks on, the trick containment along the forward walk, and the red
 * half. Returns TEST_PASS / TEST_FAIL.
 */
TestResult WalkEngine(Side& s, const Bag& bag, int (*forceTricks)(int), int (*negatable)(uint16_t),
                      int (*arm)(const uint16_t*, int, uint16_t, uint16_t*), int (*disarm)(void)) {
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
            }
            if (o == 0 && tricks == 1) {
                // G3: step-wise containment, tricks-off within tricks-on.
                int strictlyLarger = 0;
                for (size_t k = 0; k < w.trace.size() && k < forwardOff.trace.size(); ++k) {
                    int kind = -1;
                    int id = -1;
                    uint64_t bits = 0;
                    if (!Contains(w.trace[k], forwardOff.trace[k], &kind, &id, &bits)) {
                        printf("[TEST] combo-logic-monotonicity: %s step %d: the tricks-off closure has %s %d "
                               "(bits 0x%llX) that the tricks-on closure lacks\n",
                               s.name, (int)k, kind == 0 ? "check" : "region", id, (unsigned long long)bits);
                        CLMONO_ASSERT(false, "G3: turning tricks on LOST reachability - tricks are not a monotone "
                                             "parameter of the operator (ADR 0010 section 3.2)");
                    }
                    if (!Equal(w.trace[k], forwardOff.trace[k])) {
                        strictlyLarger++;
                    }
                }
                printf("[TEST] combo-logic-monotonicity: %s G3: tricks-off contained in tricks-ON at every one of "
                       "%d forward steps; strictly larger at %d of them\n",
                       s.name, (int)w.trace.size(), strictlyLarger);
                if (s.game == GAME_OOT) {
                    CLMONO_ASSERT(strictlyLarger > 0, "G3: every trick on changed no OoT step's closure - the "
                                                      "tricks-on walk is not measuring a different operator");
                }
            }
        }
        // G2: order-independence, and agreement with the one-shot round.
        Closure oneShot;
        CLMONO_ASSERT(OneShot(s, fwd, &oneShot), "the one-shot round did not open or reach a fixpoint");
        for (int o = 1; o < 4; ++o) {
            CLMONO_ASSERT(Equal(finals[o], finals[0]),
                          "G2: two grant orders of the same multiset closed on DIFFERENT sets - the round is not a "
                          "function of the multiset");
        }
        CLMONO_ASSERT(Equal(oneShot, finals[0]),
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
    const int target = arm(startIds.data(), (int)startIds.size(), negItem, &parent);
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
    return TEST_PASS;
}

/** G4 over the coordinator: prefixes of the union order, both engines at once. */
TestResult CoordinatorPrefixes(const char* label, const std::vector<ComboLogicBagItem>& unionOrder) {
    const int n = (int)unionOrder.size();
    const int points = 16;
    ComboLogicRoundResult prev;
    memset(&prev, 0, sizeof(prev));
    ComboLogicRoundResult first;
    memset(&first, 0, sizeof(first));
    bool havePrev = false;
    for (int p = 0; p <= points; ++p) {
        const int k = (int)(((long long)n * p) / points);
        ComboLogicRoundRequest req;
        memset(&req, 0, sizeof(req));
        req.assumed = (k > 0) ? unionOrder.data() : nullptr;
        req.assumedCount = k;
        req.goal = RSBS_COMBO_GOAL_BEAT_BOTH;
        ComboLogicRoundResult res;
        memset(&res, 0, sizeof(res));
        const int status = Combo_Logic_RunRound(&req, &res);
        printf("[TEST] combo-logic-monotonicity: G4 %s prefix %4d/%d: status=%s iters=%d candidates OoT=%d MM=%d "
               "crossing OoT=%d MM=%d goal OoT=%d MM=%d\n",
               label, k, n, Combo_Logic_StatusName(status), res.iterations, res.candidatesOoT, res.candidatesMM,
               res.crossingOpenOoT, res.crossingOpenMM, res.goalOoT, res.goalMM);
        CLMONO_ASSERT(status == RSBS_COMBO_LOGIC_OK, "G4: a coordinator round over a bag prefix did not return OK");
        if (havePrev) {
            CLMONO_ASSERT(res.candidatesOoT >= prev.candidatesOoT && res.candidatesMM >= prev.candidatesMM,
                          "G4: a longer prefix REACHED FEWER hosts on one side");
            CLMONO_ASSERT(res.crossingOpenOoT >= prev.crossingOpenOoT && res.crossingOpenMM >= prev.crossingOpenMM,
                          "G4: a longer prefix CLOSED a crossing");
            CLMONO_ASSERT(res.goalOoT >= prev.goalOoT && res.goalMM >= prev.goalMM,
                          "G4: a longer prefix LOST a half's goal");
        }
        if (!havePrev) {
            first = res;
        }
        prev = res;
        havePrev = true;
    }
    // Anti-vacuity: every observable the assertions above hold non-decreasing
    // actually MOVED over the prefixes, so "never dropped" is not satisfied by a
    // constant. (On the shipped default profile the whole union bag proves both
    // halves; the empty prefix proves neither and has OoT's crossing closed.)
    CLMONO_ASSERT(prev.candidatesOoT > first.candidatesOoT && prev.candidatesMM > first.candidatesMM,
                  "G4: a side's candidate count never grew over the prefixes - the prefix walk is vacuous");
    CLMONO_ASSERT(first.crossingOpenOoT == 0 && prev.crossingOpenOoT == 1,
                  "G4: OoT's crossing did not go from closed to open over the prefixes");
    CLMONO_ASSERT(first.goalOoT == 0 && prev.goalOoT == 1 && first.goalMM == 0 && prev.goalMM == 1,
                  "G4: a half's goal did not go from unprovable to provable over the prefixes");
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
    int ootKept = 0;
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
        CLMONO_ASSERT(OoT_ComboLogic_TestSetPlacedItem(ootHosts[(size_t)i], 0) == 1, "could not empty an OoT host");
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

    TestResult r = WalkEngine(ootSide, ootBag, OoT_ComboMono_ForceAllTricks, OoT_ComboMono_NegatableItem,
                              OoT_ComboMono_ArmRegionNegation, OoT_ComboMono_Disarm);
    if (r == TEST_PASS) {
        r = WalkEngine(mmSide, mmBag, MM_ComboMono_ForceAllTricks, MM_ComboMono_NegatableItem,
                       MM_ComboMono_ArmRegionNegation, MM_ComboMono_Disarm);
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
    }

    // ------------------------------------------------------------------
    // Leave the process as found. Every check below runs BEFORE the outer
    // restore, against the inner baseline.
    // ------------------------------------------------------------------
    OoT_ComboMono_Disarm();
    MM_ComboMono_Disarm();
    OoT_ComboMono_ForceAllTricks(0);
    MM_ComboMono_ForceAllTricks(0);
    for (int i = 0; i < owned; ++i) {
        CLMONO_ASSERT(OoT_ComboLogic_TestSetPlacedItem(ootHosts[(size_t)i], ootPrior[(size_t)i]) == 1,
                      "could not restore an emptied OoT host");
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
           "on; the orders agree; the planted negation went red where it had to on both engines\n",
           (int)ootBag.rows.size(), (int)mmBag.rows.size());
    return TEST_PASS;
}
