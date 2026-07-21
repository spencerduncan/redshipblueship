/**
 * GameInteractorEventsSingleExe.cpp — the MM GIEvent PUMP for single-exe
 * builds (Phase 3.0 Lane C1, #392).
 *
 * Upstream 2S2H processed the GIEvent queue in ProcessEvents
 * (2s2h/GameInteractor/GameInteractor.cpp:433), registered on the player's
 * OnActorUpdate from GameInteractor::RegisterOwnHooks. That TU is excluded
 * from the single-exe link (#395), so C0 gave MM-owned storage for the queue
 * (MM_GameEvents_Queue / MM_GameEvents_Current, GameExports_SingleExe.cpp)
 * but deliberately left the pump unwired — without it, everything the rando
 * give path enqueues (CheckQueue's GIEventGiveItem, trap events, transition
 * events) would sit in the queue forever: the historical "compiles fine,
 * does nothing" failure.
 *
 * This is a line-faithful port of upstream ProcessEvents with exactly one
 * substitution: every GameInteractor::Instance->events / ->currentEvent
 * access (MM-only data members past the end of the shared 4-byte allocation
 * — the #395 OOB read/write) becomes MM_GameEvents_Queue() /
 * MM_GameEvents_Current(). Registration goes through the MM-owned
 * S2H::GameHooks registry and is dispatched by MM_GameHooks_ExecuteOnActorUpdate
 * (games/mm/src/code/z_actor.c), so it runs on MM frames only.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/GameInteractor/GameInteractor.h" // + mm_game_hooks.h C++ surface via the single-exe tail
#include "2s2h/CustomItem/CustomItem.h"

#include <cmath>
#include <cstdio>
#include <variant>

extern "C" {
#include "z64actor.h"
#include "variables.h"
#include "functions.h"
}

static void MM_ProcessGIEvents(Actor* actor) {
    Player* player = GET_PLAYER(MM_gPlayState);

    // If the player has a message active, stop
    if (MM_gPlayState->msgCtx.msgMode != 0) {
        return;
    }

    // If the player is in a blocking cutscene, stop
    if (MM_Player_InBlockingCsMode(MM_gPlayState, player)) {
        return;
    }

    // If player is dead, stop
    if (player->stateFlags1 & PLAYER_STATE1_DEAD) {
        return;
    }

    // If there is an event active, stop
    const auto& currentEvent = MM_GameEvents_Current();
    if (auto e = std::get_if<GIEventNone>(&currentEvent)) {
        // no-op
    } else {
        return;
    }

    // If there are no events that need to happen, stop
    if (MM_GameEvents_Queue().empty()) {
        return;
    }

    MM_GameEvents_Current() = MM_GameEvents_Queue().front();
    const auto& nextEvent = MM_GameEvents_Current();

    if (auto e = std::get_if<GIEventGiveItem>(&nextEvent)) {
        EnItem00* enItem00;

        s16 flags = CustomItem::HIDE_TILL_OVERHEAD | CustomItem::KEEP_ON_PLAYER;

        // If the player is climbing or in the air, deliver the item without a cutscene but freeze the player
        if (!e->showGetItemCutscene ||
            (player->stateFlags1 &
             (PLAYER_STATE1_CHARGING_SPIN_ATTACK | PLAYER_STATE1_2000 | PLAYER_STATE1_4000 | PLAYER_STATE1_40000 |
              PLAYER_STATE1_80000 | PLAYER_STATE1_100000 | PLAYER_STATE1_200000 | PLAYER_STATE1_8000000)) ||
            (MM_Player_GetExplosiveHeld(player) > PLAYER_EXPLOSIVE_NONE)) {

            flags |= CustomItem::GIVE_OVERHEAD;
        } else {
            flags |= CustomItem::GIVE_ITEM_CUTSCENE;
        }

        enItem00 = CustomItem::Spawn(
            player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z, 0, flags, e->param,
            [](Actor* actor, PlayState* play) {
                Player* player = GET_PLAYER(MM_gPlayState);
                const auto& nextEvent = MM_GameEvents_Current();
                if (auto e = std::get_if<GIEventGiveItem>(&nextEvent)) {
                    e->giveItem(actor, play);
                    if (e->showGetItemCutscene && !(CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE)) {
                        player->actor.freezeTimer = 30;
                    }
                    MM_GameEvents_Current() = GIEventNone{};
                }
            },
            e->drawItem);
        enItem00->actor.destroy = [](Actor* actor, PlayState* play) {
            if (!(CUSTOM_ITEM_FLAGS & CustomItem::CALLED_ACTION)) {
                // Event was not handled, requeue it
                auto lostEvent = MM_GameEvents_Current();
                MM_GameEvents_Current() = GIEventNone{};
                MM_GameEvents_Queue().push_back(lostEvent);
            }
        };
    } else if (auto e = std::get_if<GIEventTransition>(&nextEvent)) {
        MM_gPlayState->nextEntrance = e->entrance;
        gSaveContext.nextCutsceneIndex = e->cutsceneIndex;
        MM_gPlayState->transitionTrigger = e->transitionTrigger;
        MM_gPlayState->transitionType = e->transitionType;
        MM_GameEvents_Current() = GIEventNone{};
    } else if (auto e = std::get_if<GIEventSpawnActor>(&nextEvent)) {
        // if true, the coordinates are made relative to the player's position and rotation, 0 rotation is facing the
        // same direction as the player, x+ is to the players right, y+ is up, z+ is in front of the player
        if (e->relativeCoords) {
            f32 x = player->actor.world.pos.x;
            f32 y = player->actor.world.pos.y;
            f32 z = player->actor.world.pos.z;
            f32 s = sin(player->actor.world.rot.y);
            f32 c = cos(player->actor.world.rot.y);
            f32 x2 = e->posX * c - e->posZ * s;
            f32 z2 = e->posX * s + e->posZ * c;
            MM_Actor_Spawn(&MM_gPlayState->actorCtx, MM_gPlayState, e->actorId, x + x2, y + e->posY, z + z2, 0,
                           e->rotY + player->actor.world.rot.y, 0, e->params);
        } else {
            MM_Actor_Spawn(&MM_gPlayState->actorCtx, MM_gPlayState, e->actorId, e->posX, e->posY, e->posZ, e->rotX,
                           e->rotY, e->rotZ, e->params);
        }
        MM_GameEvents_Current() = GIEventNone{};
    } else if (auto e = std::get_if<GIEventTrap>(&nextEvent)) {
        if (e->action) {
            e->action();
        }
        MM_GameEvents_Current() = GIEventNone{};
    }

    MM_GameEvents_Queue().erase(MM_GameEvents_Queue().begin());
}

/**
 * Once-only pump registration; called from MM_Rando_Init (which carries the
 * matching once-only guard, but a second guard here keeps the contract local).
 */
extern "C" void MM_GameEvents_RegisterPump(void) {
    static bool sRegistered = false;
    if (sRegistered) {
        return;
    }
    sRegistered = true;
    S2H::GameHooks::RegisterForID<GameInteractor::OnActorUpdate>(ACTOR_PLAYER, MM_ProcessGIEvents);
    fprintf(stderr, "[MM] GIEvent pump registered on player OnActorUpdate (Lane C1)\n");
}

#endif // RSBS_SINGLE_EXECUTABLE
