/**
 * ROM-free locks for Majora's Mask's shop and Tingle price modes (#1427):
 * OoTMM's "MM Shop Prices" and "MM Tingle Prices" (priceMmShops, priceMmTingle;
 * packages/core/src/settings/data.ts SETTING_PRICE, packages/logic/src/price.ts).
 * CTest label "redship". Two rows, two entry points:
 *
 * ============================================================================
 * mm-shop-prices (MMShopPrices) -- the options, the page rows, the logic tiers
 * ============================================================================
 *
 *  (1) Both options exist (RO_SHOP_PRICES, RO_TINGLE_PRICES under their
 *      gRando.Options.* keys) and default to Vanilla, value 0, as OoTMM's do.
 *  (2) The MM Randomizer page draws each as a live-at-generation combo box
 *      under "Shuffle Shops & Merchants", labelled "Shop Prices" / "Tingle
 *      Prices", the row after its shuffle (SoH's place for a price row), with
 *      OoTMM's four values in RSBS's order: Vanilla, Affordable, Weighted Random,
 *      Random (OoTMM lists Affordable first; Vanilla is value 0 here because it
 *      is the default, as SoH's price rows list it). The tooltips end a sentence
 *      and stay under 200 characters.
 *  (3) The oracle below covers every priced check: a check is a shop or Tingle
 *      check exactly when it is in kExpected, and the merchants are exactly the
 *      three milk sellers.
 *  (4) Logic asks for the wallet that holds the price: over every price a mode
 *      can produce (0 to 500 in steps of 5, which includes every vanilla price),
 *      CAN_AFFORD at wallet tier t is exactly "price <= that tier's capacity",
 *      which is the wallet term of OoTMM's wallet_price (data/macros/
 *      macros_mm.yml:371; its has_rupees term was never in 2Ship's CAN_AFFORD),
 *      and the giant wallet holds the highest price.
 *
 * ============================================================================
 * mm-shop-prices-pool (MMShopPricesPool) -- the price pass and determinism
 * ============================================================================
 *
 * Each leg runs the real Rando::Logic::GeneratePools over a fresh save info,
 * every option at its default but the ones the leg names, from a seeded stream.
 *
 *  (1) DEFAULT: every priced check holds its vanilla price (sold or not), every
 *      other check holds 0 (the per-check junk price is gone), and the pass draws
 *      nothing: the stream's next value is a fresh stream's first.
 *  (2) AFFORDABLE: every sold shop and Tingle slot costs 10, the merchants keep
 *      their vanilla price, and nothing is drawn.
 *  (3) THE DRAW ORDER: under Random and Weighted, the sold prices are exactly
 *      what OoTMM's formulas give over one stream drawn in the table's order
 *      (shop slots, then Tingle's maps; Random one draw x5, Weighted the second
 *      lowest of six draws x5), and the stream ends where that walk ends.
 *  (4) RANGES: over 30 seeds, every Random and Weighted price is a multiple of 5
 *      in [0, 500], the merchants keep their vanilla prices, Random spreads
 *      over the range, and Weighted favours low prices.
 *  (5) DETERMINISM: the same seed and options give the same prices and the same
 *      stream position twice; another seed gives other prices; and a slot's
 *      price does not depend on which shops are shuffled (Shuffle Shops off and
 *      on agree on the two slots that are always sold, and on the stream).
 *
 * Options are looked up BY NAME and the price values written as numbers (the
 * page's value labels name them), so both rows compile on a tree without the
 * options and fail there on their first leg.
 *
 * Restores the save fields, the excluded-check key and nothing else it touches.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_ShopPrices_RunHeadless(void) {
    return 0;
}
extern "C" int MM_ShopPricesPool_RunHeadless(void) {
    return 0;
}
#else

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"

// src/common: the page's descriptor table.
#include "combo_mm_options_view.h"

extern "C" {
#include "variables.h"
#include "functions.h"
void MM_Rando_Init(void);
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL(%d): ", code);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return code;
}

constexpr const char* kShopOptionName = "RO_SHOP_PRICES";
constexpr const char* kTingleOptionName = "RO_TINGLE_PRICES";

// RandoOptionShopPriceMode by number; leg (2) of mm-shop-prices ties each to its label.
constexpr uint32_t kVanilla = 0;
constexpr uint32_t kAffordable = 1;
constexpr uint32_t kWeighted = 2;
constexpr uint32_t kRandom = 3;
const char* const kValueLabels[] = { "Vanilla", "Affordable", "Weighted Random", "Random" };

/** One priced check: S a shop slot (Shop Prices), T a Tingle map (Tingle
 *  Prices), M a merchant (no option, the vanilla price). */
