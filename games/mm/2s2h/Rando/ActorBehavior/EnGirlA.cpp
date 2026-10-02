#include "ActorBehavior.h"
#include "2s2h/ShipUtils.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Rando/MiscBehavior/Traps.h"

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Fsn/z_en_fsn.h"
#include "overlays/actors/ovl_En_GirlA/z_en_girla.h"
#include "overlays/actors/ovl_En_Ossan/z_en_ossan.h"
#include "overlays/actors/ovl_En_Sob1/z_en_sob1.h"
#include "overlays/actors/ovl_En_Trt/z_en_trt.h"

void MM_EnGirlA_Update2(EnGirlA* enGirlA, PlayState* play);
void EnGirlA_DoNothing(EnGirlA* enGirlA, PlayState* play);
void MM_EnGirlA_SetupAction(EnGirlA* enGirlA, EnGirlAActionFunc action);
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #800 pass 1: a shop slot may host a foreign (OoT) item.
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/ForeignModel.h"
#endif

#define RANDO_DESC_TEXT_ID 0x083F
#define RANDO_CHOICE_TEXT_ID 0x0840

static const std::vector<std::string> flavorTexts = {
    "Buy it, you won't regret it!",   "A must-have for any adventurer!", "A great gift for a friend!",
    "One of a kind, don't miss out!", "A great deal for the price!",     "On sale for a limited time!",
    "Get it while it's hot!",         "Don't miss out on this deal!",
};

// ----------------------------------------------------------------------------
// #800 pass 1: AN OoT ITEM IN AN MM SHOP SLOT. The paired fill leaves such a slot
// holding MM's junk cover (RI_JUNK) with the OoT item in the placement tables
// (Rando::Foreign). Every piece of the shop that reads the slot's item reads the
// foreign one instead: its name in the shop's lines, its model on the shelf, its
// give on purchase, and its stock (once for the whole game).
// ----------------------------------------------------------------------------

std::string Rando::ActorBehavior::ShopOfferedItemName(RandoCheckId rc, const std::string& nativeName,
                                                      bool withArticle) {
#ifdef RSBS_SINGLE_EXECUTABLE
    if (const char* foreign = Rando::Foreign::ForeignNameForCheck(rc)) {
        return withArticle ? std::string(Rando::Foreign::ForeignArticleForCheck(rc)) + foreign : std::string(foreign);
    }
#endif
    return nativeName;
}

bool Rando::ActorBehavior::ShopCounterSlotSold(RandoCheckId rc) {
#ifdef RSBS_SINGLE_EXECUTABLE
    if (Rando::Foreign::IsDeliveredForeignHost(rc)) {
        return true;
    }
#endif
    return RANDO_SAVE_CHECKS[rc].cycleObtained;
}

// A shelf slot is sold out: a native one once its item is no longer obtainable,
// a foreign-hosting one once its crossing was delivered (it never restocks).
static bool EnGirlA_RandoSoldOut(RandoCheckId rc) {
#ifdef RSBS_SINGLE_EXECUTABLE
    if (Rando::Foreign::IsForeignCheck(rc)) {
        return Rando::Foreign::IsDeliveredForeignHost(rc);
    }
#endif
    return !Rando::IsItemObtainable(RANDO_SAVE_CHECKS[rc].randoItemId, rc) && RANDO_SAVE_CHECKS[rc].obtained;
}

// The name the shelf lines give slot `rc`'s item (no article, as the native lines).
static std::string EnGirlA_RandoItemName(RandoCheckId rc) {
    return Rando::ActorBehavior::ShopOfferedItemName(
        rc, Rando::StaticData::Items[RANDO_SAVE_CHECKS[rc].randoItemId].name, false);
}

void EnGirlA_RandoDrawFunc(Actor* actor, PlayState* play) {
    EnGirlA* enGirlA = (EnGirlA*)actor;

    auto randoSaveCheck = RANDO_SAVE_CHECKS[actor->world.rot.z];

    Matrix_RotateYS(enGirlA->rotY, MTXMODE_APPLY);

#ifdef RSBS_SINGLE_EXECUTABLE
    // #800: the origin's own model, lit like a native shelf item; with no
    // drawable model, MM's model-less form (RI_NONE: a sparkle, no model), the
    // stand-in CheckQueue uses too. Never the cover's model.
    if (Rando::Foreign::IsForeignCheck((RandoCheckId)actor->world.rot.z)) {
        func_800B8118(actor, play, 0);
        func_800B8050(actor, play, 0);
        if (!Rando::Foreign::DrawForeignModelForCheck((RandoCheckId)actor->world.rot.z, play)) {
            Rando::DrawItem(RI_NONE, actor);
        }
        return;
    }
#endif

    Rando::DrawItem(randoSaveCheck.randoItemId, actor);
}

