/**
 * ForeignItemsSingleExe.cpp — the OoT-side redemption give, the crossing
 * pickup, and the paired creation event (Phase 3.0 Lane C1, #392; ADR 0002;
 * ADR 0010 increments 2 and 3).
 *
 * This is the ONE translation unit where the cross-game item class is defined,
 * because it is the one place the real RG_* enumerators may appear: everything
 * leaves this TU as an origin-tagged SharedItem (or a plain display string),
 * so a raw RG_* integer never crosses a game boundary (ADR 0002, the #356 bug
 * class). MM and the ROM-free test harness reach the pool through the
 * extern "C" surface declared in src/common/foreign_items.h.
 *
 * The pinned OoT pool this TU used to define (kForeignPoolV1, ~4 rows) and the
 * reverse overlay pass (OoT_PlaceForeignItems) are RETIRED (ADR 0010 increment
 * 3, D3; lane K11): items leave origin pools, and every crossing is a placement
 * the single-bag fill makes at the creation event.
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
 * The crossing pickup (OoT_Rando_Foreign_RecordPickup) reads the placement
 * table and, behind it, the crossing store, and this file still never sees an
 * RI_*, only origin-tagged SharedItems.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring> // memcpy/memset — the creation event's snapshot bracket
#include <filesystem>
#include <string>
#include <random> // std::random_device: the completion's digest scratch name (#1044)
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h" // the shortfall and failure toasts, in SoH's own shape
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/savefile.h"            // Randomizer_InitSaveFile: runs AFTER the creation event
#include "soh/Enhancements/game-interactor/GameInteractor.h" // IsSaveLoaded: the hint text's gate (#575 H3b)
// ReachabilitySearch — the reverse pass's reachability gate (#656). The same
// header entrance.cpp reaches it through, from the same directory.
#include "3drando/fill.hpp"

#include "foreign_items.h"    // src/common — SharedItem, the placement tables
#include "combo_single_bag.h" // src/common — the single-bag fill (ADR 0010 increment 3, lane K11)
#include "crossing_store.h"   // src/common — the creation writer of the crossings (ADR 0010 O7)
#include "shared_items.h"     // src/common — Combo_RecordSharedItem (#493)
#include "shared_resources.h" // src/common — OoT_CreditSharedPickup: a counted pack credits the pool (#1061)
#include "gen_budget.h"       // src/common — the #582 fill budget + progress surface
#include "seed_package.h"     // src/common — the seed package the creation binds (#1044)

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h" // OoT_Item_Give
u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry);
}

// savefile.cpp's rupee helper (C++ linkage there; no header of its own).
void GiveLinkRupees(int numOfRupees);

// ============================================================================
// The pinned pool ("foreign item class v1") — RETIRED (ADR 0010 increment 3,
// D3; lane K11)
// ============================================================================
//
// kForeignPoolV1 (Progressive Strength, Lens of Truth, Boomerang, Megaton
// Hammer) and its criterion-attributed exclusion table fed MM's forward overlay
// pass, which pinned those items onto MM junk as duplicate copies. Under one bag
// every OoT progression item the frozen settings hand to the general pass may
// cross (the O8 owner, src/common/shared_items.h, decides membership), and which
// ones do is the single-bag fill's decision.

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
// REVERSE DIRECTION (#510): the OoT placement pass — RETIRED (ADR 0010
// increment 3, D3; lane K11)
// ============================================================================
//
// OoT_PlaceForeignItems, its chest-only host predicate and its reachability gate
// (#656) are gone with the pool they drew from. The host-class half of that
// predicate lives on as the OoT engine's `hostAcceptsForeign`
// (ComboLogicEngineOoT.cpp), which the single-bag fill consults before it draws a
// crossing onto an OoT host; the fill-side half ("the fill put junk here") and
// the reachability gate are the fill's own proof now.

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
// @return true only when a durable shared-item record was authored, or (#1061) a
//         counted pack was credited to the shared pool. The caller presents the
//         foreign pickup if and only if one of those happened.
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

    // ONCE PER HOST. The drain gates on `!loc->HasObtained()` already; the same
    // gate is here too, so the recording function cannot be the thing that
    // double-delivers when a future caller forgets it.
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = (ctx != nullptr) ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
    if (il != nullptr && il->HasObtained()) {
        return false;
    }

    // #1061: a COUNTED pack (an MM bombchu pack) does not cross. It is credited
    // to the shared pool here and OoT's count re-applies from it at once, so the
    // pickup shows in Hyrule and Termina reads it from the pool on arrival. A
    // crossing would reach the count only through MM's give, which caps
    // bombchus at MM's bomb-bag capacity and lost the pack. Durable the same way
    // a crossing is: the pool is gComboCtx, and the live count is OoT's save.
    // #1120: a CAPACITY TIER (MM's Progressive Bomb Bag, wallet, magic, ...) the
    // same way: the pool is stepped by one copy and OoT's tier re-applies at
    // once, where a crossing reached OoT only after a round trip (MM's arrival
    // give, MM's harvest, OoT's next arrival apply). A kind the frozen rules
    // disarm (bottles with Shared Bottles off) answers 0 and crosses as before.
    if (OoT_CreditSharedPickup(item) != 0) {
        return true;
    }

    // Durable immediately (the serialized array, so an OoT save+quit before the
    // next switch cannot lose the pickup — the stage/commit outbox is RAM-only,
    // see shared_items.h). ONE COPY PER PICKUP (ADR 0010 increment 3): under the
    // single bag two OoT hosts may hold two copies of one MM id, and both must
    // reach MM, so the record is never content-merged (RSBS_SHARED_ITEM_CROSSING).
    return Combo_RecordSharedItemCrossing((GameId)item->originGame, item->id) >= 0;
}

/**
 * The reverse direction's give-path entry point, called by the RC-queue drain
 * (hook_handlers.cpp) and driven directly by the ForeignItemGiveReverse lock.
 *
 * The exact twin of MM_Rando_Foreign_RecordPickup, and named to match: one
 * production function, two callers, so the lock covers the real recording path
 * rather than a copy of it.
 *
 * @return 1 if a durable crossing was authored for this OoT check (or, #1061, a
 *         counted pack credited to the shared pool), 0 otherwise
 *         (no foreign placement here, or no live pairing to author it for).
 */
extern "C" int OoT_Rando_Foreign_RecordPickup(uint16_t rc) {
    return OoT_Foreign_RecordPickupImpl(rc) ? 1 : 0;
}

// THE FULL OUTBOX (#1222; declared in src/common/foreign_items.h). The drain used
// to record and, on a refusal (every shared row holding an item MM has not
// redeemed), fall through to the ordinary give of the junk cover the host holds,
// which marks the check collected: the MM item was lost. Asked BEFORE the record,
// a check the outbox has no row for is held here instead: not collected, nothing
// given, nothing counted. The drain hands it back to its queue on the first frame
// a row is free (after a crossing to Termina redeems the rows MM was waiting on).
//
// RAM only, like the drain's own queue: it survives a crossing, which is what
// frees the rows, but not a quit. A check held at a quit keeps its flag and its
// item is not given (#1222's PR names this as the open limit).
static std::vector<uint16_t> sOutboxHeldChecks;

extern "C" int OoT_Rando_Foreign_HoldPickup(uint16_t rc) {
    if (Combo_GetForeignPlacementForOoTCheck(rc) == nullptr || !Combo_ForeignPairingActive() ||
        Combo_SharedItemCrossingHasRoom()) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = (ctx != nullptr) ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
    if (il != nullptr && il->HasObtained()) {
        return 0; // delivered: the record's once-per-host gate answers, as before
    }
    if (std::find(sOutboxHeldChecks.begin(), sOutboxHeldChecks.end(), rc) == sOutboxHeldChecks.end()) {
        sOutboxHeldChecks.push_back(rc);
        fprintf(stderr,
                "[OoT] foreign pickup HELD: OoT check %u stays open, the shared outbox is full; it is given after the "
                "next crossing frees a row (#1222)\n",
                (unsigned)rc);
        fflush(stderr);
    }
    return 1;
}

