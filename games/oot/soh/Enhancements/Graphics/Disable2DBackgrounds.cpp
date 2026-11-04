#include <libultraship/bridge.h>
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/resource/type/scenecommand/SetTimeSettings.h"

extern "C" {
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
#include "macros.h"
#include "variables.h"
}

bool OoT_Scene_CommandTimeSettings(PlayState* play, SOH::ISceneCommand* cmd);

#define CVAR_NAME CVAR_ENHANCEMENT("3DSceneRender")
#define CVAR_VALUE CVarGetInteger(CVAR_NAME, 0)

std::set<SceneID> fogControlList = {
    SCENE_MARKET_ENTRANCE_DAY,
    SCENE_MARKET_ENTRANCE_NIGHT,
    SCENE_MARKET_ENTRANCE_RUINS,
    SCENE_BACK_ALLEY_DAY,
    SCENE_BACK_ALLEY_NIGHT,
    SCENE_MARKET_DAY,
    SCENE_MARKET_NIGHT,
    SCENE_MARKET_RUINS,
    SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY,
    SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT,
    SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS,
};

std::set<SceneID> skyboxSceneControlList = {
    SCENE_MARKET_ENTRANCE_DAY,
    SCENE_MARKET_ENTRANCE_NIGHT,
    SCENE_MARKET_ENTRANCE_RUINS,
    SCENE_BACK_ALLEY_DAY,
    SCENE_BACK_ALLEY_NIGHT,
    SCENE_MARKET_DAY,
    SCENE_MARKET_NIGHT,
    SCENE_MARKET_RUINS,
    SCENE_CASTLE_COURTYARD_ZELDA,
    SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY,
    SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT,
    SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS,
    SCENE_FOREST_TEMPLE,
};

std::set<SkyboxId> skyboxIdControlList = {
    SKYBOX_BAZAAR,
    SKYBOX_HOUSE_LINK,
    SKYBOX_MARKET_ADULT,
    SKYBOX_MARKET_CHILD_DAY,
    SKYBOX_MARKET_CHILD_NIGHT,
    SKYBOX_HAPPY_MASK_SHOP,
    SKYBOX_HOUSE_KNOW_IT_ALL_BROTHERS,
    SKYBOX_HOUSE_OF_TWINS,
    SKYBOX_STABLES,
    SKYBOX_HOUSE_KAKARIKO,
    SKYBOX_KOKIRI_SHOP,
    SKYBOX_GORON_SHOP,
    SKYBOX_ZORA_SHOP,
    SKYBOX_POTION_SHOP_KAKARIKO,
    SKYBOX_POTION_SHOP_MARKET,
    SKYBOX_HOUSE_RICHARD,
    SKYBOX_HOUSE_IMPA,
    SKYBOX_TENT,
    SKYBOX_HOUSE_MIDO,
    SKYBOX_HOUSE_SARIA,
    SKYBOX_HOUSE_ALLEY,
};

void SyncSkyboxTimeToCurrentTime() {
    SOH::SetTimeSettings cmd;
    cmd.settings = { 0xFF, 0xFF, static_cast<uint8_t>(OoT_gPlayState->envCtx.timeIncrement) };
    OoT_Scene_CommandTimeSettings(OoT_gPlayState, &cmd);
}

void Register3DPreRenderedScenes() {
    COND_HOOK(AfterSceneCommands, CVAR_VALUE, [](int16_t sceneNum) {
        if (!skyboxSceneControlList.contains(static_cast<SceneID>(sceneNum)) &&
            // The interior scenes that use the fake skybox that is in skyboxIdControlList never have a real skybox to
            // show because we disable the 2D background, so we might as well put the normal sky there anyway just in
            // case any void space manages to peek through the geometry. And by replacing the skybox ID instead of
            // preventing the load of the textures when they're used we prevent potential bad access later.
            !skyboxIdControlList.contains(static_cast<SkyboxId>(OoT_gPlayState->skyboxId))) {
            return;
        }

        // Add a skybox on scenes from skyboxSceneControlList
        OoT_gPlayState->envCtx.skyboxDisabled = false;
        SyncSkyboxTimeToCurrentTime();

        // Replace skybox with normal sky
        OoT_gPlayState->skyboxId = SKYBOX_NORMAL_SKY;
        // Apply the always cloudy skybox as an adult for Temple of Time and the Market
        if (sceneNum == SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS || sceneNum == SCENE_MARKET_RUINS ||
            sceneNum == SCENE_MARKET_ENTRANCE_RUINS) {
            OoT_gWeatherMode = 3;
            gSaveContext.retainWeatherMode = 1;
        }
    });

    COND_HOOK(OnPlayDrawBegin, CVAR_VALUE, []() {
        if (!fogControlList.contains(static_cast<SceneID>(OoT_gPlayState->sceneNum))) {
            return;
        }

        if ((HREG(80) != 10) || (HREG(82) != 0)) {
            // Furthest possible fog and zFar
            OoT_gPlayState->view.zFar = 12800;
            OoT_gPlayState->lightCtx.fogNear = 1000; // Set to 1000 to complete disable fog entirely
            OoT_gPlayState->lightCtx.fogFar = 12800;
        }
    });

    COND_VB_SHOULD(VB_DRAW_2D_BACKGROUND, CVAR_VALUE, { *should = false; });
}

static RegisterShipInitFunc initFunc(Register3DPreRenderedScenes, { CVAR_NAME });
