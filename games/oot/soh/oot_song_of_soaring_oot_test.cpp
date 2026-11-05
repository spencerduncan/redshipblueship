/**
 * @file oot_song_of_soaring_oot_test.cpp
 * OoT's half of the Song of Soaring (OoT) locks (#1169 lane XW4; the rules are
 * RSBS_XITEM2_SONG_SOARING_OOT and RSBS_XITEM2_SHARED_SONG_SOARING in
 * src/common/foreign_items.h, the item is
 * soh/Enhancements/randomizer/SongOfSoaringOoT.cpp, the count and the refusal
 * are src/common/warp_song_items.c).
 *
 * THE HELPERS the rows in src/common/tests/test_song_soaring_oot.c read OoT's
 * enums, its item table, its REAL logic and its generated world through. They
 * only put a private logic probe or the live save in a known state and read it
 * back; every decision is the production code's. They live here because only an
 * OoT TU has RG_*, RHT_*, RAND_INF_*, Rando::Logic and the seed context in scope.
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h" // TEXT_RANDOMIZER_CUSTOM_ITEM
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"    // itemPool: the general pass's pool

extern "C" s32 Flags_GetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_UnsetRandomizerInf(RandomizerInf flag);
extern "C" u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry);
// z_inventory.c: OoT_gItemIcons' entry count, which only its defining TU sees.
extern "C" const u32 OoT_gItemIconsCount;
// z_draw.c: OoT_sDrawItemTable's entry count.
extern "C" s32 OoT_GetItem_DrawTableCount(void);

/** OoT's pool id for the Song of Soaring (OoT). */
extern "C" int OoT_SoaringTest_PoolId(void) {
    return (int)RG_SONG_OF_SOARING_OOT;
}

/**
 * The ids an append-only item must sit between, by @p which: 0 RG_STONE_MASK (the
 * id before), 1 RG_STICKS (the first logic-only id, never placed), 2 RG_MAX,
 * 3 RAND_INF_OBTAINED_SONG_OF_SOARING_OOT, 4 RAND_INF_OBTAINED_STONE_MASK,
 * 5 RAND_INF_MAX, 6 the u16 words OoT saves the flags in, 7 RHT_SONG_OF_SOARING_OOT,
 * 8 RHT_STONE_MASK, 9 RHT_MAX, 10 OoT_gItemIcons' entry count, 11 ITEM_MASK_STONE
 * (the last custom ItemID), 12 OoT_gItemSlots' entry count, else -1.
 */
extern "C" int OoT_SoaringTest_Id(int which) {
    switch (which) {
        case 0:
            return (int)RG_STONE_MASK;
        case 1:
            return (int)RG_STICKS;
        case 2:
            return (int)RG_MAX;
        case 3:
            return (int)RAND_INF_OBTAINED_SONG_OF_SOARING_OOT;
        case 4:
            return (int)RAND_INF_OBTAINED_STONE_MASK;
        case 5:
            return (int)RAND_INF_MAX;
        case 6:
            return (int)(sizeof(gSaveContext.ship.randomizerInf) / sizeof(gSaveContext.ship.randomizerInf[0]));
        case 7:
            return (int)RHT_SONG_OF_SOARING_OOT;
        case 8:
            return (int)RHT_STONE_MASK;
        case 9:
            return (int)RHT_MAX;
        case 10:
            return (int)OoT_gItemIconsCount;
        case 11:
            return (int)ITEM_MASK_STONE;
        case 12:
            return (int)ARRAY_COUNT(OoT_gItemSlots);
        default:
            return -1;
    }
}

/** The item table's English name for the Song of Soaring (OoT). */
extern "C" const char* OoT_SoaringTest_Name(void) {
    static std::string sName;
    sName = Rando::StaticData::RetrieveItem(RG_SONG_OF_SOARING_OOT).GetName().GetEnglish();
    return sName.c_str();
}

/**
 * One fact of the item table's row, 1 or 0, by @p fact: 0 it is a song
 * (ITEMTYPE_SONG: the restricted song passes take it), 1 an advancement item,
 * 2 a MOD_RANDOMIZER row whose getItemId is its RG id (the give routes on it),
 * 3 its entry's itemId is its RG id, 256 or above, so never an OoT ItemID (it
 * reaches no ItemID-indexed table), 4 its object is OBJECT_GI_MELODY, 5 its draw
 * row is GID_SONG_GENERIC and inside OoT_sDrawItemTable, 6 its custom draw is
 * Randomizer_DrawSongOfSoaringOoT, 7 its text is the custom-item text, 8 it is a
 * MAJOR item, 9 its hint key is RHT_SONG_OF_SOARING_OOT, 10 Logic's RG-to-flag
 * table maps it to RAND_INF_OBTAINED_SONG_OF_SOARING_OOT. -1 for an unknown fact
 * or a row with no static entry.
 */
