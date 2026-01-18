/*
 * File: z_en_dns.c
 * Overlay: En_Dns
 * Description: Deku Salesman
 */

#include "z_en_dns.h"
#include "objects/object_shopnuts/object_shopnuts.h"
#include "vt.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void OoT_EnDns_Init(Actor* thisx, PlayState* play);
void OoT_EnDns_Destroy(Actor* thisx, PlayState* play);
void OoT_EnDns_Update(Actor* thisx, PlayState* play);
void OoT_EnDns_Draw(Actor* thisx, PlayState* play);

u32 EnDns_CanBuyPrice(EnDns* this);
u32 EnDns_CanBuyDekuNuts(EnDns* this);
u32 EnDns_CanBuyDekuSticks(EnDns* this);
u32 EnDns_CanBuyDekuSeeds(EnDns* this);
u32 EnDns_CanBuyDekuShield(EnDns* this);
u32 EnDns_CanBuyBombs(EnDns* this);
u32 EnDns_CanBuyArrows(EnDns* this);
u32 EnDns_CanBuyBottle(EnDns* this);

void EnDns_PayPrice(EnDns* this);
void EnDns_PayForDekuNuts(EnDns* this);
void EnDns_PayForHeartPiece(EnDns* this);
void EnDns_PayForBombs(EnDns* this);
void EnDns_PayForArrows(EnDns* this);
void EnDns_PayForDekuStickUpgrade(EnDns* this);
void EnDns_PayForDekuNutUpgrade(EnDns* this);

void EnDns_SetupIdle(EnDns* this, PlayState* play);
void EnDns_Idle(EnDns* this, PlayState* play);
void EnDns_Talk(EnDns* this, PlayState* play);
void EnDns_OfferSaleItem(EnDns* this, PlayState* play);
void EnDns_SetupSale(EnDns* this, PlayState* play);
void EnDns_Sale(EnDns* this, PlayState* play);
void EnDns_SetupBurrow(EnDns* this, PlayState* play);
void EnDns_SetupNoSaleBurrow(EnDns* this, PlayState* play);
void EnDns_Burrow(EnDns* this, PlayState* play);
void EnDns_PostBurrow(EnDns* this, PlayState* play);

const ActorInit En_Dns_InitVars = {
    ACTOR_EN_DNS,
    ACTORCAT_BG,
    FLAGS,
    OBJECT_SHOPNUTS,
    sizeof(EnDns),
    (ActorFunc)OoT_EnDns_Init,
    (ActorFunc)OoT_EnDns_Destroy,
    (ActorFunc)OoT_EnDns_Update,
    (ActorFunc)OoT_EnDns_Draw,
    NULL,
};

static ColliderCylinderInitType1 OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 18, 32, 0, { 0, 0, 0 } },
};

static u16 sStartingTextIds[] = {
    0x10A0, 0x10A1, 0x10A2, 0x10CA, 0x10CB, 0x10CC, 0x10CD, 0x10CE, 0x10CF, 0x10DC, 0x10DD,
};

// Debug text: "sells"  { "Deku Nuts",    "Deku Sticks",        "Piece of Heart",  "Deku Seeds",
//                        "Deku Shield",  "Bombs",              "Arrows",          "Red Potion",
//                        "Green Potion", "Deku Stick Upgrade", "Deku Nut Upgrade" }
static char* sItemDebugTxt[] = {
    "デクの実売り            ", "デクの棒売り            ", "ハートの欠片売り        ", "デクの種売り            ",
    "デクの盾売り            ", "バクダン売り            ", "矢売り                  ", "赤のくすり売り          ",
    "緑のくすり売り          ", "デクの棒持てる数を増やす", "デクの実持てる数を増やす",
};

static DnsItemEntry sItemNuts = { 20, 5, GI_NUTS_5_2, EnDns_CanBuyDekuNuts, EnDns_PayForDekuNuts };
static DnsItemEntry sItemSticks = { 15, 1, GI_STICKS_1, EnDns_CanBuyDekuSticks, EnDns_PayPrice };
static DnsItemEntry sItemHeartPiece = { 10, 1, GI_HEART_PIECE, EnDns_CanBuyPrice, EnDns_PayForHeartPiece };
static DnsItemEntry sItemSeeds = { 40, 30, GI_SEEDS_30, EnDns_CanBuyDekuSeeds, EnDns_PayPrice };
static DnsItemEntry sItemDekuShield = { 50, 1, GI_SHIELD_DEKU, EnDns_CanBuyDekuShield, EnDns_PayPrice };
static DnsItemEntry sItemBombs = { 40, 5, GI_BOMBS_5, EnDns_CanBuyBombs, EnDns_PayForBombs };
static DnsItemEntry sItemArrows = { 70, 20, GI_ARROWS_LARGE, EnDns_CanBuyArrows, EnDns_PayForArrows };
static DnsItemEntry sItemRedPotion = { 40, 1, GI_POTION_RED, EnDns_CanBuyBottle, EnDns_PayPrice };
static DnsItemEntry sItemGreenPotion = { 40, 1, GI_POTION_GREEN, EnDns_CanBuyBottle, EnDns_PayPrice };