void EnGirlA_RandoBought(PlayState* play, EnGirlA* enGirlA) {
    enGirlA->isOutOfStock = true;
    enGirlA->actor.draw = NULL;
}

void EnGirlA_RandoRestock(PlayState* play, EnGirlA* enGirlA) {
    auto randoSaveCheck = RANDO_SAVE_CHECKS[enGirlA->actor.world.rot.z];

#ifdef RSBS_SINGLE_EXECUTABLE
    // #800: a foreign item is sold once; the cover it holds must not restock.
    if (Rando::Foreign::IsForeignCheck((RandoCheckId)enGirlA->actor.world.rot.z)) {
        if (!EnGirlA_RandoSoldOut((RandoCheckId)enGirlA->actor.world.rot.z)) {
            enGirlA->isOutOfStock = false;
            enGirlA->actor.draw = EnGirlA_RandoDrawFunc;
        }
        return;
    }
#endif

    if (Rando::IsItemObtainable(randoSaveCheck.randoItemId, (RandoCheckId)enGirlA->actor.world.rot.z)) {
        enGirlA->isOutOfStock = false;
        enGirlA->actor.draw = EnGirlA_RandoDrawFunc;
    }
}

s32 EnGirlA_RandoCanBuyFunc(PlayState* play, EnGirlA* enGirlA) {
    if (gSaveContext.save.saveInfo.playerData.rupees < play->msgCtx.unk1206C) {
        return CANBUY_RESULT_NEED_RUPEES;
    }

    auto randoSaveCheck = RANDO_SAVE_CHECKS[enGirlA->actor.world.rot.z];

#ifdef RSBS_SINGLE_EXECUTABLE
    // #800: the cover is always "obtainable"; the foreign item is not, once it
    // crossed.
    if (Rando::Foreign::IsForeignCheck((RandoCheckId)enGirlA->actor.world.rot.z)) {
        return EnGirlA_RandoSoldOut((RandoCheckId)enGirlA->actor.world.rot.z) ? CANBUY_RESULT_CANNOT_GET_NOW
                                                                                : CANBUY_RESULT_SUCCESS_2;
    }
#endif

    if (!Rando::IsItemObtainable(randoSaveCheck.randoItemId, (RandoCheckId)enGirlA->actor.world.rot.z)) {
        return CANBUY_RESULT_CANNOT_GET_NOW;
    }

    return CANBUY_RESULT_SUCCESS_2;
}

void EnGirlA_RandoBuyFunc(PlayState* play, EnGirlA* enGirlA) {
    auto& randoSaveCheck = RANDO_SAVE_CHECKS[enGirlA->actor.world.rot.z];
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800: a slot hosting an OoT item hands it to the shared structure through
    // the same foreign give a chest's pickup uses (CheckQueue.cpp), which also
    // marks the slot delivered. The shop keeps its own purchase dialogue, as for
    // a native purchase. With no live pairing (#610) nothing crosses, and the
    // slot degrades to the MM cover it holds, given below exactly as natively.
    if (Rando::Foreign::IsForeignCheck((RandoCheckId)enGirlA->actor.world.rot.z)) {
        MM_Rupees_ChangeBy(-play->msgCtx.unk1206C);
        if (Rando::Foreign::GiveForeignCheck((RandoCheckId)enGirlA->actor.world.rot.z)) {
            return;
        }
        Rando::GiveItem(Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)enGirlA->actor.world.rot.z));
        return;
    }
#endif
    RandoItemId randoItemId = Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)enGirlA->actor.world.rot.z);
    randoSaveCheck.obtained = true;
    MM_Rupees_ChangeBy(-play->msgCtx.unk1206C);
    if (randoItemId == RI_TRAP) {
        RollTrapType();
    }
    Rando::GiveItem(randoItemId);
}

