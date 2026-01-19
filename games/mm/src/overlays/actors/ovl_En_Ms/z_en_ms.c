/*
 * File: z_en_ms.c
 * Overlay: ovl_En_Ms
 * Description: Bean Seller
 */

#include "z_en_ms.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void MM_EnMs_Init(Actor* thisx, PlayState* play);
void MM_EnMs_Destroy(Actor* thisx, PlayState* play);
void MM_EnMs_Update(Actor* thisx, PlayState* play);
void MM_EnMs_Draw(Actor* thisx, PlayState* play);

void MM_EnMs_Wait(EnMs* this, PlayState* play);
void MM_EnMs_Talk(EnMs* this, PlayState* play);
void MM_EnMs_Sell(EnMs* this, PlayState* play);
void MM_EnMs_TalkAfterPurchase(EnMs* this, PlayState* play);

ActorProfile En_Ms_Profile = {
    /**/ ACTOR_EN_MS,
    /**/ ACTORCAT_NPC,
    /**/ FLAGS,
    /**/ OBJECT_MS,
    /**/ sizeof(EnMs),
    /**/ MM_EnMs_Init,
    /**/ MM_EnMs_Destroy,
    /**/ MM_EnMs_Update,
    /**/ MM_EnMs_Draw,
};

static ColliderCylinderInitType1 MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 22, 37, 0, { 0, 0, 0 } },
};

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_U8(attentionRangeType, ATTENTION_RANGE_2, ICHAIN_CONTINUE),
    ICHAIN_F32(lockOnArrowOffset, 500, ICHAIN_STOP),
};

void MM_EnMs_Init(Actor* thisx, PlayState* play) {
    EnMs* this = (EnMs*)thisx;

    MM_Actor_ProcessInitChain(thisx, MM_sInitChain);
    MM_SkelAnime_InitFlex(play, &this->skelAnime, &gBeanSalesmanSkel, &gBeanSalesmanEatingAnim, this->jointTable,
                       this->morphTable, BEAN_SALESMAN_LIMB_MAX);
    MM_Collider_InitCylinder(play, &this->collider);
    MM_Collider_SetCylinderType1(play, &this->collider, &this->actor, &MM_sCylinderInit);
    MM_ActorShape_Init(&this->actor.shape, 0.0f, MM_ActorShadow_DrawCircle, 35.0f);
    MM_Actor_SetScale(&this->actor, 0.015f);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE; // Eating Magic Beans all day will do that to you
    this->actionFunc = MM_EnMs_Wait;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -1.0f;
}

void MM_EnMs_Destroy(Actor* thisx, PlayState* play) {
    EnMs* this = (EnMs*)thisx;

    MM_Collider_DestroyCylinder(play, &this->collider);
}

void MM_EnMs_Wait(EnMs* this, PlayState* play) {
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    if (gSaveContext.save.saveInfo.inventory.items[SLOT_MAGIC_BEANS] == ITEM_NONE) {
        this->actor.textId = 0x92E;
    } else {
        this->actor.textId = 0x932;
    }

    if (Actor_TalkOfferAccepted(&this->actor, &play->state)) {
        this->actionFunc = MM_EnMs_Talk;
    } else if ((this->actor.xzDistToPlayer < 90.0f) && (ABS_ALT(yawDiff) < 0x2000)) {
        Actor_OfferTalk(&this->actor, play, 90.0f);
    }
}

void MM_EnMs_Talk(EnMs* this, PlayState* play) {
    switch (MM_Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_DONE:
            if (MM_Message_ShouldAdvance(play)) {
                this->actionFunc = MM_EnMs_Wait;
            }
            break;

        case TEXT_STATE_EVENT:
            if (MM_Message_ShouldAdvance(play)) {
                MM_Message_CloseTextbox(play);
                MM_Actor_OfferGetItem(&this->actor, play, GI_MAGIC_BEANS, this->actor.xzDistToPlayer,
                                   this->actor.playerHeightRel);
                this->actionFunc = MM_EnMs_Sell;
            }
            break;

        case TEXT_STATE_CHOICE:
            if (MM_Message_ShouldAdvance(play)) {
                switch (play->msgCtx.choiceIndex) {
                    case 0: // yes
                        MM_Message_CloseTextbox(play);
                        if (gSaveContext.save.saveInfo.playerData.rupees < 10) {
                            Audio_PlaySfx(NA_SE_SY_ERROR);
                            MM_Message_ContinueTextbox(play, 0x935);
                        } else if (AMMO(ITEM_MAGIC_BEANS) >= 20) {
                            Audio_PlaySfx(NA_SE_SY_ERROR);
                            MM_Message_ContinueTextbox(play, 0x937);
                        } else {
                            Audio_PlaySfx_MessageDecide();
                            MM_Actor_OfferGetItem(&this->actor, play, GI_MAGIC_BEANS, 90.0f, 10.0f);
                            MM_Rupees_ChangeBy(-10);
                            this->actionFunc = MM_EnMs_Sell;
                        }
                        break;

                    case 1: // no
                    default:
                        Audio_PlaySfx_MessageCancel();
                        MM_Message_ContinueTextbox(play, 0x934);
                        break;
                }
            }
            break;

        default:
            break;
    }
}

void MM_EnMs_Sell(EnMs* this, PlayState* play) {
    if (MM_Actor_HasParent(&this->actor, play)) {
        this->actor.textId = 0;
        Actor_OfferTalkExchange(&this->actor, play, this->actor.xzDistToPlayer, this->actor.playerHeightRel,
                                PLAYER_IA_NONE);
        this->actionFunc = MM_EnMs_TalkAfterPurchase;
    } else {
        MM_Actor_OfferGetItem(&this->actor, play, GI_MAGIC_BEANS, this->actor.xzDistToPlayer, this->actor.playerHeightRel);
    }
}

void MM_EnMs_TalkAfterPurchase(EnMs* this, PlayState* play) {
    if (Actor_TalkOfferAccepted(&this->actor, &play->state)) {
        MM_Message_ContinueTextbox(play, 0x936);
        this->actionFunc = MM_EnMs_Talk;
    } else {
        Actor_OfferTalkExchange(&this->actor, play, this->actor.xzDistToPlayer, this->actor.playerHeightRel,
                                PLAYER_IA_MINUS1);
    }
}

void MM_EnMs_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    EnMs* this = (EnMs*)thisx;

    MM_Actor_SetFocus(&this->actor, 20.0f);
    this->actor.lockOnArrowOffset = 500.0f;
    MM_Actor_SetScale(&this->actor, 0.015f);
    MM_SkelAnime_Update(&this->skelAnime);
    this->actionFunc(this, play);
    MM_Collider_UpdateCylinder(&this->actor, &this->collider);
    MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void MM_EnMs_Draw(Actor* thisx, PlayState* play) {
    EnMs* this = (EnMs*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MM_SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NULL, &this->actor);
}