static DnsItemEntry sItemStickUpgrade = { 40, 1, GI_STICK_UPGRADE_20, EnDns_CanBuyPrice, EnDns_PayForDekuStickUpgrade };
static DnsItemEntry sItemNutUpgrade = { 40, 1, GI_NUT_UPGRADE_30, EnDns_CanBuyPrice, EnDns_PayForDekuNutUpgrade };

static DnsItemEntry* sItemEntries[] = {
    &sItemNuts,   &sItemSticks,    &sItemHeartPiece,  &sItemSeeds,        &sItemDekuShield, &sItemBombs,
    &sItemArrows, &sItemRedPotion, &sItemGreenPotion, &sItemStickUpgrade, &sItemNutUpgrade,
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_S8(naviEnemyId, 0x4E, ICHAIN_CONTINUE),
    ICHAIN_U8(targetMode, 2, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 30, ICHAIN_STOP),
};

static AnimationMinimalInfo OoT_sAnimationInfo[] = {
    { &gBusinessScrubNervousIdleAnim, ANIMMODE_LOOP, 0.0f },
    { &gBusinessScrubLeaveBurrowAnim, ANIMMODE_ONCE, 0.0f },
    { &gBusinessScrubNervousTransitionAnim, ANIMMODE_ONCE, 0.0f },
};

void OoT_EnDns_Init(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;

    if (this->actor.params < 0) {
        // "Function Error (Deku Salesman)"
        osSyncPrintf(VT_FGCOL(RED) "引数エラー（売りナッツ）[ arg_data = %d ]" VT_RST "\n", this->actor.params);
        OoT_Actor_Kill(&this->actor);
        return;
    }
    // Sell Seeds instead of Arrows if Link is child
    if ((this->actor.params == DNS_TYPE_ARROWS_30) && (LINK_AGE_IN_YEARS == YEARS_CHILD)) {
        this->actor.params = DNS_TYPE_DEKU_SEEDS_30;
    }
    // "Deku Salesman"
    osSyncPrintf(VT_FGCOL(GREEN) "◆◆◆ 売りナッツ『%s』 ◆◆◆" VT_RST "\n", sItemDebugTxt[this->actor.params],
                 this->actor.params);
    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    OoT_SkelAnime_InitFlex(play, &this->skelAnime, &gBusinessScrubSkel, &gBusinessScrubNervousTransitionAnim,
                       this->jointTable, this->morphTable, 18);
    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinderType1(play, &this->collider, &this->actor, &OoT_sCylinderInit);
    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 35.0f);
    OoT_Actor_SetScale(&this->actor, 0.01f);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->maintainCollider = 1;
    this->standOnGround = 1;
    this->dropCollectible = 0;
    this->actor.speedXZ = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -1.0f;
    this->actor.textId = sStartingTextIds[this->actor.params];
    this->dnsItemEntry = sItemEntries[this->actor.params];
    this->actionFunc = EnDns_SetupIdle;
}

void OoT_EnDns_Destroy(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);

    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

void OoT_EnDns_ChangeAnim(EnDns* this, u8 index) {
    s16 frameCount;

    frameCount = OoT_Animation_GetLastFrame(OoT_sAnimationInfo[index].animation);
    this->animIndex = index; // Not used anywhere else?
    OoT_Animation_Change(&this->skelAnime, OoT_sAnimationInfo[index].animation, 1.0f, 0.0f, (f32)frameCount,
                     OoT_sAnimationInfo[index].mode, OoT_sAnimationInfo[index].morphFrames);
}

/* Item give checking functions */

