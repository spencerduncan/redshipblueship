/**
 * @file mm_shared_swim_test.cpp
 * MM's half of the Shared Swim locks (#1369; the rule is RSBS_XITEM_SHARED_SWIM in
 * src/common/foreign_items.h, and the coupled-buttons marker
 * RSBS_COMBO_FLAG_COUPLED_OCARINA_BUTTONS).
 *
 * THE SHIM HELPERS the redship rows `shared-swim-pairs` and `shared-swim-arrival`
 * (src/common/tests/test_shared_swim.c) drive MM's REAL MM_HarvestSharedResources
 * / MM_ApplySharedResources, the Shared Swim member
 * (MM_RegisterSharedSwimFamily, GameExports_SingleExe.cpp) and the pair's one
 * shuffle (Rando::Foreign::ShuffleSwimForWorld / ShuffleOcarinaButtonsForWorld,
 * Rando::GetComputedStartingItems) through. They only put MM's live save in a
 * known state (after MM_SharedSwordTest_ArmLive), or hand those predicates an
 * option array of their own, and read the answers back. They live here because
 * only an MM TU has MM's `gSaveContext`, its RI_* ids and its RANDO_INF_* flags in
 * scope.
 */

#include "global.h"

#include <cstring>
#include <memory>
#include <vector>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared swim is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedSwimTest_Set(int shuffle, int held, int vanilla) {
    (void)shuffle;
    (void)held;
    (void)vanilla;
}
extern "C" int MM_SharedSwimTest_Held(void) {
    return 0;
}
extern "C" int MM_SharedSwimTest_PoolId(void) {
    return -1;
}
extern "C" int MM_SharedSwimTest_WorldShuffles(int ownSwim, int ownButtons, int vanilla) {
    (void)ownSwim;
    (void)ownButtons;
    (void)vanilla;
    return 0;
}
extern "C" int MM_SharedSwimTest_ComputedStart(int ownSwim, int ownButtons, int vanilla) {
    (void)ownSwim;
    (void)ownButtons;
    (void)vanilla;
    return 0;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Foreign.h"

namespace {

/** A frozen option array with MM's own swim and button shuffles and its logic as
 *  given, every other option at 0 (Glitchless logic). */
std::vector<uint32_t> MMSwimOptions(int ownSwim, int ownButtons, int vanilla) {
    std::vector<uint32_t> options(RO_MAX, 0);
    options[RO_SHUFFLE_SWIM] = ownSwim != 0 ? RO_GENERIC_YES : RO_GENERIC_NO;
    options[RO_SHUFFLE_OCARINA_BUTTONS] = ownButtons != 0 ? RO_GENERIC_YES : RO_GENERIC_NO;
    options[RO_LOGIC] = vanilla != 0 ? RO_LOGIC_VANILLA : RO_LOGIC_GLITCHLESS;
    return options;
}

} // namespace

/** MM's live save as a randomizer file with its own swim shuffle @p shuffle (0
 *  off, 1 on), the swim ability (RANDO_INF_OBTAINED_SWIM) held when @p held, and
 *  Vanilla logic when @p vanilla (Glitchless otherwise). Call after
 *  MM_SharedSwordTest_ArmLive. */
extern "C" void MM_SharedSwimTest_Set(int shuffle, int held, int vanilla) {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.randoSaveOptions[RO_SHUFFLE_SWIM] =
        (shuffle != 0) ? RO_GENERIC_YES : RO_GENERIC_NO;
    gSaveContext.save.shipSaveInfo.rando.randoSaveOptions[RO_LOGIC] =
        (vanilla != 0) ? RO_LOGIC_VANILLA : RO_LOGIC_GLITCHLESS;
    const int flag = RANDO_INF_OBTAINED_SWIM;
    const u16 bit = (u16)(1u << (flag & 0xF));
    if (held != 0) {
        gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] |= bit;
    } else {
        gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] &= (u16)~bit;
    }
}

/** Does MM's live save hold the swim ability (the flag the respawn-on-water gate
 *  reads, Rando/ActorBehavior/Player.cpp)? */
extern "C" int MM_SharedSwimTest_Held(void) {
    const int flag = RANDO_INF_OBTAINED_SWIM;
    return (gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] & (1u << (flag & 0xF))) != 0u ? 1 : 0;
}

/** MM's pool id for the Shared Swim member. */
extern "C" int MM_SharedSwimTest_PoolId(void) {
    return (int)RI_ABILITY_SWIM;
}

/** The pair's one shuffle as MM's sites read it, over an option array with MM's
 *  own shuffles and logic as given and the CURRENT frozen combo record: bit 0
 *  Rando::Foreign::ShuffleSwimForWorld, bit 1 ShuffleOcarinaButtonsForWorld. */
extern "C" int MM_SharedSwimTest_WorldShuffles(int ownSwim, int ownButtons, int vanilla) {
    const std::vector<uint32_t> options = MMSwimOptions(ownSwim, ownButtons, vanilla);
    int bits = 0;
    bits |= Rando::Foreign::ShuffleSwimForWorld(options.data()) ? 1 : 0;
    bits |= Rando::Foreign::ShuffleOcarinaButtonsForWorld(options.data()) ? 2 : 0;
    return bits;
}

/** MM's computed start (Rando::GetComputedStartingItems) over a save with MM's own
 *  shuffles and logic as given, under the CURRENT frozen combo record: bit 0 the
 *  start holds RI_ABILITY_SWIM, bit 1 it holds all five ocarina buttons. */
extern "C" int MM_SharedSwimTest_ComputedStart(int ownSwim, int ownButtons, int vanilla) {
    auto info = std::make_unique<RandoSaveInfo>();
    std::memset(info.get(), 0, sizeof(RandoSaveInfo));
    const std::vector<uint32_t> options = MMSwimOptions(ownSwim, ownButtons, vanilla);
    for (int i = 0; i < (int)RO_MAX; i++) {
        info->randoSaveOptions[i] = options[(size_t)i];
    }
    const std::vector<RandoItemId> start = Rando::GetComputedStartingItems(*info);
    bool swim = false;
    int buttons = 0;
    for (RandoItemId item : start) {
        swim = swim || item == RI_ABILITY_SWIM;
        buttons += (item >= RI_OCARINA_BUTTON_A && item <= RI_OCARINA_BUTTON_C_UP) ? 1 : 0;
    }
    return (swim ? 1 : 0) | (buttons == 5 ? 2 : 0);
}

#endif
