/*
 * File: z_item_inbox.c
 * Overlay: ovl_Item_Inbox
 * Description: Zelda's magic effect when opening gates in castle collapse
 */

#include "z_item_inbox.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void OoT_ItemInbox_Init(Actor* thisx, PlayState* play);
void OoT_ItemInbox_Destroy(Actor* thisx, PlayState* play);
void OoT_ItemInbox_Update(Actor* thisx, PlayState* play);
void OoT_ItemInbox_Draw(Actor* thisx, PlayState* play);

void ItemInbox_Wait(ItemInbox* this, PlayState* play);

const ActorInit Item_Inbox_InitVars = {
    ACTOR_ITEM_INBOX,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(ItemInbox),
    (ActorFunc)OoT_ItemInbox_Init,
    (ActorFunc)OoT_ItemInbox_Destroy,
    (ActorFunc)OoT_ItemInbox_Update,
    (ActorFunc)OoT_ItemInbox_Draw,
    NULL,
};

void OoT_ItemInbox_Init(Actor* thisx, PlayState* play) {
    ItemInbox* this = (ItemInbox*)thisx;

    this->actionFunc = ItemInbox_Wait;
    OoT_Actor_SetScale(&this->actor, 0.2);
}

void OoT_ItemInbox_Destroy(Actor* thisx, PlayState* play) {
}

void ItemInbox_Wait(ItemInbox* this, PlayState* play) {
    if (OoT_Flags_GetTreasure(play, (this->actor.params >> 8) & 0x1F)) {
        OoT_Actor_Kill(&this->actor);
    }
}

void OoT_ItemInbox_Update(Actor* thisx, PlayState* play) {
    ItemInbox* this = (ItemInbox*)thisx;

    this->actionFunc(this, play);
}

void OoT_ItemInbox_Draw(Actor* thisx, PlayState* play) {
    ItemInbox* this = (ItemInbox*)thisx;

    func_8002EBCC(&this->actor, play, 0);
    func_8002ED80(&this->actor, play, 0);
    OoT_GetItem_Draw(play, this->actor.params & 0xFF);
}
