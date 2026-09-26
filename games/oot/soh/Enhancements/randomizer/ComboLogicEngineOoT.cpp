/**
 * ComboLogicEngineOoT.cpp — the OoT half of the combo-logic coordinator's
 * engine surface (ADR 0010 increment 3, #645; O4 = COMPOSITION).
 *
 * ============================================================================
 * WHAT THIS IS
 * ============================================================================
 *
 * `src/common/combo_logic.h` declares a `ComboLogicEngine` vtable: eleven
 * required calls plus an optional snapshot/restore pair, over game-neutral
 * scalars only. This TU implements that vtable for OoT out of the primitives
 * `3drando` already has, and registers it at process start. It is the ONE place
 * besides ForeignItemsSingleExe.cpp where `RG_*` / `RC_*` / `RR_*` may be in
 * scope on the OoT side (ADR 0002): everything leaves here as a `SharedItem`,
 * a host check id, or a plain int.
 *
 * **NOTHING HERE RUNS IN A PRODUCTION PATH.** Registering an engine does not
 * call it: no creation seam, no `Fill()` and no arrival invokes
 * `Combo_Logic_RunRound` or `Combo_Logic_RunFill` at this commit, and the
 * coordinator refuses any paired fill until BOTH engines are registered (MM's
 * is a separate lane). The registrar below therefore changes no generated
 * world — see the PR's world-unchanged evidence.
 *
 * ============================================================================
 * THE MAPPING, CALL BY CALL, AND WHY EACH ONE IS THE PRIMITIVE IT IS
 * ============================================================================
 *
 *   beginQuery        `Logic::Reset(true)` — THE DETACH. `NewSaveContext()`
 *                     allocates a fresh heap `SaveContext` and points
 *                     `Logic::mSaveContext` at it, so the whole round's
 *                     simulated inventory lives off the live save
 *                     (audit §1.8 / §4.2). Then `Regions::AccessReset()` and
 *                     `ctx->LocationReset()`, so the reached set is defined
 *                     (empty) even before the first `expand`.
 *   assumeOwnItem     `Item::ApplyEffect()` on the item the `RG_*` names, into
 *                     the detached save. DE-DUPLICATED per round — see the
 *                     block at OoT_ComboLogic_AssumeOwnItem, which is the one
 *                     place this implementation could not take the contract
 *                     literally without corrupting the inventory.
 *   expand            one `ReachabilitySearch(ctx->allLocations)`. Returns
 *                     whether the closure GREW since this round's previous
 *                     expand, measured on (#reached checks, #reached regions).
 *   crossingOpen      `RegionTable(RR_MARKET_MASK_SHOP)->Child()` (audit §4.5).
 *                     The Happy Mask Shop pair is pinned out of OoT's own
 *                     entrance shuffle, so this region key is the crossing in
 *                     every world.
 *   checkReached      `ItemLocation::IsAddedToPool()`.
 *   reachedEmptyHosts reached, item-free, coordinator-unassigned checks in
 *                     ASCENDING `RC_*` order.
 *   allEmptyHosts     the same set WITHOUT the reachability term, and without
 *                     consulting any round state — the `none` rung's host
 *                     source, legal outside a bracket.
 *   goalReached       `RG_TRIFORCE` REACHED, which is `CheckBeatable`'s own
 *                     condition (`fill.cpp:445`) read as a fact instead of
 *                     recomputed — `CheckBeatable` itself calls `ResetLogic`
 *                     and would throw the round's state away, and the contract
 *                     requires a pure read.
 *   place             OoT-origin: `Context::PlaceItemInLocation`. MM-origin: a
 *                     fixed junk cover and nothing else — the foreign identity
 *                     stays in the coordinator's table, never in OoT's. The call
 *                     is bracketed over BOTH halves of what `Item::ApplyEffect`
 *                     writes — the save context AND `Logic::inLogic` — see the
 *                     block at OoT_ComboLogic_Place.
 *   clearPlacements   restore every host this engine was given back to the item
 *                     it held BEFORE the coordinator touched it.
 *   endQuery          re-point `Logic::mSaveContext` where it was, which is the
 *                     audit's §4.4 re-attach obligation.
 *   snapshot/restore  BOTH NULL. OoT's queries are pure once detached, so there
 *                     is nothing to bracket; supplying a pair that copied
 *                     `gSaveContext` would cost 136 KB per round to restore
 *                     bytes no query writes.
 *
 * ============================================================================
 * THE THREE HAZARDS THIS FILE EXISTS TO NOT FALL INTO
 * ============================================================================
 *
 * (1) THE RESIDUE. `ReachabilitySearch` does NOT reset the `logic` singleton's
 *     simulated inventory: `ResetLogic` clears region bits and pool marks and
 *     then ADDS the starting inventory on top of whatever the last search left
 *     collected (`fill.cpp:315-329`). A round that skipped `Logic::Reset(true)`
 *     would answer a question about a world holding strictly more items than it
 *     was asked about, silently, and it looks like the fill working. This is not
 *     hypothetical in this tree: the #656 reverse-placement gate reported 48 of
 *     57 hosts reachable against a truth of 57 for exactly this reason. So
 *     `beginQuery` resets — and `expand` resets again, because it re-derives the
 *     round's inventory per call (see the block there).
 *
 *     WHICH RESET THE LOCKS ATTRIBUTE, precisely, because review caught the first
 *     version of this comment over-claiming. The three-answers-agree leg (same
 *     query twice, then once after an unrelated bare search) is satisfied by
 *     EITHER reset: deleting `beginQuery`'s leaves it green, because `expand`
 *     rebuilds the baseline before any answer is read. So that leg locks "some
 *     reset happens before an answer is read", which is the property the
 *     coordinator actually depends on. `beginQuery`'s own reset is attributed by a
 *     SEPARATE leg: with residue deliberately present, the simulated inventory is
 *     read between `beginQuery` and the first `expand` and must be empty — an
 *     assertion nothing but this `Reset(true)` can satisfy.
 *
 * (2) THE LIVE SAVE. On save load the port points `Logic` at `&gSaveContext`
 *     (`logic.cpp:2164`), so a query issued without the detach applies item
 *     effects INTO the player's save. Every mutating primitive this file calls
 *     is therefore inside the detach — including `place`, which is called
 *     OUTSIDE a round (see the bracket at OoT_ComboLogic_Place).
 *
 * (3) THE RE-ATTACH. `endQuery` must leave `Logic::mSaveContext` pointing where
 *     `beginQuery` found it, or the next in-game progressive give resolves
 *     against a stale simulated save (audit §1.8 hazard 1, §4.4). Two cases, and
 *     only one of them is restorable: if it pointed at `&gSaveContext` we
 *     re-point there exactly (and release the round's heap copy, or the fill
 *     leaks ~136 KB per round); if it pointed at a HEAP context — i.e. a
 *     generation was mid-flight — `Logic::Reset(true)`'s own `NewSaveContext()`
 *     already `free()`d it (`logic.cpp:2310-2315`), so that pointer cannot be
 *     restored by anyone and the correct end state is the fresh detached context
 *     we leave. That is the same state OoT's own `AssumedFill` leaves between
 *     its per-item `logic->Reset()` calls, so a mid-generation caller is
 *     unaffected. Stated rather than silently reinterpreted.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "3drando/fill.hpp"

#include "combo_logic.h" // src/common — the vtable this file implements
#include "context.h"     // src/common — SharedItem, GameId

extern "C" {
#include <z64.h>
#include "variables.h"
}

namespace {

// ============================================================================
// Engine state. File-static, so `self` is NULL — which combo_logic.h documents
// as what a real engine passes (only a test needs `self`).
// ============================================================================

/** The check id space: the range the enumerations WALK, and the bound on any id
 *  they can emit. It is NOT the widest answer they can give — review caught that
 *  over-claim. The widest answer is |`ctx->allLocations`| (see
 *  OoTComboLogicOwnedHosts), a settings-dependent number strictly smaller than
 *  this, and it is that number a coordinator host buffer must be sized from. The
 *  lock measures it. */
constexpr int kOoTCheckIdSpace = (int)RC_MAX;

/** The junk an MM-origin placement leaves physically in the OoT host. FIXED, not
 *  drawn: `GetJunkItem()` consumes the fill's RNG stream (`item_pool.cpp:70-77`)
 *  and an engine call must consume none — a query runs a variable number of
 *  times per fill, so one draw in here would make the world a function of the
 *  search's shape (combo_logic.h, "AN ENGINE MUST LEAVE UNTOUCHED"). */
