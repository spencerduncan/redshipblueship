#include "global.h"

void TitleSetup_InitImpl(GameState* gameState) {
    osSyncPrintf("ゼルダ共通データ初期化\n"); // "Zelda common data initalization"
    OoT_SaveContext_Init();
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, Title_Init, TitleContext);
}

void OoT_TitleSetup_Destroy(GameState* gameState) {
}

void OoT_TitleSetup_Init(GameState* gameState) {
    gameState->destroy = OoT_TitleSetup_Destroy;
    TitleSetup_InitImpl(gameState);
}
