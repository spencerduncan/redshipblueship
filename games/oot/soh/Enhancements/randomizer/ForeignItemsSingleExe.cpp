/**
 * ForeignItemsSingleExe.cpp — the pinned foreign-item pool and the OoT-side
 * redemption give (Phase 3.0 Lane C1, #392; ADR 0002).
 *
 * This is the ONE translation unit where the cross-game item class is defined,
 * because it is the one place the real RG_* enumerators may appear: everything
 * leaves this TU as an origin-tagged SharedItem (or a plain display string),
 * so a raw RG_* integer never crosses a game boundary (ADR 0002, the #356 bug
 * class). MM and the ROM-free test harness reach the pool through the
 * extern "C" surface declared in src/common/foreign_items.h.
 *
 * Pool choice (C0 handoff on #392): ~4 OoT progression items with clean give
 * semantics. The handoff's example list named RG_PROGRESSIVE_STRENGTH_UPGRADE,
 * which does not exist; the real enumerator is RG_PROGRESSIVE_STRENGTH.
 *
 * Redemption give: progressive entries resolve against the LIVE save via
 * Item::GetGIEntry_Copy() — Logic's save context is pointed at gSaveContext on
 * every save load (SaveManager.cpp / savefile.cpp), so this is the same
 * resolution SoH's own in-game gives use — then dispatch mirrors
 * savefile.cpp's StartingItemGive: MOD_NONE entries through OoT_Item_Give
 * (the NULL-play starting-item precedent), MOD_RANDOMIZER entries through
 * Randomizer_Item_Give.
 *
 * Lives in soh/Enhancements/randomizer/ (soh_rando) which links WHOLE_ARCHIVE,
 * so these definitions always survive the link.
 *
 * #510 added the REVERSE direction's producer half at the bottom of this file:
 * OoT_Foreign_IsEligibleHost (which OoT check may host an MM item) and
 * OoT_PlaceForeignItems (the generation-time placement pass Playthrough_Init
 * calls). Those are the twin of MM's Rando::Foreign, and they consume MM's
 * kForeignPoolMMV1 through the same origin-indexed registry — so this file still
 * never sees an RI_*, only origin-tagged SharedItems.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring> // memcpy/memset — the creation event's snapshot bracket
#include <string>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
// ReachabilitySearch — the reverse pass's reachability gate (#656). The same
// header entrance.cpp reaches it through, from the same directory.
#include "3drando/fill.hpp"

#include "foreign_items.h"       // src/common — ComboForeignItemDef, SharedItem
#include "shared_items.h"        // src/common — Combo_RecordSharedItem (#493)
#include "notification_bridge.h" // src/common — the shared refusal/failure overlay
#include "gen_budget.h"          // src/common — the #582 fill budget + progress surface

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h" // OoT_Item_Give
u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry);
}

// savefile.cpp's rupee helper (C++ linkage there; no header of its own).
void GiveLinkRupees(int numOfRupees);

// ============================================================================
// The pinned pool ("foreign item class v1")
// ============================================================================

// Aggregate member-wise init is the sanctioned way to build a SharedItem (the
// ADR's static_asserts reject raw-integer conversion; explicit members carry
// the tag). Values must fit the struct's uint8_t/uint16_t members — a
// constant-expression enumerator that fits is not a narrowing conversion.
// The article column matches OoT's own item table (Item::GetArticle), so MM's
// pickup textbox reads exactly like an MM one: "You found the Fairy Bow!".
// CRITERION 6 APPLIES HERE TOO (#525). "Fairy Bow" and "Bomb Bag" used to sit
// in this table and left with shared ammo: the quiver and bomb-bag capacity
// tiers are now ONE quantity spanning both games (src/common/shared_resources.h),
// and in MM owning the bow IS quiver tier 1, so either row would have crossed
// as a mutation of the shared resource rather than as a new item.
// "Progressive Hookshot" left the same way when the hookshot became
// RSBS_SHARED_RES_HOOKSHOT_TIER — a progressive whose every step now moves a
// shared quantity is the clearest case criterion 6 has. Nothing in this pool
// may be a shared cross-game resource: no wallet, heart, magic, ammo or
// hookshot row.
//
// "Lens of Truth" IS THE CROSS-POOL COLLISION, deliberately. Display names are
// the spoiler-load persistence key, and the lookups are keyed on
// (originGame, name) precisely because a bare name can appear in both pools;
// "Bomb Bag" was that pair until shared ammo retired both halves, so this row
// was added in the same commit that deleted them and inherits the job. The
// collision is asserted on purpose in test_foreign_items.c — it must not be
// "fixed" by renaming either side. MM's twin is the RI_LENS row, and the two
// strings must stay byte-identical.
//
// Boomerang and Megaton Hammer widen a pool that was down to three rows against
// MM's 116, which made the forward direction ship nearly the same items every
// seed (a follow-up #525 lists in as many words). Both are plain MOD_NONE
// inventory items whose give falls through OoT_Item_Give's generic tail — save
// writes only, no PlayState deref — which is what makes them safe on the
// NULL-play redemption path the pool is given through.
// The `iconName` column is the ITEM_* texture-map key each item's OoT arrival
// toast renders (#494) — the exact string GetTextureForItemId returns for that
// item's GetItemEntry.itemId, which is what the Notification overlay resolves
// through GetTextureByName. Verified against item_list.cpp's item table and the
// Plandomizer itemImageMap: GI_LENS -> ITEM_LENS, GI_BOOMERANG -> ITEM_BOOMERANG,
// GI_HAMMER -> ITEM_HAMMER. Progressive Strength has no single item id (it
// resolves per-tier at give time), so it takes the base-tier bracelet icon as a
// stable representative rather than a per-tier lookup on the NULL-play redemption
// path — a decoration, not the give. src/common serves these back verbatim via
// Combo_GetForeignItemIconName; it never has to translate an RG_* itself.
//
// THE CLASS COLUMN (#495, ADR 0011 decision 3). Every row now names the ONE
// RSBS_ITEMCLASS_* bit it belongs to, and the pool draw is a rule evaluation
// over those bits rather than "the whole array, in order". Both placement
// passes call Combo_ForeignPoolDrawFor, which filters this table by the frozen
// itemClass* bitset and preserves pool order.
//
// ALL FOUR ROWS ARE `PROGRESSION`, and that is the point rather than an
// accident: this table WAS the entire cross-game item class, and it becomes ONE
// CLASS'S MEMBERSHIP. Every row is a major/progressive OoT item whose give is
// unconditionally effectful and NULL-play safe — which is exactly what
// RSBS_ITEMCLASS_PROGRESSION names. The other five classes are unpopulated on
// this side today: OoT's songs, masks (it has none), dungeon items and dungeon
// rewards have not been adjudicated against the six criteria, and an unaudited
// row admitted by a class bit would be a crossing the redemption path cannot
// safely give. Appending members later is a table edit here, not a format
// change anywhere — which is the whole reason the class is a rule.
static const ComboForeignItemDef kForeignPoolV1[] = {
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_PROGRESSIVE_STRENGTH },
      "Progressive Strength Upgrade",
      "a ",
      RSBS_ITEMCLASS_PROGRESSION,
      "ITEM_BRACELET" },
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_LENS_OF_TRUTH },
      "Lens of Truth",
      "the ",
      RSBS_ITEMCLASS_PROGRESSION,
      "ITEM_LENS" },
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_BOOMERANG },
      "Boomerang",
      "the ",
      RSBS_ITEMCLASS_PROGRESSION,
      "ITEM_BOOMERANG" },
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_MEGATON_HAMMER },
      "Megaton Hammer",
      "the ",
      RSBS_ITEMCLASS_PROGRESSION,
      "ITEM_HAMMER" },
};

static constexpr int kForeignPoolCount = sizeof(kForeignPoolV1) / sizeof(kForeignPoolV1[0]);
// A COMPILE-TIME BOUND ON THE MAXIMUM, not the runtime limit. How many of these
// rows a given world may actually place is the frozen record's poolSizeOoT,
// clamped by Combo_ComboPoolSizeFor and applied at the placement pass (ADR 0011
// increment 3). This assert survives because the OoT table happens to be
// smaller than the carve; the MM table deliberately carries no such assert, for
// the reason its own header states.
static_assert(kForeignPoolCount <= (int)RSBS_FOREIGN_PLACEMENT_CAP,
              "the pinned foreign pool must fit the gComboCtx placement carve");

// ----------------------------------------------------------------------------
// THE EXCLUSIONS, WITH ATTRIBUTION (ADR 0011 decision 3.4)
// ----------------------------------------------------------------------------
//
// The six criteria are numbered in src/common/foreign_items.h. Every id below
// was considered for this pool and REJECTED, and each names the criterion that
// rejected it. Promoting these from prose to a table is what gives the class
// rule an observable: without it, the only evidence "the rule ran" is a table
// that happens to look right, and a row that quietly drifted back in would be
// invisible to CI. The ForeignItemClass lock walks this table and asserts every
// entry is absent from the pool AND absent from the name inverse.
//
// This is deliberately the ADJUDICATED set, not a machine sweep of RG_NONE..
// RG_MAX. Criterion 3 ("the give is unconditionally effectful") is a property of
// the PAIRED world's option profile, which OoT's generation pass cannot read —
// ADR 0011 decision 3.5 / answer O8 keep that criterion blanket until ADR 0010
// increment 2 moves the MM freeze ahead of Fill() and publishes option VALUES
// rather than a digest. A sweep would therefore have to guess criterion 3, and
// guessing it is precisely the promise ("it will be awarded there!") that
// criterion 3 exists to protect.
namespace {
struct ForeignExclusionOoT {
    uint16_t id;
    uint8_t criterion;
};

const ForeignExclusionOoT kForeignExclusionsOoT[] = {
    // (1) Sentinels. RG_NONE is "the fill placed nothing here"; it is not an item.
    { (uint16_t)RG_NONE, (uint8_t)RSBS_FOREIGN_CRIT_REAL_ITEM },
    // (2) Junk-class. A foreign HOST already physically holds an OoT junk item —
    //     that is the degrade invariant — so crossing junk spends one of at most
    //     RSBS_FOREIGN_PLACEMENT_CAP slots on a worse duplicate of what is there.
    { (uint16_t)RG_GREEN_RUPEE, (uint8_t)RSBS_FOREIGN_CRIT_NOT_JUNK },
    // (4) Global world event: the Triforce completion cascade is a per-world goal
    //     quantity, and detonating it from a redemption flush is exactly what a
    //     cross-game seam must not do.
    { (uint16_t)RG_TRIFORCE, (uint8_t)RSBS_FOREIGN_CRIT_NO_WORLD_EVENT },
    { (uint16_t)RG_TRIFORCE_PIECE, (uint8_t)RSBS_FOREIGN_CRIT_NO_WORLD_EVENT },
    // (5) Reward, not punishment. The MM-side pickup text promises an award in
    //     Hyrule; delivering a trap instead would make that text a lie. (The give
    //     still HANDLES RG_ICE_TRAP — it arrives through other paths — but the
    //     cross-game class does not source it.)
    { (uint16_t)RG_ICE_TRAP, (uint8_t)RSBS_FOREIGN_CRIT_REWARD },
    // (6) #525 shared cross-game resources. These are one quantity spanning both
    //     games now, so there is nothing left for them to CROSS. The first three
    //     were REALLY IN THIS TABLE and left with the sharing that replaced them —
    //     "Fairy Bow" and "Bomb Bag" with shared ammo, "Progressive Hookshot" with
    //     RSBS_SHARED_RES_HOOKSHOT_TIER — which is what makes this block a record
    //     of decisions rather than a hypothetical.
    { (uint16_t)RG_FAIRY_BOW, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_BOMB_BAG, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_PROGRESSIVE_HOOKSHOT, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_PROGRESSIVE_BOW, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_PROGRESSIVE_BOMB_BAG, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_PROGRESSIVE_WALLET, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_PROGRESSIVE_MAGIC_METER, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_HEART_CONTAINER, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
    { (uint16_t)RG_PIECE_OF_HEART, (uint8_t)RSBS_FOREIGN_CRIT_NOT_SHARED_RESOURCE },
};

constexpr int kForeignExclusionsOoTCount = sizeof(kForeignExclusionsOoT) / sizeof(kForeignExclusionsOoT[0]);
} // namespace

// Publish the table into src/common's origin-indexed registry (ADR 0009
// decision 3) rather than defining the lookups here. The pool DEFINITION still
// lives in this TU — that is the ADR 0002 invariant, and the RG_* enumerators
// above are why — but the lookups now have to serve two pools, and src/common
// cannot call into either game. Registration inverts that: each pool TU hands
// its static table down, and neither game has to be linkable from the other.
//
// File-scope initializer, so it runs before main() and before any gameplay or
// test code can ask for the pool. This TU lives in soh_rando, which links
// WHOLE_ARCHIVE, so the initializer is never dropped as unreferenced.
namespace {
struct ForeignPoolV1Registrar {
    ForeignPoolV1Registrar() {
        Combo_RegisterForeignItemPool((uint8_t)GAME_OOT, kForeignPoolV1, kForeignPoolCount);
    }
};
const ForeignPoolV1Registrar gForeignPoolV1Registrar;
} // namespace

// ============================================================================
// Redemption give (called by OoT_AwardSharedItem, the A1 consumer callback)
// ============================================================================

extern "C" int OoT_ForeignItem_Give(uint16_t rgId) {
    // Progressive resolution walks Rando::Context / Logic / OTRGlobals; all
    // three exist once OoT has booted to gameplay, which the presence-gated
    // consumption point guarantees. Guard anyway: a give we cannot perform is
    // logged loudly rather than crashing the arrival path.
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandomizer == nullptr ||
        Rando::Context::GetInstance() == nullptr || Rando::Context::GetInstance()->GetLogic() == nullptr) {
        fprintf(stderr, "[OoT] foreign give unavailable (rando context not live), RG id=%u\n", (unsigned)rgId);
        return 0;
    }

    const GetItemEntry entry = Rando::StaticData::RetrieveItem((RandomizerGet)rgId).GetGIEntry_Copy();

    // Mirror savefile.cpp's StartingItemGive dispatch (NULL play: the
    // starting-item precedent; none of the pool's resolved entries touch play).
    if (entry.modIndex == MOD_NONE) {
        if (entry.itemId >= ITEM_RUPEE_GREEN && entry.itemId <= ITEM_RUPEE_GOLD) {
            static const int kRupeeCounts[] = { 1, 5, 20, 50, 200 };
            GiveLinkRupees(kRupeeCounts[entry.itemId - ITEM_RUPEE_GREEN]);
        } else {
            OoT_Item_Give(NULL, (uint8_t)entry.itemId);
        }
        return 1;
    }
    if (entry.modIndex == MOD_RANDOMIZER) {
        if (entry.getItemId == RG_ICE_TRAP) {
            gSaveContext.ship.pendingIceTrapCount++;
        } else {
            Randomizer_Item_Give(NULL, entry);
        }
        return 1;
    }

    fprintf(stderr, "[OoT] foreign give: unhandled modIndex %d for RG id=%u\n", (int)entry.modIndex, (unsigned)rgId);
    return 0;
}

// ============================================================================
// REVERSE DIRECTION (#510): host eligibility + the OoT placement pass
// ============================================================================
//
// The mirror of Rando::Foreign::IsEligibleHost / PlaceForeignItems on the MM
// side (games/mm/2s2h/Rando/Foreign.cpp), for the other direction: MM items
// (kForeignPoolMMV1) hosted in OoT checks, recorded in
// gComboCtx.foreignPlacementsOoT and delivered in Termina by MM_AwardSharedItem.
//
// THE ASYMMETRY THAT MATTERS: **OoT has no RCTYPE_CHEST.** MM classifies a chest
// with a dedicated check type, so its predicate keys on
// `randoCheckType == RCTYPE_CHEST`. OoT's chests are RCTYPE_STANDARD and are
// identified by the ACTOR they are built from — Location::Chest stores
// ACTOR_EN_BOX (location_list.cpp passes it for every chest row) — so the
// equivalent test here is on GetActorID(), not on the check type. Keying this
// on a nonexistent RCTYPE_CHEST would silently accept nothing.
//
// TWO OBJECTS, TWO ACCESSORS. The static class of a check and the item the fill
// actually placed there live in different tables and are reached differently:
//   - Rando::StaticData::GetLocation(rc)          -> Rando::Location*    (static)
//   - Rando::Context::GetInstance()->GetItemLocation(rc) -> Rando::ItemLocation* (fill)
// Reading both off one pointer does not compile; they are separate types.
//
// WHY JUNK-CLASS. Same invariant MM's predicate enforces: a foreign host must
// physically hold a legal junk item of its OWN game, because that is what the
// check degrades to if the placement table is ever absent (a pre-#493 .redsave
// zero-extends to an empty table). Nothing crashes, nothing aliases — the player
// just gets the blue rupee that was really in the chest.
static bool OoT_Foreign_IsEligibleHostImpl(RandomizerCheck rc) {
    if (rc <= RC_UNKNOWN_CHECK || rc >= RC_MAX) {
        return false;
    }

    Rando::Location* loc = Rando::StaticData::GetLocation(rc);
    // A gap in the table is default-constructed and keeps RC_UNKNOWN_CHECK, so
    // an id that does not name a real row fails this identity test.
    if (loc == nullptr || loc->GetRandomizerCheck() != rc) {
        return false;
    }

    // Tier A, and the only tier: an actual treasure chest.
    if (loc->GetActorID() != ACTOR_EN_BOX) {
        return false;
    }

    // Commerce is excluded EXPLICITLY even though no shop/scrub/merchant row is
    // ACTOR_EN_BOX today. Their give-and-price flow and their spoiler shape both
    // differ from the ordinary collect path this presentation targets, so the
    // exclusion must survive any future widening of the actor test (the same
    // reasoning MM's IsAllowedHostClass states for RCTYPE_SHOP).
    const RandomizerCheckType checkType = loc->GetRCType();
    if (checkType == RCTYPE_SHOP || checkType == RCTYPE_SCRUB || checkType == RCTYPE_MERCHANT ||
        checkType == RCTYPE_CHEST_GAME) {
        return false;
    }
    if (loc->IsShop()) {
        return false;
    }

    // The fill-side half. RG_NONE is "the fill placed nothing here" (an
    // unshuffled or unreached location) — the OoT analogue of MM's `.shuffled`
    // test, and the reason a ROM-free run with no fill accepts nothing.
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return false;
    }
    Rando::ItemLocation* itemLoc = ctx->GetItemLocation(rc);
    if (itemLoc == nullptr) {
        return false;
    }
    const RandomizerGet placedItem = itemLoc->GetPlacedRandomizerGet();
    if (placedItem == RG_NONE) {
        return false;
    }

    return Rando::StaticData::RetrieveItem(placedItem).GetCategory() == ITEM_CATEGORY_JUNK;
}

// ============================================================================
// THE REVERSE PASS'S REACHABILITY GATE (#656; ADR 0010 increment 1.3, P6)
// ============================================================================
//
// WHAT WAS ASYMMETRIC. Increment 1.3's reachability requirement (PR #580) was
// applied to the FORWARD pass only: Rando::Foreign::PlaceForeignItems filters
// its candidates through Rando::Logic::ComputeReachableCheckSet(), while this
// file's reverse pass accepted every eligible chest in 1..RC_MAX regardless of
// whether OoT's own solver can reach it. Under a setting that does not
// guarantee full reachability that silently hosts an MM item on an OoT check
// the player can never open — a beatability break with no signal until someone
// notices the check is unobtainable.
//
// THE ORACLE IS THE FILL'S OWN, not a second definition of reachable.
// ReachabilitySearch (3drando/fill.hpp) walks the region graph from RR_ROOT with
// the starting inventory applied and every placed item collected as it goes, and
// marks each location it reaches with ItemLocation::AddToPool(). That mark is the
// one ValidateEntrances tests to decide ctx->allLocationsReachable (fill.cpp:
// "Location ... not reachable"), so "reachable" here is the fill's own notion
// rather than a second one that could drift from it; the drift would look like a
// placement bug.
//
// WHY THE MARKS MUST BE RECOMPUTED RATHER THAN READ. addedToPool is scratch
// state that every search resets (ResetLogic -> Context::LocationReset), so
// whatever survives Fill() is the residue of whichever search ran last —
// CalculateBarren's, not a closure over the finished world. Reading it would be
// a gate whose answer depends on the order of the calculations before it.
//
// ...AND WHY THE RECOMPUTE MUST RESET Logic FIRST. This is the correction that
// makes the gate correct rather than merely present, and it was MEASURED, not
// reasoned: ResetLogic's own comment is "Reset non-Logic-class logic", and it
// does not touch the `logic` singleton's simulated inventory at all — it only
// clears region access and the pool marks, then ADDS the starting inventory on
// top of whatever items the previous search happened to leave collected. So
// ReachabilitySearch alone is not a function of the finished world; it is a
// function of the finished world PLUS the residue of the last search. Without
// the logic->Reset() below, this gate reported 48 of 57 eligible hosts reachable
// when it ran at the tail of a real generation and 57 of 57 when the same pass
// ran again a moment later — it silently removed nine reachable hosts and
// changed which checks the world used, and ForeignPlacementOoT's "re-arming must
// reproduce the same table" assertion is what caught it. Every other caller that
// wants a closure over a finished world resets first the same way
// (GeneratePlaythrough: logic->Reset() then ResetLogic; IsBeatableWithout:
// logic->Reset() then CheckBeatable).
//
// VACUOUS UNDER THE SHIPPED DEFAULT, AND THAT IS THE POINT. RSK_ALL_LOCATIONS_
// REACHABLE defaults to RO_GENERIC_ON (settings.cpp), so a default seed has
// every location in the closure and this gate removes no candidate — which is
// what keeps the reverse pass's placements byte-identical to the pre-gate ones.
//
// THAT USED TO READ "and therefore the rando tier's determinism digests", which
// credited the wrong rows (#688): SeedDeterminism and its siblings diff two runs
// of the SAME binary, so a gate that deterministically dropped nine reachable
// hosts passed all of them — which is precisely how the stale-inventory defect
// above reached a green tier. Since #688 the rows that hold this claim are the
// GOLDEN ones (GoldenSeedDigestDefault / GoldenSeedDigestProfileV1), which
// compare one run against `tests/golden/`; a gate that starts dropping hosts
// under the shipped default moves foreignOoTHash, foreignOoTCount and the
// per-slot foreignOoT<n> lines and turns them red. It earns its place
// on the non-default settings (ALR off, and the no-logic rules) where
// unreachable locations genuinely exist. The counters below make the vacuity
// MEASURED rather than asserted: the lock reads eligible vs reachable and fails
// if the gate ever drops a host under the shipped default, which is exactly how
// the stale-inventory defect above was found.
//
// NO RNG, AND NOTHING AFTER IT DRAWS. Neither logic->Reset() nor
// ReachabilitySearch consumes a random number (the fill already calls the search
// inside its own playthrough work, before the spoiler is written), and this pass
// runs after Fill(), GenerateHash() and SpoilerLog_Write(), so it cannot shift a
// draw the fill already made. The state it mutates — region access flags, the
// simulated inventory, the pool marks — is generation scratch: Logic::Reset's
// NewSaveContext() allocates a fresh heap SaveContext and never writes
// gSaveContext, and nothing in the tree reads addedToPool after generation
// (SeedContext.cpp's only use is another reset).
static int sOoTLastEligibleHosts = 0;
static int sOoTLastReachableHosts = 0;
// Production is always true. The reverse-placement lock flips it off so it can
// install a hand-made reachability mark set and prove the PASS honours it —
// otherwise the pass's own recompute would erase the injected state and the
// lock could only ever test the predicate, never the gate. See
// OoT_Foreign_TestSetReachabilityRecompute.
static bool sOoTRecomputeHostReachability = true;

// Recompute the closure, for its SIDE EFFECT on the pool marks. The return value
// is deliberately ignored: with every location holding a placed item and
// calculatingAvailableChecks false, ReachabilitySearch never pushes a placed
// location into accessibleLocations (fill.cpp AddCheckToLogic), so the returned
// vector is empty while the AddToPool marks it set along the way are the complete
// closure. Those marks are what ValidateEntrances reads.
//
// logic->Reset() FIRST, and it is load-bearing — see the block above. Without it
// the search starts from the previous search's collected inventory and the gate
// is not a function of the finished world.
static void OoT_Foreign_RecomputeHostReachability() {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || logic == nullptr) {
        // No fill in this process: nothing to compute a closure over. Leaving the
        // marks alone rather than clearing them keeps the pass's behaviour
        // decided by the eligibility predicate, which is the only oracle a
        // fill-less process has.
        return;
    }
    logic->Reset();
    ReachabilitySearch(ctx->allLocations);
}

static bool OoT_Foreign_IsReachableHostImpl(RandomizerCheck rc) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return false;
    }
    Rando::ItemLocation* itemLoc = ctx->GetItemLocation(rc);
    return itemLoc != nullptr && itemLoc->IsAddedToPool();
}

// Local, self-contained PRNG for placement selection — deliberately NOT drawn
// from Random_Init's stream. The fill consumes that stream, so taking numbers
// out of it here would shift every subsequent draw and change the OoT world
// itself as a side effect of the cross-game feature being on. Same reasoning,
// same xorshift32, as Rando::Foreign's sSelectState.
static uint32_t sOoTSelectState;

static uint32_t OoT_Foreign_SelectNext() {
    uint32_t x = sOoTSelectState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sOoTSelectState = (x != 0) ? x : 0xB5297A4Du;
    return sOoTSelectState;
}

/**
 * Place MM items into eligible OoT checks. Called once from Playthrough_Init,
 * AFTER the gComboCtx pairing stamp (the identity this derives from must be live).
 *
 * @return the number of placements made (>= 0), or NEGATIVE if the pairing is
 *         active but the pass could not honour it at all — see the note below.
 *
 * ERROR SIGNALLING — RETURN CODE, NEVER AN EXCEPTION. OoT's generation chain has
 * no try/catch anywhere: Rando_HeadlessSeedTest (extern "C") -> GenerateRandomizer
 * -> Playthrough_Init. A throw would cross that extern "C" boundary uncaught and
 * std::terminate the process, killing the headless SeedDeterminism/MMRandoGen CI
 * rows and the live GUI generate alike. GenerateRandomizer already speaks return
 * codes (`if (ret < 0) return false`), so shortfall rides that convention.
 *
 * A PARTIAL placement is NOT a shortfall and must not fail generation: the pool
 * is ~128 entries against a cap of 8, so placing fewer than the pool is the
 * normal, intended outcome. Only two states are errors, and both mean the
 * cross-game half of a PAIRED world would be silently absent:
 *   -1  the MM pool is not registered at all (the Mode-B elision class — if
 *       2ship_rando's pool TU is ever dropped from the link this turns a silent
 *       feature loss into a loud generation failure)
 *   -2  no eligible host exists in the finished fill. OoT has ~100 chest rows and
 *       junk is plentiful, so zero is not a reachable fill outcome; it means the
 *       predicate and the table have drifted apart.
 */
