#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "functions.h"
extern PlayState* OoT_gPlayState;
void func_80838280(Player* player);
}

/**
 * DAMAGE_PLAYER
 */

void Anchor::SendPacket_DamagePlayer(u32 clientId, u8 damageEffect, u8 damage) {
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = DAMAGE_PLAYER;
    payload["targetClientId"] = clientId;
    payload["damageEffect"] = damageEffect;
    payload["damage"] = damage;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_DamagePlayer(nlohmann::json payload) {
    uint32_t clientId = payload["clientId"].get<uint32_t>();
    if (!clients.contains(clientId) || clients[clientId].player == nullptr) {
        return;
    }

    AnchorClient& anchorClient = clients[clientId];
    Player* otherPlayer = anchorClient.player;
    Player* self = GET_PLAYER(OoT_gPlayState);

    // Prevent incoming damage during cutscenes or item get sequences
    if (OoT_Player_InBlockingCsMode(OoT_gPlayState, self) || self->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS ||
        self->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) {
        return;
    }

    u8 damageEffect = payload["damageEffect"].get<u8>();
    u8 damage = payload["damage"].get<u8>();

    self->actor.colChkInfo.damage = damage * 8; // Arbitrary number currently, need to fine tune

    if (damageEffect == DUMMY_PLAYER_HIT_RESPONSE_FIRE) {
        for (int i = 0; i < ARRAY_COUNT(self->bodyFlameTimers); i++) {
            self->bodyFlameTimers[i] = OoT_Rand_S16Offset(0, 200);
        }
        self->bodyIsBurning = true;
    } else if (damageEffect == DUMMY_PLAYER_HIT_RESPONSE_STUN) {
        self->actor.freezeTimer = 20;
        OoT_Actor_SetColorFilter(&self->actor, 0, 0xFF, 0, 24);
        return;
    }

    func_80837C0C(OoT_gPlayState, self, damageEffect, 4.0f, 5.0f,
                  OoT_Actor_WorldYawTowardActor(&otherPlayer->actor, &self->actor), 20);
}
