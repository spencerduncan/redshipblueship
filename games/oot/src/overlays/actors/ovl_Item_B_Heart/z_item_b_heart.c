/*
 * File: z_item_b_heart.c
 * Overlay: ovl_Item_B_Heart
 * Description: Heart Container
 */

#include "z_item_b_heart.h"
#include "objects/object_gi_hearts/object_gi_hearts.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS 0

void OoT_ItemBHeart_Init(Actor* thisx, PlayState* play);
void OoT_ItemBHeart_Destroy(Actor* thisx, PlayState* play);
void OoT_ItemBHeart_Update(Actor* thisx, PlayState* play);
void OoT_ItemBHeart_Draw(Actor* thisx, PlayState* play);

void func_80B85264(ItemBHeart* this, PlayState* play);

const ActorInit Item_B_Heart_InitVars = {
    ACTOR_ITEM_B_HEART,
    ACTORCAT_MISC,
    FLAGS,
    OBJECT_GI_HEARTS,
    sizeof(ItemBHeart),
    (ActorFunc)OoT_ItemBHeart_Init,
    (ActorFunc)OoT_ItemBHeart_Destroy,
    (ActorFunc)OoT_ItemBHeart_Update,
    (ActorFunc)OoT_ItemBHeart_Draw,
    NULL,
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 0, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 4000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 800, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 800, ICHAIN_STOP),
};

void OoT_ItemBHeart_Init(Actor* thisx, PlayState* play) {
    ItemBHeart* this = (ItemBHeart*)thisx;

    if (GameInteractor_Should(VB_ITEM_B_HEART_DESPAWN, OoT_Flags_GetCollectible(play, 0x1F), this)) {
        OoT_Actor_Kill(&this->actor);
    } else {
        OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
        OoT_ActorShape_Init(&this->actor.shape, 0.0f, NULL, 0.8f);
    }
}

void OoT_ItemBHeart_Destroy(Actor* thisx, PlayState* play) {
}

void OoT_ItemBHeart_Update(Actor* thisx, PlayState* play) {
    ItemBHeart* this = (ItemBHeart*)thisx;

    func_80B85264(this, play);
    OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, 4);
    if (OoT_Actor_HasParent(&this->actor, play)) {
        OoT_Flags_SetCollectible(play, 0x1F);
        OoT_Actor_Kill(&this->actor);
    } else {
        OoT_Actor_OfferGetItem(&this->actor, play, GI_HEART_CONTAINER_2, 30.0f, 40.0f);
    }
}

void func_80B85264(ItemBHeart* this, PlayState* play) {
    f32 yOffset;

    this->unk_164++;
    yOffset = (OoT_Math_SinS(this->unk_164 * 0x60C) * 5.0f) + 20.0f;
    OoT_Math_ApproachF(&this->actor.world.pos.y, this->actor.home.pos.y + yOffset, 0.1f, this->unk_158);
    OoT_Math_ApproachF(&this->unk_158, 2.0f, 1.0f, 0.1f);
    this->actor.shape.rot.y += 0x400;

    OoT_Math_ApproachF(&this->actor.scale.x, 0.4f, 0.1f, 0.01f);
    this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;
}

void OoT_ItemBHeart_Draw(Actor* thisx, PlayState* play) {
    ItemBHeart* this = (ItemBHeart*)thisx;
    Actor* actorIt;
    u8 flag = false;

    OPEN_DISPS(play->state.gfxCtx);

    actorIt = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head;

    while (actorIt != NULL) {
        if ((actorIt->id == ACTOR_DOOR_WARP1) && (actorIt->projectedPos.z > this->actor.projectedPos.z)) {
            flag = true;
            break;
        }
        actorIt = actorIt->next;
    }

    if (flag) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gGiHeartBorderDL);
        gSPDisplayList(POLY_XLU_DISP++, gGiHeartContainerDL);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gGiHeartBorderDL);
        gSPDisplayList(POLY_OPA_DISP++, gGiHeartContainerDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