extern "C" int OoT_PlaceForeignItems(void) {
    // A re-generated world must not inherit the previous one's placements.
    Combo_ClearForeignPlacementsOoT();

    if (!Combo_ForeignPairingActive()) {
        return 0; // solo OoT rando: nothing to pair with, and that is normal
    }

    // THE DIRECTION GATE (ADR 0011 increment 4, #493's "the direction byte").
    // Read from the FROZEN record — never a live CVar — through the same
    // accessor MM's forward pass uses for its own origin. GAME_MM is this
    // pass's origin: it places MM-ORIGIN items into OoT checks, so it is armed
    // by RSBS_COMBO_DIR_REVERSE and by RSBS_COMBO_DIR_BOTH.
    //
    // Zero placements is the correct outcome, NOT an error: RSBS_COMBO_DIR_OFF
    // and RSBS_COMBO_DIR_FORWARD both describe real, chooseable paired worlds
    // (ADR 0011 decision 2.3), so this returns 0 like the solo case rather than
    // one of the negative shortfall codes, which mean "a paired world's
    // cross-game half would be SILENTLY absent". Under the shipped default
    // (BOTH) this predicate is true and nothing moves — the parity that keeps
    // foreignOoTHash byte-stable, which since #688 is checked by the GOLDEN row
    // GoldenSeedDigestDefault and not (as this comment used to say) by
    // SeedDeterminism, which cannot see a deterministic move at all.
    if (!Combo_ComboDirectionArms((uint8_t)GAME_MM)) {
        fprintf(stderr,
                "[OoT] foreign placement: direction=%u does not arm MM-origin crossings — no reverse placements "
                "(frozen=%d)\n",
                (unsigned)Combo_ComboDirection(), Combo_ComboSettingsFrozen() ? 1 : 0);
        return 0;
    }

    const ComboForeignItemDef* pool = nullptr;
    const int poolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &pool);
    if (poolCount <= 0 || pool == nullptr) {
        fprintf(stderr, "[OoT] foreign placement: pairing is active but MM's source pool is empty — "
                        "2ship_rando's pool TU was elided (#510)\n");
        return -1;
    }

    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        fprintf(stderr, "[OoT] foreign placement: no Rando::Context\n");
        return -1;
    }

    // THE REACHABILITY GATE (#656), computed before the candidate walk and
    // before the selection stream is seeded. Symmetric with the forward pass,
    // which computes Rando::Logic::ComputeReachableCheckSet() at the same point
    // for the same reason. See the block above OoT_Foreign_Recompute-
    // HostReachability for the oracle, the vacuity under the shipped default and
    // why it consumes no RNG.
    if (sOoTRecomputeHostReachability) {
        OoT_Foreign_RecomputeHostReachability();
    }

    // Candidates in ascending RandomizerCheck order. Walking the enum range
    // rather than ctx->allLocations keeps the order fixed by construction — it
    // cannot be perturbed by pool-bookkeeping changes — which is what a STORED
    // digest needs (the golden rows; #688). Same shape as the digest's own walk.
    //
    // Reachability composes OUTSIDE OoT_Foreign_IsEligibleHostImpl, exactly as
    // it does on MM's side and for the same reason: that predicate is also the
    // LOAD path's gate and the ROM-free eligibility lock's subject, neither of
    // which has a reachability closure to consult.
    std::vector<RandomizerCheck> candidates;
    int eligibleHosts = 0;
    for (int i = 1; i < RC_MAX; i++) {
        const RandomizerCheck rc = (RandomizerCheck)i;
        if (!OoT_Foreign_IsEligibleHostImpl(rc)) {
            continue;
        }
        eligibleHosts++;
        if (OoT_Foreign_IsReachableHostImpl(rc)) {
            candidates.push_back(rc);
        }
    }
    sOoTLastEligibleHosts = eligibleHosts;
    sOoTLastReachableHosts = (int)candidates.size();

    // Both counts printed every generation, like MM's pass prints its own: host
    // supply is a number worth watching in CI logs and playtest output BEFORE it
    // becomes a shortfall, and the gap between them is how much work the
    // reachability gate did for this world.
    fprintf(stderr,
            "[OoT] foreign placement: %d MM pool items over %zu reachable eligible host checks (%d eligible before "
            "the reachability gate)\n",
            poolCount, candidates.size(), eligibleHosts);

    if (candidates.empty()) {
        // -2 keeps its meaning: "a paired world's cross-game half would be
        // SILENTLY absent". It now covers one more cause — every eligible host
        // is outside OoT's own reachable closure — and that is a generation
        // failure for exactly the same reason the predicate-drift case is. The
        // alternative, placing into the unreachable ones anyway, is the #656 bug.
        fprintf(stderr,
                "[OoT] foreign placement: no REACHABLE eligible OoT host check in this fill (%d eligible, all "
                "outside the reachable closure) (#510/#656)\n",
                eligibleHosts);
        return -2;
    }

    // Deterministic from the paired-world identity alone: same seed + same
    // settings profile => same placements. gComboCtx.sharedRandoSettingsHash is
    // live by now (Playthrough_Init stamps it immediately before calling us), and
    // the ":foreign-oot-v1" suffix keeps this stream disjoint from MM's
    // ":foreign-v1" one so the two directions can never shadow each other.
    sOoTSelectState = SohUtils::Hash(std::to_string(ctx->GetSeed()) + ":" +
                                     std::to_string(gComboCtx.sharedRandoSettingsHash) + ":foreign-oot-v1");
    if (sOoTSelectState == 0) {
        sOoTSelectState = 0xB5297A4Du;
    }

    // BOTH sides are drawn without replacement. Drawing the ITEM matters as much
    // as drawing the host: the pool is far larger than the cap, so walking it in
    // order (as the forward pass does, where pool <= cap made that equivalent)
    // would place the same first 8 entries in every seed and make the other ~126
    // dead weight.
    //
    // WHICH pool entries are drawable is now the RULE (#495, ADR 0011 decision
    // 3): Combo_ForeignPoolDrawFor filters MM's pool by the FROZEN itemClassMM
    // bitset, in pool order. With the shipped defaults (every allocated bit) this
    // is the identity permutation 0..poolCount-1 — byte-identical to the list
    // this loop used to build by hand — which is what keeps foreignOoTHash from
    // moving, as GoldenSeedDigestDefault checks and SeedDeterminism never could
    // (#688). There is NO seed term in the class (accepted
    // answer O3): variety comes from the draw below, and a seed-varying class
    // would make the spoiler-load name inverse partial.
    std::vector<int> poolIndices((size_t)poolCount, 0);
    const int drawable = Combo_ForeignPoolDrawFor((uint8_t)GAME_MM, poolIndices.data(), poolCount);
    poolIndices.resize((size_t)(drawable > 0 ? drawable : 0));
    if (poolIndices.empty()) {
        // Every class unarmed for this direction. A real, chooseable world under
        // ADR 0011 decision 3.3 (the direction byte, not this, is what says
        // "off"), so it is a loud log and zero placements rather than a failure.
        fprintf(stderr, "[OoT] foreign placement: MM item classes %04X select no pool entry — no crossings\n",
                (unsigned)Combo_ComboItemClassFor((uint8_t)GAME_MM));
        return 0;
    }

    // How many crossings this direction may make comes from the FROZEN COMBO
    // RECORD (ADR 0011 decision 1, accepted answer O4), not from the cap alone.
    // Combo_ComboPoolSizeFor already clamps to RSBS_FOREIGN_PLACEMENT_CAP and
    // falls back to it for an unfrozen record, so the shipped defaults place
    // exactly what this line placed before the record existed — a count that
    // could exceed the table's capacity would be a setting that lies, and a
    // zero-extended legacy record must never resolve to "no crossings".
    const int poolSize = Combo_ComboPoolSizeFor((uint8_t)GAME_MM);
    // FILTER FIRST, THEN DRAW TO COUNT. The class rule decides WHICH entries are
    // drawable; the pool size decides HOW MANY of them get placed. Bounding on
    // poolIndices.size() rather than poolCount is what makes the two compose —
    // bounding on the raw pool would let the draw index past the filtered list.
    const int wanted = std::min({ (int)poolIndices.size(), poolSize, (int)candidates.size() });
    // The direction reached here is necessarily one that ARMS this pass — the
    // gate above returned already if it did not (ADR 0011 increment 4). It is
    // still printed, because "which rules produced this world" is the line a
    // reader of a generation log looks for first.
    fprintf(stderr,
            "[OoT] foreign placement: combo rules direction=%u poolSizeMM=%d classMM=%04X (%zu of %d pool entries in "
            "class) (frozen=%d)\n",
            (unsigned)Combo_ComboDirection(), poolSize, (unsigned)Combo_ComboItemClassFor((uint8_t)GAME_MM),
            poolIndices.size(), poolCount, Combo_ComboSettingsFrozen() ? 1 : 0);

    int placed = 0;
    for (int i = 0; i < wanted; i++) {
        const size_t poolPick = (size_t)(OoT_Foreign_SelectNext() % (uint32_t)poolIndices.size());
        const int poolEntry = poolIndices[poolPick];
        poolIndices.erase(poolIndices.begin() + (std::ptrdiff_t)poolPick);

        const size_t hostPick = (size_t)(OoT_Foreign_SelectNext() % (uint32_t)candidates.size());
        const RandomizerCheck hostCheck = candidates[hostPick];
        candidates.erase(candidates.begin() + (std::ptrdiff_t)hostPick);

        if (Combo_SetForeignPlacementOoT((uint16_t)hostCheck, pool[poolEntry].item) >= 0) {
            placed++;
            fprintf(stderr, "[OoT] foreign placement: '%s' hosted at OoT check %s\n", pool[poolEntry].name,
                    Rando::StaticData::GetLocation(hostCheck)->GetName().c_str());
        }
    }

    return placed;
}

