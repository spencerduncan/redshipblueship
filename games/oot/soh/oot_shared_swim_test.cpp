/**
 * @file oot_shared_swim_test.cpp
 * OoT's half of the Shared Swim locks (#1369; the rule is RSBS_XITEM_SHARED_SWIM
 * in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-swim-pairs` and `shared-swim-arrival`
 * (src/common/tests/test_shared_swim.c) drive OoT's REAL OoT_HarvestSharedResources
 * / OoT_ApplySharedResources and the Shared Swim member
 * (OoT_RegisterSharedSwimFamily, GameExports_SingleExe.cpp) through. They only put
 * OoT's live save and its randomizer context in a known state (after
 * OoT_SharedSwordTest_ArmLive) and read the swim flag back; every merge decision
 * is the production shims' and the member's. They live here because only an OoT
 * TU has OoT's `gSaveContext`, its RAND_INF_* flags and its Rando::Context in
 * scope.
 */

#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"

extern "C" bool OoT_SharedSwimSplitsScale(void);

namespace {

/** The randomizer context the rows read the swim shuffle from. Rando::Context
 *  keeps its instance as a weak pointer, so a ROM-free row must hold one for as
 *  long as it reads it. */
std::shared_ptr<Rando::Context> sHeldContext;
bool sCreatedContext = false;
uint8_t sPriorShuffle = 0;

} // namespace

/** Hold OoT's randomizer context for the row (creating one in a process that
 *  never generated) and remember its swim-shuffle option. */
extern "C" void OoT_SharedSwimTest_Begin(void) {
    sHeldContext = Rando::Context::GetInstance();
    sCreatedContext = false;
    if (sHeldContext == nullptr) {
        sHeldContext = Rando::Context::CreateInstance();
        sCreatedContext = true;
    }
    sPriorShuffle = sHeldContext->GetOption(RSK_SHUFFLE_SWIM).Get();
}

/** Put the option back and let go of the context Begin held. */
extern "C" void OoT_SharedSwimTest_End(void) {
    if (sHeldContext == nullptr) {
        return;
    }
    sHeldContext->GetOption(RSK_SHUFFLE_SWIM).Set(sPriorShuffle);
    if (sCreatedContext) {
        sHeldContext->GetLogic()->SetContext(nullptr);
    }
    sHeldContext.reset();
    sCreatedContext = false;
}

/** OoT's live save as a randomizer file with the swim shuffle @p shuffle (0 off,
 *  1 on) and the swim ability (RAND_INF_CAN_SWIM) held when @p held. Call after
 *  OoT_SharedSwordTest_ArmLive. */
extern "C" void OoT_SharedSwimTest_Set(int shuffle, int held) {
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    if (sHeldContext != nullptr) {
        sHeldContext->GetOption(RSK_SHUFFLE_SWIM).Set((uint8_t)(shuffle != 0 ? 1 : 0));
    }
    const int flag = (int)RAND_INF_CAN_SWIM;
    const u16 bit = (u16)(1u << (flag & 0xF));
    if (held != 0) {
        gSaveContext.ship.randomizerInf[flag >> 4] |= bit;
    } else {
        gSaveContext.ship.randomizerInf[flag >> 4] &= (u16)~bit;
    }
}

/** Does OoT's live save hold the swim ability (the flag the void-on-water gate
 *  reads, hook_handlers.cpp)? */
extern "C" int OoT_SharedSwimTest_Held(void) {
    const int flag = (int)RAND_INF_CAN_SWIM;
    return (gSaveContext.ship.randomizerInf[flag >> 4] & (1u << (flag & 0xF))) != 0u ? 1 : 0;
}

/** OoT's pool id for the Shared Swim member: the standalone bronze tier. */
extern "C" int OoT_SharedSwimTest_PoolId(void) {
    return (int)RG_BRONZE_SCALE;
}

/** OoT's Progressive Scale id, which the split leaves at silver and gold. */
extern "C" int OoT_SharedSwimTest_ProgressiveScaleId(void) {
    return (int)RG_PROGRESSIVE_SCALE;
}

/** 1 when the bronze tier is its own pool row in this world (the frozen rule and
 *  OoT's swim shuffle, OoT_SharedSwimSplitsScale). */
extern "C" int OoT_SharedSwimTest_SplitsScale(void) {
    return OoT_SharedSwimSplitsScale() ? 1 : 0;
}