constexpr RandomizerGet kOoTForeignJunkCover = RG_BLUE_RUPEE;

struct OoTComboPlacement {
    RandomizerCheck host;
    RandomizerGet priorItem; // what the host held before the coordinator arrived
    SharedItem item;         // origin-tagged; may be MM-origin
};

/** Hosts the COORDINATOR gave this engine, and nothing else. `clearPlacements`
 *  restores exactly these and touches nothing it did not put there. */
OoTComboPlacement sPlacements[RSBS_COMBO_LOGIC_PLACEMENT_CAP];
int sPlacementCount = 0;

/** In-query state. `sInQuery` is what makes `endQuery` idempotent and safe after
 *  a FAILED `beginQuery`: the coordinator may call the teardown on a side whose
 *  bracket never opened, and a teardown that re-pointed the save context on that
 *  path would "restore" a pointer nobody saved. */
bool sInQuery = false;
/** True iff `beginQuery` found `Logic::mSaveContext == &gSaveContext`. Only then
 *  is the prior pointer restorable — see hazard (3) in the file header. */
bool sPriorWasLiveSave = false;

/** Items granted THIS round: a membership bitset (the de-dup) and the ORDER they
 *  arrived in (what `expand` re-applies). See the de-dup block and the
 *  re-derivation block in `expand`. */
std::vector<bool> sGranted;
std::vector<uint16_t> sGrantedOrder;

/** Closure size at this round's previous `expand`, for the `changed` answer. */
int sPrevReachedChecks = -1;
int sPrevReachedRegions = -1;

/** Counters the locks read; production never does. */
int sLastExpandReachedChecks = 0;
int sLastExpandReachedRegions = 0;
int sBeginQueryCount = 0;
int sEndQueryCount = 0;

// ============================================================================
// Small helpers
// ============================================================================

Rando::Logic* OoTComboLogicSingleton() {
    // The free `logic` pointer the region files and `fill.cpp` use is rebound by
    // RegionTable_Init; going through it (rather than through
    // Rando::Context::GetLogic()) is what keeps this engine reading the SAME
    // singleton every guard in location_access/ reads. It is a
    // `std::shared_ptr<Rando::Logic>` there (`location_access.h:20`); this engine
    // only ever borrows it, never keeps it.
    return logic.get();
}

bool OoTComboLogicReady() {
    // Every mutating primitive below needs all four. `Logic::Reset(true)` in
    // particular reaches `OTRGlobals::Instance->HasOriginal()` through
    // InitSaveContext (`logic.cpp:2305`), so a process with no OTRGlobals must be
    // refused rather than crashed.
    //
    // `gRandomizer` is in the list because `Item::GetGIEntry` dereferences it
    // unconditionally for every row whose cached `giEntry` is null — the
    // progressive rows and the table's gaps (`item.cpp:111-112`,
    // `GetRandoSettingValue(RSK_INCLUDE_TYCOON_WALLET)`) — and
    // `Logic::ApplyItemEffect` calls `GetGIEntry()` on its first line
    // (`logic.cpp:1738`). So `assumeOwnItem` on a progressive id in a process
    // without a Randomizer is a null dereference, not a refusal. Added on review.
    return OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandomizer != nullptr &&
           Rando::Context::GetInstance() != nullptr && OoTComboLogicSingleton() != nullptr;
}

/** Does `rc` name a real row in the location table? A gap is default-constructed
 *  and keeps RC_UNKNOWN_CHECK, so the identity test rejects it — the same test
 *  OoT_Foreign_IsEligibleHostImpl uses, for the same reason. */
bool OoTComboLogicIsRealCheck(RandomizerCheck rc) {
    if (rc <= RC_UNKNOWN_CHECK || rc >= RC_MAX) {
        return false;
    }
    Rando::Location* loc = Rando::StaticData::GetLocation(rc);
    return loc != nullptr && loc->GetRandomizerCheck() == rc;
}

/**
 * Does `rg` name a real row in the ITEM table? The item-side counterpart of
 * OoTComboLogicIsRealCheck, added on review: the first version of this file
 * range-checked item ids and then handed them straight to
 * `Item::ApplyEffect`/`PlaceItemInLocation`, which is an identity question and not
 * a range question. `itemTable` is a `std::array<Item, RG_MAX>`
 * (`static_data.h:20`) whose unassigned rows are DEFAULT-CONSTRUCTED and keep
 * `randomizerGet == RG_NONE` (`item.cpp:15-18`), so the identity test rejects
 * exactly the gaps.
 *
 * WHAT A GAP ROW ACTUALLY DID BEFORE THIS TEST EXISTED, measured rather than
 * assumed, because the review that asked for this predicted a crash and the crash
 * is not there: `ApplyEffect` would call `ApplyItemEffect`, whose first line
 * dereferences `GetGIEntry()->objectId`; `GetGIEntry` on a null-`giEntry` row
 * takes its `default:` arm (`actual = RG_NONE`), fails the `giEntry != nullptr`
 * guard, and tail-calls `RetrieveItem(RG_NONE).GetGIEntry()` — and `RG_NONE` IS a
 * real long-constructor row (`item_list.cpp:17`), so its `giEntry` is non-null and
 * the recursion terminates in one step with a usable entry. No null dereference
 * and no unbounded recursion. What DOES happen is silent: the effect resolves
 * against `RG_NONE`'s GI entry and `ApplyEffect` then sets the row's `logicVal`,
 * which for a zero-initialised gap is `LOGIC_NONE`, i.e. `inLogic[0]` on the live
 * singleton. Cheap to refuse, so it is refused.
 *
 * NOT CHECKED, deliberately, and stated because review asked for it: a non-null
 * `GetGIEntry()` is not part of the predicate, because the only way to ask that
 * question is to CALL `GetGIEntry`, which for a gap row is the very dereference
 * chain being guarded against. The identity test dominates it — every row that
 * fails identity is a gap, and every gap is what the null-`giEntry` path is about.
 */
bool OoTComboLogicIsRealItem(RandomizerGet rg) {
    if (rg <= RG_NONE || rg >= RG_MAX) {
        return false;
    }
    return Rando::StaticData::RetrieveItem(rg).GetRandomizerGet() == rg;
}

/**
 * The hosts this engine OWNS: rows the CURRENT world's fill actually considers.
 *
 * `ctx->allLocations` is rebuilt per fill attempt (`fill.cpp:1225`), so it is a
 * function of the graph and the settings and not of anything a round does — which
 * is what the ORDER contract needs. The bitset is rebuilt on every call rather
 * than cached because a cache would silently answer for the PREVIOUS attempt's
 * world after a re-generation, which is the same class of bug `addedToPool`
 * residue already is (audit §1.8).
 */
const std::vector<bool>& OoTComboLogicOwnedHosts() {
    static std::vector<bool> owned;
    owned.assign((size_t)kOoTCheckIdSpace, false);
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return owned;
    }
    for (const RandomizerCheck rc : ctx->allLocations) {
        if (rc > RC_UNKNOWN_CHECK && rc < RC_MAX) {
            owned[(size_t)rc] = true;
        }
    }
    return owned;
}

int OoTComboLogicPlacementIndex(RandomizerCheck rc) {
    for (int i = 0; i < sPlacementCount; ++i) {
        if (sPlacements[i].host == rc) {
            return i;
        }
    }
    return -1;
}

/** Reached checks over the WHOLE id space, which is the closure observable
 *  `expand` compares round-internally. */
int OoTComboLogicReachedCheckCount() {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return 0;
    }
    int count = 0;
    for (int i = 1; i < kOoTCheckIdSpace; ++i) {
        const RandomizerCheck rc = (RandomizerCheck)i;
        if (!OoTComboLogicIsRealCheck(rc)) {
            continue;
        }
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        if (il != nullptr && il->IsAddedToPool()) {
            ++count;
        }
    }
    return count;
}

/** Reached regions, counted over the four age/time bits. A region can open for
 *  an age this expand that it did not hold last expand WITHOUT any new check
 *  becoming reachable, so counting region bits as well as checks is what stops
 *  `expand` reporting "nothing new" one alternation early. */
