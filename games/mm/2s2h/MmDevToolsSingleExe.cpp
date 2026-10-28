/**
 * @file MmDevToolsSingleExe.cpp
 * @brief MM's half of src/common/mm_dev_tools_bridge.h: Majora's Mask's developer tools as SoH's Dev Tools rows reach
 *        them (#1238, W6-DEV).
 *
 * See the bridge header for the defect and the contract. This TU is single-exe only: upstream 2Ship draws these rows in
 * its own menu (BenMenu.cpp, excluded here), which reads MM_gPlayState directly.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <unordered_map>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include "2s2h/DeveloperTools/DeveloperTools.h" // WARP_POINT_CVAR

#include "context.h"             // src/common: Context_GetCurrentGame
#include "mm_dev_tools_bridge.h" // src/common

extern "C" {
extern PlayState* MM_gPlayState;
}

// DeveloperTools/WarpPoint.cpp (linked through games/mm/CMakeLists.txt's ship__Enh_registrars list).
extern std::unordered_map<s16, const char*> warpPointSceneList;
void Warp();

namespace {

/** MM's play state while Majora's Mask is the running game, else NULL: a suspended MM leaves the pointer dangling. */
PlayState* RunningPlayState() {
    if (Context_GetCurrentGame() != GAME_MM) {
        return nullptr;
    }
    return MM_gPlayState;
}

/** Link, while MM is in gameplay with him spawned. */
Player* RunningPlayer() {
    PlayState* play = RunningPlayState();
    return play != nullptr ? GET_PLAYER(play) : nullptr;
}

void SaveConsoleVariablesNextFrame() {
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr && ctx->GetWindow() != nullptr && ctx->GetWindow()->GetGui() != nullptr) {
        ctx->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
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

// ---- The warp point: 2Ship's RenderWarpPointSection (WarpPoint.cpp), its widgets replaced by SoH's rows ----------

extern "C" int MM_DevTools_WarpPointAvailable(void) {
    return RunningPlayer() != nullptr ? 1 : 0;
}

extern "C" int MM_DevTools_SetWarpPoint(void) {
    PlayState* play = RunningPlayState();
    Player* player = RunningPlayer();
    if (play == nullptr || player == nullptr) {
        return 0;
    }
    CVarSetInteger(WARP_POINT_CVAR "Entrance", gSaveContext.save.entrance);
    CVarSetInteger(WARP_POINT_CVAR "Room", play->roomCtx.curRoom.num);
    CVarSetFloat(WARP_POINT_CVAR "X", player->actor.world.pos.x);
    CVarSetFloat(WARP_POINT_CVAR "Y", player->actor.world.pos.y);
    CVarSetFloat(WARP_POINT_CVAR "Z", player->actor.world.pos.z);
    CVarSetFloat(WARP_POINT_CVAR "Rotation", player->actor.shape.rot.y);
    CVarSetInteger(WARP_POINT_CVAR "Saved", 1);
    SaveConsoleVariablesNextFrame();
    return 1;
}

extern "C" int MM_DevTools_WarpPointSaved(void) {
    return CVarGetInteger(WARP_POINT_CVAR "Saved", 0) != 0 ? 1 : 0;
}

extern "C" int MM_DevTools_DescribeWarpPoint(char* out, size_t size) {
    if (out == nullptr || size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!MM_DevTools_WarpPointSaved()) {
        return 0;
    }
    const s16 sceneId =
        (s16)Entrance_GetSceneIdAbsolute(CVarGetInteger(WARP_POINT_CVAR "Entrance", ENTRANCE(SOUTH_CLOCK_TOWN, 0)));
    const auto it = warpPointSceneList.find(sceneId);
    const char* scene = it != warpPointSceneList.end() && it->second != nullptr ? it->second : "Unknown Scene";
    std::snprintf(out, size, "%s, room %d", scene, CVarGetInteger(WARP_POINT_CVAR "Room", 0));
    return 1;
}

extern "C" int MM_DevTools_WarpToWarpPoint(void) {
    // Never Warp()'s boot leg (MM_gPlayState NULL): that authors a new save. Gameplay, with a point saved, only.
    if (!MM_DevTools_WarpPointSaved() || RunningPlayer() == nullptr) {
        return 0;
    }
    Warp();
    return 1;
}

extern "C" void MM_DevTools_ClearWarpPoint(void) {
    CVarClear(WARP_POINT_CVAR "Entrance");
    CVarClear(WARP_POINT_CVAR "Room");
    CVarClear(WARP_POINT_CVAR "X");
    CVarClear(WARP_POINT_CVAR "Y");
    CVarClear(WARP_POINT_CVAR "Z");
    CVarClear(WARP_POINT_CVAR "Rotation");
    CVarClear(WARP_POINT_CVAR "Saved");
    SaveConsoleVariablesNextFrame();
}