void EnGirlA_RandoBuyFanfareFunc(PlayState* play, EnGirlA* enGirlA) {
    // No-op, if we made it here something went wrong
}

void EnGirlA_RandoInit(EnGirlA* enGirlA, PlayState* play) {
    enGirlA->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    enGirlA->actor.textId = RANDO_DESC_TEXT_ID;
    enGirlA->choiceTextId = RANDO_CHOICE_TEXT_ID;

    enGirlA->boughtFunc = EnGirlA_RandoBought;
    enGirlA->restockFunc = EnGirlA_RandoRestock;
    enGirlA->canBuyFunc = EnGirlA_RandoCanBuyFunc;
    enGirlA->buyFunc = EnGirlA_RandoBuyFunc;
    enGirlA->buyFanfareFunc = EnGirlA_RandoBuyFanfareFunc;

    enGirlA->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    MM_Actor_SetScale(&enGirlA->actor, 0.25f);
    enGirlA->actor.shape.yOffset = 24.0f;
    enGirlA->actor.shape.shadowScale = 4.0f;
    enGirlA->actor.floorHeight = enGirlA->actor.home.pos.y;
    enGirlA->actor.gravity = 0.0f;
    MM_EnGirlA_SetupAction(enGirlA, EnGirlA_DoNothing);
    enGirlA->isInitialized = true;
    enGirlA->mainActionFunc = MM_EnGirlA_Update2;
    enGirlA->isSelected = false;
    enGirlA->rotY = 0;
    enGirlA->initialRotY = enGirlA->actor.shape.rot.y;

    if (EnGirlA_RandoSoldOut((RandoCheckId)enGirlA->actor.world.rot.z)) {
        enGirlA->isOutOfStock = true;
        enGirlA->actor.draw = NULL;
    } else {
        enGirlA->isOutOfStock = false;
        enGirlA->actor.draw = EnGirlA_RandoDrawFunc;
    }
}