struct ExpectedSlot {
    const char* check;
    char kind;
    uint16_t vanilla;
};

// OoTMM's PRICES_RAW (packages/logic/src/price.ts) by MM check: MM_SHOP_BOMB,
// MM_SHOP_CURIOSITY, MM_SHOP_TRADING, MM_SHOP_POTION, MM_SHOP_GORON, MM_SHOP_ZORA
// in shop-id order (generator/src/mm/shop.c Shop_ItemSlot), MM_TINGLE two per
// Tingle (his own region's map, then the next region's), then the MM_MERCHANTS'
// vanilla prices (z_en_tab.c's 20 and 200, Gorman's 50). Also the draw order.
const ExpectedSlot kExpected[] = {
    { "RC_BOMB_SHOP_ITEM_01", 'S', 30 },
    { "RC_BOMB_SHOP_ITEM_02", 'S', 40 },
    { "RC_BOMB_SHOP_ITEM_03", 'S', 50 },
    { "RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM", 'S', 90 },
    { "RC_CURIOSITY_SHOP_SPECIAL_ITEM", 'S', 500 },
    { "RC_TRADING_POST_SHOP_ITEM_01", 'S', 30 },
    { "RC_TRADING_POST_SHOP_ITEM_02", 'S', 80 },
    { "RC_TRADING_POST_SHOP_ITEM_03", 'S', 80 },
    { "RC_TRADING_POST_SHOP_ITEM_04", 'S', 50 },
    { "RC_TRADING_POST_SHOP_ITEM_05", 'S', 10 },
    { "RC_TRADING_POST_SHOP_ITEM_06", 'S', 30 },
    { "RC_TRADING_POST_SHOP_ITEM_07", 'S', 30 },
    { "RC_TRADING_POST_SHOP_ITEM_08", 'S', 30 },
    { "RC_HAGS_POTION_SHOP_ITEM_01", 'S', 60 },
    { "RC_HAGS_POTION_SHOP_ITEM_02", 'S', 10 },
    { "RC_HAGS_POTION_SHOP_ITEM_03", 'S', 20 },
    { "RC_GORON_SHOP_ITEM_01", 'S', 40 },
    { "RC_GORON_SHOP_ITEM_02", 'S', 40 },
    { "RC_GORON_SHOP_ITEM_03", 'S', 80 },
    { "RC_ZORA_SHOP_ITEM_01", 'S', 90 },
    { "RC_ZORA_SHOP_ITEM_02", 'S', 20 },
    { "RC_ZORA_SHOP_ITEM_03", 'S', 60 },
    { "RC_CLOCK_TOWN_NORTH_TINGLE_MAP_01", 'T', 5 },
    { "RC_CLOCK_TOWN_NORTH_TINGLE_MAP_02", 'T', 40 },
    { "RC_ROAD_TO_SOUTHERN_SWAMP_TINGLE_MAP_01", 'T', 20 },
    { "RC_ROAD_TO_SOUTHERN_SWAMP_TINGLE_MAP_02", 'T', 40 },
    { "RC_TWIN_ISLANDS_TINGLE_MAP_01", 'T', 20 },
    { "RC_TWIN_ISLANDS_TINGLE_MAP_02", 'T', 40 },
    { "RC_MILK_ROAD_TINGLE_MAP_01", 'T', 20 },
    { "RC_MILK_ROAD_TINGLE_MAP_02", 'T', 40 },
    { "RC_GREAT_BAY_COAST_TINGLE_MAP_01", 'T', 20 },
    { "RC_GREAT_BAY_COAST_TINGLE_MAP_02", 'T', 40 },
    { "RC_IKANA_CANYON_TINGLE_MAP_01", 'T', 20 },
    { "RC_IKANA_CANYON_TINGLE_MAP_02", 'T', 40 },
    { "RC_GORMAN_MILK_PURCHASE", 'M', 50 },
    { "RC_MILK_BAR_PURCHASE_MILK", 'M', 20 },
    { "RC_MILK_BAR_PURCHASE_CHATEAU", 'M', 200 },
};
constexpr int kExpectedCount = (int)(sizeof(kExpected) / sizeof(kExpected[0]));

