/*
 * File: z_bg_spot05_soko.c
 * Overlay: ovl_Bg_Spot05_Soko
 * Description: Sacred Forest Meadow Entities
 */

#include "z_bg_spot05_soko.h"
#include "objects/object_spot05_objects/object_spot05_objects.h"

#define FLAGS 0

void BgSpot05Soko_Init(Actor* thisx, PlayState* play);
void BgSpot05Soko_Destroy(Actor* thisx, PlayState* play);
void BgSpot05Soko_Update(Actor* thisx, PlayState* play);
void BgSpot05Soko_Draw(Actor* thisx, PlayState* play);
void func_808AE5A8(BgSpot05Soko* this, PlayState* play);
void func_808AE5B4(BgSpot05Soko* this, PlayState* play);
void func_808AE630(BgSpot05Soko* this, PlayState* play);

const ActorInit Bg_Spot05_Soko_InitVars = {
    ACTOR_BG_SPOT05_SOKO,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_SPOT05_OBJECTS,
    sizeof(BgSpot05Soko),
    (ActorFunc)BgSpot05Soko_Init,
    (ActorFunc)BgSpot05Soko_Destroy,
    (ActorFunc)BgSpot05Soko_Update,
    (ActorFunc)BgSpot05Soko_Draw,
    NULL,
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_STOP),
};

static Gfx* OoT_sDLists[] = {
    object_spot05_objects_DL_000840,
    object_spot05_objects_DL_001190,
};

void BgSpot05Soko_Init(Actor* thisx, PlayState* play) {
    s32 pad1;
    BgSpot05Soko* this = (BgSpot05Soko*)thisx;
    CollisionHeader* colHeader = NULL;
    s32 pad2;

    OoT_Actor_ProcessInitChain(thisx, OoT_sInitChain);
    this->switchFlag = (thisx->params >> 8) & 0xFF;
    thisx->params &= 0xFF;
    OoT_DynaPolyActor_Init(&this->dyna, DPM_UNK);
    if (thisx->params == 0) {
        OoT_CollisionHeader_GetVirtual(&object_spot05_objects_Col_000918, &colHeader);
        if (LINK_IS_ADULT) {
            OoT_Actor_Kill(thisx);
        } else {
            this->actionFunc = func_808AE5A8;
        }
    } else {
        OoT_CollisionHeader_GetVirtual(&object_spot05_objects_Col_0012C0, &colHeader);
        if (OoT_Flags_GetSwitch(play, this->switchFlag) != 0) {
            OoT_Actor_Kill(thisx);
        } else {
            this->actionFunc = func_808AE5B4;
            thisx->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        }
    }
    this->dyna.bgId = OoT_DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
}

void BgSpot05Soko_Destroy(Actor* thisx, PlayState* play) {
    BgSpot05Soko* this = (BgSpot05Soko*)thisx;

    OoT_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void func_808AE5A8(BgSpot05Soko* this, PlayState* play) {
}

void func_808AE5B4(BgSpot05Soko* this, PlayState* play) {
    if (OoT_Flags_GetSwitch(play, this->switchFlag)) {
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->dyna.actor.world.pos, 30, NA_SE_EV_METALDOOR_CLOSE);
        OoT_Actor_SetFocus(&this->dyna.actor, 50.0f);
        OnePointCutscene_Attention(play, &this->dyna.actor);
        this->actionFunc = func_808AE630;
        this->dyna.actor.speedXZ = 0.5f;
    }
}

void func_808AE630(BgSpot05Soko* this, PlayState* play) {
    this->dyna.actor.speedXZ *= 1.5f;
    if (OoT_Math_StepToF(&this->dyna.actor.world.pos.y, this->dyna.actor.home.pos.y - 120.0f, this->dyna.actor.speedXZ) !=
        0) {
        OoT_Actor_Kill(&this->dyna.actor);
    }
}

void BgSpot05Soko_Update(Actor* thisx, PlayState* play) {
    BgSpot05Soko* this = (BgSpot05Soko*)thisx;

    this->actionFunc(this, play);
}

void BgSpot05Soko_Draw(Actor* thisx, PlayState* play) {
    OoT_Gfx_DrawDListOpa(play, OoT_sDLists[thisx->params]);
}
