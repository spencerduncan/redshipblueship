/*
 * File: z_en_wallmas.c
 * Overlay: ovl_En_Wallmas
 * Description: Wallmaster
 */

#include "z_en_wallmas.h"
#include "overlays/actors/ovl_En_Clear_Tag/z_en_clear_tag.h"
#include "overlays/actors/ovl_En_Encount1/z_en_encount1.h"
#include "overlays/actors/ovl_Obj_Ice_Poly/z_obj_ice_poly.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define FLAGS                                                                                 \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER)

void MM_EnWallmas_Init(Actor* thisx, PlayState* play);
void MM_EnWallmas_Destroy(Actor* thisx, PlayState* play);
void MM_EnWallmas_Update(Actor* thisx, PlayState* play);
void MM_EnWallmas_Draw(Actor* thisx, PlayState* play);

void MM_EnWallmas_TimerInit(EnWallmas* this, PlayState* play);
void MM_EnWallmas_WaitToDrop(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupDrop(EnWallmas* this, PlayState* play);
void MM_EnWallmas_Drop(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupLand(EnWallmas* this, PlayState* play);
void MM_EnWallmas_Land(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupStand(EnWallmas* this);
void MM_EnWallmas_Stand(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupWalk(EnWallmas* this);
void MM_EnWallmas_Walk(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupJumpToCeiling(EnWallmas* this);
void MM_EnWallmas_JumpToCeiling(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupReturnToCeiling(EnWallmas* this);
void MM_EnWallmas_ReturnToCeiling(EnWallmas* this, PlayState* play);
void EnWallmas_Damage(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupCooldown(EnWallmas* this);
void MM_EnWallmas_Cooldown(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupDie(EnWallmas* this, PlayState* play);
void MM_EnWallmas_Die(EnWallmas* this, PlayState* play);
void MM_EnWallmas_SetupTakePlayer(EnWallmas* this, PlayState* play);
void MM_EnWallmas_TakePlayer(EnWallmas* this, PlayState* play);
void MM_EnWallmas_ProximityOrSwitchInit(EnWallmas* this);
void MM_EnWallmas_WaitForProximity(EnWallmas* this, PlayState* play);
void MM_EnWallmas_WaitForSwitchFlag(EnWallmas* this, PlayState* play);
void MM_EnWallmas_Stun(EnWallmas* this, PlayState* play);

ActorProfile En_Wallmas_Profile = {
    /**/ ACTOR_EN_WALLMAS,
    /**/ ACTORCAT_ENEMY,
    /**/ FLAGS,
    /**/ OBJECT_WALLMASTER,
    /**/ sizeof(EnWallmas),
    /**/ MM_EnWallmas_Init,
    /**/ MM_EnWallmas_Destroy,
    /**/ MM_EnWallmas_Update,
    /**/ MM_EnWallmas_Draw,
};

static ColliderCylinderInit MM_sCylinderInit = {
    {
        COL_MATERIAL_HIT0,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE | ATELEM_SFX_NORMAL,
        ACELEM_ON | ACELEM_HOOKABLE,
        OCELEM_ON,
    },
    { 30, 40, 0, { 0, 0, 0 } },
};

typedef enum {
    /* 0x0 */ WALLMASTER_DMGEFF_NONE,
    /* 0x1 */ WALLMASTER_DMGEFF_STUN,
    /* 0x2 */ WALLMASTER_DMGEFF_FIRE_ARROW,
    /* 0x3 */ WALLMASTER_DMGEFF_ICE_ARROW,
    /* 0x4 */ WALLMASTER_DMGEFF_LIGHT_ARROW,
    /* 0x5 */ WALLMASTER_DMGEFF_ZORA_MAGIC,
    /* 0xF */ WALLMASTER_DMGEFF_HOOKSHOT = 0xF
} EnWallmasDamageEffect;

static DamageTable MM_sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, WALLMASTER_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Normal arrow   */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, WALLMASTER_DMGEFF_HOOKSHOT),
    /* Goron punch    */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(2, WALLMASTER_DMGEFF_FIRE_ARROW),
    /* Ice arrow      */ DMG_ENTRY(1, WALLMASTER_DMGEFF_ICE_ARROW),
    /* Light arrow    */ DMG_ENTRY(2, WALLMASTER_DMGEFF_LIGHT_ARROW),
    /* Goron spikes   */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(0, WALLMASTER_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(2, WALLMASTER_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, WALLMASTER_DMGEFF_STUN),
    /* Zora barrier   */ DMG_ENTRY(0, WALLMASTER_DMGEFF_ZORA_MAGIC),
    /* Normal shield  */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, WALLMASTER_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(1, WALLMASTER_DMGEFF_NONE),
};

static CollisionCheckInfoInit MM_sColChkInfoInit = { 3, 30, 40, 150 };

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_S8(hintId, TATL_HINT_ID_WALLMASTER, ICHAIN_CONTINUE),
    ICHAIN_F32(lockOnArrowOffset, 5500, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -1500, ICHAIN_STOP),
};

static f32 sYOffsetPerForm[PLAYER_FORM_MAX] = {
    50.0f, // PLAYER_FORM_FIERCE_DEITY
    55.0f, // PLAYER_FORM_GORON
    50.0f, // PLAYER_FORM_ZORA
    20.0f, // PLAYER_FORM_DEKU
    30.0f, // PLAYER_FORM_HUMAN
};

void MM_EnWallmas_Init(Actor* thisx, PlayState* play) {
    EnWallmas* this = (EnWallmas*)thisx;

    MM_Actor_ProcessInitChain(&this->actor, MM_sInitChain);
    MM_ActorShape_Init(&this->actor.shape, 0.0f, NULL, 0.5f);
    MM_SkelAnime_InitFlex(play, &this->skelAnime, &gWallmasterSkel, &gWallmasterIdleAnim, this->jointTable,
                       this->morphTable, WALLMASTER_LIMB_MAX);
    Collider_InitAndSetCylinder(play, &this->collider, &this->actor, &MM_sCylinderInit);
    MM_CollisionCheck_SetInfo(&this->actor.colChkInfo, &MM_sDamageTable, &MM_sColChkInfoInit);

    this->switchFlag = WALLMASTER_GET_SWITCH_FLAG(thisx);
    this->actor.params &= 0xFF;
    this->detectionRadius = this->actor.shape.rot.x * 40.0f * 0.1f;
    this->actor.shape.rot.x = 0;
    this->actor.world.rot.x = 0;
    if (this->detectionRadius <= 0.0f) {
        this->detectionRadius = 200.0f;
    }

    MM_Actor_SetFocus(&this->actor, 25.0f);

    if (WALLMASTER_IS_FROZEN(&this->actor)) {
        MM_Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_OBJ_ICE_POLY, this->actor.world.pos.x,
                           this->actor.world.pos.y - 15.0f, this->actor.world.pos.z, this->actor.world.rot.x,
                           (this->actor.world.rot.y + 0x5900), this->actor.world.rot.z,
                           OBJICEPOLY_PARAMS(80, OBJICEPOLY_SWITCH_FLAG_NONE));
        this->actor.params &= ~0x80;
        MM_EnWallmas_SetupReturnToCeiling(this);
        return;
    }

    if (WALLMASTER_GET_TYPE(&this->actor) == WALLMASTER_TYPE_FLAG) {
        if (MM_Flags_GetSwitch(play, this->switchFlag)) {
            MM_Actor_Kill(&this->actor);
            return;
        }

        MM_EnWallmas_ProximityOrSwitchInit(this);
    } else if (WALLMASTER_GET_TYPE(&this->actor) == WALLMASTER_TYPE_PROXIMITY) {
        MM_EnWallmas_ProximityOrSwitchInit(this);
    } else {
        MM_EnWallmas_TimerInit(this, play);
    }
}

void MM_EnWallmas_Destroy(Actor* thisx, PlayState* play) {
    EnWallmas* this = (EnWallmas*)thisx;

    MM_Collider_DestroyCylinder(play, &this->collider);

    if (this->actor.parent != NULL) {
        EnEncount1* encount1 = (EnEncount1*)this->actor.parent;

        if ((encount1->actor.update != NULL) && (encount1->spawnActiveCount > 0)) {
            encount1->spawnActiveCount--;
        }
    }
}

void EnWallmas_Freeze(EnWallmas* this) {
    this->drawDmgEffType = ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX;
    this->drawDmgEffScale = 0.55f;
    this->drawDmgEffFrozenSteamScale = 825.0f * 0.001f;
    this->drawDmgEffAlpha = 1.0f;
    this->collider.base.colMaterial = COL_MATERIAL_HIT3;
    this->timer = 80;
    this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    MM_Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 80);
}

void EnWallmas_ThawIfFrozen(EnWallmas* this, PlayState* play) {
    if (this->drawDmgEffType == ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX) {
        this->drawDmgEffType = ACTOR_DRAW_DMGEFF_FIRE;
        this->collider.base.colMaterial = COL_MATERIAL_HIT0;
        this->drawDmgEffAlpha = 0.0f;
        Actor_SpawnIceEffects(play, &this->actor, this->bodyPartsPos, WALLMASTER_BODYPART_MAX, 2, 0.3f, 0.2f);
        this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    }
}

void MM_EnWallmas_TimerInit(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
    this->timer = 130;
    this->actor.velocity.y = 0.0f;
    this->actor.world.pos.y = player->actor.world.pos.y;
    this->actor.floorHeight = player->actor.floorHeight;
    this->actor.draw = MM_EnWallmas_Draw;
    this->actionFunc = MM_EnWallmas_WaitToDrop;
}

void MM_EnWallmas_WaitToDrop(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f* playerPos = &player->actor.world.pos;

    this->actor.world.pos = *playerPos;
    this->actor.floorHeight = player->actor.floorHeight;
    this->actor.floorPoly = player->actor.floorPoly;

    if (this->timer != 0) {
        this->timer--;
    }

    if ((player->stateFlags1 & (PLAYER_STATE1_100000 | PLAYER_STATE1_8000000)) ||
        (player->stateFlags2 & PLAYER_STATE2_80) || (player->textboxBtnCooldownTimer > 0) ||
        (player->actor.freezeTimer > 0) || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        ((WALLMASTER_GET_TYPE(&this->actor) == WALLMASTER_TYPE_PROXIMITY) &&
         (MM_Math_Vec3f_DistXZ(&this->actor.home.pos, playerPos) > (120.f + this->detectionRadius)))) {
        AudioSfx_StopById(NA_SE_EN_FALL_AIM);
        this->timer = 130;
    }

    if (this->timer == 80) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_AIM);
    }

    if (this->timer == 0) {
        MM_EnWallmas_SetupDrop(this, play);
    }
}

void MM_EnWallmas_SetupDrop(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    MM_Animation_Change(&this->skelAnime, &gWallmasterLungeAnim, 0.0f, 20.0f,
                     MM_Animation_GetLastFrame(&gWallmasterLungeAnim), ANIMMODE_ONCE, 0.0f);

    this->yTarget = player->actor.world.pos.y;
    this->actor.world.pos.y = player->actor.world.pos.y + 300.0f;
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.floorHeight = player->actor.floorHeight;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.flags &= ~ACTOR_FLAG_DRAW_CULLING_DISABLED;
    this->actionFunc = MM_EnWallmas_Drop;
}

void MM_EnWallmas_Drop(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((player->stateFlags2 & PLAYER_STATE2_80) || (player->actor.freezeTimer > 0)) {
        MM_EnWallmas_SetupReturnToCeiling(this);
    } else if (!MM_Play_InCsMode(play) && !(player->stateFlags2 & PLAYER_STATE2_10) && (player->invincibilityTimer >= 0) &&
               (this->actor.xzDistToPlayer < 30.0f) && (this->actor.playerHeightRel < -5.0f) &&
               (-(f32)(player->cylinder.dim.height + 10) < this->actor.playerHeightRel)) {
        MM_EnWallmas_SetupTakePlayer(this, play);
    } else if (this->actor.world.pos.y <= this->yTarget) {
        this->actor.world.pos.y = this->yTarget;
        this->actor.velocity.y = 0.0f;
        MM_EnWallmas_SetupLand(this, play);
    }
}

void MM_EnWallmas_SetupLand(EnWallmas* this, PlayState* play) {
    MM_Animation_Change(&this->skelAnime, &gWallmasterJumpAnim, 1.0f, 41.0f, MM_Animation_GetLastFrame(&gWallmasterJumpAnim),
                     ANIMMODE_ONCE, -3.0f);

    MM_Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 15.0f, 6, 20.0f, 300, 100, true);
    Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_LAND);
    this->actionFunc = MM_EnWallmas_Land;
}

