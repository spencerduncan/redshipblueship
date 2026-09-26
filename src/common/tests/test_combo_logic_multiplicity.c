/**
 * @file test_combo_logic_multiplicity.c
 * @brief The multiplicity contract (combo_logic.h ABI 3, operator ruling
 *        2026-09-26) over BOTH REAL engines: every copy counts, and no copy past
 *        a top tier or a counter maximum can lower or overflow anything (#645).
 *
 * The coordinator half of the ruling — one `assumeOwnItem` per copy, order
 * independence, the bag's surplus / filler / exact-fit shapes — is locked ROM-free
 * over stub engines by `combo-logic-bag-model` (test_combo_logic.c). What a stub
 * cannot show is what each PORT's give path does with a repeat, because that is
 * the ports' own code. So this row drives the two real engines:
 *
 *   O1. OoT, THE DEFECT, OBSERVED. With the round clamp OFF — upstream's own
 *       arithmetic, the state every non-combo caller runs in — successive
 *       `RG_PROGRESSIVE_WALLET` copies walk the wallet's two-bit field past its
 *       top and it reads LOWER than before (the carry lands in the bullet bag's
 *       bits). That is the monotonicity violation the ABI-2 de-dup existed to
 *       avoid, measured rather than read out of logic.cpp.
 *   O2. OoT, THE CLAMP. With it ON, every progressive kind stops exactly at
 *       logic.cpp's own top tier, never decreases, and leaves every OTHER upgrade
 *       field untouched.
 *   O3. OoT, THROUGH THE VTABLE. The clamp is on exactly between `beginQuery` and
 *       `endQuery`; inside a round two wallet copies are two tiers (the de-dup
 *       gave one), copies past the top stay at the top, and an `expand` — which
 *       re-derives the inventory, applies the starting items and HARVESTS the
 *       generated world's own placed wallets — still reads the top tier.
 *   M1. MM, COUNTERS COUNT AND STOP. For a small key, a stray fairy and a skull
 *       token, (max + 2) copies through the vtable read min(k, max) after each
 *       copy k, with the maximum derived from MM's own static check table.
 *   M2. MM, THE HARVEST IS PER HOST. The same MM-origin item placed on two
 *       reached hosts is harvested twice in one round (the de-dup harvested it
 *       once).
 *
 * And the row leaves the process as it found it: OoT's world digest, MM's
 * snapshot not live, no coordinator placement, and the unified save buffer
 * byte-identical to the state MM's shipped profile left it in — compared BEFORE
 * the outer restore, which is the ordering combo-logic-measure's S4 learned.
 *
 * `rando` tier for the same reason as combo-logic-measure: with no generated
 * world `Logic` has no settings, `ctx->allLocations` is empty and MM's graph is
 * unpopulated, so every assertion below would be satisfied by a zero.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE and compiled as C++.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../game.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
// OoT (ComboLogicEngineOoT.cpp).
int OoT_ComboLogic_TestProgressiveKindCount(void);
int OoT_ComboLogic_TestProgressiveItemId(int kind);
int OoT_ComboLogic_TestProgressiveTopTier(int kind);
int OoT_ComboLogic_TestProgressiveWalk(int kind, int grants, int clamp, int* outLevels, uint32_t* outUpgrades);
int OoT_ComboLogic_TestRoundProgressiveLevel(int kind);
int OoT_ComboLogic_TestClampActive(void);
uint32_t OoT_ComboLogic_TestWorldDigest(void);
int Rando_HeadlessSeedTest(const char* seedStr);

// MM (ComboLogicEngineSingleExe.cpp) + its rando bring-up.
void MM_Rando_InitCore(void);
int MM_ComboLogic_ApplyShippedProfile(void);
int MM_ComboLogic_TestCounterKindCount(void);
int MM_ComboLogic_TestCounterItemId(int kind);
int MM_ComboLogic_TestCounterValue(int kind);
int MM_ComboLogic_TestCounterMax(int kind);
void MM_ComboLogic_TestZeroCounter(int kind);
int MM_ComboLogic_CounterClamps(void);
int MM_ComboLogic_RoundCopiesGranted(void);
int MM_ComboLogic_HarvestCount(void);
int MM_ComboLogic_SnapshotLive(void);
int MM_ComboLogic_HeldPlacementCount(void);
void MM_ComboLogic_ResetCounters(void);

extern char gSaveContext[];
}

#define CLX_ASSERT(cond, msg)                                                   \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);      \
            return TEST_FAIL;                                                   \
        }                                                                       \
    } while (0)

namespace {

/** The OoT upgrade field a progressive kind writes, as a mask over
 *  `inventory.upgrades` (OoT_gUpgradeMasks, z_inventory.c), or 0 for the magic
 *  meter, which is not an upgrade field. Indexed like the engine's kind table:
 *  wallet, strength, scale, bomb bag, bow. */