extern "C" int OoT_Rando_Foreign_TakeReleasedHolds(uint16_t* out, int cap) {
    if (sOutboxHeldChecks.empty() || out == nullptr || cap <= 0 || !Combo_SharedItemCrossingHasRoom()) {
        return 0;
    }
    const int n = std::min(cap, (int)sOutboxHeldChecks.size());
    for (int i = 0; i < n; i++) {
        out[i] = sOutboxHeldChecks[i];
    }
    sOutboxHeldChecks.erase(sOutboxHeldChecks.begin(), sOutboxHeldChecks.begin() + n);
    return n;
}

/**
 * #800 S1, the shop shelf (z_en_girla.c): 1 when OoT check `rc` hosts an MM item
 * (the placement table, and behind it the crossing store), the fact the shelf
 * draws and the textbox names from instead of the junk cover the OoT table holds.
 */
extern "C" int OoT_Rando_Foreign_HostsForeign(uint16_t rc) {
    return rc != 0 && Combo_GetForeignPlacementForOoTCheck(rc) != nullptr ? 1 : 0;
}

/**
 * #577 M5, the chest's appearance (z_en_box.c, OoT_EnBox_Init): the item category
 * "Chest Size & Texture Matches Contents" dresses OoT chest `rc` by, given the
 * category of the item the OoT table holds there (`coverCategory`).
 *
 * A crossing host holds OoT's junk cover (RG_BLUE_RUPEE, ComboLogicEngineOoT.cpp)
 * and would be dressed as junk whatever MM item it hides. It is dressed by the MM
 * item's class instead, as OoTMM grades a chest (#862): the chest grade MM
 * publishes for its own item (MM's randoItemType, through the item class owner,
 * shared_items.h "THE CHEST GRADE"; ADR 0002 keeps MM's item table out of this
 * TU), mapped onto OoT's own categories, so an MM small key shows OoT's key chest
 * and an MM heart piece OoT's heart chest. A stray fairy and a mask present the two
 * single-exe categories z_en_box.c dresses with SoH's fairy and mask chest models
 * (#1172; OoT's own masks stay major, as OoTMM grades them); an item with no grade
 * presents as major, the answer every crossing chest gave before #862. Every other
 * chest keeps its own item's category.
 */
extern "C" int OoT_Rando_Foreign_ChestCategory(uint16_t rc, int coverCategory) {
    const SharedItem* item = rc != 0 ? Combo_GetForeignPlacementForOoTCheck(rc) : nullptr;
    if (item == nullptr) {
        return coverCategory;
    }
    switch (Combo_ItemChestGrade(*item)) {
        case RSBS_CHEST_GRADE_LESSER:
            return (int)ITEM_CATEGORY_LESSER;
        case RSBS_CHEST_GRADE_JUNK:
            return (int)ITEM_CATEGORY_JUNK;
        case RSBS_CHEST_GRADE_HEALTH:
            return (int)ITEM_CATEGORY_HEALTH;
        case RSBS_CHEST_GRADE_BOSS_KEY:
            return (int)ITEM_CATEGORY_BOSS_KEY;
        case RSBS_CHEST_GRADE_SMALL_KEY:
            return (int)ITEM_CATEGORY_SMALL_KEY;
        case RSBS_CHEST_GRADE_TOKEN:
            return (int)ITEM_CATEGORY_SKULLTULA_TOKEN;
        case RSBS_CHEST_GRADE_FAIRY:
            return (int)ITEM_CATEGORY_MM_STRAY_FAIRY;
        case RSBS_CHEST_GRADE_MASK:
            return (int)ITEM_CATEGORY_MM_MASK;
        case RSBS_CHEST_GRADE_MAJOR:
        default:
            return (int)ITEM_CATEGORY_MAJOR;
    }
}

/** TEST BRIDGE (ForeignModel row M14): what OoT chest `rc` presents when the OoT
 *  table holds the junk cover there: 1 a major chest, 0 the cover's junk chest,
 *  -1 anything else. */
extern "C" int OoT_Rando_Foreign_TestChestShowsMajor(uint16_t rc) {
    const int shown = OoT_Rando_Foreign_ChestCategory(rc, (int)ITEM_CATEGORY_JUNK);
    return shown == (int)ITEM_CATEGORY_MAJOR ? 1 : shown == (int)ITEM_CATEGORY_JUNK ? 0 : -1;
}

/** TEST BRIDGE (ForeignModel row M15, #862): the category OoT chest `rc` is
 *  dressed by when the OoT table holds the junk cover there (what z_en_box.c's
 *  init asks), as an ITEM_CATEGORY_* value. */
extern "C" int OoT_Rando_Foreign_TestChestCategory(uint16_t rc) {
    return OoT_Rando_Foreign_ChestCategory(rc, (int)ITEM_CATEGORY_JUNK);
}

/** TEST BRIDGE (M15): OoT's OWN chest category for OoT item `rg`, read straight
 *  off the item table (Item::GetCategory, the category an OoT chest holding that
 *  item natively is dressed by). -1 past RG_MAX. */
extern "C" int OoT_Rando_Foreign_TestOwnChestCategory(uint16_t rg) {
    if (rg >= (uint16_t)RG_MAX) {
        return -1;
    }
    return (int)Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetCategory();
}

/** TEST BRIDGE (ForeignModel row M20, #1172): OoT's OWN chest category, off the
 *  item table, for the items #1172 rules on, in this order: Piece of Heart, Heart
 *  Container, Double Defense, the Treasure Game Heart, then OoT's eight masks
 *  (Keaton, Skull, Spooky, Bunny Hood, Goron, Zora, Gerudo, Truth). Writes at
 *  most `cap` values and returns how many it wrote. */
extern "C" int OoT_Rando_Foreign_TestRuledChestCategories(int* out, int cap) {
    static const RandomizerGet kRuled[] = {
        RG_PIECE_OF_HEART, RG_HEART_CONTAINER, RG_DOUBLE_DEFENSE, RG_TREASURE_GAME_HEART,
        RG_KEATON_MASK,    RG_SKULL_MASK,      RG_SPOOKY_MASK,    RG_BUNNY_HOOD,
        RG_GORON_MASK,     RG_ZORA_MASK,       RG_GERUDO_MASK,    RG_MASK_OF_TRUTH,
    };
    int n = 0;
    for (const RandomizerGet rg : kRuled) {
        if (n >= cap) {
            break;
        }
        out[n++] = (int)Rando::StaticData::RetrieveItem(rg).GetCategory();
    }
    return n;
}

/** TEST BRIDGE (M15): an ITEM_CATEGORY_* value's name in the vocabulary both
 *  games' chest bridges share ("major", "small key", ...), "?" past the enum. */
extern "C" const char* OoT_Rando_Foreign_TestChestCategoryName(int category) {
    switch (category) {
        case ITEM_CATEGORY_JUNK:
            return "junk";
        case ITEM_CATEGORY_LESSER:
            return "lesser";
        case ITEM_CATEGORY_HEALTH:
            return "health";
        case ITEM_CATEGORY_BOSS_KEY:
            return "boss key";
        case ITEM_CATEGORY_SMALL_KEY:
            return "small key";
        case ITEM_CATEGORY_SKULLTULA_TOKEN:
            return "token";
        case ITEM_CATEGORY_MAJOR:
            return "major";
        case ITEM_CATEGORY_MM_STRAY_FAIRY: // #1172
            return "fairy";
        case ITEM_CATEGORY_MM_MASK:
            return "mask";
        default:
            return "?";
    }
}

