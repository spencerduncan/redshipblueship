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
#include <string>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"

#include "foreign_items.h" // src/common — ComboForeignItemDef, SharedItem

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

    // Candidates in ascending RandomizerCheck order. Walking the enum range
    // rather than ctx->allLocations keeps the order fixed by construction — it
    // cannot be perturbed by pool-bookkeeping changes — which is what the
    // SeedDeterminism digest needs. Same shape as the digest's own walk.
    std::vector<RandomizerCheck> candidates;
    for (int i = 1; i < RC_MAX; i++) {
        const RandomizerCheck rc = (RandomizerCheck)i;
        if (OoT_Foreign_IsEligibleHostImpl(rc)) {
            candidates.push_back(rc);
        }
    }

    fprintf(stderr, "[OoT] foreign placement: %d MM pool items over %zu eligible host checks\n", poolCount,
            candidates.size());

    if (candidates.empty()) {
        fprintf(stderr, "[OoT] foreign placement: no eligible OoT host check in this fill (#510)\n");
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
    // this loop used to build by hand — which is what keeps SeedDeterminism's
    // foreignOoTHash from moving. There is NO seed term in the class (accepted
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
    // The direction itself is READ here and reported; ADR 0011 increment 4 is
    // where an unarmed direction makes this pass a no-op. It lands last on
    // purpose: it is the only increment that can change a generated world, and
    // it should land on top of a frozen, compared, rendered setting rather than
    // under one.
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

#endif // RSBS_SINGLE_EXECUTABLE
