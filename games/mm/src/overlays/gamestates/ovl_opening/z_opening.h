#ifndef Z64_OPENING_H
#define Z64_OPENING_H

#include "z64game.h"
#include "z64view.h"

typedef struct {
    /* 0x000 */ GameState state;
    /* 0x0A8 */ View view;
} TitleSetupState; // size = 0x210

// Alias for gamestate table macro (which generates MM_TitleSetupState from MM_TitleSetup)
typedef TitleSetupState MM_TitleSetupState;

void MM_TitleSetup_Init(GameState* thisx);
void MM_TitleSetup_Destroy(GameState* thisx);

#endif
