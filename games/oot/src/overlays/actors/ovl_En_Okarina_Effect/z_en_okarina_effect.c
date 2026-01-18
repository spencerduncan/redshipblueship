/*
 * File: z_en_okarina_effect.c
 * Overlay: ovl_En_Okarina_Effect
 * Description: Manages the storm created when playing Song of Storms
 */

#include "z_en_okarina_effect.h"
#include "vt.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

void OoT_EnOkarinaEffect_Init(Actor* thisx, PlayState* play);
void OoT_EnOkarinaEffect_Destroy(Actor* thisx, PlayState* play);
void OoT_EnOkarinaEffect_Update(Actor* thisx, PlayState* play);

void EnOkarinaEffect_TriggerStorm(EnOkarinaEffect* this, PlayState* play);
void EnOkarinaEffect_ManageStorm(EnOkarinaEffect* this, PlayState* play);

const ActorInit En_Okarina_Effect_InitVars = {
    ACTOR_EN_OKARINA_EFFECT,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnOkarinaEffect),
    (ActorFunc)OoT_EnOkarinaEffect_Init,
    (ActorFunc)OoT_EnOkarinaEffect_Destroy,
    (ActorFunc)OoT_EnOkarinaEffect_Update,
    NULL,
    NULL,
};

void OoT_EnOkarinaEffect_SetupAction(EnOkarinaEffect* this, EnOkarinaEffectActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_EnOkarinaEffect_Destroy(Actor* thisx, PlayState* play) {
    EnOkarinaEffect* this = (EnOkarinaEffect*)thisx;

    play->envCtx.unk_F2[0] = 0;
    if ((OoT_gWeatherMode != 4) && (OoT_gWeatherMode != 5) && (play->envCtx.gloomySkyMode == 1)) {
        play->envCtx.gloomySkyMode = 2; // end gloomy sky
        OoT_Environment_StopStormNatureAmbience(play);
    }
    play->envCtx.lightningMode = LIGHTNING_MODE_LAST;
}

void OoT_EnOkarinaEffect_Init(Actor* thisx, PlayState* play) {
    EnOkarinaEffect* this = (EnOkarinaEffect*)thisx;

    osSyncPrintf("\n\n");
    // "Ocarina Storm Effect"
    osSyncPrintf(VT_FGCOL(YELLOW) "☆☆☆☆☆ オカリナあらし効果ビカビカビカ〜 ☆☆☆☆☆ \n" VT_RST);
    osSyncPrintf("\n\n");
    if (play->envCtx.unk_EE[1] != 0) {
        OoT_Actor_Kill(&this->actor);
    }
    OoT_EnOkarinaEffect_SetupAction(this, EnOkarinaEffect_TriggerStorm);
}

void EnOkarinaEffect_TriggerStorm(EnOkarinaEffect* this, PlayState* play) {
    this->timer = 400;              // 20 seconds
    play->envCtx.unk_F2[0] = 20;    // rain intensity target
    play->envCtx.gloomySkyMode = 1; // start gloomy sky
    if ((OoT_gWeatherMode != 0) || play->envCtx.unk_17 != 0) {
        play->envCtx.unk_DE = 1;
    }
    play->envCtx.lightningMode = LIGHTNING_MODE_ON;
    OoT_Environment_PlayStormNatureAmbience(play);
    OoT_EnOkarinaEffect_SetupAction(this, EnOkarinaEffect_ManageStorm);
}

void EnOkarinaEffect_ManageStorm(EnOkarinaEffect* this, PlayState* play) {
    Flags_UnsetEnv(play, 5); // clear storms env flag
    if (((play->pauseCtx.state == 0) && (play->gameOverCtx.state == GAMEOVER_INACTIVE) &&
         (play->msgCtx.msgLength == 0) && (!OoT_FrameAdvance_IsEnabled(play)) &&
         ((play->transitionMode == TRANS_MODE_OFF) || (gSaveContext.gameMode != GAMEMODE_NORMAL))) ||
        (this->timer >= 250)) {
        if (play->envCtx.indoors || play->envCtx.unk_1F != 1) {
            this->timer--;
        }
        osSyncPrintf("\nthis->timer=[%d]", this->timer);
        if (this->timer == 308) {
            osSyncPrintf("\n\n\n豆よ のびろ 指定\n\n\n"); // "Let's grow some beans"
            Flags_SetEnv(play, 5);                        // set storms env flag
        }
    }

    if (D_8011FB38 != 0) {
        this->timer = 0;
    }

    if (this->timer == 0) {
        play->envCtx.unk_F2[0] = 0;
        if (play->csCtx.state == CS_STATE_IDLE) {
            OoT_Environment_StopStormNatureAmbience(play);
        } else if (func_800FA0B4(SEQ_PLAYER_BGM_MAIN) == NA_BGM_NATURE_AMBIENCE) {
            Audio_SetNatureAmbienceChannelIO(NATURE_CHANNEL_LIGHTNING, CHANNEL_IO_PORT_1, 0);
            Audio_SetNatureAmbienceChannelIO(NATURE_CHANNEL_RAIN, CHANNEL_IO_PORT_1, 0);
        }
        osSyncPrintf("\n\n\nE_wether_flg=[%d]", OoT_gWeatherMode);
        osSyncPrintf("\nrain_evt_trg=[%d]\n\n", play->envCtx.gloomySkyMode);
        if (OoT_gWeatherMode == 0 && (play->envCtx.gloomySkyMode == 1)) {
            play->envCtx.gloomySkyMode = 2; // end gloomy sky
        } else {
            play->envCtx.gloomySkyMode = 0;
            play->envCtx.unk_DE = 0;
        }
        play->envCtx.lightningMode = LIGHTNING_MODE_LAST;
        OoT_Actor_Kill(&this->actor);
    }
}

void OoT_EnOkarinaEffect_Update(Actor* thisx, PlayState* play) {
    EnOkarinaEffect* this = (EnOkarinaEffect*)thisx;

    this->actionFunc(this, play);
    if (BREG(0) != 0) {
        OoT_DebugDisplay_AddObject(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z,
                               this->actor.world.rot.x, this->actor.world.rot.y, this->actor.world.rot.z, 1.0f, 1.0f,
                               1.0f, 0xFF, 0, 0xFF, 0xFF, 4, play->state.gfxCtx);
    }
}