/**
 * #800 S1, the shop shelf's sold-out and can-buy tests (z_en_girla.c), and since
 * #800 pass 2 the Business Scrub's (hook_handlers.cpp: its can-buy test and its
 * despawn): 1 when OoT
 * check `rc` hosts an MM item and the check is COLLECTED (the once-per-host gate
 * OoT_Foreign_RecordPickupImpl and the drain read). That is usually "the MM item
 * crossed", but not always: after a #610 pairing refusal the drain falls through
 * to OoT's ordinary give, whose receive handler marks the check collected with
 * no crossing recorded (the shelf sold its junk cover). Either way the shelf has
 * been bought, and a shelf whose RandomizerInf flag was lost with an unsaved
 * reload would otherwise sell again and charge for nothing: the drain delivers
 * nothing for a check it has already collected.
 */
extern "C" int OoT_Rando_Foreign_HostCollected(uint16_t rc) {
    if (OoT_Rando_Foreign_HostsForeign(rc) == 0) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = (ctx != nullptr && rc < RC_MAX) ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
    return il != nullptr && il->HasObtained() ? 1 : 0;
}

/**
 * Does the crossing store host OoT item `rg` on an MM check (the single-bag fill
 * put it in Termina)? Read by OoT's hint pass (3drando/hints.cpp), which runs in
 * OoT's remainder AFTER the creation captured the crossings, so an item hint whose
 * target is not at any OoT location can say where it is (PR #743 review).
 */
extern "C" int OoT_Combo_ItemHostedInMM(int rg) {
    for (int i = 0; i < Combo_Crossings_Count(GAME_MM); ++i) {
        ComboCrossing row;
        if (Combo_Crossings_At(GAME_MM, i, &row) && row.item.originGame == (uint8_t)GAME_OOT &&
            row.item.id == (uint16_t)rg) {
            return 1;
        }
    }
    return 0;
}

/**
 * #575 H3a: the MM check that hosts OoT item `rg` in the crossing store, the first
 * such row in insertion order (an item the fill placed more than once is named by
 * its first host), or 0 when the store hosts it nowhere in MM (no row has host 0).
 * Read by OoT's hint pass under the same gate as OoT_Combo_ItemHostedInMM.
 */
extern "C" int OoT_Combo_MMHostOfItem(int rg) {
    for (int i = 0; i < Combo_Crossings_Count(GAME_MM); ++i) {
        ComboCrossing row;
        if (Combo_Crossings_At(GAME_MM, i, &row) && row.item.originGame == (uint8_t)GAME_OOT &&
            row.item.id == (uint16_t)rg) {
            return (int)row.hostCheck;
        }
    }
    return 0;
}

// ComboLogicEngineOoT.cpp: 1 while OoT's remainder hints the paired world just
// captured, and 1 while that world's spoiler is written.
extern "C" int OoT_ComboLogic_HintingPairedRemainder(void);
extern "C" int OoT_ComboLogic_WritingSingleBagSpoiler(void);

/**
 * #575 H3b: for OoT's hint PASS's "Named Item" filter (3drando/hints.cpp,
 * FilterGoodItems), whether crossing host `rc` counts as a major item: -1 when
 * `rc` is not a crossing host of the world being hinted (the filter then asks the
 * item the check holds, as upstream does), else 1 when the MM item it yields is
 * major and 0 when it is not.
 *
 * MAJOR IS MM'S OWN ANSWER, read through the item class owner (shared_items.h
 * "THE CHEST GRADE", MM's randoItemType; ADR 0002 keeps MM's item table out of
 * this TU), mapped onto OoT's own rule (Item::IsMajorItem): an MM major item, a
 * mask, and a small or boss key that crossed (a crossing key was placed by the
 * general pass, where OoT counts its own keys major) are major; a map or compass
 * (LESSER), a heart piece or container (HEALTH), a skulltula token, a stray fairy,
 * junk and an item with no grade are not, as OoT's own maps, compasses, heart
 * pieces and containers are not. So an MM compass never takes a good-item stone
 * an OoT compass could not take.
 *
 * The crossing store outlives the creation that filled it (it is cleared on a cold
 * boot, a slot load, a failed creation and a session invalidation, not by a new
 * generation), so a hint pass over any other world (a native general pass, a
 * spoiler load's static hints) would read another world's crossings. The pass
 * reads it only while OoT's remainder hints the paired world the store describes,
 * the gate the Termina answer already uses (3drando/hints.cpp).
 */
extern "C" int OoT_Combo_HintPassCrossingIsMajor(int rc) {
    if (rc <= 0 || OoT_ComboLogic_HintingPairedRemainder() == 0) {
        return -1;
    }
    const SharedItem* item = Combo_GetForeignPlacementForOoTCheck((uint16_t)rc);
    if (item == nullptr) {
        return -1;
    }
    switch (Combo_ItemChestGrade(*item)) {
        case RSBS_CHEST_GRADE_MAJOR:
        case RSBS_CHEST_GRADE_MASK:
        case RSBS_CHEST_GRADE_SMALL_KEY:
        case RSBS_CHEST_GRADE_BOSS_KEY:
            return 1;
        default:
            return 0;
    }
}

/**
 * #575 H3b: the MM item OoT check `rc` yields, for the TEXT of an OoT hint that
 * names the check (hint.cpp: the stone's words and the spoiler's "item"), or NULL
 * when the hint should name what the check physically holds.
 *
 * The text is rendered in three moments, and the store describes the world being
 * hinted in each only under its own condition:
 *   - OoT's remainder builds the paired world's hints (OoT_ComboLogic_HintingPairedRemainder);
 *   - the paired creation writes that world's spoiler (OoT_ComboLogic_WritingSingleBagSpoiler);
 *   - a paired file is loaded and played: a slot load clears the store and loads
 *     the file's own crossings, and a creation captures them, so during play the
 *     store is the loaded file's (Combo_ForeignPairingActive and a loaded save).
 * Anywhere else (a native general pass and its spoiler, the file select between a
 * generation and the next load) the store may be another world's, and the check
 * names its own item. Unpaired, the drain refuses the crossing and gives the cover,
 * so the cover is what the check yields: the Check Tracker names it too.
 */
extern "C" const SharedItem* OoT_Combo_HintTextCrossingAt(int rc) {
    if (rc <= 0) {
        return nullptr;
    }
    const bool hintingThisWorld = OoT_ComboLogic_HintingPairedRemainder() != 0 ||
                                  OoT_ComboLogic_WritingSingleBagSpoiler() != 0 ||
                                  (Combo_ForeignPairingActive() && GameInteractor::IsSaveLoaded());
    return hintingThisWorld ? Combo_GetForeignPlacementForOoTCheck((uint16_t)rc) : nullptr;
}

/** 1 when OoT check `rc` hosts an MM item in the crossing store (a crossing host). */
extern "C" int OoT_Combo_CheckHostsCrossing(int rc) {
    for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); ++i) {
        ComboCrossing row;
        if (Combo_Crossings_At(GAME_OOT, i, &row) && row.hostCheck == (uint16_t)rc) {
            return 1;
        }
    }
    return 0;
}

/** TEST BRIDGE: mark an OoT check collected (or not), as the drain does after a
 *  pickup, so a lock can drive the once-per-host gate. Returns 1 when applied, 0
 *  when this process has no OoT location table to apply it to (a ROM-free row). */