void MM_EnWallmas_Land(EnWallmas* this, PlayState* play) {
    if (MM_SkelAnime_Update(&this->skelAnime)) {
        MM_EnWallmas_SetupStand(this);
    }
}

void MM_EnWallmas_SetupStand(EnWallmas* this) {
    MM_Animation_PlayOnce(&this->skelAnime, &gWallmasterStandUpAnim);
    this->actionFunc = MM_EnWallmas_Stand;
}

void MM_EnWallmas_Stand(EnWallmas* this, PlayState* play) {
    if (MM_SkelAnime_Update(&this->skelAnime)) {
        MM_EnWallmas_SetupWalk(this);
    }

    MM_Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer + 0x8000, 0xB6);
    this->actor.world.rot.y = this->actor.shape.rot.y;
}

void MM_EnWallmas_SetupWalk(EnWallmas* this) {
    MM_Animation_PlayOnceSetSpeed(&this->skelAnime, &gWallmasterWalkAnim, 3.0f);
    this->actor.speed = 3.0f;
    this->actionFunc = MM_EnWallmas_Walk;
}

void MM_EnWallmas_Walk(EnWallmas* this, PlayState* play) {
    if (MM_SkelAnime_Update(&this->skelAnime)) {
        MM_EnWallmas_SetupJumpToCeiling(this);
    }

    MM_Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer + 0x8000, 0xB6);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (MM_Animation_OnFrame(&this->skelAnime, 0.0f) || MM_Animation_OnFrame(&this->skelAnime, 12.0f) ||
        MM_Animation_OnFrame(&this->skelAnime, 24.0f) || MM_Animation_OnFrame(&this->skelAnime, 36.0f)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_WALK);
    }
}

