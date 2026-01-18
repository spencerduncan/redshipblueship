/*
 * File: z_bg_lbfshot.c
 * Overlay: ovl_Bg_Lbfshot
 * Description: Rainbow Hookshot Pillar
 */

#include "z_bg_lbfshot.h"
#include "objects/object_lbfshot/object_lbfshot.h"

#define FLAGS 0x00000000

void BgLbfshot_Init(Actor* thisx, PlayState* play);
void BgLbfshot_Destroy(Actor* thisx, PlayState* play);
void BgLbfshot_Draw(Actor* thisx, PlayState* play);

ActorProfile Bg_Lbfshot_Profile = {
    /**/ ACTOR_BG_LBFSHOT,
    /**/ ACTORCAT_BG,
    /**/ FLAGS,
    /**/ OBJECT_LBFSHOT,
    /**/ sizeof(BgLbfshot),
    /**/ BgLbfshot_Init,
    /**/ BgLbfshot_Destroy,
    /**/ MM_Actor_Noop,
    /**/ BgLbfshot_Draw,
};

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_STOP),
};

void BgLbfshot_Init(Actor* thisx, PlayState* play) {
    BgLbfshot* this = (BgLbfshot*)thisx;

    MM_Actor_ProcessInitChain(&this->dyna.actor, MM_sInitChain);
    this->dyna.actor.cullingVolumeDistance = 4000.0f;
    MM_DynaPolyActor_Init(&this->dyna, DYNA_TRANSFORM_POS);
    DynaPolyActor_LoadMesh(play, &this->dyna, &object_lbfshot_Colheader_0014D8);
}
void BgLbfshot_Destroy(Actor* thisx, PlayState* play) {
    BgLbfshot* this = (BgLbfshot*)thisx;

    MM_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}
void BgLbfshot_Draw(Actor* thisx, PlayState* play) {
    MM_Gfx_DrawDListOpa(play, object_lbfshot_DL_000228);
}