extern "C" int OoT_Rando_Foreign_TestSetObtained(uint16_t rc, int obtained) {
    auto ctx = Rando::Context::GetInstance();
    Rando::ItemLocation* il = (ctx != nullptr && rc < RC_MAX) ? ctx->GetItemLocation((RandomizerCheck)rc) : nullptr;
    if (il == nullptr) {
        return 0;
    }
    il->SetCheckStatus(obtained != 0 ? RCSHOW_COLLECTED : RCSHOW_UNCHECKED);
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

// The MM half of the creation event (games/mm/2s2h/GameExports_SingleExe.cpp):
// authored first, armed LAST (after the crossings and the one spoiler; #680's
// order, restored on the PR #743 review).
extern "C" int MM_Rando_AuthorHalfAtCreation(int slot, const char* ootSpoilerPath, const uint8_t* mmPlayerName);
extern "C" int MM_Rando_ArmCreatedHalf(int slot);
// OoT's typed name in MM's charset (#773), soh/SaveManager.cpp.
extern "C" void OoT_PlayerName_ToMMCharset(const uint8_t* ootName, uint8_t filenameLanguage, uint8_t* mmName);
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
// MM's finished half, kept past the bracket for the ONE spoiler's join (lane K11):
// the join reads MM's world out of gSaveContext, and under one bag it runs AFTER
// OoT's own remainder has been placed and its spoiler written, which needs OoT's
// bytes live. So the join gets a second, short bracket over these bytes.
static char sMmFinishedSave[sizeof(SaveContext)];

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
// ComboLogicEngineOoT.cpp: OoT's per-game remainder after the single-bag fill.
extern "C" int OoT_ComboLogic_FinishGeneralPass(int writeSpoiler);

/**
 * OoT'S SIDE OF A PAIRED CREATION, after the MM half has run the single-bag fill
 * (ADR 0010 increment 3; lane K11). Two steps, in this order:
 *
 *   1. THE CROSSINGS, captured from the coordinator's tables into the crossing
 *      store (ADR 0010 O7): the one durable record of which host of either game
 *      yields an item of the other, frozen with the world and persisted in the
 *      .redsave's Tier-4 by the file's first Save_SaveFile().
 *   2. OoT's REMAINDER (OoT_ComboLogic_FinishGeneralPass): its junk, renewables
 *      and traps onto its leftover hosts, its overrides, its hints and, when
 *      `writeSpoiler`, its spoiler document.
 *
 * A named function rather than lines in OoT_RunPairedCreationEvent because the
 * headless harnesses that drive MM's half directly (the golden digests, the
 * ladder and switch-entry rows) must complete the SAME world the creation event
 * completes, not a copy of the steps.
 *
 * @return the number of crossings stored (>= 0); negative when the creation must
 *         fail (-1 the store refused the crossings, -2 OoT's remainder could not
 *         run). On failure the store is left empty.
 */
extern "C" int OoT_Creation_FinishPairedHalf(int writeSpoiler) {
    const int crossings = Combo_Crossings_CaptureFromCoordinator();
    if (crossings < 0) {
        fprintf(stderr, "[OoT] creation: the crossing store refused the single bag's crossings (%s)\n",
                Combo_Crossings_StatusName(crossings));
        return -1;
    }
    if (OoT_ComboLogic_FinishGeneralPass(writeSpoiler) != 0) {
        fprintf(stderr, "[OoT] creation: OoT's remainder after the single-bag fill could not run\n");
        Combo_Crossings_Clear();
        return -2;
    }
    return crossings;
}

// ----------------------------------------------------------------------------
// THE CREATION'S STEP ORDER, RECORDED (PR #743 review)
// ----------------------------------------------------------------------------
// #680's order is: crossings stored, spoiler written and joined, identity
// published, MM shadow armed LAST. The event records each step as it completes,
// with whether the MM shadow was armed at that moment ('1') or not ('0'), so a
// lock can read "M0S0J0A1" off a real creation: MM's half authored unarmed, the
// store and OoT's remainder unarmed, the join unarmed, then armed. Arming anywhere
// earlier (inside MM's half, where it used to happen) reads "M1S1J1A1".
static char sCreationSequence[16];
static int sCreationSequenceLen = 0;

static void CreationStep(char step) {
    if (sCreationSequenceLen + 2 < (int)sizeof(sCreationSequence)) {
        sCreationSequence[sCreationSequenceLen++] = step;
        sCreationSequence[sCreationSequenceLen++] = Context_HasFrozenState(GAME_MM) ? '1' : '0';
        sCreationSequence[sCreationSequenceLen] = 0;
    }
}

/** TEST BRIDGE: the last paired creation event's step record (see above). */
extern "C" const char* OoT_Creation_TestLastSequence(void) {
    return sCreationSequence;
}

// ADR 0010 section 1.2's creation warning: the halves the last SUCCESSFUL paired
// creation carried no proof for (Combo_Logic_UnprovedHalves). RSBS_COMBO_HALF_NONE_YET
// until an event reaches its end, so a lock can tell "warned about nothing" from
// "never asked".
#define RSBS_COMBO_HALF_NONE_YET 0xFFFFFFFFu
static uint32_t sCreationUnprovedHalves = RSBS_COMBO_HALF_NONE_YET;

/** TEST BRIDGE: the halves the last paired creation warned about (see above). */
extern "C" uint32_t OoT_Creation_TestLastUnprovedHalves(void) {
    return sCreationUnprovedHalves;
}

/**
 * THE GOAL WARNING (ADR 0010 section 1.2), in SoH's own toast shape: one line,
 * the default colours, muted for the same reason as the failure toast below (the
 * creation event also runs inside the display-free locks).
 *
 * @param unprovedHalves RSBS_COMBO_HALF_* bits (Combo_Logic_UnprovedHalves);
 *        0 draws nothing. Its own function so the ui tier's
 *        "toast/creation-goal-warning" page draws the production toast.
 *
 * The copy names the game, spelled out (docs/ui-style-guide.md R-N8), and fits
 * the 53 characters an 832-px window shows on the overlay's one line.
 */
extern "C" void OoT_Creation_EmitGoalWarningToast(uint32_t unprovedHalves) {
    const char* message = nullptr;
    switch (unprovedHalves & (RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM)) {
        case RSBS_COMBO_HALF_MM:
            message = "Majora's Mask may be unfinishable.";
            break;
        case RSBS_COMBO_HALF_OOT:
            message = "Ocarina of Time may be unfinishable.";
            break;
        case RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM:
            message = "neither game is proven finishable.";
            break;
        default:
            return;
    }
    Notification::Emit({
        .prefix = "Not proven:",
        .message = message,
        .mute = true,
    });
}

// The completed world, between this completion and the package it is written into.
// Game-thread state like the bracket above.
static std::atomic<bool> sPairedWorldComplete{ false };
static uint32_t sCompletedUnprovedHalves = 0;
static unsigned sCompletedGoal = 0;
static std::string sCompletedDigest;
static std::string sCompletedMMProfile;

// The digest writers the goldens use (games/mm/2s2h/mm_rando_gen_test.cpp,
// 3drando/menu.cpp), so the digest a package carries is the golden's.
extern "C" int Rando_HeadlessWriteDeterminismHead(const char* outPath);
extern "C" int MM_SeedPackage_AppendDigestTail(const char* outPath);
// MM's frozen profile off its save (games/mm/2s2h/Rando/Foreign.cpp).
extern "C" int MM_Rando_ProfileDumpFromSave(char* out, size_t cap);
// MM's half (games/mm/2s2h/GameExports_SingleExe.cpp).
extern "C" size_t MM_SeedPackage_HalfSize(void);
extern "C" void MM_SeedPackage_StampPlayerName(const uint8_t* playerName);
// SeedPackageSingleExe.cpp: what a creation binds.
extern "C" int OoT_SeedPackage_BindForCreation(const uint8_t** outHalf, size_t* outSize, uint32_t* outUnprovedHalves);
extern "C" int OoT_SeedPackage_CompleteForCreation(void);
// src/common/test_runner.cpp: a --test or --integration-test process (the creation's
// completion of a headless generation is for those alone).
extern "C" bool TestRunner_IsTestMode(void);

/**
 * RETRACT A PAIRED WORLD THAT FAILED: everything the freeze and the completion published,
 * so no artifact of a half-created world survives — no identity for a later
 * arrival to compare against, no crossing tables for either direction, no armed
 * MM shadow, no engine record a later fill could "restore", no combo or triforce
 * record. ONE function for every failure route out of the event (PR #743 review):
 * the MM-half failure and the OoT-tail failure used to carry two hand-copied lists,
 * and the tail's had already lost the triforce record's zeroing.
 *
 * Since #1044 the world is completed at Generate, so this is shared by Generate's
 * completion (no file yet, so no slot surface) and the creation event, whose
 * RetractFailedPairedCreation below adds the player-visible half (the #533 slot
 * latch and the toast) for the same reason: a creation that failed must surface
 * identically from every route into it.
 */
static void RetractPairedWorld(void) {
    Combo_GenProgress_End(false);
    Context_ClearFrozenState(GAME_MM);
    Combo_ClearForeignPlacements();
    Combo_ClearForeignPlacementsOoT();
    Combo_ClearForeignGiveCaps();
    // The single bag's own artifacts (lane K11): no crossings for a world that
    // was not created, and no engine record that a later fill could "restore".
    Combo_Crossings_Clear();
    Combo_SingleBag_Forget();
    // formatVersion 0 is the record's ABSENT tag (ADR 0011 decision 4.2) — the
    // occupancy byte that makes the other eleven usable. Zeroing it is how a
    // record is retracted; Combo_RetractCreationRecords is not a general
    // "unfreeze" API, because the only legitimate retraction is this one.
    //
    // The O10 triforce record (ADR 0010) and the cross-game item rules (#976)
    // were frozen beside the combo record and go with it, in one call shared with
    // Playthrough_Init's rollback so the two exits cannot drift apart. Left
    // behind, a triforce record would sit next to an ABSENT goal as a hunt nobody
    // froze (Combo_TriforceRecordDivergence reads that as damage), and item rules
    // would be carried by the KEEP invalidation onto disk.
    Combo_RetractCreationRecords();
    gComboCtx.sourceIsRando = false;
    gComboCtx.sharedRandoSeed = 0;
    gComboCtx.sharedRandoSettingsHash = 0;
    gComboCtx.mmProfileDigest = 0;
    gComboCtx.mmKeepItemsReset = 0; // #1175: frozen beside the digest, retracted with it
    gComboCtx.mmPairedAttempt = 0;
    // #1044: no world, so no package either, and nothing waits at a general pass.
    sPairedWorldComplete = false;
    OoT_SeedPackage_Unstage();
}

static void RetractFailedPairedCreation(int slot) {
    RetractPairedWorld();
    RsbsSave_RefuseSlotGeneration(slot);
    OoT_Creation_ReportFailureAtFileSelect(slot, 0);
}

// ----------------------------------------------------------------------------
// GENERATE'S COMPLETION (#1044): THE WHOLE PAIRED WORLD, AT GENERATE
// ----------------------------------------------------------------------------
// Until #1044 everything below ran inside the creation event, at file select,
// against the CVars as they stood when the player named a file: MM's half, the
// single-bag fill under MM's attempt ladder, the crossings, OoT's remainder and
// the one spoiler. Now it runs when the player presses Generate -- on the game
// thread, the frame the Generate worker ends (OoT_SeedPackage_FileSelectTick,
// z_file_choose.c), because MM's generation writes gSaveContext and the file
// select reads it every frame (CreationProgressOverlay.cpp's header), so the
// worker cannot run it beside a live file select. The world is then written as a
// seed package and staged (SeedPackageSingleExe.cpp), and the creation event only
// binds it: it stamps the file's name into MM's half and arms it.
//
// Everything that varies the world is read here, once: the identity and the
// records Playthrough_Init froze, the MM profile its stamp froze (MM's half reads
// the CVars it was stamped from, in the same frame-free stretch), the attempt the
// ladder converges on. Nothing at file select can move it any more.
//
// The step record and the overlay are the creation's, unchanged: the record reads
// "M0S0J0" here and the creation appends "A1" when it arms.

// The digest writers write files, so the completion gives them one of its own in
// the temp directory, under a random name (every lane's runs share that directory).
static std::filesystem::path PairedDigestScratchPath() {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        dir = std::filesystem::path(".");
    }
    std::random_device entropy;
    char name[64];
    snprintf(name, sizeof(name), "rsbs-seed-digest-%08X%08X.txt", (unsigned)entropy(), (unsigned)entropy());
    return dir / name;
}