// ROM-free test bridge (redship tier; src/common/tests/test_foreign_items.c).
// The SAME predicate the candidate loop above calls — exposed rather than
// paraphrased in the test for the reason MM_Rando_Foreign_IsEligibleHost is: a
// lock that restates the rule stops testing it the moment the rule moves.
extern "C" int OoT_Foreign_IsEligibleHost(uint16_t rc) {
    return OoT_Foreign_IsEligibleHostImpl((RandomizerCheck)rc) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// The reachability gate's test surface (#656). Four bridges, each exposing a
// fact the lock cannot otherwise observe, and none of them called by production.
// ---------------------------------------------------------------------------

/** The reachability term ALONE, for the check the candidate walk would ask
 *  about. Exposed rather than paraphrased for the same reason
 *  OoT_Foreign_IsEligibleHost is: a lock that restates the rule stops testing it
 *  the moment the rule moves. */
extern "C" int OoT_Foreign_IsReachableHost(uint16_t rc) {
    return OoT_Foreign_IsReachableHostImpl((RandomizerCheck)rc) ? 1 : 0;
}

/** Eligible hosts counted by the LAST placement pass, before the gate. */
extern "C" int OoT_Foreign_TestLastEligibleHosts(void) {
    return sOoTLastEligibleHosts;
}

/** ...and after it. Equal to the above under the shipped default (All Locations
 *  Reachable is on), which is how the lock asserts the gate moves nothing there
 *  instead of taking it on trust. */
extern "C" int OoT_Foreign_TestLastReachableHosts(void) {
    return sOoTLastReachableHosts;
}

/**
 * Turn the pass's own closure recompute off (0) or on (1), returning the
 * previous setting.
 *
 * TEST-ONLY, AND LOAD-BEARING FOR NON-VACUITY. With the recompute on, any
 * reachability state a lock installs by hand is erased by the pass's first act,
 * so the only thing a lock could prove is that the predicate distinguishes —
 * never that the PASS honours it. With it off, a lock can mark a specific host
 * unreachable, run the real OoT_PlaceForeignItems, and assert that host is never
 * chosen: the assertion that goes red if the gate is deleted. Production never
 * calls this, and the flag defaults to the production behaviour, so a build that
 * lost the test TU still gates.
 */
extern "C" int OoT_Foreign_TestSetReachabilityRecompute(int enable) {
    const int previous = sOoTRecomputeHostReachability ? 1 : 0;
    sOoTRecomputeHostReachability = (enable != 0);
    return previous;
}

/** Set (1) or clear (0) one check's reachability mark, through the same
 *  ItemLocation pool API the fill's own search uses. Returns 1 when the mark was
 *  applied. Test-only; pairs with the recompute switch above. */
extern "C" int OoT_Foreign_TestSetHostReachable(uint16_t rc, int reachable) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return 0;
    }
    Rando::ItemLocation* itemLoc = ctx->GetItemLocation((RandomizerCheck)rc);
    if (itemLoc == nullptr) {
        return 0;
    }
    if (reachable != 0) {
        itemLoc->AddToPool();
    } else {
        itemLoc->RemoveFromPool();
    }
    return 1;
}

