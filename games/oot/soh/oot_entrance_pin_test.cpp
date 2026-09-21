/**
 * @file oot_entrance_pin_test.cpp
 * Locks for #661: in single-executable builds the Happy Mask Shop interior pair
 * must never enter ANY of OoT's own entrance-shuffle pools, because that door IS
 * the OoT<->MM crossing.
 *
 * Two rows, deliberately at different altitudes:
 *   - OoTEntrancePin  (CTest label "redship", dispatch "oot-entrance-pin"):
 *     ROM-free, display-free probe of the POOL CANDIDATE LIST itself. Builds the
 *     Interior pool twice off the same live region graph — once with upstream's
 *     GetShuffleableEntrances(), once with the pinning GetShufflePoolEntrances()
 *     — and asserts the crossing is present in the first and absent from the
 *     second. That pairing is what makes the row non-vacuous: it fails if the
 *     filter is removed (the pin is gone) AND it fails if upstream ever drops
 *     the pair from the Interior table or renames a region (the "RED" side stops
 *     being red), instead of quietly passing against a stale expectation.
 *   - RandoEntrancePin (CTest label "rando", dispatch "rando-entrance-pin"):
 *     an END-TO-END generation with Interior shuffle ALL plus the strongest
 *     pool-mixing configuration that would otherwise swallow this pair (mixed
 *     pools over interiors + overworld, decoupled), asserting the generated
 *     entrance-override table never names either crossing index, while OTHER
 *     interior entrances were in fact shuffled.
 *
 * WHY POOL MEMBERSHIP IS THE RIGHT INVARIANT. Everything downstream keys off it:
 * SetShuffledEntrances() marks only pooled entrances as shuffled;
 * CreateEntranceOverrides() and ValidateWorld() walk every entrance and then
 * filter on IsShuffled(); the spoiler's entrance section is written from
 * EntranceShuffler::playthroughEntrances, which only shuffled entrances reach.
 * So "never in a pool" is simultaneously "no override row", "vanilla at runtime",
 * "vanilla in the entrance tracker" and "absent from the spoiler".
 *
 * The region-keyed predicate under test is tied back to the INDEX-keyed crossing
 * the combo layer owns (src/common/entrance.h's OOT_ENTR_HAPPY_MASK_SHOP 0x0530 /
 * OOT_ENTR_MARKET_FROM_MASK_SHOP 0x01D1) by the index probe below, so a future
 * region rename cannot silently pin the wrong door.
 */

#include "Enhancements/randomizer/randomizer_entrance.h"

#include <libultraship/bridge.h>

#include <cstdio>

// #661 probe surface, bodies in games/oot/soh/Enhancements/randomizer/
// entrance.cpp next to the pin itself (the predicate and the pool wrapper are
// file-local there by design).
extern "C" int Rando_InitRegionGraphForTest(void);
extern "C" int Rando_CountCrossingEntrancesInInteriorPool(int onlyPrimary, int filtered, int* poolSize);
extern "C" int Rando_GetCrossingEntranceIndices(int* forwardIndex, int* reverseIndex);

// games/oot/soh/Enhancements/randomizer/3drando/menu.cpp — the same headless
// generation bridge the RandoGen rows use. It creates the real Settings options
// and then SetAllToContext()s them, so CVars set BEFORE this call are what the
// generation runs with.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);

namespace {

#define OEP_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// src/common/entrance.h's crossing ids. Duplicated as literals rather than
// included: <z64.h> pulls in SoH's own C-linkage Entrance_* family, which
// collides with src/common/entrance.h's C++-linkage Entrance_* API in any
// OoT-side TU (see the long note in oot_scene_flag_freeze_test.cpp). The index
// probe below is precisely the check that keeps these two literals honest.
const int kOotEntrHappyMaskShop = 0x0530;
const int kOotEntrMarketFromMaskShop = 0x01D1;

// A pinned seed so a failure is reproducible. Any seed exercises the pool
// construction; entrance shuffle runs before the item fill.
const char* const kSeed = "rsbs-entrance-pin-661";

} // namespace