int OoTComboLogicReachedRegionCount() {
    int count = 0;
    // The same range `Regions::GetAllRegions()` builds (`location_access.cpp:1063`)
    // — that helper is file-local to location_access.cpp and not declared in the
    // header, so the range is written out here rather than reached for.
    for (int i = (int)RR_NONE + 1; i < (int)RR_MAX; ++i) {
        Region* region = RegionTable((RandomizerRegion)i);
        if (region == nullptr) {
            continue;
        }
        count += region->childDay ? 1 : 0;
        count += region->childNight ? 1 : 0;
        count += region->adultDay ? 1 : 0;
        count += region->adultNight ? 1 : 0;
    }
    return count;
}

/**
 * A detached `SaveContext` this engine owns, allocated once and reused, plus a
 * PRISTINE copy of it kept as the reset source.
 *
 * WHY `place` NEEDS IT. `Context::PlaceItemInLocation` applies the item's effect
 * immediately under Glitchless REGARDLESS of its `applyEffectImmediately`
 * argument (`SeedContext.cpp:141-145`), and `place` is called OUTSIDE any query
 * bracket — between rounds, and in a round's teardown. With `Logic` attached to
 * `&gSaveContext` (its state whenever a save is loaded) an unbracketed place
 * would write the item into the PLAYER'S SAVE. combo_logic.h is explicit that
 * `place` must not grant the item; this is how OoT honours that without
 * reimplementing the port's placement bookkeeping.
 *
 * WHY THERE ARE TWO, added on review. The first version said the effect lands "in
 * scratch that the next `beginQuery` wipes", and that was simply false:
 * `beginQuery`'s `Logic::Reset(true)` calls `NewSaveContext()`, which allocates a
 * FRESH context and never touches this function-local static. So the scratch was
 * allocated once and then accumulated every `place`'s effects for the life of the
 * process — including the unclamped `SetUpgrade(x, CurrentUpgrade + 1)` rows,
 * which is the same non-idempotence the de-dup in `assumeOwnItem` exists for. It
 * is not memory-unsafe (`SetUpgrade` writes a masked bitfield in-bounds) and
 * nothing reads the scratch back, but "accumulates unbounded garbage" is not a
 * thing to leave in a file whose comments are the specification. So the scratch is
 * now reset from a pristine copy before every `place`, and the effect of one
 * `place` is a function of that `place` alone.
 *
 * Both are allocated through `Logic`'s own `NewSaveContext()` with `mSaveContext`
 * parked at NULL first, so it frees nothing on the way, and the engine owns
 * exactly two extra `SaveContext`s for the life of the process.
 */
SaveContext* OoTComboLogicNewDetachedSave() {
    Rando::Logic* lg = OoTComboLogicSingleton();
    if (lg == nullptr) {
        return nullptr;
    }
    SaveContext* prior = lg->GetSaveContext(); // may allocate if none yet
    lg->SetSaveContext(nullptr);               // so NewSaveContext frees nothing
    lg->NewSaveContext();
    SaveContext* fresh = lg->GetSaveContext();
    lg->SetSaveContext(prior);
    return fresh;
}

SaveContext* OoTComboLogicScratchSave(bool reset) {
    static SaveContext* scratch = nullptr;
    static SaveContext* pristine = nullptr;
    if (scratch == nullptr) {
        scratch = OoTComboLogicNewDetachedSave();
        pristine = OoTComboLogicNewDetachedSave();
        if (scratch == nullptr || pristine == nullptr) {
            return nullptr;
        }
    }
    if (reset) {
        // `InitSaveContext` reads a handful of settings, so a copy taken at first
        // use can go stale across a re-generation — which costs nothing, because no
        // rule ever reads the scratch back; what matters is that it does not
        // accumulate. `endQuery` asks for the POINTER only (reset = false), because
        // all it needs is to know which context it must not free.
        memcpy(scratch, pristine, sizeof(SaveContext));
    }
    return scratch;
}

/**
 * THE OTHER HALF OF AN ITEM EFFECT, which the save-context bracket cannot reach.
 *
 * `Item::ApplyEffect` does two things (`item.cpp:49-57`): `ApplyItemEffect`, whose
 * every write goes through `Logic::mSaveContext` (`SetUpgrade`, `SetInventory`,
 * `SetQuestItem`, `SetRandoInf`) and is therefore redirected by parking the save
 * pointer; and `logic->Set(logicVal, true)`, which writes `Logic::inLogic[]` — a
 * plain member of the Logic object (`logic.h:172`), NOT part of `mSaveContext`.
 * Swapping the save pointer does not redirect it, so before review's catch a
 * `place` set the placed item's logic value on the LIVE singleton that every
 * region guard reads, and nothing cleared it until the next `Logic::Reset`.
 *
 * That is harmless while a round always follows (both `beginQuery` and `expand`
 * call `Reset(true)`, which memsets `inLogic` — `logic.cpp:2660`), and it is NOT
 * harmless under the coordinator's `none` rung, which runs no round at all: only
 * `clearPlacements`, `allEmptyHosts` and `place` (`combo_logic.c`), so every
 * `place` would leave one more logic value latched on the live singleton and a
 * later OoT evaluation that does not reset first — `CheckBeatable` (`ResetLogic`
 * only) or a bare `ReachabilitySearch` — would evaluate guards with the whole bag
 * already held. An OVER-approximation, the one direction an assumed fill may not
 * err in. Hence: saved and restored around every `place`.
 *
 * `inLogic` is private; `Logic::Get`/`Logic::Set` are the public pair over it and
 * `LOGIC_MAX` bounds it, so the save is a plain bool array and not a reach into
 * the object.
 */
void OoTComboLogicSaveLogicVals(bool* out) {
    Rando::Logic* lg = OoTComboLogicSingleton();
    if (lg == nullptr) {
        return;
    }
    for (int i = 0; i < (int)LOGIC_MAX; ++i) {
        out[i] = lg->Get((LogicVal)i);
    }
}

void OoTComboLogicRestoreLogicVals(const bool* saved) {
    Rando::Logic* lg = OoTComboLogicSingleton();
    if (lg == nullptr) {
        return;
    }
    for (int i = 0; i < (int)LOGIC_MAX; ++i) {
        lg->Set((LogicVal)i, saved[i]);
    }
}

// ============================================================================
// The vtable
// ============================================================================

int OoT_ComboLogic_BeginQuery(void* self) {
    (void)self;
    // Validate BEFORE mutating anything, so a refusal leaves no half-open
    // bracket for `endQuery` to guess about.
    if (!OoTComboLogicReady()) {
        fprintf(stderr, "[OoT/ComboLogic] beginQuery refused: no OTRGlobals / Rando::Context / logic singleton\n");
        return 0;
    }
    if (sInQuery) {
        // The coordinator calls beginQuery exactly once per round. A second one
        // without a teardown is a coordinator bug, and silently re-detaching
        // would orphan the first round's heap context.
        fprintf(stderr, "[OoT/ComboLogic] beginQuery refused: a query is already open\n");
        return 0;
    }

    auto ctx = Rando::Context::GetInstance();
    Rando::Logic* lg = OoTComboLogicSingleton();

    sPriorWasLiveSave = (lg->GetSaveContext() == &gSaveContext);

    // THE DETACH. Everything the round simulates lands in the fresh heap
    // SaveContext this allocates, never in gSaveContext.
    lg->Reset(true);

    // A DEFINED graph state even before the first `expand`: region bits and pool
    // marks cleared. Without these, `crossingOpen` and `checkReached` between
    // `beginQuery` and the first `expand` would report the PREVIOUS round's
    // residue — and the lock asserts the crossing is CLOSED here and OPEN after
    // the expand, which is the differential that proves the reset happens at all.
    //
    // THE SETTINGS' STARTING INVENTORY IS DELIBERATELY NOT APPLIED HERE. `expand`
    // re-derives the round's whole inventory from scratch on every call (see the
    // block there) and `ReachabilitySearch`'s own `ResetLogic` applies the
    // starting inventory exactly once per search, so applying it here too would
    // either be discarded or double-counted. The inventory a round evaluates
    // against is therefore (starting inventory + everything granted), applied
    // once each, and that is decided in one place rather than two.
    Regions::AccessReset();
    ctx->LocationReset();

    sGranted.assign((size_t)RG_MAX, false);
    sGrantedOrder.clear();
    sPrevReachedChecks = -1;
    sPrevReachedRegions = -1;
    sInQuery = true;
    ++sBeginQueryCount;
    return 1;
}

