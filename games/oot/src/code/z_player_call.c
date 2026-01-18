#include "global.h"

#define FLAGS                                                                                 \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA | ACTOR_FLAG_CAN_PRESS_SWITCHES)

void (*sPlayerCallInitFunc)(Actor* thisx, PlayState* play);
void (*sPlayerCallDestroyFunc)(Actor* thisx, PlayState* play);
void (*sPlayerCallUpdateFunc)(Actor* thisx, PlayState* play);
void (*sPlayerCallDrawFunc)(Actor* thisx, PlayState* play);

void OoT_PlayerCall_Init(Actor* thisx, PlayState* play);
void OoT_PlayerCall_Destroy(Actor* thisx, PlayState* play);
void OoT_PlayerCall_Update(Actor* thisx, PlayState* play);
void OoT_PlayerCall_Draw(Actor* thisx, PlayState* play);

void OoT_Player_Init(Actor* thisx, PlayState* play);
void OoT_Player_Destroy(Actor* thisx, PlayState* play);
void OoT_Player_Update(Actor* thisx, PlayState* play);
void OoT_Player_Draw(Actor* thisx, PlayState* play);

const ActorInit Player_InitVars = {
    ACTOR_PLAYER,
    ACTORCAT_PLAYER,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(Player),
    (ActorFunc)OoT_PlayerCall_Init,
    (ActorFunc)OoT_PlayerCall_Destroy,
    (ActorFunc)OoT_PlayerCall_Update,
    (ActorFunc)OoT_PlayerCall_Draw,
    NULL,
};

void OoT_PlayerCall_InitFuncPtrs(void) {
    sPlayerCallInitFunc = OoT_KaleidoManager_GetRamAddr(OoT_Player_Init);
    sPlayerCallDestroyFunc = OoT_KaleidoManager_GetRamAddr(OoT_Player_Destroy);
    sPlayerCallUpdateFunc = OoT_KaleidoManager_GetRamAddr(OoT_Player_Update);
    sPlayerCallDrawFunc = OoT_KaleidoManager_GetRamAddr(OoT_Player_Draw);
}

void OoT_PlayerCall_Init(Actor* thisx, PlayState* play) {
    OoT_KaleidoScopeCall_LoadPlayer();
    OoT_PlayerCall_InitFuncPtrs();
    sPlayerCallInitFunc(thisx, play);
}

void OoT_PlayerCall_Destroy(Actor* thisx, PlayState* play) {
    OoT_KaleidoScopeCall_LoadPlayer();
    sPlayerCallDestroyFunc(thisx, play);
}

void OoT_PlayerCall_Update(Actor* thisx, PlayState* play) {
    OoT_KaleidoScopeCall_LoadPlayer();
    sPlayerCallUpdateFunc(thisx, play);
}

void OoT_PlayerCall_Draw(Actor* thisx, PlayState* play) {
    OoT_KaleidoScopeCall_LoadPlayer();
    sPlayerCallDrawFunc(thisx, play);
}
