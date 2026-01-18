/*
 * File: z_item_inbox.c
 * Overlay: ovl_Item_Inbox
 * Description: Unused, can draw GetItem models. Perhaps intended to draw items inside chests.
 */

#include "z_item_inbox.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void MM_ItemInbox_Init(Actor* thisx, PlayState* play);
void MM_ItemInbox_Destroy(Actor* thisx, PlayState* play);
void MM_ItemInbox_Update(Actor* thisx, PlayState* play);
void MM_ItemInbox_Draw(Actor* thisx, PlayState* play);

void ItemInbox_Idle(ItemInbox* this, PlayState* play);

ActorProfile Item_Inbox_Profile = {
    /**/ ACTOR_ITEM_INBOX,
    /**/ ACTORCAT_NPC,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(ItemInbox),
    /**/ MM_ItemInbox_Init,
    /**/ MM_ItemInbox_Destroy,
    /**/ MM_ItemInbox_Update,
    /**/ MM_ItemInbox_Draw,
};

void MM_ItemInbox_Init(Actor* thisx, PlayState* play) {
    ItemInbox* this = (ItemInbox*)thisx;

    this->actionFunc = ItemInbox_Idle;
    MM_Actor_SetScale(&this->actor, 0.2f);
}

void MM_ItemInbox_Destroy(Actor* thisx, PlayState* play) {
}

void ItemInbox_Idle(ItemInbox* this, PlayState* play) {
    if (MM_Flags_GetTreasure(play, (this->actor.params >> 8) & 0x1F)) {
        MM_Actor_Kill(&this->actor);
    }
}

void MM_ItemInbox_Update(Actor* thisx, PlayState* play) {
    ItemInbox* this = (ItemInbox*)thisx;

    this->actionFunc(this, play);
}

void MM_ItemInbox_Draw(Actor* thisx, PlayState* play) {
    ItemInbox* this = (ItemInbox*)thisx;

    func_800B8050(&this->actor, play, 0);
    func_800B8118(&this->actor, play, 0);
    MM_GetItem_Draw(play, this->actor.params & 0xFF);
}