/**
 * Grant one OoT item into the detached simulated inventory.
 *
 * DE-DUPLICATION IS LOAD-BEARING, AND IT IS THE ONE PLACE THIS ENGINE COULD NOT
 * TAKE THE K1 CONTRACT LITERALLY. combo_logic.h says this call "MAY BE CALLED
 * REPEATEDLY, with the same id, within one round ... Repeats must be harmless."
 * `Logic::ApplyItemEffect` does not have that property for OoT's progressives:
 * `RG_PROGRESSIVE_STRENGTH` does `SetUpgrade(UPG_STRENGTH, CurrentUpgrade + 1)`
 * with NO upper clamp (`logic.cpp:1799-1809`; the bomb bag, quiver, wallet and
 * scale rows are the same shape), and `SetUpgrade` writes a masked bitfield — so
 * the fourth grant of a three-tier upgrade walks into the NEXT upgrade's bits.
 * That is not merely "not harmless", it can LOWER another capacity, which is the
 * monotonicity violation the whole assumed fill rests on not happening.
 *
 * So a repeat of an id already granted this round is dropped. The cost, stated
 * plainly rather than left for someone to discover: a bag holding TWO copies of
 * the same progressive id grants ONE tier per round, not two. That is
 * CONSERVATIVE — the round proves reachability under strictly less inventory
 * than the assumption nominally allows, so nothing it proves reachable is
 * actually unreachable — and it is the only reading available, because the
 * surface passes no multiplicity. A multiplicity-aware assume needs a count on
 * the call; that is a contract change for a later increment, recorded in the PR.
 */
void OoT_ComboLogic_AssumeOwnItem(void* self, uint16_t ownItemId) {
    (void)self;
    if (!sInQuery || !OoTComboLogicReady()) {
        return;
    }
    // IDENTITY, not range (added on review): a range test accepts the item table's
    // gaps, and a gap row is default-constructed — see OoTComboLogicIsRealItem for
    // what applying one actually does.
    if (!OoTComboLogicIsRealItem((RandomizerGet)ownItemId)) {
        fprintf(stderr, "[OoT/ComboLogic] assumeOwnItem ignored: %u is not a real OoT item row\n", (unsigned)ownItemId);
        return;
    }
    if (sGranted.size() > (size_t)ownItemId) {
        if (sGranted[(size_t)ownItemId]) {
            return; // already held this round; see the block above
        }
        sGranted[(size_t)ownItemId] = true;
    }
    sGrantedOrder.push_back(ownItemId);
    Rando::StaticData::RetrieveItem((RandomizerGet)ownItemId).ApplyEffect();
}

/**
 * One reachability expansion.
 *
 * `ReachabilitySearch` recomputes the WHOLE closure from `RR_ROOT` each call
 * (its `ResetLogic` clears region bits and pool marks first) over the CURRENT
 * simulated inventory, harvesting every placed item it reaches as it goes. So
 * one call is already a fixpoint over the placements; what the coordinator's
 * alternation adds is the items that arrive from the OTHER game between calls.
 *
 * `changed` is therefore "did the closure GROW since this round's last expand",
 * measured on two counts that can only rise while the inventory only rises. It
 * is not "did the search do work" — the search does the same work every time.
 * The first expand of a round always reports change (there is no previous
 * measurement), which is correct: something opened, namely everything.
 *
 * THE ROUND'S INVENTORY IS RE-DERIVED FROM SCRATCH ON EVERY CALL, and that is a
 * correctness fix rather than tidiness. `ReachabilitySearch`'s `ResetLogic`
 * APPLIES THE STARTING INVENTORY EVERY TIME (`fill.cpp:324-325`) without clearing
 * the simulated inventory first, so a round that expands three times applies it
 * three times. For the progressive rows that is not idempotent — each application
 * is `SetUpgrade(x, CurrentUpgrade + 1)` — so a world whose settings start the
 * player with a progressive item would have its wallet, quiver or strength tier
 * INFLATED by one per extra alternation, and the round would prove reachability
 * the player does not have. That is an OVER-approximation, which is the one
 * direction an assumed fill may not err in.
 *
 * So each expand rebuilds the baseline: `Logic::Reset(true)` (inventory empty,
 * still detached), re-apply exactly the ids granted this round in the order they
 * arrived, then let the search apply the starting inventory once and re-harvest
 * every placed item it reaches. The result is that `expand` is a pure function of
 * (granted set, placements, settings): calling it twice with nothing granted in
 * between gives the identical closure, which is also what makes the round
 * converge in two alternations instead of drifting.
 */
int OoT_ComboLogic_Expand(void* self) {
    (void)self;
    if (!sInQuery || !OoTComboLogicReady()) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    Rando::Logic* lg = OoTComboLogicSingleton();

    // Rebuild the round's inventory baseline — see the block above.
    lg->Reset(true);
    Regions::AccessReset();
    ctx->LocationReset();
    for (const uint16_t granted : sGrantedOrder) {
        Rando::StaticData::RetrieveItem((RandomizerGet)granted).ApplyEffect();
    }

    // The return value is deliberately unused: with placed items everywhere and
    // calculatingAvailableChecks false the returned vector holds only the EMPTY
    // reachable locations, while the `addedToPool` marks the search set along the
    // way are the complete closure — and those marks are what `checkReached`,
    // `reachedEmptyHosts` and `goalReached` read.
    ReachabilitySearch(ctx->allLocations);

    const int checks = OoTComboLogicReachedCheckCount();
    const int regions = OoTComboLogicReachedRegionCount();
    sLastExpandReachedChecks = checks;
    sLastExpandReachedRegions = regions;

    const bool grew = (sPrevReachedChecks < 0) || (checks > sPrevReachedChecks) || (regions > sPrevReachedRegions);
    sPrevReachedChecks = checks;
    sPrevReachedRegions = regions;
    return grew ? 1 : 0;
}

int OoT_ComboLogic_CrossingOpen(void* self) {
    (void)self;
    if (!OoTComboLogicReady()) {
        return 0;
    }
    // Audit §4.5: the OoT->MM crossing is the Happy Mask Shop, which is
    // child-only and day-entered; the coordinator's observable is the region's
    // CHILD access. A region fact, not a new graph edge — no region file changes
    // for increment 3 (audit amendment 3).
    Region* shop = RegionTable(RR_MARKET_MASK_SHOP);
    return (shop != nullptr && shop->Child()) ? 1 : 0;
}

int OoT_ComboLogic_CheckReached(void* self, uint16_t hostCheck) {
    (void)self;
    if (!OoTComboLogicReady() || !OoTComboLogicIsRealCheck((RandomizerCheck)hostCheck)) {
        return 0;
    }
    Rando::ItemLocation* il = Rando::Context::GetInstance()->GetItemLocation((RandomizerCheck)hostCheck);
    return (il != nullptr && il->IsAddedToPool()) ? 1 : 0;
}

/**
 * Shared body of the two host enumerations. `reachedOnly` adds the reachability
 * term and nothing else, which is what makes `allEmptyHosts` a SUPERSET of
 * `reachedEmptyHosts` by construction rather than by review.
 *
 * ORDER: ascending `RC_*` id. A function of the location table alone — it cannot
 * be perturbed by the order items were granted this round, which is the property
 * combo_logic.h requires so that the coordinator's uniform draw is a function of
 * the seed and not of the search's shape.
 *
 * TRUNCATION: at most `cap` ids are written and the TOTAL is always returned, in
 * both the count-only and the write case, so the coordinator can tell a truncated
 * answer from an exhausted one.
 *
 * WHAT BOUNDS THE TOTAL, corrected on review. Not RC_MAX: every id not in `owned`
 * is skipped, so the total is at most |`ctx->allLocations`| — which is a function
 * of the SETTINGS (`Context::GenerateLocationPool` adds pots, grass, crates,
 * beehives, trees, bushes, freestanding, fish, scrubs, cows and tokens as their
 * shuffles are enabled) and can exceed `RSBS_COMBO_LOGIC_PLACEMENT_CAP`, at which
 * point `ComboLogicCollectFrom` refuses the fill with ERR_CAPACITY rather than
 * truncating. The lock measures the real number over a fully emptied world and
 * asserts it against the cap; `OoT_ComboLogic_TestOwnedHostCount` is the bridge.
 */
