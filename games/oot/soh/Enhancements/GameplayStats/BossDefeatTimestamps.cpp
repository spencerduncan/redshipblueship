#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

extern "C" SaveContext gSaveContext;

#ifdef RSBS_SINGLE_EXECUTABLE
// soh/Enhancements/randomizer/ComboGoalEndingOoT.cpp
extern "C" int OoT_ComboGoal_OnGanonDefeated(void);
#endif

static void MarkGameCompleteOnGanonDefeat() {
#ifdef RSBS_SINGLE_EXECUTABLE
    // RSBS (#762): in a paired world whose frozen goal is still unmet the game
    // is not complete, so OoT's stat timers keep running. The defeat is
    // recorded (src/common/combo_goal.h).
    if (!OoT_ComboGoal_OnGanonDefeated()) {
        return;
    }
#endif
    gSaveContext.ship.stats.gameComplete = true;
}

#define BOSS_DEFEAT_TIMESTAMP(actorID, timestamp) \
    COND_ID_HOOK(OnBossDefeat, actorID, true,     \
                 [](void* refActor) { gSaveContext.ship.stats.itemTimestamp[timestamp] = GAMEPLAYSTAT_TOTAL_TIME; });

static void RegisterBossDefeatTimestamps() {
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_GOMA, TIMESTAMP_DEFEAT_GOHMA);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_DODONGO, TIMESTAMP_DEFEAT_KING_DODONGO);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_VA, TIMESTAMP_DEFEAT_BARINADE);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_GANONDROF, TIMESTAMP_DEFEAT_PHANTOM_GANON);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_FD2, TIMESTAMP_DEFEAT_VOLVAGIA);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_MO, TIMESTAMP_DEFEAT_MORPHA);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_SST, TIMESTAMP_DEFEAT_BONGO_BONGO);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_TW, TIMESTAMP_DEFEAT_TWINROVA);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_GANON, TIMESTAMP_DEFEAT_GANONDORF);
    BOSS_DEFEAT_TIMESTAMP(ACTOR_BOSS_GANON2, TIMESTAMP_DEFEAT_GANON);

    COND_ID_HOOK(OnBossDefeat, ACTOR_BOSS_GANON2, true, [](void* refActor) { MarkGameCompleteOnGanonDefeat(); });
}

static RegisterShipInitFunc initFunc(RegisterBossDefeatTimestamps);
