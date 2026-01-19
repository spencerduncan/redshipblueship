/*
 * File: z_en_hs.c
 * Overlay: ovl_En_Hs
 * Description: Carpenter's Son
 */

#include "z_en_hs.h"
#include "vt.h"
#include "objects/object_hs/object_hs.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void OoT_EnHs_Init(Actor* thisx, PlayState* play);
void OoT_EnHs_Destroy(Actor* thisx, PlayState* play);
void OoT_EnHs_Update(Actor* thisx, PlayState* play);
void OoT_EnHs_Draw(Actor* thisx, PlayState* play);

void func_80A6E9AC(EnHs* this, PlayState* play);
void func_80A6E6B0(EnHs* this, PlayState* play);

const ActorInit En_Hs_InitVars = {
    ACTOR_EN_HS,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_HS,
    sizeof(EnHs),
    (ActorFunc)OoT_EnHs_Init,
    (ActorFunc)OoT_EnHs_Destroy,
    (ActorFunc)OoT_EnHs_Update,
    (ActorFunc)OoT_EnHs_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_ENEMY,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 40, 40, 0, { 0, 0, 0 } },
};

void func_80A6E3A0(EnHs* this, EnHsActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_EnHs_Init(Actor* thisx, PlayState* play) {
    EnHs* this = (EnHs*)thisx;
    s32 pad;

    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 36.0f);
    OoT_SkelAnime_InitFlex(play, &this->skelAnime, &object_hs_Skel_006260, &object_hs_Anim_0005C0, this->jointTable,
                       this->morphTable, 16);
    OoT_Animation_PlayLoop(&this->skelAnime, &object_hs_Anim_0005C0);
    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    OoT_Actor_SetScale(&this->actor, 0.01f);

    if (!LINK_IS_ADULT) {
        this->actor.params = 0;
    } else {
        this->actor.params = 1;
    }

    if (this->actor.params == 1) {
        // "chicken shop (adult era)"
        osSyncPrintf(VT_FGCOL(CYAN) " ヒヨコの店(大人の時) \n" VT_RST);
        func_80A6E3A0(this, func_80A6E9AC);
        if (GameInteractor_Should(VB_DESPAWN_GROG, Flags_GetItemGetInf(ITEMGETINF_30), this)) {
            // "chicken shop closed"
            osSyncPrintf(VT_FGCOL(CYAN) " ヒヨコ屋閉店 \n" VT_RST);
            OoT_Actor_Kill(&this->actor);
        }
    } else {
        // "chicken shop (child era)"
        osSyncPrintf(VT_FGCOL(CYAN) " ヒヨコの店(子人の時) \n" VT_RST);
        func_80A6E3A0(this, func_80A6E9AC);
    }

    this->unk_2A8 = 0;
    this->actor.targetMode = 6;
}

void OoT_EnHs_Destroy(Actor* thisx, PlayState* play) {
    EnHs* this = (EnHs*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);

    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

s32 func_80A6E53C(EnHs* this, PlayState* play, u16 textId, EnHsActionFunc actionFunc) {
    s16 yawDiff;

    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        func_80A6E3A0(this, actionFunc);
        return 1;
    }

    this->actor.textId = textId;
    yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    if ((ABS(yawDiff) <= 0x2150) && (this->actor.xzDistToPlayer < 100.0f)) {
        this->unk_2A8 |= 1;
        func_8002F2CC(&this->actor, play, 100.0f);
    }

    return 0;
}

void func_80A6E5EC(EnHs* this, PlayState* play) {
    if (OoT_Actor_TextboxIsClosing(&this->actor, play)) {
        func_80A6E3A0(this, func_80A6E6B0);
    }

    this->unk_2A8 |= 1;
}

void func_80A6E630(EnHs* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) && OoT_Message_ShouldAdvance(play)) {
        if (GameInteractor_Should(VB_TRADE_TIMER_ODD_MUSHROOM, true)) {
            Interface_SetSubTimer(180);
            gSaveContext.eventInf[1] &= ~1;
        }
        func_80A6E3A0(this, func_80A6E6B0);
    }

    this->unk_2A8 |= 1;
}

void func_80A6E6B0(EnHs* this, PlayState* play) {
    func_80A6E53C(this, play, 0x10B6, func_80A6E5EC);
}

void func_80A6E6D8(EnHs* this, PlayState* play) {
    if (OoT_Actor_TextboxIsClosing(&this->actor, play)) {
        func_80A6E3A0(this, func_80A6E9AC);
    }
}

void func_80A6E70C(EnHs* this, PlayState* play) {
    if (OoT_Actor_TextboxIsClosing(&this->actor, play)) {
        func_80A6E3A0(this, func_80A6E9AC);
    }
}