static std::string ReadWholeFile(const std::filesystem::path& path) {
    std::string out;
    FILE* f = fopen(path.string().c_str(), "rb");
    if (f == nullptr) {
        return out;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    fclose(f);
    out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
    return out;
}

/**
 * Complete the paired world OoT's generation left at its general pass: MM's half
 * (the single-bag fill inside it), the crossings, OoT's remainder and its spoiler,
 * the spoiler join, and the package's digest and MM profile. Game thread only,
 * with no generation in flight. On failure everything the freeze published is
 * retracted (RetractPairedWorld) and nothing is completed.
 * @return 0 when completed; RSBS_SEEDPKG_ERR_NO_WORLD when nothing was waiting;
 *         RSBS_SEEDPKG_ERR_GENERATION when the completion failed.
 */
extern "C" int OoT_PairedGen_CompleteWorld(void) {
    sPairedWorldComplete = false;
    if (!Combo_ForeignPairingActive() || OoT_ComboLogic_GeneralPassDeferred() == 0) {
        return RSBS_SEEDPKG_ERR_NO_WORLD;
    }

    // The pre-Fill gate's answer, read FROM THE FROZEN RECORD rather than
    // re-resolved from the CVars (#657/#667): this completion must be unable to
    // reach a different conclusion than the freeze did.
    const bool crossingsAuthored =
        Combo_ComboDirectionArms((uint8_t)GAME_OOT) || Combo_ComboDirectionArms((uint8_t)GAME_MM);
    fprintf(stderr,
            "[OoT] paired completion: masterSeed=%u settingsHash=%08X mmProfileDigest=%08X comboFingerprint=%08X "
            "crossingsAuthored=%d\n",
            gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest,
            gComboCtx.comboSettingsHash, crossingsAuthored ? 1 : 0);
    // A LOG, NOT A REFUSAL (#657): the frozen record is the authority.
    if (Combo_ForeignCrossingsRequested() != crossingsAuthored) {
        fprintf(stderr,
                "[OoT] paired completion: WARNING the live combo store now resolves crossings=%d against a frozen "
                "record that authored %d — a raw write reached a frozen world; the RECORD wins (#657)\n",
                Combo_ForeignCrossingsRequested() ? 1 : 0, crossingsAuthored ? 1 : 0);
    }

    // THE DIGEST'S HEAD: OoT's world as Generate's worker left it, at its general
    // pass -- exactly where the golden rows write it.
    const std::filesystem::path digestPath = PairedDigestScratchPath();
    std::error_code digestEc;
    std::filesystem::remove(digestPath, digestEc);
    bool digestOk = Rando_HeadlessWriteDeterminismHead(digestPath.string().c_str()) == 0;

    // The progress session a player waits through (#582), on the overlay painted
    // from inside this call, installed first so Begin's own report paints.
    OoT_CreationProgressOverlay_Install();
    Combo_GenProgress_Begin();

    // THE BRACKET (the creation's, unchanged; see the storage above). No name: the
    // file's name is stamped when a file binds the package.
    memcpy(sOoTSaveSnapshot, &gSaveContext, sizeof(SaveContext));
    sCreationBracketActive = true;

    sCreationSequenceLen = 0;
    sCreationSequence[0] = 0;
    sCreationUnprovedHalves = RSBS_COMBO_HALF_NONE_YET;
    const int mmRc = MM_Rando_AuthorHalfAtCreation(-1, "", nullptr);
    CreationStep('M');

    sCreationBracketActive = false;
    // MM's finished half, kept for the spoiler join, the digest and the package.
    memcpy(sMmFinishedSave, &gSaveContext, sizeof(SaveContext));
    memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    const uint32_t ootTailStartMs = Combo_GenProgress_ElapsedMs();

    if (mmRc != 0) {
        fprintf(stderr, "[OoT] paired completion: FAILED (MM half rc=%d) — retracting the pairing identity\n", mmRc);
        fflush(stderr);
        std::filesystem::remove(digestPath, digestEc);
        RetractPairedWorld();
        return RSBS_SEEDPKG_ERR_GENERATION;
    }

    // ------------------------------------------------------------------------
    // THE SINGLE BAG'S OoT-SIDE TAIL (ADR 0010 increment 3, D3; lane K11). The MM
    // half ran the single-bag fill over both worlds and MM's own pass over MM's
    // leftovers. What is left is OoT's, with OoT's bytes live (its spoiler writer
    // reads gSaveContext.language):
    //
    //   1. THE CROSSINGS, captured from the coordinator's tables into the store
    //      (ADR 0010 O7), the one durable record of which host of either game
    //      yields an item of the other.
    //   2. OoT's REMAINDER: its junk, renewables and traps onto its leftover
    //      hosts, overrides, hints, and its spoiler document.
    //   3. THE ONE SPOILER: MM's half joined into OoT's document as "combo", over a
    //      second short bracket that puts MM's finished bytes back in view; in the
    //      same bracket, the digest's world lines and MM's frozen profile.
    //   4. COMMIT: the coordinator's and both engines' roll-back records dropped.
    //
    // The ARM is no longer here: it is the creation's (#680's order still holds
    // inside it -- the store, the spoiler and the identity all exist before the
    // armed shadow does).
    // ------------------------------------------------------------------------
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_CROSSINGS, 0, "Filling Hyrule's remaining checks");
    const int crossings = OoT_Creation_FinishPairedHalf(1);
    const bool tailOk = crossings >= 0;
    if (tailOk) {
        CreationStep('S');
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_SPOILER, 0, "writing the paired spoiler");
        const std::string cvarPath = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
        std::string ootSpoilerAbsolute;
        if (cvarPath.empty()) {
            fprintf(stderr,
                    "[OoT] paired completion: no OoT spoiler on record - the paired half has nothing to join\n");
        } else {
            std::string relative = cvarPath;
            if (relative.rfind("./", 0) == 0) {
                relative = relative.substr(2);
            }
            ootSpoilerAbsolute = Ship::Context::GetPathRelativeToAppDirectory(relative.c_str());
        }
        memcpy(sOoTSaveSnapshot, &gSaveContext, sizeof(SaveContext));
        memcpy(&gSaveContext, sMmFinishedSave, sizeof(SaveContext));
        sCreationBracketActive = true;
        if (!ootSpoilerAbsolute.empty()) {
            MM_Rando_AugmentSpoilerWithPairedHalf(ootSpoilerAbsolute.c_str());
            CreationStep('J');
        }
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_PUBLISH, 0, nullptr);
        if (digestOk) {
            digestOk = MM_SeedPackage_AppendDigestTail(digestPath.string().c_str()) == 0;
        }
        static char sProfileDump[1 << 16];
        sCompletedMMProfile = MM_Rando_ProfileDumpFromSave(sProfileDump, sizeof(sProfileDump)) >= 0
                                  ? std::string(sProfileDump)
                                  : std::string("{}");
        sCreationBracketActive = false;
        memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    }
    const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
    fprintf(stderr,
            "[OoT] paired completion: single bag — %d crossings stored (%d OoT items in Termina, %d MM items in "
            "Hyrule), %d rows placed, %d surplus dropped, %d rounds, fill %ums\n",
            crossings, bag->crossingsIntoMM, bag->crossingsIntoOoT, bag->fill.placed, bag->fill.surplusDropped,
            bag->fill.rounds, bag->wallMs);
    // Read off the fill before Forget drops its report: which halves the world is
    // generated WITHOUT a proof for (none under beat-both).
    const uint32_t unprovedHalves = Combo_Logic_UnprovedHalves(&bag->fill);
    const unsigned frozenGoal = (unsigned)bag->goal;
    Combo_SingleBag_Forget();
    if (!tailOk) {
        std::filesystem::remove(digestPath, digestEc);
        RetractPairedWorld();
        return RSBS_SEEDPKG_ERR_GENERATION;
    }

    sCompletedDigest = digestOk ? ReadWholeFile(digestPath) : std::string();
    std::filesystem::remove(digestPath, digestEc);
    if (!digestOk) {
        fprintf(stderr, "[OoT] paired completion: the world's digest could not be written; the package carries none\n");
    }
    fprintf(stderr,
            "[OoT] paired completion: the OoT-side tail after MM's half returned (crossings, OoT's remainder, the "
            "spoiler join, the digest) took %ums (#582); steps %s\n",
            Combo_GenProgress_ElapsedMs() - ootTailStartMs, sCreationSequence);
    fflush(stderr);
    Combo_GenProgress_End(true);
    sCompletedUnprovedHalves = unprovedHalves;
    sCompletedGoal = frozenGoal;
    sPairedWorldComplete = true;
    return 0;
}