void MM_EnWallmas_SetupJumpToCeiling(EnWallmas* this) {
    MM_Animation_PlayOnce(&this->skelAnime, &gWallmasterStopWalkAnim);
    this->actor.speed = 0.0f;
    this->actionFunc = MM_EnWallmas_JumpToCeiling;
}

void MM_EnWallmas_JumpToCeiling(EnWallmas* this, PlayState* play) {
    if (MM_SkelAnime_Update(&this->skelAnime)) {
        MM_EnWallmas_SetupReturnToCeiling(this);
    }
}

void MM_EnWallmas_SetupReturnToCeiling(EnWallmas* this) {
    this->timer = 0;
    this->actor.speed = 0.0f;
    MM_Animation_Change(&this->skelAnime, &gWallmasterJumpAnim, 3.0f, 0.0f, MM_Animation_GetLastFrame(&gWallmasterJumpAnim),
                     ANIMMODE_ONCE, -3.0f);
    this->actionFunc = MM_EnWallmas_ReturnToCeiling;
}

void MM_EnWallmas_ReturnToCeiling(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    MM_SkelAnime_Update(&this->skelAnime);
    if (this->skelAnime.curFrame > 20.0f) {
        this->actor.world.pos.y += 30.0f;
        this->timer += 9;
        this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
    }

    if (MM_Animation_OnFrame(&this->skelAnime, 20.0f)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_UP);
    }

    if (this->actor.playerHeightRel < -900.0f) {
        if (WALLMASTER_GET_TYPE(&this->actor) == WALLMASTER_TYPE_FLAG) {
            MM_Actor_Kill(&this->actor);
            return;
        }

        if ((WALLMASTER_GET_TYPE(&this->actor) == WALLMASTER_TYPE_TIMER_ONLY) ||
            (MM_Math_Vec3f_DistXZ(&this->actor.home.pos, &player->actor.world.pos) < this->detectionRadius)) {
            MM_EnWallmas_TimerInit(this, play);
        } else {
            MM_EnWallmas_ProximityOrSwitchInit(this);
        }
    }
}

