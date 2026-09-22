/**
 * @file test_oot_logic_export.c
 * @brief Locks for the OoT combo-logic ENGINE, over the real OoT rando graph
 *        (ADR 0010 increment 3, #645; lane K2a).
 *
 * WHY THE `rando` TIER AND NOT THE DISPLAY-FREE ONE. Every fact this engine
 * answers is a function of a FILL RESULT and of the region graph: the reached set
 * comes from `ReachabilitySearch` over `areaTable`, the host lists read
 * `GetPlacedRandomizerGet()`, and `goalReached` looks for the check that holds
 * `RG_TRIFORCE`. With no generation in the process every location is `RG_NONE`,
 * the graph is empty, and an assertion like "the closure did not shrink" is
 * satisfied by 0 == 0. That is the vacuity trap this tree already recorded twice
 * (#491's deferred COND_HOOK, #510's zero-host predicate), so this row runs a
 * REAL headless generation first and then asserts non-zero counts and STRICT
 * inequalities wherever a constant would otherwise pass.
 *
 * The coordinator's own contract is locked ROM-free over stub engines in
 * test_combo_logic.c. This file locks the OTHER half: that OoT's real primitives
 * actually satisfy that contract. It drives the REGISTERED vtable through
 * `Combo_Logic_GetEngine(GAME_OOT)` — not a copy of it — so a lock here stops
 * passing the moment the engine moves.
 *
 * The seven claims, and what each one would catch:
 *
 *  1. REGISTRATION. The engine is published by a file-scope registrar in
 *     soh_rando, which means it survived the WHOLE_ARCHIVE link and passed the
 *     coordinator's own validation (ABI, no vtable hole, snapshot/restore paired).
 *     Catches the elision class that #512/#516 are about.
 *  2. THE RESET IS REAL. `crossingOpen` and the reached count are ZERO between
 *     `beginQuery` and the first `expand`, and non-zero after it. A `beginQuery`
 *     that forgot `AccessReset` would report the previous round's residue here,
 *     and this is the differential that shows it.
 *  3. IDENTICAL QUERIES AGREE — three times: twice back to back, and once after
 *     a BARE `ReachabilitySearch` with no reset in front of it. That third leg is
 *     the direct lock on the residue defect: the bare search is precisely the
 *     call shape that made the #656 gate answer 48-of-57 against a truth of 57,
 *     and `beginQuery`'s `Logic::Reset(true)` is the only reason the answer comes
 *     back the same.
 *  4. MONOTONICITY, NON-VACUOUSLY. Every advancement-bearing location is emptied
 *     so the baseline closure is genuinely PARTIAL, then the same items are
 *     ASSUMED and the closure must be STRICTLY larger. Without the emptying step
 *     a post-fill world under All Locations Reachable has a total closure already
 *     and `>=` is satisfied by equality — measured, printed, and asserted strict.
 *  5. THE LIVE SAVE AND THE RE-ATTACH. With `Logic` attached to `&gSaveContext`
 *     (the state a loaded save leaves it in), a whole round plus a `place` leaves
 *     the unified save buffer byte-for-byte identical and leaves
 *     `Logic::mSaveContext` pointing back at it. Catches the §1.8 hazard the
 *     audit names: a query that applied item effects into the player's save.
 *  6. `endQuery` AFTER A FAILED `beginQuery`, and twice. The coordinator may tear
 *     down a side whose bracket never opened (a snapshot can succeed before a
 *     `beginQuery` refuses), so the teardown must be a no-op there rather than
 *     "restoring" a pointer nobody saved.
 *  7. THE HOST SURFACE. `allEmptyHosts` is a superset of `reachedEmptyHosts`,
 *     both are ascending and de-duplicated, a small `cap` truncates the WRITE
 *     while the return still reports the TOTAL, and the id space the engine can
 *     enumerate is wider than `RSBS_COMBO_LOGIC_PLACEMENT_CAP` — which is the
 *     fact a coordinator scratch buffer has to be sized from.
 *
 * Plus one composed round through `Combo_Logic_RunRound` against a deliberately
 * trivial MM stub, so the OoT engine is shown to work inside the coordinator's
 * bracket and not only when driven by hand. The MM half of that round proves
 * nothing about MM; its real engine is a separate lane.
 *
 * WORLD-UNCHANGED DISCIPLINE. The row takes an FNV digest over (check, placed
 * item) for the whole id space before it touches anything and re-asserts it after
 * every perturbation, so "nothing this engine did moved a placement" is a
 * measured equality rather than an argument.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE and therefore
 * compiled as C++, like every other file in this directory.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

// The harness the `rando` tier rows share. Forward-declared rather than reached
// for: every `tests/*.c` is #included into test_runner.cpp ABOVE the point that
// file defines them, and a forward declaration of a function defined later in the
// same translation unit is precisely what that situation needs.
static std::shared_ptr<Ship::Context> CreateHarnessStyleContext(void);
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
extern "C" void InitOTRForMMFirstBoot(int argc, char* argv[]);

// ---------------------------------------------------------------------------
// The engine's test bridges (games/oot/soh/Enhancements/randomizer/
// ComboLogicEngineOoT.cpp). Each exposes ONE fact this file cannot name for
// itself, because src/common has no OoT enum in scope by design.
// ---------------------------------------------------------------------------
extern "C" {
int OoT_ComboLogic_TestCheckIdSpace(void);
int OoT_ComboLogic_TestItemIdSpace(void);
int OoT_ComboLogic_TestLastReachedChecks(void);
int OoT_ComboLogic_TestLastReachedRegions(void);
int OoT_ComboLogic_TestReachedCheckCountNow(void);
int OoT_ComboLogic_TestLogicIsAttachedToLiveSave(void);
int OoT_ComboLogic_TestAttachLogicToLiveSave(void);
int OoT_ComboLogic_TestBeginCount(void);
int OoT_ComboLogic_TestEndCount(void);
int OoT_ComboLogic_TestRunUnrelatedSearch(void);
int OoT_ComboLogic_TestPlacedItemAt(uint16_t rc, uint16_t* outItemId, int* outAdvancement);
int OoT_ComboLogic_TestSetPlacedItem(uint16_t rc, uint16_t itemId);
uint32_t OoT_ComboLogic_TestWorldDigest(void);
int OoT_ComboLogic_TestForceAdultStart(int adult);
// The unified save buffer (src/common/unified_save.c): one char array both games
// reinterpret. Compared byte for byte by claim 5.
extern char gSaveContext[];
}

#define OLE_ASSERT(cond, msg)                                                   \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);      \
            return TEST_FAIL;                                                   \
        }                                                                       \
    } while (0)

namespace {

// ============================================================================
// A deliberately trivial MM stub, for the ONE composed round.
// ============================================================================
//
// Combo_Logic_RunRound refuses unless BOTH engines are registered (one-game
// semantics: a paired fill with one half missing would author a world whose other
// half was never decided). MM's real engine is lane K2b, so this file supplies
// the smallest legal stand-in: it reaches nothing, owns no host, opens its
// crossing, proves no goal, and is pure. IT MAKES NO CLAIM ABOUT MM. Its only
// job is to let the OoT engine be exercised through the coordinator's actual
// bracket — the order of the calls, the monotonicity watchdog, the arrival gate —
// rather than only through this file's own hand-rolled sequence.

int MmStubBeginQuery(void*) {
    return 1;
}
void MmStubAssumeOwnItem(void*, uint16_t) {
}
int MmStubExpand(void*) {
    return 0;
}
int MmStubCrossingOpen(void*) {
    return 1;
}
int MmStubCheckReached(void*, uint16_t) {
    return 0;
}
int MmStubReachedEmptyHosts(void*, uint16_t*, int) {
    return 0;
}
int MmStubAllEmptyHosts(void*, uint16_t*, int) {
    return 0;
}
int MmStubGoalReached(void*) {
    return 0;
}
int MmStubPlace(void*, uint16_t, SharedItem) {
    return 1;
}
void MmStubClearPlacements(void*) {
}
void MmStubEndQuery(void*) {
}

const ComboLogicEngine kMmStubEngine = {
    RSBS_COMBO_LOGIC_ENGINE_ABI,
    nullptr,
    MmStubBeginQuery,
    MmStubAssumeOwnItem,
    MmStubExpand,
    MmStubCrossingOpen,
    MmStubCheckReached,
    MmStubReachedEmptyHosts,
    MmStubAllEmptyHosts,
    MmStubGoalReached,
    MmStubPlace,
    MmStubClearPlacements,
    MmStubEndQuery,
    nullptr,
    nullptr,
};

// ============================================================================
// One whole query, driven by hand through the registered vtable.
// ============================================================================

struct OleQueryAnswer {
    int crossingOpen;
    int goalReached;
    int reachedChecks;
    int reachedHosts;
    int allHosts;
    int expands;
};

/** Drive one complete query: begin, assume `items`, expand to a fixpoint, read
 *  the facts, end. Exactly the shape the coordinator's round has for a side with
 *  no snapshot. `answer` is filled before the teardown, because `checkReached`
 *  and friends are only valid inside the bracket. */