// OoTMM's ranges with colossalWallets off: a draw in [0, 101), times 5.
constexpr int32_t kDrawSpan = 101;
constexpr uint16_t kStep = 5;
constexpr uint16_t kMaxPrice = 500;
constexpr uint16_t kAffordablePrice = 10;
// The draw that tells where a stream stands after a generation.
constexpr int32_t kSentinelSpan = 0x7FFFFFFF;

/** kExpected's check ids, resolved by name; empty on a failure (named in `why`). */
bool ResolveExpected(std::vector<RandoCheckId>& ids, std::string& why) {
    ids.clear();
    for (const ExpectedSlot& slot : kExpected) {
        const RandoCheckId id = Rando::StaticData::GetCheckIdFromName(slot.check);
        if (id == RC_UNKNOWN) {
            why = std::string("MM's check table has no ") + slot.check;
            ids.clear();
            return false;
        }
        ids.push_back(id);
    }
    return true;
}

/** Both option ids, or RO_MAX for a missing one. */
RandoOptionId ShopOption() {
    return Rando::StaticData::GetOptionIdFromName(kShopOptionName);
}
RandoOptionId TingleOption() {
    return Rando::StaticData::GetOptionIdFromName(kTingleOptionName);
}

// ============================================================================
// mm-shop-prices
// ============================================================================

int CheckOptionRow(RandoOptionId option, const char* name, const char* label, uint16_t shuffleOption,
                   const char* shuffleLabel, int code) {
    if (option >= RO_MAX) {
        return Fail(code, "MM has no %s option: the price modes are missing (#1427)", name);
    }
    const auto& row = Rando::StaticData::Options.at(option);
    const std::string cvar = std::string("gRando.Options.") + name;
    if (strcmp(row.cvar, cvar.c_str()) != 0 || row.defaultValue != kVanilla) {
        return Fail(code + 1, "%s binds '%s' and defaults to %u; expected '%s' and Vanilla (0), OoTMM's default", name,
                    row.cvar, (unsigned)row.defaultValue, cvar.c_str());
    }
    const ComboMMOptionDesc* desc = Combo_MMOptionById((uint16_t)option);
    if (desc == NULL) {
        return Fail(code + 2, "the MM Randomizer page has no row for %s", name);
    }
    if (desc->label == NULL || strcmp(desc->label, label) != 0 || desc->widget != (uint8_t)COMBO_MM_WIDGET_COMBO ||
        desc->defaultValue != (int32_t)kVanilla || desc->liveness != (uint8_t)COMBO_MM_LIVENESS_GENERATION_ONLY ||
        strcmp(Combo_MMOptionGroupName(desc->group), "Shuffle Shops & Merchants") != 0) {
        return Fail(code + 3,
                    "the %s row is '%s', not a generation-only combo box labelled '%s', defaulting to Vanilla, under "
                    "Shuffle Shops & Merchants",
                    name, desc->label ? desc->label : "(null)", label);
    }
    const int valueCount = (int)(sizeof(kValueLabels) / sizeof(kValueLabels[0]));
    if (desc->valueCount != (uint8_t)valueCount || desc->valueLabels == NULL) {
        return Fail(code + 4, "the %s row offers %u values, OoTMM's price setting has %d", name,
                    (unsigned)desc->valueCount, valueCount);
    }
    for (int v = 0; v < valueCount; v++) {
        if (desc->valueLabels[v] == NULL || strcmp(desc->valueLabels[v], kValueLabels[v]) != 0) {
            return Fail(code + 4, "the %s row's value %d is '%s', expected '%s'", name, v,
                        desc->valueLabels[v] ? desc->valueLabels[v] : "(null)", kValueLabels[v]);
        }
    }
    const size_t tipLength = desc->tooltip != NULL ? strlen(desc->tooltip) : 0;
    if (tipLength == 0 || tipLength > 200 || desc->tooltip[tipLength - 1] != '.') {
        return Fail(code + 5, "the %s row's tooltip is %zu characters and must end a sentence within 200", name,
                    tipLength);
    }
    int shuffleAt = -1;
    int rowAt = -1;
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* at = Combo_MMOptionAt(i);
        if (at->id == shuffleOption) {
            shuffleAt = i;
        } else if (at->id == (uint16_t)option) {
            rowAt = i;
        }
    }
    if (shuffleAt < 0 || rowAt != shuffleAt + 1) {
        return Fail(code + 6, "%s is row %d and %s row %d: a price row belongs right after its shuffle, as in SoH",
                    label, rowAt, shuffleLabel, shuffleAt);
    }
    return 0;
}