void EnWallmas_SetupDamage(EnWallmas* this, s32 arg1) {
    MM_Animation_MorphToPlayOnce(&this->skelAnime, &gWallmasterDamageAnim, -3.0f);

    if (arg1) {
        func_800BE504(&this->actor, &this->collider);
    }

    MM_Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 20);
    this->actor.speed = 5.0f;
    this->actor.velocity.y = 10.0f;
    this->actionFunc = EnWallmas_Damage;
}

void EnWallmas_Damage(EnWallmas* this, PlayState* play) {
    if (MM_SkelAnime_Update(&this->skelAnime)) {
        if (this->actor.colChkInfo.health == 0) {
            MM_EnWallmas_SetupDie(this, play);
        } else {
            MM_EnWallmas_SetupCooldown(this);
        }
    }

    if (MM_Animation_OnFrame(&this->skelAnime, 13.0f)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_EYEGOLE_ATTACK);
    }

    MM_Math_StepToF(&this->actor.speed, 0.0f, 0.2f);
}

void MM_EnWallmas_SetupCooldown(EnWallmas* this) {
    MM_Animation_PlayOnce(&this->skelAnime, &gWallmasterRecoverFromDamageAnim);
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = MM_EnWallmas_Cooldown;
}

void MM_EnWallmas_Cooldown(EnWallmas* this, PlayState* play) {
    if (MM_SkelAnime_Update(&this->skelAnime)) {
        MM_EnWallmas_SetupReturnToCeiling(this);
    }
}

