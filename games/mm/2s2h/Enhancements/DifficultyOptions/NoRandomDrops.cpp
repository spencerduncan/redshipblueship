#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

#define CVAR_NAME "gEnhancements.NoRandomDrops"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

#define DISABLE_VB_DROP(vb) COND_VB_SHOULD(vb, CVAR, { *should = false; })

void RegisterNoRandomDrops() {
    COND_VB_SHOULD(VB_DROP_COLLECTIBLE, CVAR, {
        if (!MM_gPlayState) {
            return;
        }

        Vec3f unused = va_arg(args, Vec3f);
        u32 item = va_arg(args, u32) & 0xFF;

        s16 scene = MM_gPlayState->sceneId;
        s8 room = MM_gPlayState->roomCtx.curRoom.num;

        // Gekko mini-boss in Woodfall Temple and Poe Sisters fight should allow arrow drops
        if (((scene == SCENE_MITURIN && room == 8) || scene == SCENE_TOUGITES) && item == ITEM00_ARROWS_10) {
            return;
        }

        // Gekko mini-boss in Great Bay Temple as well as Gomess and the Flippy room in Inverted Stone Tower Temple
        // should allow both arrows and magic
        if (((scene == SCENE_SEA && room == 5) || (scene == SCENE_INISIE_R && (room == 5 || room == 11))) &&
            (item == ITEM00_ARROWS_10 || item == ITEM00_MAGIC_JAR_SMALL || item == ITEM00_MAGIC_JAR_BIG)) {
            return;
        }

        // Everything else blocked
        *should = false;
    });
}

static RegisterShipInitFunc initFunc(RegisterNoRandomDrops, { CVAR_NAME });