int OoTComboLogicEnumerateHosts(uint16_t* out, int cap, bool reachedOnly) {
    if (!OoTComboLogicReady()) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    const std::vector<bool>& owned = OoTComboLogicOwnedHosts();

    int total = 0;
    for (int i = 1; i < kOoTCheckIdSpace; ++i) {
        const RandomizerCheck rc = (RandomizerCheck)i;
        if (!owned[(size_t)i] || !OoTComboLogicIsRealCheck(rc)) {
            continue;
        }
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        if (il == nullptr) {
            continue;
        }
        // "Unassigned" is: nothing is in it, AND this engine has not been given
        // it by the coordinator. The second term is belt-and-braces (a host the
        // coordinator placed on holds either a real item or the junk cover, so
        // the first term already excludes it) and it keeps the two facts
        // independent, so a future junk cover of RG_NONE could not resurrect a
        // filled host as a candidate.
        if (il->GetPlacedRandomizerGet() != RG_NONE || OoTComboLogicPlacementIndex(rc) >= 0) {
            continue;
        }
        if (reachedOnly && !il->IsAddedToPool()) {
            continue;
        }
        if (out != nullptr && total < cap) {
            out[total] = (uint16_t)rc;
        }
        ++total;
    }
    return total;
}

int OoT_ComboLogic_ReachedEmptyHosts(void* self, uint16_t* out, int cap) {
    (void)self;
    return OoTComboLogicEnumerateHosts(out, cap, true);
}

int OoT_ComboLogic_AllEmptyHosts(void* self, uint16_t* out, int cap) {
    (void)self;
    // CALLED OUTSIDE ANY ROUND, and this body honours that: it consults the
    // location table and the engine's own placements, never `sInQuery`, the
    // simulated inventory or the reached marks.
    return OoTComboLogicEnumerateHosts(out, cap, false);
}

/**
 * Is OoT's half beaten right now?
 *
 * `CheckBeatable`'s condition, read as a FACT rather than recomputed. The
 * condition itself is `fill.cpp:445`: a location holding `RG_TRIFORCE` becomes
 * `addedToPool`. Calling `CheckBeatable()` here instead would call `ResetLogic`
 * and throw away the round's inventory — the contract wants a pure read, and a
 * goal probe that reset the search would make every round after it answer a
 * different question.
 *
 * The scan covers the whole id space because WHICH check holds the triforce is a
 * settings answer (`RC_GANON` normally, `RC_TRIFORCE_COMPLETED` under triforce
 * hunt — `item_pool.cpp:322-331`), and a world with no `RG_TRIFORCE` placed at
 * all (no fill in this process) correctly answers 0.
 */
int OoT_ComboLogic_GoalReached(void* self) {
    (void)self;
    if (!OoTComboLogicReady()) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    for (int i = 1; i < kOoTCheckIdSpace; ++i) {
        const RandomizerCheck rc = (RandomizerCheck)i;
        if (!OoTComboLogicIsRealCheck(rc)) {
            continue;
        }
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        if (il != nullptr && il->GetPlacedRandomizerGet() == RG_TRIFORCE && il->IsAddedToPool()) {
            return 1;
        }
    }
    return 0;
}

/**
 * Record that an OoT host holds `item`.
 *
 * Own-origin: `Context::PlaceItemInLocation`, the port's own primitive, so the
 * location's bookkeeping is whatever OoT's fill would have produced.
 * Foreign-origin: the host physically receives a FIXED OoT junk item and the
 * foreign identity stays in the coordinator's table — a raw `RI_*` never enters
 * OoT's tables (ADR 0002), and the junk is the degrade the whole cross-game
 * feature already relies on (`foreign_items.h`: with the placement table absent
 * the check just yields the junk it really holds).
 *
 * BRACKETED ON TWO THINGS, because this is called outside any query: the save
 * context is parked at an engine-owned scratch that is RESET before each call (see
 * OoTComboLogicScratchSave — the first version of this file claimed the next
 * `beginQuery` wiped it, which was false), and `Logic::inLogic[]` is saved and
 * restored around the call because `Item::ApplyEffect` writes it too and no
 * save-pointer swap can redirect that (see OoTComboLogicSaveLogicVals). Together
 * those two make "place must not grant the item" true of the whole live singleton
 * and not merely of the player's save.
 *
 * IDEMPOTENT for the same (host, item): the coordinator re-applies its whole
 * table after every snapshot restore, so a repeat is a no-op and not a refusal.
 */