// ============================================================================
// REVERSE DIRECTION (#493): the PICKUP CORE — the OoT twin of
// Rando::Foreign::RecordForeignPickup
// ============================================================================
//
// WHY THIS IS A NAMED FUNCTION AND NOT THE FOUR LINES IT REPLACES. Until now
// the reverse leg's producer lived entirely inside the RC-queue drain lambda in
// hook_handlers.cpp, which needs a live PlayState, a Player and the CheckTracker
// — so nothing ROM-free could enter it, and #493's own Lock section says in as
// many words that a row which pokes the placement table instead "would be
// vacuous". The forward direction has had the extraction since Lane C1
// (Rando::Foreign::RecordForeignPickup + the MM_Rando_Foreign_RecordPickup
// bridge); this is the missing twin, and the ForeignItemGiveReverse row now
// drives it.
//
// The split is deliberately the SAME one MM makes: this function owns the
// DECISION and the durable RECORD, and owns nothing about presentation. The
// toast, the tracker write and the queue pop stay at the hook, because they are
// display/gameplay concerns that a display-free tier cannot honestly assert.
//
// THE PAIRING REFUSAL IS THE #610 RULE, APPLIED TO THE DIRECTION THAT DID NOT
// HAVE IT. Combo_RecordSharedItem takes no identity argument and performs no
// identity check (src/common/shared_items.c): whatever it records is redeemed by
// whichever paired MM world arrives next, blind to which world authored it. MM's
// forward-side core has refused to author such a record since #610; the reverse
// side did not, so an OoT session whose foreignPlacementsOoT survived into an
// unpaired state (a .redsave loaded into a solo session, a future spoiler-drop
// route, a session invalidation that KEEPs the table) could mint a crossing with
// no paired world to receive it. The check DEGRADES to the junk-class OoT item
// the host physically holds — the documented absent-placement behaviour
// (foreign_items.h) — rather than to a crossing nobody can receive.
//
// @return true only when a durable shared-item record was authored. The caller
//         presents the foreign pickup if and only if that happened.
static bool OoT_Foreign_RecordPickupImpl(uint16_t rc) {
    const SharedItem* item = Combo_GetForeignPlacementForOoTCheck(rc);
    if (item == nullptr) {
        return false;
    }

    if (!Combo_ForeignPairingActive()) {
        fprintf(stderr,
                "[OoT] foreign pickup REFUSED: OoT check %u holds a foreign placement, but this session has no live "
                "cross-game pairing (sourceIsRando=%d settingsHash=%08X). No durable shared-item record is authored — "
                "there is no paired world it could belong to (#610/#493)\n",
                (unsigned)rc, gComboCtx.sourceIsRando ? 1 : 0, gComboCtx.sharedRandoSettingsHash);
        fflush(stderr);
        return false;
    }

    // Durable immediately (Combo_RecordSharedItem writes the serialized array,
    // so an OoT save+quit before the next switch cannot lose the pickup — the
    // stage/commit outbox is RAM-only, see shared_items.h). The producer de-dups
    // an identical un-redeemed entry, so a re-fired queue cannot double-record.
    return Combo_RecordSharedItem((GameId)item->originGame, item->id) >= 0;
}

