#include "global.h"

// Cross-game session invalidation (#440). Declared locally rather than by
// including src/common/context.h: games/oot/src/**.c does not have src/common
// on its include path, and this is the same convention z_play.c uses for the
// Combo_* entry points. Self-suppresses on a cross-game arrival — see
// context.h.
extern int Context_InvalidateSessionOnReturnToTitle(void);

void TitleSetup_InitImpl(GameState* gameState) {
    osSyncPrintf("ゼルダ共通データ初期化\n"); // "Zelda common data initalization"
    OoT_SaveContext_Init();
    // Retire the cross-game session as well as OoT's own SaveContext (#440).
    // OoT_SaveContext_Init above wipes gSaveContext, which is the game's OWN
    // reset inverse; the cross-game frozen blobs, both shadows, and gComboCtx
    // are process-global and had NO inverse at all, so a soft reset left them
    // fully intact for the next session to inherit. This is the single call
    // site of OoT_SaveContext_Init, and every soft-reset route reaches it:
    // the debug-console "reset" command that the Ctrl+R menu item dispatches
    // (-> OoT_TitleSetup_Init) and the N64 reset-button emulation (-> PreNMI,
    // which restarts the boot chain). A cold boot reaches it too, where the
    // clear is a harmless no-op on already-zero state.
    Context_InvalidateSessionOnReturnToTitle();
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, Title_Init, TitleContext);
}

void OoT_TitleSetup_Destroy(GameState* gameState) {
}

void OoT_TitleSetup_Init(GameState* gameState) {
    gameState->destroy = OoT_TitleSetup_Destroy;
    TitleSetup_InitImpl(gameState);
}
