#include "z_arms_hook.h"
#include "objects/object_link_boy/object_link_boy.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void OoT_ArmsHook_Init(Actor* thisx, PlayState* play);
void OoT_ArmsHook_Destroy(Actor* thisx, PlayState* play);
void OoT_ArmsHook_Update(Actor* thisx, PlayState* play);
void OoT_ArmsHook_Draw(Actor* thisx, PlayState* play);

void OoT_ArmsHook_Wait(ArmsHook* this, PlayState* play);
void OoT_ArmsHook_Shoot(ArmsHook* this, PlayState* play);

const ActorInit Arms_Hook_InitVars = {
    ACTOR_ARMS_HOOK,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_LINK_BOY,
    sizeof(ArmsHook),
    (ActorFunc)OoT_ArmsHook_Init,
    (ActorFunc)OoT_ArmsHook_Destroy,
    (ActorFunc)OoT_ArmsHook_Update,
    (ActorFunc)OoT_ArmsHook_Draw,
    NULL,
};

static ColliderQuadInit OoT_sQuadInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000080, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static Vec3f sUnusedVec1 = { 0.0f, 0.5f, 0.0f };
static Vec3f sUnusedVec2 = { 0.0f, 0.5f, 0.0f };

static Color_RGB8 sUnusedColors[] = {
    { 255, 255, 100 },
    { 255, 255, 50 },
};

static Vec3f D_80865B70 = { 0.0f, 0.0f, 0.0f };
static Vec3f D_80865B7C = { 0.0f, 0.0f, 900.0f };
static Vec3f D_80865B88 = { 0.0f, 500.0f, -3000.0f };
static Vec3f D_80865B94 = { 0.0f, -500.0f, -3000.0f };
static Vec3f D_80865BA0 = { 0.0f, 500.0f, 1200.0f };
static Vec3f D_80865BAC = { 0.0f, -500.0f, 1200.0f };

void OoT_ArmsHook_SetupAction(ArmsHook* this, ArmsHookActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_ArmsHook_Init(Actor* thisx, PlayState* play) {
    ArmsHook* this = (ArmsHook*)thisx;

    OoT_Collider_InitQuad(play, &this->collider);
    OoT_Collider_SetQuad(play, &this->collider, &this->actor, &OoT_sQuadInit);
    OoT_ArmsHook_SetupAction(this, OoT_ArmsHook_Wait);
    this->unk_1E8 = this->actor.world.pos;
}

void OoT_ArmsHook_Destroy(Actor* thisx, PlayState* play) {
    ArmsHook* this = (ArmsHook*)thisx;

    if (this->grabbed != NULL) {
        this->grabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
    }
    OoT_Collider_DestroyQuad(play, &this->collider);
}

void OoT_ArmsHook_Wait(ArmsHook* this, PlayState* play) {
    if (this->actor.parent == NULL) {
        Player* player = GET_PLAYER(play);
        // get correct timer length for hookshot or longshot
        s32 length = ((player->heldItemAction == PLAYER_IA_HOOKSHOT) ? 13 : 26) *
                     CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f);

        OoT_ArmsHook_SetupAction(this, OoT_ArmsHook_Shoot);
        Actor_SetProjectileSpeed(&this->actor, 20.0f);
        this->actor.parent = &GET_PLAYER(play)->actor;
        this->timer = length;
    }
}

void func_80865044(ArmsHook* this) {
    this->actor.child = this->actor.parent;
    this->actor.parent->parent = &this->actor;
}

s32 OoT_ArmsHook_AttachToPlayer(ArmsHook* this, Player* player) {
    player->actor.child = &this->actor;
    player->heldActor = &this->actor;
    if (this->actor.child != NULL) {
        player->actor.parent = NULL;
        this->actor.child = NULL;
        return true;
    }
    return false;
}

void ArmsHook_DetachHookFromActor(ArmsHook* this) {
    if (this->grabbed != NULL) {
        this->grabbed->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        this->grabbed = NULL;
    }
}

s32 OoT_ArmsHook_CheckForCancel(ArmsHook* this) {
    Player* player = (Player*)this->actor.parent;

    if (Player_HoldsHookshot(player)) {
        if ((player->itemAction != player->heldItemAction) || (player->actor.flags & ACTOR_FLAG_TALK) ||
            ((player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_DAMAGED)))) {
            this->timer = 0;
            ArmsHook_DetachHookFromActor(this);
            OoT_Math_Vec3f_Copy(&this->actor.world.pos, &player->unk_3C8);
            return 1;
        }
    }
    return 0;
}

void ArmsHook_AttachHookToActor(ArmsHook* this, Actor* actor) {
    actor->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
    this->grabbed = actor;
    OoT_Math_Vec3f_Diff(&actor->world.pos, &this->actor.world.pos, &this->grabbedDistDiff);
}

