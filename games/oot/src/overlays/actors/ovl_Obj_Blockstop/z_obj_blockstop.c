/*
 * File: z_obj_blockstop.c
 * Overlay: ovl_Obj_Blockstop
 * Description: Stops blocks and sets relevant flags when the block is in position.
 */

#include "z_obj_blockstop.h"
#include "overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"

#define FLAGS 0

void OoT_ObjBlockstop_Init(Actor* thisx, PlayState* play);
void ObjBlockstop_Destroy(Actor* thisx, PlayState* play);
void OoT_ObjBlockstop_Update(Actor* thisx, PlayState* play);

const ActorInit Obj_Blockstop_InitVars = {
    ACTOR_OBJ_BLOCKSTOP,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(ObjBlockstop),
    (ActorFunc)OoT_ObjBlockstop_Init,
    (ActorFunc)ObjBlockstop_Destroy,
    (ActorFunc)OoT_ObjBlockstop_Update,
    NULL,
    NULL,
};

void OoT_ObjBlockstop_Init(Actor* thisx, PlayState* play) {
    ObjBlockstop* this = (ObjBlockstop*)thisx;

    if (OoT_Flags_GetSwitch(play, this->actor.params)) {
        OoT_Actor_Kill(&this->actor);
    } else {
        this->actor.world.pos.y++;
    }
}

void ObjBlockstop_Destroy(Actor* thisx, PlayState* play) {
}

void OoT_ObjBlockstop_Update(Actor* thisx, PlayState* play) {
    ObjBlockstop* this = (ObjBlockstop*)thisx;
    DynaPolyActor* dynaPolyActor;
    Vec3f sp4C;
    s32 bgId;
    s32 pad;

    if (OoT_BgCheck_EntityLineTest2(&play->colCtx, &this->actor.home.pos, &this->actor.world.pos, &sp4C,
                                &this->actor.floorPoly, false, false, true, true, &bgId, &this->actor)) {
        dynaPolyActor = OoT_DynaPoly_GetActor(&play->colCtx, bgId);

        if (dynaPolyActor != NULL && dynaPolyActor->actor.id == ACTOR_OBJ_OSHIHIKI) {
            if ((dynaPolyActor->actor.params & 0x000F) == PUSHBLOCK_HUGE_START_ON ||
                (dynaPolyActor->actor.params & 0x000F) == PUSHBLOCK_HUGE_START_OFF) {
                Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
            }

            OoT_Flags_SetSwitch(play, this->actor.params);
            OoT_Actor_Kill(&this->actor);
        }
    }
}