extern "C" int OoT_SoaringTest_RowFact(int fact) {
    Rando::Item& item = Rando::StaticData::RetrieveItem(RG_SONG_OF_SOARING_OOT);
    const GetItemEntry* entry = item.GetStaticGIEntry();
    if (entry == nullptr) {
        return -1;
    }
    switch (fact) {
        case 0:
            return item.GetItemType() == ITEMTYPE_SONG ? 1 : 0;
        case 1:
            return item.IsAdvancement() ? 1 : 0;
        case 2:
            return (entry->modIndex == MOD_RANDOMIZER && entry->getItemId == (int16_t)RG_SONG_OF_SOARING_OOT) ? 1 : 0;
        case 3:
            return (entry->itemId == (uint16_t)RG_SONG_OF_SOARING_OOT && entry->itemId > 0xFF) ? 1 : 0;
        case 4:
            return entry->objectId == OBJECT_GI_MELODY ? 1 : 0;
        case 5:
            return (entry->gid == GID_SONG_GENERIC && (s32)entry->gid < OoT_GetItem_DrawTableCount()) ? 1 : 0;
        case 6:
            return entry->drawFunc == Randomizer_DrawSongOfSoaringOoT ? 1 : 0;
        case 7:
            return entry->textId == TEXT_RANDOMIZER_CUSTOM_ITEM ? 1 : 0;
        case 8:
            return entry->getItemCategory == ITEM_CATEGORY_MAJOR ? 1 : 0;
        case 9:
            return item.GetHintKey() == RHT_SONG_OF_SOARING_OOT ? 1 : 0;
        case 10:
            return Rando::Logic::RandoGetToRandInf[RG_SONG_OF_SOARING_OOT] ==
                           (uint32_t)RAND_INF_OBTAINED_SONG_OF_SOARING_OOT
                       ? 1
                       : 0;
        default:
            return -1;
    }
}

namespace {

/** A private logic probe over a zeroed save with empty pockets (the Stone Mask
 *  row's shape): runs @p fn on it and lets go of a context it had to make. -1
 *  with no randomizer context. */
template <typename Fn> int WithProbe(Fn fn) {
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
    // inexact inventory checks read as held.
    std::memset(save->inventory.items, ITEM_NONE, sizeof(save->inventory.items));
    auto probe = std::make_unique<Rando::Logic>();
    probe->SetContext(ctx);
    probe->SetSaveContext(save.get());
    probe->Reset(false);
    const int result = fn(*probe, *save);
    probe.reset();
    if (created) {
        // Context and its Logic hold each other: let go of the one this row made.
        ctx->GetLogic()->SetContext(nullptr);
    }
    return result;
}

} // namespace

/**
 * OoT's REAL logic for the Song of Soaring (OoT), under whatever second rules
 * word the caller froze: a child (@p child 1) or an adult, holding the song's
 * flag (@p held), a Fairy Ocarina (@p ocarina) and the ocarina buttons in
 * @p buttons (bit 0 A, 1 C-up, 2 C-down, 3 C-left, 4 C-right; Ocarina Button
 * Shuffle decides which of these a world has). @p which: 0 CanUse, else HasItem.
 * 1 or 0; -1 with no randomizer context.
 */
extern "C" int OoT_SoaringTest_Logic(int held, int child, int ocarina, uint32_t buttons, int which) {
    return WithProbe([&](Rando::Logic& probe, SaveContext&) {
        probe.IsChild = child != 0;
        probe.IsAdult = child == 0;
        if (ocarina != 0) {
            probe.SetInventory(ITEM_OCARINA_FAIRY, ITEM_OCARINA_FAIRY);
        }
        probe.SetRandoInf((uint32_t)RAND_INF_HAS_OCARINA_A, (buttons & 0x01u) != 0u);
        probe.SetRandoInf((uint32_t)RAND_INF_HAS_OCARINA_C_UP, (buttons & 0x02u) != 0u);
        probe.SetRandoInf((uint32_t)RAND_INF_HAS_OCARINA_C_DOWN, (buttons & 0x04u) != 0u);
        probe.SetRandoInf((uint32_t)RAND_INF_HAS_OCARINA_C_LEFT, (buttons & 0x08u) != 0u);
        probe.SetRandoInf((uint32_t)RAND_INF_HAS_OCARINA_C_RIGHT, (buttons & 0x10u) != 0u);
        probe.SetRandoInf((uint32_t)RAND_INF_OBTAINED_SONG_OF_SOARING_OOT, held != 0);
        const bool result = which == 0 ? probe.CanUse(RG_SONG_OF_SOARING_OOT) : probe.HasItem(RG_SONG_OF_SOARING_OOT);
        return result ? 1 : 0;
    });
}