void renameStolenBombBag(u16* textId, bool* loadFromMessageTable) {
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    entry.msg = "Tonight's special, stolen from the Bomb Shop: %r{{itemName}}%w. Check it out!\x19\xA8";
    CustomMessage::Replace(&entry.msg, "{{itemName}}",
                           EnGirlA_RandoItemName(RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM));
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

void renameSpecialBargain(u16* textId, bool* loadFromMessageTable) {
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    entry.msg = "Tonight's bargain: %r{{itemName}}%w. Check it out!\x19\xA8";
    CustomMessage::Replace(&entry.msg, "{{itemName}}", EnGirlA_RandoItemName(RC_CURIOSITY_SHOP_SPECIAL_ITEM));
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

void ReplaceCannotBuyMessage(u16* textId, bool* loadFromMessageTable) {
    CustomMessage::Entry entry = {
        .msg = "Sorry, you can't buy this right now.\xE0",
    };

    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

RandoCheckId IdentifyShopItem(Actor* actor) {
    switch (MM_gPlayState->sceneId) {
        case SCENE_8ITEMSHOP:
            switch (actor->params) {
                case 10:
                case 18:
                    return RC_TRADING_POST_SHOP_ITEM_01;
                case 5:
                case 14:
                    return RC_TRADING_POST_SHOP_ITEM_02;
                case 6:
                case 17:
                    return RC_TRADING_POST_SHOP_ITEM_03;
                case 3:
                case 11:
                    return RC_TRADING_POST_SHOP_ITEM_04;
                case 7:
                case 16:
                    return RC_TRADING_POST_SHOP_ITEM_05;
                case 8:
                case 12:
                    return RC_TRADING_POST_SHOP_ITEM_06;
                case 9:
                case 15:
                    return RC_TRADING_POST_SHOP_ITEM_07;
                case 4:
                case 13:
                    return RC_TRADING_POST_SHOP_ITEM_08;
            }
            break;
        case SCENE_BOMYA:
            switch (actor->params) {
                case 26:
                    return RC_BOMB_SHOP_ITEM_01;
                case 25:
                    return RC_BOMB_SHOP_ITEM_02;
                case 24: // After saving Bomb Shop lady
                    return RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM;
                case 23:
                    return RC_BOMB_SHOP_ITEM_03;
            }
            break;
        case SCENE_WITCH_SHOP:
            switch (actor->params) {
                case 2:
                    return RC_HAGS_POTION_SHOP_ITEM_01;
                case 1:
                    return RC_HAGS_POTION_SHOP_ITEM_02;
                case 0:
                    return RC_HAGS_POTION_SHOP_ITEM_03;
            }
            break;
        case SCENE_GORONSHOP:
            switch (actor->params) {
                case 30:
                case 33:
                    return RC_GORON_SHOP_ITEM_01;
                case 31:
                case 34:
                    return RC_GORON_SHOP_ITEM_02;
                case 32:
                case 35:
                    return RC_GORON_SHOP_ITEM_03;
            }
            break;
        case SCENE_BANDROOM:
            switch (actor->params) {
                case 27:
                    return RC_ZORA_SHOP_ITEM_01;
                case 28:
                    return RC_ZORA_SHOP_ITEM_02;
                case 29:
                    return RC_ZORA_SHOP_ITEM_03;
            }
            break;
        case SCENE_AYASHIISHOP:
            switch (actor->params) {
                case 19: // Saved Bomb Shop lady and recovered big bomb bag, so a new item is in stock
                    return RC_CURIOSITY_SHOP_SPECIAL_ITEM;
                case 21: // Sakon stole the bomb bag, so the bomb shop check is in stock
                    return RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM;
            }
            break;
    }

    return RC_UNKNOWN;
}

RandoCheckId IdentifyActiveShopItem() {
    RandoCheckId randoCheckId = RC_UNKNOWN;

    if (MM_gPlayState->msgCtx.talkActor == nullptr) {
        return RC_UNKNOWN;
    }

    if (MM_gPlayState->msgCtx.talkActor->id == ACTOR_EN_TRT) {
        EnTrt* enTrt = (EnTrt*)MM_gPlayState->msgCtx.talkActor;
        if (enTrt->items[enTrt->cursorIndex] != nullptr) {
            randoCheckId = (RandoCheckId)enTrt->items[enTrt->cursorIndex]->actor.world.rot.z;
        }
    } else if (MM_gPlayState->msgCtx.talkActor->id == ACTOR_EN_OSSAN) {
        if (MM_gPlayState->msgCtx.talkActor->params == 0 || MM_gPlayState->msgCtx.talkActor->params == 1) {
            EnOssan* enOssan = (EnOssan*)MM_gPlayState->msgCtx.talkActor;
            if (enOssan->items[enOssan->cursorIndex] != nullptr) {
                randoCheckId = (RandoCheckId)enOssan->items[enOssan->cursorIndex]->actor.world.rot.z;
            }
        } else {
            EnSob1* enSob1 = (EnSob1*)MM_gPlayState->msgCtx.talkActor;
            if (enSob1->items[enSob1->cursorIndex] != nullptr) {
                randoCheckId = (RandoCheckId)enSob1->items[enSob1->cursorIndex]->actor.world.rot.z;
            }
        }
    } else if (MM_gPlayState->msgCtx.talkActor->id == ACTOR_EN_FSN) {
        EnFsn* enFsn = (EnFsn*)MM_gPlayState->msgCtx.talkActor;
        if (enFsn->items[enFsn->cursorIndex] != nullptr) {
            randoCheckId = (RandoCheckId)enFsn->items[enFsn->cursorIndex]->actor.world.rot.z;
        }
    }

    return randoCheckId;
}

void Rando::ActorBehavior::InitEnGirlABehavior() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_GIRLA, IS_RANDO, [](Actor* actor) {
        EnGirlA* enGirlA = (EnGirlA*)actor;

        RandoCheckId randoCheckId = IdentifyShopItem(actor);
        if (randoCheckId != RC_UNKNOWN && RANDO_SAVE_CHECKS[randoCheckId].shuffled) {
            enGirlA->actor.world.rot.z = randoCheckId;
            enGirlA->mainActionFunc = EnGirlA_RandoInit;
        }
    });

    // Shop item description
    COND_ID_HOOK(OnOpenText, RANDO_DESC_TEXT_ID, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        RandoCheckId randoCheckId = IdentifyActiveShopItem();

        if (randoCheckId == RC_UNKNOWN) {
            return;
        }

        auto randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        // Not using formatting here, to ensure the item name and price stay on one line
        entry.autoFormat = false;
        entry.msg = "\x01{{itemName}}: {{rupees}} Rupees\x11\x00";
        entry.msg += '\x00';
        CustomMessage::Replace(&entry.msg, "{{itemName}}", EnGirlA_RandoItemName(randoCheckId));
        CustomMessage::Replace(&entry.msg, "{{rupees}}", std::to_string(randoSaveCheck.price));

        if (EnGirlA_RandoSoldOut(randoCheckId)) {
            entry.msg += "Out of Stock";
        } else {
            entry.msg += flavorTexts[Ship_Random(0, flavorTexts.size())];
        }
        entry.msg += "\x1A\xBF";

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Shop item purchase
    COND_ID_HOOK(OnOpenText, RANDO_CHOICE_TEXT_ID, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        RandoCheckId randoCheckId = IdentifyActiveShopItem();

        if (randoCheckId == RC_UNKNOWN) {
            return;
        }

        auto randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        // Not using formatting here, to ensure the item name and price stay on one line
        entry.autoFormat = false;
        entry.firstItemCost = randoSaveCheck.price;
        entry.msg = "\x01{{itemName}}: {{rupees}} Rupees\x02\x11\xC2I'll buy it\x11No thanks\xBF";
        CustomMessage::Replace(&entry.msg, "{{itemName}}", EnGirlA_RandoItemName(randoCheckId));
        CustomMessage::Replace(&entry.msg, "{{rupees}}", std::to_string(randoSaveCheck.price));

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Magic Potion Shop Hag "I can't get the ingredients for this"
    COND_ID_HOOK(OnOpenText, 0x880, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        RandoCheckId randoCheckId = IdentifyActiveShopItem();

        if (randoCheckId == RC_UNKNOWN) {
            return;
        }

        auto randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        // Not using formatting here, to ensure the item name and price stay on one line
        entry.autoFormat = false;
        entry.firstItemCost = randoSaveCheck.price;
        entry.msg = "\x01{{itemName}}: {{itemPrice}} Rupees\x11\x00";
        entry.msg += '\x00';
        entry.msg += "I need a mushroom to make this.\x1A";
        CustomMessage::Replace(&entry.msg, "{{itemName}}", EnGirlA_RandoItemName(randoCheckId));
        CustomMessage::Replace(&entry.msg, "{{itemPrice}}", std::to_string(randoSaveCheck.price));
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Magic Potion Shop Hag "Well, I can use this to make something, come back later"
    COND_ID_HOOK(OnOpenText, 0x884, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        RandoCheckId randoCheckId = RC_HAGS_POTION_SHOP_ITEM_01;
        auto& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        if (!randoSaveCheck.shuffled || randoSaveCheck.eligible) {
            return;
        }

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "I used this to make %r{{itemName}}%w, take it!\x19";
        const std::string itemName = Rando::ActorBehavior::ShopOfferedItemName(
            randoCheckId, Rando::StaticData::GetItemName(randoSaveCheck.randoItemId), true);
        CustomMessage::Replace(&entry.msg, "{{itemName}}", itemName);

        // Mark the item as eligible for purchase
        randoSaveCheck.eligible = true;
        // Set flag that is normally set in the return experience that this skips
        SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_FREE_BLUE_POTION);
        // Mark the item as out of stock
        EnTrt* enTrt =
            (EnTrt*)MM_Actor_FindNearby(MM_gPlayState, &GET_PLAYER(MM_gPlayState)->actor, ACTOR_EN_TRT, ACTORCAT_NPC, 100.0f);
        if (enTrt != nullptr) {
            EnGirlA* enGirlA = enTrt->items[2];
            enGirlA->isOutOfStock = true;
            enGirlA->actor.draw = NULL;
        }

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Bomb Shop "We're expecting new stock" (hint)
    COND_ID_HOOK(OnOpenText, 0x648, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        auto randoSaveCheck = RANDO_SAVE_CHECKS[RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM];
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg =
            "If nothing devastating happens to Mommy tonight, we should be able to sell %r{{itemName}}%w.\x19\xA8";
        const std::string itemName = Rando::ActorBehavior::ShopOfferedItemName(
            RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM, Rando::StaticData::GetItemName(randoSaveCheck.randoItemId),
            true);
        CustomMessage::Replace(&entry.msg, "{{itemName}}", itemName);
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Bomb Shop "We should have had..."
    COND_ID_HOOK(OnOpenText, 0x64A, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "Thanks to a mishap, we did not receive our %r{{itemName}}%w stock. Maybe next time...\x19\xA8";
        CustomMessage::Replace(&entry.msg, "{{itemName}}",
                               EnGirlA_RandoItemName(RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM));
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Bomb Shop "I thought we could finally sell"
    COND_ID_HOOK(OnOpenText, 0x660, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        auto randoSaveCheck = RANDO_SAVE_CHECKS[RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM];

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "It's over... Now we'll never sell %r{{itemName}}%w...\x19\xA8";
        const std::string itemName = Rando::ActorBehavior::ShopOfferedItemName(
            RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM, Rando::StaticData::GetItemName(randoSaveCheck.randoItemId),
            true);
        CustomMessage::Replace(&entry.msg, "{{itemName}}", itemName);

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Bomb Shop "We just got a larger bomb bag in stock"
    COND_ID_HOOK(OnOpenText, 0x649, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "We just got some new stock: %r{{itemName}}%w.\x19\xA8";
        CustomMessage::Replace(&entry.msg, "{{itemName}}",
                               EnGirlA_RandoItemName(RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM));

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Curiosity Shop "Tonight's special was stolen" (check that is shared with Bomb Shop)
    COND_ID_HOOK(OnOpenText, 0x29D3, IS_RANDO, renameStolenBombBag);
    COND_ID_HOOK(OnOpenText, 0x29D7, IS_RANDO, renameStolenBombBag);
    // Curiosity Shop "Tonight's bargain is" (special item check)
    COND_ID_HOOK(OnOpenText, 0x29D4, IS_RANDO, renameSpecialBargain);
    COND_ID_HOOK(OnOpenText, 0x29D8, IS_RANDO, renameSpecialBargain);

    // Magic Potion Shop Hag CANBUY_RESULT_CANNOT_GET_NOW (this text ID does not exist and just softlocks)
    COND_ID_HOOK(OnOpenText, 0x643, IS_RANDO, ReplaceCannotBuyMessage);
    // Goron Shop CANBUY_RESULT_CANNOT_GET_NOW
    COND_ID_HOOK(OnOpenText, 0xBD2, IS_RANDO, ReplaceCannotBuyMessage);
    // Bomb Shop CANBUY_RESULT_CANNOT_GET_NOW
    COND_ID_HOOK(OnOpenText, 0x645, IS_RANDO, ReplaceCannotBuyMessage);
    // Trading Post CANBUY_RESULT_CANNOT_GET_NOW
    COND_ID_HOOK(OnOpenText, 0x6BE, IS_RANDO, ReplaceCannotBuyMessage);
    COND_ID_HOOK(OnOpenText, 0x6DB, IS_RANDO, ReplaceCannotBuyMessage);
    // Zora Shop CANBUY_RESULT_CANNOT_GET_NOW (this text ID does not exist and just softlocks)
    COND_ID_HOOK(OnOpenText, 0x12E1, IS_RANDO, ReplaceCannotBuyMessage);
}

#ifdef RSBS_SINGLE_EXECUTABLE
// ============================================================================
// TEST BRIDGE (ForeignItemGiveShop row, src/common/tests/test_foreign_shop_mm.c;
// #800 pass 1). Drives the REAL shelf functions a shopkeeper calls, in the order
// it calls them (EnOssan/EnTrt/EnFsn/EnSob1: canBuyFunc, then buyFunc on
// CANBUY_RESULT_SUCCESS_2, then the restock on the next visit), against a shelf
// actor standing on `randoCheckId`, which the caller has made a crossing host
// (placement present; pairing live or not, per `paired`).
//
// What it reads back: whether the purchase authored the crossing (the shared
// structure grew by one OoT-tagged CROSSING row) or, unpaired, authored nothing
// (#610); that the price was charged; that the slot stays sold for good; and that
// the counter refuses a second sale. Every piece of state it touches (the save
// check, the rupee counters, MM_gPlayState) is restored. Returns 0 on success, a
// step code otherwise, after printing the observation.
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include "shared_items.h" // src/common: Combo_CountSharedItems, the crossing record

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

// ============================================================================
// #800 PLAYTEST DRIVE (GameExports_SingleExe.cpp, gameplay round trip,
// RSBS_GP_MM_SHOP=1, 100 live frames into the MM play window). Picks the first
// OoT item the paired world's crossing store put on a shelf of the Bomb Shop or
// the Trading Post (the two Clock Town shops whose shelves stand every day), sets
// the clock to noon so both are open, and walks the player in through the shop's
// front door. Test-only: nothing reaches it unless the env var is set.
// Returns the host check id, 0 when the world has no such crossing, or -1 when
// a textbox is up (an arrival's own get-item), so the caller retries next frame.
// ============================================================================
#include "crossing_store.h" // src/common: the paired world's crossings
#include "foreign_model.h"  // src/common: the model answer the drive logs

static RandoCheckId sShopPlaytestHost = RC_UNKNOWN;
static SharedItem sShopPlaytestItem;

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
    if (MM_gPlayState == nullptr || MM_gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
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
                    "[S2-PLAYTEST] MM shop slot %u (%s) hosts OoT item %u (%s), price %u; walking into %s at noon "
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

// Inside the shop (called every live MM frame after the walk, same opt-in): at
// shop frame 40 log the shelf standing on the host (stock, draw, model answer);
// at 100 open its own description textbox through the shopkeeper with the cursor
// on it (the RANDO_DESC_TEXT_ID hook above builds the text); close it at 700; at
// 760 buy it through the shelf's real canBuy/buy/bought functions, as the
// shopkeeper does on "I'll buy it"; at 900 log the OoT-bound record and the
// sold-out state.
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
    // Post, params 0/1) or EnSob1 (the Bomb Shop), as IdentifyActiveShopItem reads it.
    const bool isOssan = keeper->params <= 1;
    EnGirlA** items = isOssan ? ((EnOssan*)keeper)->items : ((EnSob1*)keeper)->items;
    u8* cursor = isOssan ? &((EnOssan*)keeper)->cursorIndex : &((EnSob1*)keeper)->cursorIndex;
    if (sFrame == 100) {
        for (u8 i = 0; i < (isOssan ? 8 : 3); i++) {
            if (items[i] == shelf) {
                *cursor = i;
            }
        }
        fprintf(stderr, "[S2-PLAYTEST] opening the shelf's description textbox 0x%04X (cursor %u)\n",
                (unsigned)RANDO_DESC_TEXT_ID, (unsigned)*cursor);
        fflush(stderr);
        MM_Message_StartTextbox(play, RANDO_DESC_TEXT_ID, keeper);
    }
    if (sFrame == 700) {
        MM_Message_CloseTextbox(play);
    }
    if (sFrame == 760) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[sShopPlaytestHost];
        if (gSaveContext.save.saveInfo.playerData.rupees < (s16)check.price) {
            gSaveContext.save.saveInfo.playerData.rupees = (s16)check.price;
        }
        play->msgCtx.unk1206C = (s16)check.price;
        sSharedBefore = Combo_CountSharedItems(GAME_OOT, true);
        const s32 canBuy = shelf->canBuyFunc(play, shelf);
        fprintf(stderr, "[S2-PLAYTEST] buying check %u at %u rupees (have %d): canBuy=%d\n",
                (unsigned)sShopPlaytestHost, (unsigned)check.price,
                (int)gSaveContext.save.saveInfo.playerData.rupees, (int)canBuy);
        fflush(stderr);
        if (canBuy == CANBUY_RESULT_SUCCESS_2) {
            shelf->buyFunc(play, shelf);
            shelf->boughtFunc(play, shelf);
        }
    }
    if (sFrame == 900) {
        fprintf(stderr,
                "[S2-PLAYTEST] OoT-bound shared-item records: %d before the purchase, %d after; slot obtained=%d; "
                "sold out for good=%d; outOfStock=%d\n",
                sSharedBefore, Combo_CountSharedItems(GAME_OOT, true),
                RANDO_SAVE_CHECKS[sShopPlaytestHost].obtained ? 1 : 0,
                EnGirlA_RandoSoldOut(sShopPlaytestHost) ? 1 : 0, shelf->isOutOfStock ? 1 : 0);
        fflush(stderr);
    }
}
#endif // RSBS_SINGLE_EXECUTABLE
