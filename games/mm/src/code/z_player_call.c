#include "prevent_bss_reordering.h"
#include "global.h"
#include "z64pause_menu.h"

#define FLAGS                                                                                  \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_SOARING_AND_SOT_CS |          \
     ACTOR_FLAG_UPDATE_DURING_OCARINA | ACTOR_FLAG_CAN_PRESS_SWITCHES | ACTOR_FLAG_MINIMAP_ICON_ENABLED)

ActorFunc MM_sPlayerCallInitFunc;
ActorFunc MM_sPlayerCallDestroyFunc;
ActorFunc MM_sPlayerCallUpdateFunc;
ActorFunc MM_sPlayerCallDrawFunc;

ActorProfile Player_Profile = {
    /**/ ACTOR_PLAYER,
    /**/ ACTORCAT_PLAYER,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(Player),
    /**/ MM_PlayerCall_Init,
    /**/ MM_PlayerCall_Destroy,
    /**/ MM_PlayerCall_Update,
    /**/ MM_PlayerCall_Draw,
};

void MM_Player_Init(Actor* thisx, PlayState* play);
void MM_Player_Destroy(Actor* thisx, PlayState* play);
void MM_Player_Update(Actor* thisx, PlayState* play);
void MM_Player_Draw(Actor* thisx, PlayState* play);

void MM_PlayerCall_InitFuncPtrs(void) {
    MM_sPlayerCallInitFunc = MM_KaleidoManager_GetRamAddr(MM_Player_Init);
    MM_sPlayerCallDestroyFunc = MM_KaleidoManager_GetRamAddr(MM_Player_Destroy);
    MM_sPlayerCallUpdateFunc = MM_KaleidoManager_GetRamAddr(MM_Player_Update);
    MM_sPlayerCallDrawFunc = MM_KaleidoManager_GetRamAddr(MM_Player_Draw);
}

void MM_PlayerCall_Init(Actor* thisx, PlayState* play) {
    MM_KaleidoScopeCall_LoadPlayer();
    MM_PlayerCall_InitFuncPtrs();
    MM_sPlayerCallInitFunc(thisx, play);
}

void MM_PlayerCall_Destroy(Actor* thisx, PlayState* play) {
    MM_KaleidoScopeCall_LoadPlayer();
    MM_sPlayerCallDestroyFunc(thisx, play);
}

void MM_PlayerCall_Update(Actor* thisx, PlayState* play) {
    MM_KaleidoScopeCall_LoadPlayer();
    MM_sPlayerCallUpdateFunc(thisx, play);
}

void MM_PlayerCall_Draw(Actor* thisx, PlayState* play) {
    MM_KaleidoScopeCall_LoadPlayer();
    MM_sPlayerCallDrawFunc(thisx, play);
}
