// Majora's Mask's shop, Tingle and merchant prices (#1427). The contract, the
// OoTMM lineage and the draw order are in ShopPrices.h.
#include "ShopPrices.h"

#include "2s2h/ShipUtils.h"

#include <algorithm>

namespace Rando {

namespace ShopPrices {

namespace {

// OoTMM's randomPrice / randomPriceBeta range with colossalWallets off: a draw in
// [0, kDrawSpan), times kDrawnPriceStep, so 0 to 500 Rupees (ShopPrices.h says
// why the cap stays there although MM's shared tier 3 holds 999).
constexpr int32_t kDrawSpan = kMaxDrawnPrice / kDrawnPriceStep + 1;
// betaApproxInt(random, alpha 2, beta 5, ...): the alpha-th lowest of
// alpha + beta - 1 draws.
constexpr int kWeightedDraws = 2 + 5 - 1;
constexpr int kWeightedRank = 2 - 1;

uint16_t RandomPrice() {
    return (uint16_t)(Ship_Random(0, kDrawSpan) * kDrawnPriceStep);
}

uint16_t WeightedPrice() {
    int32_t draws[kWeightedDraws];
    for (int i = 0; i < kWeightedDraws; i++) {
        draws[i] = Ship_Random(0, kDrawSpan);
    }
    std::sort(draws, draws + kWeightedDraws);
    return (uint16_t)(draws[kWeightedRank] * kDrawnPriceStep);
}

} // namespace

// clang-format off
const std::vector<Slot>& Slots() {
    static const std::vector<Slot> kSlots = {
        // OoTMM MM_SHOP_BOMB [30, 40, 50, 90]: shop ids 0x00-0x03.
        { RC_BOMB_SHOP_ITEM_01,                        SlotKind::Shop,     30 },  // Bombs (10)
        { RC_BOMB_SHOP_ITEM_02,                        SlotKind::Shop,     40 },  // Bombchu (10)
        { RC_BOMB_SHOP_ITEM_03,                        SlotKind::Shop,     50 },  // Bomb Bag
        { RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM, SlotKind::Shop,     90 },  // Big Bomb Bag
        // MM_SHOP_CURIOSITY [500]: 0x04.
        { RC_CURIOSITY_SHOP_SPECIAL_ITEM,              SlotKind::Shop,     500 }, // All-Night Mask
        // MM_SHOP_TRADING [30, 80, 80, 50, 10, 30, 30, 30]: 0x05-0x0C.
        { RC_TRADING_POST_SHOP_ITEM_01,                SlotKind::Shop,     30 },  // Red Potion
        { RC_TRADING_POST_SHOP_ITEM_02,                SlotKind::Shop,     80 },  // Green Potion
        { RC_TRADING_POST_SHOP_ITEM_03,                SlotKind::Shop,     80 },  // Hero's Shield
        { RC_TRADING_POST_SHOP_ITEM_04,                SlotKind::Shop,     50 },  // Fairy
        { RC_TRADING_POST_SHOP_ITEM_05,                SlotKind::Shop,     10 },  // Deku Stick
        { RC_TRADING_POST_SHOP_ITEM_06,                SlotKind::Shop,     30 },  // Arrows (30)
        { RC_TRADING_POST_SHOP_ITEM_07,                SlotKind::Shop,     30 },  // Deku Nuts (10)
        { RC_TRADING_POST_SHOP_ITEM_08,                SlotKind::Shop,     30 },  // Arrows (50)
        // MM_SHOP_POTION [60, 10, 20]: 0x0D-0x0F.
        { RC_HAGS_POTION_SHOP_ITEM_01,                 SlotKind::Shop,     60 },  // Blue Potion
        { RC_HAGS_POTION_SHOP_ITEM_02,                 SlotKind::Shop,     10 },  // Green Potion
        { RC_HAGS_POTION_SHOP_ITEM_03,                 SlotKind::Shop,     20 },  // Red Potion
        // MM_SHOP_GORON [40, 40, 80]: 0x10-0x12.
        { RC_GORON_SHOP_ITEM_01,                       SlotKind::Shop,     40 },  // Bombs (10)
        { RC_GORON_SHOP_ITEM_02,                       SlotKind::Shop,     40 },  // Arrows (10)
        { RC_GORON_SHOP_ITEM_03,                       SlotKind::Shop,     80 },  // Red Potion
        // MM_SHOP_ZORA [90, 20, 60]: 0x13-0x15.
        { RC_ZORA_SHOP_ITEM_01,                        SlotKind::Shop,     90 },  // Hero's Shield
        { RC_ZORA_SHOP_ITEM_02,                        SlotKind::Shop,     20 },  // Arrows (10)
        { RC_ZORA_SHOP_ITEM_03,                        SlotKind::Shop,     60 },  // Red Potion
        // MM_TINGLE [5, 40, 20, 40, 20, 40, 20, 40, 20, 40, 20, 40]: Tingle n's
        // first map (his own region's) at 2n, his second (the next region's) at 2n + 1.
        { RC_CLOCK_TOWN_NORTH_TINGLE_MAP_01,           SlotKind::Tingle,   5 },
        { RC_CLOCK_TOWN_NORTH_TINGLE_MAP_02,           SlotKind::Tingle,   40 },
        { RC_ROAD_TO_SOUTHERN_SWAMP_TINGLE_MAP_01,     SlotKind::Tingle,   20 },
        { RC_ROAD_TO_SOUTHERN_SWAMP_TINGLE_MAP_02,     SlotKind::Tingle,   40 },
        { RC_TWIN_ISLANDS_TINGLE_MAP_01,               SlotKind::Tingle,   20 },
        { RC_TWIN_ISLANDS_TINGLE_MAP_02,               SlotKind::Tingle,   40 },
        { RC_MILK_ROAD_TINGLE_MAP_01,                  SlotKind::Tingle,   20 },
        { RC_MILK_ROAD_TINGLE_MAP_02,                  SlotKind::Tingle,   40 },
        { RC_GREAT_BAY_COAST_TINGLE_MAP_01,            SlotKind::Tingle,   20 },
        { RC_GREAT_BAY_COAST_TINGLE_MAP_02,            SlotKind::Tingle,   40 },
        { RC_IKANA_CANYON_TINGLE_MAP_01,               SlotKind::Tingle,   20 },
        { RC_IKANA_CANYON_TINGLE_MAP_02,               SlotKind::Tingle,   40 },
        // MM_MERCHANTS: no price option, the vanilla price always.
        { RC_GORMAN_MILK_PURCHASE,                     SlotKind::Merchant, 50 },
        { RC_MILK_BAR_PURCHASE_MILK,                   SlotKind::Merchant, 20 },
        { RC_MILK_BAR_PURCHASE_CHATEAU,                SlotKind::Merchant, 200 },
    };
    return kSlots;
}
// clang-format on

uint16_t PriceForMode(uint32_t mode, uint16_t vanillaPrice) {
    switch (mode) {
        case RO_PRICES_AFFORDABLE:
            return kAffordablePrice;
        case RO_PRICES_WEIGHTED:
            return WeightedPrice();
        case RO_PRICES_RANDOM:
            return RandomPrice();
        case RO_PRICES_VANILLA:
        default:
            return vanillaPrice;
    }
}

void Assign(RandoSaveInfo& saveInfo, const std::vector<RandoCheckId>& checkPool) {
    const uint32_t shopMode = saveInfo.randoSaveOptions[RO_SHOP_PRICES];
    const uint32_t tingleMode = saveInfo.randoSaveOptions[RO_TINGLE_PRICES];
    for (const Slot& slot : Slots()) {
        uint32_t mode = RO_PRICES_VANILLA;
        if (slot.kind == SlotKind::Shop) {
            mode = shopMode;
        } else if (slot.kind == SlotKind::Tingle) {
            mode = tingleMode;
        }
        // Drawn before the sold test, so every slot of a random mode draws.
        const uint16_t modePrice = PriceForMode(mode, slot.vanillaPrice);
        RandoSaveCheck& check = saveInfo.randoSaveChecks[slot.randoCheckId];
        const bool sold =
            check.skipped || std::find(checkPool.begin(), checkPool.end(), slot.randoCheckId) != checkPool.end();
        check.price = sold ? modePrice : slot.vanillaPrice;
    }
}

} // namespace ShopPrices

} // namespace Rando
