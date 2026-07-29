/**
 * @file test_shared_resources.c
 * @brief ROM-free locks for shared cross-game resources (#525).
 *
 * A shared resource is ONE quantity spanning both games — capacity AND current
 * value — as opposed to a shared ITEM's one-way single-use crossing. What is
 * locked here is the arithmetic that makes that safe:
 *
 *  (1) THE WATERMARK, and the 500-vs-999 round trip it exists for. MM's tier-3
 *      wallet holds 500 and OoT's holds 999. Under a naive `shared = live`
 *      harvest, carrying 800 rupees into MM clamps the live count to 500 and
 *      the next suspend writes that 500 back over the pool — 300 rupees gone,
 *      silently, on EVERY round trip. This is the regression this file exists
 *      to make impossible.
 *
 *  (2) THE TWO DISCIPLINES ARE DIFFERENT. Monotonic resources max-merge in both
 *      directions and cannot decay; consumables delta-harvest and MUST decay
 *      when the player spends. Applying one discipline to the other's resource
 *      is a correctness bug either way round — a decaying wallet tier, or a
 *      rupee count that can only ever grow.
 *
 *  (3) THE FIRST-HARVEST SEED, which is where a `.redsave` load would otherwise
 *      DOUBLE the player's money. The watermark is RAM-only, so a freshly
 *      loaded session has none; harvesting a delta against zero would
 *      contribute the whole restored balance a second time. The empty/occupied
 *      split is what tells "a cold boot genuinely earned this" apart from "the
 *      pool already counted this".
 *
 *  (4) ZERO IS A LEGAL VALUE, and is distinguishable from unset. This is the
 *      whole reason the slots are kind-tagged rather than bare u16s: the
 *      .redsave growth contract says zero means unset, and a broke player has
 *      0 rupees.
 *
 *  (5) THE HEART QUANTITY round-trips through its split, and clamps at 20
 *      hearts. Neither game's give path clamps capacity.
 *
 * Display-free, ROM-free and save-free: everything under test is gComboCtx plus
 * a RAM watermark table, so this runs in the plain `redship` tier.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++,
 * like test_grant_sources.c); every symbol it drives is C-linkage via
 * shared_resources.h.
 */

#include "../context.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>

#define SR_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s (line %d)\n", #cond, __LINE__);                                                    \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

// Both games' real tier-3 wallet capacities. Named rather than inlined because
// the DIFFERENCE between them is the subject of half this file.
#define SR_OOT_WALLET_CAP_T3 999u
#define SR_MM_WALLET_CAP_T3 500u

// Start from a world with no shared state at all: an empty pool AND no live
// watermarks. This is a cold boot, and it is deliberately the same pair of calls
// src/common/context.cpp makes when it invalidates a session.
static void SR_FreshWorld(void) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
}

// Read a slot's value, or a sentinel that no test expects, so a "resource is
// unset" bug shows up as a wrong number rather than a stale local.
static uint16_t SR_Pool(uint8_t kind) {
    uint16_t value = 0xBEEF;
    if (!Combo_GetSharedResource(kind, &value)) {
        return 0xBEEF;
    }
    return value;
}

