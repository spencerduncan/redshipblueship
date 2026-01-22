/**
 * @file combo_context.c
 * @brief Cross-game context for OoT+MM unified engine
 *
 * Manages shared state between Ocarina of Time and Majora's Mask,
 * including game switching, shared flags, and shared items.
 */

#include "rsbs/combo_context.h"
#include <string.h>

#define COMBO_CONTEXT_VERSION 1
#define COMBO_CONTEXT_MAGIC "OoT+MM<3"

ComboContext gComboCtx;

void ComboContext_Init(void) {
    memset(&gComboCtx, 0, sizeof(ComboContext));
    memcpy(gComboCtx.magic, COMBO_CONTEXT_MAGIC, 8);
    gComboCtx.version = COMBO_CONTEXT_VERSION;
    gComboCtx.switchRequested = false;
    gComboCtx.targetGame = COMBO_GAME_NONE;
    gComboCtx.targetEntrance = 0;
    gComboCtx.sourceGame = COMBO_GAME_NONE;
    gComboCtx.sourceEntrance = 0;
    gComboCtx.saveSlot = -1;
}

void ComboContext_RequestSwitch(ComboGame target, uint16_t entrance) {
    gComboCtx.switchRequested = true;
    gComboCtx.targetGame = target;
    gComboCtx.targetEntrance = entrance;
}

bool ComboContext_IsSwitchPending(void) {
    return gComboCtx.switchRequested;
}

void ComboContext_ClearSwitch(void) {
    gComboCtx.switchRequested = false;
    gComboCtx.targetGame = COMBO_GAME_NONE;
    gComboCtx.targetEntrance = 0;
}