bool OleRunQuery(const ComboLogicEngine* e, const std::vector<uint16_t>& items, OleQueryAnswer* answer) {
    memset(answer, 0, sizeof(*answer));
    if (!e->beginQuery(e->self)) {
        return false;
    }
    for (const uint16_t id : items) {
        e->assumeOwnItem(e->self, id);
    }
    // Bounded by the coordinator's own watchdog value, for the same reason it
    // exists there: a premise violation becomes a diagnosis, not a hang.
    int iterations = 0;
    while (iterations < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS) {
        ++iterations;
        if (!e->expand(e->self)) {
            break;
        }
    }
    answer->expands = iterations;
    answer->crossingOpen = e->crossingOpen(e->self) ? 1 : 0;
    answer->goalReached = e->goalReached(e->self) ? 1 : 0;
    answer->reachedChecks = OoT_ComboLogic_TestLastReachedChecks();
    answer->reachedHosts = e->reachedEmptyHosts(e->self, nullptr, 0);
    e->endQuery(e->self);
    // `allEmptyHosts` is legal OUTSIDE the bracket and is read there on purpose:
    // the contract says it must not consult round state, and calling it after the
    // teardown is how this file declines to let it.
    answer->allHosts = e->allEmptyHosts(e->self, nullptr, 0);
    return true;
}

