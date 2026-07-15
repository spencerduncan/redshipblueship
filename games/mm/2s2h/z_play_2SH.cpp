#include "BenPort.h"
#include "2s2h/resource/type/Scene.h"
#include <ship/utils/StringHelper.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include <ship/resource/ResourceManager.h>

extern "C" {
#include "global.h"
extern uintptr_t MM_gSegments[NUM_SEGMENTS];
}

// static: OoT's z_play_otr.cpp defines an identically-mangled OTRPlay_LoadFile (single-exe, #344)
static Ship::IResource* OTRPlay_LoadFile(PlayState* play, const char* fileName) {
    auto res = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(fileName);
    return res.get();
}

s32 MM_OTRScene_ExecuteCommands(PlayState* play, S2H::Scene* scene);

extern "C" void MM_OTRPlay_InitScene(PlayState* play, s32 spawn) {
    play->curSpawn = spawn;
    play->linkActorEntry = nullptr;
    play->actorCsCamList = nullptr;
    play->setupEntranceList = nullptr;
    play->setupExitList = nullptr;
    play->naviQuestHints = nullptr;
    play->setupPathList = nullptr;
    play->sceneMaterialAnims = nullptr;
    play->roomCtx.unk74 = nullptr;
    play->numSetupActors = 0;
    Object_InitContext(&play->state, &play->objectCtx);
    MM_LightContext_Init(play, &play->lightCtx);
    Scene_ResetTransitionActorList(&play->state, &play->transitionActors);
    Room_Init(play, &play->roomCtx);
    gSaveContext.worldMapArea = 0;
    MM_OTRScene_ExecuteCommands(play, (S2H::Scene*)play->sceneSegment);
    MM_Play_InitEnvironment(play, play->skyboxId);
}

extern "C" void MM_OTRPlay_SpawnScene(PlayState* play, s32 sceneId, s32 spawn) {
    s32 pad;
    SceneTableEntry* scene = &MM_gSceneTable[sceneId];

    scene->unk_D = 0;
    play->loadedScene = scene;
    play->sceneId = sceneId;
    play->sceneConfig = scene->drawConfig;
    std::string scenePath =
        StringHelper::Sprintf("scenes/nonmq/%s/%s", scene->segment.fileName, scene->segment.fileName);
    play->sceneSegment = OTRPlay_LoadFile(play, scenePath.c_str());
    if (play->sceneSegment == nullptr) {
        // No fallback scene makes sense for MM; fail loudly instead of crashing blind in
        // MM_OTRScene_ExecuteCommands (#344 was a silent mislink — keep failures diagnosable).
        fprintf(stderr, "[MM] FATAL: failed to load scene resource '%s' (sceneId %d)\n", scenePath.c_str(), sceneId);
        fflush(stderr);
        return;
    }
    scene->unk_D = 0;
    MM_gSegments[2] = (uintptr_t)play->sceneSegment;
    MM_OTRPlay_InitScene(play, spawn);
    Room_SetupFirstRoom(play, &play->roomCtx);
}

extern "C" s32 MM_OTRfunc_800973FC(PlayState* play, RoomContext* roomCtx) {
    if (roomCtx->status == 1) {
        // if (!MM_osRecvMesg(&roomCtx->loadQueue, nullptr, OS_MESG_NOBLOCK)) {
        if (1) {
            roomCtx->status = 0;
            roomCtx->curRoom.segment = roomCtx->roomRequestAddr;
            MM_gSegments[3] = (uintptr_t)roomCtx->roomRequestAddr;

            MM_OTRScene_ExecuteCommands(play, (S2H::Scene*)roomCtx->curRoom.segment);
            func_80123140(play, GET_PLAYER(play));
            MM_Actor_SpawnTransitionActors(play, &play->actorCtx);
            if (((play->sceneId != SCENE_IKANA) || (roomCtx->curRoom.num != 1)) && (play->sceneId != SCENE_IKNINSIDE)) {
                play->envCtx.lightSettingOverride = LIGHT_SETTING_OVERRIDE_NONE;
                play->envCtx.lightBlendOverride = LIGHT_BLEND_OVERRIDE_NONE;
            }
            func_800FEAB0();
            if (Environment_GetStormState(play) == STORM_STATE_OFF) {
                MM_Environment_StopStormNatureAmbience(play);
            }
            // Insert hook
            GameInteractor_ExecuteAfterRoomSceneCommands(play->sceneId, roomCtx->curRoom.num);
            return 1;
        }

        return 0;
    }

    return 1;
}
