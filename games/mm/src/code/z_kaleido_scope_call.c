#include "z64pause_menu.h"

#include "z64.h"
#include "z64shrink_window.h"

void (*MM_sKaleidoScopeUpdateFunc)(PlayState* play);
void (*MM_sKaleidoScopeDrawFunc)(PlayState* play);

extern void MM_KaleidoScope_Update(PlayState* play);
extern void MM_KaleidoScope_Draw(PlayState* play);

void MM_KaleidoScopeCall_LoadPlayer(void) {
    KaleidoMgrOverlay* playerActorOvl = &MM_gKaleidoMgrOverlayTable[KALEIDO_OVL_PLAYER_ACTOR];

    if (MM_gKaleidoMgrCurOvl != playerActorOvl) {
        if (MM_gKaleidoMgrCurOvl != NULL) {
            MM_KaleidoManager_ClearOvl(MM_gKaleidoMgrCurOvl);
        }

        MM_KaleidoManager_LoadOvl(playerActorOvl);
    }
}

void MM_KaleidoScopeCall_Init(PlayState* play) {
    MM_sKaleidoScopeUpdateFunc = MM_KaleidoManager_GetRamAddr(MM_KaleidoScope_Update);
    MM_sKaleidoScopeDrawFunc = MM_KaleidoManager_GetRamAddr(MM_KaleidoScope_Draw);
    MM_KaleidoSetup_Init(play);
}

void MM_KaleidoScopeCall_Destroy(PlayState* play) {
    MM_KaleidoSetup_Destroy(play);
}

void MM_KaleidoScopeCall_Update(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    KaleidoMgrOverlay* kaleidoScopeOvl = &MM_gKaleidoMgrOverlayTable[KALEIDO_OVL_KALEIDO_SCOPE];

    if (!IS_PAUSED(&play->pauseCtx)) {
        return;
    }

    if ((pauseCtx->state == PAUSE_STATE_OPENING_0) || (pauseCtx->state == PAUSE_STATE_OWL_WARP_0)) {
        if (ShrinkWindow_Letterbox_GetSize() == 0) {
            R_PAUSE_BG_PRERENDER_STATE = PAUSE_BG_PRERENDER_SETUP;
            pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
            pauseCtx->savePromptState = PAUSE_SAVEPROMPT_STATE_APPEARING;
            pauseCtx->state = (pauseCtx->state & 0xFFFF) + 1;
        }
    } else if (pauseCtx->state == PAUSE_STATE_GAMEOVER_0) {
        R_PAUSE_BG_PRERENDER_STATE = PAUSE_BG_PRERENDER_SETUP;
        pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
        pauseCtx->savePromptState = PAUSE_SAVEPROMPT_STATE_APPEARING;
        pauseCtx->state = (pauseCtx->state & 0xFFFF) + 1;
    } else if ((pauseCtx->state == PAUSE_STATE_OPENING_1) || (pauseCtx->state == PAUSE_STATE_GAMEOVER_1) ||
               (pauseCtx->state == PAUSE_STATE_OWL_WARP_1)) {
        if (R_PAUSE_BG_PRERENDER_STATE == PAUSE_BG_PRERENDER_READY) {
            pauseCtx->state++;
        }
    } else if (pauseCtx->state != PAUSE_STATE_OFF) {
        if (MM_gKaleidoMgrCurOvl != kaleidoScopeOvl) {
            if (MM_gKaleidoMgrCurOvl != NULL) {
                MM_KaleidoManager_ClearOvl(MM_gKaleidoMgrCurOvl);
            }

            MM_KaleidoManager_LoadOvl(kaleidoScopeOvl);
        }

        if (MM_gKaleidoMgrCurOvl == kaleidoScopeOvl) {
            MM_sKaleidoScopeUpdateFunc(play);

            if (!IS_PAUSED(&play->pauseCtx)) {
                MM_KaleidoManager_ClearOvl(kaleidoScopeOvl);
                MM_KaleidoScopeCall_LoadPlayer();
            }
        }
    }
}

void MM_KaleidoScopeCall_Draw(PlayState* play) {
    KaleidoMgrOverlay* kaleidoScopeOvl = &MM_gKaleidoMgrOverlayTable[KALEIDO_OVL_KALEIDO_SCOPE];

    if (R_PAUSE_BG_PRERENDER_STATE == PAUSE_BG_PRERENDER_READY) {
        if (((play->pauseCtx.state >= PAUSE_STATE_OPENING_3) && (play->pauseCtx.state <= PAUSE_STATE_SAVEPROMPT)) ||
            ((play->pauseCtx.state >= PAUSE_STATE_GAMEOVER_3) && (play->pauseCtx.state <= PAUSE_STATE_UNPAUSE_SETUP))) {
            if (MM_gKaleidoMgrCurOvl == kaleidoScopeOvl) {
                MM_sKaleidoScopeDrawFunc(play);
            }
        }
    }
}
