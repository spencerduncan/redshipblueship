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
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>

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
static const ComboForeignItemDef kForeignPoolV1[] = {
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_PROGRESSIVE_HOOKSHOT }, "Progressive Hookshot" },
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_FAIRY_BOW }, "Fairy Bow" },
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_BOMB_BAG }, "Bomb Bag" },
    { { (uint8_t)GAME_OOT, 0, (uint16_t)RG_PROGRESSIVE_STRENGTH }, "Progressive Strength Upgrade" },
};

static constexpr int kForeignPoolCount = sizeof(kForeignPoolV1) / sizeof(kForeignPoolV1[0]);
static_assert(kForeignPoolCount <= (int)RSBS_FOREIGN_PLACEMENT_CAP,
              "the pinned foreign pool must fit the gComboCtx placement carve");

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

#endif // RSBS_SINGLE_EXECUTABLE