/**
 * The logic's item effect (what the solver's collection runs): apply the Song of
 * Soaring (OoT) to an empty probe and read back @p questItems (the quest bits it
 * left) and the return value, its flag (1 set, 0 not). Then the undo must clear
 * the flag again: 2 when apply set it and the undo cleared it. -1 with no
 * context.
 */
extern "C" int OoT_SoaringTest_ApplyEffect(uint32_t* questItems) {
    return WithProbe([&](Rando::Logic& probe, SaveContext& save) {
        probe.ApplyItemEffect(Rando::StaticData::RetrieveItem(RG_SONG_OF_SOARING_OOT), true);
        const bool set = probe.CheckRandoInf((uint32_t)RAND_INF_OBTAINED_SONG_OF_SOARING_OOT);
        if (questItems != nullptr) {
            *questItems = save.inventory.questItems;
        }
        probe.ApplyItemEffect(Rando::StaticData::RetrieveItem(RG_SONG_OF_SOARING_OOT), false);
        const bool cleared = !probe.CheckRandoInf((uint32_t)RAND_INF_OBTAINED_SONG_OF_SOARING_OOT);
        return set ? (cleared ? 2 : 1) : 0;
    });
}

// ---- The rando-tier readers (a generated world) ------------------------------

/** How many copies of the Song of Soaring (OoT) the last generation holds: in
 *  OoT's pool still (a paired world's general pass is the single bag's, so its
 *  items wait in `itemPool`) plus placed on OoT's checks. -1 with no context. */
extern "C" int OoT_SoaringTest_WorldCopies(void) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return -1;
    }
    int copies = (int)std::count(itemPool.begin(), itemPool.end(), RG_SONG_OF_SOARING_OOT);
    for (const RandomizerCheck rc : ctx->allLocations) {
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        if (il != nullptr && il->GetPlacedRandomizerGet() == RG_SONG_OF_SOARING_OOT) {
            copies++;
        }
    }
    return copies;
}

/** How many of OoT's song checks (RCTYPE_SONG_LOCATION) hold the Song of Soaring
 *  (OoT) in the last generation. -1 with no context. */
extern "C" int OoT_SoaringTest_OnSongLocations(void) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return -1;
    }
    int copies = 0;
    for (const RandomizerCheck rc : ctx->allLocations) {
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        if (il != nullptr && il->GetPlacedRandomizerGet() == RG_SONG_OF_SOARING_OOT &&
            Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_SONG_LOCATION) {
            copies++;
        }
    }
    return copies;
}

/** 1 when the hint text of RHT_SONG_OF_SOARING_OOT names the song (the clear
 *  text holds "Song of Soaring"), else 0. The hint tables exist once a generation
 *  has run. */
extern "C" int OoT_SoaringTest_HintNamesSong(void) {
    const std::string clear = Rando::StaticData::hintTextTable[RHT_SONG_OF_SOARING_OOT].GetClear().GetEnglish(MF_CLEAN);
    return clear.find("Song of Soaring") != std::string::npos ? 1 : 0;
}

/**
 * The REAL give (Randomizer_Item_Give with a NULL play, as the crossing
 * redemption calls it) on the live save: 1 when it set the song's flag and left
 * OoT's quest bits and every inventory slot as they were, 0 otherwise. The flag
 * is cleared again afterwards.
 */
extern "C" int OoT_SoaringTest_RealGive(void) {
    Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT);
    const uint32_t questBefore = gSaveContext.inventory.questItems;
    u8 itemsBefore[sizeof(gSaveContext.inventory.items)];
    std::memcpy(itemsBefore, gSaveContext.inventory.items, sizeof(itemsBefore));
    Randomizer_Item_Give(nullptr, Rando::StaticData::RetrieveItem(RG_SONG_OF_SOARING_OOT).GetGIEntry_Copy());
    const bool flag = Flags_GetRandomizerInf(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT) != 0;
    const bool questSame = gSaveContext.inventory.questItems == questBefore;
    const bool itemsSame = std::memcmp(itemsBefore, gSaveContext.inventory.items, sizeof(itemsBefore)) == 0;
    Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT);
    return (flag && questSame && itemsSame) ? 1 : 0;
}