void MM_EnWallmas_SetupDie(EnWallmas* this, PlayState* play) {
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    func_800B3030(play, &this->actor.world.pos, &gZeroVec3f, &gZeroVec3f, 250, -10, 2);
    MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 11, NA_SE_EN_EXTINCT);
    this->actionFunc = MM_EnWallmas_Die;
}

void MM_EnWallmas_Die(EnWallmas* this, PlayState* play) {
    if (MM_Math_StepToF(&this->actor.scale.x, 0.0f, 0.0015f)) {
        MM_Actor_SetScale(&this->actor, 0.01f);
        MM_Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x90);
        MM_Actor_Kill(&this->actor);
    }

    this->actor.scale.z = this->actor.scale.x;
    this->actor.scale.y = this->actor.scale.x;
}

void MM_EnWallmas_SetupTakePlayer(EnWallmas* this, PlayState* play) {
    MM_Animation_MorphToPlayOnce(&this->skelAnime, &gWallmasterHoverAnim, -5.0f);
    this->timer = -30;
    this->actionFunc = MM_EnWallmas_TakePlayer;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->yTarget = this->actor.playerHeightRel;
    Player_SetCsAction(play, &this->actor, PLAYER_CSACTION_18);
}

void MM_EnWallmas_TakePlayer(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (MM_Animation_OnFrame(&this->skelAnime, 1.0f)) {
        MM_Player_PlaySfx(player, player->ageProperties->voiceSfxIdOffset + NA_SE_VO_LI_DAMAGE_S);
        Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_CATCH);
    }

    if (MM_SkelAnime_Update(&this->skelAnime)) {
        player->actor.world.pos.x = this->actor.world.pos.x;
        player->actor.world.pos.z = this->actor.world.pos.z;

        if (this->timer < 0) {
            this->actor.world.pos.y += 2.0f;
        } else {
            this->actor.world.pos.y += 10.0f;
        }

        player->actor.world.pos.y = this->actor.world.pos.y - sYOffsetPerForm[GET_PLAYER_FORM];
        if (this->timer == -30) {
            MM_Player_PlaySfx(player, player->ageProperties->voiceSfxIdOffset + NA_SE_VO_LI_TAKEN_AWAY);
        }

        if (this->timer == 0) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_UP);
        }

        this->timer += 2;
    } else {
        MM_Math_StepToF(&this->actor.world.pos.y, sYOffsetPerForm[GET_PLAYER_FORM] + player->actor.world.pos.y, 5.0f);
    }

    MM_Math_StepToF(&this->actor.world.pos.x, player->actor.world.pos.x, 3.0f);
    MM_Math_StepToF(&this->actor.world.pos.z, player->actor.world.pos.z, 3.0f);

    if (this->timer == 30) {
        Audio_PlaySfx(NA_SE_OC_ABYSS);
        func_80169FDC(play);
    }
}

void MM_EnWallmas_ProximityOrSwitchInit(EnWallmas* this) {
    this->timer = 0;
    this->actor.draw = NULL;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    if (WALLMASTER_GET_TYPE(&this->actor) == WALLMASTER_TYPE_PROXIMITY) {
        this->actionFunc = MM_EnWallmas_WaitForProximity;
    } else {
        this->actionFunc = MM_EnWallmas_WaitForSwitchFlag;
    }
}

void MM_EnWallmas_WaitForProximity(EnWallmas* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (MM_Math_Vec3f_DistXZ(&this->actor.home.pos, &player->actor.world.pos) < this->detectionRadius) {
        MM_EnWallmas_TimerInit(this, play);
    }
}

void MM_EnWallmas_WaitForSwitchFlag(EnWallmas* this, PlayState* play) {
    if (MM_Flags_GetSwitch(play, this->switchFlag)) {
        MM_EnWallmas_TimerInit(this, play);
        this->timer = 81;
    }
}