// ---------------------------------------------------------------------------
// redship tier: the pair is absent from the shuffle-pool candidate list.
// ---------------------------------------------------------------------------
extern "C" int OoTTest_EntrancePinPool(void) {
    printf("[TEST] oot-entrance-pin: the Happy Mask Shop pair never enters a shuffle pool (#661)\n");

    OEP_ASSERT(Rando_InitRegionGraphForTest() == 0, "could not bring up the rando region graph");

    // --- RED side: upstream's unfiltered builder DOES offer the pair. If this
    // ever reads 0 the pin below would be trivially satisfied, so the row says
    // so instead of passing.
    int rawPrimarySize = 0;
    const int rawPrimary =
        Rando_CountCrossingEntrancesInInteriorPool(/*onlyPrimary=*/1, /*filtered=*/0, &rawPrimarySize);
    printf("[TEST]   unfiltered Interior pool: %d of %d entrances are the crossing\n", rawPrimary, rawPrimarySize);
    OEP_ASSERT(rawPrimarySize > 20, "unfiltered Interior pool is implausibly small - graph did not initialize");
    OEP_ASSERT(rawPrimary == 1, "unfiltered Interior pool does not offer exactly one crossing primary - the RED side "
                                "of this lock is gone, re-derive the pin against upstream's table");

    int rawBothSize = 0;
    const int rawBoth = Rando_CountCrossingEntrancesInInteriorPool(/*onlyPrimary=*/0, /*filtered=*/0, &rawBothSize);
    OEP_ASSERT(rawBoth == 2, "unfiltered Interior pool does not offer both crossing directions");

    // --- GREEN side: the pinning builder offers neither direction, and removed
    // ONLY those entrances (so the filter is not eating the pool).
    int pinnedPrimarySize = 0;
    const int pinnedPrimary =
        Rando_CountCrossingEntrancesInInteriorPool(/*onlyPrimary=*/1, /*filtered=*/1, &pinnedPrimarySize);
    printf("[TEST]   pinned Interior pool: %d of %d entrances are the crossing\n", pinnedPrimary, pinnedPrimarySize);
    OEP_ASSERT(pinnedPrimary == 0, "the crossing is STILL a candidate in the Interior shuffle pool - the pin is not in "
                                   "force (is SINGLE_EXECUTABLE_BUILD defined for this TU?)");
    OEP_ASSERT(pinnedPrimarySize == rawPrimarySize - rawPrimary, "the pin removed more than the crossing from the "
                                                                 "Interior pool");

    int pinnedBothSize = 0;
    const int pinnedBoth =
        Rando_CountCrossingEntrancesInInteriorPool(/*onlyPrimary=*/0, /*filtered=*/1, &pinnedBothSize);
    OEP_ASSERT(pinnedBoth == 0, "the crossing's REVERSE direction is still a candidate (decoupled / mixed pools are "
                                "built from this same list)");
    OEP_ASSERT(pinnedBothSize == rawBothSize - rawBoth, "the pin removed more than the crossing pair");

    // --- Identity: the pinned pair really is the combo's crossing, by index.
    int forwardIndex = -1;
    int reverseIndex = -1;
    const int found = Rando_GetCrossingEntranceIndices(&forwardIndex, &reverseIndex);
    printf("[TEST]   crossing indices: forward 0x%04X, reverse 0x%04X\n", forwardIndex, reverseIndex);
    OEP_ASSERT(found == 2, "the crossing pair is not two entrances in the region graph");
    OEP_ASSERT(forwardIndex == kOotEntrHappyMaskShop,
               "the pinned Market->Mask Shop entrance is not OOT_ENTR_HAPPY_MASK_SHOP");
    OEP_ASSERT(reverseIndex == kOotEntrMarketFromMaskShop,
               "the pinned Mask Shop->Market entrance is not OOT_ENTR_MARKET_FROM_MASK_SHOP");

    printf("[TEST] PASS: oot-entrance-pin\n");
    return 0;
}

// ---------------------------------------------------------------------------
// rando tier: a real generation with interior shuffle ON never overrides it.
// ---------------------------------------------------------------------------
extern "C" int RandoTest_EntrancePinGenerated(void) {
    printf("[TEST] rando-entrance-pin: a generated seed with interior shuffle ON keeps the mask-shop door vanilla "
           "(#661)\n");

    // The strongest configuration that would otherwise swallow this pair:
    // Interior = All (special interiors folded in), overworld entrances on,
    // mixed pools over interiors + overworld (two pools, so MIXED_ENTRANCE_POOLS
    // survives ShuffleAllEntrances' totalMixedPools < 2 reset), and decoupled so
    // the InteriorReverse pool is built too.
    CVarSetInteger("gRandoSettings.ShuffleInteriorsEntrances", 2); // RO_INTERIOR_ENTRANCE_SHUFFLE_ALL
    CVarSetInteger("gRandoSettings.ShuffleOverworldEntrances", 1);
    CVarSetInteger("gRandoSettings.MixedEntrances", 1);
    CVarSetInteger("gRandoSettings.MixInteriors", 1);
    CVarSetInteger("gRandoSettings.MixOverworld", 1);
    CVarSetInteger("gRandoSettings.DecoupleEntrances", 1);

    const int rc = Rando_HeadlessSeedTest(kSeed);
    OEP_ASSERT(rc == 0, "generation failed with interior + overworld + mixed + decoupled entrance shuffle");

    EntranceOverride* overrides = Randomizer_GetEntranceOverrides();
    OEP_ASSERT(overrides != nullptr, "no entrance override table after generation");

    int rows = 0;
    int crossingRows = 0;
    for (int i = 0; i < ENTRANCE_OVERRIDES_MAX_COUNT; i++) {
        if (Entrance_EntranceIsNull(&overrides[i])) {
            continue;
        }
        rows++;
        const int fields[4] = { overrides[i].index, overrides[i].destination, overrides[i].override,
                                overrides[i].overrideDestination };
        for (int f = 0; f < 4; f++) {
            if (fields[f] == kOotEntrHappyMaskShop || fields[f] == kOotEntrMarketFromMaskShop) {
                printf("[TEST]   override row %d names the crossing: type=%u index=0x%04X destination=0x%04X "
                       "override=0x%04X overrideDestination=0x%04X\n",
                       i, overrides[i].type, overrides[i].index, overrides[i].destination, overrides[i].override,
                       overrides[i].overrideDestination);
                crossingRows++;
                break;
            }
        }
    }

    printf("[TEST]   %d entrance override rows written, %d of them name the crossing\n", rows, crossingRows);

    // Non-vacuity: entrance shuffle must actually have DONE something, or "no
    // row names the crossing" is true of an empty table.
    OEP_ASSERT(rows > 20, "the generation produced almost no entrance overrides - the settings above did not take, so "
                          "this row would pass vacuously");
    OEP_ASSERT(crossingRows == 0, "a generated entrance override names the Happy Mask Shop crossing - the cross-game "
                                  "door was shuffled");

    printf("[TEST] PASS: rando-entrance-pin\n");
    return 0;
}