int RunShopPrices() {
    // ---- (1) + (2) the options and their rows ------------------------------
    MM_RandoOptionsUi_Register();
    int rc =
        CheckOptionRow(ShopOption(), kShopOptionName, "Shop Prices", (uint16_t)RO_SHUFFLE_SHOPS, "Shuffle Shops", 1);
    if (rc != 0) {
        return rc;
    }
    rc = CheckOptionRow(TingleOption(), kTingleOptionName, "Tingle Prices", (uint16_t)RO_SHUFFLE_TINGLE_SHOPS,
                        "Shuffle Tingle Maps", 11);
    if (rc != 0) {
        return rc;
    }

    // ---- (3) the oracle covers every priced check ---------------------------
    std::vector<RandoCheckId> ids;
    std::string why;
    if (!ResolveExpected(ids, why)) {
        return Fail(21, "%s", why.c_str());
    }
    std::set<RandoCheckId> priced(ids.begin(), ids.end());
    if ((int)priced.size() != kExpectedCount) {
        return Fail(22, "the price oracle names a check twice");
    }
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        const bool shopType = check.randoCheckType == RCTYPE_SHOP || check.randoCheckType == RCTYPE_TINGLE_SHOP;
        if (shopType != (priced.count(randoCheckId) != 0)) {
            return Fail(23, "%s %s a shop or Tingle check but %s a price", check.name, shopType ? "is" : "is not",
                        shopType ? "has no" : "has");
        }
    }
    for (int i = 0; i < kExpectedCount; i++) {
        const RandoCheckType type = Rando::StaticData::Checks.at(ids[(size_t)i]).randoCheckType;
        const bool tingle = type == RCTYPE_TINGLE_SHOP;
        if ((kExpected[i].kind == 'T') != tingle) {
            return Fail(24, "%s is priced as %c but is %sa Tingle map", kExpected[i].check, kExpected[i].kind,
                        tingle ? "" : "not ");
        }
    }

    // ---- (4) logic asks for the wallet that holds the price -----------------
    const RandoCheckId probe = ids[4]; // RC_CURIOSITY_SHOP_SPECIAL_ITEM, the 500-Rupee All-Night Mask
    const u32 savedUpgrades = gSaveContext.save.saveInfo.inventory.upgrades;
    const u16 savedPrice = RANDO_SAVE_CHECKS[probe].price;
    int failure = 0;
    if (MM_gUpgradeCapacities[UPG_WALLET][2] < kMaxPrice) {
        failure = Fail(31, "the giant wallet holds %u Rupees, less than the %u a price can reach",
                       (unsigned)MM_gUpgradeCapacities[UPG_WALLET][2], (unsigned)kMaxPrice);
    }
    for (int tier = 0; tier <= 2 && failure == 0; tier++) {
        MM_Inventory_ChangeUpgrade(UPG_WALLET, (u32)tier);
        const uint16_t capacity = MM_gUpgradeCapacities[UPG_WALLET][tier];
        for (uint16_t price = 0; price <= kMaxPrice && failure == 0; price += kStep) {
            RANDO_SAVE_CHECKS[probe].price = price;
            const bool affordable = CAN_AFFORD(probe);
            if (affordable != (price <= capacity)) {
                failure = Fail(32,
                               "CAN_AFFORD reads %d for %u Rupees at wallet tier %d (capacity %u): logic must ask "
                               "for exactly the wallet that holds the price",
                               affordable ? 1 : 0, (unsigned)price, tier, (unsigned)capacity);
            }
        }
    }
    gSaveContext.save.saveInfo.inventory.upgrades = savedUpgrades;
    RANDO_SAVE_CHECKS[probe].price = savedPrice;
    if (failure != 0) {
        return failure;
    }

    printf("[TEST] PASS: Shop Prices and Tingle Prices are OoTMM's four modes, Vanilla by default, drawn after their "
           "shuffles; %d checks are priced; logic asks for the wallet that holds each price up to %u\n",
           kExpectedCount, (unsigned)kMaxPrice);
    return 0;
}

