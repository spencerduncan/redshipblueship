#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/resource/type/Scene.h"
#include "soh/resource/type/scenecommand/SceneCommand.h"
#include "soh/resource/type/scenecommand/SetCollisionHeader.h"
#include "spdlog/spdlog.h"

#define CVAR_GRAVE_HOLE_NAME CVAR_ENHANCEMENT("GraveHoles")
#define GRAVE_HOLES_DEFAULT 0
#define CVAR_GRAVE_HOLE_VALUE CVarGetInteger(CVAR_GRAVE_HOLE_NAME, GRAVE_HOLES_DEFAULT)
#define GRAVEYARD_SCENE_FILEPATH "scenes/shared/spot02_scene/spot02_scene"
#define CUSTOM_SURFACE_TYPE 32

const static std::array<std::pair<std::pair<u16, u16>, std::pair<u16, u16>>, 6> graveyardGeometryPatches = { {
    // { { startPolygon, endPolygon }, { originalSurfaceType, patchedSurfaceType } }
    { { 487, 509 }, { 20, CUSTOM_SURFACE_TYPE } }, // Floor around graves
    { { 651, 658 }, { 20, CUSTOM_SURFACE_TYPE } }, // Floor around Royal Family Tomb
    { { 613, 620 }, { 0, 15 } },                   // Grave ledges (Hylian Shield)
    { { 623, 630 }, { 0, 15 } },                   // Grave ledges (Redead)
    { { 633, 640 }, { 0, 15 } },                   // Grave ledges (Dampe)
    { { 643, 650 }, { 0, 15 } },                   // Grave ledges (Royal Family)
} };

CollisionHeader* getGraveyardCollisionHeader() {
    /*
     * Load the graveyard collision header manually. Since its position varies between versions, we cannot directly use
     * dspot02_sceneCollisionHeader_003C54. We have to scroll through the scene cmds to get the header the same way the
     * game does.
     */
    SOH::Scene* scene =
        (SOH::Scene*)Ship::Context::GetInstance()->GetResourceManager()->LoadResource(GRAVEYARD_SCENE_FILEPATH).get();
    // Any of these can be null after a failed archive read (#560): the scene itself,
    // an individual command ParseSceneCommand could not build, and the collision
    // header sub-load SetCollisionHeaderFactory stores without checking. Bail out
    // instead of patching through a null — the patch re-runs on the next ShipInit or
    // CVar change, so skipping one attempt is recoverable.
    if (scene == nullptr) {
        SPDLOG_ERROR("GraveHoleJumps: failed to load the graveyard scene; skipping the geometry patch");
        return nullptr;
    }
    SOH::SetCollisionHeader* sceneCmd = nullptr;
    for (size_t i = 0; i < scene->commands.size(); i++) {
        auto cmd = scene->commands[i];
        if (cmd != nullptr && cmd->cmdId == SOH::SceneCommandID::SetCollisionHeader) {
            sceneCmd = static_cast<SOH::SetCollisionHeader*>(cmd.get());
            break;
        }
    }
    if (sceneCmd == nullptr || sceneCmd->collisionHeader == nullptr) {
        SPDLOG_ERROR("GraveHoleJumps: graveyard scene has no usable collision header; skipping the geometry patch");
        return nullptr;
    }
    CollisionHeader* graveyardColHeader = (CollisionHeader*)sceneCmd->GetRawPointer();
    if (graveyardColHeader == nullptr) {
        SPDLOG_ERROR("GraveHoleJumps: graveyard collision header has no data; skipping the geometry patch");
        return nullptr;
    }
    uint32_t surfaceTypesCount = sceneCmd->collisionHeader->surfaceTypesCount;

    /*
     * Copy the surface type list and give ourselves some extra space to create another surface type for Link to fall
     * into graves. NTSC 1.0's graveyard has 31 surface types, while later versions have 32. The contents of the lists
     * are shifted somewhat between versions, so to be safe we just create an extra slot that is not in any version.
     */
    static SurfaceType newSurfaceTypes[33];
    if (graveyardColHeader->surfaceTypeList != newSurfaceTypes) {
        memcpy(newSurfaceTypes, graveyardColHeader->surfaceTypeList, sizeof(SurfaceType) * surfaceTypesCount);
        newSurfaceTypes[CUSTOM_SURFACE_TYPE].data[0] = 0x24000004;
        newSurfaceTypes[CUSTOM_SURFACE_TYPE].data[1] = 0xFC8;
        graveyardColHeader->surfaceTypeList = newSurfaceTypes;
    }

    return graveyardColHeader;
}

void ApplyGraveyardGeometryPatches() {
    // The graveyard scene lives in the game archive. This runs at ShipInit
    // time (and on CVar change) now that soh_enh is force-linked (#361), and
    // LoadResource in RSBS's supported no-game-archive boots (#330) blocks
    // forever — it hung the rando-gen CI tests at 180s. Same gate as
    // OTRMessage_Init and InitTTSBank; re-runs apply once an archive exists.
    if (OTRGlobals::Instance == nullptr ||
        (!OTRGlobals::Instance->HasMasterQuest() && !OTRGlobals::Instance->HasOriginal())) {
        return;
    }
    // Re-fetch the collision header on every run instead of caching it in a static. The graveyard scene
    // lives in oot.o2r, which is hot-swapped on cross-game switches (OoT archives are unloaded when
    // switching to MM and re-added on the way back, see EnsureGameArchivesLoaded in rsbs/src/main.cpp).
    // A cached pointer into the old resource dangles after that swap, so writing through it on the next
    // ShipInit/CVar-change re-run was a use-after-free. LoadResource is a cache hit while the resource
    // is loaded, so re-fetching is cheap, and it also re-applies the patch to a freshly reloaded scene.
    CollisionHeader* graveyardColHeader = getGraveyardCollisionHeader();
    if (graveyardColHeader == nullptr) {
        return;
    }
    for (auto& mappingPatch : graveyardGeometryPatches) {
        for (int i = mappingPatch.first.first; i <= mappingPatch.first.second; i++) {
            CollisionPoly* poly = &graveyardColHeader->polyList[i];
            poly->type = CVAR_GRAVE_HOLE_VALUE ? mappingPatch.second.first : mappingPatch.second.second;
        }
    }
}

void RegisterGraveHoleJumps() {
    ApplyGraveyardGeometryPatches();
}

static RegisterShipInitFunc initFunc(RegisterGraveHoleJumps, { CVAR_GRAVE_HOLE_NAME });