/**
 * The reverse direction's give-path entry point, called by the RC-queue drain
 * (hook_handlers.cpp) and driven directly by the ForeignItemGiveReverse lock.
 *
 * The exact twin of MM_Rando_Foreign_RecordPickup, and named to match: one
 * production function, two callers, so the lock covers the real recording path
 * rather than a copy of it.
 *
 * @return 1 if a durable crossing was authored for this OoT check, 0 otherwise
 *         (no foreign placement here, or no live pairing to author it for).
 */
extern "C" int OoT_Rando_Foreign_RecordPickup(uint16_t rc) {
    return OoT_Foreign_RecordPickupImpl(rc) ? 1 : 0;
}

/**
 * Walk the CRITERION-ATTRIBUTED EXCLUSION table (#495, ADR 0011 decision 3.4):
 * entry `index`'s rejected RG_* id and the criterion number that rejected it.
 *
 * The observable that makes the class rule testable. src/common has no OoT enum
 * in scope by design, so the lock cannot name RG_FAIRY_BOW itself; it walks this
 * bridge instead and asserts each excluded id is absent from the pool and from
 * the name inverse. Exposed rather than re-listed in the test for the standing
 * reason: a lock that keeps its own copy of the rule stops testing the rule.
 *
 * @return 1 while `index` names an entry, 0 once it is past the end.
 */
