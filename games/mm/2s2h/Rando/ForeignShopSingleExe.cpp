/**
 * @file ForeignShopSingleExe.cpp
 * @brief #800 pass 1, MM side (an OoT item in an MM shop slot): the test bridges
 *        and the playtest drive. Nothing in this file runs in play.
 *
 * The shop code itself lives in the vendored ActorBehavior files (EnGirlA.cpp,
 * EnSob1.cpp, EnIn.cpp, EnTab.cpp). This file drives the REAL shelf functions
 * those files define, from outside them:
 *
 *  - MM_EnGirlA_TestForeignPurchase: the ForeignItemGiveShop row's purchase legs
 *    (src/common/tests/test_foreign_shop_mm.c);
 *  - MM_EnGirlA_TestHagsMushroomRearm: the same row's re-armed Hags leg;
 *  - MM_Shop_PlaytestWarp / MM_Shop_PlaytestShopFrame: the env-gated playtest
 *    drive (RSBS_GP_MM_SHOP=1), called from the gameplay round trip's MM frame
 *    tick in GameExports_SingleExe.cpp.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Foreign.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_GirlA/z_en_girla.h"
#include "overlays/actors/ovl_En_Ossan/z_en_ossan.h"
#include "overlays/actors/ovl_En_Sob1/z_en_sob1.h"
}

// src/common. Outside any extern "C" block: each header manages its own linkage.
#include "crossing_store.h" // the paired world's crossings (the drive's pick)
#include "foreign_model.h"  // the model answer the drive logs
#include "shared_items.h"   // Combo_CountSharedItems, the crossing record

// The shelf's own functions (ActorBehavior/EnGirlA.cpp), the ones a shopkeeper
// (EnOssan/EnTrt/EnFsn/EnSob1) calls.
s32 EnGirlA_RandoCanBuyFunc(PlayState* play, EnGirlA* enGirlA);
void EnGirlA_RandoBuyFunc(PlayState* play, EnGirlA* enGirlA);
void EnGirlA_RandoBought(PlayState* play, EnGirlA* enGirlA);
void EnGirlA_RandoRestock(PlayState* play, EnGirlA* enGirlA);
void EnGirlA_RandoDrawFunc(Actor* actor, PlayState* play);

// EnGirlA.cpp's RANDO_DESC_TEXT_ID: the shelf's description text, which its
// OnOpenText hook builds.
static constexpr u16 kShelfDescTextId = 0x083F;

// ============================================================================
// TEST BRIDGE (ForeignItemGiveShop row; #800 pass 1). Drives the REAL shelf
// functions a shopkeeper calls, in the order it calls them (canBuyFunc, then
// buyFunc on CANBUY_RESULT_SUCCESS_2, then the restock on the next visit),
// against a shelf actor standing on `randoCheckId`, which the caller has made a
// crossing host (placement present; pairing live or not, per `paired`).
//
// What it reads back: whether the purchase authored the crossing (the shared
// structure grew by one OoT-tagged CROSSING row) or, unpaired, authored nothing
// (#610); that the price was charged; that the slot stays sold for good; and that
// the counter refuses a second sale. Every piece of state it touches (the save
// check, the rupee counters, MM_gPlayState) is restored. Returns 0 on success, a
// step code otherwise, after printing the observation.
// ============================================================================
extern "C" int MM_EnGirlA_TestForeignPurchase(uint16_t randoCheckId, int paired) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX) {
        return 90;
    }
    const RandoSaveCheck priorCheck = RANDO_SAVE_CHECKS[randoCheckId];
    const s16 priorRupees = gSaveContext.save.saveInfo.playerData.rupees;
    const s16 priorAccumulator = gSaveContext.rupeeAccumulator;
    PlayState* const priorPlay = MM_gPlayState;

    PlayState* play = (PlayState*)calloc(1, sizeof(PlayState));
    EnGirlA* shelf = (EnGirlA*)calloc(1, sizeof(EnGirlA));
    const s16 kPrice = 37;
    shelf->actor.world.rot.z = (s16)randoCheckId;
    play->msgCtx.unk1206C = kPrice; // what the shopkeeper loads from the slot's price
    MM_gPlayState = play;

    // The slot as the paired fill leaves it: shuffled, holding MM's junk cover.
    RandoSaveCheck& check = RANDO_SAVE_CHECKS[randoCheckId];
    check.randoItemId = RI_JUNK;
    check.shuffled = true;
    check.obtained = false;
    check.cycleObtained = false;
    check.eligible = false;
    check.price = kPrice;
    gSaveContext.save.saveInfo.playerData.rupees = 100;
    gSaveContext.rupeeAccumulator = 0;

    int code = 0;
    const int before = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true);
    const s32 canBuy = EnGirlA_RandoCanBuyFunc(play, shelf);
    int crossed = 0;
    s32 canBuyAgain = -1;
    bool restocked = false;
    int lastOrigin = -1;
    int lastFlags = -1;
    if (canBuy != CANBUY_RESULT_SUCCESS_2) {
        code = 1;
    } else {
        EnGirlA_RandoBuyFunc(play, shelf);
        crossed = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) - before;
        for (uint32_t i = 0; crossed > 0 && i < RSBS_SHARED_ITEM_CAP; i++) {
            const SharedItem& row = gComboCtx.sharedItemsTagged[i];
            if (row.originGame != (uint8_t)GAME_NONE) {
                lastOrigin = row.originGame; // the last written row
                lastFlags = row.flags;
            }
        }
        // The shopkeeper's boughtFunc, then a later visit's restock.
        EnGirlA_RandoBought(play, shelf);
        EnGirlA_RandoRestock(play, shelf);
        restocked = !shelf->isOutOfStock || shelf->actor.draw != NULL;
        canBuyAgain = EnGirlA_RandoCanBuyFunc(play, shelf);
    }
    printf("[TEST]   S shop check %u (%s): canBuy=%d; after the buy: crossings authored=%d (origin %d, flags %d), "
           "obtained=%d, charged=%d; restock %s; canBuy again=%d\n",
           (unsigned)randoCheckId, paired ? "paired" : "UNPAIRED", (int)canBuy, crossed, lastOrigin, lastFlags,
           check.obtained ? 1 : 0, -(int)gSaveContext.rupeeAccumulator, restocked ? "RESTOCKED" : "kept it sold",
           (int)canBuyAgain);
    if (code == 0) {
        if (paired && crossed != 1) {
            code = 2; // the purchase did not hand the OoT item to the shared structure
        } else if (paired && (lastOrigin != GAME_OOT || lastFlags != RSBS_SHARED_ITEM_CROSSING)) {
            code = 3;
        } else if (!paired && crossed != 0) {
            code = 4; // #610: no live pairing, so no record may be authored
        } else if (!check.obtained || gSaveContext.rupeeAccumulator != -kPrice) {
            code = 5;
        } else if (restocked) {
            code = 6; // a foreign slot sells its item once for the whole game
        } else if (canBuyAgain != CANBUY_RESULT_CANNOT_GET_NOW) {
            code = 7;
        }
    }

    check = priorCheck;
    gSaveContext.save.saveInfo.playerData.rupees = priorRupees;
    gSaveContext.rupeeAccumulator = priorAccumulator;
    MM_gPlayState = priorPlay;
    free(shelf);
    free(play);
    return code;
}

/** The Hags' mushroom slot (EnGirlA.cpp's text 0x884 hook arms it). */
extern "C" uint16_t MM_EnGirlA_TestHagsMushroomCheck(void) {
    return (uint16_t)RC_HAGS_POTION_SHOP_ITEM_01;
}

