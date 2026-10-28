#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#ifdef RSBS_SINGLE_EXECUTABLE
u16 MM_Play_MoonCrashArrivalEntrance(u16 entrance); // games/mm/2s2h/GameExports_SingleExe.cpp
#endif
}

// RSBS (#1278): a paired file lands where the Song of Time does, not in the
// salesman's cycle-reset cutscene (OoTMM's rewrite of 0xC030). A macro because
// the landing is written inside the COND_VB_SHOULD body, where #ifdef cannot go.
#ifdef RSBS_SINGLE_EXECUTABLE
#define RSBS_MOON_CRASH_LANDING(entrance) MM_Play_MoonCrashArrivalEntrance(entrance)
#else
#define RSBS_MOON_CRASH_LANDING(entrance) (entrance)
#endif

#define CVAR_NAME "gEnhancements.TimeSavers.SkipCutscene.Story"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

/**
 *  Skips only the visible cutscenes (Clock Tower destruction and Link's terrible fate with the Fire Wall).
 *  Note: There is a cutscene command SPECIFICALLY for resetting game state related to the moon crash,
 *        CS_MISC_RESET_SAVE_FROM_MOON_CRASH, that MUST NOT be skipped, or else
 *        it would break the vanilla functionality of a Moon Crash reset.
 */
void RegisterSkipMoonCrash() {
    // Skips initial cutscene in Termina Field where the moon falls, goes straight to clock tower cutscene
    COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, CVAR, {
        // #1018-#1022, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla verdict untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        if (gSaveContext.save.cutsceneIndex == 0x0 && gSaveContext.save.entrance == ENTRANCE(TERMINA_FIELD, 12)) {
            // This cutscene command would otherwise be run as part of the Fire Wall cutscene that gets skipped.
            // IT MUST BE CALLED HERE IF THAT CUTSCENE IS SKIPPED OR ELSE THE GAME STATE WILL NOT RESET CORRECTLY!
            Sram_ResetSaveFromMoonCrash(&MM_gPlayState->sramCtx);

            // RSBS (#1278): through RSBS_MOON_CRASH_LANDING, above.
            gSaveContext.save.entrance = RSBS_MOON_CRASH_LANDING(ENTRANCE(CLOCK_TOWER_INTERIOR, 3));
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipMoonCrash, { CVAR_NAME });
