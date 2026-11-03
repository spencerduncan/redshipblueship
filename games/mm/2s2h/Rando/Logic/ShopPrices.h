#ifndef RANDO_LOGIC_SHOP_PRICES_H
#define RANDO_LOGIC_SHOP_PRICES_H

/**
 * What Majora's Mask's shops, Tingle and the milk sellers charge in a randomized
 * world (#1427): OoTMM's "MM Shop Prices" and "MM Tingle Prices" (priceMmShops,
 * priceMmTingle).
 *
 * THE OPTIONS. RO_SHOP_PRICES prices the shop slots, RO_TINGLE_PRICES Tingle's
 * maps, each one of OoTMM's SETTING_PRICE values (packages/core/src/settings/
 * data.ts:11-21), default Vanilla as in OoTMM:
 *
 *   Vanilla     the slot's own price (OoTMM's PRICES_RAW, below)
 *   Affordable  10 Rupees ("All prices are set to 10 rupees.")
 *   Weighted    0 to 500 Rupees in steps of 5, low prices favoured: OoTMM's
 *               randomPriceBeta, the second lowest of six draws in [0, 100], x5
 *   Random      0 to 500 Rupees in steps of 5: OoTMM's randomPrice, one draw
 *               in [0, 100], x5
 *
 * (packages/logic/src/price.ts; 500 is OoTMM's cap with colossalWallets off, and
 * the giant wallet's capacity. RSBS has no colossalWallets setting, but in the
 * single executable MM's wallet tier 3 is OoT's Tycoon's Wallet at 999 (#1283,
 * z_inventory.c), where OoTMM's colossalWallets would raise random prices to 995.
 * Keeping the 500 cap is a deliberate, conservative divergence: no price asks for
 * the shared tier 3.)
 *
 * THE TABLE. Every priced MM check, once: the 22 shop slots in OoTMM's MM_SHOPS
 * order (Bomb Shop, Curiosity Shop, Trading Post, Kotake's potion shop, Goron
 * shop, Zora shop; slot n is OoTMM's shop id n, generator/src/mm/shop.c
 * Shop_ItemSlot), Tingle's 12 maps in OoTMM's MM_TINGLE order (two per Tingle,
 * Clock Town first), then OoTMM's three MM_MERCHANTS (Gorman's milk, the Milk
 * Bar's milk and Chateau Romani; MM_MERCHANTS is OoTMM's location list,
 * packages/logic/src/locations.ts, not a price range). OoTMM has no MM merchant
 * price option, so they always charge their vanilla price (z_en_tab.c's 20 and
 * 200 Rupees, the "50 Rupees will do ya" Gorman line). The Big Bomb Bag slot is
 * one check here (RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM) where OoTMM
 * prices the Curiosity Shop's copy separately (MM_SHOPS_EX, 100): it takes the
 * Bomb Shop's price.
 *
 * THE PASS. Assign() runs once per pool generation, after the check pool is
 * known (Logic/GeneratePools.cpp), and walks the table in its order. Under a
 * random mode EVERY slot of that mode draws from the seeded stream, sold or not,
 * so a slot's price is a function of the seed and the two modes alone and the
 * stream's position after the pass does not depend on which shops are shuffled.
 * Vanilla and Affordable draw nothing. A sold slot (in the check pool, or an
 * excluded check the pool sells as junk) takes its mode's price; an unsold slot
 * takes its vanilla price, which is what the game charges for it (the rando shop
 * hooks act on shuffled slots only) and what logic then reads for its vanilla
 * item. No other check is priced. GeneratePools calls it in the single-exe build
 * only; a standalone 2Ship build keeps upstream's Ship_Random(0, 200) roll.
 *
 * LOGIC. CAN_AFFORD (Logic.h) asks for no wallet below 100 Rupees, the adult
 * wallet up to 200 and the giant wallet above, which for prices up to 500 is the
 * wallet term of OoTMM's wallet_price (data/macros/macros_mm.yml:371,
 * childWallets off): the wallet that holds the price. OoTMM's wallet_price also
 * asks has_rupees (event RUPEES, :318) for a non-zero price; 2Ship's CAN_AFFORD
 * never modelled that term, before this pass or after it. No mode prices above
 * kMaxDrawnPrice, the giant wallet's capacity, so no mode asks for more than the
 * giant wallet.
 */

#include "Rando/Rando.h"

#include <cstdint>
#include <vector>

namespace Rando {

namespace ShopPrices {

/** Which option prices a slot. */
enum class SlotKind : uint8_t {
    Shop,     // RO_SHOP_PRICES (OoTMM MM_SHOPS)
    Tingle,   // RO_TINGLE_PRICES (OoTMM MM_TINGLE)
    Merchant, // no option: OoTMM's MM_MERCHANTS keep their vanilla price
};

struct Slot {
    RandoCheckId randoCheckId;
    SlotKind kind;
    uint16_t vanillaPrice;
};

/** What an Affordable slot costs. */
constexpr uint16_t kAffordablePrice = 10;
/** The step every drawn price is a multiple of. */
constexpr uint16_t kDrawnPriceStep = 5;
/** The most a drawn price can be, and the giant wallet's capacity. */
constexpr uint16_t kMaxDrawnPrice = 500;

/** Every priced MM check, once, in draw order (see the header). */
const std::vector<Slot>& Slots();

/**
 * The price @p mode (a RandoOptionShopPriceMode value) gives a slot whose vanilla
 * price is @p vanillaPrice. Weighted draws six values and Random one from
 * Ship_Random; Vanilla and Affordable draw nothing. A mode outside
 * RandoOptionShopPriceMode is Vanilla.
 */
uint16_t PriceForMode(uint32_t mode, uint16_t vanillaPrice);

/**
 * The price pass: every table slot's price in @p saveInfo, by the options in
 * @p saveInfo, with @p checkPool the checks GeneratePools put in the pool.
 */
void Assign(RandoSaveInfo& saveInfo, const std::vector<RandoCheckId>& checkPool);

} // namespace ShopPrices

} // namespace Rando

#endif // RANDO_LOGIC_SHOP_PRICES_H
