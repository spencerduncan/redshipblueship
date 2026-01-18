#include "global.h"
#include "vt.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

void (*sKaleidoScopeUpdateFunc)(PlayState* play);
void (*sKaleidoScopeDrawFunc)(PlayState* play);
f32 gBossMarkScale = 1.0f;
u32 D_8016139C;
PauseMapMarksData* gLoadedPauseMarkDataTable;

extern void OoT_KaleidoScope_Update(PlayState* play);
extern void OoT_KaleidoScope_Draw(PlayState* play);

void OoT_KaleidoScopeCall_LoadPlayer() {
    KaleidoMgrOverlay* playerActorOvl = &OoT_gKaleidoMgrOverlayTable[KALEIDO_OVL_PLAYER_ACTOR];

    if (OoT_gKaleidoMgrCurOvl != playerActorOvl) {
        if (OoT_gKaleidoMgrCurOvl != NULL) {
            osSyncPrintf(VT_FGCOL(GREEN));
            osSyncPrintf("カレイド領域 強制排除\n"); // "Kaleido area forced exclusion"
            osSyncPrintf(VT_RST);

            OoT_KaleidoManager_ClearOvl(OoT_gKaleidoMgrCurOvl);
        }

        osSyncPrintf(VT_FGCOL(GREEN));
        osSyncPrintf("プレイヤーアクター搬入\n"); // "Player actor import"
        osSyncPrintf(VT_RST);

        OoT_KaleidoManager_LoadOvl(playerActorOvl);
    }
}

void OoT_KaleidoScopeCall_Init(PlayState* play) {
    // "Kaleidoscope replacement construction"
    osSyncPrintf("カレイド・スコープ入れ替え コンストラクト \n");

    sKaleidoScopeUpdateFunc = OoT_KaleidoManager_GetRamAddr(OoT_KaleidoScope_Update);
    sKaleidoScopeDrawFunc = OoT_KaleidoManager_GetRamAddr(OoT_KaleidoScope_Draw);

    LOG_ADDRESS("kaleido_scope_move", OoT_KaleidoScope_Update);
    LOG_ADDRESS("kaleido_scope_move_func", sKaleidoScopeUpdateFunc);
    LOG_ADDRESS("kaleido_scope_draw", OoT_KaleidoScope_Draw);
    LOG_ADDRESS("kaleido_scope_draw_func", sKaleidoScopeDrawFunc);

    OoT_KaleidoSetup_Init(play);
}

void OoT_KaleidoScopeCall_Destroy(PlayState* play) {
    // "Kaleidoscope replacement destruction"
    osSyncPrintf("カレイド・スコープ入れ替え デストラクト \n");

    OoT_KaleidoSetup_Destroy(play);
}

void OoT_KaleidoScopeCall_Update(PlayState* play) {
    KaleidoMgrOverlay* kaleidoScopeOvl = &OoT_gKaleidoMgrOverlayTable[KALEIDO_OVL_KALEIDO_SCOPE];
    PauseContext* pauseCtx = &play->pauseCtx;

    GameInteractor_ExecuteOnKaleidoUpdate();

    if (!gSaveContext.ship.stats.gameComplete && (!IS_BOSS_RUSH || !gSaveContext.ship.quest.data.bossRush.isPaused)) {
        gSaveContext.ship.stats.pauseTimer++;
    }

    if ((pauseCtx->state != 0) || (pauseCtx->debugState != 0)) {
        if (pauseCtx->state == 1) {
            if (ShrinkWindow_GetCurrentVal() == 0) {
                HREG(80) = 7;
                HREG(82) = 3;
                R_PAUSE_MENU_MODE = 1;
                pauseCtx->unk_1E4 = 0;
                pauseCtx->unk_1EC = 0;
                pauseCtx->state = (pauseCtx->state & 0xFFFF) + 1;
                gSaveContext.ship.stats.count[COUNT_PAUSES]++;
            }
        } else if (pauseCtx->state == 8) {
            HREG(80) = 7;
            HREG(82) = 3;
            R_PAUSE_MENU_MODE = 1;
            pauseCtx->unk_1E4 = 0;
            pauseCtx->unk_1EC = 0;
            pauseCtx->state = (pauseCtx->state & 0xFFFF) + 1;
        } else if ((pauseCtx->state == 2) || (pauseCtx->state == 9)) {
            osSyncPrintf("PR_KAREIDOSCOPE_MODE=%d\n", R_PAUSE_MENU_MODE);

            if (R_PAUSE_MENU_MODE >= 3) {
                pauseCtx->state++;
            }
        } else if (pauseCtx->state != 0) {
            if (OoT_gKaleidoMgrCurOvl != kaleidoScopeOvl) {
                if (OoT_gKaleidoMgrCurOvl != NULL) {
                    osSyncPrintf(VT_FGCOL(GREEN));
                    // "Kaleido area Player Forced Elimination"
                    osSyncPrintf("カレイド領域 プレイヤー 強制排除\n");
                    osSyncPrintf(VT_RST);

                    OoT_KaleidoManager_ClearOvl(OoT_gKaleidoMgrCurOvl);
                }

                osSyncPrintf(VT_FGCOL(GREEN));
                // "Kaleido area Kaleidoscope loading"
                osSyncPrintf("カレイド領域 カレイドスコープ搬入\n");
                osSyncPrintf(VT_RST);

                OoT_KaleidoManager_LoadOvl(kaleidoScopeOvl);
            }

            if (OoT_gKaleidoMgrCurOvl == kaleidoScopeOvl) {
                sKaleidoScopeUpdateFunc(play);

                if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0)) {
                    osSyncPrintf(VT_FGCOL(GREEN));
                    // "Kaleido area Kaleidoscope Emission"
                    osSyncPrintf("カレイド領域 カレイドスコープ排出\n");
                    osSyncPrintf(VT_RST);

                    OoT_KaleidoManager_ClearOvl(kaleidoScopeOvl);
                    OoT_KaleidoScopeCall_LoadPlayer();
                }
            }
        }
    }
}

void OoT_KaleidoScopeCall_Draw(PlayState* play) {
    KaleidoMgrOverlay* kaleidoScopeOvl = &OoT_gKaleidoMgrOverlayTable[KALEIDO_OVL_KALEIDO_SCOPE];

    if (R_PAUSE_MENU_MODE >= 3) {
        if (((play->pauseCtx.state >= 4) && (play->pauseCtx.state <= 7)) ||
            ((play->pauseCtx.state >= 11) && (play->pauseCtx.state <= 18))) {
            if (OoT_gKaleidoMgrCurOvl == kaleidoScopeOvl) {
                sKaleidoScopeDrawFunc(play);
            }
        }
    }
}
