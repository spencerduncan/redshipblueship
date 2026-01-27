/*
 * File: z_en_okarina_effect.c
 * Overlay: ovl_En_Okarina_Effect
 * Description: Manages the storm created when playing Song of Storms
 */

#include "z_en_okarina_effect.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

void MM_EnOkarinaEffect_Init(Actor* thisx, PlayState* play);
void MM_EnOkarinaEffect_Destroy(Actor* thisx, PlayState* play);
void MM_EnOkarinaEffect_Update(Actor* thisx, PlayState* play);

void func_8096B104(EnOkarinaEffect* this, PlayState* play);
void func_8096B174(EnOkarinaEffect* this, PlayState* play);
void func_8096B1FC(EnOkarinaEffect* this, PlayState* play);

ActorProfile En_Okarina_Effect_Profile = {
    /**/ ACTOR_EN_OKARINA_EFFECT,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnOkarinaEffect),
    /**/ MM_EnOkarinaEffect_Init,
    /**/ MM_EnOkarinaEffect_Destroy,
    /**/ MM_EnOkarinaEffect_Update,
    /**/ NULL,
};

void MM_EnOkarinaEffect_SetupAction(EnOkarinaEffect* this, EnOkarinaEffectActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void MM_EnOkarinaEffect_Destroy(Actor* thisx, PlayState* play) {
}

void MM_EnOkarinaEffect_Init(Actor* thisx, PlayState* play) {
    EnOkarinaEffect* this = (EnOkarinaEffect*)thisx;

    if (play->envCtx.precipitation[PRECIP_RAIN_CUR] != 0) {
        MM_Actor_Kill(&this->actor);
    }
    MM_EnOkarinaEffect_SetupAction(this, func_8096B104);
}

void func_8096B104(EnOkarinaEffect* this, PlayState* play) {
    this->timer = 80;
    play->envCtx.precipitation[PRECIP_SOS_MAX] = 60;
    MM_gLightningStrike.delayTimer = 501.0f;
    play->envCtx.lightningState = LIGHTNING_LAST;
    MM_Environment_PlayStormNatureAmbience(play);
    MM_EnOkarinaEffect_SetupAction(this, func_8096B174);
}

void func_8096B174(EnOkarinaEffect* this, PlayState* play) {
    DECR(this->timer);

    if ((play->pauseCtx.state == PAUSE_STATE_OFF) && (play->gameOverCtx.state == GAMEOVER_INACTIVE) &&
        (play->msgCtx.msgLength == 0) && !MM_FrameAdvance_IsEnabled(play) && (this->timer == 0)) {
        MM_EnOkarinaEffect_SetupAction(this, func_8096B1FC);
    }
}

void func_8096B1FC(EnOkarinaEffect* this, PlayState* play) {
    if (play->envCtx.precipitation[PRECIP_SOS_MAX] != 0) {
        if ((play->state.frames & 3) == 0) {
            play->envCtx.precipitation[PRECIP_SOS_MAX]--;
            if (play->envCtx.precipitation[PRECIP_SOS_MAX] == 8) {
                MM_Environment_StopStormNatureAmbience(play);
            }
        }
    } else {
        MM_Actor_Kill(&this->actor);
    }
}

void MM_EnOkarinaEffect_Update(Actor* thisx, PlayState* play) {
    EnOkarinaEffect* this = (EnOkarinaEffect*)thisx;

    this->actionFunc(this, play);
}