// ============================================================================
// TEST BRIDGE (ForeignItemGiveShop row, the re-armed Hags leg; #800 pass 1).
// The Hags' mushroom line (text 0x884, EnGirlA.cpp) arms the slot's
// `.eligible` and CheckQueue gives it. Its weekly flags reset every cycle, so a
// later cycle's mushroom reaches the line again. Here, on the slot as the caller
// placed it (a foreign host, or native when nothing is placed):
//   1. the first cycle's line may arm it (HagsMushroomSlotArmable, the predicate
//      the hook asks first);
//   2. the give CheckQueue makes once it is armed: GiveForeignCheck for a foreign
//      host, the native lambda's delivered bits otherwise;
//   3. OnCycleSave's three-day reset of the slot (`cycleObtained` cleared);
//   4. the later cycle's line asks again.
// Writes 1/0 for steps 1, 2 (crossed) and 4. The save check is restored.
// ============================================================================
extern "C" void MM_EnGirlA_TestHagsMushroomRearm(int* outFirstArm, int* outCrossed, int* outLaterArm) {
    const RandoCheckId rc = RC_HAGS_POTION_SHOP_ITEM_01;
    const RandoSaveCheck priorCheck = RANDO_SAVE_CHECKS[rc];
    RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
    check.randoItemId = RI_JUNK;
    check.shuffled = true;
    check.obtained = false;
    check.cycleObtained = false;
    check.eligible = false;

    *outFirstArm = Rando::ActorBehavior::HagsMushroomSlotArmable() ? 1 : 0;
    check.eligible = true; // the hook's arm
    if (Rando::Foreign::IsForeignCheck(rc)) {
        *outCrossed = Rando::Foreign::GiveForeignCheck(rc) ? 1 : 0;
    } else {
        *outCrossed = 0;
        check.cycleObtained = true;
        check.obtained = true;
        check.eligible = false;
    }
    check.cycleObtained = false; // OnCycleSave.cpp
    *outLaterArm = Rando::ActorBehavior::HagsMushroomSlotArmable() ? 1 : 0;
    printf("[TEST]   S6 Hags' mushroom slot %u (%s): first cycle arms=%d, crossed=%d; after the three-day reset "
           "the line arms again=%d\n",
           (unsigned)rc, Rando::Foreign::IsForeignCheck(rc) ? "foreign host" : "native", *outFirstArm, *outCrossed,
           *outLaterArm);
    check = priorCheck;
}

