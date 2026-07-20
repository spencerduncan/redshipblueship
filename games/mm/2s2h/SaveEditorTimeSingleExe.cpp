/**
 * Single-exe home for MM's UpdateGameTime (Lane C0, #392).
 *
 * Upstream, UpdateGameTime lives in 2s2h/DeveloperTools/SaveEditor.cpp — the
 * dev-tools ImGui save editor — which is excluded from single-exe builds with
 * the rest of DeveloperTools (games/mm/CMakeLists.txt). Un-eliding
 * 2ship_rando made the symbol a hard link dependency: the randomizer's Time
 * Trap (Rando/MiscBehavior/Traps.cpp) advances the in-game clock through it.
 *
 * This TU carries a copy of that function (and its file-local
 * FindEnTest4Actor helper) for single-exe builds only; outside single-exe
 * this file compiles empty and SaveEditor.cpp remains the one definition, so
 * the two copies can never both link. If upstream 2S2H changes
 * UpdateGameTime, re-sync this copy (provenance: SaveEditor.cpp, upstream
 * shape as of the Lane C0 port).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/ShipUtils.h"

extern "C" {
#include <z64.h>
#include <z64save.h>
#include <macros.h>
#include <variables.h>
#include <functions.h>
#include <seqcmd.h>
#include "overlays/actors/ovl_En_Test4/z_en_test4.h"
#include "overlays/actors/ovl_Obj_Tokei_Step/z_obj_tokei_step.h"

void ObjTokeiStep_DoNothing(ObjTokeiStep* objTokeiStep, PlayState* play);
void EnTest4_GetBellTimeOnDay3(EnTest4* thisx);
void EnTest4_GetBellTimeAndShrinkScreenBeforeDay3(EnTest4* thisx, PlayState* play);
}

static EnTest4* FindEnTest4Actor() {
    if (MM_gPlayState == NULL) {
        return NULL;
    }

    Actor* enTest4Search = MM_gPlayState->actorCtx.actorLists[ACTORCAT_SWITCH].first;

    while (enTest4Search != NULL) {
        if (enTest4Search->id == ACTOR_EN_TEST4) {
            return (EnTest4*)enTest4Search;
        }
        enTest4Search = enTest4Search->next;
    }

    return NULL;
}

void UpdateGameTime(u16 gameTime) {
    bool newTimeIsNight = (gameTime > CLOCK_TIME(18, 0)) || (gameTime < CLOCK_TIME(6, 0));
    bool prevTimeIsNight = (CURRENT_TIME > CLOCK_TIME(18, 0)) || (CURRENT_TIME < CLOCK_TIME(6, 0));

    gSaveContext.save.time = gameTime;

    if (MM_gPlayState == NULL) {
        return;
    }

    // Clear weather from day 2
    MM_gWeatherMode = WEATHER_MODE_CLEAR;
    MM_gPlayState->envCtx.lightningState = LIGHTNING_OFF;

    // When transitioning over night boundaries, stop the sequences and ask to replay, then respawn actors
    if (newTimeIsNight != prevTimeIsNight) {
        // AMBIENCE_ID_13 is used to persist a scenes sequence through night, so we shouldn't
        // change anything if thats active
        if (MM_gPlayState->sceneSequences.ambienceId != AMBIENCE_ID_13) {
            SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_AMBIENCE, 0);
            SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 240);
            gSaveContext.seqId = NA_BGM_DISABLED;
            gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
            MM_Environment_PlaySceneSequence(MM_gPlayState);
        }

        // Kills/Spawns half-day actors
        MM_gPlayState->numSetupActors = -MM_gPlayState->numSetupActors;
    }

    EnTest4* enTest4 = FindEnTest4Actor();

    // Update EnTest4 actor to be in sync with the new time
    // This ensures that day transitions are not triggered with the change
    if (enTest4 != NULL) {
        enTest4->prevTime = gameTime;
        enTest4->prevBellTime = gameTime;
        enTest4->daytimeIndex = newTimeIsNight ? 0 : 1;

        // Sets the nextBellTime based on the new current time
        if (CURRENT_DAY == 3) {
            EnTest4_GetBellTimeOnDay3(enTest4);
        } else {
            EnTest4_GetBellTimeAndShrinkScreenBeforeDay3(enTest4, MM_gPlayState);
        }

        // Unset any screen scaling from the above funcs
        gSaveContext.screenScale = 1000.0f;
        gSaveContext.screenScaleFlag = false;
    }

    // Open the Clock Tower rooftop
    if (((CURRENT_DAY == 3) && (CURRENT_TIME < CLOCK_TIME(6, 0)))) {
        ObjTokeiStep* objTokeiStep = (ObjTokeiStep*)MM_Actor_FindNearby(
            MM_gPlayState, &GET_PLAYER(MM_gPlayState)->actor, ACTOR_OBJ_TOKEI_STEP, ACTORCAT_BG, 99999.9f);
        if (objTokeiStep != NULL && objTokeiStep->actionFunc == ObjTokeiStep_DoNothing) {
            objTokeiStep->dyna.actor.draw = ObjTokeiStep_DrawOpen;
            ObjTokeiStep_SetupOpen(objTokeiStep);
        }
    }
}

#endif // RSBS_SINGLE_EXECUTABLE