int OoT_ComboLogic_Place(void* self, uint16_t hostCheck, SharedItem item) {
    (void)self;
    if (!OoTComboLogicReady()) {
        return 0;
    }
    const RandomizerCheck rc = (RandomizerCheck)hostCheck;
    if (!OoTComboLogicIsRealCheck(rc)) {
        fprintf(stderr, "[OoT/ComboLogic] place refused: %u is not a real OoT check\n", (unsigned)hostCheck);
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = ctx->GetItemLocation(rc);
    if (il == nullptr) {
        return 0;
    }

    const int existing = OoTComboLogicPlacementIndex(rc);
    if (existing >= 0) {
        const SharedItem& held = sPlacements[existing].item;
        if (held.originGame == item.originGame && held.id == item.id) {
            return 1; // idempotent re-apply
        }
        fprintf(stderr,
                "[OoT/ComboLogic] place: OoT check %u already holds origin=%u id=%u; replacing with origin=%u id=%u\n",
                (unsigned)hostCheck, (unsigned)held.originGame, (unsigned)held.id, (unsigned)item.originGame,
                (unsigned)item.id);
    } else if (sPlacementCount >= RSBS_COMBO_LOGIC_PLACEMENT_CAP) {
        fprintf(stderr, "[OoT/ComboLogic] place refused: engine placement table full (%d)\n",
                (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP);
        return 0;
    }

    RandomizerGet toPlace;
    if (item.originGame == (uint8_t)GAME_OOT) {
        // Identity, not range — same reason as in assumeOwnItem, and the refusal
        // happens before any bookkeeping so a rejected place changes nothing.
        if (!OoTComboLogicIsRealItem((RandomizerGet)item.id)) {
            fprintf(stderr, "[OoT/ComboLogic] place refused: %u is not a real OoT item row\n", (unsigned)item.id);
            return 0;
        }
        toPlace = (RandomizerGet)item.id;
    } else if (item.originGame == (uint8_t)GAME_NONE) {
        fprintf(stderr, "[OoT/ComboLogic] place refused: item carries no origin\n");
        return 0;
    } else {
        toPlace = kOoTForeignJunkCover;
    }

    const int slot = (existing >= 0) ? existing : sPlacementCount;
    if (existing < 0) {
        sPlacements[slot].host = rc;
        sPlacements[slot].priorItem = il->GetPlacedRandomizerGet();
        ++sPlacementCount;
    }
    sPlacements[slot].item = item;

    // THE BRACKET, over BOTH halves of what the item effect writes. Restored
    // unconditionally, because the re-attach rule only means something if nothing
    // can leave `Logic` pointing somewhere else.
    //
    // Half one: the save context, parked at a freshly reset scratch. Bracketed
    // whatever `prior` was, not only when it was `&gSaveContext` (tightened on
    // review): parking and restoring a heap context is just as safe, nothing is
    // freed here, and "place never grants" should not be a property that depends on
    // whether a generation happens to be mid-flight.
    //
    // Half two: `Logic::inLogic[]`, which no save-pointer swap can redirect —
    // see OoTComboLogicSaveLogicVals.
    Rando::Logic* lg = OoTComboLogicSingleton();
    SaveContext* prior = lg->GetSaveContext();
    SaveContext* scratch = OoTComboLogicScratchSave(true);
    const bool bracket = (scratch != nullptr);
    static bool sLogicValsBeforePlace[LOGIC_MAX];
    OoTComboLogicSaveLogicVals(sLogicValsBeforePlace);
    if (bracket) {
        lg->SetSaveContext(scratch);
    }
    ctx->PlaceItemInLocation(rc, toPlace, false);
    if (bracket) {
        lg->SetSaveContext(prior);
    }
    OoTComboLogicRestoreLogicVals(sLogicValsBeforePlace);
    return 1;
}

void OoT_ComboLogic_ClearPlacements(void* self) {
    (void)self;
    if (!OoTComboLogicReady()) {
        sPlacementCount = 0;
        return;
    }
    auto ctx = Rando::Context::GetInstance();
    // Restore the PRIOR item, not RG_NONE: the coordinator draws its hosts from
    // this engine's empty lists, so the prior is RG_NONE in the fill's own shape,
    // but Combo_Logic_Place can author onto any host and a roll-back that wrote
    // RG_NONE over somebody else's placement would corrupt a world it never
    // decided.
    for (int i = sPlacementCount - 1; i >= 0; --i) {
        Rando::ItemLocation* il = ctx->GetItemLocation(sPlacements[i].host);
        if (il != nullptr) {
            il->SetPlacedItem(sPlacements[i].priorItem);
        }
    }
    sPlacementCount = 0;
}

/**
 * End the round and put `Logic`'s coupling back.
 *
 * SAFE AFTER A FAILED `beginQuery` AND SAFE TWICE. `sInQuery` is the guard: a
 * teardown on a side whose bracket never opened returns immediately, so there is
 * no double-restore and no "restore" of a pointer nobody captured.
 *
 * WHAT THE COORDINATOR ACTUALLY DOES, corrected on review. The first version of
 * this comment said the coordinator is entitled to tear down a side whose bracket
 * never opened, and the merged coordinator does not do that: its teardown is
 * `if (began[g]) e->endQuery(e->self)` and `began[g]` is set only after
 * `beginQuery` returned nonzero (`src/common/combo_logic.c`). So this guard is
 * DEFENSIVE against a coordinator that stops guarding — which a future increment
 * may well do, since the snapshot is taken before `beginQuery` and an unguarded
 * symmetric teardown is the obvious simplification — and not a contract obligation
 * the coordinator as merged imposes. The lock is honest about the same thing: it
 * provokes a refusal it CAN provoke (a second `beginQuery` inside an open query)
 * and then asserts a repeated `endQuery` is a no-op.
 */
void OoT_ComboLogic_EndQuery(void* self) {
    (void)self;
    if (!sInQuery) {
        return;
    }
    sInQuery = false;
    ++sEndQueryCount;

    Rando::Logic* lg = OoTComboLogicSingleton();
    if (lg == nullptr) {
        return;
    }
    if (!sPriorWasLiveSave) {
        // The prior pointer was a heap context that `Logic::Reset(true)` already
        // free()d; the honest end state is the fresh detached context we hold.
        return;
    }
    SaveContext* mine = lg->GetSaveContext();
    lg->SetSaveContext(&gSaveContext);
    if (mine != nullptr && mine != &gSaveContext && mine != OoTComboLogicScratchSave(false)) {
        // Released with free(), mirroring `Logic::NewSaveContext` exactly
        // (`logic.cpp:2310-2315`). Without this the fill orphans one ~136 KB
        // SaveContext per ROUND — and it runs a round per bag item — because the
        // next round's Reset(true) sees mSaveContext == &gSaveContext and frees
        // nothing. The new/free mismatch is the port's own inherited UB (audit
        // §1.8 hazard 2), recorded there and not introduced here.
        free(mine);
    }
}

// ============================================================================
// Registration
// ============================================================================
//
// File-scope initializer, so the engine is published before main() and before
// any test or gameplay code can ask for it. This TU lives in soh_rando, which
// links WHOLE_ARCHIVE (games/oot/CMakeLists.txt), so the initializer is never
// dropped as unreferenced — the same property the foreign pool's registrar
// relies on, and the reason this file sits in this directory.
//
// snapshot/restore are BOTH NULL: OoT's queries are pure once detached.

const ComboLogicEngine kOoTComboLogicEngine = {
    /* abiVersion        */ RSBS_COMBO_LOGIC_ENGINE_ABI,
    /* self              */ nullptr,
    /* beginQuery        */ OoT_ComboLogic_BeginQuery,
    /* assumeOwnItem     */ OoT_ComboLogic_AssumeOwnItem,
    /* expand            */ OoT_ComboLogic_Expand,
    /* crossingOpen      */ OoT_ComboLogic_CrossingOpen,
    /* checkReached      */ OoT_ComboLogic_CheckReached,
    /* reachedEmptyHosts */ OoT_ComboLogic_ReachedEmptyHosts,
    /* allEmptyHosts     */ OoT_ComboLogic_AllEmptyHosts,
    /* goalReached       */ OoT_ComboLogic_GoalReached,
    /* place             */ OoT_ComboLogic_Place,
    /* clearPlacements   */ OoT_ComboLogic_ClearPlacements,
    /* endQuery          */ OoT_ComboLogic_EndQuery,
    /* snapshot          */ nullptr,
    /* restore           */ nullptr,
};

struct OoTComboLogicRegistrar {
    OoTComboLogicRegistrar() {
        Combo_Logic_RegisterEngine(GAME_OOT, &kOoTComboLogicEngine);
    }
};
const OoTComboLogicRegistrar gOoTComboLogicRegistrar;

} // namespace

// ============================================================================
// TEST BRIDGES (rando tier; src/common/tests/test_oot_logic_export.c)
// ============================================================================
//
// Each one exposes ONE fact src/common cannot name for itself, because it has no
// OoT enum in scope by design. None is called by production, and none of them
// re-implements a rule — the locks drive the REAL vtable above through
// Combo_Logic_GetEngine(GAME_OOT).

/** RC_MAX: the range the enumerations walk. Printed by the lock for context; it is
 *  NOT the sizing fact (see OoT_ComboLogic_TestOwnedHostCount, which is). */
extern "C" int OoT_ComboLogic_TestCheckIdSpace(void) {
    return kOoTCheckIdSpace;
}

/**
 * |`ctx->allLocations`| restricted to real rows: the number of hosts this engine
 * OWNS, and therefore the largest answer `allEmptyHosts` can ever return (it is
 * that answer exactly, when every owned host is empty). THIS is the number a
 * coordinator host buffer must hold, and the lock measures it two ways — through
 * this bridge and by emptying every owned host and asking the engine — so the
 * sizing statement is a measurement instead of the constant-vs-constant comparison
 * review (rightly) called a tautology.
 */
extern "C" int OoT_ComboLogic_TestOwnedHostCount(void) {
    if (!OoTComboLogicReady()) {
        return -1;
    }
    const std::vector<bool>& owned = OoTComboLogicOwnedHosts();
    int total = 0;
    for (int i = 1; i < kOoTCheckIdSpace; ++i) {
        if (owned[(size_t)i] && OoTComboLogicIsRealCheck((RandomizerCheck)i)) {
            ++total;
        }
    }
    return total;
}

/** The owned host ids in ascending order, so a lock can empty exactly the engine's
 *  own host set and restore it. Same truncation contract as the vtable's
 *  enumerations: at most `cap` written, true total returned. */
extern "C" int OoT_ComboLogic_TestOwnedHosts(uint16_t* out, int cap) {
    if (!OoTComboLogicReady()) {
        return -1;
    }
    const std::vector<bool>& owned = OoTComboLogicOwnedHosts();
    int total = 0;
    for (int i = 1; i < kOoTCheckIdSpace; ++i) {
        if (!owned[(size_t)i] || !OoTComboLogicIsRealCheck((RandomizerCheck)i)) {
            continue;
        }
        if (out != nullptr && total < cap) {
            out[total] = (uint16_t)i;
        }
        ++total;
    }
    return total;
}

/**
 * How many of `Logic::inLogic[]`'s flags are set on the LIVE singleton right now,
 * and an FNV digest over all of them.
 *
 * The simulated inventory's own observable, and the reason two locks exist that
 * could not before:
 *
 *  - `beginQuery`'s `Logic::Reset(true)` memsets `inLogic` (`logic.cpp:2660`) and
 *    nothing in the rest of `Reset` sets a logic value, so the count is EXACTLY 0
 *    between `beginQuery` and the first `expand`. Read with residue deliberately
 *    present, that assertion can only be satisfied by `beginQuery`'s own reset —
 *    which is how the residue leg became attributable to it instead of to
 *    `expand`'s.
 *  - `Item::ApplyEffect` sets one of these flags (`item.cpp:56`) OUTSIDE the
 *    save-context bracket, so "place did not grant the item" is checkable here and
 *    nowhere else.
 */
extern "C" int OoT_ComboLogic_TestLogicValsHeld(void) {
    Rando::Logic* lg = OoTComboLogicSingleton();
    if (lg == nullptr) {
        return -1;
    }
    int held = 0;
    for (int i = 0; i < (int)LOGIC_MAX; ++i) {
        if (lg->Get((LogicVal)i)) {
            ++held;
        }
    }
    return held;
}

extern "C" uint32_t OoT_ComboLogic_TestLogicValDigest(void) {
    Rando::Logic* lg = OoTComboLogicSingleton();
    uint32_t hash = 2166136261u;
    if (lg == nullptr) {
        return 0;
    }
    for (int i = 0; i < (int)LOGIC_MAX; ++i) {
        hash ^= (uint32_t)(lg->Get((LogicVal)i) ? 1u : 0u);
        hash *= 16777619u;
    }
    return hash;
}

/**
 * The lowest item id in (RG_NONE, RG_MAX) whose `itemTable` row is a GAP — i.e.
 * fails OoTComboLogicIsRealItem — or -1 if the table has no gaps.
 *
 * Exists so the input-validation lock can hand the engine an id that is in RANGE
 * and is not an ITEM, which is the case a range-only check accepted.
 */
extern "C" int OoT_ComboLogic_TestFindGapItemId(void) {
    if (!OoTComboLogicReady()) {
        return -1;
    }
    for (int i = (int)RG_NONE + 1; i < (int)RG_MAX; ++i) {
        if (!OoTComboLogicIsRealItem((RandomizerGet)i)) {
            return i;
        }
    }
    return -1;
}

/** RG_MAX: the item id space, for the locks' id-range assertions. */
extern "C" int OoT_ComboLogic_TestItemIdSpace(void) {
    return (int)RG_MAX;
}

/** Reached checks and reached region-bits as of the last `expand`. The closure
 *  observable, so a lock can assert the closure GREW under an assumption instead
 *  of taking monotonicity on trust. */
extern "C" int OoT_ComboLogic_TestLastReachedChecks(void) {
    return sLastExpandReachedChecks;
}

extern "C" int OoT_ComboLogic_TestLastReachedRegions(void) {
    return sLastExpandReachedRegions;
}

/** Reached checks RIGHT NOW, recomputed. Distinct from the cached pair above:
 *  the locks use this to read the closure after calls the engine did not
 *  make. */
extern "C" int OoT_ComboLogic_TestReachedCheckCountNow(void) {
    return OoTComboLogicReachedCheckCount();
}

/** 1 iff `Logic::mSaveContext == &gSaveContext`. The re-attach rule's whole
 *  observable (audit §4.4). */
extern "C" int OoT_ComboLogic_TestLogicIsAttachedToLiveSave(void) {
    Rando::Logic* lg = logic.get();
    return (lg != nullptr && lg->GetSaveContext() == &gSaveContext) ? 1 : 0;
}

/** Point `Logic` at the live save, returning the previous attachment. The locks
 *  use it to put the engine in the state a LOADED SAVE puts it in, which is the
 *  only state in which the re-attach rule and the live-save bracket have
 *  anything to protect. */
extern "C" int OoT_ComboLogic_TestAttachLogicToLiveSave(void) {
    Rando::Logic* lg = logic.get();
    if (lg == nullptr) {
        return -1;
    }
    const int previous = (lg->GetSaveContext() == &gSaveContext) ? 1 : 0;
    lg->SetSaveContext(&gSaveContext);
    return previous;
}

/** How many times the vtable's begin/end have run. A lock reads these to prove
 *  a teardown after a FAILED begin did not count as a query. */
extern "C" int OoT_ComboLogic_TestBeginCount(void) {
    return sBeginQueryCount;
}

extern "C" int OoT_ComboLogic_TestEndCount(void) {
    return sEndQueryCount;
}

/**
 * The residue producer: one BARE `ReachabilitySearch` with no preceding
 * `Logic::Reset`, which is exactly the call shape that made the #656 gate answer
 * 48-of-57 against a truth of 57.
 *
 * Exists so a lock can run an UNRELATED search between two identical queries and
 * require the second answer to match the first. Without this bridge that lock
 * cannot exist at all from src/common, and the residue defect — the one this
 * engine's `beginQuery` is built to prevent — would be untested.
 *
 * @return the reached-check count the bare search leaves behind, or -1 if the
 *         solver is not live.
 */
extern "C" int OoT_ComboLogic_TestRunUnrelatedSearch(void) {
    if (!OoTComboLogicReady()) {
        return -1;
    }
    auto ctx = Rando::Context::GetInstance();
    ReachabilitySearch(ctx->allLocations);
    return OoTComboLogicReachedCheckCount();
}

/**
 * The item a check holds, and whether it is an ADVANCEMENT item.
 *
 * The locks use it to build a genuinely PARTIAL closure over the real graph:
 * empty every advancement-bearing location, and the world collapses towards
 * sphere zero, which is the only way a post-fill process can observe
 * monotonicity doing work (with All Locations Reachable on and every item
 * placed, the baseline closure is already total and "assuming more did not
 * shrink it" is satisfied by a constant).
 *
 * @return 1 while `rc` names a real row, 0 otherwise.
 */
extern "C" int OoT_ComboLogic_TestPlacedItemAt(uint16_t rc, uint16_t* outItemId, int* outAdvancement) {
    if (!OoTComboLogicReady() || !OoTComboLogicIsRealCheck((RandomizerCheck)rc)) {
        return 0;
    }
    Rando::ItemLocation* il = Rando::Context::GetInstance()->GetItemLocation((RandomizerCheck)rc);
    if (il == nullptr) {
        return 0;
    }
    const RandomizerGet placed = il->GetPlacedRandomizerGet();
    if (outItemId != nullptr) {
        *outItemId = (uint16_t)placed;
    }
    if (outAdvancement != nullptr) {
        *outAdvancement = (placed != RG_NONE && Rando::StaticData::RetrieveItem(placed).IsAdvancement()) ? 1 : 0;
    }
    return 1;
}

/** Write a check's placed item WITHOUT applying its effect — `SetPlacedItem`
 *  alone, so the locks can empty and then exactly restore a set of hosts and
 *  assert the world digest came back byte-identical. Test-only. */
extern "C" int OoT_ComboLogic_TestSetPlacedItem(uint16_t rc, uint16_t itemId) {
    if (!OoTComboLogicReady() || !OoTComboLogicIsRealCheck((RandomizerCheck)rc)) {
        return 0;
    }
    if (itemId >= (uint16_t)RG_MAX) {
        return 0;
    }
    Rando::ItemLocation* il = Rando::Context::GetInstance()->GetItemLocation((RandomizerCheck)rc);
    if (il == nullptr) {
        return 0;
    }
    il->SetPlacedItem((RandomizerGet)itemId);
    return 1;
}

/**
 * An FNV-1a digest over (check id, placed item) for the WHOLE id space — the
 * generated world's placement, as this process holds it.
 *
 * This is the lane's own world-unchanged observable: every lock that perturbs
 * the world takes it before and after and requires equality, so "the engine
 * changed no placement" is measured rather than argued. It is NOT the golden
 * seed digest and must not be presented as one; it covers OoT's item locations
 * only.
 */
extern "C" uint32_t OoT_ComboLogic_TestWorldDigest(void) {
    uint32_t hash = 2166136261u;
    if (!OoTComboLogicReady()) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    for (int i = 1; i < kOoTCheckIdSpace; ++i) {
        const RandomizerCheck rc = (RandomizerCheck)i;
        if (!OoTComboLogicIsRealCheck(rc)) {
            continue;
        }
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        const uint32_t placed = (il == nullptr) ? 0u : (uint32_t)il->GetPlacedRandomizerGet();
        const uint32_t words[2] = { (uint32_t)i, placed };
        for (int b = 0; b < 8; ++b) {
            hash ^= (uint32_t)((words[b / 4] >> ((b % 4) * 8)) & 0xFFu);
            hash *= 16777619u;
        }
    }
    return hash;
}

/**
 * Force the SELECTED STARTING AGE to adult (1) or child (0), returning the
 * previous value, so a lock can ask what `crossingOpen` answers from an adult
 * start without paying a second whole generation.
 *
 * LEGITIMATE BECAUSE `AccessReset` READS THE OPTION AT CALL TIME
 * (`location_access.cpp:1092-1097`): the starting age is not baked into the
 * graph, it is the bit `RR_ROOT` gets when a query begins. The lock restores the
 * previous value and re-asserts the world digest, so no placement moves.
 *
 * @return the previous value (0 child, 1 adult), or -1 if there is no context.
 */
extern "C" int OoT_ComboLogic_TestForceAdultStart(int adult) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return -1;
    }
    const int previous = ctx->GetOption(RSK_SELECTED_STARTING_AGE).Is(RO_AGE_CHILD) ? 0 : 1;
    ctx->GetOption(RSK_SELECTED_STARTING_AGE).Set((adult != 0) ? RO_AGE_ADULT : RO_AGE_CHILD);
    return previous;
}

