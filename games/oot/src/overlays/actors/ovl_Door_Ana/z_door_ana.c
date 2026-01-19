/*
 * File: z_door_ana.c
 * Overlay: ovl_Door_Ana
 * Description: Grottos Entrances/Exits
 */

#include "z_door_ana.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/OTRGlobals.h"

#define FLAGS ACTOR_FLAG_UPDATE_DURING_OCARINA

void OoT_DoorAna_Init(Actor* thisx, PlayState* play);
void OoT_DoorAna_Destroy(Actor* thisx, PlayState* play);
void OoT_DoorAna_Update(Actor* thisx, PlayState* play);
void OoT_DoorAna_Draw(Actor* thisx, PlayState* play);

void OoT_DoorAna_WaitClosed(DoorAna* this, PlayState* play);
void OoT_DoorAna_WaitOpen(DoorAna* this, PlayState* play);
void DoorAna_GrabPlayer(DoorAna* this, PlayState* play);

const ActorInit Door_Ana_InitVars = {
    ACTOR_DOOR_ANA,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_FIELD_KEEP,
    sizeof(DoorAna),
    (ActorFunc)OoT_DoorAna_Init,
    (ActorFunc)OoT_DoorAna_Destroy,
    (ActorFunc)OoT_DoorAna_Update,
    (ActorFunc)OoT_DoorAna_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000048, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 50, 10, 0, { 0 } },
};

// array of entrance table entries to grotto destinations
static s16 sGrottoEntrances[] = {
    ENTR_FAIRYS_FOUNTAIN_0, ENTR_GROTTOS_0,  ENTR_GROTTOS_1,  ENTR_GROTTOS_2,  ENTR_GROTTOS_3,
    ENTR_GROTTOS_4,         ENTR_GROTTOS_5,  ENTR_GROTTOS_6,  ENTR_GROTTOS_7,  ENTR_GROTTOS_8,
    ENTR_GROTTOS_9,         ENTR_GROTTOS_10, ENTR_GROTTOS_11, ENTR_GROTTOS_12, ENTR_GROTTOS_13,
};

void OoT_DoorAna_SetupAction(DoorAna* this, DoorAnaActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_DoorAna_Init(Actor* thisx, PlayState* play) {
    DoorAna* this = (DoorAna*)thisx;

    this->actor.shape.rot.z = 0;
    this->actor.shape.rot.y = this->actor.shape.rot.z;
    // init block for grottos that are initially "hidden" (require explosives/hammer/song of storms to open)
    if ((this->actor.params & 0x300) != 0) {
        // only allocate collider for grottos that need bombing/hammering open
        if ((this->actor.params & 0x200) != 0) {
            OoT_Collider_InitCylinder(play, &this->collider);
            OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
        } else {
            this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        }
        OoT_Actor_SetScale(&this->actor, 0);
        OoT_DoorAna_SetupAction(this, OoT_DoorAna_WaitClosed);
    } else {
        OoT_DoorAna_SetupAction(this, OoT_DoorAna_WaitOpen);
    }
    this->actor.targetMode = 0;
}

void OoT_DoorAna_Destroy(Actor* thisx, PlayState* play) {
    DoorAna* this = (DoorAna*)thisx;

    // free collider if it has one
    if ((this->actor.params & 0x200) != 0) {
        OoT_Collider_DestroyCylinder(play, &this->collider);
    }
}

// update routine for grottos that are currently "hidden"/unopened
void OoT_DoorAna_WaitClosed(DoorAna* this, PlayState* play) {
    u32 openGrotto = false;

    if (!(this->actor.params & 0x200)) {
        // opening with song of storms
        if (this->actor.xyzDistToPlayerSq < 40000.0f && Flags_GetEnv(play, 5)) {
            openGrotto = true;
            this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        }
    } else {
        // bombing/hammering open a grotto
        if (this->collider.base.acFlags & AC_HIT) {
            openGrotto = true;
            OoT_Collider_DestroyCylinder(play, &this->collider);
        } else {
            OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
            OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
    // open the grotto
    if (openGrotto) {
        this->actor.params &= ~0x0300;
        OoT_DoorAna_SetupAction(this, OoT_DoorAna_WaitOpen);
        Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                               &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
    }
    func_8002F5F0(&this->actor, play);
}

// update routine for grottos that are open
void OoT_DoorAna_WaitOpen(DoorAna* this, PlayState* play) {
    Player* player;
    s32 destinationIdx;

    player = GET_PLAYER(play);
    if (OoT_Math_StepToF(&this->actor.scale.x, 0.01f, 0.001f)) {
        if ((this->actor.targetMode != 0) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
            (player->stateFlags1 & PLAYER_STATE1_FLOOR_DISABLED) && (player->av1.actionVar1 == 0)) {
            destinationIdx = ((this->actor.params >> 0xC) & 7) - 1;
            OoT_Play_SetupRespawnPoint(play, RESPAWN_MODE_RETURN, 0x4FF);
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.y = this->actor.world.pos.y;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].yaw = this->actor.home.rot.y;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].data = this->actor.params & 0xFFFF;
            if (destinationIdx < 0) {
                destinationIdx = this->actor.home.rot.z + 1;
            }
            play->nextEntranceIndex = sGrottoEntrances[destinationIdx];

            // In ER, load the correct entrance based on the grotto link is falling into
            if (IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_ENTRANCES)) {
                Grotto_OverrideActorEntrance(&this->actor);
            }

            OoT_DoorAna_SetupAction(this, DoorAna_GrabPlayer);
        } else {
            if (!OoT_Player_InCsMode(play) && !(player->stateFlags1 & (PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER)) &&
                this->actor.xzDistToPlayer <= 15.0f && -50.0f <= this->actor.yDistToPlayer &&
                this->actor.yDistToPlayer <= 15.0f) {
                player->stateFlags1 |= PLAYER_STATE1_FLOOR_DISABLED;
                this->actor.targetMode = 1;
            } else {
                this->actor.targetMode = 0;
            }
        }
    }
    OoT_Actor_SetScale(&this->actor, this->actor.scale.x);
}

// update function for after the player has triggered the grotto
void DoorAna_GrabPlayer(DoorAna* this, PlayState* play) {
    Player* player;

    if (this->actor.yDistToPlayer <= 0.0f && 15.0f < this->actor.xzDistToPlayer) {
        player = GET_PLAYER(play);
        player->actor.world.pos.x = OoT_Math_SinS(this->actor.yawTowardsPlayer) * 15.0f + this->actor.world.pos.x;
        player->actor.world.pos.z = OoT_Math_CosS(this->actor.yawTowardsPlayer) * 15.0f + this->actor.world.pos.z;
    }
}

void OoT_DoorAna_Update(Actor* thisx, PlayState* play) {
    DoorAna* this = (DoorAna*)thisx;

    this->actionFunc(this, play);
    // Changes the grottos facing angle based on camera angle
    if (!CVarGetInteger(CVAR_ENHANCEMENT("DisableGrottoRotation"), 0)) {
        this->actor.shape.rot.y = OoT_Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
    }
}

void OoT_DoorAna_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, gGrottoDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
