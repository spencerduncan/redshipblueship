#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
extern s16 MM_sEquipState;
extern s16 MM_sEquipAnimTimer;
}

#define CVAR_NAME "gEnhancements.SkipArrowAnimation"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterSkipMagicArrowEquip() {
    COND_HOOK(OnKaleidoUpdate, CVAR, [](PauseContext* pauseCtx) {
        // #1035, per PR #673's criterion: this TU is in the single-exe link now and the body writes through the
        // pauseCtx the dispatcher hands over; a missing one leaves the vanilla equip animation alone.
        RSBS_SINGLE_EXE_REQUIRE(pauseCtx != NULL);
        if (MM_sEquipState == EQUIP_STATE_MAGIC_ARROW_GROW_ORB) {
            MM_sEquipState = EQUIP_STATE_MAGIC_ARROW_MOVE_TO_BOW_SLOT;
            MM_sEquipAnimTimer = 2;
            pauseCtx->equipAnimAlpha = 255;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipMagicArrowEquip, { CVAR_NAME });