void MM_EnWallmas_SetupStun(EnWallmas* this) {
    this->actor.speed = 0.0f;
    if (this->actor.velocity.y > 0.0f) {
        this->actor.velocity.y = 0.0f;
    }

    func_800BE504(&this->actor, &this->collider);
    this->actionFunc = MM_EnWallmas_Stun;
}

void MM_EnWallmas_Stun(EnWallmas* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }

    if (this->timer == 0) {
        EnWallmas_ThawIfFrozen(this, play);
        if (this->actor.colChkInfo.health == 0) {
            EnWallmas_SetupDamage(this, false);
        } else {
            this->actor.world.rot.y = this->actor.shape.rot.y;
            MM_EnWallmas_SetupReturnToCeiling(this);
        }
    }
}

void EnWallmas_UpdateDamage(EnWallmas* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        MM_Actor_SetDropFlag(&this->actor, &this->collider.elem);

        if ((this->drawDmgEffType != ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX) ||
            (!(this->collider.elem.acHitElem->atDmgInfo.dmgFlags & 0xDB0B3))) {
            if (MM_Actor_ApplyDamage(&this->actor) == 0) {
                MM_Enemy_StartFinishingBlow(play, &this->actor);
                Actor_PlaySfx(&this->actor, NA_SE_EN_DAIOCTA_REVERSE);
                this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            } else if (this->actor.colChkInfo.damage != 0) {
                Actor_PlaySfx(&this->actor, NA_SE_EN_FALL_DAMAGE);
            }

            EnWallmas_ThawIfFrozen(this, play);

            if (this->actor.colChkInfo.damageEffect != WALLMASTER_DMGEFF_HOOKSHOT) {
                if (this->actor.colChkInfo.damageEffect == WALLMASTER_DMGEFF_ICE_ARROW) {
                    EnWallmas_Freeze(this);
                    if (this->actor.colChkInfo.health == 0) {
                        this->timer = 3;
                        this->collider.base.acFlags &= ~AC_ON;
                    }

                    MM_EnWallmas_SetupStun(this);
                } else if (this->actor.colChkInfo.damageEffect == WALLMASTER_DMGEFF_STUN) {
                    this->timer = 40;
                    MM_Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, 40);
                    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
                    MM_EnWallmas_SetupStun(this);
                } else if (this->actor.colChkInfo.damageEffect == WALLMASTER_DMGEFF_ZORA_MAGIC) {
                    this->timer = 40;
                    MM_Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, 40);
                    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
                    this->drawDmgEffScale = 0.55f;
                    this->drawDmgEffAlpha = 2.0f;
                    this->drawDmgEffType = ACTOR_DRAW_DMGEFF_ELECTRIC_SPARKS_MEDIUM;
                    MM_EnWallmas_SetupStun(this);
                } else {
                    if (this->actor.colChkInfo.damageEffect == WALLMASTER_DMGEFF_FIRE_ARROW) {
                        this->drawDmgEffAlpha = 4.0f;
                        this->drawDmgEffScale = 0.55f;
                        this->drawDmgEffType = ACTOR_DRAW_DMGEFF_FIRE;
                    } else if (this->actor.colChkInfo.damageEffect == WALLMASTER_DMGEFF_LIGHT_ARROW) {
                        this->drawDmgEffAlpha = 4.0f;
                        this->drawDmgEffScale = 0.55f;
                        this->drawDmgEffType = ACTOR_DRAW_DMGEFF_LIGHT_ORBS;
                        MM_Actor_Spawn(&play->actorCtx, play, ACTOR_EN_CLEAR_TAG, this->collider.elem.acDmgInfo.hitPos.x,
                                    this->collider.elem.acDmgInfo.hitPos.y, this->collider.elem.acDmgInfo.hitPos.z, 0,
                                    0, 0, CLEAR_TAG_PARAMS(CLEAR_TAG_LARGE_LIGHT_RAYS));
                    }

                    EnWallmas_SetupDamage(this, true);
                }
            }
        }
    }
}

