/*
 * File: z_obj_roomtimer.c
 * Overlay: ovl_Obj_Roomtimer
 * Description: Starts Timer 1 with a value specified in params
 */

#include "z_obj_roomtimer.h"

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void OoT_ObjRoomtimer_Init(Actor* thisx, PlayState* play);
void OoT_ObjRoomtimer_Destroy(Actor* thisx, PlayState* play);
void OoT_ObjRoomtimer_Update(Actor* thisx, PlayState* play);

void func_80B9D054(ObjRoomtimer* this, PlayState* play);
void func_80B9D0B0(ObjRoomtimer* this, PlayState* play);

const ActorInit Obj_Roomtimer_InitVars = {
    ACTOR_OBJ_ROOMTIMER,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(ObjRoomtimer),
    (ActorFunc)OoT_ObjRoomtimer_Init,
    (ActorFunc)OoT_ObjRoomtimer_Destroy,
    (ActorFunc)OoT_ObjRoomtimer_Update,
    (ActorFunc)NULL,
    NULL,
};

void OoT_ObjRoomtimer_Init(Actor* thisx, PlayState* play) {
    ObjRoomtimer* this = (ObjRoomtimer*)thisx;
    s16 params = this->actor.params;

    this->switchFlag = (params >> 10) & 0x3F;
    this->actor.params = params & 0x3FF;
    params = this->actor.params;

    if (params != 0x3FF) {
        if (params > 600) {
            this->actor.params = 600;
        } else {
            this->actor.params = params;
        }
    }

    this->actionFunc = func_80B9D054;
}

void OoT_ObjRoomtimer_Destroy(Actor* thisx, PlayState* play) {
    ObjRoomtimer* this = (ObjRoomtimer*)thisx;

    if ((this->actor.params != 0x3FF) && (gSaveContext.timerSeconds > 0)) {
        gSaveContext.timerState = TIMER_STATE_STOP;
    }
}

void func_80B9D054(ObjRoomtimer* this, PlayState* play) {
    if (this->actor.params != 0x3FF) {
        Interface_SetTimer(this->actor.params);
    }

    OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
    this->actionFunc = func_80B9D0B0;
}

void func_80B9D0B0(ObjRoomtimer* this, PlayState* play) {
    if (Flags_GetTempClear(play, this->actor.room)) {
        if (this->actor.params != 0x3FF) {
            gSaveContext.timerState = TIMER_STATE_STOP;
        }
        OoT_Flags_SetClear(play, this->actor.room);
        OoT_Flags_SetSwitch(play, this->switchFlag);
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
        OoT_Actor_Kill(&this->actor);
    } else {
        if ((this->actor.params != 0x3FF) && (gSaveContext.timerSeconds == 0)) {
            Audio_PlaySoundGeneral(NA_SE_OC_ABYSS, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                   &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
            Play_TriggerVoidOut(play);
            OoT_Actor_Kill(&this->actor);
        }
    }
}

void OoT_ObjRoomtimer_Update(Actor* thisx, PlayState* play) {
    ObjRoomtimer* this = (ObjRoomtimer*)thisx;

    this->actionFunc(this, play);
}
