/*
 * File: z_en_lightbox.c
 * Overlay: ovl_En_Lightbox
 * Description:
 */

#include "z_en_lightbox.h"
#include "objects/object_lightbox/object_lightbox.h"

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void EnLightbox_Init(Actor* thisx, PlayState* play);
void EnLightbox_Destroy(Actor* thisx, PlayState* play);
void EnLightbox_Update(Actor* thisx, PlayState* play);
void EnLightbox_Draw(Actor* thisx, PlayState* play);

const ActorInit En_Lightbox_InitVars = {
    ACTOR_EN_LIGHTBOX,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_LIGHTBOX,
    sizeof(EnLightbox),
    (ActorFunc)EnLightbox_Init,
    (ActorFunc)EnLightbox_Destroy,
    (ActorFunc)EnLightbox_Update,
    (ActorFunc)EnLightbox_Draw,
    NULL,
};

void EnLightbox_Init(Actor* thisx, PlayState* play) {
    CollisionHeader* colHeader = NULL;
    EnLightbox* this = (EnLightbox*)thisx;
    s32 pad[4];

    switch (thisx->params) {
        case 0:
            OoT_Actor_SetScale(thisx, 0.025f);
            break;
        case 1:
            OoT_Actor_SetScale(thisx, 0.05f);
            break;
        case 2:
            OoT_Actor_SetScale(thisx, 0.075f);
            break;
        case 3:
            OoT_Actor_SetScale(thisx, 0.1f);
        default:
            break;
    }

    thisx->focus.pos = thisx->world.pos;
    thisx->colChkInfo.cylRadius = 30;
    thisx->colChkInfo.cylHeight = 50;
    OoT_ActorShape_Init(&thisx->shape, 0.0f, OoT_ActorShadow_DrawCircle, 6.0f);
    this->dyna.interactFlags = 0;
    this->dyna.transformFlags = 0;
    thisx->targetMode = 0;
    thisx->gravity = -2.0f;
    OoT_CollisionHeader_GetVirtual(&object_lightbox_Col_001F10, &colHeader);
    this->dyna.bgId = OoT_DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
}

void EnLightbox_Destroy(Actor* thisx, PlayState* play) {
    EnLightbox* this = (EnLightbox*)thisx;

    OoT_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void EnLightbox_Update(Actor* thisx, PlayState* play) {
    EnLightbox* this = (EnLightbox*)thisx;

    if (this->dyna.unk_162 != 0) {
        if (OoT_Actor_HasNoParent(thisx, play)) {
            this->dyna.unk_162 = 0;
        }
    } else {
        if (OoT_Actor_HasParent(thisx, play)) {
            this->dyna.unk_162++;
        } else {
            if (thisx->speedXZ) {
                if (thisx->bgCheckFlags & 8) {
                    thisx->world.rot.y = (thisx->world.rot.y + thisx->wallYaw) - thisx->world.rot.y;
                    Audio_PlaySoundGeneral(NA_SE_EV_BOMB_BOUND, &thisx->projectedPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                           &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
                    thisx->speedXZ *= 0.7f;
                    thisx->bgCheckFlags &= ~0x8;
                }
            }

            if ((thisx->bgCheckFlags & 1) == 0) {
                OoT_Math_StepToF(&thisx->speedXZ, 0, IREG(57) / 100.0f);
            } else {
                OoT_Math_StepToF(&thisx->speedXZ, 0, IREG(58) / 100.0f);
                if ((thisx->bgCheckFlags & 2) && (thisx->velocity.y < IREG(59) / 100.0f)) {
                    Audio_PlaySoundGeneral(NA_SE_EV_BOMB_BOUND, &thisx->projectedPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                           &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
                    thisx->velocity.y *= IREG(60) / 100.0f;
                    thisx->bgCheckFlags &= ~0x1;
                } else {
                    OoT_Actor_OfferCarry(thisx, play);
                }
            }
        }
    }
    Actor_MoveXZGravity(thisx);
    OoT_Actor_UpdateBgCheckInfo(play, thisx, thisx->colChkInfo.cylHeight, thisx->colChkInfo.cylRadius,
                            thisx->colChkInfo.cylRadius, 0x1D);
    thisx->focus.pos = thisx->world.pos;
}

void EnLightbox_Draw(Actor* thisx, PlayState* play) {
    OoT_Gfx_DrawDListOpa(play, object_lightbox_DL_000B70);
}
