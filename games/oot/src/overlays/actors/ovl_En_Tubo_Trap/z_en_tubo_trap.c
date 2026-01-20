/*
 * File: z_en_tubo_trap.c
 * Overlay: ovl_En_Tubo_Trap
 * Description: Flying pot enemy
 */

#include "z_en_tubo_trap.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"
#include "vt.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void OoT_EnTuboTrap_Init(Actor* thisx, PlayState* play);
void OoT_EnTuboTrap_Destroy(Actor* thisx, PlayState* play);
void OoT_EnTuboTrap_Update(Actor* thisx, PlayState* play);
void OoT_EnTuboTrap_Draw(Actor* thisx, PlayState* play);

void EnTuboTrap_WaitForProximity(EnTuboTrap* this, PlayState* play);
void OoT_EnTuboTrap_Levitate(EnTuboTrap* this, PlayState* play);
void EnTuboTrap_Fly(EnTuboTrap* this, PlayState* play);

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x04 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 9, 23, 0, { 0 } },
};

const ActorInit En_Tubo_Trap_InitVars = {
    ACTOR_EN_TUBO_TRAP,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_GAMEPLAY_DANGEON_KEEP,
    sizeof(EnTuboTrap),
    (ActorFunc)OoT_EnTuboTrap_Init,
    (ActorFunc)OoT_EnTuboTrap_Destroy,
    (ActorFunc)OoT_EnTuboTrap_Update,
    (ActorFunc)OoT_EnTuboTrap_Draw,
    NULL,
};

void OoT_EnTuboTrap_Init(Actor* thisx, PlayState* play) {
    EnTuboTrap* this = (EnTuboTrap*)thisx;

    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 2.0f);
    osSyncPrintf("\n\n");
    osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ 壷トラップ ☆☆☆☆☆ %x\n" VT_RST, this->actor.params); // "Urn Trap"
    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
    OoT_Actor_SetScale(&this->actor, 0.1f);
    this->actionFunc = EnTuboTrap_WaitForProximity;
}

void OoT_EnTuboTrap_Destroy(Actor* thisx, PlayState* play) {
    EnTuboTrap* this = (EnTuboTrap*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);
}

void OoT_EnTuboTrap_DropCollectible(EnTuboTrap* this, PlayState* play) {
    s16 params = this->actor.params;
    s16 param3FF = (params >> 6) & 0x3FF;

    if (param3FF >= 0 && param3FF < 0x1A) {
        OoT_Item_DropCollectible(play, &this->actor.world.pos, param3FF | ((params & 0x3F) << 8));
    }
}

void OoT_EnTuboTrap_SpawnEffectsOnLand(EnTuboTrap* this, PlayState* play) {
    f32 rand;
    f32 cos;
    f32 sin;
    Vec3f pos;
    Vec3f velocity;
    s16 var;
    s32 arg5;
    s32 i;
    Vec3f* actorPos = &this->actor.world.pos;

    for (i = 0, var = 0; i < 15; i++, var += 20000) {
        sin = OoT_Math_SinS(var);
        cos = OoT_Math_CosS(var);
        pos.x = sin * 8.0f;
        pos.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cos * 8.0f;

        velocity.x = pos.x * 0.23f;
        velocity.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        velocity.z = pos.z * 0.23f;

        pos.x += actorPos->x;
        pos.y += actorPos->y;
        pos.z += actorPos->z;

        rand = OoT_Rand_ZeroOne();
        if (rand < 0.2f) {
            arg5 = 96;
        } else if (rand < 0.6f) {
            arg5 = 64;
        } else {
            arg5 = 32;
        }

        OoT_EffectSsKakera_Spawn(play, &pos, &velocity, actorPos, -240, arg5, 10, 10, 0, (OoT_Rand_ZeroOne() * 65.0f) + 15.0f,
                             0, 32, 60, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_DANGEON_KEEP, gPotFragmentDL);
    }

    func_80033480(play, actorPos, 30.0f, 4, 20, 50, 0);
}

