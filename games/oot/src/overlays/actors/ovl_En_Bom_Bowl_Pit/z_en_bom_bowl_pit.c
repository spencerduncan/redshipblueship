#include "z_en_bom_bowl_pit.h"
#include "vt.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
#include "overlays/actors/ovl_En_Ex_Item/z_en_ex_item.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void EnBomBowlPit_Init(Actor* thisx, PlayState* play);
void EnBomBowlPit_Destroy(Actor* thisx, PlayState* play);
void EnBomBowlPit_Update(Actor* thisx, PlayState* play);

void EnBomBowlPit_SetupDetectHit(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_DetectHit(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_CameraDollyIn(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_SpawnPrize(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_SetupGivePrize(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_GivePrize(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_WaitTillPrizeGiven(EnBomBowlPit* this, PlayState* play);
void EnBomBowlPit_Reset(EnBomBowlPit* this, PlayState* play);

static s32 OoT_sGetItemIds[] = { GI_BOMB_BAG_30, GI_HEART_PIECE, GI_BOMBCHUS_10, GI_BOMBS_1, GI_RUPEE_PURPLE };

const ActorInit En_Bom_Bowl_Pit_InitVars = {
    ACTOR_EN_BOM_BOWL_PIT,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnBomBowlPit),
    (ActorFunc)EnBomBowlPit_Init,
    (ActorFunc)EnBomBowlPit_Destroy,
    (ActorFunc)EnBomBowlPit_Update,
    NULL,
    NULL,
};

void EnBomBowlPit_Init(Actor* thisx, PlayState* play) {
    EnBomBowlPit* this = (EnBomBowlPit*)thisx;

    this->actionFunc = EnBomBowlPit_SetupDetectHit;
}

void EnBomBowlPit_Destroy(Actor* thisx, PlayState* play) {
}

void EnBomBowlPit_SetupDetectHit(EnBomBowlPit* this, PlayState* play) {
    if (this->start != 0) {
        this->start = this->status = 0;
        this->actionFunc = EnBomBowlPit_DetectHit;
    }
}

void EnBomBowlPit_DetectHit(EnBomBowlPit* this, PlayState* play) {
    EnBomChu* chu;
    Vec3f chuPosDiff;

    if (play->cameraPtrs[MAIN_CAM]->setting == CAM_SET_CHU_BOWLING) {
        chu = (EnBomChu*)play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;

        while (chu != NULL) {
            if ((&chu->actor == &this->actor) || (chu->actor.id != ACTOR_EN_BOM_CHU)) {
                chu = (EnBomChu*)chu->actor.next;
                continue;
            }

            chuPosDiff.x = chu->actor.world.pos.x - this->actor.world.pos.x;
            chuPosDiff.y = chu->actor.world.pos.y - this->actor.world.pos.y;
            chuPosDiff.z = chu->actor.world.pos.z - this->actor.world.pos.z;

            if (((fabsf(chuPosDiff.x) < 40.0f) || (BREG(2))) && ((fabsf(chuPosDiff.y) < 40.0f) || (BREG(2))) &&
                ((fabsf(chuPosDiff.z) < 40.0f) || (BREG(2)))) {
                OoT_Player_SetCsActionWithHaltedActors(play, NULL, 8);
                chu->timer = 1;

                this->camId = OoT_Play_CreateSubCamera(play);
                OoT_Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
                OoT_Play_ChangeCameraStatus(play, this->camId, CAM_STAT_ACTIVE);

                this->unk_1C8.x = this->unk_1C8.y = this->unk_1C8.z = 0.1f;
                this->unk_1A4.x = this->unk_1A4.y = this->unk_1A4.z = 0.1f;

                this->unk_180.x = this->unk_168.x = play->view.lookAt.x;
                this->unk_180.y = this->unk_168.y = play->view.lookAt.y;
                this->unk_180.z = this->unk_168.z = play->view.lookAt.z;

                this->unk_18C.x = this->unk_174.x = play->view.eye.x;
                this->unk_18C.y = this->unk_174.y = play->view.eye.y;
                this->unk_18C.z = this->unk_174.z = play->view.eye.z;

                this->unk_1BC.x = 20.0f;
                this->unk_1BC.y = 100.0f;
                this->unk_1BC.z = -800.0f;

                this->unk_198.x = 20.0f;
                this->unk_198.y = 50.0f;
                this->unk_198.z = -485.0f;

                this->unk_1B0.x = fabsf(this->unk_18C.x - this->unk_198.x) * 0.02f;
                this->unk_1B0.y = fabsf(this->unk_18C.y - this->unk_198.y) * 0.02f;
                this->unk_1B0.z = fabsf(this->unk_18C.z - this->unk_198.z) * 0.02f;

                this->unk_1D4.x = fabsf(this->unk_180.x - this->unk_1BC.x) * 0.02f;
                this->unk_1D4.y = fabsf(this->unk_180.y - this->unk_1BC.y) * 0.02f;
                this->unk_1D4.z = fabsf(this->unk_180.z - this->unk_1BC.z) * 0.02f;

                Play_CameraSetAtEye(play, this->camId, &this->unk_180, &this->unk_18C);
                this->actor.textId = 0xF;
                OoT_Message_StartTextbox(play, this->actor.textId, NULL);
                this->unk_154 = TEXT_STATE_EVENT;
                Sfx_PlaySfxCentered(NA_SE_EV_HIT_SOUND);
                OoT_Player_SetCsActionWithHaltedActors(play, NULL, 8);
                this->status = 1;
                this->actionFunc = EnBomBowlPit_CameraDollyIn;
                break;
            } else {
                chu = (EnBomChu*)chu->actor.next;
            }
        }
    }
}

void EnBomBowlPit_CameraDollyIn(EnBomBowlPit* this, PlayState* play) {
    if (this->camId != SUBCAM_FREE) {
        OoT_Math_ApproachF(&this->unk_180.x, this->unk_1BC.x, this->unk_1C8.x, this->unk_1D4.x);
        OoT_Math_ApproachF(&this->unk_180.y, this->unk_1BC.y, this->unk_1C8.y, this->unk_1D4.y);
        OoT_Math_ApproachF(&this->unk_180.z, this->unk_1BC.z, this->unk_1C8.z, this->unk_1D4.z);
        OoT_Math_ApproachF(&this->unk_18C.x, this->unk_198.x, this->unk_1A4.x, this->unk_1B0.x);
        OoT_Math_ApproachF(&this->unk_18C.y, this->unk_198.y, this->unk_1A4.y, this->unk_1B0.y);
        OoT_Math_ApproachF(&this->unk_18C.z, this->unk_198.z, this->unk_1A4.z, this->unk_1B0.z);
    }

    Play_CameraSetAtEye(play, this->camId, &this->unk_180, &this->unk_18C);

    if ((this->unk_154 == OoT_Message_GetState(&play->msgCtx)) && OoT_Message_ShouldAdvance(play)) {
        OoT_Message_CloseTextbox(play);
    }

    if ((fabsf(this->unk_18C.x - this->unk_198.x) < 5.0f) && (fabsf(this->unk_18C.y - this->unk_198.y) < 5.0f) &&
        (fabsf(this->unk_18C.z - this->unk_198.z) < 5.0f) && (fabsf(this->unk_180.x - this->unk_1BC.x) < 5.0f) &&
        (fabsf(this->unk_180.y - this->unk_1BC.y) < 5.0f) && (fabsf(this->unk_180.z - this->unk_1BC.z) < 5.0f)) {
        OoT_Message_CloseTextbox(play);
        this->timer = 30;
        this->actionFunc = EnBomBowlPit_SpawnPrize;
    }
}

void EnBomBowlPit_SpawnPrize(EnBomBowlPit* this, PlayState* play) {
    if (this->timer == 0) {
        this->exItem = (EnExItem*)OoT_Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_EX_ITEM,
                                                     this->actor.world.pos.x, this->actor.world.pos.y,
                                                     this->actor.world.pos.z - 70.0f, 0, 0, 0, this->prizeIndex);
        if (this->exItem != NULL) {
            this->actionFunc = EnBomBowlPit_SetupGivePrize;
        }
    }
}

void EnBomBowlPit_SetupGivePrize(EnBomBowlPit* this, PlayState* play) {
    if (this->exItemDone != 0) {
        switch (this->prizeIndex) {
            case EXITEM_BOMB_BAG_BOWLING:
                Flags_SetItemGetInf(ITEMGETINF_11);
                break;
            case EXITEM_HEART_PIECE_BOWLING:
                Flags_SetItemGetInf(ITEMGETINF_12);
                break;
        }

        OoT_Play_ClearCamera(play, this->camId);
        OoT_Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_ACTIVE);
        OoT_Player_SetCsActionWithHaltedActors(play, NULL, 8);
        this->actionFunc = EnBomBowlPit_GivePrize;
    }
}

void EnBomBowlPit_GivePrize(EnBomBowlPit* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    OoT_Player_SetCsActionWithHaltedActors(play, NULL, 7);
    this->getItemId = OoT_sGetItemIds[this->prizeIndex];

    if ((this->getItemId == GI_BOMB_BAG_30) && (CUR_CAPACITY(UPG_BOMB_BAG) == 30)) {
        this->getItemId = GI_BOMB_BAG_40;
    }

    player->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
    this->actor.parent = NULL;
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_BOMBCHU_BOWLING, true, this)) {
        OoT_Actor_OfferGetItem(&this->actor, play, this->getItemId, 2000.0f, 1000.0f);
    }
    player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
    this->actionFunc = EnBomBowlPit_WaitTillPrizeGiven;
}

void EnBomBowlPit_WaitTillPrizeGiven(EnBomBowlPit* this, PlayState* play) {
    if (OoT_Actor_HasParent(&this->actor, play) || !GameInteractor_Should(VB_GIVE_ITEM_FROM_BOMBCHU_BOWLING, true, this)) {
        this->actionFunc = EnBomBowlPit_Reset;
    } else {
        OoT_Actor_OfferGetItem(&this->actor, play, this->getItemId, 2000.0f, 1000.0f);
    }
}

void EnBomBowlPit_Reset(EnBomBowlPit* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) && OoT_Message_ShouldAdvance(play)) {
        // "Normal termination"/"completion"
        osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ 正常終了 ☆☆☆☆☆ \n" VT_RST);
        if (this->getItemId == GI_HEART_PIECE) {
            gSaveContext.healthAccumulator = MAX_HEALTH;
            // "Ah recovery!" (?)
            osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ あぁ回復！ ☆☆☆☆☆ \n" VT_RST);
        }
        this->exItemDone = 0;
        this->status = 2;
        this->actionFunc = EnBomBowlPit_SetupDetectHit;
    }
}

void EnBomBowlPit_Update(Actor* thisx, PlayState* play) {
    EnBomBowlPit* this = (EnBomBowlPit*)thisx;

    this->actionFunc(this, play);

    if (this->timer != 0) {
        this->timer--;
    }
}