// ============================================================================
// #800 PLAYTEST DRIVE (GameExports_SingleExe.cpp, gameplay round trip,
// RSBS_GP_MM_SHOP=1, from 100 live frames into the MM play window). Test-only:
// nothing reaches it unless the env var is set. It is not a player's visit:
//  - it picks the first OoT item the paired world's crossing store put on a
//    shelf of the Bomb Shop or the Trading Post (the two Clock Town shops whose
//    shelves stand every day), sets the clock to noon so both are open, and
//    WARPS the player in through the shop's entrance (nextEntrance + a fade);
//  - inside, it TELEPORTS the player to the counter facing the shelf;
//  - it puts the shopkeeper's cursor on the shelf and OPENS the shelf's
//    description textbox directly (MM_Message_StartTextbox with the shopkeeper
//    as talk actor), not through the shopkeeper's state machine;
//  - it SETS the rupee counter to the price when the wallet holds less, then
//    calls the shelf's own canBuy/buy/bought functions, as the shopkeeper does
//    on "I'll buy it".
// ============================================================================
static RandoCheckId sShopPlaytestHost = RC_UNKNOWN;
static SharedItem sShopPlaytestItem;
// GameExports_SingleExe.cpp: the window (game and ImGui) as a PNG, through the
// shared in-process capture (#843, src/common/frame_capture.h).
extern "C" void MM_Playtest_DumpGameFramebuffer(const char* tag);