/**
 * The completed world's parts, for the package (SeedPackageSingleExe.cpp).
 * @return 1 while a completed world is held; 0 otherwise (and the outputs are left as they were).
 */
extern "C" int OoT_PairedGen_CompletedWorld(const uint8_t** outHalf, size_t* outHalfSize, const char** outDigest,
                                            const char** outMMProfile, uint32_t* outUnprovedHalves, unsigned* outGoal) {
    if (!sPairedWorldComplete) {
        return 0;
    }
    if (outHalf != nullptr) {
        *outHalf = reinterpret_cast<const uint8_t*>(sMmFinishedSave);
    }
    if (outHalfSize != nullptr) {
        *outHalfSize = MM_SeedPackage_HalfSize();
    }
    if (outDigest != nullptr) {
        *outDigest = sCompletedDigest.c_str();
    }
    if (outMMProfile != nullptr) {
        *outMMProfile = sCompletedMMProfile.c_str();
    }
    if (outUnprovedHalves != nullptr) {
        *outUnprovedHalves = sCompletedUnprovedHalves;
    }
    if (outGoal != nullptr) {
        *outGoal = sCompletedGoal;
    }
    return 1;
}

/** A failed completion's retraction, for the package's write path (SeedPackageSingleExe.cpp). */
extern "C" void OoT_PairedGen_RetractWorld(void) {
    RetractPairedWorld();
}

/**
 * THE CREATION EVENT, AFTER #1044: BIND THE STAGED PACKAGE. Called for every
 * randomizer file by OoT_Creation_AuthorRandoFile, before Randomizer_InitSaveFile.
 *
 * The paired world was completed at Generate and staged as a seed package (or a
 * package was loaded from the Randomizer folder, which stages it the same way).
 * The creation re-applies the package's frozen identity, records, crossings and
 * give capabilities (the file-create invalidation may have dropped some of them,
 * and a package outlives the session that loaded it), stamps the file's name into
 * MM's half and ARMS it as the MM shadow. No fill runs and no CVar is read: the
 * settings as they stand at file select cannot move the world.
 *
 * A world still waiting at its general pass with no package staged had no
 * file-select frame complete it. In a player's game that cannot happen: the
 * Generate button and SoH's gen_rando console command both end on the Generate
 * worker, whose next file-select frame completes and stages the world. So the
 * creation never generates in a player's game: such a world is REFUSED below
 * ("no seed package is staged"), never quietly completed at file select. Only a
 * test process (--test / --integration-test, whose headless generations drive no
 * file-select frame) completes it here, with the same function Generate runs,
 * and then binds it like any other.
 *
 * @return 1 when the file may be written; 0 when the creation failed (the
 *         identity is retracted and the refusal surface is raised).
 */
