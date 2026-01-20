#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_En_Ice_Hono/z_en_ice_hono.h"
void EnIceHono_CapturableFlame(EnIceHono* thisx, PlayState* play);
u32 EnIceHono_InBottleRange(EnIceHono* thisx, PlayState* play);
}

extern PlayState* OoT_gPlayState;

void OnEnIceHonoUpdate(void* actor) {
    EnIceHono* thisx = (EnIceHono*)actor;
    if (thisx->actionFunc != EnIceHono_CapturableFlame && EnIceHono_InBottleRange(thisx, OoT_gPlayState)) {
        // GI_MAX in this case allows the player to catch the actor in a bottle
        OoT_Actor_OfferGetItem(&thisx->actor, OoT_gPlayState, GI_MAX, 60.0f, 100.0f);
    }
}

void RegisterRebottleBlueFire() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_ICE_HONO, CVarGetInteger(CVAR_ENHANCEMENT("RebottleBlueFire"), 0),
                 OnEnIceHonoUpdate);
}

static RegisterShipInitFunc initFunc(RegisterRebottleBlueFire, { CVAR_ENHANCEMENT("RebottleBlueFire") });