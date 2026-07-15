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
    fprintf(stderr, "[MM-DIAG] InitScene: ExecuteCommands returned, calling InitEnvironment skyboxId=%d\n",
            play->skyboxId);
    fflush(stderr);
    MM_Play_InitEnvironment(play, play->skyboxId);
    fprintf(stderr, "[MM-DIAG] InitScene: InitEnvironment done\n");
    fflush(stderr);
}

extern "C" void MM_OTRPlay_SpawnScene(PlayState* play, s32 sceneId, s32 spawn) {
    s32 pad;
    fprintf(stderr, "[MM-DIAG] SpawnScene ENTER sceneId=%d spawn=%d\n", sceneId, spawn);
    fflush(stderr);
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
    fprintf(stderr, "[MM-DIAG] SpawnScene: scene loaded seg=%p, calling InitScene\n", (void*)play->sceneSegment);
    fflush(stderr);
    MM_OTRPlay_InitScene(play, spawn);
    fprintf(stderr, "[MM-DIAG] SpawnScene: InitScene done, calling Room_SetupFirstRoom\n");
    fflush(stderr);
    Room_SetupFirstRoom(play, &play->roomCtx);
    fprintf(stderr, "[MM-DIAG] SpawnScene: Room_SetupFirstRoom done, SpawnScene RETURN\n");
    fflush(stderr);
}

extern "C" s32 MM_OTRfunc_800973FC(PlayState* play, RoomContext* roomCtx) {
    if (roomCtx->status == 1) {
        // if (!MM_osRecvMesg(&roomCtx->loadQueue, nullptr, OS_MESG_NOBLOCK)) {
        if (1) {
            roomCtx->status = 0;
            roomCtx->curRoom.segment = roomCtx->roomRequestAddr;
            MM_gSegments[3] = (uintptr_t)roomCtx->roomRequestAddr;

            fprintf(stderr, "[MM-DIAG] RoomCmds(800973FC): room seg=%p, ExecuteCommands\n",
                    (void*)roomCtx->curRoom.segment);
            fflush(stderr);
            MM_OTRScene_ExecuteCommands(play, (S2H::Scene*)roomCtx->curRoom.segment);
            fprintf(stderr, "[MM-DIAG] RoomCmds: player=%p, calling func_80123140\n", (void*)GET_PLAYER(play));
            fflush(stderr);
            func_80123140(play, GET_PLAYER(play));
            fprintf(stderr, "[MM-DIAG] RoomCmds: func_80123140 done, SpawnTransitionActors\n");
            fflush(stderr);
            MM_Actor_SpawnTransitionActors(play, &play->actorCtx);
            fprintf(stderr, "[MM-DIAG] RoomCmds: SpawnTransitionActors done\n");
            fflush(stderr);
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