const uint32_t kOoTKindFieldMask[] = { 0x00003000u, 0x000001C0u, 0x00000E00u, 0x00000038u, 0x00000007u, 0u };

const char* const kOoTKindName[] = { "wallet", "strength", "scale", "bomb bag", "bow", "magic" };
const char* const kMmKindName[] = { "Woodfall small key", "Stone Tower small key", "Woodfall stray fairy",
                                    "swamp skull token" };

} // namespace

TestResult ComboLogicMultiplicity_Run(void) {
    printf("[TEST] combo-logic-multiplicity: every copy counts, and no copy past a top tier or a counter maximum "
           "lowers or overflows anything — over both real engines (combo_logic.h ABI 3; #645)\n");

    const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mm = Combo_Logic_GetEngine(GAME_MM);
    CLX_ASSERT(oot != nullptr && mm != nullptr, "both real engines must be registered");
    CLX_ASSERT(oot->abiVersion == 3u && mm->abiVersion == 3u, "both engines must speak ABI 3 (one call is one copy)");

    CLX_ASSERT(Rando_HeadlessSeedTest("RSBSCOMBOMULTI1") == 0, "headless OoT seed generation failed");
    MM_Rando_InitCore();

    std::unique_ptr<unsigned char[]> saveBefore(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveBefore.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    const uint32_t worldDigest0 = OoT_ComboLogic_TestWorldDigest();
    CLX_ASSERT(worldDigest0 != 0u, "OoT's world digest is zero — no fill result is visible");

    // ==================================================================
    // O1. THE DEFECT, OBSERVED: the clamp off is upstream's arithmetic.
    // ==================================================================
    const int kinds = OoT_ComboLogic_TestProgressiveKindCount();
    CLX_ASSERT(kinds == 6, "the OoT kind table changed shape — re-state this row's field masks");
    {
        int levels[8];
        uint32_t word = 0u;
        CLX_ASSERT(OoT_ComboLogic_TestProgressiveWalk(0, 6, 0, levels, &word) == 6, "the unclamped wallet walk ran");
        printf("[TEST] combo-logic-multiplicity: O1 wallet, clamp OFF (upstream): levels after each copy = %d %d %d %d "
               "%d %d; upgrades word 0x%08X\n",
               levels[0], levels[1], levels[2], levels[3], levels[4], levels[5], (unsigned)word);
        bool lowered = false;
        for (int i = 1; i < 6; ++i) {
            if (levels[i] < levels[i - 1]) {
                lowered = true;
            }
        }
        CLX_ASSERT(lowered,
                   "the unclamped wallet walk never LOWERED the wallet — the defect this clamp exists for is not "
                   "observable on this tree, so the clamp's lock below would be vacuous; re-derive the premise");
        CLX_ASSERT((word & 0x0001C000u) != 0u,
                   "the lowering did not carry into the bullet bag's bits — the mechanism differs from the one "
                   "logic.cpp's clamp comment states");
    }

    // ==================================================================
    // O2. THE CLAMP, per kind.
    // ==================================================================
    for (int kind = 0; kind < kinds; ++kind) {
        int base[1];
        uint32_t baseWord = 0u;
        CLX_ASSERT(OoT_ComboLogic_TestProgressiveWalk(kind, 0, 1, base, &baseWord) == 0, "the zero-copy walk ran");
        const int top = OoT_ComboLogic_TestProgressiveTopTier(kind);
        CLX_ASSERT(top >= 2 && top <= 3, "a top tier outside 2..3 — the rule changed");
        const int grants = top + 4; // the first-tier flag copy, every tier, and surplus past the top
        int levels[8];
        uint32_t word = 0u;
        CLX_ASSERT(OoT_ComboLogic_TestProgressiveWalk(kind, grants, 1, levels, &word) == grants, "clamped walk ran");
        printf("[TEST] combo-logic-multiplicity: O2 %-8s clamp ON: top=%d levels=", kOoTKindName[kind], top);
        for (int i = 0; i < grants; ++i) {
            printf("%d ", levels[i]);
        }
        printf("\n");
        for (int i = 1; i < grants; ++i) {
            CLX_ASSERT(levels[i] >= levels[i - 1], "a clamped copy LOWERED a progressive tier");
        }
        CLX_ASSERT(levels[grants - 1] == top,
                   "surplus copies did not end exactly at the item's top tier — the clamp over- or under-shoots");
        for (int i = 0; i < grants; ++i) {
            CLX_ASSERT(levels[i] <= top, "a clamped copy went past the top tier");
        }
        const uint32_t own = kOoTKindFieldMask[kind];
        CLX_ASSERT((word & ~own) == (baseWord & ~own),
                   "a clamped walk changed ANOTHER upgrade field — a carry walked into a neighbour");
    }

    // ==================================================================
    // O3. THROUGH THE VTABLE: scoped clamp, per-copy tiers, and expand.
    // ==================================================================
    {
        const int walletId = OoT_ComboLogic_TestProgressiveItemId(0);
        const int top = OoT_ComboLogic_TestProgressiveTopTier(0);
        CLX_ASSERT(walletId > 0, "no wallet id");
        CLX_ASSERT(OoT_ComboLogic_TestClampActive() == 0, "the clamp is on OUTSIDE a round");
        CLX_ASSERT(oot->beginQuery(oot->self) != 0, "OoT beginQuery refused");
        CLX_ASSERT(OoT_ComboLogic_TestClampActive() == 1, "the clamp is off INSIDE a round");
        const int l0 = OoT_ComboLogic_TestRoundProgressiveLevel(0);
        int levels[8];
        const int copies = top + 3;
        for (int i = 0; i < copies; ++i) {
            oot->assumeOwnItem(oot->self, (uint16_t)walletId);
            levels[i] = OoT_ComboLogic_TestRoundProgressiveLevel(0);
        }
        printf("[TEST] combo-logic-multiplicity: O3 round: wallet level %d before, then", l0);
        for (int i = 0; i < copies; ++i) {
            printf(" %d", levels[i]);
        }
        (void)oot->expand(oot->self);
        const int afterExpand = OoT_ComboLogic_TestRoundProgressiveLevel(0);
        printf("; %d after expand (re-derived, starting items applied, the world's own wallets harvested)\n",
               afterExpand);
        oot->endQuery(oot->self);
        CLX_ASSERT(OoT_ComboLogic_TestClampActive() == 0, "endQuery left the clamp on");

        // COUNTING: some pair of consecutive copies below the top must differ by
        // one tier. Under the ABI-2 de-dup every copy after the first was dropped
        // and the level never moved again.
        int increments = 0;
        int prev = l0;
        for (int i = 0; i < copies; ++i) {
            CLX_ASSERT(levels[i] >= prev, "a copy LOWERED the round's wallet");
            if (levels[i] == prev + 1) {
                increments++;
            }
            prev = levels[i];
        }
        CLX_ASSERT(increments >= 2,
                   "fewer than two wallet copies advanced the tier — the round is still de-duplicating by id");
        CLX_ASSERT(levels[copies - 1] == top, "copies past the top did not rest exactly at the top tier");
        CLX_ASSERT(afterExpand == top,
                   "after expand the wallet is not at the top tier — the re-derivation, the starting inventory or the "
                   "harvest of the world's own placed wallets bypassed the clamp");
    }
    CLX_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0, "the OoT legs moved a placement");

    // ==================================================================
    // MM: the shipped profile, as the creation seam leaves it.
    // ==================================================================
    MM_ComboLogic_ResetCounters();
    (void)MM_ComboLogic_ApplyShippedProfile();
    std::unique_ptr<unsigned char[]> saveAfterProfile(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);

    // ------------------------------------------------------------------
    // M1. Counters count, and stop at their maximum.
    // ------------------------------------------------------------------
    const int mmKinds = MM_ComboLogic_TestCounterKindCount();
    CLX_ASSERT(mmKinds == 4, "the MM counter kind table changed shape");
    int clampsExpected = 0;
    for (int kind = 0; kind < mmKinds; ++kind) {
        const int id = MM_ComboLogic_TestCounterItemId(kind);
        const int max = MM_ComboLogic_TestCounterMax(kind);
        CLX_ASSERT(id > 0 && max > 0, "a counter kind with no id or no maximum — MM's static table did not come up");
        CLX_ASSERT(mm->snapshot(mm->self) != 0, "MM snapshot refused");
        CLX_ASSERT(mm->beginQuery(mm->self) != 0, "MM beginQuery refused");
        MM_ComboLogic_TestZeroCounter(kind);
        CLX_ASSERT(MM_ComboLogic_TestCounterValue(kind) == 0, "the counter did not zero");
        const int copies = max + 2;
        bool ok = true;
        for (int k = 1; k <= copies; ++k) {
            mm->assumeOwnItem(mm->self, (uint16_t)id);
            const int v = MM_ComboLogic_TestCounterValue(kind);
            const int expect = (k < max) ? k : max;
            if (v != expect) {
                printf("[TEST]   %s: after copy %d read %d, expected %d\n", kMmKindName[kind], k, v, expect);
                ok = false;
            }
        }
        const int granted = MM_ComboLogic_RoundCopiesGranted();
        mm->restore(mm->self);
        mm->endQuery(mm->self);
        printf("[TEST] combo-logic-multiplicity: M1 %-22s max=%d (derived), %d copies -> %s\n", kMmKindName[kind],
               max, copies, ok ? "counted to the maximum and stopped" : "WRONG");
        CLX_ASSERT(ok, "an MM counter did not count one per copy up to its maximum and stop there");
        CLX_ASSERT(granted == copies, "the round did not record every copy it was given");
        clampsExpected += 2;
    }
    CLX_ASSERT(MM_ComboLogic_CounterClamps() == clampsExpected,
               "the counter clamp absorbed a different number of copies than the surplus the leg supplied");

    // ------------------------------------------------------------------
    // M2. The own-origin harvest is PER HOST.
    // ------------------------------------------------------------------
    {
        const int fairyKind = 2;
        const int fairyId = MM_ComboLogic_TestCounterItemId(fairyKind);
        uint16_t hosts[2] = { 0, 0 };
        CLX_ASSERT(mm->snapshot(mm->self) != 0 && mm->beginQuery(mm->self) != 0, "MM probe round refused");
        (void)mm->expand(mm->self);
        const int reachedTotal = mm->reachedEmptyHosts(mm->self, hosts, 2);
        mm->restore(mm->self);
        mm->endQuery(mm->self);
        CLX_ASSERT(reachedTotal >= 2, "MM reaches fewer than two hosts from the shipped profile");

        SharedItem fairy;
        fairy.originGame = (uint8_t)GAME_MM;
        fairy.flags = 0u;
        fairy.id = (uint16_t)fairyId;
        CLX_ASSERT(mm->place(mm->self, hosts[0], fairy) != 0 && mm->place(mm->self, hosts[1], fairy) != 0,
                   "MM refused to place the fairy on two hosts it reached");

        const int harvestsBefore = MM_ComboLogic_HarvestCount();
        CLX_ASSERT(mm->snapshot(mm->self) != 0 && mm->beginQuery(mm->self) != 0, "MM harvest round refused");
        MM_ComboLogic_TestZeroCounter(fairyKind);
        (void)mm->expand(mm->self);
        (void)mm->expand(mm->self); // a second expand must not harvest either host again
        const int fairies = MM_ComboLogic_TestCounterValue(fairyKind);
        const int harvests = MM_ComboLogic_HarvestCount() - harvestsBefore;
        mm->restore(mm->self);
        mm->endQuery(mm->self);
        mm->clearPlacements(mm->self);
        printf("[TEST] combo-logic-multiplicity: M2 one MM-origin fairy on each of hosts %u and %u -> %d harvests, "
               "%d fairies in the round\n",
               (unsigned)hosts[0], (unsigned)hosts[1], harvests, fairies);
        CLX_ASSERT(harvests == 2 && fairies == 2,
                   "two placed copies of one id on two reached hosts must harvest as TWO copies, once each, however "
                   "many expands the round runs");
    }

    // ==================================================================
    // Teardown, with the S4 ordering: compare BEFORE the outer restore.
    // ==================================================================
    Combo_Logic_ResetPlacements();
    CLX_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0, "MM still holds a placement");
    CLX_ASSERT(MM_ComboLogic_SnapshotLive() == 0, "MM's snapshot is still live");
    CLX_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0, "the row moved an OoT placement");
    CLX_ASSERT(memcmp(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
               "the MM legs did not return the unified save buffer to the profile-applied state — a copy leaked "
               "past a restore");
    memcpy(gSaveContext, saveBefore.get(), OOT_SAVE_CONTEXT_SIZE); // construction, not a claim

    printf("[TEST] PASS: every copy counted on both real engines; OoT's progressive rows stop at their top tier "
           "(and were observed wrapping without the clamp); MM's counters stop at their derived maxima; the harvest "
           "is per host\n");
    return TEST_PASS;
}
