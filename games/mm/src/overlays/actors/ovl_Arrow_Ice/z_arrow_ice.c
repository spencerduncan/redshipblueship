/*
 * File: z_arrow_ice.c
 * Overlay: ovl_Arrow_Ice
 * Description: Ice Arrow. Spawned as a child of a normal arrow.
 */

#include "z_arrow_ice.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#include "2s2h/BenGui/CosmeticEditor.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

void MM_ArrowIce_Init(Actor* thisx, PlayState* play);
void MM_ArrowIce_Destroy(Actor* thisx, PlayState* play);
void MM_ArrowIce_Update(Actor* thisx, PlayState* play);
void MM_ArrowIce_Draw(Actor* thisx, PlayState* play);

void MM_ArrowIce_Charge(ArrowIce* this, PlayState* play);
void MM_ArrowIce_Fly(ArrowIce* this, PlayState* play);

#include "overlays/ovl_Arrow_Ice/ovl_Arrow_Ice.h"

ActorProfile Arrow_Ice_Profile = {
    /**/ ACTOR_ARROW_ICE,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(ArrowIce),
    /**/ MM_ArrowIce_Init,
    /**/ MM_ArrowIce_Destroy,
    /**/ MM_ArrowIce_Update,
    /**/ MM_ArrowIce_Draw,
};

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_F32(cullingVolumeDistance, 2000, ICHAIN_STOP),
};

