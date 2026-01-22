#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
}

#define CVAR_NAME "gCheats.EasyFrameAdvance"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

static int frameAdvanceTimer = 0;

void RegisterEasyFrameAdvance() {
    COND_HOOK(OnGameStateMainStart, CVAR, []() {
        if (MM_gPlayState == NULL) {
            return;
        }

        Input* input = CONTROLLER1(&MM_gPlayState->state);
        PauseContext* pauseCtx = &MM_gPlayState->pauseCtx;

        if (frameAdvanceTimer > 0 && pauseCtx->state == PAUSE_STATE_OFF) {
            frameAdvanceTimer--;
            if (frameAdvanceTimer == 0 && CHECK_BTN_ALL(input->cur.button, BTN_START)) {
                input->press.button |= BTN_START;
            }
        }

        if (pauseCtx->state == PAUSE_STATE_UNPAUSE_CLOSE) {
            frameAdvanceTimer = 2;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterEasyFrameAdvance, { CVAR_NAME });
