/**
 * @file oot_blast_mask_test.cpp
 * OoT's half of the Blast Mask locks (#979, epic #975 P4; the rules are
 * RSBS_XITEM_BLAST_MASK_OOT and RSBS_XITEM_SHARED_BLAST in
 * src/common/foreign_items.h, the item is
 * soh/Enhancements/randomizer/BlastMaskOoT.cpp).
 *
 * THE HELPERS the redship row `blast-mask-oot-items`
 * (src/common/tests/test_blast_mask_oot.c) reads OoT's tables, its live save and
 * its REAL logic through. They only put the save or a private logic probe in a
 * known state and read it back; every decision is the production code's. They
 * live here because only an OoT TU has OoT's ItemIDs, its tables, `gSaveContext`
 * and Rando::Logic in scope.
 */

#include <cstdint>
#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/BlastMaskOoT.h"

extern "C" s32 Flags_GetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_UnsetRandomizerInf(RandomizerInf flag);
// z_inventory.c: OoT_gItemIcons' entry count, which only its defining TU sees.
extern "C" const u32 OoT_gItemIconsCount;

/** The Blast Mask's custom ItemID, and Roc's Feather's (the first custom one). */
extern "C" uint8_t OoT_BlastMaskTest_ItemId(void) {
    return (uint8_t)ITEM_MASK_BLAST;
}

extern "C" uint8_t OoT_BlastMaskTest_RocsFeatherId(void) {
    return (uint8_t)ITEM_ROCS_FEATHER;
}

/** OoT_gItemSlots' entry count: an ItemID at or past it has no inventory slot,
 *  and SLOT() must never be asked for it. */
extern "C" int OoT_BlastMaskTest_ItemSlotsCount(void) {
    return (int)ARRAY_COUNT(OoT_gItemSlots);
}

/** OoT_gItemIcons' entry count, from its definition. */
extern "C" uint32_t OoT_BlastMaskTest_ItemIconsCount(void) {
    return (uint32_t)OoT_gItemIconsCount;
}

/** OoT_gItemIcons' row for `item` (a path), or NULL past the table. */
extern "C" const char* OoT_BlastMaskTest_Icon(uint8_t item) {
    return (uint32_t)item < (uint32_t)OoT_gItemIconsCount ? static_cast<const char*>(OoT_gItemIcons[item]) : nullptr;
}

/** gItemAgeReqs[item] (256 entries since #983), and the child age it should hold. */
extern "C" int OoT_BlastMaskTest_AgeReq(uint8_t item) {
    return (int)gItemAgeReqs[item];
}

extern "C" int OoT_BlastMaskTest_ChildAge(void) {
    return (int)LINK_AGE_CHILD;
}

/** OoT's pool id for the Blast Mask (RG_BLAST_MASK), and whether the item table
 *  names it an advancement item (what makes the bag class it PROGRESSION). */
extern "C" int OoT_BlastMaskTest_PoolId(void) {
    return (int)RG_BLAST_MASK;
}

extern "C" int OoT_BlastMaskTest_IsAdvancement(void) {
    return Rando::StaticData::RetrieveItem(RG_BLAST_MASK).IsAdvancement() ? 1 : 0;
}

/** Give (1) or take (0) OoT's ownership of the Blast Mask in the live save: its
 *  RAND_INF flag, which its get-item sets. */
extern "C" void OoT_BlastMaskTest_SetOwned(int held) {
    if (held != 0) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    } else {
        Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    }
}

extern "C" int OoT_BlastMaskTest_Owned(void) {
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK) ? 1 : 0;
}

/** The live save's child trade slot, and a write to it. */
extern "C" int OoT_BlastMaskTest_TradeSlot(void) {
    return (int)INV_CONTENT(ITEM_TRADE_CHILD);
}

extern "C" void OoT_BlastMaskTest_SetTradeSlot(uint8_t item) {
    INV_CONTENT(ITEM_TRADE_CHILD) = item;
}

/**
 * OoT's REAL logic for the Blast Mask: a private Rando::Logic over a zeroed save
 * holding nothing but the mask's ownership flag (@p held), as a child or an
 * adult (@p child), under whatever cross-game item rules the caller froze.
 * @p which: 0 BlastOrSmash, 1 CanCutShrubs, 2 CanHitSwitch(ED_CLOSE),
 * 3 CanKillEnemy(RE_BEAMOS), 4 CanUse(RG_BLAST_MASK), else HasItem(RG_BLAST_MASK).
 * 1 or 0; -1 with no randomizer context.
 */
extern "C" int OoT_BlastMaskTest_Logic(int held, int child, int which) {
    std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    bool created = false;
    if (ctx == nullptr) {
        ctx = Rando::Context::CreateInstance();
        created = true;
    }
    if (ctx == nullptr) {
        return -1;
    }
    auto save = std::make_unique<SaveContext>();
    std::memset(save.get(), 0, sizeof(SaveContext));
    // Empty pockets: a zeroed slot holds ITEM_DEKU_STICK (0), which the logic's
    // inexact inventory checks (CheckInventory(item, false)) read as held.
    std::memset(save->inventory.items, ITEM_NONE, sizeof(save->inventory.items));
    auto probe = std::make_unique<Rando::Logic>();
    probe->SetContext(ctx);
    probe->SetSaveContext(save.get());
    probe->Reset(false);
    probe->IsChild = child != 0;
    probe->IsAdult = child == 0;
    probe->SetRandoInf((uint32_t)RAND_INF_OBTAINED_BLAST_MASK, held != 0);
    bool result = false;
    switch (which) {
        case 0:
            result = probe->BlastOrSmash();
            break;
        case 1:
            result = probe->CanCutShrubs();
            break;
        case 2:
            result = probe->CanHitSwitch(ED_CLOSE);
            break;
        case 3:
            result = probe->CanKillEnemy(RE_BEAMOS);
            break;
        case 4:
            result = probe->CanUse(RG_BLAST_MASK);
            break;
        default:
            result = probe->HasItem(RG_BLAST_MASK);
            break;
    }
    probe.reset();
    if (created) {
        // Context and its Logic hold each other: let go of the one this row made.
        ctx->GetLogic()->SetContext(nullptr);
    }
    return result ? 1 : 0;
}
