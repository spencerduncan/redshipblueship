/*
 * File: z_obj_hana.c
 * Overlay: Obj_Hana
 * Description: Grave Flower
 */

#include "z_obj_hana.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"

#define FLAGS 0

void OoT_ObjHana_Init(Actor* thisx, PlayState* play);
void OoT_ObjHana_Destroy(Actor* thisx, PlayState* play);
void OoT_ObjHana_Update(Actor* thisx, PlayState* play);
void OoT_ObjHana_Draw(Actor* thisx, PlayState* play);

const ActorInit Obj_Hana_InitVars = {
    ACTOR_OBJ_HANA,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_GAMEPLAY_FIELD_KEEP,
    sizeof(ObjHana),
    (ActorFunc)OoT_ObjHana_Init,
    (ActorFunc)OoT_ObjHana_Destroy,
    (ActorFunc)OoT_ObjHana_Update,
    (ActorFunc)OoT_ObjHana_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 8, 10, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit OoT_sColChkInfoInit = { 0, 12, 60, MASS_IMMOVABLE };

typedef struct {
    /* 0x00 */ Gfx* dList;
    /* 0x04 */ f32 scale;
    /* 0x08 */ f32 yOffset;
    /* 0x0C */ s16 radius;
    /* 0x0E */ s16 height;
} HanaParams; // size = 0x10

static HanaParams sHanaParams[] = {
    { gHanaDL, 0.01f, 0.0f, -1, 0 },
    { gFieldKakeraDL, 0.1f, 58.0f, 10, 18 },
    { gFieldBushDL, 0.4f, 0.0f, 12, 44 },
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 900, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 60, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 800, ICHAIN_STOP),
};

void OoT_ObjHana_Init(Actor* thisx, PlayState* play) {
    ObjHana* this = (ObjHana*)thisx;
    s16 type = this->actor.params & 3;
    HanaParams* params = &sHanaParams[type];

    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    OoT_Actor_SetScale(&this->actor, params->scale);
    this->actor.shape.yOffset = params->yOffset;
    if (params->radius >= 0) {
        OoT_Collider_InitCylinder(play, &this->collider);
        OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        this->collider.dim.radius = params->radius;
        this->collider.dim.height = params->height;
        OoT_CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &OoT_sColChkInfoInit);
    }

    if (type == 2 && (OoT_Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_ZELDAS_LETTER))) {
        OoT_Actor_Kill(&this->actor);
    }
}

void OoT_ObjHana_Destroy(Actor* thisx, PlayState* play) {
    ObjHana* this = (ObjHana*)thisx;

    if (sHanaParams[this->actor.params & 3].radius >= 0) {
        OoT_Collider_DestroyCylinder(play, &this->collider);
    }
}

void OoT_ObjHana_Update(Actor* thisx, PlayState* play) {
    ObjHana* this = (ObjHana*)thisx;

    if (sHanaParams[this->actor.params & 3].radius >= 0 && this->actor.xzDistToPlayer < 400.0f) {
        OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

void OoT_ObjHana_Draw(Actor* thisx, PlayState* play) {
    OoT_Gfx_DrawDListOpa(play, sHanaParams[thisx->params & 3].dList);
}