void MM_ArrowIce_SetupAction(ArrowIce* this, ArrowIceActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void MM_ArrowIce_Init(Actor* thisx, PlayState* play) {
    ArrowIce* this = (ArrowIce*)thisx;

    MM_Actor_ProcessInitChain(&this->actor, MM_sInitChain);
    this->radius = 0;
    this->height = 1.0f;
    MM_ArrowIce_SetupAction(this, MM_ArrowIce_Charge);
    MM_Actor_SetScale(&this->actor, 0.01f);
    this->alpha = 100;
    this->timer = 0;
    this->blueingEffectMagnitude = 0.0f;
}

void MM_ArrowIce_Destroy(Actor* thisx, PlayState* play) {
    MM_Magic_Reset(play);
    (void)"消滅"; // Unreferenced in retail, means "Disappearance"
}

void MM_ArrowIce_Charge(ArrowIce* this, PlayState* play) {
    EnArrow* arrow = (EnArrow*)this->actor.parent;

    if ((arrow == NULL) || (arrow->actor.update == NULL)) {
        MM_Actor_Kill(&this->actor);
        return;
    }

    if (this->radius < 10) {
        this->radius++;
    }
    // copy position and rotation from arrow
    this->actor.world.pos = arrow->actor.world.pos;
    this->actor.shape.rot = arrow->actor.shape.rot;

    Actor_PlaySfx_Flagged(&this->actor, NA_SE_PL_ARROW_CHARGE_ICE - SFX_FLAG);

    // if arrow has no parent, player has fired the arrow
    if (arrow->actor.parent == NULL) {
        this->firedPos = this->actor.world.pos;
        this->radius = 10;
        MM_ArrowIce_SetupAction(this, MM_ArrowIce_Fly);
        this->alpha = 255;
    }
}

void ArrowIce_LerpFiredPosition(Vec3f* firedPos, Vec3f* icePos, f32 scale) {
    VEC3F_LERPIMPDST(firedPos, firedPos, icePos, scale);
}

void MM_ArrowIce_Hit(ArrowIce* this, PlayState* play) {
    f32 scale;
    u16 timer;

    if (this->actor.projectedW < 50.0f) {
        scale = 10.0f;
    } else if (this->actor.projectedW > 950.0f) {
        scale = 310.0f;
    } else {
        scale = this->actor.projectedW;
        scale = (scale - 50.0f) * (1.0f / 3.0f) + 10.0f;
    }

    timer = this->timer;
    if (timer != 0) {
        this->timer--;

        if (this->timer >= 8) {
            f32 offset = ((this->timer - 8) * (1.0f / 24.0f));

            offset = SQ(offset);
            this->radius = (((1.0f - offset) * scale) + 10.0f);
            this->height = F32_LERPIMP(this->height, 2.0f, 0.1f);
            if (this->timer < 16) {
                this->alpha = ((this->timer * 35) - 280);
            }
        }
    }

    if (this->timer >= 9) {
        if (this->blueingEffectMagnitude < 1.0f) {
            this->blueingEffectMagnitude += 0.25f;
        }
    } else {
        if (this->blueingEffectMagnitude > 0.0f) {
            this->blueingEffectMagnitude -= 0.125f;
        }
    }

    if (this->timer < 8) {
        this->alpha = 0;
    }

    if (this->timer == 0) {
        this->timer = 255;
        MM_Actor_Kill(&this->actor);
    }
}

void MM_ArrowIce_Fly(ArrowIce* this, PlayState* play) {
    EnArrow* arrow = (EnArrow*)this->actor.parent;
    f32 distanceScaled;
    s32 pad;

    if ((arrow == NULL) || (arrow->actor.update == NULL)) {
        MM_Actor_Kill(&this->actor);
        return;
    }
    // copy position and rotation from arrow
    this->actor.world.pos = arrow->actor.world.pos;
    this->actor.shape.rot = arrow->actor.shape.rot;
    distanceScaled = MM_Math_Vec3f_DistXYZ(&this->firedPos, &this->actor.world.pos) * (1.0f / 24.0f);
    this->height = distanceScaled;
    if (distanceScaled < 1.0f) {
        this->height = 1.0f;
    }
    ArrowIce_LerpFiredPosition(&this->firedPos, &this->actor.world.pos, 0.05f);

    if (arrow->unk_261 & 1) {
        Actor_PlaySfx(&this->actor, NA_SE_IT_EXPLOSION_ICE);
        MM_ArrowIce_SetupAction(this, MM_ArrowIce_Hit);
        this->timer = 32;
        this->alpha = 255;
    } else if (arrow->unk_260 < 34) {
        if (this->alpha < 35) {
            MM_Actor_Kill(&this->actor);
        } else {
            this->alpha -= 25;
        }
    }
}

void MM_ArrowIce_Update(Actor* thisx, PlayState* play) {
    ArrowIce* this = (ArrowIce*)thisx;

    if ((play->msgCtx.msgMode == MSGMODE_E) || (play->msgCtx.msgMode == MSGMODE_SONG_PLAYED)) {
        MM_Actor_Kill(&this->actor);
        return;
    } else {
        this->actionFunc(this, play);
    }
}

void MM_ArrowIce_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    ArrowIce* this = (ArrowIce*)thisx;
    Actor* transform;
    u32 stateFrames = play->state.frames;
    EnArrow* arrow = (EnArrow*)this->actor.parent;

    if ((arrow != NULL) && (arrow->actor.update != NULL) && (this->timer < 255)) {
        transform = (arrow->unk_261 & 2) ? &this->actor : &arrow->actor;

        OPEN_DISPS(play->state.gfxCtx);

        MM_Matrix_Translate(transform->world.pos.x, transform->world.pos.y, transform->world.pos.z, MTXMODE_NEW);
        Matrix_RotateYS(transform->shape.rot.y, MTXMODE_APPLY);
        Matrix_RotateXS(transform->shape.rot.x, MTXMODE_APPLY);
        Matrix_RotateZS(transform->shape.rot.z, MTXMODE_APPLY);
        MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);

        // Draw blue effect over the screen when arrow hits
        if (this->blueingEffectMagnitude > 0.0f) {
            POLY_XLU_DISP = Gfx_SetupDL57(POLY_XLU_DISP);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, (s32)(this->blueingEffectMagnitude * 10.0f) & 0xFF,
                            (s32)(50.0f * this->blueingEffectMagnitude) & 0xFF,
                            (s32)(150.0f * this->blueingEffectMagnitude) & 0xFF);
            gDPSetAlphaDither(POLY_XLU_DISP++, G_AD_DISABLE);
            gDPSetColorDither(POLY_XLU_DISP++, G_CD_DISABLE);
            gSPDisplayList(POLY_XLU_DISP++, D_0E000000_TO_SEGMENTED(fillRect));
        }

        // Draw ice on the arrow
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPSetPrimColorOverride(POLY_XLU_DISP++, 0x80, 0x80, 170, 255, 255, (s32)(this->alpha * 0.5f) & 0xFF,
                                COSMETIC_ELEMENT_ICE_ARROW_PRIMARY);
        gDPSetEnvColorOverride(POLY_XLU_DISP++, 0, 0, 255, 128, COSMETIC_ELEMENT_ICE_ARROW_SECONDARY);
        MM_Matrix_RotateZYX(0x4000, 0, 0, MTXMODE_APPLY);
        if (this->timer != 0) {
            MM_Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        } else {
            MM_Matrix_Translate(0.0f, 1500.0f, 0.0f, MTXMODE_APPLY);
        }
        MM_Matrix_Scale(this->radius * 0.2f, this->height * 3.0f, this->radius * 0.2f, MTXMODE_APPLY);
        MM_Matrix_Translate(0.0f, -700.0f, 0.0f, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, gIceArrowMaterialDL);
        gSPDisplayList(POLY_XLU_DISP++,
                       MM_Gfx_TwoTexScroll(play->state.gfxCtx, 0, 511 - (stateFrames * 5) % 512, 0, 128, 32, 1,
                                        511 - (stateFrames * 10) % 512, 511 - (stateFrames * 10) % 512, 4, 16));
        gSPDisplayList(POLY_XLU_DISP++, gIceArrowModelDL);

        CLOSE_DISPS(play->state.gfxCtx);
    }
}
