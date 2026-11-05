#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/CustomMessage/CustomMessage.h"

extern "C" {
#include "variables.h"
#include "overlays/actors/ovl_En_Js/z_en_js.h"
#include "objects/object_stk/object_stk.h"

void func_80968DD0(EnJs* enJs, PlayState* play);
void func_8096971C(EnJs* enJs, PlayState* play);
extern ColliderCylinderInit D_8096AC30;
}

void OverrideSubJsText(u16* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(MM_gPlayState);
    if (player->talkActor == NULL) {
        return;
    }
    s32 jsType = ENJS_GET_TYPE(player->talkActor);
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

    switch (*textId) {
        // Do you want to play? Yes/No
        case 0x2215: {
            switch (RANDO_SAVE_OPTIONS[RO_ACCESS_TRIALS]) {
                case RO_ACCESS_TRIALS_20_MASKS: {
                    u8 requiredMasks = 0;
                    switch (jsType) {
                        case 1:
                            requiredMasks = 2;
                            break;
                        case 2:
                            requiredMasks = 6;
                            break;
                        case 3:
                            requiredMasks = 12;
                            break;
                        case 4:
                            requiredMasks = 20;
                            break;
                    }

                    if (Rando::Logic::MoonMaskCount() < requiredMasks) {
                        entry.msg =
                            "You need more masks to play... Come back when you have at least {{requiredMasks}}...";
                        entry.nextMessageID = 0x2216;
                        CustomMessage::Replace(&entry.msg, "{{requiredMasks}}", std::to_string(requiredMasks));
                    }
                    break;
                }
                case RO_ACCESS_TRIALS_REMAINS: {
                    QuestItem questItem = QUEST_17; // Unused
                    RandoItemId itemId = RI_NONE;
                    switch (jsType) {
                        case 1:
                            questItem = QUEST_REMAINS_ODOLWA;
                            itemId = RI_REMAINS_ODOLWA;
                            break;
                        case 2:
                            questItem = QUEST_REMAINS_GOHT;
                            itemId = RI_REMAINS_GOHT;
                            break;
                        case 3:
                            questItem = QUEST_REMAINS_GYORG;
                            itemId = RI_REMAINS_GYORG;
                            break;
                        case 4:
                            questItem = QUEST_REMAINS_TWINMOLD;
                            itemId = RI_REMAINS_TWINMOLD;
                            break;
                    }
                    if (questItem != QUEST_17 && !CHECK_QUEST_ITEM(questItem)) {
                        entry.msg = "You need to find {{item}} before you can play...";
                        entry.nextMessageID = 0x2216;
                        CustomMessage::Replace(&entry.msg, "{{item}}", Rando::StaticData::GetItemName(itemId));
                    }
                    break;
                }
                case RO_ACCESS_TRIALS_FORMS:
                    if (jsType == 1 && !HAS_ITEM(ITEM_MASK_DEKU)) {
                        entry.msg = "You need to find the Deku Mask before you can play...";
                        entry.nextMessageID = 0x2216;
                    } else if (jsType == 2 && !HAS_ITEM(ITEM_MASK_GORON)) {
                        entry.msg = "You need to find the Goron Mask before you can play...";
                        entry.nextMessageID = 0x2216;
                    } else if (jsType == 3 && !HAS_ITEM(ITEM_MASK_ZORA)) {
                        entry.msg = "You need to find the Zora Mask before you can play...";
                        entry.nextMessageID = 0x2216;
                    }
                    break;
            }

            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
            break;
        }
        // You don't want to play? oh...
        case 0x2216: {
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = "...\xE0";
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
            return;
        }
    }
}

void OverrideMainJsText(u16* textId, bool* loadFromMessageTable) {
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    bool override = false;

    switch (*textId) {
        // Will you play...with me? Yes/No
        case 0x21FC: {
            if (!RANDO_SAVE_CHECKS[RC_MOON_FIERCE_DEITY_MASK].cycleObtained && Rando::Logic::MoonMaskCount() >= 20) {
                RANDO_SAVE_CHECKS[RC_MOON_FIERCE_DEITY_MASK].eligible = true;
                entry.msg = "You... You seem strong... I have a gift for you...";
                entry.nextMessageID = 0x21FD;
                override = true;
            }

            if (Rando::Logic::MoonMaskCount() < RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_MASKS_COUNT] ||
                Rando::Logic::RemainsCount() < RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_REMAINS_COUNT]) {
                entry.msg = "You are not strong enough to play with me...";
                entry.nextMessageID = 0x21FD;
                override = true;
            }
            break;
        }
        // Skip having to say yes twice
        // "You only have weak masks...So...you'll play?"
        case 0x21FE: {
            *textId = 0x2200;
            break;
        }
        // You're a boring kid.
        case 0x21FD: {
            entry.msg = "...\xE0";
            override = true;
            break;
        }
    }
    if (override) {
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    }
}

void EnJs_WaitForPlayer(EnJs* enJs, PlayState* play);
void EnJs_PromptForDialog(EnJs* enJs, PlayState* play);

