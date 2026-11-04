/**
 * @file GoronRollingLenientSpikes.cpp
 * @brief Lenient Goron Spikes (#1426): OoTMM's lenientSpikes, as a Majora's Mask enhancement.
 *
 * OoTMM ships it on by default ("Goron Spikes can charge midair and keep their charge. Minimum speed for Goron Spikes
 * is removed.", packages/core/src/settings/data.ts) as three byte patches in ovl_player_actor (PG_MM_LENIENT_SPIKES,
 * packages/generator/src/mm/actors/Player.S). Read against the USA ROM's player overlay (VRAM 0x8082DA90), they are:
 *  1. +0x2a95c `slti at, t7, 0x36b0` becomes 0x1000: the roll speed (actionVar2) each step of the spike charge needs,
 *     the third term of VB_GORON_ROLL_INCREASE_SPIKE_LEVEL's vanilla condition.
 *  2. +0x2a41c `lui at, 0x4140` (12.0f) becomes 0.0f: the speed (unk_B08) under which spikes already out drop, the
 *     last term of VB_GORON_ROLL_DISABLE_SPIKE_MODE's vanilla condition.
 *  3. +0x2a138 in func_80857AEC `sb t2, 0xae7(s0)` (actionVar1 = 4) stores t9, the value it just loaded, instead: a
 *     landing keeps the charge built before the jump (VB_GORON_ROLL_RESET_SPIKE_LEVEL_ON_LANDING answers false).
 *
 * The key defaults to IS_RANDO, so a rando file has it on unless the player turns it off, as OoTMM's default has it on
 * for every seed. Runtime only: no logic reads it, so it changes no placement.
 *
 * ORDER-INDEPENDENT. Goron Rolling Ignores Magic answers the same two VBs. Both of these hooks only RELAX their
 * verdict (the charge may step, the spikes may stay), and they honour Ignores Magic's key, so either registration
 * order gives the same answer; Ignores Magic's charge hook relaxes too since this change (it assigned before).
 */

#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"

#define CVAR_NAME "gEnhancements.Masks.GoronRollingLenientSpikes"
#define CVAR CVarGetInteger(CVAR_NAME, IS_RANDO)

extern "C" {
#include "macros.h"
#include "variables.h"
extern PlayState* MM_gPlayState;
extern Input* sPlayerControlInput;
}

namespace {

// OoTMM's two thresholds (patches 1 and 2 above); vanilla's are 0x36B0 and 12.0f.
constexpr s16 kLenientChargeMinRollSpeed = 0x1000;
constexpr f32 kLenientSpikesKeepSpeed = 0.0f;

// Goron Rolling Ignores Magic's key: with it on, magic neither gates the charge nor drops the spikes.
constexpr const char* kIgnoresMagicCVar = "gEnhancements.Masks.GoronRollingIgnoresMagic";

bool GoronRollIgnoresMagic() {
    return CVarGetInteger(kIgnoresMagicCVar, 0) != 0;
}

} // namespace

void RegisterGoronRollingLenientSpikes() {
    // Patch 1: each charge step needs a roll speed of 0x1000, not 0x36B0. The magic terms stay vanilla's.
    COND_VB_SHOULD(VB_GORON_ROLL_INCREASE_SPIKE_LEVEL, CVAR, {
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        Player* player = GET_PLAYER(MM_gPlayState);
        const bool magicOk = GoronRollIgnoresMagic() || ((gSaveContext.magicState == MAGIC_STATE_IDLE) &&
                                                         (gSaveContext.save.saveInfo.playerData.magic >= 2));
        if (magicOk && (player->av2.actionVar2 >= kLenientChargeMinRollSpeed)) {
            *should = true;
        }
    });

    // Patch 2: spikes already out stay out at any speed. Releasing A or running out of magic still drops them.
    COND_VB_SHOULD(VB_GORON_ROLL_DISABLE_SPIKE_MODE, CVAR, {
        RSBS_SINGLE_EXE_REQUIRE((MM_gPlayState != NULL) && (sPlayerControlInput != NULL));
        Player* player = GET_PLAYER(MM_gPlayState);
        const bool outOfMagic = !GoronRollIgnoresMagic() && (gSaveContext.save.saveInfo.playerData.magic == 0);
        const bool disableSpikes = (player->stateFlags3 & PLAYER_STATE3_80000) &&
                                   (!CHECK_BTN_ALL(sPlayerControlInput->cur.button, BTN_A) || outOfMagic ||
                                    ((player->av1.actionVar1 == 4) && (player->unk_B08 < kLenientSpikesKeepSpeed)));
        if (!disableSpikes) {
            *should = false;
        }
    });

    // Patch 3: a landing keeps the charge.
    COND_VB_SHOULD(VB_GORON_ROLL_RESET_SPIKE_LEVEL_ON_LANDING, CVAR, { *should = false; });
}

static RegisterShipInitFunc initFunc(RegisterGoronRollingLenientSpikes, { CVAR_NAME, "IS_RANDO" });
