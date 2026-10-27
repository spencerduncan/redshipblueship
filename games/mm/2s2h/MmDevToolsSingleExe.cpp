/**
 * @file MmDevToolsSingleExe.cpp
 * @brief MM's half of src/common/mm_dev_tools_bridge.h: Majora's Mask's developer tools as SoH's Dev Tools rows reach
 *        them (#1238, W6-DEV).
 *
 * See the bridge header for the defect and the contract. This TU is single-exe only: upstream 2Ship draws these rows in
 * its own menu (BenMenu.cpp, excluded here), which reads MM_gPlayState directly.
 */

#include "global.h"

#include <cstring>

#include "context.h"             // src/common: Context_GetCurrentGame
#include "mm_dev_tools_bridge.h" // src/common

extern "C" {
extern PlayState* MM_gPlayState;
}

namespace {

/** MM's play state while Majora's Mask is the running game, else NULL: a suspended MM leaves the pointer dangling. */
PlayState* RunningPlayState() {
    if (Context_GetCurrentGame() != GAME_MM) {
        return nullptr;
    }
    return MM_gPlayState;
}

PlayState* sStandInReplaced = nullptr;
bool sStandInActive = false;

} // namespace

extern "C" int32_t* MM_DevTools_FrameAdvanceEnabled(void) {
    PlayState* play = RunningPlayState();
    return play != nullptr ? &play->frameAdvCtx.enabled : nullptr;
}

extern "C" void MM_DevTools_SetStandInPlayState(int on) {
    // Heap, not a static: MM's PlayState is large, and a stand-in only ever lives for one test leg.
    static PlayState* sStandIn = nullptr;
    if (on != 0) {
        if (sStandIn == nullptr) {
            sStandIn = new PlayState();
        }
        std::memset(static_cast<void*>(sStandIn), 0, sizeof(PlayState));
        if (!sStandInActive) {
            sStandInReplaced = MM_gPlayState;
            sStandInActive = true;
        }
        MM_gPlayState = sStandIn;
    } else if (sStandInActive) {
        MM_gPlayState = sStandInReplaced;
        sStandInReplaced = nullptr;
        sStandInActive = false;
    }
}
