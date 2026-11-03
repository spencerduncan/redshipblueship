#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/Rando/Logic/Logic.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/Foreign.h" // #1369: ShuffleSwimForWorld / ShuffleOcarinaButtonsForWorld
#endif

extern "C" {
#include "variables.h"
#include "functions.h"

#include "include/z64player.h"
extern s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 arg3);
extern void Player_Action_1(Player* player, PlayState* play);
}

static u8 lastOcarinaButton = OCARINA_BTN_INVALID;

#ifdef RSBS_SINGLE_EXECUTABLE
// RedShipBlueShip #1369: one Shuffle Swim / Shuffle Ocarina Buttons for the pair.
// Each gate below is armed by MM's own frozen option OR the pair's one shuffle of
// that item from the frozen combo record (Rando::Foreign::ShuffleSwimForWorld).
static bool RsbsShuffleSwimOn() {
    return Rando::Foreign::ShuffleSwimForWorld(RANDO_SAVE_OPTIONS);
}
static bool RsbsShuffleOcarinaButtonsOn() {
    return Rando::Foreign::ShuffleOcarinaButtonsForWorld(RANDO_SAVE_OPTIONS);
}
#else
static bool RsbsShuffleSwimOn() {
    return RANDO_SAVE_OPTIONS[RO_SHUFFLE_SWIM];
}
static bool RsbsShuffleOcarinaButtonsOn() {
    return RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA_BUTTONS];
}
#endif

void RespawnOnWaterTouch(Player* player) {
    // This is Honey & Darlings Shop, touching the water ends the minigame as its vanilla behavior.
    // No reason to handle it a second time here.
    if (MM_gPlayState->sceneId == SCENE_BOWLING) {
        return;
    }

    if (player->stateFlags1 & PLAYER_STATE1_8000000) {
        // Mimic Deku Hop failure behavior
        Player_SetAction(MM_gPlayState, player, Player_Action_1, 0);
        player->stateFlags1 |= PLAYER_STATE1_20000000;
    }
}

void Rando::ActorBehavior::InitPlayerBehavior() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, IS_RANDO && RsbsShuffleSwimOn(), [](Actor* actor) {
        Player* player = GET_PLAYER(MM_gPlayState);
        if (!Flags_GetRandoInf(RANDO_INF_OBTAINED_SWIM)) {
            RespawnOnWaterTouch(player);
        }
    });

    COND_VB_SHOULD(VB_PLAY_OCARINA_NOTE, IS_RANDO && RsbsShuffleOcarinaButtonsOn(), {
        u8* sCurOcarinaButtonIndex = va_arg(args, u8*);
        u8* sCurOcarinaPitch = va_arg(args, u8*);
        u8 currentOcarinaButton = *sCurOcarinaButtonIndex;

        if (!Flags_GetRandoInf(((*sCurOcarinaButtonIndex) - OCARINA_BTN_A) + RANDO_INF_OBTAINED_OCARINA_BUTTON_A)) {
            *sCurOcarinaButtonIndex = OCARINA_BTN_INVALID;
            *sCurOcarinaPitch = OCARINA_PITCH_NONE;
            if (lastOcarinaButton != currentOcarinaButton) {
                lastOcarinaButton = currentOcarinaButton;
                if (currentOcarinaButton != OCARINA_BTN_INVALID) {
                    Audio_PlaySfx(NA_SE_SY_OCARINA_ERROR);
                }
            }
        }
    });
}