// Returns the host check id, 0 when the world has no such crossing, or -1 when a
// textbox is up (an arrival's own get-item), so the caller retries next frame.
extern "C" int MM_Shop_PlaytestWarp(void) {
    struct ShopDoor {
        RandoCheckId first;
        RandoCheckId last;
        u16 entrance;
        const char* name;
    };
    static const ShopDoor kDoors[] = {
        { RC_BOMB_SHOP_ITEM_01, RC_BOMB_SHOP_ITEM_03, ENTRANCE(BOMB_SHOP, 0), "the Bomb Shop" },
        { RC_TRADING_POST_SHOP_ITEM_01, RC_TRADING_POST_SHOP_ITEM_08, ENTRANCE(TRADING_POST, 0), "the Trading Post" },
    };
    static int sWaited = 0;
    if (MM_gPlayState == nullptr) {
        return -1;
    }
    if (MM_gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
        // Nobody is at the keyboard: a textbox still up after 300 frames is
        // waiting for a press that will not come, so the drive closes it.
        if (++sWaited % 300 == 0) {
            fprintf(stderr, "[S2-PLAYTEST] closing textbox 0x%04X (msgMode %d), up for %d frames\n",
                    (unsigned)MM_gPlayState->msgCtx.currentTextId, (int)MM_gPlayState->msgCtx.msgMode, sWaited);
            fflush(stderr);
            MM_Message_CloseTextbox(MM_gPlayState);
        }
        return -1;
    }
    const int count = Combo_Crossings_Count(GAME_MM);
    for (const ShopDoor& door : kDoors) {
        for (int i = 0; i < count; i++) {
            ComboCrossing crossing;
            if (!Combo_Crossings_At(GAME_MM, i, &crossing)) {
                continue;
            }
            const RandoCheckId host = (RandoCheckId)crossing.hostCheck;
            if (host < door.first || host > door.last) {
                continue;
            }
            const char* foreign = Rando::Foreign::ForeignNameForCheck(host);
            fprintf(stderr,
                    "[S2-PLAYTEST] MM shop slot %u (%s) hosts OoT item %u (%s), price %u; warping into %s at noon "
                    "(crossing %d of %d)\n",
                    (unsigned)host, Rando::StaticData::Checks[host].name, (unsigned)crossing.item.id,
                    foreign != nullptr ? foreign : "?", (unsigned)RANDO_SAVE_CHECKS[host].price, door.name, i + 1,
                    count);
            fflush(stderr);
            gSaveContext.save.time = CLOCK_TIME(12, 0);
            MM_gPlayState->nextEntrance = door.entrance;
            MM_gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            MM_gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
            gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
            sShopPlaytestHost = host;
            sShopPlaytestItem = crossing.item;
            return host;
        }
    }
    fprintf(stderr, "[S2-PLAYTEST] no MM-hosted crossing of %d is on a Bomb Shop or Trading Post shelf\n", count);
    fflush(stderr);
    return 0;
}