extern "C" int OoT_RunPairedCreationEvent(int slot) {
    if (OoT_SeedPackage_Staged() == 0 && Combo_ForeignPairingActive() && OoT_ComboLogic_GeneralPassDeferred() != 0 &&
        TestRunner_IsTestMode()) {
        fprintf(stderr,
                "[OoT] creation event: slot %d — TEST PROCESS: the paired world is still waiting at its general pass "
                "(a headless generation, no file-select frame completed it); completing it now with Generate's own "
                "completion (#1044)\n",
                slot);
        if (OoT_SeedPackage_CompleteForCreation() != 1) {
            // The completion retracted the identity; the creation surfaces it.
            RsbsSave_RefuseSlotGeneration(slot);
            OoT_Creation_ReportFailureAtFileSelect(slot, 0);
            return 0;
        }
    }

    if (OoT_SeedPackage_Staged() == 0) {
        if (!Combo_ForeignPairingActive()) {
            // A randomizer file with no pairing identity: a world loaded from a
            // solo OoT spoiler (no "combo" section), or one whose stamp the KEEP
            // identity check discarded (#597). Every randomizer file in this build
            // is a paired file, and the file select refuses to open one without
            // its pairing (#836), so creating it would write a file nobody can
            // open. Refused as a creation failure instead: nothing is written, the
            // player generates again or loads a seed package.
            fprintf(stderr,
                    "[OoT] creation event: slot %d REFUSED — the randomizer world has no cross-game pairing identity "
                    "(a solo spoiler, or a discarded stamp); a file created from it could not be opened (#836)\n",
                    slot);
        } else {
            fprintf(stderr,
                    "[OoT] creation event: slot %d REFUSED — a pairing identity is resident but no seed package is "
                    "staged for it; generate the seed again or load its package (#1044)\n",
                    slot);
        }
        fflush(stderr);
        RsbsSave_RefuseSlotGeneration(slot);
        OoT_Creation_ReportFailureAtFileSelect(slot, 0);
        return 0;
    }

    const uint8_t* half = nullptr;
    size_t halfSize = 0;
    uint32_t unprovedHalves = 0;
    const int bound = OoT_SeedPackage_BindForCreation(&half, &halfSize, &unprovedHalves);
    if (bound != RSBS_SEEDPKG_OK || half == nullptr || halfSize == 0 || halfSize > sizeof(SaveContext)) {
        fprintf(stderr, "[OoT] creation event: slot %d REFUSED — the staged seed package could not be bound (%s)\n",
                slot, SeedPackage_StatusName(bound));
        fflush(stderr);
        RetractFailedPairedCreation(slot);
        return 0;
    }
    fprintf(stderr,
            "[OoT] creation event: slot %d binds the seed package %s — masterSeed=%u settingsHash=%08X "
            "mmProfileDigest=%08X comboFingerprint=%08X\n",
            slot, OoT_SeedPackage_StagedPath(), gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash,
            gComboCtx.mmProfileDigest, gComboCtx.comboSettingsHash);

    // THE PAIRED WORLD'S ONE NAME (#773): OoT's typed name, already in gSaveContext
    // (Sram_InitSave writes it before this seam), translated before the bracket
    // hands the buffer to MM's half.
    uint8_t mmPlayerName[8];
    OoT_PlayerName_ToMMCharset(gSaveContext.playerName, gSaveContext.ship.filenameLanguage, mmPlayerName);
    memcpy(sOoTSaveSnapshot, &gSaveContext, sizeof(SaveContext));
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memcpy(&gSaveContext, half, halfSize);
    sCreationBracketActive = true;
    MM_SeedPackage_StampPlayerName(mmPlayerName);
    const int armRc = MM_Rando_ArmCreatedHalf(slot);
    CreationStep('A');
    sCreationBracketActive = false;
    memcpy(&gSaveContext, sOoTSaveSnapshot, sizeof(SaveContext));
    if (armRc != 0) {
        RetractFailedPairedCreation(slot);
        return 0;
    }

    // ADR 0010 section 1.2: a world accepted with a half that carries no proof is
    // created WITH A VISIBLE WARNING naming that half, at creation.
    sCreationUnprovedHalves = unprovedHalves;
    if (unprovedHalves != 0u) {
        fprintf(stderr,
                "[OoT] creation event: slot %d created with NO PROOF for %s%s%s -- accepted, as ADR 0010 section 1.2 "
                "permits, and warned\n",
                slot, (unprovedHalves & RSBS_COMBO_HALF_OOT) != 0u ? "Ocarina of Time" : "",
                (unprovedHalves == (RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM)) ? " and " : "",
                (unprovedHalves & RSBS_COMBO_HALF_MM) != 0u ? "Majora's Mask" : "");
        OoT_Creation_EmitGoalWarningToast(unprovedHalves);
    }
    fprintf(stderr,
            "[OoT] creation event: slot %d complete — both halves authored under one frozen identity; steps %s\n", slot,
            sCreationSequence);
    fflush(stderr);
    return 1;
}

// ----------------------------------------------------------------------------
// THE STAGED GENERATION IS CONSUMED BY THE FILE IT CREATES (#564 V2)
// ----------------------------------------------------------------------------
// OoT's Generate STAGES one world (Rando::Context's mSeedGenerated, set by
// RandoMain::GenerateRando); the file select's "Start Randomizer" and
// OoT_Sram_InitSave's isRandoFile both read it. Under one-game semantics a world
// is the identity of ONE file, so the creation that uses a staged generation
// consumes it: nothing is staged afterwards, nothing waits at a general pass, and
// the file select turns a second file away at "Start Randomizer", saying which
// file the seed made (OoT_FileSelect_NoSeedText), before any creation work.
// Before this, the generation stayed staged after the file was written, and a
// second file from it was named and ran the paired creation event until MM's
// half reached the single-bag fill, which refused an OoT world whose general pass
// had already finished.
//
// A FAILED creation consumes it too: the event has retracted the identity, so no
// file could be created from that world ("Not created:" asks for a new seed).
//
// The record is what the file select reads: the slot the consumed generation
// created, OOT_STAGED_GEN_FAILED when its creation failed, or OOT_STAGED_GEN_NONE
// when nothing has been consumed since the last generation began
// (Playthrough_Init forgets it). Process-local, like the staged generation.
#define OOT_STAGED_GEN_NONE (-1)
#define OOT_STAGED_GEN_FAILED (-2)
static int sStagedGenerationConsumedBy = OOT_STAGED_GEN_NONE;
// ComboLogicEngineOoT.cpp: cleared by every new generation, a spoiler load and
// OoT's remainder; here, by the consumption of the world it was waiting for.
extern "C" void OoT_ComboLogic_SetGeneralPassDeferred(int deferred);

/** The slot the last consumed generation created, or OOT_STAGED_GEN_FAILED (-2) /
 *  OOT_STAGED_GEN_NONE (-1); see above. */
extern "C" int OoT_Creation_StagedGenerationConsumedBy(void) {
    return sStagedGenerationConsumedBy;
}

/** 1 while a creation has consumed the last staged generation and no generation
 *  has begun since. The file select then does not auto-load the spoiler that
 *  creation wrote (FileChoose_UpdateRandomizer): a paired world's spoiler is
 *  refused as a solo world, with a toast, every time it is loaded. */
extern "C" int OoT_Creation_StagedGenerationConsumed(void) {
    return sStagedGenerationConsumedBy != OOT_STAGED_GEN_NONE ? 1 : 0;
}

/** Where every generation begins (Playthrough_Init): a new world replaces the
 *  consumed one. */
extern "C" void OoT_Creation_ForgetConsumedGeneration(void) {
    sStagedGenerationConsumedBy = OOT_STAGED_GEN_NONE;
    // #1044: a new generation replaces the staged package too (Playthrough_Init
    // calls this on the Generate worker; the package's own state is locked).
    OoT_SeedPackage_Unstage();
    sPairedWorldComplete = false;
}

/** #1044: a seed package was staged (SeedPackageSingleExe.cpp): it is a fresh world
 *  for the file select's purposes, as a generation is, without unstaging itself. */
extern "C" void OoT_Creation_MarkFreshStagedWorld(void) {
    sStagedGenerationConsumedBy = OOT_STAGED_GEN_NONE;
}

static void ConsumeStagedGeneration(int slot, bool created) {
    Rando::Context::GetInstance()->SetSeedGenerated(false);
    OoT_ComboLogic_SetGeneralPassDeferred(0);
    // #1044: the package the file bound is consumed with it; another file loads it again.
    OoT_SeedPackage_Unstage();
    sPairedWorldComplete = false;
    sStagedGenerationConsumedBy = created ? slot : OOT_STAGED_GEN_FAILED;
    fprintf(stderr,
            "[OoT] creation: slot %d %s; the staged generation is consumed, and another file needs a new "
            "generation (#564 V2)\n",
            slot, created ? "created" : "not created");
    fflush(stderr);
}

