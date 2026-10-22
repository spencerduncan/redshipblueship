/**
 * @file oot_price_range_test.cpp
 * Lock for #999: the Check Tracker's "Available Checks" asks GetMinimumPrice for
 * the cheapest price an unidentified shop, scrub or merchant slot could carry, and
 * for a Range price setting that is min(range 1, range 2) * 5. The inherited code
 * read range 1 for BOTH ends, so a reversed range (range 1 above range 2, which the
 * generator accepts: shops.cpp draws Random(range2, range1 + 1) in that order)
 * priced every unidentified slot at range 1 and hid checks the player could
 * already afford.
 *
 * OoTPriceRangeMinimum (CTest label "redship", dispatch "oot-price-range-minimum"):
 * display-free and ROM-free. For each of the three price families it sets the
 * mode to Range and asks the file-local GetMinimumPrice, through the
 * Rando_MinimumRangePriceForTest bridge in location_access.cpp, for the minimum of
 * a pair given in both orders, so each assertion names the family and the order it
 * fails for.
 */

#include "soh/Enhancements/randomizer/randomizerTypes.h"

#include <cstdio>

// games/oot/soh/Enhancements/randomizer/location_access.cpp (the file-local
// GetMinimumPrice is reached through this bridge, not a header).
extern "C" int Rando_MinimumRangePriceForTest(int family, int range1, int range2);

namespace {
struct Family {
    const char* name;
    int type;
};
} // namespace

#define OPR_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

extern "C" int OoTTest_PriceRangeMinimum(void) {
    printf("[TEST] oot-price-range-minimum: a Range price setting's tracker minimum is min(range 1, range 2) * 5 in "
           "either slider order (#999)\n");

    const Family families[] = {
        { "shop", RCTYPE_SHOP },
        { "scrub", RCTYPE_SCRUB },
        { "merchant", RCTYPE_MERCHANT },
    };
    // Raw slider units; the price is the slider times 5. {30, 10} is a real price
    // range of 50 to 150, the issue's scenario scaled to sliders.
    const struct {
        int range1;
        int range2;
        int expected;
    } cases[] = {
        { 10, 30, 50 },  // labelled order: lower first
        { 30, 10, 50 },  // reversed: the unfixed read priced this 150
        { 30, 30, 150 }, // equal ends
        { 0, 199, 0 },   // free to the maximum, ascending
        { 199, 0, 0 },   // and descending: the unfixed read priced this 995
    };

    for (const Family& family : families) {
        for (const auto& c : cases) {
            const int got = Rando_MinimumRangePriceForTest(family.type, c.range1, c.range2);
            printf("[TEST]   %s family, range1=%d range2=%d: minimum %d (expected %d)\n", family.name, c.range1,
                   c.range2, got, c.expected);
            OPR_ASSERT(got == c.expected, "GetMinimumPrice did not return min(range1, range2) * 5 for a Range price "
                                          "setting - the tracker prices unidentified slots at the wrong end of a "
                                          "reversed range");
        }
    }

    printf("[TEST] PASS: oot-price-range-minimum\n");
    return 0;
}
