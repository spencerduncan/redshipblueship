/*
 * File: z_en_eg.c
 * Overlay: ovl_En_Eg
 * Description: Triggers a void out (used in the tower collapse sequence?)
 */

#include "z_en_eg.h"
#include "vt.h"

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void EnEg_Init(Actor* thisx, PlayState* play);
void EnEg_Destroy(Actor* thisx, PlayState* play);
void EnEg_Update(Actor* thisx, PlayState* play);
void EnEg_Draw(Actor* thisx, PlayState* play);

void func_809FFDC8(EnEg* this, PlayState* play);

static s32 voided = false;

static EnEgActionFunc OoT_sActionFuncs[] = {
    func_809FFDC8,
};

#ifdef RSBS_SINGLE_EXECUTABLE
void OoT_EnEg_Reset(void);
#endif

const ActorInit En_Eg_InitVars = {
    ACTOR_EN_EG,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_ZL2,
    sizeof(EnEg),
    (ActorFunc)EnEg_Init,
    (ActorFunc)EnEg_Destroy,
    (ActorFunc)EnEg_Update,
    (ActorFunc)EnEg_Draw,
#ifdef RSBS_SINGLE_EXECUTABLE
    (ActorResetFunc)OoT_EnEg_Reset,
#else
    NULL,
#endif
};

void EnEg_PlayVoidOutSFX() {
    Sfx_PlaySfxCentered2(NA_SE_OC_ABYSS);
}

void EnEg_Destroy(Actor* thisx, PlayState* play) {
    voided = false;
}

void EnEg_Init(Actor* thisx, PlayState* play) {
    EnEg* this = (EnEg*)thisx;

    this->action = 0;
}

void func_809FFDC8(EnEg* this, PlayState* play) {
    if (!voided && (gSaveContext.subTimerSeconds < 1) && OoT_Flags_GetSwitch(play, 0x36) && (kREG(0) == 0)) {
        // Void the player out
        Play_TriggerRespawn(play);
        gSaveContext.respawnFlag = -2;
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        EnEg_PlayVoidOutSFX();
        voided = true;
    }
}

void EnEg_Update(Actor* thisx, PlayState* play) {
    EnEg* this = (EnEg*)thisx;
    s32 action = this->action;

    if (((action < 0) || (0 < action)) || (OoT_sActionFuncs[action] == NULL)) {
        // "Main Mode is wrong!!!!!!!!!!!!!!!!!!!!!!!!!"
        osSyncPrintf(VT_FGCOL(RED) "メインモードがおかしい!!!!!!!!!!!!!!!!!!!!!!!!!\n" VT_RST);
    } else {
        OoT_sActionFuncs[action](this, play);
    }
}

void EnEg_Draw(Actor* thisx, PlayState* play) {
}

#ifdef RSBS_SINGLE_EXECUTABLE
// [RSBS #750] EnEg_Destroy clears voided, the once-only latch of the tower-collapse void-out trigger. Nothing but
// Destroy ever put it back. A cross-game departure abandons OoT's Play gamestate without deleting its actors
// (OoT_RetireAbandonedSession, GameExports_SingleExe.cpp), so it kept the abandoned session's value.
// OoT_Actor_FreeOverlay calls this only once the overlay has no clients, when every Destroy has already restored the
// initial value, so on a normal teardown it changes nothing.
void OoT_EnEg_Reset(void) {
    voided = false;
}

// [RSBS #750] Seed and read the static(s) above for the oot-abandoned-session-statics row
// (games/oot/soh/oot_abandoned_session_test.cpp): dirty != 0 puts them where a live client leaves them, 0 puts back
// the initial value; the check is nonzero while any is not at its initial value.
void OoT_EnEg_SetDestroyStaticsForTest(s32 dirty) {
    if (dirty) {
        voided = true;
    } else {
        voided = false;
    }
}

s32 OoT_EnEg_DestroyStaticsDirtyForTest(void) {
    return voided != false;
}
#endif