/**
 * TEST BRIDGE (#564 V2): what the file select's Generate leaves behind, a STAGED
 * generation. RandoMain::GenerateRando marks the context generated; the headless
 * generation the locks drive (Rando_HeadlessSeedTest) does not, so a lock that
 * models "Generate, then create a file" marks it here, as Generate would.
 */
extern "C" void OoT_Creation_TestStageGeneration(void) {
    Rando::Context::GetInstance()->SetSeedGenerated(true);
}

/**
 * TEST BRIDGE (#1055): the seed name OoT's writer gives the current generation's
 * spoiler (SpoilerLog_Write, 3drando/spoiler_log.cpp): the five hash icons, two
 * digits each, joined by '-'. Returns 0 (and writes "" when it can) when @p out
 * cannot hold it.
 */
extern "C" int OoT_Creation_TestSpoilerSeedName(char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return 0;
    }
    std::string name;
    const auto& icons = Rando::Context::GetInstance()->hashIconIndexes;
    for (size_t i = 0; i < icons.size(); i++) {
        char part[8];
        snprintf(part, sizeof(part), i == 0 ? "%02u" : "-%02u", (unsigned)icons[i]);
        name += part;
    }
    if (name.size() + 1 > cap) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, name.c_str(), name.size() + 1);
    return 1;
}

/**
 * TEST BRIDGE (#1055): removes Randomizer/<seed>.json and every
 * Randomizer/<seed>-<fingerprint>.json of the current generation, so a spoiler
 * that a run of another build left in the shared build directory cannot meet
 * this creation. The rows that create a paired file there call it after
 * generating and before creating. @return the number of files removed.
 */
extern "C" int OoT_Creation_TestClearPairedSpoiler(void) {
    char seed[64];
    if (!OoT_Creation_TestSpoilerSeedName(seed, sizeof(seed)) || seed[0] == '\0') {
        return 0;
    }
    const std::string seedName = seed;
    const std::string suffixPrefix = seedName + "-";
    std::vector<std::filesystem::path> matches;
    std::error_code ec;
    const std::filesystem::path dir = Ship::Context::GetPathRelativeToAppDirectory("Randomizer");
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::path& p = it->path();
        if (p.extension() != ".json") {
            continue;
        }
        const std::string stem = p.stem().string();
        bool ours = stem == seedName;
        if (!ours && stem.size() == suffixPrefix.size() + 8 &&
            stem.compare(0, suffixPrefix.size(), suffixPrefix) == 0) {
            ours = stem.find_first_not_of("0123456789ABCDEF", suffixPrefix.size()) == std::string::npos;
        }
        if (ours) {
            matches.push_back(p);
        }
    }
    int removed = 0;
    for (const auto& p : matches) {
        std::error_code rmEc;
        if (std::filesystem::remove(p, rmEc)) {
            fprintf(stderr, "[OoT] test: removed '%s' before the paired creation (#1055)\n", p.string().c_str());
            removed++;
        }
    }
    fflush(stderr);
    return removed;
}

/**
 * A RANDOMIZER FILE'S AUTHORING, IN ITS ONE CORRECT ORDER (PR #743 review): the
 * paired creation event, THEN Randomizer_InitSaveFile. Called by OoT_Sram_InitSave
 * (z_sram.c) for every randomizer file in the single executable, and by the
 * combo-single-bag row, so the lock drives the production order rather than a
 * copy of it.
 *
 * WHY THIS ORDER. Under the single bag a paired OoT world stops at its general
 * pass at Generate, and its general-pass hosts stay EMPTY until the event places
 * the bag and OoT's remainder. Randomizer_InitSaveFile hands out creation-time
 * items by READING hosts (Link's Pocket, Impa's song under Skip Child Zelda,
 * Malon's egg and Zelda's letter, the Master Sword on an adult start) and marks
 * each check collected. Any of them can be a general-pass host (Link's Pocket =
 * Anything; songs Anywhere; Shuffle Master Sword). Read empty,
 * Context::GetFinalGIEntry answers the host's VANILLA item, so the player got a
 * free vanilla item and never received the one the fill placed and the proof
 * counted. With the event first, every host is placed before anything reads it;
 * for an unpaired file the event authors nothing and the order changes nothing.
 *
 * LAST, THE STAGED GENERATION IS CONSUMED (#564 V2, above), whether the file was
 * created or not. A world loaded from a spoiler is not a staged generation (the
 * event refuses it as unpaired) and is left as it is.
 *
 * @return 1 when the file may be written; 0 when the paired creation failed (the
 *         event has already retracted the identity and raised the refusal
 *         surface), in which case Randomizer_InitSaveFile does not run either.
 */
extern "C" int OoT_Creation_AuthorRandoFile(int slot) {
    const bool staged = Rando::Context::GetInstance()->IsSeedGenerated();
    const bool created = OoT_RunPairedCreationEvent(slot) != 0;
    if (created) {
        Randomizer_InitSaveFile();
    }
    if (staged) {
        ConsumeStagedGeneration(slot, created);
    }
    return created ? 1 : 0;
}

/**
 * THE SHORTFALL TOAST (#583), in SoH's own toast shape.
 *
 * SoH's toasts are one short line in Notification::Options' default colours
 * (Enhancements/QoL/Autosave.cpp's "Game autosaved", Plandomizer.cpp's "Invalid
 * Spoiler Log Format"); this one used to carry hand-picked RGBA and three
 * sentences, which the overlay draws as ONE line at 1.8x -- several times the
 * width of the window. It keeps the fact that matters (how many of the requested
 * items found a host) and leaves the explanation to the stderr line above its
 * caller and to the spoiler, which records every crossing.
 *
 * Its own function so the UI snapshot can draw the production toast
 * (`redship --test ui-snapshot`, the toast pages) rather than a copy of it.
 * Duration: the player's own Notifications.Duration, as every SoH toast.
 * Muted for the same reason the failure toast below is: the creation event runs
 * inside the display-free locks as well as inside file select, and
 * Notification::Emit's unmuted arm plays an OoT sound.
 *
 * NO PRODUCTION CALLER UNDER THE SINGLE BAG (ADR 0010 increment 3). The count it
 * reports came from the post-fill overlay passes over the pinned pools
 * (the retired MM_Rando_LastPlacementStats); the single-bag fill replaced both, so the
 * creation event no longer raises it. It stays because the ui tier's
 * "toast/creation-shortfall" page draws it; it goes when that page does.
 */
extern "C" void OoT_Creation_EmitShortfallToast(int placed, int requested) {
    char message[96];
    snprintf(message, sizeof(message), "%d of %d placed in Termina.", placed, requested);
    Notification::Emit({
        .prefix = "Fewer cross-game items:",
        .message = message,
        .mute = true,
    });
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
 * THE TOAST IS CHECKED BY THE UI TIER, the verdict by the creation locks. The
 * locks assert the CREATION's verdict (no file written, no identity left, slot
 * refused). The ui tier's UiSnapshot row draws this function's toast through the
 * real Notifications window (the "toast/creation-failure" page) and fails it if
 * the toast is missing, blank, runs off an 832-px window, or is drawn below the
 * player's Notifications.Size there; pairing-refusal-toast-fit checks the same
 * at 640, 832 and 3840 px (#906). What neither can show is the toast over a
 * live file select; that is still a playtest observation.
 *
 * THE COPY is one line, because the notification overlay draws prefix and
 * message on ONE line at 1.8x and never wraps: it must keep the overlay's
 * margins in the smallest window the ui tier renders (832 px, at most 772 px
 * wide) at the player's own size. "Not created" says both that no file exists
 * and that nothing was saved. #906: the copy named "Majora's Mask options"
 * until the overlay's fit showed it was 800 px wide there (2 px from the left
 * edge); "other options" keeps the advice and the game is named on the stderr
 * line above.
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

    Notification::Emit({
        .prefix = "Not created:",
        .message = "try a new seed or other options.",
        .mute = true, // see the header: this seam is driven by display-free locks too
    });
}

#endif // RSBS_SINGLE_EXECUTABLE