void EnJs_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* enJs) {
    if (limbIndex == MOONCHILD_LIMB_HEAD) {
        static Vec3f D_8096AD14 = { 300.0f, 0.0f, 0.0f };
        MM_Matrix_MultVec3f(&D_8096AD14, &enJs->focus.pos);
        OPEN_DISPS(play->state.gfxCtx);

        Gfx_SetupDL25_Opa(play->state.gfxCtx);

        MM_Matrix_RotateZYX(0, -0x4000, -0x8BA, MTXMODE_APPLY);
        MM_Matrix_Translate(-25.0f, -295.0f, 0.0f, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        MM_gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSkullKidLinkMask1DL);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnJs_Draw(Actor* thisx, PlayState* play) {
    EnJs* enJs = (EnJs*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MM_SkelAnime_DrawFlexOpa(play, enJs->skelAnime.skeleton, enJs->skelAnime.jointTable, enJs->skelAnime.dListCount,
                             NULL, EnJs_PostLimbDraw, &enJs->actor);
}

void EnJs_HandleDialog(EnJs* enJs, PlayState* play) {
    if ((MM_Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && MM_Message_ShouldAdvance(play)) {
        if (play->msgCtx.choiceIndex != 0) {
            play->msgCtx.ocarinaMode = OCARINA_MODE_APPLY_SOT;
            gSaveContext.nextCutsceneIndex = 0;
            play->nextEntrance = ENTRANCE(CUTSCENE, 0);
            play->transitionTrigger = TRANS_TRIGGER_START;
            play->transitionType = 2;
        }
        enJs->actionFunc = EnJs_WaitForPlayer;
    }
}

void EnJs_PromptForDialog(EnJs* enJs, PlayState* play) {
    if (Actor_TalkOfferAccepted(&enJs->actor, &play->state)) {
        enJs->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        MM_Message_StartTextbox(play, 0x1B8A, &enJs->actor);
        play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_SOT;
        enJs->actionFunc = EnJs_HandleDialog;
    } else if (enJs->actor.xzDistToPlayer < 100.0f) {
        Actor_OfferTalk(&enJs->actor, play, 100.0f);
    }
}

void EnJs_WaitForPlayer(EnJs* enJs, PlayState* play) {
    if (Actor_TalkOfferAccepted(&enJs->actor, &play->state)) {
        MM_Message_StartTextbox(play, 0x2201, &enJs->actor);
        enJs->actionFunc = EnJs_PromptForDialog;
    } else if (enJs->actor.xzDistToPlayer < 100.0f) {
        Actor_OfferTalk(&enJs->actor, play, 100.0f);
    }
}

void Rando::ActorBehavior::InitEnJsBehavior() {
    COND_VB_SHOULD(VB_JS_CONSIDER_ELIGIBLE_FOR_DEITY, IS_RANDO, { *should = false; });

    COND_VB_SHOULD(VB_JS_OVERRIDE_MASK_CHECK, IS_RANDO, {
        s32* jsType = va_arg(args, s32*);
        bool* result = va_arg(args, bool*);

        switch (*jsType) {
            case 1:
            case 2:
            case 3:
            case 4:
                *should = true;
                *result = true;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
                if (MM_gPlayState->sceneId != SCENE_SOUGEN) {
                    *should = true;
                    *result = false;
                }
                break;
            default:
                break;
        }
    });

    COND_ID_HOOK(OnOpenText, 0x2215, IS_RANDO, OverrideSubJsText);
    COND_ID_HOOK(OnOpenText, 0x2216, IS_RANDO, OverrideSubJsText);
    COND_ID_HOOK(OnOpenText, 0x21FC, IS_RANDO, OverrideMainJsText);
    COND_ID_HOOK(OnOpenText, 0x21FE, IS_RANDO, OverrideMainJsText);
    COND_ID_HOOK(OnOpenText, 0x21FD, IS_RANDO, OverrideMainJsText);

    // Handle spawning an extra Moon Child to reset time on the Moon and Clock Tower Rooftop
    COND_HOOK(OnSceneInit, IS_RANDO, [](s8 sceneId, s8 spawnNum) {
        if (sceneId == SCENE_SOUGEN) {
            MM_Actor_Spawn(&MM_gPlayState->actorCtx, MM_gPlayState, ACTOR_EN_JS, -140.0f, -21.0f, 800.0f, 0, -25112, 0,
                           13);
        } else if (sceneId == SCENE_OKUJOU) {
            MM_Actor_Spawn(&MM_gPlayState->actorCtx, MM_gPlayState, ACTOR_EN_JS, 162.0f, 34.0f, 40.0f, 0, -16384, 0,
                           13);
        }
    });

    COND_VB_SHOULD(VB_ENABLE_OBJECT_DEPENDENCY, IS_RANDO, {
        ObjectId objectId = (ObjectId)va_arg(args, int);
        if (objectId == OBJECT_OB && MM_gPlayState->sceneId == SCENE_OKUJOU) {
            *should = false;
        }
    });

    COND_ID_HOOK(OnActorInit, ACTOR_EN_JS, IS_RANDO, [](Actor* actor) {
        EnJs* enJs = (EnJs*)actor;
        if (ENJS_GET_TYPE(actor) != 13) {
            return;
        }

        enJs->actor.draw = EnJs_Draw;
        func_80968DD0(enJs, MM_gPlayState);
        enJs->actionFunc = EnJs_WaitForPlayer;
    });

    COND_ID_HOOK(OnOpenText, 0x2201, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        Player* player = GET_PLAYER(MM_gPlayState);
        if (player->talkActor == NULL || player->talkActor->id != ACTOR_EN_JS) {
            return;
        }
        s32 jsType = ENJS_GET_TYPE(player->talkActor);
        if (jsType != 13) {
            return;
        }

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "Still playing? I can return you to the %gFirst Day%w if you want...";
        entry.nextMessageID = 0xFFFF;
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    COND_ID_HOOK(OnOpenText, 0x1B8A, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        Player* player = GET_PLAYER(MM_gPlayState);
        if (player->talkActor == NULL || player->talkActor->id != ACTOR_EN_JS) {
            return;
        }
        s32 jsType = ENJS_GET_TYPE(player->talkActor);
        if (jsType != 13) {
            return;
        }

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "Return to the First Day?\n \n%g\xC2Yes\nNo";
        entry.nextMessageID = 0xFFFF;
        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });
}