extern "C" int OoT_ForeignItem_TestExclusionAt(int index, uint16_t* outId, uint8_t* outCriterion) {
    if (index < 0 || index >= kForeignExclusionsOoTCount) {
        return 0;
    }
    if (outId != nullptr) {
        *outId = kForeignExclusionsOoT[index].id;
    }
    if (outCriterion != nullptr) {
        *outCriterion = kForeignExclusionsOoT[index].criterion;
    }
    return 1;
}

// ============================================================================
// THE MERGED CREATION EVENT (ADR 0010 increment 2; #564's creation-event
// contract; epic #644)
// ============================================================================
//
// WHERE IT RUNS. games/oot/src/code/z_sram.c, inside Save_InitFile, after
// Context_InvalidateSessionOnNewGame retires the previous cross-game session
// and after Randomizer_InitSaveFile authors OoT's half, and BEFORE
// Save_SaveFile writes the slot. That ordering is the whole design: the
// .redsave this file's very first save produces already carries a complete,
// armed MM half, so arrival has nothing left to author.
//
// WHY THE ORCHESTRATOR IS HERE AND NOT IN z_sram.c. games/oot/src/**.c has no
// src/common on its include path (the same reason
// Context_InvalidateSessionOnNewGame is declared locally there), and the seam
// needs gComboCtx, the MM bridges and a 136KB scratch buffer. z_sram.c declares
// one function and calls it; everything structural lives in this C++ TU, which
// already owns the reverse crossing pass.
//
// THE SNAPSHOT BRACKET IS NOT OPTIONAL. gSaveContext is ONE buffer shared by
// both games (src/common/unified_save.c) reinterpreted through two layouts, so
// MM's generation writes over the OoT file this seam is in the middle of
// creating. docs/solver-inventory.md §6.1 amendment (1) names this bracket as
// the first thing any coordinator must do around an MM call, and this is its
// first production instance. The buffer is snapshotted whole
// (OOT_SAVE_CONTEXT_SIZE, the larger of the two layouts) rather than at either
// game's sizeof, because a partial restore would leave MM's bytes visible past
// OoT's struct end.
//
// FAILURE IS TOTAL (ADR 0010 increment 2). "Generation failure fails the
// creation, at file select, wholly — no partial identity, no vanilla Termina."
// On any nonzero return from the MM half this function rolls the identity back
// to nothing, clears both placement tables, and returns nonzero; z_sram.c then
// abandons the file. The caller sees one boolean and a reason string.

// The MM half of the creation event (games/mm/2s2h/GameExports_SingleExe.cpp).
extern "C" int MM_Rando_GenerateAtCreation(int slot, const char* ootSpoilerPath);
// The #533 refusal surface (src/common/save.h) and this file's own
// file-select failure toast, both raised from the failure branch below so
// that ONE callable carries the whole terminal-failure contract — z_sram.c
// only has to not write the file, and the lock can drive the surface without
// standing up OoT's file select.
extern "C" void RsbsSave_RefuseSlotGeneration(int slot);
extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason);
// The ON-SCREEN progress surface's presentation half (#582),
// games/oot/soh/SohGui/CreationProgressOverlay.cpp. Installed from the seam
// below rather than at boot so the thread it latches as "the renderer's" is
// provably this one.
extern "C" void OoT_CreationProgressOverlay_Install(void);

// The unified buffer's true capacity. context.h (already included above) pulls
// game.h, so OOT_SAVE_CONTEXT_SIZE is in scope; this assertion is what makes
// "snapshot the whole buffer" mean what it says.
static_assert(sizeof(SaveContext) <= OOT_SAVE_CONTEXT_SIZE,
              "OoT's runtime SaveContext outgrew the unified gSaveContext storage "
              "(src/common/unified_save.c); raise OOT_SAVE_CONTEXT_SIZE in src/common/game.h");
// ...and the fact that makes the bracket below SAFE at OoT's sizeof rather than
// at the unified capacity: MM's whole SaveContext fits INSIDE OoT's, so MM's
// generation cannot write past OoT's struct end and nothing beyond it needs
// restoring. Compile-time rather than a comment, because if MM's capacity ever
// overtook OoT's struct the bracket would start silently under-restoring.
static_assert(MM_SAVE_CONTEXT_SIZE <= sizeof(SaveContext),
              "MM's SaveContext capacity outgrew OoT's struct, so MM's generation can now write past the region "
              "the creation event's snapshot bracket restores (ForeignItemsSingleExe.cpp)");

// ----------------------------------------------------------------------------
// THE BRACKET'S STORAGE, AND THE ON-SCREEN OVERLAY'S WINDOW INTO IT (#582)
// ----------------------------------------------------------------------------
//
// The snapshot buffer was a static local inside OoT_RunPairedCreationEvent; it
// is at file scope now because a SECOND caller needs it. The overlay paints
// FROM INSIDE the bracketed call (that is the whole point of #582: the thread
// that would draw is the thread MM's fill is running on), and
// `Gui::StartDraw()` -> `Gui::DrawMenu()` draws every registered GuiWindow —
// including SoH's item and check trackers (SohGui.cpp's AddGuiWindow calls),
// which read gSaveContext every frame. Inside the bracket that buffer holds
// MM's world reinterpreted through OoT's layout, so an open tracker would read
// MM bytes as OoT inventory: nonsense at best, an out-of-range table index at
// worst, in the middle of creating the player's file.
//
// NAMING THE DRAW SITE CORRECTLY IS LOAD-BEARING, and the first cut of #582 got
// it wrong in a way that misplaced the fix. It said `Gui::EndDraw` /
// `DrawFloatingWindows` and handed this bracket only the overlay's own ImGui
// draw — so the trackers, which draw one statement EARLIER inside
// `Gui::StartDraw()`, were outside it and still read MM's bytes. In this tree
// `Gui::DrawFloatingWindows()` draws no SoH window at all; it is
// ImGui::UpdatePlatformWindows / RenderPlatformWindowsDefault under
// ImGuiConfigFlags_ViewportsEnable. The painter now hands this bracket the WHOLE
// StartDraw -> EndFrame sequence (CreationProgressOverlay.cpp), which is what
// makes the guarantee below true of every widget on a pumped frame rather than
// only of the progress window.
//
// So a painted frame runs under OoT's bytes and MM's are put back afterwards.
// The swap is WHOLE-BUFFER in both directions, which is what makes it invisible
// to the fill: MM's generation cannot observe a buffer that is byte-identical
// before and after every call it did not make. Two statics rather than one
// because both worlds have to be live at once for the duration of the paint;
// sizeof(SaveContext) each, for the reason the bracket itself is that size (the
// static_assert above).
//
// OUTSIDE a bracket this is a plain call-through, which is what makes the
// headless rows and the menu-side generation path see no new behaviour at all.
static char sOoTSaveSnapshot[sizeof(SaveContext)];
static char sMmInFlightSave[sizeof(SaveContext)];
static bool sCreationBracketActive = false;