// Inside the shop (called every live MM frame after the warp, same opt-in): at
// shop frame 20 teleport the player to the counter; at 40 log the shelf standing
// on the host (stock, draw, model answer); at 60 the first screenshot; at 100
// open the shelf's description textbox with the cursor on it (the
// RANDO_DESC_TEXT_ID hook in EnGirlA.cpp builds the text); at 400 the second
// screenshot; close it at 700; at 760 set the wallet to the price if short and
// buy through the shelf's real canBuy/buy/bought functions; at 900 the third
// screenshot and the OoT-bound record and the sold-out state.
extern "C" void MM_Shop_PlaytestShopFrame(void) {
    static int sFrame = 0;
    static int sSharedBefore = -1;
    PlayState* play = MM_gPlayState;
    if (sShopPlaytestHost == RC_UNKNOWN || play == nullptr ||
        (play->sceneId != SCENE_BOMYA && play->sceneId != SCENE_8ITEMSHOP)) {
        return;
    }
    EnGirlA* shelf = nullptr;
    Actor* keeper = nullptr;
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = play->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->id == ACTOR_EN_GIRLA && a->world.rot.z == (s16)sShopPlaytestHost) {
                shelf = (EnGirlA*)a;
            } else if (a->id == ACTOR_EN_OSSAN) {
                keeper = a;
            }
        }
    }
    if (shelf == nullptr || keeper == nullptr) {
        return;
    }
    sFrame++;
    if (sFrame == 20) {
        // Teleport the player to the counter, facing the shelf, so the screenshots
        // show it close up (the shopkeeper is behind the counter, the shelf on it).
        Player* player = GET_PLAYER(play);
        f32 dx = shelf->actor.world.pos.x - keeper->world.pos.x;
        f32 dz = shelf->actor.world.pos.z - keeper->world.pos.z;
        const f32 len = sqrtf(dx * dx + dz * dz);
        if (player != nullptr && len > 1.0f) {
            player->actor.world.pos.x = shelf->actor.world.pos.x + dx / len * 90.0f;
            player->actor.world.pos.z = shelf->actor.world.pos.z + dz / len * 90.0f;
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw =
                MM_Math_Vec3f_Yaw(&player->actor.world.pos, &shelf->actor.world.pos);
            fprintf(stderr, "[S2-PLAYTEST] teleported the player to the counter in front of check %u\n",
                    (unsigned)sShopPlaytestHost);
            fflush(stderr);
        }
    }
    if (sFrame == 40) {
        ComboModelAnswer answer;
        const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_MM, sShopPlaytestItem, &answer);
        fprintf(stderr, "[S2-PLAYTEST] shelf on check %u: foreign=%d outOfStock=%d drawFunc=%s; model answer %s%s\n",
                (unsigned)sShopPlaytestHost, Rando::Foreign::IsForeignCheck(sShopPlaytestHost) ? 1 : 0,
                shelf->isOutOfStock ? 1 : 0,
                shelf->actor.draw == EnGirlA_RandoDrawFunc ? "EnGirlA_RandoDrawFunc" : "other",
                kind == COMBO_MODEL_ANSWER_DESCRIPTOR    ? "DESCRIPTOR, first list "
                : kind == COMBO_MODEL_ANSWER_HOST_NATIVE ? "HOST_NATIVE"
                                                         : "NONE (the model-less stand-in)",
                kind == COMBO_MODEL_ANSWER_DESCRIPTOR ? answer.model.parts[0].dl : "");
        fflush(stderr);
    }
    // The cursor lives in the shopkeeper's own struct: EnOssan (the Trading
    // Post, params 0/1) or EnSob1 (the Bomb Shop), as EnGirlA.cpp's
    // IdentifyActiveShopItem reads it.
    const bool isOssan = keeper->params <= 1;
    EnGirlA** items = isOssan ? ((EnOssan*)keeper)->items : ((EnSob1*)keeper)->items;
    u8* cursor = isOssan ? &((EnOssan*)keeper)->cursorIndex : &((EnSob1*)keeper)->cursorIndex;
    if (sFrame == 60) {
        MM_Playtest_DumpGameFramebuffer("w8-S2-play-shelf");
    }
    if (sFrame == 100) {
        for (u8 i = 0; i < (isOssan ? 8 : 3); i++) {
            if (items[i] == shelf) {
                *cursor = i;
            }
        }
        fprintf(stderr,
                "[S2-PLAYTEST] opening the shelf's description textbox 0x%04X directly (cursor %u, talk actor set "
                "to the shopkeeper)\n",
                (unsigned)kShelfDescTextId, (unsigned)*cursor);
        fflush(stderr);
        // In play the player is already talking to the shopkeeper when the shelf's
        // text opens; Message_OpenText runs the text hook before StartTextbox sets
        // the talk actor, so set it first, as the conversation would have.
        play->msgCtx.talkActor = keeper;
        MM_Message_StartTextbox(play, kShelfDescTextId, keeper);
    }
    if (sFrame == 400) {
        MM_Playtest_DumpGameFramebuffer("w8-S2-play-textbox");
    }
    if (sFrame == 700) {
        MM_Message_CloseTextbox(play);
    }
    if (sFrame == 760) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[sShopPlaytestHost];
        const int had = gSaveContext.save.saveInfo.playerData.rupees;
        if (had < (int)check.price) {
            gSaveContext.save.saveInfo.playerData.rupees = (s16)check.price;
        }
        play->msgCtx.unk1206C = (s16)check.price;
        sSharedBefore = Combo_CountSharedItems(GAME_OOT, true);
        const s32 canBuy = shelf->canBuyFunc(play, shelf);
        fprintf(stderr,
                "[S2-PLAYTEST] buying check %u at %u rupees (wallet held %d, set to %d by the drive): canBuy=%d\n",
                (unsigned)sShopPlaytestHost, (unsigned)check.price, had,
                (int)gSaveContext.save.saveInfo.playerData.rupees, (int)canBuy);
        fflush(stderr);
        if (canBuy == CANBUY_RESULT_SUCCESS_2) {
            shelf->buyFunc(play, shelf);
            shelf->boughtFunc(play, shelf);
        }
    }
    if (sFrame == 900) {
        MM_Playtest_DumpGameFramebuffer("w8-S2-play-sold");
        fprintf(stderr,
                "[S2-PLAYTEST] OoT-bound shared-item records: %d before the purchase, %d after; slot obtained=%d; "
                "sold out for good=%d; outOfStock=%d\n",
                sSharedBefore, Combo_CountSharedItems(GAME_OOT, true),
                RANDO_SAVE_CHECKS[sShopPlaytestHost].obtained ? 1 : 0,
                Rando::ActorBehavior::ShopShelfSlotSold(sShopPlaytestHost) ? 1 : 0, shelf->isOutOfStock ? 1 : 0);
        fflush(stderr);
    }
}

#endif // RSBS_SINGLE_EXECUTABLE
