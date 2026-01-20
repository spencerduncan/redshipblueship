#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"

void MM_Player_PlaySfx(Player* player, u16 sfxId);
void MM_PlayerCall_Init(Actor* thisx, PlayState* play);
void MM_PlayerCall_Update(Actor* thisx, PlayState* play);
void MM_PlayerCall_Draw(Actor* thisx, PlayState* play);
void MM_TransitionFade_SetColor(void* thisx, u32 color);
}

#define CVAR_NAME "gEnhancements.Masks.FastTransformation"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterFastTransformation() {
    COND_VB_SHOULD(VB_PREVENT_MASK_TRANSFORMATION_CS, CVAR, {
        *should = true;
        Player* player = GET_PLAYER(MM_gPlayState);

        // This was mostly copied directly from func_8012301C within z_player_lib.c
        s16 objectId = gPlayerFormObjectIds[GET_PLAYER_FORM];

        gActorOverlayTable[ACTOR_PLAYER].profile->objectId = objectId;
        func_8012F73C(&MM_gPlayState->objectCtx, player->actor.objectSlot, objectId);
        player->actor.objectSlot = Object_GetSlot(&MM_gPlayState->objectCtx, GAMEPLAY_KEEP);

        s32 objectSlot = Object_GetSlot(&MM_gPlayState->objectCtx, gActorOverlayTable[ACTOR_PLAYER].profile->objectId);
        player->actor.objectSlot = objectSlot;
        player->actor.shape.rot.z = GET_PLAYER_FORM + 1;
        player->actor.init = MM_PlayerCall_Init;
        player->actor.update = MM_PlayerCall_Update;
        player->actor.draw = MM_PlayerCall_Draw;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;

        MM_TransitionFade_SetColor(&MM_gPlayState->unk_18E48, 0x000000);
        R_TRANS_FADE_FLASH_ALPHA_STEP = -1;
        MM_Player_PlaySfx(GET_PLAYER(MM_gPlayState), NA_SE_SY_TRANSFORM_MASK_FLASH);

        // Clear previous mask to prevent crashing with masks being drawn while we switch transformations
        if (player->transformation == PLAYER_FORM_HUMAN) {
            player->prevMask = PLAYER_MASK_NONE;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterFastTransformation, { CVAR_NAME });