u32 EnDns_CanBuyDekuNuts(EnDns* this) {
    if ((CUR_CAPACITY(UPG_NUTS) != 0) && (AMMO(ITEM_NUT) >= CUR_CAPACITY(UPG_NUTS))) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    if (OoT_Item_CheckObtainability(ITEM_NUT) == ITEM_NONE) {
        return DNS_CANBUY_RESULT_SUCCESS_NEW_ITEM;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyDekuSticks(EnDns* this) {
    if ((CUR_CAPACITY(UPG_STICKS) != 0) && (AMMO(ITEM_STICK) >= CUR_CAPACITY(UPG_STICKS))) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    if (OoT_Item_CheckObtainability(ITEM_STICK) == ITEM_NONE) {
        return DNS_CANBUY_RESULT_SUCCESS_NEW_ITEM;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyPrice(EnDns* this) {
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyDekuSeeds(EnDns* this) {
    if (INV_CONTENT(ITEM_SLINGSHOT) == ITEM_NONE) {
        return DNS_CANBUY_RESULT_CANT_GET_NOW;
    }
    if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    if (OoT_Item_CheckObtainability(ITEM_SEEDS) == ITEM_NONE) {
        return DNS_CANBUY_RESULT_SUCCESS_NEW_ITEM;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyDekuShield(EnDns* this) {
    if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU)) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyBombs(EnDns* this) {
    if (!CHECK_QUEST_ITEM(QUEST_GORON_RUBY)) {
        return DNS_CANBUY_RESULT_CANT_GET_NOW;
    }
    if (AMMO(ITEM_BOMB) >= CUR_CAPACITY(UPG_BOMB_BAG)) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyArrows(EnDns* this) {
    if (OoT_Item_CheckObtainability(ITEM_BOW) == ITEM_NONE) {
        return DNS_CANBUY_RESULT_CANT_GET_NOW;
    }
    if (AMMO(ITEM_BOW) >= CUR_CAPACITY(UPG_QUIVER)) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

u32 EnDns_CanBuyBottle(EnDns* this) {
    if (!OoT_Inventory_HasEmptyBottle()) {
        return DNS_CANBUY_RESULT_CAPACITY_FULL;
    }
    if (gSaveContext.rupees < this->dnsItemEntry->itemPrice) {
        return DNS_CANBUY_RESULT_NEED_RUPEES;
    }
    return DNS_CANBUY_RESULT_SUCCESS;
}

/* Paying and flagging functions */

void EnDns_PayPrice(EnDns* this) {
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_PayForDekuNuts(EnDns* this) {
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_PayForHeartPiece(EnDns* this) {
    Flags_SetItemGetInf(ITEMGETINF_DEKU_SCRUB_HEART_PIECE);
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_PayForBombs(EnDns* this) {
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_PayForArrows(EnDns* this) {
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_PayForDekuStickUpgrade(EnDns* this) {
    OoT_Flags_SetInfTable(INFTABLE_BOUGHT_STICK_UPGRADE);
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_PayForDekuNutUpgrade(EnDns* this) {
    OoT_Flags_SetInfTable(INFTABLE_BOUGHT_NUT_UPGRADE);
    OoT_Rupees_ChangeBy(-this->dnsItemEntry->itemPrice);
}

void EnDns_SetupIdle(EnDns* this, PlayState* play) {
    if (this->skelAnime.curFrame == this->skelAnime.endFrame) {
        this->actionFunc = EnDns_Idle;
        OoT_EnDns_ChangeAnim(this, DNS_ANIM_IDLE);
    }
}

void EnDns_Idle(EnDns* this, PlayState* play) {
    OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 2000, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = EnDns_Talk;
    } else {
        if ((this->collider.base.ocFlags1 & OC1_HIT) || this->actor.isTargeted) {
            this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        } else {
            this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        }
        if (this->actor.xzDistToPlayer < 130.0f) {
            func_8002F2F4(&this->actor, play);
        }
    }
}

void EnDns_Talk(EnDns* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && OoT_Message_ShouldAdvance(play)) {
        switch (play->msgCtx.choiceIndex) {
            case 0: // OK
                switch (this->dnsItemEntry->purchaseableCheck(this)) {
                    case DNS_CANBUY_RESULT_NEED_RUPEES:
                        OoT_Message_ContinueTextbox(play, 0x10A5);
                        this->actionFunc = EnDns_SetupNoSaleBurrow;
                        break;
                    case DNS_CANBUY_RESULT_CAPACITY_FULL:
                        OoT_Message_ContinueTextbox(play, 0x10A6);
                        this->actionFunc = EnDns_SetupNoSaleBurrow;
                        break;
                    case DNS_CANBUY_RESULT_CANT_GET_NOW:
                        OoT_Message_ContinueTextbox(play, 0x10DE);
                        this->actionFunc = EnDns_SetupNoSaleBurrow;
                        break;
                    case DNS_CANBUY_RESULT_SUCCESS_NEW_ITEM:
                    case DNS_CANBUY_RESULT_SUCCESS:
                        OoT_Message_ContinueTextbox(play, 0x10A7);
                        this->actionFunc = EnDns_SetupSale;
                        break;
                }
                break;
            case 1: // No way
                OoT_Message_ContinueTextbox(play, 0x10A4);
                this->actionFunc = EnDns_SetupNoSaleBurrow;
        }
    }
}

void EnDns_OfferSaleItem(EnDns* this, PlayState* play) {
    u16 pendingGetItemId;
    if (this->actor.params == DNS_TYPE_DEKU_STICK_UPGRADE) {
        if (CUR_UPG_VALUE(UPG_STICKS) < 2) {
            pendingGetItemId = GI_STICK_UPGRADE_20;
        } else {
            pendingGetItemId = GI_STICK_UPGRADE_30;
        }
    } else if (this->actor.params == DNS_TYPE_DEKU_NUT_UPGRADE) {
        if (CUR_UPG_VALUE(UPG_NUTS) < 2) {
            pendingGetItemId = GI_NUT_UPGRADE_30;
        } else {
            pendingGetItemId = GI_NUT_UPGRADE_40;
        }
    } else {
        pendingGetItemId = this->dnsItemEntry->getItemId;
    }
    GetItemEntry itemEntry = ItemTable_Retrieve(pendingGetItemId);
    gSaveContext.ship.pendingSale = itemEntry.itemId;
    gSaveContext.ship.pendingSaleMod = itemEntry.modIndex;
    OoT_Actor_OfferGetItem(&this->actor, play, pendingGetItemId, 130.0f, 100.0f);
}

void EnDns_SetupSale(EnDns* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && OoT_Message_ShouldAdvance(play)) {
        OoT_Message_CloseTextbox(play);
        if (GameInteractor_Should(VB_GIVE_ITEM_FROM_BUSINESS_SCRUB, true, this)) {
            EnDns_OfferSaleItem(this, play);
        }
        this->actionFunc = EnDns_Sale;
    }
}

void EnDns_Sale(EnDns* this, PlayState* play) {
    if (OoT_Actor_HasParent(&this->actor, play) || !GameInteractor_Should(VB_GIVE_ITEM_FROM_BUSINESS_SCRUB, true, this)) {
        this->actor.parent = NULL;
        this->actionFunc = EnDns_SetupBurrow;
    } else {
        EnDns_OfferSaleItem(this, play);
    }
}

void EnDns_SetupBurrow(EnDns* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) {
        if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) && OoT_Message_ShouldAdvance(play)) {
            this->dnsItemEntry->setRupeesAndFlags(this);
            this->dropCollectible = 1;
            this->maintainCollider = 0;
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            OoT_EnDns_ChangeAnim(this, DNS_ANIM_BURROW);
            this->actionFunc = EnDns_Burrow;
        }
    } else {
        this->dnsItemEntry->setRupeesAndFlags(this);
        this->dropCollectible = 1;
        this->maintainCollider = 0;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        OoT_EnDns_ChangeAnim(this, DNS_ANIM_BURROW);
        this->actionFunc = EnDns_Burrow;
    }
}

void EnDns_SetupNoSaleBurrow(EnDns* this, PlayState* play) {
    if ((OoT_Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) && OoT_Message_ShouldAdvance(play)) {
        this->maintainCollider = 0;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        OoT_EnDns_ChangeAnim(this, DNS_ANIM_BURROW);
        this->actionFunc = EnDns_Burrow;
    }
}

void EnDns_Burrow(EnDns* this, PlayState* play) {
    f32 frameCount = OoT_Animation_GetLastFrame(&gBusinessScrubLeaveBurrowAnim);

    if (this->skelAnime.curFrame == frameCount) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_AKINDONUTS_HIDE);
        this->actionFunc = EnDns_PostBurrow;
        this->standOnGround = 0;
        this->yInitPos = this->actor.world.pos.y;
    }
}

void EnDns_PostBurrow(EnDns* this, PlayState* play) {
    f32 depth;
    Vec3f initPos;
    s32 i;

    depth = this->yInitPos - this->actor.world.pos.y;
    if ((this->dustTimer & 3) == 0) {
        initPos.x = this->actor.world.pos.x;
        initPos.y = this->yInitPos;
        initPos.z = this->actor.world.pos.z;
        func_80028990(play, 20.0f, &initPos);
    }
    this->actor.shape.rot.y += 0x2000;
    // Drops only if you bought its item
    if (depth > 400.0f) {
        if (this->dropCollectible) {
            initPos.x = this->actor.world.pos.x;
            initPos.y = this->yInitPos;
            initPos.z = this->actor.world.pos.z;
            for (i = 0; i < 3; i++) {
                OoT_Item_DropCollectible(play, &initPos, ITEM00_HEART);
            }
        }
        OoT_Actor_Kill(&this->actor);
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    }
}

void OoT_EnDns_Update(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;
    s16 pad;

    this->dustTimer++;
    this->actor.textId = sStartingTextIds[this->actor.params];
    OoT_Actor_SetFocus(&this->actor, 60.0f);
    OoT_Actor_SetScale(&this->actor, 0.01f);
    OoT_SkelAnime_Update(&this->skelAnime);
    Actor_MoveXZGravity(&this->actor);
    this->actionFunc(this, play);
    if (this->standOnGround) {
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 20.0f, 4);
    }
    if (this->maintainCollider) {
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

void OoT_EnDns_Draw(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, NULL, NULL, &this->actor);
}
