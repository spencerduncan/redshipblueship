/*
 * File: z_en_ds.c
 * Overlay: ovl_En_Ds
 * Description: Potion Shop Granny
 */

#include "z_en_ds.h"
#include "objects/object_ds/object_ds.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void EnDs_Init(Actor* thisx, PlayState* play);
void EnDs_Destroy(Actor* thisx, PlayState* play);
void EnDs_Update(Actor* thisx, PlayState* play);
void EnDs_Draw(Actor* thisx, PlayState* play);

void EnDs_Wait(EnDs* this, PlayState* play);

const ActorInit En_Ds_InitVars = {
    ACTOR_EN_DS,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_DS,
    sizeof(EnDs),
    (ActorFunc)EnDs_Init,
    (ActorFunc)EnDs_Destroy,
    (ActorFunc)EnDs_Update,
    (ActorFunc)EnDs_Draw,
    NULL,
};

void EnDs_Init(Actor* thisx, PlayState* play) {
    EnDs* this = (EnDs*)thisx;

    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 36.0f);
    OoT_SkelAnime_InitFlex(play, &this->skelAnime, &gPotionShopLadySkel, &gPotionShopLadyAnim, this->jointTable,
                       this->morphTable, 6);
    OoT_Animation_PlayOnce(&this->skelAnime, &gPotionShopLadyAnim);

    this->actor.colChkInfo.mass = MASS_IMMOVABLE;

    OoT_Actor_SetScale(&this->actor, 0.013f);

    this->actionFunc = EnDs_Wait;
    this->actor.targetMode = 1;
    this->unk_1E8 = 0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->unk_1E4 = 0.0f;
}

void EnDs_Destroy(Actor* thisx, PlayState* play) {
    EnDs* this = (EnDs*)thisx;

    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

void EnDs_Talk(EnDs* this, PlayState* play) {
    if (OoT_Actor_TextboxIsClosing(&this->actor, play)) {
        this->actionFunc = EnDs_Wait;
        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    }
    this->unk_1E8 |= 1;
}

void EnDs_TalkNoEmptyBottle(EnDs* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && OoT_Message_ShouldAdvance(play)) {
        OoT_Message_CloseTextbox(play);
        this->actionFunc = EnDs_Wait;
    }
    this->unk_1E8 |= 1;
}

void EnDs_TalkAfterGiveOddPotion(EnDs* this, PlayState* play) {
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = EnDs_Talk;
    } else {
        this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        func_8002F2CC(&this->actor, play, 1000.0f);
    }
}

void EnDs_DisplayOddPotionText(EnDs* this, PlayState* play) {
    if (OoT_Actor_TextboxIsClosing(&this->actor, play)) {
        this->actor.textId = 0x504F;
        this->actionFunc = EnDs_TalkAfterGiveOddPotion;
        this->actor.flags &= ~ACTOR_FLAG_TALK;
        Flags_SetItemGetInf(ITEMGETINF_30);
    }
}

void EnDs_GiveOddPotion(EnDs* this, PlayState* play) {
    if (OoT_Actor_HasParent(&this->actor, play) || !GameInteractor_Should(VB_TRADE_ODD_MUSHROOM, true, this)) {
        this->actor.parent = NULL;
        this->actionFunc = EnDs_DisplayOddPotionText;
        gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    } else {
        OoT_Actor_OfferGetItem(&this->actor, play, GI_ODD_POTION, 10000.0f, 50.0f);
    }
}

void EnDs_TalkAfterBrewOddPotion(EnDs* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && OoT_Message_ShouldAdvance(play)) {
        OoT_Message_CloseTextbox(play);
        this->actionFunc = EnDs_GiveOddPotion;
        u32 itemId = GI_ODD_POTION;
        if (GameInteractor_Should(VB_TRADE_ODD_MUSHROOM, true, this)) {
            OoT_Actor_OfferGetItem(&this->actor, play, itemId, 10000.0f, 50.0f);
        }
    }
}

void EnDs_BrewOddPotion3(EnDs* this, PlayState* play) {
    if (this->brewTimer > 0) {
        this->brewTimer -= 1;
    } else {
        this->actionFunc = EnDs_TalkAfterBrewOddPotion;
        OoT_Message_ContinueTextbox(play, 0x504D);
    }

    OoT_Math_StepToF(&this->unk_1E4, 0, 0.03f);
    OoT_Environment_AdjustLights(play, this->unk_1E4 * (2.0f - this->unk_1E4), 0.0f, 0.1f, 1.0f);
}

void EnDs_BrewOddPotion2(EnDs* this, PlayState* play) {
    if (this->brewTimer > 0) {
        this->brewTimer -= 1;
    } else {
        this->actionFunc = EnDs_BrewOddPotion3;
        this->brewTimer = GameInteractor_Should(VB_PLAY_EYEDROP_CREATION_ANIM, true, this) ? 60 : 0;
        OoT_Flags_UnsetSwitch(play, 0x3F);
    }
}

void EnDs_BrewOddPotion1(EnDs* this, PlayState* play) {
    if (this->brewTimer > 0) {
        this->brewTimer -= 1;
    } else {
        this->actionFunc = EnDs_BrewOddPotion2;
        this->brewTimer = GameInteractor_Should(VB_PLAY_EYEDROP_CREATION_ANIM, true, this) ? 20 : 0;
    }

    OoT_Math_StepToF(&this->unk_1E4, 1.0f, 0.01f);
    OoT_Environment_AdjustLights(play, this->unk_1E4 * (2.0f - this->unk_1E4), 0.0f, 0.1f, 1.0f);
}