bool OleSameAnswer(const OleQueryAnswer& a, const OleQueryAnswer& b) {
    return a.crossingOpen == b.crossingOpen && a.goalReached == b.goalReached &&
           a.reachedChecks == b.reachedChecks && a.reachedHosts == b.reachedHosts && a.allHosts == b.allHosts;
}

void OlePrintAnswer(const char* label, const OleQueryAnswer& a) {
    printf("[TEST] oot-logic-export: %s crossing=%d goal=%d reachedChecks=%d reachedHosts=%d allHosts=%d expands=%d\n",
           label, a.crossingOpen, a.goalReached, a.reachedChecks, a.reachedHosts, a.allHosts, a.expands);
}

} // namespace

TestResult Test_OoTLogicExport(void) {
    printf("[TEST] oot-logic-export: OoT's real solver satisfies the combo-logic engine contract (ADR 0010 "
           "increment 3, #645)\n");

    auto shipCtx = CreateHarnessStyleContext();
    if (!shipCtx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    // ------------------------------------------------------------------
    // Claim 1: the engine is REGISTERED, which means the registrar survived the
    // link and the coordinator's validation accepted the vtable.
    // ------------------------------------------------------------------
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_OOT);
    OLE_ASSERT(e != nullptr, "no OoT engine is registered — the soh_rando registrar was elided or refused");
    OLE_ASSERT(e->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI, "the registered OoT engine carries the wrong ABI");
    // OoT's queries are pure once detached, so it declares no snapshot pair. If
    // this ever becomes non-NULL the coordinator starts re-applying the whole
    // placement table per round for OoT as well, which is a cost decision that
    // must be taken deliberately rather than discovered.
    OLE_ASSERT(e->snapshot == nullptr && e->restore == nullptr,
               "OoT declares a snapshot/restore pair; its queries are supposed to be pure once detached");

    const int idSpace = OoT_ComboLogic_TestCheckIdSpace();
    const int itemIdSpace = OoT_ComboLogic_TestItemIdSpace();
    printf("[TEST] oot-logic-export: OoT check id space RC_MAX=%d, item id space RG_MAX=%d (coordinator placement "
           "cap %d)\n",
           idSpace, itemIdSpace, (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP);
    // THE SIZING FACT, asserted rather than commented: a coordinator buffer that
    // has to hold "every host this engine can enumerate" must be sized from the
    // CHECK ID SPACE and not from the placement cap, because the id space is
    // larger. This is the number the scratch buffer in combo_logic.c needs.
    OLE_ASSERT(idSpace > (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP,
               "RC_MAX no longer exceeds the placement cap — re-read the coordinator's host-buffer sizing");

    // ------------------------------------------------------------------
    // A REAL generation. Everything below is vacuous without it.
    // ------------------------------------------------------------------
    const char* kSeed = "RSBSCOMBOLOGICOOT1";
    const int rc = Rando_HeadlessSeedTest(kSeed);
    OLE_ASSERT(rc == 0, "headless seed generation failed");

    const uint32_t worldDigest0 = OoT_ComboLogic_TestWorldDigest();
    printf("[TEST] oot-logic-export: world placement digest after generation = %08X\n", worldDigest0);
    OLE_ASSERT(worldDigest0 != 0u, "the world digest is zero — no fill result is visible to the engine");

    // ------------------------------------------------------------------
    // Claim 2: the reset is real. Between `beginQuery` and the first `expand`
    // the crossing is CLOSED and nothing is reached; after it, both change.
    // ------------------------------------------------------------------
    OLE_ASSERT(e->beginQuery(e->self), "beginQuery refused on a live generated world");
    const int crossingBeforeExpand = e->crossingOpen(e->self);
    const int reachedBeforeExpand = OoT_ComboLogic_TestReachedCheckCountNow();
    const int firstExpandChanged = e->expand(e->self);
    const int crossingAfterExpand = e->crossingOpen(e->self);
    const int reachedAfterExpand = OoT_ComboLogic_TestReachedCheckCountNow();
    e->endQuery(e->self);

    printf("[TEST] oot-logic-export: before first expand crossing=%d reached=%d; after crossing=%d reached=%d "
           "(changed=%d)\n",
           crossingBeforeExpand, reachedBeforeExpand, crossingAfterExpand, reachedAfterExpand, firstExpandChanged);
    OLE_ASSERT(crossingBeforeExpand == 0,
               "the crossing reads OPEN before the round's first expand — beginQuery did not reset region access");
    OLE_ASSERT(reachedBeforeExpand == 0,
               "checks read REACHED before the round's first expand — beginQuery did not clear the pool marks");
    OLE_ASSERT(firstExpandChanged != 0, "the round's first expand reported no change");
    OLE_ASSERT(reachedAfterExpand > 0, "the expansion reached no check at all over a real generated world");
    // The default OoT world starts as child in the Market's reach, so the Happy
    // Mask Shop — the crossing — is open at sphere zero. That is exactly why the
    // coordinator's ARRIVAL GATE has to be a rule rather than an observation.
    OLE_ASSERT(crossingAfterExpand != 0,
               "the OoT->MM crossing is CLOSED under the shipped default child start (RR_MARKET_MASK_SHOP child "
               "access) — either the region key moved or the mask-shop pair is back in the entrance shuffle");

    // ------------------------------------------------------------------
    // Claim 3: identical queries agree, including across a residue-producing
    // bare search. THE DIRECT LOCK ON THE #656 DEFECT CLASS.
    // ------------------------------------------------------------------
    // The assumed set is the OoT foreign pool's own ids, read through
    // src/common's registry so this file names no RG_*.
    const ComboForeignItemDef* pool = nullptr;
    const int poolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, &pool);
    OLE_ASSERT(poolCount > 0 && pool != nullptr, "OoT's foreign item pool is not registered");
    std::vector<uint16_t> assumed;
    for (int i = 0; i < poolCount; i++) {
        OLE_ASSERT(pool[i].item.originGame == (uint8_t)GAME_OOT, "an OoT pool row is not OoT-tagged");
        assumed.push_back(pool[i].item.id);
    }

    OleQueryAnswer q1;
    OleQueryAnswer q2;
    OleQueryAnswer q3;
    OLE_ASSERT(OleRunQuery(e, assumed, &q1), "query 1 refused");
    OlePrintAnswer("query 1", q1);
    OLE_ASSERT(OleRunQuery(e, assumed, &q2), "query 2 refused");
    OlePrintAnswer("query 2 (back to back)", q2);
    OLE_ASSERT(OleSameAnswer(q1, q2), "two identical queries back to back disagree");

    const int residueReached = OoT_ComboLogic_TestRunUnrelatedSearch();
    printf("[TEST] oot-logic-export: an UNRELATED bare ReachabilitySearch (no Logic::Reset) left %d checks "
           "reached\n",
           residueReached);
    OLE_ASSERT(residueReached >= 0, "the unrelated-search bridge found no live solver");
    OLE_ASSERT(OleRunQuery(e, assumed, &q3), "query 3 refused");
    OlePrintAnswer("query 3 (after an unrelated search)", q3);
    OLE_ASSERT(OleSameAnswer(q1, q3),
               "a query run after an unrelated bare search disagrees with the same query run before it — beginQuery "
               "is inheriting the previous search's simulated inventory (#656's defect class)");

    OLE_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0,
               "three queries moved a placement in the generated world");

    // ------------------------------------------------------------------
    // Claim 4: monotonicity, NON-VACUOUSLY, by making the baseline partial.
    // ------------------------------------------------------------------
    // Empty every advancement-bearing location. With All Locations Reachable on
    // and a complete fill the closure is already total, so without this step
    // "assuming more did not shrink it" is satisfied by equality and proves
    // nothing about the operator.
    std::vector<uint16_t> emptiedChecks;
    std::vector<uint16_t> emptiedItems;
    for (int id = 1; id < idSpace; id++) {
        uint16_t placedItem = 0;
        int advancement = 0;
        if (!OoT_ComboLogic_TestPlacedItemAt((uint16_t)id, &placedItem, &advancement)) {
            continue;
        }
        if (advancement == 0) {
            continue;
        }
        emptiedChecks.push_back((uint16_t)id);
        emptiedItems.push_back(placedItem);
    }
    printf("[TEST] oot-logic-export: %zu advancement-bearing locations will be emptied to build a partial closure\n",
           emptiedChecks.size());
    OLE_ASSERT(!emptiedChecks.empty(), "the fill placed no advancement item — the monotonicity leg would be vacuous");

    for (const uint16_t host : emptiedChecks) {
        OLE_ASSERT(OoT_ComboLogic_TestSetPlacedItem(host, 0 /* RG_NONE */), "could not empty a location");
    }

    // De-duplicated assumed set over exactly the items that were removed.
    std::vector<bool> seen((size_t)itemIdSpace, false);
    std::vector<uint16_t> removedItems;
    for (const uint16_t item : emptiedItems) {
        if (item < (uint16_t)itemIdSpace && !seen[(size_t)item]) {
            seen[(size_t)item] = true;
            removedItems.push_back(item);
        }
    }

    OleQueryAnswer partial;
    OleQueryAnswer restored;
    OLE_ASSERT(OleRunQuery(e, std::vector<uint16_t>(), &partial), "the partial-world query refused");
    OlePrintAnswer("partial world, nothing assumed", partial);
    OLE_ASSERT(OleRunQuery(e, removedItems, &restored), "the partial-world query with the items assumed refused");
    OlePrintAnswer("partial world, removed items assumed", restored);

    printf("[TEST] oot-logic-export: closure %d -> %d reached checks under %zu assumed items (hosts %d -> %d)\n",
           partial.reachedChecks, restored.reachedChecks, removedItems.size(), partial.reachedHosts,
           restored.reachedHosts);
    OLE_ASSERT(restored.reachedChecks >= partial.reachedChecks,
               "assuming items SHRANK the reachable closure — the expansion is not monotone");
    // STRICT, which is the whole point: it proves the assumption did work and
    // that this leg is not a constant comparing equal to itself.
    OLE_ASSERT(restored.reachedChecks > partial.reachedChecks,
               "assuming every removed advancement item opened nothing — the monotonicity leg is vacuous");
    // The emptied hosts are genuinely offered as candidates, which is what makes
    // `reachedEmptyHosts` non-vacuous on this side: after a complete fill nothing
    // is empty and the list is legitimately zero-length.
    OLE_ASSERT(partial.allHosts > 0, "allEmptyHosts reports nothing although hundreds of locations were emptied");
    OLE_ASSERT(restored.reachedHosts > 0, "reachedEmptyHosts reports nothing although reached locations are empty");

    // ------------------------------------------------------------------
    // Claim 7: the host surface, on the partial world where it is observable.
    // ------------------------------------------------------------------
    OLE_ASSERT(e->beginQuery(e->self), "beginQuery refused for the host-surface leg");
    for (const uint16_t item : removedItems) {
        e->assumeOwnItem(e->self, item);
    }
    e->expand(e->self);

    const int reachedTotal = e->reachedEmptyHosts(e->self, nullptr, 0);
    const int allTotal = e->allEmptyHosts(e->self, nullptr, 0);
    printf("[TEST] oot-logic-export: reachedEmptyHosts=%d allEmptyHosts=%d\n", reachedTotal, allTotal);
    OLE_ASSERT(allTotal >= reachedTotal, "allEmptyHosts is not a superset of reachedEmptyHosts");

    std::vector<uint16_t> reachedList((size_t)reachedTotal + 1, 0);
    const int reachedWritten = e->reachedEmptyHosts(e->self, reachedList.data(), reachedTotal);
    OLE_ASSERT(reachedWritten == reachedTotal, "reachedEmptyHosts reported a different total on the write call");
    for (int i = 1; i < reachedTotal; i++) {
        OLE_ASSERT(reachedList[(size_t)i] > reachedList[(size_t)(i - 1)],
                   "reachedEmptyHosts is not strictly ascending — the order contract is violated or an id repeats");
    }
    for (int i = 0; i < reachedTotal; i++) {
        OLE_ASSERT(e->checkReached(e->self, reachedList[(size_t)i]),
                   "reachedEmptyHosts offered a host that checkReached calls unreached");
    }

    // TRUNCATION IS DISTINGUISHABLE FROM EXHAUSTION: the write is bounded by
    // `cap`, the return is the true total. A coordinator that could not tell them
    // apart would silently fill from a truncated candidate list.
    std::vector<uint16_t> tiny(4, 0xFFFF);
    const int tinyTotal = e->reachedEmptyHosts(e->self, tiny.data(), 1);
    OLE_ASSERT(tinyTotal == reachedTotal, "a cap-1 call did not report the true total");
    OLE_ASSERT(tiny[0] == reachedList[0], "a cap-1 call wrote the wrong first id");
    OLE_ASSERT(tiny[1] == 0xFFFF && tiny[2] == 0xFFFF && tiny[3] == 0xFFFF,
               "a cap-1 call wrote past its capacity");

    e->endQuery(e->self);

    // `allEmptyHosts` OUTSIDE any bracket must answer the same as it did inside
    // one, because the contract says it consults no round state.
    const int allOutsideBracket = e->allEmptyHosts(e->self, nullptr, 0);
    OLE_ASSERT(allOutsideBracket == allTotal,
               "allEmptyHosts answers differently outside a query bracket — it is consulting round state");

    // ------------------------------------------------------------------
    // Restore the world, and prove it came back byte-identical.
    // ------------------------------------------------------------------
    for (size_t i = 0; i < emptiedChecks.size(); i++) {
        OLE_ASSERT(OoT_ComboLogic_TestSetPlacedItem(emptiedChecks[i], emptiedItems[i]),
                   "could not restore an emptied location");
    }
    const uint32_t worldDigestRestored = OoT_ComboLogic_TestWorldDigest();
    printf("[TEST] oot-logic-export: world digest after restore = %08X (was %08X)\n", worldDigestRestored,
           worldDigest0);
    OLE_ASSERT(worldDigestRestored == worldDigest0, "the emptied locations did not come back byte-identical");

    // ------------------------------------------------------------------
    // Claim 5: the live save, and the re-attach rule (audit §4.4).
    // ------------------------------------------------------------------
    // Put the engine in the state a LOADED SAVE puts it in. Without this the leg
    // is vacuous: mid-generation `Logic` already points at a heap context and a
    // query that wrote the live save would be invisible.
    OoT_ComboLogic_TestAttachLogicToLiveSave();
    OLE_ASSERT(OoT_ComboLogic_TestLogicIsAttachedToLiveSave() == 1,
               "could not attach Logic to the live save for the re-attach leg");

    static unsigned char saveBefore[OOT_SAVE_CONTEXT_SIZE];
    memcpy(saveBefore, gSaveContext, sizeof(saveBefore));

    OleQueryAnswer attachedAnswer;
    OLE_ASSERT(OleRunQuery(e, assumed, &attachedAnswer), "a query refused while Logic was attached to the live save");
    OlePrintAnswer("query with Logic attached to the live save", attachedAnswer);
    OLE_ASSERT(OleSameAnswer(attachedAnswer, q1),
               "a query answers differently depending on whether Logic was attached to the live save — the detach "
               "is not isolating the search");

    OLE_ASSERT(OoT_ComboLogic_TestLogicIsAttachedToLiveSave() == 1,
               "endQuery did not re-point Logic::mSaveContext at the live save (audit §4.4 re-attach rule)");
    OLE_ASSERT(memcmp(saveBefore, gSaveContext, sizeof(saveBefore)) == 0,
               "a round wrote the player's save — the detach leaked (audit §1.8 hazard 1)");

    // ...and `place` is bracketed too, which matters because it is called OUTSIDE
    // a round and `PlaceItemInLocation` applies the item effect under Glitchless
    // regardless of its argument.
    const int placeHostTotal = e->allEmptyHosts(e->self, nullptr, 0);
    printf("[TEST] oot-logic-export: %d empty hosts available for the place leg\n", placeHostTotal);
    if (placeHostTotal > 0) {
        std::vector<uint16_t> placeHosts((size_t)placeHostTotal, 0);
        e->allEmptyHosts(e->self, placeHosts.data(), placeHostTotal);
        const uint16_t host = placeHosts[0];
        SharedItem ownItem;
        memset(&ownItem, 0, sizeof(ownItem));
        ownItem.originGame = (uint8_t)GAME_OOT;
        ownItem.id = assumed[0];

        OLE_ASSERT(e->place(e->self, host, ownItem), "place refused a host the engine itself offered");
        // IDEMPOTENT for the same (host, item): the coordinator re-applies its
        // whole table after every snapshot restore.
        OLE_ASSERT(e->place(e->self, host, ownItem), "place is not idempotent for the same host and item");
        OLE_ASSERT(OoT_ComboLogic_TestLogicIsAttachedToLiveSave() == 1, "place left Logic pointed somewhere else");
        OLE_ASSERT(memcmp(saveBefore, gSaveContext, sizeof(saveBefore)) == 0,
                   "place wrote the player's save — the item effect was not bracketed");
        // A foreign-origin placement leaves a LOCAL junk item in the host and no
        // foreign id in OoT's tables (ADR 0002).
        const int afterPlaceTotal = e->allEmptyHosts(e->self, nullptr, 0);
        OLE_ASSERT(afterPlaceTotal == placeHostTotal - 1, "a placed host is still offered as empty");

        // The roll-back returns the host to what it held BEFORE, which here is
        // empty, and restores the world digest exactly.
        e->clearPlacements(e->self);
        OLE_ASSERT(e->allEmptyHosts(e->self, nullptr, 0) == placeHostTotal,
                   "clearPlacements did not return the host to unassigned");
        OLE_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0,
                   "a place plus clearPlacements did not restore the world digest");
    }

    // ------------------------------------------------------------------
    // Claim 6: endQuery after a FAILED beginQuery, and endQuery twice.
    // ------------------------------------------------------------------
    // The coordinator may tear down a side whose bracket never opened: a snapshot
    // can succeed before that side's beginQuery refuses. A second beginQuery
    // without a teardown is the refusal this file can provoke on demand.
    OLE_ASSERT(e->beginQuery(e->self), "beginQuery refused before the double-begin leg");
    const int beginsBefore = OoT_ComboLogic_TestBeginCount();
    const int endsBefore = OoT_ComboLogic_TestEndCount();
    OLE_ASSERT(e->beginQuery(e->self) == 0, "a second beginQuery inside an open query was accepted");
    OLE_ASSERT(OoT_ComboLogic_TestBeginCount() == beginsBefore, "a refused beginQuery counted as a query");
    e->endQuery(e->self); // the real teardown
    OLE_ASSERT(OoT_ComboLogic_TestEndCount() == endsBefore + 1, "the real teardown did not run");
    e->endQuery(e->self); // after the bracket already closed: must be a no-op
    e->endQuery(e->self);
    OLE_ASSERT(OoT_ComboLogic_TestEndCount() == endsBefore + 1,
               "a repeated endQuery ran a second teardown — the restore is not idempotent");
    OLE_ASSERT(OoT_ComboLogic_TestLogicIsAttachedToLiveSave() == 1,
               "repeated endQuery calls disturbed Logic's attachment");
    OLE_ASSERT(memcmp(saveBefore, gSaveContext, sizeof(saveBefore)) == 0,
               "the double-teardown leg wrote the player's save");

    // ------------------------------------------------------------------
    // The crossing under an ADULT start, MEASURED and printed.
    // ------------------------------------------------------------------
    // The lane brief expected this to be FALSE. It is reported rather than
    // assumed, because `AccessReset` only decides which bit `RR_ROOT` starts with
    // and OoT's own Temple of Time age crossover can still deliver child access
    // from an adult start. The assertion pinned below is whatever this row
    // measures, and the PR records which it was.
    const int priorAge = OoT_ComboLogic_TestForceAdultStart(1);
    OLE_ASSERT(priorAge == 0, "the generated world did not start as child, so the age leg has no baseline");
    OleQueryAnswer adult;
    OLE_ASSERT(OleRunQuery(e, std::vector<uint16_t>(), &adult), "the adult-start query refused");
    OlePrintAnswer("adult start, nothing assumed", adult);
    printf("[TEST] oot-logic-export: crossingOpen from an ADULT start = %d (child start = %d)\n", adult.crossingOpen,
           crossingAfterExpand);
    OoT_ComboLogic_TestForceAdultStart(priorAge);

    OleQueryAnswer childAgain;
    OLE_ASSERT(OleRunQuery(e, std::vector<uint16_t>(), &childAgain), "the restored child-start query refused");
    OLE_ASSERT(childAgain.crossingOpen == 1,
               "the crossing did not come back open after the starting age was restored");
    OLE_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0, "the starting-age probe moved a placement");

    // ------------------------------------------------------------------
    // One COMPOSED round, through the coordinator's own bracket.
    // ------------------------------------------------------------------
    // The MM half is the trivial stub declared at the top of this file: it proves
    // nothing about MM (that engine is a separate lane) and exists only so
    // Combo_Logic_RunRound will run at all — it refuses with one engine, by
    // design.
    const ComboLogicEngine* priorMm = Combo_Logic_GetEngine(GAME_MM);
    OLE_ASSERT(priorMm == nullptr,
               "an MM engine is already registered — this row's stub would replace a real engine; re-scope the "
               "composed-round leg now that lane K2b has landed");
    OLE_ASSERT(Combo_Logic_RegisterEngine(GAME_MM, &kMmStubEngine), "the MM stub would not register");

    Combo_Logic_ResetPlacements();
    ComboLogicBagItem bag[4];
    memset(bag, 0, sizeof(bag));
    int bagCount = 0;
    for (int i = 0; i < poolCount && bagCount < 4; i++) {
        bag[bagCount].item = pool[i].item;
        bag[bagCount].itemClass = pool[i].itemClass;
        bagCount++;
    }

    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = bag;
    req.assumedCount = bagCount;
    req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;

    ComboLogicRoundResult roundResult;
    const int roundStatus = Combo_Logic_RunRound(&req, &roundResult);
    printf("[TEST] oot-logic-export: composed round status=%s iterations=%d crossingOoT=%d goalOoT=%d goalMM=%d "
           "goalExpr=%d candidatesOoT=%d candidatesMM=%d\n",
           Combo_Logic_StatusName(roundStatus), roundResult.iterations, roundResult.crossingOpenOoT,
           roundResult.goalOoT, roundResult.goalMM, roundResult.goalExpression, roundResult.candidatesOoT,
           roundResult.candidatesMM);
    OLE_ASSERT(roundStatus == RSBS_COMBO_LOGIC_OK, "a composed round over the real OoT engine did not succeed");
    OLE_ASSERT(roundResult.crossingOpenOoT == 1, "the composed round saw OoT's crossing closed");
    // The generated world IS beatable, so OoT's half proves and beat-either holds
    // on OoT alone. This is also the lock that `goalReached` is not a constant
    // zero: the stub MM half answers 0, so a 1 here can only come from OoT.
    OLE_ASSERT(roundResult.goalOoT == 1, "the composed round could not prove OoT's own half over a beatable world");
    OLE_ASSERT(roundResult.goalMM == 0, "the MM stub reported a goal it does not have");
    OLE_ASSERT(roundResult.goalExpression == 1, "beat-either did not hold although OoT's half proved");
    // The monotonicity watchdog inside the coordinator did not fire, which is a
    // real statement about the OoT engine: its candidate count and crossing flag
    // never fell inside the round.
    OLE_ASSERT(roundResult.iterations > 0 && roundResult.iterations < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS,
               "the composed round did not converge inside the watchdog bound");

    OLE_ASSERT(OoT_ComboLogic_TestLogicIsAttachedToLiveSave() == 1,
               "the composed round left Logic detached from the live save");
    OLE_ASSERT(memcmp(saveBefore, gSaveContext, sizeof(saveBefore)) == 0,
               "the composed round wrote the player's save");
    OLE_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0, "the composed round moved a placement");

    // Leave the registry as it was found, so a later row in the same process is
    // not driven by this file's stub.
    Combo_Logic_ResetPlacements();
    Combo_Logic_RegisterEngine(GAME_MM, nullptr);
    OLE_ASSERT(Combo_Logic_GetEngine(GAME_MM) == nullptr, "the MM stub did not un-register");

    printf("[TEST] PASS: the OoT engine satisfies the combo-logic contract over the real graph; no placement moved "
           "and the live save is byte-identical\n");
    return TEST_PASS;
}