void OoT_EnTuboTrap_SpawnEffectsInWater(EnTuboTrap* this, PlayState* play) {
    f32 rand;
    f32 cos;
    f32 sin;
    Vec3f pos;
    Vec3f velocity;
    s16 var;
    s32 arg5;
    s32 i;
    Vec3f* actorPos = &this->actor.world.pos;

    pos = *actorPos;
    pos.y += this->actor.yDistToWater;

    OoT_EffectSsGSplash_Spawn(play, &pos, 0, 0, 0, 400);

    for (i = 0, var = 0; i < 15; i++, var += 20000) {
        sin = OoT_Math_SinS(var);
        cos = OoT_Math_CosS(var);
        pos.x = sin * 8.0f;
        pos.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cos * 8.0f;

        velocity.x = pos.x * 0.20f;
        velocity.y = (OoT_Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.20f;

        pos.x += actorPos->x;
        pos.y += actorPos->y;
        pos.z += actorPos->z;

        rand = OoT_Rand_ZeroOne();
        if (rand < 0.2f) {
            arg5 = 64;
        } else {
            arg5 = 32;
        }

        OoT_EffectSsKakera_Spawn(play, &pos, &velocity, actorPos, -180, arg5, 30, 30, 0, (OoT_Rand_ZeroOne() * 65.0f) + 15.0f,
                             0, 32, 70, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_DANGEON_KEEP, gPotFragmentDL);
    }
}

void OoT_EnTuboTrap_HandleImpact(EnTuboTrap* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Player* player2 = GET_PLAYER(play);

    if ((this->actor.bgCheckFlags & 0x20) && (this->actor.yDistToWater > 15.0f)) {
        OoT_EnTuboTrap_SpawnEffectsInWater(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_BOMB_DROP_WATER);
        OoT_EnTuboTrap_DropCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        return;
    }

    if (this->collider.base.atFlags & AT_BOUNCED) {
        this->collider.base.atFlags &= ~AT_BOUNCED;
        OoT_EnTuboTrap_SpawnEffectsOnLand(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_IT_SHIELD_REFLECT_SW);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_POT_BROKEN);
        OoT_EnTuboTrap_DropCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        return;
    }

    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        OoT_EnTuboTrap_SpawnEffectsOnLand(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_EXPLOSION);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_POT_BROKEN);
        OoT_EnTuboTrap_DropCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        return;
    }

    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if (this->collider.base.at == &player->actor) {
            OoT_EnTuboTrap_SpawnEffectsOnLand(this, play);
            OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_POT_BROKEN);
            OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &player2->actor.world.pos, 40, NA_SE_PL_BODY_HIT);
            OoT_EnTuboTrap_DropCollectible(this, play);
            OoT_Actor_Kill(&this->actor);
            GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
            return;
        }
    }

    if ((this->actor.bgCheckFlags & 8) || (this->actor.bgCheckFlags & 1)) {
        OoT_EnTuboTrap_SpawnEffectsOnLand(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_POT_BROKEN);
        OoT_EnTuboTrap_DropCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
        return;
    }
}

void EnTuboTrap_WaitForProximity(EnTuboTrap* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 targetHeight;

    if (BREG(2) != 0) {
        osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ わて     ☆☆☆☆☆ %f\n" VT_RST, this->actor.world.pos.y);   // "You"
        osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ おいどん ☆☆☆☆☆ %f\n" VT_RST, player->actor.world.pos.y); // "Me"
        osSyncPrintf("\n\n");
    }

    if (this->actor.xzDistToPlayer < 200.0f && this->actor.world.pos.y <= player->actor.world.pos.y) {
        OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_ENEMY);
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        targetHeight = 40.0f + -10.0f * gSaveContext.linkAge;

        this->targetY = player->actor.world.pos.y + targetHeight;
        if (this->targetY < this->actor.world.pos.y) {
            this->targetY = this->actor.world.pos.y + targetHeight;
        }

        this->originPos = this->actor.world.pos;
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_POT_MOVE_START);
        this->actionFunc = OoT_EnTuboTrap_Levitate;
    }
}

void OoT_EnTuboTrap_Levitate(EnTuboTrap* this, PlayState* play) {
    this->actor.shape.rot.y += 5000;
    OoT_Math_ApproachF(&this->actor.world.pos.y, this->targetY, 0.8f, 3.0f);

    if (fabsf(this->actor.world.pos.y - this->targetY) < 10.0f) {
        this->actor.speedXZ = 10.0f;
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        this->actionFunc = EnTuboTrap_Fly;
    }
}

void EnTuboTrap_Fly(EnTuboTrap* this, PlayState* play) {
    f32 dx = this->originPos.x - this->actor.world.pos.x;
    f32 dy = this->originPos.y - this->actor.world.pos.y;
    f32 dz = this->originPos.z - this->actor.world.pos.z;

    Audio_PlayActorSound2(&this->actor, NA_SE_EN_TUBOOCK_FLY - SFX_FLAG);

    if (240.0f < sqrtf(SQ(dx) + SQ(dy) + SQ(dz))) {
        OoT_Math_ApproachF(&this->actor.gravity, -3.0f, 0.2f, 0.5f);
    }

    this->actor.shape.rot.y += 5000;
    OoT_EnTuboTrap_HandleImpact(this, play);
}

void OoT_EnTuboTrap_Update(Actor* thisx, PlayState* play) {
    EnTuboTrap* this = (EnTuboTrap*)thisx;
    s32 pad;

    this->actionFunc(this, play);
    Actor_MoveXZGravity(&this->actor);
    OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 10.0f, 20.0f, 0x1D);
    OoT_Actor_SetFocus(&this->actor, 0.0f);
    OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
    OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}

void OoT_EnTuboTrap_Draw(Actor* thisx, PlayState* play) {
    OoT_Gfx_DrawDListOpa(play, gPotDL);
}