// ============================================================================
// mm-shop-prices-pool
// ============================================================================

struct PoolRun {
    std::vector<uint16_t> prices; // by RandoCheckId
    int32_t sentinel = 0;         // the stream's next draw after GeneratePools
};

/** One pool generation over a fresh save info: every option at its default but
 *  the four named here, from a stream seeded with @p seed. */
PoolRun GeneratePrices(uint64_t seed, bool shopsOn, bool tingleOn, uint32_t shopMode, uint32_t tingleMode) {
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    info->randoSaveOptions[RO_SHUFFLE_SHOPS] = shopsOn ? RO_GENERIC_YES : RO_GENERIC_NO;
    info->randoSaveOptions[RO_SHUFFLE_TINGLE_SHOPS] = tingleOn ? RO_GENERIC_YES : RO_GENERIC_NO;
    info->randoSaveOptions[ShopOption()] = shopMode;
    info->randoSaveOptions[TingleOption()] = tingleMode;
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Ship_Random_Seed(seed);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    PoolRun run;
    run.sentinel = Ship_Random(0, kSentinelSpan);
    run.prices.assign((size_t)RC_MAX, 0);
    for (int id = 0; id < (int)RC_MAX; id++) {
        run.prices[(size_t)id] = info->randoSaveChecks[id].price;
    }
    return run;
}

/** A fresh stream's first draw from @p seed. */
int32_t FreshSentinel(uint64_t seed) {
    Ship_Random_Seed(seed);
    return Ship_Random(0, kSentinelSpan);
}

/** OoTMM's formulas over one stream: randomPrice and randomPriceBeta(2, 5). */
uint16_t OracleRandom() {
    return (uint16_t)(Ship_Random(0, kDrawSpan) * kStep);
}
uint16_t OracleWeighted() {
    int32_t draws[6];
    for (int i = 0; i < 6; i++) {
        draws[i] = Ship_Random(0, kDrawSpan);
    }
    std::sort(draws, draws + 6);
    return (uint16_t)(draws[1] * kStep);
}

/** The prices OoTMM's walk gives every shop and Tingle slot (merchants: 0, not
 *  drawn) under @p shopMode and @p tingleMode from @p seed, and where the stream
 *  ends. Only random modes may be passed. */
PoolRun OraclePrices(uint64_t seed, uint32_t shopMode, uint32_t tingleMode, const std::vector<RandoCheckId>& ids) {
    PoolRun run;
    run.prices.assign((size_t)RC_MAX, 0);
    Ship_Random_Seed(seed);
    for (int i = 0; i < kExpectedCount; i++) {
        if (kExpected[i].kind == 'M') {
            continue;
        }
        const uint32_t mode = kExpected[i].kind == 'S' ? shopMode : tingleMode;
        run.prices[(size_t)ids[(size_t)i]] = mode == kWeighted ? OracleWeighted() : OracleRandom();
    }
    run.sentinel = Ship_Random(0, kSentinelSpan);
    return run;
}