void func_80A6E740(EnHs* this, PlayState* play) {
    if (OoT_Actor_HasParent(&this->actor, play) || !GameInteractor_Should(VB_TRADE_COJIRO, true, this)) {
        this->actor.parent = NULL;
        Flags_SetRandomizerInf(RAND_INF_ADULT_TRADES_LW_TRADE_COJIRO);
        func_80A6E3A0(this, func_80A6E630);
    } else {
        OoT_Actor_OfferGetItem(&this->actor, play, GI_ODD_MUSHROOM, 10000.0f, 50.0f);
    }

    this->unk_2A8 |= 1;
}

void func_80A6E7BC(EnHs* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && OoT_Message_ShouldAdvance(play)) {
        switch (play->msgCtx.choiceIndex) {
            case 0:
                func_80A6E3A0(this, func_80A6E740);
                if (GameInteractor_Should(VB_TRADE_COJIRO, true, this)) {
                    OoT_Actor_OfferGetItem(&this->actor, play, GI_ODD_MUSHROOM, 10000.0f, 50.0f);
                }
                break;
            case 1:
                OoT_Message_ContinueTextbox(play, 0x10B4);
                func_80A6E3A0(this, func_80A6E70C);
                break;
        }

        OoT_Animation_Change(&this->skelAnime, &object_hs_Anim_0005C0, 1.0f, 0.0f,
                         OoT_Animation_GetLastFrame(&object_hs_Anim_0005C0), ANIMMODE_LOOP, 8.0f);
    }

    this->unk_2A8 |= 1;
}

void func_80A6E8CC(EnHs* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && OoT_Message_ShouldAdvance(play)) {
        OoT_Message_ContinueTextbox(play, 0x10B3);
        func_80A6E3A0(this, func_80A6E7BC);
        OoT_Animation_Change(&this->skelAnime, &object_hs_Anim_000528, 1.0f, 0.0f,
                         OoT_Animation_GetLastFrame(&object_hs_Anim_000528), ANIMMODE_LOOP, 8.0f);
    }

    if (this->unk_2AA > 0) {
        this->unk_2AA--;
        if (this->unk_2AA == 0) {
            OoT_Player_PlaySfx(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
        }
    }

    this->unk_2A8 |= 1;
}

void func_80A6E9AC(EnHs* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yawDiff;

    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        if (func_8002F368(play) == 7) {
            player->actor.textId = 0x10B2;
            func_80A6E3A0(this, func_80A6E8CC);
            OoT_Animation_Change(&this->skelAnime, &object_hs_Anim_000304, 1.0f, 0.0f,
                             OoT_Animation_GetLastFrame(&object_hs_Anim_000304), ANIMMODE_LOOP, 8.0f);
            this->unk_2AA = 40;
            Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
        } else {
            player->actor.textId = 0x10B1;
            func_80A6E3A0(this, func_80A6E6D8);
        }
    } else {
        yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
        this->actor.textId = 0x10B1;
        if ((ABS(yawDiff) <= 0x2150) && (this->actor.xzDistToPlayer < 100.0f)) {
            func_8002F298(&this->actor, play, 100.0f, 7);
        }
    }
}

void OoT_EnHs_Update(Actor* thisx, PlayState* play) {
    EnHs* this = (EnHs*)thisx;
    s32 pad;

    OoT_Collider_UpdateCylinder(thisx, &this->collider);
    OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_MoveXZGravity(&this->actor);
    OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, 4);
    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        this->skelAnime.curFrame = 0.0f;
    }

    this->actionFunc(this, play);

    if (this->unk_2A8 & 1) {
        func_80038290(play, &this->actor, &this->unk_29C, &this->unk_2A2, this->actor.focus.pos);
        this->unk_2A8 &= ~1;
    } else {
        OoT_Math_SmoothStepToS(&this->unk_29C.x, 12800, 6, 6200, 100);
        OoT_Math_SmoothStepToS(&this->unk_29C.y, 0, 6, 6200, 100);
        OoT_Math_SmoothStepToS(&this->unk_2A2.x, 0, 6, 6200, 100);
        OoT_Math_SmoothStepToS(&this->unk_2A2.y, 0, 6, 6200, 100);
    }
}

s32 OoT_EnHs_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnHs* this = (EnHs*)thisx;

    switch (limbIndex) {
        case 9:
            rot->x += this->unk_29C.y;
            rot->z += this->unk_29C.x;
            break;
        case 10:
            *dList = NULL;
            return false;
        case 11:
            *dList = NULL;
            return false;
        case 12:
            if (this->actor.params == 1) {
                *dList = NULL;
                return false;
            }
            break;
        case 13:
            if (this->actor.params == 1) {
                *dList = NULL;
                return false;
            }
            break;
    }
    return false;
}

void OoT_EnHs_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f D_80A6EDFC = { 300.0f, 1000.0f, 0.0f };
    EnHs* this = (EnHs*)thisx;

    if (limbIndex == 9) {
        OoT_Matrix_MultVec3f(&D_80A6EDFC, &this->actor.focus.pos);
    }
}

void OoT_EnHs_Draw(Actor* thisx, PlayState* play) {
    EnHs* this = (EnHs*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, OoT_EnHs_OverrideLimbDraw, OoT_EnHs_PostLimbDraw, this);
}
