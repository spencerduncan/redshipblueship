#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#define CVAR_NAME "gEnhancements.DifficultyOptions.DamageMultiplier"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

// #1037: this TU is in the single-exe link now (2ship_enh_registrars). The key is MM's own, held apart from SoH's
// gEnhancements.DamageMult because the value lists differ (OoT 0-8, here 0-4 plus 10 = 1 Hit KO); the row that writes
// it is hosted beside SoH's on Enhancements > Difficulty. The body only shifts the damage the hook is handed, so it
// reads no play state.
void RegisterDamageMultiplier() {
    COND_VB_SHOULD(VB_MULTIPLY_INFLICTED_DMG, CVAR, {
        s32* damage = va_arg(args, s32*);
        *damage <<= CVAR;
    });
}

static RegisterShipInitFunc initFunc(RegisterDamageMultiplier, { CVAR_NAME });
