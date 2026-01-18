/*
 * File: z_obj_hana.c
 * Overlay: ovl_Obj_Hana
 * Description: Orange Graveyard Flower
 */

#include "z_obj_hana.h"
#include "objects/object_hana/object_hana.h"

#define FLAGS 0x00000000

void MM_ObjHana_Init(Actor* thisx, PlayState* play);
void MM_ObjHana_Destroy(Actor* thisx, PlayState* play);
void MM_ObjHana_Update(Actor* thisx, PlayState* play);
void MM_ObjHana_Draw(Actor* thisx, PlayState* play);

ActorProfile Obj_Hana_Profile = {
    /**/ ACTOR_OBJ_HANA,
    /**/ ACTORCAT_PROP,
    /**/ FLAGS,
    /**/ OBJECT_HANA,
    /**/ sizeof(ObjHana),
    /**/ MM_ObjHana_Init,
    /**/ MM_ObjHana_Destroy,
    /**/ MM_ObjHana_Update,
    /**/ MM_ObjHana_Draw,
};

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDistance, 900, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeScale, 40, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDownward, 40, ICHAIN_STOP),
};

void MM_ObjHana_Init(Actor* thisx, PlayState* play) {
    ObjHana* this = (ObjHana*)thisx;

    MM_Actor_ProcessInitChain(&this->actor, MM_sInitChain);
}

void MM_ObjHana_Destroy(Actor* thisx, PlayState* play) {
}

void MM_ObjHana_Update(Actor* thisx, PlayState* play) {
}

void MM_ObjHana_Draw(Actor* thisx, PlayState* play) {
    MM_Gfx_DrawDListOpa(play, gGraveyardFlowersDL);
}
