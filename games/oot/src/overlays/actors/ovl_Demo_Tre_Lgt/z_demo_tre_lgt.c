#include "z_demo_tre_lgt.h"
#include "overlays/actors/ovl_En_Box/z_en_box.h"
#include "objects/object_box/object_box.h"
#include <assert.h>

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void OoT_DemoTreLgt_Init(Actor* thisx, PlayState* play);
void OoT_DemoTreLgt_Destroy(Actor* thisx, PlayState* play);
void OoT_DemoTreLgt_Update(Actor* thisx, PlayState* play);
void OoT_DemoTreLgt_Draw(Actor* thisx, PlayState* play);

void func_80993848(DemoTreLgt* this, PlayState* play);
void func_80993754(DemoTreLgt* this);
void func_8099375C(DemoTreLgt* this, PlayState* play);
void func_809937B4(DemoTreLgt* this, PlayState* play, f32 currentFrame);

typedef struct {
    /* 0x00 */ f32 startFrame;
    /* 0x04 */ f32 endFrame;
    /* 0x08 */ f32 unk_08;
    /* 0x0C */ f32 unk_0C;
} DemoTreLgtInfo; // size = 0x10

static DemoTreLgtInfo sDemoTreLgtInfo[] = {
    { 1.0f, 136.0f, 190.0f, 40.0f },
    { 1.0f, 136.0f, 220.0f, 50.0f },
};

const ActorInit Demo_Tre_Lgt_InitVars = {
    ACTOR_DEMO_TRE_LGT,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_BOX,
    sizeof(DemoTreLgt),
    (ActorFunc)OoT_DemoTreLgt_Init,
    (ActorFunc)OoT_DemoTreLgt_Destroy,
    (ActorFunc)OoT_DemoTreLgt_Update,
    (ActorFunc)OoT_DemoTreLgt_Draw,
    NULL,
};

static TransformUpdateIndex* sTransformUpdIdx[] = { &gTreasureChestCurveAnim_4B60, &gTreasureChestCurveAnim_4F70 };

static DemoTreLgtActionFunc OoT_sActionFuncs[] = {
    func_8099375C,
    func_80993848,
};

void OoT_DemoTreLgt_Init(Actor* thisx, PlayState* play) {
    DemoTreLgt* this = (DemoTreLgt*)thisx;

    if (!OoT_SkelCurve_Init(play, &this->skelCurve, &gTreasureChestCurveSkel, sTransformUpdIdx[0])) {
        // "Demo_Tre_Lgt_Actor_ct (); Construct failed"
        osSyncPrintf("Demo_Tre_Lgt_Actor_ct();コンストラクト失敗\n");
    }

    assert(true);

    this->unk_170 = 255;
    this->unk_174 = 255;
    this->status = 0;
    func_80993754(this);
}

void OoT_DemoTreLgt_Destroy(Actor* thisx, PlayState* play) {
    DemoTreLgt* this = (DemoTreLgt*)thisx;

    OoT_SkelCurve_Destroy(play, &this->skelCurve);
}

void func_80993754(DemoTreLgt* this) {
    this->action = DEMO_TRE_LGT_ACTION_WAIT;
}

void func_8099375C(DemoTreLgt* this, PlayState* play) {
    EnBox* treasureChest = (EnBox*)this->actor.parent;

    if (treasureChest != NULL && OoT_Animation_OnFrame(&treasureChest->skelanime, 10.0f)) {
        func_809937B4(this, play, treasureChest->skelanime.curFrame);
    }
}

void func_809937B4(DemoTreLgt* this, PlayState* play, f32 currentFrame) {
    SkelAnimeCurve* skelCurve = &this->skelCurve;
    s32 pad[2];

    this->action = DEMO_TRE_LGT_ACTION_ANIMATE;

    OoT_SkelCurve_SetAnim(skelCurve, sTransformUpdIdx[gSaveContext.linkAge], 1.0f,
                      sDemoTreLgtInfo[gSaveContext.linkAge].endFrame + sDemoTreLgtInfo[gSaveContext.linkAge].unk_08,
                      currentFrame, 1.0f);
    OoT_SkelCurve_Update(play, skelCurve);
}

void func_80993848(DemoTreLgt* this, PlayState* play) {
    f32 currentFrame = this->skelCurve.animCurFrame;

    if (currentFrame < sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].endFrame) {
        this->unk_170 = 255;
    } else {
        if (currentFrame <= (sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].endFrame +
                             sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].unk_08)) {
            this->unk_170 = ((((sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].endFrame - currentFrame) /
                               sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].unk_08) *
                              255.0f) +
                             255.0f);
        } else {
            this->unk_170 = 0;
        }
    }
    if (currentFrame < sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].unk_0C) {
        this->unk_174 = 255;
    } else if (currentFrame < (sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].unk_0C + 10.0f)) {
        this->unk_174 =
            ((((sDemoTreLgtInfo[((void)0, gSaveContext.linkAge)].unk_0C - currentFrame) / 10.0f) * 255.0f) + 255.0f);
    } else {
        this->unk_174 = 0;
    }
    if ((currentFrame > 30.0f) && !(this->status & 1)) {
        this->status |= 1;
        Audio_PlaySoundGeneral(NA_SE_EV_TRE_BOX_FLASH, &this->actor.projectedPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                               &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
    }
    if (OoT_SkelCurve_Update(play, &this->skelCurve)) {
        OoT_Actor_Kill(&this->actor);
    }
}

void OoT_DemoTreLgt_Update(Actor* thisx, PlayState* play) {
    DemoTreLgt* this = (DemoTreLgt*)thisx;

    OoT_sActionFuncs[this->action](this, play);
}

s32 DemoTreLgt_PostLimbDraw(PlayState* play, SkelAnimeCurve* skelCurve, s32 limbIndex, void* thisx) {
    s32 pad;
    DemoTreLgt* this = (DemoTreLgt*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               OoT_Gfx_TwoTexScroll(play->state.gfxCtx, 0, (play->state.frames * 2) % 256, 0, 64, 32, 1,
                                (play->state.frames * -2) % 256, 0, 64, 32));

    if (limbIndex == 1) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 180, this->unk_170);
    } else if ((limbIndex == 13) || (limbIndex == 7) || (limbIndex == 4) || (limbIndex == 10)) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 180, this->unk_174);
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return 1;
}

void OoT_DemoTreLgt_Draw(Actor* thisx, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    DemoTreLgt* this = (DemoTreLgt*)thisx;

    OPEN_DISPS(gfxCtx);

    if (this->action != DEMO_TRE_LGT_ACTION_ANIMATE) {
        return;
    }

    Gfx_SetupDL_25Xlu(gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 255, 0, 0);
    OoT_SkelCurve_Draw(&this->actor, play, &this->skelCurve, DemoTreLgt_PostLimbDraw, NULL, 1, thisx);

    CLOSE_DISPS(gfxCtx);
}
