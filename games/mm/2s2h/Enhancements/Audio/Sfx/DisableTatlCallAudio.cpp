#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

// #1239: SoH's "Disable Navi Call Audio" mutes the companion fairy's call in both games (kConvergedKeys); MM's own
// Tatl spelling is retired onto it (ConfigVersion9Updater).
#define CVAR_NAME "gAudioEditor.DisableNaviCallAudio"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterDisableTatlCallAudio() {
    COND_VB_SHOULD(VB_PLAY_TATL_CALL_AUDIO, CVAR, { *should = false; });
}

static RegisterShipInitFunc initFunc(RegisterDisableTatlCallAudio, { CVAR_NAME });
