/**
 * @file oot_shared_ocarina_buttons_test.cpp
 * OoT's half of the Shared Ocarina Buttons locks (#1067; the rule is
 * RSBS_XITEM_SHARED_OCARINA_BUTTONS in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-ocarina-buttons-pairs` and
 * `shared-ocarina-buttons-arrival` (src/common/tests/test_shared_ocarina_buttons.c)
 * drive OoT's REAL OoT_HarvestSharedResources / OoT_ApplySharedResources
 * through. They only put OoT's live save and its randomizer context in a known
 * state (after OoT_SharedSwordTest_ArmLive) and read the button flags back;
 * every merge decision is the production shims' and the family members
 * OoT_RegisterSharedOcarinaButtonFamilies registers (GameExports_SingleExe.cpp).
 * They live here because only an OoT TU has OoT's `gSaveContext`, its
 * RAND_INF_* flags and its Rando::Context in scope.
 *
 * Buttons are numbered in OoT's own order, the row's numbering for both games:
 * 0 A, 1 C-Up, 2 C-Down, 3 C-Left, 4 C-Right (RG_OCARINA_*_BUTTON and
 * RAND_INF_HAS_OCARINA_* both run in that order).
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

namespace {

constexpr int kButtonCount = 5;

/** The randomizer context the rows read the button shuffle from. Rando::Context
 *  keeps its instance as a weak pointer, so a ROM-free row must hold one for
 *  as long as it reads it. */
std::shared_ptr<Rando::Context> sHeldContext;
bool sCreatedContext = false;
uint8_t sPriorShuffle = 0;

RandomizerInf ButtonFlag(int button) {
    return (RandomizerInf)(RAND_INF_HAS_OCARINA_A + button);
}

} // namespace

/** Hold OoT's randomizer context for the row (creating one in a process that
 *  never generated) and remember its button-shuffle option. */
extern "C" void OoT_SharedButtonsTest_Begin(void) {
    sHeldContext = Rando::Context::GetInstance();
    sCreatedContext = false;
    if (sHeldContext == nullptr) {
        sHeldContext = Rando::Context::CreateInstance();
        sCreatedContext = true;
    }
    sPriorShuffle = sHeldContext->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).Get();
}

/** Put the option back and let go of the context Begin held. */
extern "C" void OoT_SharedButtonsTest_End(void) {
    if (sHeldContext == nullptr) {
        return;
    }
    sHeldContext->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).Set(sPriorShuffle);
    if (sCreatedContext) {
        sHeldContext->GetLogic()->SetContext(nullptr);
    }
    sHeldContext.reset();
    sCreatedContext = false;
}

/** OoT's live save as a randomizer file with the button shuffle @p shuffle (0
 *  off, 1 on) and exactly the buttons in @p heldMask (bit i = button i) owned,
 *  as RAND_INF_HAS_OCARINA_* records them (savefile.cpp sets all five at
 *  creation when the shuffle is off). Call after OoT_SharedSwordTest_ArmLive. */
extern "C" void OoT_SharedButtonsTest_Set(int shuffle, unsigned heldMask) {
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    if (sHeldContext != nullptr) {
        sHeldContext->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).Set((uint8_t)(shuffle != 0 ? 1 : 0));
    }
    for (int i = 0; i < kButtonCount; i++) {
        const int flag = (int)ButtonFlag(i);
        const u16 bit = (u16)(1u << (flag & 0xF));
        if ((heldMask & (1u << i)) != 0u) {
            gSaveContext.ship.randomizerInf[flag >> 4] |= bit;
        } else {
            gSaveContext.ship.randomizerInf[flag >> 4] &= (u16)~bit;
        }
    }
}

/** The buttons OoT's live save owns, as a mask (bit i = button i). */
extern "C" unsigned OoT_SharedButtonsTest_Held(void) {
    unsigned mask = 0;
    for (int i = 0; i < kButtonCount; i++) {
        const int flag = (int)ButtonFlag(i);
        if ((gSaveContext.ship.randomizerInf[flag >> 4] & (1u << (flag & 0xF))) != 0u) {
            mask |= 1u << i;
        }
    }
    return mask;
}

/** OoT's pool id for button @p button (RG_OCARINA_A_BUTTON + button). */
extern "C" int OoT_SharedButtonsTest_PoolId(int button) {
    return (int)RG_OCARINA_A_BUTTON + button;
}
