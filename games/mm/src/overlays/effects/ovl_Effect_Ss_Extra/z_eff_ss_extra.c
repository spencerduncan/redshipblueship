/*
 * File: z_eff_ss_extra.c
 * Overlay: ovl_Effect_Ss_Extra
 * Description: The floating points that pop-up in Swamp Bow Minigame
 *   i.e. 100 points for hitting the deku scrubs in the background
 */

#include "z_eff_ss_extra.h"
#include "objects/object_yabusame_point/object_yabusame_point.h"

#define PARAMS ((EffectSsExtraInitParams*)initParamsx)

u32 MM_EffectSsExtra_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx);
void MM_EffectSsExtra_Update(PlayState* play, u32 index, EffectSs* this);
void MM_EffectSsExtra_Draw(PlayState* play, u32 index, EffectSs* this);

static s16 MM_sScores[] = { EXTRA_SCORE_30, EXTRA_SCORE_60, EXTRA_SCORE_100 };

EffectSsProfile Effect_Ss_Extra_Profile = {
    EFFECT_SS_EXTRA,
    MM_EffectSsExtra_Init,
};

static TexturePtr sPointTextures[] = { gYabusamePoint30Tex, gYabusamePoint60Tex, gYabusamePoint100Tex };

#define rObjectSlot regs[0]
#define rTimer regs[1]
#define rScoreIndex regs[2]
#define rScale regs[3]

u32 MM_EffectSsExtra_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx) {
    s32 pad;
    EffectSsExtraInitParams* params = PARAMS;
    s32 objectSlot;

    objectSlot = Object_GetSlot(&play->objectCtx, OBJECT_YABUSAME_POINT);
    if ((objectSlot > OBJECT_SLOT_NONE) && MM_Object_IsLoaded(&play->objectCtx, objectSlot)) {
        uintptr_t segBackup = MM_gSegments[6];

        MM_gSegments[6] = OS_K0_TO_PHYSICAL(play->objectCtx.slots[objectSlot].segment);

        this->pos = params->pos;
        this->velocity = params->velocity;
        this->accel = params->accel;
        this->draw = MM_EffectSsExtra_Draw;
        this->update = MM_EffectSsExtra_Update;
        this->life = 50;
        this->rScoreIndex = params->scoreIndex;
        this->rScale = params->scale;
        this->rTimer = 5;
        this->rObjectSlot = objectSlot;

        MM_gSegments[6] = segBackup;
        return 1;
    }
    return 0;
}

void MM_EffectSsExtra_Draw(PlayState* play, u32 index, EffectSs* this) {
    s32 pad;
    f32 scale;
    void* objectPtr;

    scale = this->rScale / 100.0f;
    objectPtr = play->objectCtx.slots[this->rObjectSlot].segment;

    OPEN_DISPS(play->state.gfxCtx);

    MM_gSegments[6] = OS_K0_TO_PHYSICAL(objectPtr);

    gSPSegment(POLY_XLU_DISP++, 0x06, objectPtr);

    MM_Matrix_Translate(this->pos.x, this->pos.y, this->pos.z, MTXMODE_NEW);
    MM_Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MM_Matrix_ReplaceRotation(&play->billboardMtxF);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08, Lib_SegmentedToVirtual(sPointTextures[this->rScoreIndex]));

    gSPDisplayList(POLY_XLU_DISP++, &gYabusamePointDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MM_EffectSsExtra_Update(PlayState* play, u32 index, EffectSs* this) {
    if (this->rTimer != 0) {
        this->rTimer--;
    } else {
        this->velocity.y = 0.0f;
    }

    if (this->rTimer == 1) {
        play->interfaceCtx.minigamePoints = MM_sScores[this->rScoreIndex];
    }
}