void MM_EnWallmas_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    EnWallmas* this = (EnWallmas*)thisx;

    EnWallmas_UpdateDamage(this, play);
    this->actionFunc(this, play);

    if ((this->actionFunc != MM_EnWallmas_WaitToDrop) && (this->actionFunc != MM_EnWallmas_WaitForProximity) &&
        (this->actionFunc != MM_EnWallmas_TakePlayer) && (this->actionFunc != MM_EnWallmas_WaitForSwitchFlag)) {
        if ((this->actionFunc != MM_EnWallmas_ReturnToCeiling) && (this->actionFunc != MM_EnWallmas_TakePlayer)) {
            Actor_MoveWithGravity(&this->actor);
        }

        if (this->actionFunc != MM_EnWallmas_Drop) {
            MM_Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 25.0f, 0.0f,
                                    UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 |
                                        UPDBGCHECKINFO_FLAG_10);
        }

        if ((this->actionFunc != MM_EnWallmas_Die) && (this->actionFunc != MM_EnWallmas_Drop)) {
            MM_Collider_UpdateCylinder(&this->actor, &this->collider);
            MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            if ((this->actionFunc != EnWallmas_Damage) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                (this->actor.freezeTimer == 0)) {
                MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            }
        }

        MM_Actor_SetFocus(&this->actor, 25.0f);

        if (this->drawDmgEffAlpha > 0.0f) {
            if (this->drawDmgEffType != ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX) {
                MM_Math_StepToF(&this->drawDmgEffAlpha, 0.0f, 0.05f);
                this->drawDmgEffScale = (this->drawDmgEffAlpha + 1.0f) * 0.275f;
                this->drawDmgEffScale = CLAMP_MAX(this->drawDmgEffScale, 0.55f);
            } else if (!MM_Math_StepToF(&this->drawDmgEffFrozenSteamScale, 0.55f, 0.01375f)) {
                Actor_PlaySfx_Flagged(&this->actor, NA_SE_EV_ICE_FREEZE - SFX_FLAG);
            }
        }
    }
}