extern "C" void OoT_Creation_PaintWithOoTSaveVisible(void (*paint)(void)) {
    if (paint == nullptr) {
        return;
    }
    if (!sCreationBracketActive) {
        paint();
        return;
    }
    memcpy(sMmInFlightSave, &gSaveContext, sizeof(SaveContext));
    memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    // RAII rather than a trailing memcpy: this runs inside MM's fill, and an
    // escaping exception (the fill itself throws GenerationTimeout and
    // std::runtime_error) that skipped the restore would hand the rest of the
    // fill OoT's bytes to work on.
    struct MmSaveRestore {
        ~MmSaveRestore() {
            memcpy(&gSaveContext, sMmInFlightSave, sizeof(SaveContext));
        }
    } restore;
    paint();
}

/**
 * An FNV-1a signature of the WHOLE live gSaveContext (#582's review).
 *
 * TEST SEAM WITH A PURPOSE THE POST-HOC COMPARISON COULD NOT SERVE. The
 * combo-creation-event row already compares gSaveContext byte for byte AFTER the
 * creation returns, and that comparison is green whether or not the paint bracket
 * exists at all — the unconditional restore above puts OoT's bytes back either
 * way. What it cannot see is which world's bytes a widget drawn on a PUMPED FRAME
 * read while the creation was still in flight, which is exactly the property the
 * bracket is for and exactly where the first cut of #582 got it wrong.
 *
 * So the bytes get a cheap identity a test-registered GuiWindow can record from
 * inside `Gui::DrawMenu()`'s own loop — the same loop the item and check trackers
 * draw from — and compare against the post-creation value. A signature rather
 * than a copy because the observer runs inside a frame and must not spend 136 KB
 * of memcpy per draw, and because equality is the whole question.
 */
extern "C" uint32_t OoT_Creation_SaveSignature(void) {
    const unsigned char* bytes = (const unsigned char*)&gSaveContext;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(SaveContext); i++) {
        hash ^= (uint32_t)bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

/**
 * IS THE LIVE gSaveContext OoT's SNAPSHOT RIGHT NOW? Asked from inside a pumped
 * frame, while the creation is still in flight (#582's review).
 *
 * @return 0 when no creation bracket is active, so there is nothing to check;
 *         1 when a bracket IS active and the live buffer is byte-identical to
 *         OoT's snapshot — i.e. this frame's widgets are looking at the same world
 *         the surrounding file-select frames look at;
 *        -1 when a bracket is active and the live buffer is MM's in-flight world.
 *
 * WHY THE COMPARISON HAPPENS HERE AND NOT IN THE TEST. The question is only
 * meaningful DURING the bracket, and a row cannot see inside a frame. Recording a
 * signature and comparing it afterwards is not equivalent and was tried first: the
 * LAST frame of a creation is the terminal paint, which happens after the bracket
 * closed, so a remembered-last-value observer reads OoT's world no matter how
 * wrong the bracket is, and the check passes vacuously. The verdict has to be
 * formed frame by frame, while the answer can still be "no".
 *
 * memcmp rather than the signature above because exactness is free here: this runs
 * at most at the overlay's 10 Hz repaint interval, and only in a process that
 * registered the observer.
 */
extern "C" int OoT_Creation_LiveSaveIsOoTSnapshot(void) {
    if (!sCreationBracketActive) {
        return 0;
    }
    return memcmp(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext)) == 0 ? 1 : -1;
}

/**
 * Run the MM half of the creation event over a snapshot-bracketed
 * gSaveContext, and publish or retract the pairing identity accordingly.
 *
 * @param slot the slot being created (gSaveContext.fileNum at the seam).
 * @return 1 when the whole creation succeeded (including "this is not a paired
 *         file", which succeeds by authoring nothing); 0 when the paired
 *         creation FAILED and the file must not be written.
 */