// ============================================================================
// THE FILL-CLASS SOURCE (ADR 0010 answer O8; #645 increment 3, lane K5)
// ============================================================================
//
// OoT's rows for the single-owner classification table in src/common/
// shared_items.{h,c}. This TU is the SOURCE, never the owner: it answers "which
// class is this RG_*" once per id when the owner builds its table, and every
// consumer then reads the owner's stored row. It lives here, beside the engine,
// because this is already the OoT TU that may name `RG_*` for the coordinator
// (ADR 0002) and it links WHOLE_ARCHIVE, so the registrar below survives the link
// exactly as the engine's does.
//
// Deliberately NOT inside the engine's vtable or any function K4's multiplicity
// work edits: the bag will read the owner table, not this function.

#include "shared_items.h" // src/common — the owner this source registers with

namespace {

/** OoT's item table is filled by `Rando::StaticData::InitItemTable` at OTR
 *  bring-up; before that every row is default-constructed. The same readiness
 *  test menu.cpp's hint locks use: the RG_NONE sentinel has a name once the
 *  table exists. */
bool OoTFillClassTableReady() {
    return !Rando::StaticData::RetrieveItem(RG_NONE).GetName().GetEnglish().empty();
}

/** Money: a renewable whatever GetItemCategory() files it under (the huge and
 *  treasure-game rupees are LESSER/MAJOR, the rest JUNK). */
bool OoTFillClassIsRupee(RandomizerGet rg) {
    switch (rg) {
        case RG_GREEN_RUPEE:
        case RG_BLUE_RUPEE:
        case RG_RED_RUPEE:
        case RG_PURPLE_RUPEE:
        case RG_HUGE_RUPEE:
        case RG_TREASURE_GAME_GREEN_RUPEE:
            return true;
        default:
            return false;
    }
}

/**
 * The CONFINEMENT family of an OoT item: the settings that can hold it in one of
 * OoT's restricted placement passes (keysanity, song / token / reward shuffle),
 * which run before the general pass the union bag is drawn from (ADR 0010 O4
 * amendment 2). A property of the item's family, so it is tagged on every class,
 * not only on progression. The shop's own stock (`RG_BUY_*`) is placed only into
 * shop slots and no setting lets it roam; triforce pieces are a per-world goal
 * quantity.
 */
uint32_t OoTFillClassArmedBy(RandomizerGet rg, ItemType type) {
    if (rg == RG_TRIFORCE_PIECE) {
        return RSBS_FILL_ARM_WORLD_EVENT;
    }
    switch (type) {
        case ITEMTYPE_SMALLKEY:
        case ITEMTYPE_FORTRESS_SMALLKEY:
            return RSBS_FILL_ARM_SMALL_KEYS_ROAM;
        case ITEMTYPE_BOSSKEY:
            return RSBS_FILL_ARM_BOSS_KEYS_ROAM;
        case ITEMTYPE_MAP:
        case ITEMTYPE_COMPASS:
            return RSBS_FILL_ARM_MAPS_ROAM;
        case ITEMTYPE_SONG:
            return RSBS_FILL_ARM_SONGS_ROAM;
        case ITEMTYPE_TOKEN:
            return RSBS_FILL_ARM_TOKENS_ROAM;
        case ITEMTYPE_DUNGEONREWARD:
            return RSBS_FILL_ARM_REWARDS_ROAM;
        case ITEMTYPE_SHOP:
            return RSBS_FILL_ARM_SHOP_STOCK;
        default:
            return 0u;
    }
}

} // namespace