int RunShopPricesPool() {
    if (ShopOption() >= RO_MAX || TingleOption() >= RO_MAX) {
        return Fail(1, "MM has no %s / %s option: the price modes are missing (#1427)", kShopOptionName,
                    kTingleOptionName);
    }
    std::vector<RandoCheckId> ids;
    std::string why;
    if (!ResolveExpected(ids, why)) {
        return Fail(2, "%s", why.c_str());
    }
    MM_Rando_Init(); // fills Rando::Logic::Regions (irreversible; ShipInit::InitAll)
    if (Rando::Logic::Regions.empty()) {
        return Fail(3, "MM's region graph is empty: no pool can be generated");
    }
    // An excluded check is sold as junk and would shift what this row reads.
    const std::string savedExcludes = CVarGetString("gRando.ExcludedChecks", "");
    CVarClear("gRando.ExcludedChecks");

    auto run = [&]() -> int {
        // ---- (1) the default profile ---------------------------------------
        constexpr uint64_t kSeed = 0x5409u;
        const PoolRun def = GeneratePrices(kSeed, false, false, kVanilla, kVanilla);
        std::set<RandoCheckId> priced(ids.begin(), ids.end());
        for (int i = 0; i < kExpectedCount; i++) {
            if (def.prices[(size_t)ids[(size_t)i]] != kExpected[i].vanilla) {
                return Fail(11, "default profile: %s costs %u, its vanilla price is %u", kExpected[i].check,
                            (unsigned)def.prices[(size_t)ids[(size_t)i]], (unsigned)kExpected[i].vanilla);
            }
        }
        for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
            if (priced.count(randoCheckId) == 0 && def.prices[(size_t)randoCheckId] != 0) {
                return Fail(12, "default profile: %s, which sells nothing, carries a price of %u", check.name,
                            (unsigned)def.prices[(size_t)randoCheckId]);
            }
        }
        if (def.sentinel != FreshSentinel(kSeed)) {
            return Fail(13, "default profile: pool generation drew from the stream; Vanilla prices draw nothing");
        }

        // ---- (2) Affordable --------------------------------------------------
        const PoolRun cheap = GeneratePrices(kSeed, true, true, kAffordable, kAffordable);
        for (int i = 0; i < kExpectedCount; i++) {
            const uint16_t want = kExpected[i].kind == 'M' ? kExpected[i].vanilla : kAffordablePrice;
            if (cheap.prices[(size_t)ids[(size_t)i]] != want) {
                return Fail(21, "Affordable: %s costs %u, expected %u", kExpected[i].check,
                            (unsigned)cheap.prices[(size_t)ids[(size_t)i]], (unsigned)want);
            }
        }
        if (cheap.sentinel != FreshSentinel(kSeed)) {
            return Fail(22, "Affordable: pool generation drew from the stream; Affordable draws nothing");
        }

        // ---- (3) the draw order ----------------------------------------------
        const uint32_t kModes[][2] = { { kRandom, kRandom }, { kWeighted, kWeighted }, { kWeighted, kRandom } };
        for (const auto& modes : kModes) {
            const PoolRun got = GeneratePrices(kSeed, true, true, modes[0], modes[1]);
            const PoolRun want = OraclePrices(kSeed, modes[0], modes[1], ids);
            for (int i = 0; i < kExpectedCount; i++) {
                const size_t id = (size_t)ids[(size_t)i];
                const uint16_t expected = kExpected[i].kind == 'M' ? kExpected[i].vanilla : want.prices[id];
                if (got.prices[id] != expected) {
                    return Fail(31, "shops %s, Tingle %s: %s costs %u, OoTMM's walk in table order gives %u",
                                kValueLabels[modes[0]], kValueLabels[modes[1]], kExpected[i].check,
                                (unsigned)got.prices[id], (unsigned)expected);
                }
            }
            if (got.sentinel != want.sentinel) {
                return Fail(32, "shops %s, Tingle %s: the stream does not end where the table's walk ends",
                            kValueLabels[modes[0]], kValueLabels[modes[1]]);
            }
        }

        // ---- (4) ranges over 30 seeds ----------------------------------------
        for (uint32_t mode : { kRandom, kWeighted }) {
            std::set<uint16_t> distinct;
            double sum = 0;
            int samples = 0;
            for (uint64_t seed = 1; seed <= 30; seed++) {
                const PoolRun got = GeneratePrices(seed * 7919u, true, true, mode, mode);
                for (int i = 0; i < kExpectedCount; i++) {
                    const uint16_t price = got.prices[(size_t)ids[(size_t)i]];
                    if (kExpected[i].kind == 'M') {
                        if (price != kExpected[i].vanilla) {
                            return Fail(41, "%s: the merchant %s costs %u; merchants keep their vanilla %u",
                                        kValueLabels[mode], kExpected[i].check, (unsigned)price,
                                        (unsigned)kExpected[i].vanilla);
                        }
                        continue;
                    }
                    if (price > kMaxPrice || price % kStep != 0) {
                        return Fail(42, "%s: %s costs %u, outside 0-%u in steps of %u", kValueLabels[mode],
                                    kExpected[i].check, (unsigned)price, (unsigned)kMaxPrice, (unsigned)kStep);
                    }
                    distinct.insert(price);
                    sum += price;
                    samples++;
                }
            }
            const double mean = sum / samples;
            // Uniform over 0-500 has mean 250; OoTMM's beta(2, 5) about 143. Each
            // mean is over 1020 draws, so these bounds are many deviations wide.
            if (mode == kRandom && (mean < 200.0 || mean > 300.0 || distinct.size() < 50)) {
                return Fail(43,
                            "Random: mean %.1f over %d prices and %zu distinct values; a uniform 0-500 draw "
                            "spreads over the range",
                            mean, samples, distinct.size());
            }
            if (mode == kWeighted && (mean < 100.0 || mean > 190.0)) {
                return Fail(44, "Weighted Random: mean %.1f over %d prices; OoTMM's weighting favours low prices", mean,
                            samples);
            }
        }

        // ---- (5) determinism -------------------------------------------------
        const PoolRun once = GeneratePrices(kSeed, true, true, kWeighted, kRandom);
        const PoolRun twice = GeneratePrices(kSeed, true, true, kWeighted, kRandom);
        if (once.prices != twice.prices || once.sentinel != twice.sentinel) {
            return Fail(51, "the same seed and options gave two different price tables");
        }
        const PoolRun other = GeneratePrices(kSeed + 1, true, true, kWeighted, kRandom);
        if (other.prices == once.prices) {
            return Fail(52, "two seeds gave one price table: the prices do not come from the seeded stream");
        }
        const PoolRun shopsOff = GeneratePrices(kSeed, false, true, kRandom, kRandom);
        const PoolRun shopsOn = GeneratePrices(kSeed, true, true, kRandom, kRandom);
        for (int i = 0; i < kExpectedCount; i++) {
            const size_t id = (size_t)ids[(size_t)i];
            const bool alwaysSold = strcmp(kExpected[i].check, "RC_CURIOSITY_SHOP_SPECIAL_ITEM") == 0 ||
                                    strcmp(kExpected[i].check, "RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM") == 0;
            if (alwaysSold || kExpected[i].kind == 'T') {
                if (shopsOff.prices[id] != shopsOn.prices[id]) {
                    return Fail(53,
                                "%s costs %u with Shuffle Shops off and %u with it on: a slot's price must not "
                                "depend on which shops are shuffled",
                                kExpected[i].check, (unsigned)shopsOff.prices[id], (unsigned)shopsOn.prices[id]);
                }
            } else if (shopsOff.prices[id] != kExpected[i].vanilla) {
                return Fail(54, "%s is not shuffled but costs %u; an unsold slot charges its vanilla %u",
                            kExpected[i].check, (unsigned)shopsOff.prices[id], (unsigned)kExpected[i].vanilla);
            }
        }
        if (shopsOff.sentinel != shopsOn.sentinel) {
            return Fail(55, "Shuffle Shops moved the stream: every slot of a random mode must draw, sold or not");
        }
        return 0;
    };

    const int result = run();
    CVarSetString("gRando.ExcludedChecks", savedExcludes.c_str());
    if (savedExcludes.empty()) {
        CVarClear("gRando.ExcludedChecks");
    }
    if (result != 0) {
        return result;
    }
    printf("[TEST] PASS: the default profile prices %d checks at vanilla and draws nothing; Affordable is 10; "
           "Random and Weighted Random follow OoTMM's formulas over one stream in table order, within 0-%u; the "
           "prices are a function of the seed and the modes\n",
           kExpectedCount, (unsigned)kMaxPrice);
    return 0;
}

} // namespace

extern "C" int MM_ShopPrices_RunHeadless(void) {
    printf("[TEST] mm-shop-prices: OoTMM's MM shop and Tingle price modes, their rows and the logic tiers (#1427)\n");
    return RunShopPrices();
}

extern "C" int MM_ShopPricesPool_RunHeadless(void) {
    printf("[TEST] mm-shop-prices-pool: the price pass over a real pool generation, and its determinism (#1427)\n");
    return RunShopPricesPool();
}

#endif // RSBS_SINGLE_EXECUTABLE
