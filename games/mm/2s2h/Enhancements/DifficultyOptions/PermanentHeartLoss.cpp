#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

#define CVAR_NAME "gEnhancements.PermanentHeartLoss"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

// MM_-prefixed for the single exe: OoT's PermanentLosses.cpp has a static RegisterPermanentHeartLoss of its own, and
// the CI symbol probe (check-registrar-elision.sh) matches by demangled name.
void MM_RegisterPermanentHeartLoss() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, CVAR, [](Actor* actor) {
        if (gSaveContext.save.saveInfo.playerData.healthCapacity > 16 &&
            gSaveContext.save.saveInfo.playerData.healthCapacity - gSaveContext.save.saveInfo.playerData.health >= 16) {
            gSaveContext.save.saveInfo.playerData.healthCapacity -= 16;
            gSaveContext.save.saveInfo.playerData.health =
                MIN(gSaveContext.save.saveInfo.playerData.health, gSaveContext.save.saveInfo.playerData.healthCapacity);
        }
    });
}

static RegisterShipInitFunc initFunc(MM_RegisterPermanentHeartLoss, { CVAR_NAME });