void OoT_ArmsHook_Shoot(ArmsHook* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* touchedActor;
    Actor* grabbed;
    Vec3f bodyDistDiffVec;
    Vec3f newPos;
    f32 bodyDistDiff;
    f32 phi_f16;
    DynaPolyActor* dynaPolyActor;
    f32 sp94;
    f32 sp90;
    s32 pad;
    CollisionPoly* poly;
    s32 bgId;
    Vec3f sp78;
    Vec3f prevFrameDiff;
    Vec3f sp60;
    f32 sp5C;
    f32 sp58;
    f32 velocity;
    s32 pad1;

    if ((this->actor.parent == NULL) || (!Player_HoldsHookshot(player))) {
        ArmsHook_DetachHookFromActor(this);
        OoT_Actor_Kill(&this->actor);
        return;
    }

    func_8002F8F0(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    OoT_ArmsHook_CheckForCancel(this);

    if ((this->timer != 0) && (this->collider.base.atFlags & AT_HIT) &&
        (this->collider.info.atHitInfo->elemType != ELEMTYPE_UNK4)) {
        touchedActor = this->collider.base.at;
        if ((touchedActor->update != NULL) &&
            (touchedActor->flags & (ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER))) {
            if (this->collider.info.atHitInfo->bumperFlags & BUMP_HOOKABLE) {
                ArmsHook_AttachHookToActor(this, touchedActor);
                if (CHECK_FLAG_ALL(touchedActor->flags, ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER)) {
                    func_80865044(this);
                }
            }
        }
        this->timer = 0;
        Audio_PlaySoundGeneral(NA_SE_IT_ARROW_STICK_CRE, &this->actor.projectedPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                               &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
    } else if (DECR(this->timer) == 0) {
        grabbed = this->grabbed;
        if (grabbed != NULL) {
            if ((grabbed->update == NULL) || !CHECK_FLAG_ALL(grabbed->flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
                grabbed = NULL;
                this->grabbed = NULL;
            } else if (this->actor.child != NULL) {
                sp94 = OoT_Actor_WorldDistXYZToActor(&this->actor, grabbed);
                sp90 = OoT_sqrtf(SQ(this->grabbedDistDiff.x) + SQ(this->grabbedDistDiff.y) + SQ(this->grabbedDistDiff.z));
                OoT_Math_Vec3f_Diff(&grabbed->world.pos, &this->grabbedDistDiff, &this->actor.world.pos);
                if (50.0f < (sp94 - sp90)) {
                    ArmsHook_DetachHookFromActor(this);
                    grabbed = NULL;
                }
            }
        }

        bodyDistDiff = OoT_Math_Vec3f_DistXYZAndStoreDiff(&player->unk_3C8, &this->actor.world.pos, &bodyDistDiffVec);
        if (bodyDistDiff < 30.0f) {
            velocity = 0.0f;
            phi_f16 = 0.0f;
        } else {
            if (this->actor.child != NULL) {
                velocity = 30.0f;
            } else if (grabbed != NULL) {
                velocity = 50.0f;
            } else {
                velocity = 200.0f;
            }
            phi_f16 = bodyDistDiff - velocity;
            if (bodyDistDiff <= velocity) {
                phi_f16 = 0.0f;
            }
            velocity = phi_f16 / bodyDistDiff;
        }

        newPos.x = bodyDistDiffVec.x * velocity;
        newPos.y = bodyDistDiffVec.y * velocity;
        newPos.z = bodyDistDiffVec.z * velocity;

        if (this->actor.child == NULL) {
            if ((grabbed != NULL) && (grabbed->id == ACTOR_BG_SPOT06_OBJECTS)) {
                OoT_Math_Vec3f_Diff(&grabbed->world.pos, &this->grabbedDistDiff, &this->actor.world.pos);
                phi_f16 = 1.0f;
            } else {
                OoT_Math_Vec3f_Sum(&player->unk_3C8, &newPos, &this->actor.world.pos);
                if (grabbed != NULL) {
                    OoT_Math_Vec3f_Sum(&this->actor.world.pos, &this->grabbedDistDiff, &grabbed->world.pos);
                }
            }
        } else {
            OoT_Math_Vec3f_Diff(&bodyDistDiffVec, &newPos, &player->actor.velocity);
            player->actor.world.rot.x =
                OoT_Math_Atan2S(OoT_sqrtf(SQ(bodyDistDiffVec.x) + SQ(bodyDistDiffVec.z)), -bodyDistDiffVec.y);
        }

        if (phi_f16 < 50.0f) {
            ArmsHook_DetachHookFromActor(this);
            if (phi_f16 == 0.0f) {
                OoT_ArmsHook_SetupAction(this, OoT_ArmsHook_Wait);
                if (OoT_ArmsHook_AttachToPlayer(this, player)) {
                    OoT_Math_Vec3f_Diff(&this->actor.world.pos, &player->actor.world.pos, &player->actor.velocity);
                    player->actor.velocity.y -= 20.0f;
                }
            }
        }
    } else {
        Actor_MoveXZGravity(&this->actor);
        OoT_Math_Vec3f_Diff(&this->actor.world.pos, &this->actor.prevPos, &prevFrameDiff);
        OoT_Math_Vec3f_Sum(&this->unk_1E8, &prevFrameDiff, &this->unk_1E8);
        this->actor.shape.rot.x = OoT_Math_Atan2S(this->actor.speedXZ, -this->actor.velocity.y);
        sp60.x = this->unk_1F4.x - (this->unk_1E8.x - this->unk_1F4.x);
        sp60.y = this->unk_1F4.y - (this->unk_1E8.y - this->unk_1F4.y);
        sp60.z = this->unk_1F4.z - (this->unk_1E8.z - this->unk_1F4.z);
        u16 buttonsToCheck = BTN_A | BTN_B | BTN_R | BTN_CUP | BTN_CLEFT | BTN_CRIGHT | BTN_CDOWN;
        if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
            buttonsToCheck |= BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
        }
        if (OoT_BgCheck_EntityLineTest1(&play->colCtx, &sp60, &this->unk_1E8, &sp78, &poly, true, true, true, true,
                                    &bgId) &&
            !func_8002F9EC(play, &this->actor, poly, bgId, &sp78)) {
            sp5C = COLPOLY_GET_NORMAL(poly->normal.x);
            sp58 = COLPOLY_GET_NORMAL(poly->normal.z);
            OoT_Math_Vec3f_Copy(&this->actor.world.pos, &sp78);
            this->actor.world.pos.x += 10.0f * sp5C;
            this->actor.world.pos.z += 10.0f * sp58;
            this->timer = 0;
            if (OoT_SurfaceType_IsHookshotSurface(&play->colCtx, poly, bgId)) {
                if (bgId != BGCHECK_SCENE) {
                    dynaPolyActor = OoT_DynaPoly_GetActor(&play->colCtx, bgId);
                    if (dynaPolyActor != NULL) {
                        ArmsHook_AttachHookToActor(this, &dynaPolyActor->actor);
                    }
                }
                func_80865044(this);
                Audio_PlaySoundGeneral(NA_SE_IT_HOOKSHOT_STICK_OBJ, &this->actor.projectedPos, 4,
                                       &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
            } else {
                OoT_CollisionCheck_SpawnShieldParticlesMetal(play, &this->actor.world.pos);
                Audio_PlaySoundGeneral(NA_SE_IT_HOOKSHOT_REFLECT, &this->actor.projectedPos, 4,
                                       &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
            }
        } else if (CHECK_BTN_ANY(play->state.input[0].press.button, (buttonsToCheck))) {
            this->timer = 0;
        }
    }
}

void OoT_ArmsHook_Update(Actor* thisx, PlayState* play) {
    ArmsHook* this = (ArmsHook*)thisx;

    this->actionFunc(this, play);
    this->unk_1F4 = this->unk_1E8;
}

void OoT_ArmsHook_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    ArmsHook* this = (ArmsHook*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f sp78;
    Vec3f sp6C;
    Vec3f sp60;
    f32 sp5C;
    f32 sp58;

    if ((player->actor.draw != NULL) && (player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT)) {
        OPEN_DISPS(play->state.gfxCtx);

        if ((OoT_ArmsHook_Shoot != this->actionFunc) || (this->timer <= 0)) {
            OoT_Matrix_MultVec3f(&D_80865B70, &this->unk_1E8);
            OoT_Matrix_MultVec3f(&D_80865B88, &sp6C);
            OoT_Matrix_MultVec3f(&D_80865B94, &sp60);
            this->hookInfo.active = 0;
        } else {
            OoT_Matrix_MultVec3f(&D_80865B7C, &this->unk_1E8);
            OoT_Matrix_MultVec3f(&D_80865BA0, &sp6C);
            OoT_Matrix_MultVec3f(&D_80865BAC, &sp60);
        }

        func_80090480(play, &this->collider, &this->hookInfo, &sp6C, &sp60);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
            CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD) {
            OoT_Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gLinkAdultHookshotTipDL);
        OoT_Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
        OoT_Math_Vec3f_Diff(&player->unk_3C8, &this->actor.world.pos, &sp78);
        sp58 = SQ(sp78.x) + SQ(sp78.z);
        sp5C = OoT_sqrtf(sp58);
        Matrix_RotateY(OoT_Math_FAtan2F(sp78.x, sp78.z), MTXMODE_APPLY);
        Matrix_RotateX(OoT_Math_FAtan2F(-sp78.y, sp5C), MTXMODE_APPLY);
        if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
            CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD) {
            OoT_Matrix_Scale(0.012f, 0.012f, OoT_sqrtf(SQ(sp78.y) + sp58) * 0.01f, MTXMODE_APPLY);
        } else {
            OoT_Matrix_Scale(0.015f, 0.015f, OoT_sqrtf(SQ(sp78.y) + sp58) * 0.01f, MTXMODE_APPLY);
        }
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gLinkAdultHookshotChainDL);

        CLOSE_DISPS(play->state.gfxCtx);
    }
}