void EnWallmas_DrawShadow(EnWallmas* this, PlayState* play) {
    s32 pad;
    f32 xzScale;
    MtxF mf;
    Gfx* gfx;

    if ((this->actor.floorPoly != NULL) && ((this->timer < 81) || (this->actionFunc == MM_EnWallmas_Stun))) {
        OPEN_DISPS(play->state.gfxCtx);

        gfx = POLY_OPA_DISP;

        MM_gSPDisplayList(&gfx[0], gSetupDLs[SETUPDL_44]);
        gDPSetPrimColor(&gfx[1], 0, 0, 0, 0, 0, 255);
        func_800C0094(this->actor.floorPoly, this->actor.world.pos.x, this->actor.floorHeight, this->actor.world.pos.z,
                      &mf);
        MM_Matrix_Mult(&mf, MTXMODE_NEW);

        if ((this->actionFunc != MM_EnWallmas_WaitToDrop) && (this->actionFunc != MM_EnWallmas_ReturnToCeiling) &&
            (this->actionFunc != MM_EnWallmas_TakePlayer) && (this->actionFunc != MM_EnWallmas_WaitForSwitchFlag)) {
            xzScale = this->actor.scale.x * 50.0f;
        } else {
            xzScale = CLAMP_MAX(80 - this->timer, 80) * 0.00625f;
        }

        MM_Matrix_Scale(xzScale, 1.0f, xzScale, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(&gfx[2], play->state.gfxCtx);
        MM_gSPDisplayList(&gfx[3], gCircleShadowDL);

        POLY_OPA_DISP = &gfx[4];

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

s32 EnWallmas_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnWallmas* this = (EnWallmas*)thisx;

    if (limbIndex == WALLMASTER_LIMB_ROOT) {
        if (this->actionFunc != MM_EnWallmas_TakePlayer) {
            pos->z -= 1600.0f;
        } else {
            pos->z -= (1600.0f * (this->skelAnime.endFrame - this->skelAnime.curFrame)) / this->skelAnime.endFrame;
        }
    }

    return false;
}

/**
 * This maps a given limb based on its limbIndex to its appropriate index
 * in the bodyPartsPos array.
 */
static s8 sLimbToBodyParts[WALLMASTER_LIMB_MAX] = {
    BODYPART_NONE,         // WALLMASTER_LIMB_NONE
    BODYPART_NONE,         // WALLMASTER_LIMB_ROOT
    BODYPART_NONE,         // WALLMASTER_LIMB_HAND
    BODYPART_NONE,         // WALLMASTER_LIMB_INDEX_FINGER_ROOT
    WALLMASTER_BODYPART_0, // WALLMASTER_LIMB_INDEX_FINGER_PROXIMAL
    BODYPART_NONE,         // WALLMASTER_LIMB_INDEX_FINGER_DISTAL_ROOT
    BODYPART_NONE,         // WALLMASTER_LIMB_INDEX_FINGER_MIDDLE
    WALLMASTER_BODYPART_1, // WALLMASTER_LIMB_INDEX_FINGER_DISTAL
    BODYPART_NONE,         // WALLMASTER_LIMB_RING_FINGER_ROOT
    WALLMASTER_BODYPART_2, // WALLMASTER_LIMB_RING_FINGER_PROXIMAL
    BODYPART_NONE,         // WALLMASTER_LIMB_RING_FINGER_DISTAL_ROOT
    BODYPART_NONE,         // WALLMASTER_LIMB_RING_FINGER_MIDDLE
    WALLMASTER_BODYPART_3, // WALLMASTER_LIMB_RING_FINGER_DISTAL
    BODYPART_NONE,         // WALLMASTER_LIMB_MIDDLE_FINGER_ROOT
    WALLMASTER_BODYPART_4, // WALLMASTER_LIMB_MIDDLE_FINGER_PROXIMAL
    BODYPART_NONE,         // WALLMASTER_LIMB_MIDDLE_FINGER_DISTAL_ROOT
    BODYPART_NONE,         // WALLMASTER_LIMB_MIDDLE_FINGER_MIDDLE
    WALLMASTER_BODYPART_5, // WALLMASTER_LIMB_MIDDLE_FINGER_DISTAL
    BODYPART_NONE,         // WALLMASTER_LIMB_WRIST_ROOT
    BODYPART_NONE,         // WALLMASTER_LIMB_WRIST
    BODYPART_NONE,         // WALLMASTER_LIMB_THUMB_ROOT
    WALLMASTER_BODYPART_6, // WALLMASTER_LIMB_THUMB_PROXIMAL
    WALLMASTER_BODYPART_7, // WALLMASTER_LIMB_THUMB_DISTAL_ROOT
    BODYPART_NONE,         // WALLMASTER_LIMB_THUMB_MIDDLE
    WALLMASTER_BODYPART_8, // WALLMASTER_LIMB_THUMB_DISTAL
};

void EnWallmas_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    EnWallmas* this = (EnWallmas*)thisx;
    Gfx* gfx;

    if (sLimbToBodyParts[limbIndex] != BODYPART_NONE) {
        Matrix_MultZero(&this->bodyPartsPos[sLimbToBodyParts[limbIndex]]);
    }

    if (limbIndex == WALLMASTER_LIMB_WRIST) {
        Matrix_MultVecX(1000.0f, &this->bodyPartsPos[WALLMASTER_BODYPART_9]);
        Matrix_MultVecX(-1000.0f, &this->bodyPartsPos[WALLMASTER_BODYPART_10]);
    } else if (limbIndex == WALLMASTER_LIMB_HAND) {
        OPEN_DISPS(play->state.gfxCtx);

        gfx = POLY_OPA_DISP;

        MM_Matrix_Push();
        MM_Matrix_Translate(1600.0f, -700.0f, -1700.0f, MTXMODE_APPLY);
        Matrix_RotateYS(0x2AAA, MTXMODE_APPLY);
        Matrix_RotateZS(0xAAA, MTXMODE_APPLY);
        MM_Matrix_Scale(2.0f, 2.0f, 2.0f, MTXMODE_APPLY);

        MATRIX_FINALIZE_AND_LOAD(&gfx[0], play->state.gfxCtx);
        MM_gSPDisplayList(&gfx[1], gWallmasterLittleFingerDL);

        POLY_OPA_DISP = &gfx[2];

        MM_Matrix_Pop();

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void MM_EnWallmas_Draw(Actor* thisx, PlayState* play) {
    EnWallmas* this = (EnWallmas*)thisx;

    if (this->actionFunc != MM_EnWallmas_WaitToDrop) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        MM_SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                              EnWallmas_OverrideLimbDraw, EnWallmas_PostLimbDraw, &this->actor);
        Actor_DrawDamageEffects(play, &this->actor, this->bodyPartsPos, WALLMASTER_BODYPART_MAX, this->drawDmgEffScale,
                                this->drawDmgEffFrozenSteamScale, this->drawDmgEffAlpha, this->drawDmgEffType);
    }

    if (this->actor.colorFilterTimer != 0) {
        func_800AE5A0(play);
    }

    EnWallmas_DrawShadow(this, play);
}
