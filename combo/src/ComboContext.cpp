/**
 * ComboContext.cpp - Cross-game context implementation
 *
 * Implements the C API for context manipulation and provides
 * the global context instance for unified builds.
 */

#include "combo/ComboContext.h"

// For non-unified builds, gComboCtx is defined in unified_main.cpp
// For shared library builds, these are defined elsewhere
#ifndef COMBO_UNIFIED_BUILD
namespace Combo {
    ComboContext gComboCtx;
    Game gCurrentGame = Game::None;
}
#endif

extern "C" {

void Combo_RequestSwitch(uint16_t targetEntrance) {
    using namespace Combo;

    // Toggle to the other game
    Game target = (gCurrentGame == Game::OoT) ? Game::MM : Game::OoT;
    Combo_SwitchToGame(target, targetEntrance);
}

void Combo_SwitchToGame(Combo::Game target, uint16_t entrance) {
    using namespace Combo;

    gComboCtx.sourceGame = gCurrentGame;
    gComboCtx.targetGame = target;
    gComboCtx.targetEntrance = entrance;
    gComboCtx.switchRequested = true;
}

bool Combo_IsSwitchRequested(void) {
    return Combo::gComboCtx.switchRequested;
}

Combo::Game Combo_GetCurrentGame(void) {
    return Combo::gCurrentGame;
}

void Combo_SetSharedFlag(uint8_t flagId, uint8_t bit) {
    if (flagId < 64 && bit < 32) {
        Combo::gComboCtx.sharedFlags[flagId] |= (1u << bit);
    }
}

bool Combo_GetSharedFlag(uint8_t flagId, uint8_t bit) {
    if (flagId < 64 && bit < 32) {
        return (Combo::gComboCtx.sharedFlags[flagId] & (1u << bit)) != 0;
    }
    return false;
}

void Combo_ClearSharedFlag(uint8_t flagId, uint8_t bit) {
    if (flagId < 64 && bit < 32) {
        Combo::gComboCtx.sharedFlags[flagId] &= ~(1u << bit);
    }
}

} // extern "C"