void EnDs_OfferOddPotion(EnDs* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && OoT_Message_ShouldAdvance(play)) {
        switch (play->msgCtx.choiceIndex) {
            case 0: // yes
                this->actionFunc = EnDs_BrewOddPotion1;
                this->brewTimer = GameInteractor_Should(VB_PLAY_EYEDROP_CREATION_ANIM, true, this) ? 60 : 0;
                OoT_Flags_SetSwitch(play, 0x3F);
                play->msgCtx.msgMode = MSGMODE_PAUSED;
                player->exchangeItemId = EXCH_ITEM_NONE;
                break;
            case 1: // no
                OoT_Message_ContinueTextbox(play, 0x504C);
                this->actionFunc = EnDs_Talk;
        }
    }
}

s32 EnDs_CheckRupeesAndBottle() {
    if (GameInteractor_Should(VB_GRANNY_SAY_INSUFFICIENT_RUPEES, gSaveContext.rupees < 100, NULL)) {
        return 0;
    } else if (GameInteractor_Should(VB_NEED_BOTTLE_FOR_GRANNYS_ITEM, OoT_Inventory_HasEmptyBottle() == 0)) {
        return 1;
    } else {
        return 2;
    }
}

void EnDs_GiveBluePotion(EnDs* this, PlayState* play) {
    if (OoT_Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->actionFunc = EnDs_Talk;
    } else {
        OoT_Actor_OfferGetItem(&this->actor, play, GI_POTION_BLUE, 10000.0f, 50.0f);
    }
}

void EnDs_OfferBluePotion(EnDs* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && OoT_Message_ShouldAdvance(play)) {
        switch (play->msgCtx.choiceIndex) {
            case 0: // yes
                switch (EnDs_CheckRupeesAndBottle()) {
                    case 0: // have less than 100 rupees
                        OoT_Message_ContinueTextbox(play, 0x500E);
                        break;
                    case 1: // have 100 rupees but no empty bottle
                        OoT_Message_ContinueTextbox(play, 0x96);
                        this->actionFunc = EnDs_TalkNoEmptyBottle;
                        return;
                    case 2: // have 100 rupees and empty bottle
                        if (GameInteractor_Should(VB_GRANNY_TAKE_MONEY, true, this)) {
                            OoT_Rupees_ChangeBy(-100);
                        }
                        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;

                        if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GRANNYS_SHOP, true, this)) {
                            GetItemEntry itemEntry = ItemTable_Retrieve(GI_POTION_BLUE);
                            OoT_Actor_OfferGetItem(&this->actor, play, GI_POTION_BLUE, 10000.0f, 50.0f);
                            gSaveContext.ship.pendingSale = itemEntry.itemId;
                            gSaveContext.ship.pendingSaleMod = itemEntry.modIndex;
                            this->actionFunc = EnDs_GiveBluePotion;
                        }

                        return;
                }
                break;
            case 1: // no
                OoT_Message_ContinueTextbox(play, 0x500D);
        }
        this->actionFunc = EnDs_Talk;
    }
}

void EnDs_Wait(EnDs* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yawDiff;

    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        if (func_8002F368(play) == EXCH_ITEM_ODD_MUSHROOM) {
            Audio_PlaySoundGeneral(NA_SE_SY_TRE_BOX_APPEAR, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                   &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
            player->actor.textId = 0x504A;
            this->actionFunc = EnDs_OfferOddPotion;
        } else if (GameInteractor_Should(VB_OFFER_BLUE_POTION, Flags_GetItemGetInf(ITEMGETINF_30),
                                         this)) { // Traded odd mushroom
            player->actor.textId = 0x500C;
            this->actionFunc = EnDs_OfferBluePotion;
        } else {
            if (INV_CONTENT(ITEM_ODD_MUSHROOM) == ITEM_ODD_MUSHROOM) {
                player->actor.textId = 0x5049;
            } else {
                player->actor.textId = 0x5048;
            }
            this->actionFunc = EnDs_Talk;
        }
    } else {
        yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
        this->actor.textId = 0x5048;

        if ((ABS(yawDiff) < 0x2151) && (this->actor.xzDistToPlayer < 200.0f)) {
            func_8002F298(&this->actor, play, 100.0f, EXCH_ITEM_ODD_MUSHROOM);
            this->unk_1E8 |= 1;
        }
    }
}

void EnDs_Update(Actor* thisx, PlayState* play) {
    EnDs* this = (EnDs*)thisx;

    if (OoT_SkelAnime_Update(&this->skelAnime) != 0) {
        this->skelAnime.curFrame = 0.0f;
    }

    this->actionFunc(this, play);

    if (this->unk_1E8 & 1) {
        func_80038290(play, &this->actor, &this->unk_1D8, &this->unk_1DE, this->actor.focus.pos);
    } else {
        OoT_Math_SmoothStepToS(&this->unk_1D8.x, 0, 6, 0x1838, 100);
        OoT_Math_SmoothStepToS(&this->unk_1D8.y, 0, 6, 0x1838, 100);
        OoT_Math_SmoothStepToS(&this->unk_1DE.x, 0, 6, 0x1838, 100);
        OoT_Math_SmoothStepToS(&this->unk_1DE.y, 0, 6, 0x1838, 100);
    }
}

s32 EnDs_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnDs* this = (EnDs*)thisx;

    if (limbIndex == 5) {
        rot->x += this->unk_1D8.y;
        rot->z += this->unk_1D8.x;
    }
    return false;
}

void EnDs_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sMultVec = { 1100.0f, 500.0f, 0.0f };
    EnDs* this = (EnDs*)thisx;

    if (limbIndex == 5) {
        OoT_Matrix_MultVec3f(&sMultVec, &this->actor.focus.pos);
    }
}

void EnDs_Draw(Actor* thisx, PlayState* play) {
    EnDs* this = (EnDs*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, EnDs_OverrideLimbDraw, EnDs_PostLimbDraw, this);
}