TestResult Test_SharedResources(void) {
    printf("[TEST] shared-resources: one quantity across both games — watermark, disciplines, seed, clamps (#525)\n");

    // ------------------------------------------------------------------
    // (4) Unset is not zero. A fresh world has NO shared rupees, which is a
    // different fact from "the player has 0 rupees" — and the .redsave growth
    // contract makes a zero-extended legacy record read exactly like a fresh
    // world, so these two must stay distinguishable.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    uint16_t probe = 0;
    SR_ASSERT(!Combo_GetSharedResource(RSBS_SHARED_RES_RUPEES, &probe));
    SR_ASSERT(Combo_CountSharedResources() == 0);

    // Harvesting a genuine zero from a world that never shared anything must
    // NOT manufacture a slot: nothing has been shared, and saying otherwise
    // would make a fresh world indistinguishable from a played one. This
    // matters because both games harvest their WHOLE resource set at every
    // suspend, so a player with no wallet upgrade and no double defense yet
    // pushes zeros through here constantly.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 0);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, 0);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE, 0);
    SR_ASSERT(!Combo_GetSharedResource(RSBS_SHARED_RES_RUPEES, &probe));
    SR_ASSERT(Combo_CountSharedResources() == 0);

    // ------------------------------------------------------------------
    // (1) THE ROUND TRIP. OoT earns 800 with a tier-3 wallet, crosses to MM
    // (which cannot hold more than 500), sits there spending nothing, and comes
    // home. The player must return with all 800.
    // ------------------------------------------------------------------
    SR_FreshWorld();

    // OoT suspends holding 800. Empty slot + no watermark => the whole balance
    // is genuinely new money and enters the pool.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 800);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 800);

    // Arrive in MM. Only 500 fits.
    uint16_t mmLive = 0;
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, SR_MM_WALLET_CAP_T3, &mmLive));
    SR_ASSERT(mmLive == 500);
    // The 300 that did not fit stays in the pool rather than being lost at the
    // clamp — the pool holds the true total, the game shows what it can.
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 800);

    // MM suspends having spent nothing. THIS is the line a naive
    // `shared = live` harvest gets wrong: it would write 500 over the 800.
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 500);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 800);

    // Home again, with the full balance.
    uint16_t ootLive = 0;
    SR_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, SR_OOT_WALLET_CAP_T3, &ootLive));
    SR_ASSERT(ootLive == 800);

    // And it is stable under repetition — a second round trip must not shave
    // another 300 off, which is how the naive bug actually presents.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 800);
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, SR_MM_WALLET_CAP_T3, &mmLive));
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 500);
    SR_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, SR_OOT_WALLET_CAP_T3, &ootLive));
    SR_ASSERT(ootLive == 800);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 800);

    // ------------------------------------------------------------------
    // (2a) A consumable MUST decay when the player actually spends. The
    // watermark protects the clamp overflow, not the player's spending.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 800);
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, SR_MM_WALLET_CAP_T3, &mmLive));
    SR_ASSERT(mmLive == 500);
    // Spend 200 in Termina, then suspend.
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, 300);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 600);
    SR_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, SR_OOT_WALLET_CAP_T3, &ootLive));
    SR_ASSERT(ootLive == 600);

    // Spending down to nothing leaves an OCCUPIED slot holding zero — the
    // "legitimately broke" state, which the kind tag is what makes expressible.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 0);
    SR_ASSERT(Combo_GetSharedResource(RSBS_SHARED_RES_RUPEES, &probe));
    SR_ASSERT(probe == 0);

    // ------------------------------------------------------------------
    // (2b) A monotonic resource cannot decay, in either direction.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, 3);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_WALLET_TIER) == 3);
    // MM harvesting a LOWER tier must not downgrade the shared wallet.
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, 1);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_WALLET_TIER) == 3);

    // Apply raises the arriving game and never lowers it.
    uint16_t tier = 1;
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, 3, &tier));
    SR_ASSERT(tier == 3);
    uint16_t higher = 3;
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, 2);
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, 3, &higher));
    SR_ASSERT(higher == 3);

    // Double defense is the same discipline as a 0/1 flag, and once earned in
    // either game it is earned in both.
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_DOUBLE_DEFENSE, 1);
    uint16_t dd = 0;
    SR_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE, 1, &dd));
    SR_ASSERT(dd == 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE, 0);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_DOUBLE_DEFENSE) == 1);

    // ------------------------------------------------------------------
    // (3) THE FIRST-HARVEST SEED — the .redsave double-count guard.
    // ------------------------------------------------------------------
    // Case (a): cold boot, nothing ever shared. The player earned 200 from
    // scratch, so all 200 must enter the pool.
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 200);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 200);

    // Case (b): a .redsave load. The pool comes back holding 300 and OoT's own
    // save comes back holding the same 300 — one balance, stored twice. Only
    // the watermarks are dropped, exactly as rsbs::SaveManager::Load does.
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 300);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 300);
    Combo_ResetSharedResourceWatermarks(); // <- the load

    // The first harvest after that load must contribute NOTHING. Getting this
    // wrong doubles the player's money on the first switch after every load.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 300);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 300);

    // ...and money earned AFTER the load still counts. The seed must set a
    // baseline, not disable harvesting.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 450);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 450);

    // ------------------------------------------------------------------
    // Applying a resource nobody has ever shared reports "nothing to apply" and
    // leaves the game's own value alone — and must NOT record a watermark, so
    // that the balance the game already had still joins the pool at its next
    // harvest. Seeding happens in one place (the harvest rule) so that which
    // side moved first cannot change the answer.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    uint16_t untouched = 137;
    SR_ASSERT(!Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, SR_OOT_WALLET_CAP_T3, &untouched));
    SR_ASSERT(untouched == 137);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 137);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 137);

    // ------------------------------------------------------------------
    // (5) The canonical heart quantity: capacity + 4 per un-converted piece,
    // round-tripping through its own split, clamped at 20 hearts.
    // ------------------------------------------------------------------
    uint16_t capacity = 0;
    uint16_t pieces = 0;

    // Three hearts, no pieces.
    SR_ASSERT(Combo_MakeHealthQuarters(0x30, 0) == 0x30);
    Combo_SplitHealthQuarters(0x30, &capacity, &pieces);
    SR_ASSERT(capacity == 0x30 && pieces == 0);

    // Three hearts and three pieces — the state OoT can sit in and MM cannot.
    // It survives the round trip intact rather than being normalized away.
    SR_ASSERT(Combo_MakeHealthQuarters(0x30, 3) == 0x3C);
    Combo_SplitHealthQuarters(0x3C, &capacity, &pieces);
    SR_ASSERT(capacity == 0x30 && pieces == 3);

    // The FOURTH piece is a whole heart. This is the conversion the two games
    // disagree about the timing of; as one number it is just arithmetic, and
    // the split hands MM a state it can express.
    SR_ASSERT(Combo_MakeHealthQuarters(0x30, 4) == 0x40);
    Combo_SplitHealthQuarters(0x40, &capacity, &pieces);
    SR_ASSERT(capacity == 0x40 && pieces == 0);

    // The 20-heart clamp, from both directions. Neither game's give path
    // clamps capacity, and the life meter past 20 hearts is untested in both.
    SR_ASSERT(Combo_MakeHealthQuarters(0x140, 0) == RSBS_SHARED_RES_MAX_HEALTH_QUARTERS);
    SR_ASSERT(Combo_MakeHealthQuarters(0x140, 3) == RSBS_SHARED_RES_MAX_HEALTH_QUARTERS);
    SR_ASSERT(Combo_MakeHealthQuarters(0x200, 0) == RSBS_SHARED_RES_MAX_HEALTH_QUARTERS);
    Combo_SplitHealthQuarters(0x400, &capacity, &pieces);
    SR_ASSERT(capacity == RSBS_SHARED_RES_MAX_HEALTH_QUARTERS && pieces == 0);

    // Health capacity merges monotonically and clamps on the way out too, so an
    // over-20 pool can never reach a live save.
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, 0x140);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS, 0x30);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_HEALTH_QUARTERS) == 0x140);
    uint16_t quarters = 0x30;
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS,
                                        (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS, &quarters));
    SR_ASSERT(quarters == RSBS_SHARED_RES_MAX_HEALTH_QUARTERS);

    // ------------------------------------------------------------------
    // Current health is a CONSUMABLE, not a capacity: one bar across both
    // games, per OoTMM. Damage taken in MM must still be there in OoT.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT, 0x50); // 5 hearts
    uint16_t health = 0x30;
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_CURRENT, 0x100, &health));
    SR_ASSERT(health == 0x50);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_CURRENT, 0x20); // took 3 hearts of damage
    SR_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT, 0x100, &health));
    SR_ASSERT(health == 0x20);

    // ------------------------------------------------------------------
    // Magic (#525's optional tier): the meter level is a MONOTONIC capacity
    // and current magic a CONSUMABLE — the wallet/rupee split, one resource
    // over. Units are shared verbatim (0x30 per bar in both games), so the
    // games' caps differ only by LEVEL: a single-magic game can display at
    // most 0x30 of a double-magic pool.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    // OoT earned double magic; MM holds single. The level never downgrades.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, 2);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_LEVEL, 1);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_LEVEL) == 2);
    uint16_t magicLevel = 1;
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_LEVEL, 2, &magicLevel));
    SR_ASSERT(magicLevel == 2);
    // Discipline pin: a LOWER harvest AFTER an apply is the one sequence the
    // two disciplines answer differently — misclassified as consumable this
    // computes delta -1 against the apply's watermark and decays the pool to
    // 1; monotonic max-merge holds 2. (The harvests above pass either way:
    // before any apply, the consumable first-harvest seed makes them look
    // monotonic. Verified by mutation — dropping MAGIC_LEVEL from
    // IsMonotonicKind fails exactly here and nowhere else.)
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_LEVEL, 1);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_LEVEL) == 2);

    // A full double meter (0x60) visits a game that displays a single bar
    // (cap 0x30): the pool keeps the true amount, the arriving game shows what
    // fits, and nothing is lost on the way home — the 500/999 wallet
    // invariant, in magic units.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, 0x60);
    uint16_t magic = 0;
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_CURRENT, 0x30, &magic));
    SR_ASSERT(magic == 0x30);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x60);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_CURRENT, 0x30); // spent nothing in Termina
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x60);
    SR_ASSERT(Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, 0x60, &magic));
    SR_ASSERT(magic == 0x60);

    // Spending decays the shared meter: a 0x20 spell cast in Hyrule is gone
    // from the one meter MM reads too.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, 0x40);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x40);
    SR_ASSERT(Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_MAGIC_CURRENT, 0x30, &magic));
    SR_ASSERT(magic == 0x30);

    // ------------------------------------------------------------------
    // Garbage in: a bogus game or kind must change nothing.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_NONE, RSBS_SHARED_RES_RUPEES, 500);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_NONE, 500);
    Combo_HarvestSharedResource(GAME_OOT, (uint8_t)RSBS_SHARED_RES_KIND_COUNT, 500);
    SR_ASSERT(Combo_CountSharedResources() == 0);
    uint16_t sink = 42;
    SR_ASSERT(!Combo_ApplySharedResource(GAME_NONE, RSBS_SHARED_RES_RUPEES, 999, &sink));
    SR_ASSERT(!Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 999, NULL));
    SR_ASSERT(sink == 42);

    // ------------------------------------------------------------------
    // All seven shared resources (v1 + magic) coexist: the slot array holds
    // the whole set at once, with no kind overwriting another's slot.
    // ------------------------------------------------------------------
    SR_FreshWorld();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 250);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, 0x60);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT, 0x40);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, 0x30);
    SR_ASSERT(Combo_CountSharedResources() == 7);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 250);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_WALLET_TIER) == 2);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_HEALTH_QUARTERS) == 0x60);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_HEALTH_CURRENT) == 0x40);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_DOUBLE_DEFENSE) == 1);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_LEVEL) == 1);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_MAGIC_CURRENT) == 0x30);
    // The set fits with exactly one slot to spare. The ammo upgrades — the
    // queued remainder of the class — do NOT fit there; they arrive with the
    // second reserved[] block carve (see RSBS_SHARED_RESOURCE_CAP's comment in
    // context.h), not by squeezing into this array.
    SR_ASSERT(Combo_CountSharedResources() < (int)RSBS_SHARED_RESOURCE_CAP);

    // Session invalidation retires the pool AND the watermarks together — a
    // watermark surviving into a fresh world would measure a delta against a
    // balance that no longer exists.
    SR_FreshWorld();
    SR_ASSERT(Combo_CountSharedResources() == 0);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 700);
    SR_ASSERT(SR_Pool(RSBS_SHARED_RES_RUPEES) == 700);

    printf("[TEST] PASS: watermark survives the 500/999 round trip, disciplines split, load cannot double-count\n");
    return TEST_PASS;
}
