/*
 * File: z_bg_spot11_bakudankabe.c
 * Overlay: ovl_Bg_Spot11_Bakudankabe
 * Description: Destructible Wall (Desert Colossus)
 */

#include "z_bg_spot11_bakudankabe.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"
#include "objects/object_spot11_obj/object_spot11_obj.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void BgSpot11Bakudankabe_Init(Actor* thisx, PlayState* play);
void BgSpot11Bakudankabe_Destroy(Actor* thisx, PlayState* play);
void BgSpot11Bakudankabe_Update(Actor* thisx, PlayState* play);
void BgSpot11Bakudankabe_Draw(Actor* thisx, PlayState* play);

const ActorInit Bg_Spot11_Bakudankabe_InitVars = {
    ACTOR_BG_SPOT11_BAKUDANKABE,
    ACTORCAT_BG,
    FLAGS,
    OBJECT_SPOT11_OBJ,
    sizeof(BgSpot11Bakudankabe),
    (ActorFunc)BgSpot11Bakudankabe_Init,
    (ActorFunc)BgSpot11Bakudankabe_Destroy,
    (ActorFunc)BgSpot11Bakudankabe_Update,
    (ActorFunc)BgSpot11Bakudankabe_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000008, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 40, 80, 0, { 2259, 108, -1580 } },
};

static Vec3f D_808B272C = { 2259.0f, 108.0f, -1550.0f };
static Vec3f D_808B2738 = { 2259.0f, 108.0f, -1550.0f };

void func_808B2180(BgSpot11Bakudankabe* this, PlayState* play) {
    s32 pad;

    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->dyna.actor, &OoT_sCylinderInit);
    this->collider.dim.pos.x += (s16)this->dyna.actor.world.pos.x;
    this->collider.dim.pos.y += (s16)this->dyna.actor.world.pos.y;
    this->collider.dim.pos.z += (s16)this->dyna.actor.world.pos.z;
}

void func_808B2218(BgSpot11Bakudankabe* this, PlayState* play) {
    Actor* thisx = &this->dyna.actor;
    Vec3f burstDepthY;
    Vec3f burstDepthX;
    s32 i;

    burstDepthX.z = 0;
    burstDepthX.x = 0;

    for (i = 0; i < 20; i++) {
        s16 scale;
        s32 gravityInfluence;
        s32 rotationSpeed;

        OoT_Math_Vec3f_Sum(&thisx->world.pos, &D_808B272C, &burstDepthY);

        burstDepthY.x += (OoT_Rand_ZeroOne() - 0.5f) * 120.0f;
        burstDepthY.y += (30.0f + (i * 6.5f));
        burstDepthY.z += (OoT_Rand_ZeroOne() - 0.5f) * 20.0f;

        burstDepthX.y = (OoT_Rand_ZeroOne() - 0.2f) * 12.0f;
        scale = (OoT_Rand_ZeroOne() * 55.0f) + 8.0f;

        if (scale < 20) {
            gravityInfluence = -300;
        } else if (scale < 35) {
            gravityInfluence = -360;
        } else {
            gravityInfluence = -420;
        }
        if (OoT_Rand_ZeroOne() < 0.4f) {
            rotationSpeed = 65;
        } else {
            rotationSpeed = 33;
        }
        OoT_EffectSsKakera_Spawn(play, &burstDepthY, &burstDepthX, &burstDepthY, gravityInfluence, rotationSpeed, 0x1E, 4,
                             0, scale, 1, 3, 80, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_FIELD_KEEP, gFieldKakeraDL);
    }
    OoT_Math_Vec3f_Sum(&thisx->world.pos, &D_808B272C, &burstDepthY);
    func_80033480(play, &burstDepthY, 70, 4, 110, 160, 1);
    burstDepthY.y += 40;
    func_80033480(play, &burstDepthY, 70, 5, 110, 160, 1);
    burstDepthY.y += 40;
    func_80033480(play, &burstDepthY, 70, 4, 110, 160, 1);
}

void BgSpot11Bakudankabe_Init(Actor* thisx, PlayState* play) {
    BgSpot11Bakudankabe* this = (BgSpot11Bakudankabe*)thisx;
    s32 pad;
    CollisionHeader* colHeader = NULL;

    OoT_DynaPolyActor_Init(&this->dyna, DPM_UNK);
    if (OoT_Flags_GetSwitch(play, (this->dyna.actor.params & 0x3F))) {
        OoT_Actor_Kill(&this->dyna.actor);
        return;
    }
    func_808B2180(this, play);
    OoT_CollisionHeader_GetVirtual(&gDesertColossusBombableWallCol, &colHeader);
    this->dyna.bgId = OoT_DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
    OoT_Actor_SetScale(&this->dyna.actor, 1.0f);
    osSyncPrintf("(spot11 爆弾壁)(arg_data 0x%04x)\n", this->dyna.actor.params);
}

void BgSpot11Bakudankabe_Destroy(Actor* thisx, PlayState* play) {
    BgSpot11Bakudankabe* this = (BgSpot11Bakudankabe*)thisx;

    OoT_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    OoT_Collider_DestroyCylinder(play, &this->collider);
}

void BgSpot11Bakudankabe_Update(Actor* thisx, PlayState* play) {
    BgSpot11Bakudankabe* this = (BgSpot11Bakudankabe*)thisx;

    if (this->collider.base.acFlags & AC_HIT) {
        func_808B2218(this, play);
        OoT_Flags_SetSwitch(play, (this->dyna.actor.params & 0x3F));
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &D_808B2738, 40, NA_SE_EV_WALL_BROKEN);
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
        OoT_Actor_Kill(&this->dyna.actor);
        return;
    }
    OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

void BgSpot11Bakudankabe_Draw(Actor* thisx, PlayState* play) {
    BgSpot11Bakudankabe* this = (BgSpot11Bakudankabe*)thisx;

    OoT_Gfx_DrawDListOpa(play, gDesertColossusBombableWallDL);
}
