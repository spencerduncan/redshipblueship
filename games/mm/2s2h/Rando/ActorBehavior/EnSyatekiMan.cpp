#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
void MM_Player_StartTalking(PlayState* play, Actor* actor);
}

// This is the same block found for non-scripted actors in OfferGetItem.cpp
void talkToShootingGalleryNpc(Player* player, Actor* actor) {
    player->talkActor = actor;
    player->talkActorDistance = actor->xzDistToPlayer;
    player->exchangeItemAction = PLAYER_IA_MINUS1;
    MM_Player_StartTalking(MM_gPlayState, actor);
}

/*
 * #1216: a gallery's quiver flag is its high-score check's own flag (StaticData/Checks.cpp). The vanilla
 * *_SetupGiveReward sets it in the HasParent branch after whatever reward was taken while it was clear, which arms the
 * check through OnFlagSet, and with the quiver maxed the selector falls through to a rupee. While the flag is clear
 * that rupee therefore stands in for the check, and giving it as well would be a second item for one check.
 * - Town: GI_RUPEE_PURPLE comes only from the new-record (0x407) branch, the one that sets the Town quiver flag.
 * - Swamp: the HasParent branch sets the quiver flag first whenever it is clear, whatever the reward.
 * OnFlagSet arms only a shuffled check, so an unshuffled one keeps its rupee.
 */
static bool GalleryRupeeStandsInForHighScore(GetItemId item) {
    if (MM_gPlayState->sceneId == SCENE_SYATEKI_MIZU) {
        return item == GI_RUPEE_PURPLE &&
               !CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_TOWN_SHOOTING_GALLERY_QUIVER_UPGRADE) &&
               RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE].shuffled;
    }
    if (MM_gPlayState->sceneId == SCENE_SYATEKI_MORI) {
        return (item == GI_RUPEE_RED || item == GI_RUPEE_PURPLE) &&
               !CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_SWAMP_SHOOTING_GALLERY_QUIVER_UPGRADE) &&
               RANDO_SAVE_CHECKS[RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE].shuffled;
    }
    return false;
}

void Rando::ActorBehavior::InitEnSyatekiManBehavior() {
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* actor = va_arg(args, Actor*);
        // Hijack the quivers and Heart Pieces, and a rupee that stands in for the open high-score check; let the
        // repeat rupee rewards remain as normal
        if (actor->id == ACTOR_EN_SYATEKI_MAN && (*item > GI_RUPEE_HUGE || GalleryRupeeStandsInForHighScore(*item))) {
            Player* player = GET_PLAYER(MM_gPlayState);
            if (MM_gPlayState->sceneId == SCENE_SYATEKI_MIZU) { // Town Shooting Gallery
                if (CURRENT_DAY == 3) {
                    // On the third day, there is additional dialog after the reward
                    talkToShootingGalleryNpc(player, actor);
                    actor->flags |= ACTOR_FLAG_TALK;
                } else {
                    /*
                     * On the other two days, there is no dialog after the reward. The simplest thing to make the NPC
                     * behave is to add an extra textbox. 0x3F8 is "Usually this place is packed..."
                     */
                    MM_Message_StartTextbox(MM_gPlayState, 0x3F8, actor);
                }
            } else if (MM_gPlayState->sceneId == SCENE_SYATEKI_MORI) { // Swamp Shooting Gallery
                talkToShootingGalleryNpc(player, actor);
            }
            actor->parent = &player->actor;
            *should = false;
        }
    });
}