extern "C" int OoT_RunPairedCreationEvent(int slot) {
    if (!Combo_ForeignPairingActive()) {
        // A vanilla file, or a rando file whose stamp the KEEP identity check
        // discarded (#597). Nothing to author; not a failure.
        return 1;
    }

    // The pre-Fill gate's answer, read HERE FROM THE FROZEN RECORD rather than
    // re-resolved from the CVars (#657/#667). That is the whole point of the
    // gate's second act: the seam must be unable to reach a different conclusion
    // than the creation did, and the only way to guarantee that is to read the
    // record the freeze wrote. Combo_ComboDirectionArms serves the frozen
    // direction once Combo_ComboSettingsFrozen() is true, so this is the same
    // fact both placement passes gate on.
    const bool crossingsAuthored =
        Combo_ComboDirectionArms((uint8_t)GAME_OOT) || Combo_ComboDirectionArms((uint8_t)GAME_MM);
    fprintf(stderr,
            "[OoT] creation event: slot %d, masterSeed=%u settingsHash=%08X mmProfileDigest=%08X "
            "comboFingerprint=%08X crossingsAuthored=%d\n",
            slot, gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest,
            gComboCtx.comboSettingsHash, crossingsAuthored ? 1 : 0);
    // A LOG, NOT A REFUSAL, and the asymmetry is deliberate. The sanctioned
    // writers refuse while frozen, so a live CVar that disagrees with the record
    // by now can only have come from a raw store write — worth naming loudly
    // because it means something is authoring behind the freeze. It changes
    // nothing: the frozen record is the authority for a created world, which is
    // precisely why a player toggling a setting between Generate and file select
    // must NOT fail their creation.
    if (Combo_ForeignCrossingsRequested() != crossingsAuthored) {
        fprintf(stderr,
                "[OoT] creation event: WARNING the live combo store now resolves crossings=%d against a frozen "
                "record that authored %d — a raw write reached a frozen world; the RECORD wins (#657)\n",
                Combo_ForeignCrossingsRequested() ? 1 : 0, crossingsAuthored ? 1 : 0);
    }

    // The OoT spoiler document this creation will grow its "combo" section into
    // (#660). CVAR_GENERAL("SpoilerLog") holds "./Randomizer/<hash-icons>.json",
    // written minutes ago by SpoilerLog_Write; resolve it the way that writer
    // did. Handed to the MM half rather than used here, because the join needs
    // MM's world live in gSaveContext and the bracket below takes that away the
    // moment the call returns.
    std::string ootSpoilerAbsolute;
    {
        const std::string cvarPath = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
        if (cvarPath.empty()) {
            fprintf(stderr, "[OoT] creation event: no OoT spoiler on record - the paired half has nothing to join\n");
        } else {
            std::string relative = cvarPath;
            if (relative.rfind("./", 0) == 0) {
                relative = relative.substr(2);
            }
            ootSpoilerAbsolute = Ship::Context::GetPathRelativeToAppDirectory(relative.c_str());
        }
    }

    // The creation seam's own progress session (#582). Separate from the one
    // Playthrough_Init opened around OoT's staged generation, because the two
    // are separated by however long the player spent in the menu. THIS is the
    // one whose elapsed time a player actually waits through at file select, and
    // therefore the one the ~30 s floor is about (P12).
    //
    // The on-screen leg is installed FIRST so that Begin's own report already
    // paints: the player must see the overlay before MM's first fill attempt
    // starts, not after it ends. Installing here (rather than at boot) also
    // latches the render thread to the thread the creation actually blocks,
    // which is what keeps OoT's menu-side worker-thread generation from
    // reaching a renderer (CreationProgressOverlay.cpp).
    OoT_CreationProgressOverlay_Install();
    Combo_GenProgress_Begin();

    // THE BRACKET. Its buffer is a file static (declared above, beside the
    // overlay's window into it) rather than a stack array: OoT's SaveContext is
    // ~136 KB, far past any sane frame budget; this seam is game-thread-only,
    // and MM's own attempt ladder uses the same shape for the same reason. The
    // `active` flag is what lets a frame painted from INSIDE the call below show
    // OoT's bytes instead of MM's (#582).
    //
    // SIZED AT OoT's sizeof, NOT at the unified capacity, and that is a fix
    // rather than a nicety. Copying OOT_SAVE_CONTEXT_SIZE bytes THROUGH a
    // SaveContext* reads and writes past the declared object even though the
    // real storage behind it is a larger char array, which glibc's
    // _FORTIFY_SOURCE catches as "*** buffer overflow detected ***" and aborts —
    // green on MSVC, SIGABRT on the Linux CI leg. The static_assert above is what
    // makes the smaller copy sufficient: MM's whole SaveContext fits inside
    // OoT's, so there is nothing past OoT's struct end for MM to have written.
    memcpy(sOoTSaveSnapshot, &gSaveContext, sizeof(SaveContext));
    sCreationBracketActive = true;

    const int mmRc = MM_Rando_GenerateAtCreation(slot, ootSpoilerAbsolute.c_str());

    sCreationBracketActive = false;
    memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    // The OoT-side tail, measured for the same reason MM measures its two
    // stretches (#582's review asked for numbers rather than the assertion that
    // "everything else reports a phase and moves on within milliseconds"). This one
    // is the shortfall stats read and a toast, and the number below is what says so.
    const uint32_t ootTailStartMs = Combo_GenProgress_ElapsedMs();

    if (mmRc != 0) {
        // TERMINAL. Retract everything the freeze published so no artifact of a
        // half-created world survives: no identity for a later arrival to
        // compare against, no crossing tables for either direction, no armed MM
        // shadow (the MM half never armed one — it returned before that, or
        // arming itself failed).
        fprintf(stderr,
                "[OoT] creation event: FAILED (MM half rc=%d) — retracting the pairing identity; slot %d must not be "
                "written\n",
                mmRc, slot);
        fflush(stderr);
        Combo_GenProgress_End(false);
        // The player-visible half, raised HERE rather than by the caller: a
        // creation that failed must surface identically from every route into
        // it, and the only way to guarantee that is for the surface to live
        // with the verdict.
        RsbsSave_RefuseSlotGeneration(slot);
        OoT_Creation_ReportFailureAtFileSelect(slot, 0);
        Context_ClearFrozenState(GAME_MM);
        Combo_ClearForeignPlacements();
        Combo_ClearForeignPlacementsOoT();
        Combo_ClearForeignGiveCaps();
        // formatVersion 0 is the record's ABSENT tag (ADR 0011 decision 4.2) —
        // the occupancy byte that makes the other eleven usable. Zeroing it is
        // how a record is retracted; there is deliberately no "unfreeze" API,
        // because the only legitimate retraction is this one.
        memset(&gComboCtx.comboSettings, 0, sizeof(gComboCtx.comboSettings));
        gComboCtx.comboSettingsHash = 0;
        gComboCtx.sourceIsRando = false;
        gComboCtx.sharedRandoSeed = 0;
        gComboCtx.sharedRandoSettingsHash = 0;
        gComboCtx.mmProfileDigest = 0;
        gComboCtx.mmPairedAttempt = 0;
        return 0;
    }

    // ------------------------------------------------------------------------
    // THE FOREIGN-PLACEMENT SHORTFALL, SURFACED AT CREATION (#583).
    //
    // The under-supply rule (#580) places fewer crossings rather than stranding
    // them on unreachable hosts. That is right, and it was invisible: a player
    // promised four crossings who got two found out by reading the spoiler JSON.
    // The number is decided here, so it is announced here — on the same overlay
    // every other creation-time verdict uses, with the counts that explain it.
    //
    // NOT AN ERROR AND NOT A FAILURE. While crossings are duplicate overlays
    // (increments 1-2) the origin world keeps its own copy of every pool item,
    // so a missing crossing costs "fewer extras" and never a winnable world.
    // The toast says fewer, the creation succeeds, and the spoiler keeps the
    // durable record.
    {
        int requested = 0;
        int placed = 0;
        int eligible = 0;
        int reachable = 0;
        if (MM_Rando_LastPlacementStats(&requested, &placed, &eligible, &reachable)) {
            fprintf(stderr,
                    "[OoT] creation event: SHORTFALL — %d of %d cross-game items found a host in Termina "
                    "(%d eligible host checks, %d of them reachable)\n",
                    placed, requested, eligible, reachable);
            fflush(stderr);

            static char shortfallMessage[224];
            snprintf(shortfallMessage, sizeof(shortfallMessage),
                     "Only %d of %d Ocarina of Time items could be hidden in Termina - this seed's reachable "
                     "chests ran short. Your Hyrule world still contains all of them; you will simply find "
                     "fewer of them over there.",
                     placed, requested);
            ComboNotification shortfallToast;
            memset(&shortfallToast, 0, sizeof(shortfallToast));
            shortfallToast.prefix = "Fewer cross-game items:";
            shortfallToast.prefixColor[0] = 1.0f;
            shortfallToast.prefixColor[1] = 0.8f;
            shortfallToast.prefixColor[2] = 0.3f;
            shortfallToast.prefixColor[3] = 1.0f;
            shortfallToast.message = shortfallMessage;
            shortfallToast.messageColor[0] = 1.0f;
            shortfallToast.messageColor[1] = 1.0f;
            shortfallToast.messageColor[2] = 1.0f;
            shortfallToast.messageColor[3] = 1.0f;
            shortfallToast.remainingTime = 15.0f;
            // Muted for the same reason the failure toast below is: the creation
            // event runs inside the display-free locks as well as inside file
            // select, and Notification::Emit's unmuted arm plays an OoT sound.
            shortfallToast.mute = 1;
            OoT_Notification_Emit(&shortfallToast);
        }
    }

    fprintf(stderr,
            "[OoT] creation event: the OoT-side tail after MM's half returned (shortfall stats + toast) took %ums "
            "(#582)\n",
            Combo_GenProgress_ElapsedMs() - ootTailStartMs);
    fflush(stderr);
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_PUBLISH, 0, nullptr);
    Combo_GenProgress_End(true);
    fprintf(stderr, "[OoT] creation event: slot %d complete — both halves authored under one frozen identity\n", slot);
    fflush(stderr);
    return 1;
}

/**
 * THE FILE-SELECT FAILURE SURFACE (ADR 0010 increment 2; #533/#568's machinery,
 * new leg).
 *
 * Before this increment a paired generation that could not converge was
 * discovered in Termina, hours later: OnFileCreate's catch reverted the MM save
 * to vanilla and the arrival gate raised the refusal. The whole point of moving
 * generation to the creation seam is that the failure now lands WHERE THE
 * DECISION WAS MADE — at file select, on the same shared overlay the arrival
 * refusals use, while the player still has the settings that caused it in front
 * of them.
 *
 * MUTED, like every other cross-game refusal toast, and the reasoning that said
 * otherwise was wrong in an instructive way. "This fires inside OoT's own file
 * select, where the audio session certainly exists" is true of PRODUCTION and
 * false of this function's other callers: the creation event is driven directly
 * by the ROM-free and display-free locks, where Notification::Emit's unmuted arm
 * reaches Audio_PlaySoundGeneral with no archives mounted. That is the same
 * hazard the arrival refusals mute for, and the seam does not get an exemption
 * just because its production caller is better equipped. The toast is the
 * surface; the sound is not load-bearing.
 *
 * VISUAL, THEREFORE UNVERIFIABLE BY THE TIERS. The locks assert the CREATION's
 * verdict (no file written, no identity left, slot refused); that the toast
 * renders is a playtest observation. Stated rather than implied.
 *
 * @param slot   the slot whose creation failed.
 * @param reason reserved for a future failure taxonomy; 0 today ("generation
 *               did not converge"), which is the only way to get here.
 */
extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason) {
    (void)reason;
    fprintf(stderr,
            "[OoT] creation event: slot %d REFUSED at file select — the paired Majora's Mask world could not be "
            "generated; no file was written and no pairing identity survives\n",
            slot);
    fflush(stderr);

    ComboNotification failureToast;
    memset(&failureToast, 0, sizeof(failureToast));
    failureToast.prefix = "File NOT created:";
    failureToast.prefixColor[0] = 0.9f;
    failureToast.prefixColor[1] = 0.35f;
    failureToast.prefixColor[2] = 0.3f;
    failureToast.prefixColor[3] = 1.0f;
    failureToast.message = "The paired Majora's Mask world could not be generated for this seed and these "
                           "settings. Nothing was saved. Try a different seed, or relax the Majora's Mask "
                           "options, and create the file again.";
    failureToast.messageColor[0] = 1.0f;
    failureToast.messageColor[1] = 1.0f;
    failureToast.messageColor[2] = 1.0f;
    failureToast.messageColor[3] = 1.0f;
    failureToast.remainingTime = 20.0f;
    failureToast.mute = 1; // see the header: this seam is driven by display-free locks too
    OoT_Notification_Emit(&failureToast);
}

#endif // RSBS_SINGLE_EXECUTABLE
