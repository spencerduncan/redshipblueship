#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "overlays/actors/ovl_Obj_Sound/z_obj_sound.h"
}

#define CVAR_NAME "gAudioEditor.MuteCarpenterSfx"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterMuteCarpenterSfx() {
    COND_ID_HOOK(ShouldActorInit, ACTOR_OBJ_SOUND, CVAR, [](Actor* actor, bool* should) {
        ObjSound* objSound = (ObjSound*)actor;

        // #1239: linked into the single exe; never read a play state that is not there.
        if (MM_gPlayState == nullptr) {
            return;
        }
        // Returning if not in South or East Clocktown just as a safety precaution, incase these actor params are used
        // elsewhere
        if (MM_gPlayState->sceneId != SCENE_CLOCKTOWER && MM_gPlayState->sceneId != SCENE_TOWN) {
            return;
        }

        if ((objSound->actor.params & OBJ_SOUND_ID_MASK) == 16) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterMuteCarpenterSfx, { CVAR_NAME });
