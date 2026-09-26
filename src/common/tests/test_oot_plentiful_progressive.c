/**
 * @file test_oot_plentiful_progressive.c
 * @brief #726: OoT's OWN fill, under a plentiful item pool, never reasons with a
 *        progressive tier past the item's top — and the wallet never wraps.
 *
 * `Logic::ApplyItemEffect`'s progressive rows do `SetUpgrade(x, CurrentUpgrade(x)
 * + 1)`, and `SetUpgrade` ORs the level into its field without masking it. The
 * multiplicity work (PR #728) bounded that only inside a combo round. OoT's own
 * generation was left on upstream's arithmetic, and a plentiful pool
 * (`item_pool.cpp`, `AddItemToPool`'s first count) holds one more copy of the
 * wallet, bow, slingshot, bomb bag, strength, scale and magic than the balanced
 * pool. `ReachabilitySearch` harvests every copy it reaches, so the simulated
 * save the native fill reasons with walks past the top: with the tycoon wallet
 * included the fourth wallet copy wraps the two-bit field to 0 (a 99-rupee
 * wallet, LOWER than the tier before) and carries into the bullet bag's bits;
 * without it the third copy reads as a tycoon wallet the seed does not contain.
 *
 * What this row does, over ONE native generation (no combo round, no
 * coordinator; the profile is set here so it travels with the lock):
 *
 *   L1. The profile took: plentiful, tycoon included, infinite upgrades off
 *       (with them on, a copy at the top resolves to the infinite item and never
 *       walks, so the row would be vacuous).
 *   L2. Anti-vacuity: the world holds MORE wallet copies than wallet tiers, and
 *       the native search reaches every one of them. Without this the row could
 *       pass on a world that never offered the overshoot.
 *   L3. THE LOCK. After the native full-world harvest, every progressive kind's
 *       tier is at most its top tier (logic.cpp's own rule,
 *       ComboLogicProgressiveTopTier), and the wallet is exactly AT its top.
 *       Upstream's arithmetic (main before #726) fails this: the wallet reads 0.
 *
 * The scratch-save walks in combo-logic-multiplicity still observe upstream's
 * arithmetic (their `clamp == 0` legs suppress the tier clamp explicitly), so the
 * defect stays measured there; this row is the native fill's half.
 *
 * `rando` tier: it needs a generated world. Linkage note: #included into
 * test_runner.cpp at FILE SCOPE and compiled as C++.
 */

#include "../test_runner.h"

#include <cstdio>

extern "C" {
// ComboLogicEngineOoT.cpp.
int OoT_ComboLogic_TestProgressiveKindCount(void);
int OoT_ComboLogic_TestProgressiveTopTier(int kind);
int OoT_ComboLogic_TestClampActive(void);
int OoT_NativeFill_TestPlentifulTycoonProfile(void);
int OoT_NativeFill_TestHarvestProgressives(int* outLevels, int* outCopies, int* outReached);
int Rando_HeadlessSeedTest(const char* seedStr);
}

#define PLP_ASSERT(cond, msg)                                               \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

/** Indexed like the engine's progressive kind table (ComboLogicEngineOoT.cpp). */
const char* const kPlentifulKindName[] = { "wallet", "strength",  "scale",  "bomb bag", "bow",
                                           "magic",  "slingshot", "sticks", "nuts" };
constexpr int kPlentifulKindTable = (int)(sizeof(kPlentifulKindName) / sizeof(kPlentifulKindName[0]));
constexpr int kPlentifulKindWallet = 0;

} // namespace

TestResult OoTPlentifulProgressive_Run(void) {
    printf("[TEST] oot-plentiful-progressive: OoT's own fill under a plentiful pool never reasons with a tier past "
           "the top, and the wallet never wraps (#726)\n");

    const int kinds = OoT_ComboLogic_TestProgressiveKindCount();
    PLP_ASSERT(kinds == kPlentifulKindTable, "the OoT kind table changed shape — re-state this row's names");

    // OoT's own option CVars, the same surface the settings menu writes.
    static const char* const kProfile[] = { "gRandoSettings.ItemPool", "gRandoSettings.IncludeTycoonWallet",
                                            "gRandoSettings.InfiniteUpgrades" };
    static const int kProfileValue[] = { 0 /* RO_ITEM_POOL_PLENTIFUL */, 1, 0 /* RO_INF_UPGRADES_OFF */ };
    for (int i = 0; i < 3; ++i) {
        CVarSetInteger(kProfile[i], kProfileValue[i]);
    }
    const int genRc = Rando_HeadlessSeedTest("RSBSPLENTIFUL726");
    for (int i = 0; i < 3; ++i) {
        CVarClear(kProfile[i]);
    }
    PLP_ASSERT(genRc == 0, "headless OoT seed generation failed on the plentiful profile");

    // L1.
    PLP_ASSERT(OoT_NativeFill_TestPlentifulTycoonProfile() == 1,
               "the generated world is not plentiful + tycoon + infinite-upgrades-off — the profile did not take, so "
               "nothing below would test #726");
    PLP_ASSERT(OoT_ComboLogic_TestClampActive() == 0,
               "the combo round clamp is on outside a round — this row must read OoT's NATIVE arithmetic");

    int levels[kPlentifulKindTable];
    int copies[kPlentifulKindTable];
    int reached[kPlentifulKindTable];
    PLP_ASSERT(OoT_NativeFill_TestHarvestProgressives(levels, copies, reached) == kinds,
               "the native harvest bridge refused (solver not live, or a round is open)");

    bool anyPastTop = false;
    for (int kind = 0; kind < kinds; ++kind) {
        const int top = OoT_ComboLogic_TestProgressiveTopTier(kind);
        printf("[TEST] oot-plentiful-progressive: %-9s copies=%d reached=%d level=%d top=%d%s\n",
               kPlentifulKindName[kind], copies[kind], reached[kind], levels[kind], top,
               levels[kind] > top ? "  <-- PAST THE TOP" : "");
        if (levels[kind] > top) {
            anyPastTop = true;
        }
    }

    // L2. With the tycoon included the wallet's tiers are 1..3 above the child
    // wallet, so more reached copies than that is the overshoot's precondition.
    const int walletTop = OoT_ComboLogic_TestProgressiveTopTier(kPlentifulKindWallet);
    PLP_ASSERT(walletTop == 3, "the tycoon profile's wallet top tier is not 3 — the top-tier rule changed");
    PLP_ASSERT(copies[kPlentifulKindWallet] > walletTop,
               "the plentiful world holds no more wallet copies than wallet tiers — the overshoot is not reachable "
               "on this profile, so the lock below would be vacuous; re-derive the premise from item_pool.cpp");
    PLP_ASSERT(reached[kPlentifulKindWallet] == copies[kPlentifulKindWallet],
               "the native search did not reach every wallet copy — the harvest this row reads is partial");

    // L3.
    PLP_ASSERT(!anyPastTop, "OoT's native fill reasons with a progressive tier PAST the item's top (#726)");
    PLP_ASSERT(levels[kPlentifulKindWallet] == walletTop,
               "after harvesting every wallet copy the native simulated save does not hold the top wallet — the "
               "fourth copy wrapped the two-bit field (#726)");

    printf("[TEST] PASS: oot-plentiful-progressive\n");
    return TEST_PASS;
}