/**
 * Classify one OoT item id (ComboItemClassifyFn). The precedence is the owner's
 * (shared_items.h): TRAP, then PROGRESSION by OoT's own `IsAdvancement()`, then
 * RENEWABLE, then JUNK.
 *
 * NOT A FILL ITEM (returns 0): ids past RG_MAX, gap rows (the identity test
 * OoTComboLogicIsRealItem already applies to every engine input), and the
 * ITEMTYPE_EVENT rows — RG_NONE (the sentinel), RG_TRIFORCE (the goal, placed at
 * RC_GANON / RC_TRIFORCE_COMPLETED by item_pool.cpp's fixed placement) and
 * RG_HINT (placed on gossip stones by hints.cpp). Each of those is placed by a
 * fixed placement and is never drawn from any fill pool, and classing the goal
 * item as "junk" because it is not advancement would be a wrong answer, not a
 * conservative one.
 *
 * RENEWABLE, precisely: every refill and drop; every shop-stock row except
 * RG_SOLD_OUT (a shop sells it again); money; and the remaining ITEM rows that
 * OoT's own category files as JUNK (recovery heart, fish, milk). The two
 * non-advancement EQUIP rows (Deku and Hylian shields) are renewable too: both
 * are sold in shops and the Deku shield burns.
 */
extern "C" int OoT_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out) {
    ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u };
    if (out != nullptr) {
        *out = row;
    }
    if (!OoTFillClassTableReady()) {
        return -1;
    }
    if (id >= (uint16_t)RG_MAX) {
        return 0;
    }
    const RandomizerGet rg = (RandomizerGet)id;
    if (!OoTComboLogicIsRealItem(rg)) {
        return 0;
    }
    Rando::Item& item = Rando::StaticData::RetrieveItem(rg);
    const ItemType type = item.GetItemType();
    if (type == ITEMTYPE_EVENT) {
        return 0;
    }

    if (rg == RG_ICE_TRAP) {
        row.fillClass = RSBS_FILL_CLASS_TRAP;
    } else if (item.IsAdvancement()) {
        row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
    } else if (type == ITEMTYPE_REFILL || type == ITEMTYPE_DROP || type == ITEMTYPE_EQUIP ||
               (type == ITEMTYPE_SHOP && rg != RG_SOLD_OUT) || OoTFillClassIsRupee(rg) ||
               (type == ITEMTYPE_ITEM && item.GetCategory() == ITEM_CATEGORY_JUNK)) {
        row.fillClass = RSBS_FILL_CLASS_RENEWABLE;
    } else {
        row.fillClass = RSBS_FILL_CLASS_JUNK;
    }
    row.armedBy = OoTFillClassArmedBy(rg, type);
    if (out != nullptr) {
        *out = row;
    }
    return 1;
}

namespace {
const ComboItemClassSource kOoTItemClassSource = {
    /* abiVersion */ RSBS_ITEM_CLASS_SOURCE_ABI,
    /* idSpace    */ (uint16_t)RG_MAX,
    /* classify   */ OoT_ComboLogic_ClassifyItem,
};

struct OoTItemClassRegistrar {
    OoTItemClassRegistrar() {
        Combo_RegisterItemClassSource(GAME_OOT, &kOoTItemClassSource);
    }
};
const OoTItemClassRegistrar gOoTItemClassRegistrar;
} // namespace

/**
 * TEST BRIDGE (redship tier; src/common/tests/test_shared_items_class.c): bring
 * OoT's item table up in a process that never ran the OTR bring-up — the
 * display-free, ROM-free tier. Mirrors `Rando_HeadlessSeedTest`'s fallback order
 * (a Context first, because InitItemTable reads the context's Logic) and
 * `Rando_InitRegionGraphForTest`'s idempotence: a process whose table already
 * exists is left exactly as it is.
 *
 * @return 0 when the table is ready afterwards, -1 otherwise.
 */
extern "C" int OoT_ComboLogic_TestEnsureItemTable(void) {
    if (!OoTFillClassTableReady()) {
        if (Rando::Context::GetInstance() == nullptr) {
            // The Context <-> Logic shared_ptr back-edge keeps the instance alive
            // past this scope (TrackerAdapterSingleExe.cpp documents the cycle).
            Rando::Context::CreateInstance();
        }
        Rando::StaticData::InitItemTable();
    }
    return OoTFillClassTableReady() ? 0 : -1;
}

/** TEST BRIDGE: OoT's own fill predicate for one id, read directly off the item
 *  table and NOT through the classifier, so the lock can check the classifier's
 *  precedence against it. 1 advancement, 0 not, -1 not a real item row. */
extern "C" int OoT_ComboLogic_TestFillAdvancement(uint16_t id) {
    if (id >= (uint16_t)RG_MAX || !OoTComboLogicIsRealItem((RandomizerGet)id)) {
        return -1;
    }
    return Rando::StaticData::RetrieveItem((RandomizerGet)id).IsAdvancement() ? 1 : 0;
}
#endif // RSBS_SINGLE_EXECUTABLE
