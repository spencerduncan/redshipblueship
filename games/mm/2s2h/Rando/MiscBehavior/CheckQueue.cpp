#include "MiscBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"
#include "Traps.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/Foreign.h" // Lane C1 (#392): foreign-check lookup + shared-structure recording
// Lane 6 (#502): drain of the deferred cross-game give (ForeignItemsSingleExe.cpp).
extern "C" int MM_ForeignItem_FlushPending(void);
#endif

extern "C" {
#include "variables.h"
#include <functions.h>
extern TexturePtr MM_gItemIcons[131];
extern s16 D_801CFF94[250];
}

static bool queued = false;

// This function handles queuing up item gives that the player has been marked as eligible for. If you are looking for
// the behavior of the actual giving itself, the heavy lifting is done by the GameInteractor queue. This function is
// currently called every frame, and loops through the entire list of checks, this works for now but as the check list
// grows we should keep an eye on performance.
//
// Though it seems counter-intuitive, we currently only allow one thing from randommizer to be queued at a time,
// primarily because of the way an item can be converted may change as you queue up multiple items. This is actually
// fine for both the giving/drawing, as we can call ConvertItem() inside the Give/Draw lambda, but the message we
// pass to the queue is static and would need to be updated if we allowed multiple items to be queued at once.
void Rando::MiscBehavior::CheckQueue() {
#ifdef RSBS_SINGLE_EXECUTABLE
    // Lane 6 (#502): the gameplay-gated frame tick MM's foreign-item give
    // defers onto. MM's redemption point (MM_Play_ConsumeStartupEntrance)
    // runs before `MM_gPlayState = this`, so the award callback queues rather
    // than dereferencing a null play; this is where the queue drains. See
    // 2s2h/Rando/ForeignItemsSingleExe.cpp for the full argument.
    //
    // ABOVE the `queued` early-out on purpose: a pending cross-game give must
    // not be held behind an unrelated in-world check that happens to be
    // mid-cutscene. The flush is a cheap counter read when nothing is pending,
    // which is every frame but the handful after an arrival.
    MM_ForeignItem_FlushPending();
#endif

    if (queued) {
        return;
    }

    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        auto randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        if (randoSaveCheck.eligible) {
            queued = true;

#ifdef RSBS_SINGLE_EXECUTABLE
            // Lane C1 (#392): a check hosting a FOREIGN item (an OoT item in
            // this MM world — gComboCtx.foreignPlacements) hands the item to
            // the shared structure instead of MM's give path.
            //
            // PRESENTED AS AN ORDINARY MM PICKUP (#510). It used to announce
            // itself — a giant-rupee stand-in model, a cross-game aura, and a
            // textbox explaining the item belonged to Ocarina of Time and would
            // be awarded there. All three are gone. A foreign item now reads
            // exactly like any other check: "You found the Fairy Bow!".
            //
            // NO STAND-IN MODEL. We cannot draw OoT's real model — oot.o2r and
            // mm.o2r share one flat ArchiveManager namespace with 151 documented
            // object collisions (docs/resource-namespace-audit.md), 41 of them in
            // exactly the object_gi_* class this would need, and oot.o2r is not
            // even mounted until OoT has been entered (rsbs/src/main.cpp's
            // EnsureGameArchivesLoaded). So instead of substituting a DIFFERENT
            // item's model, we use MM's own model-LESS pickup form: RI_NONE
            // draws no model and falls through to DrawSparkles, which is the
            // identical presentation MM already gives Magic Upgrades, the Swim
            // ability and Progressive Time (DrawItem.cpp). Showing nothing is
            // native; showing a rupee that is not a rupee was the placeholder.
            if (Rando::Foreign::IsForeignCheck(randoCheckId)) {
                MM_GameEvents_Queue().emplace_back(GIEventGiveItem{
                    // Always cutscene: a foreign item is progression by
                    // construction (the pool excludes junk), and this is the
                    // only surface that names it, so it must not be swallowed by
                    // the player's skip-junk-cutscene tier.
                    .showGetItemCutscene = true,
                    .param = (int16_t)randoCheckId,
                    .giveItem =
                        [](Actor* actor, PlayState* play) {
                            auto& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                            const RandoCheckId checkId = (RandoCheckId)CUSTOM_ITEM_PARAM;
                            const char* foreignName = Rando::Foreign::ForeignNameForCheck(checkId);

                            // Record into gComboCtx.sharedItemsTagged (origin-
                            // tagged, durable immediately, de-duped). OoT's
                            // consumer awards it on the next arrival there.
                            Rando::Foreign::RecordForeignPickup(checkId);

                            // Same sentence shape as the native branch below:
                            // "You found " + article + name + "!". The article
                            // rides the pooled descriptor because MM cannot read
                            // OoT's item table (ADR 0002).
                            CustomMessage::Entry entry = {
                                .textboxType = 2,
                                .icon = Rando::StaticData::GetIconForZMessage(RI_NONE),
                                .msg = std::string("You found ") +
                                       Rando::Foreign::ForeignArticleForCheck(checkId) +
                                       (foreignName != nullptr ? foreignName : "a foreign item") + "!",
                            };
                            if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                                CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                            } else {
                                CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
                            }

                            randoSaveCheck.cycleObtained = true;
                            randoSaveCheck.obtained = true;
                            randoSaveCheck.eligible = false;
                            queued = false;
                            // Post-give, CUSTOM_ITEM_PARAM carries an RI for
                            // the draw path (matching the normal branch).
                            CUSTOM_ITEM_PARAM = RI_NONE;
                        },
                    .drawItem =
                        [](Actor* actor, PlayState* play) {
                            // Byte-for-byte the native branch's draw, with the
                            // model-less item. No actor argument: passing it
                            // adds hilites the native path does not, which would
                            // itself be a cross-game tell.
                            MM_Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                            Rando::DrawItem(RI_NONE);
                        } });
                return;
            }
#endif

            MM_GameEvents_Queue().emplace_back(GIEventGiveItem{
                .showGetItemCutscene =
                    Rando::StaticData::ShouldShowGetItemCutscene(ConvertItem(randoSaveCheck.randoItemId, randoCheckId)),
                .param = (int16_t)randoCheckId,
                .giveItem =
                    [](Actor* actor, PlayState* play) {
                        auto& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                        RandoItemId randoItemId =
                            Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)CUSTOM_ITEM_PARAM);
                        std::string prefix = "You found";
                        std::string message = Rando::StaticData::GetItemName(randoItemId);

                        if (randoItemId == RI_JUNK) {
                            randoItemId = Rando::CurrentJunkItem();
                        }
                        if (randoItemId == RI_TRIFORCE_PIECE) {
                            if (gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces + 1 >=
                                RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED]) {
                                prefix = "You";
                                message = "completed the Triforce";
                            }
                            randoItemId = RI_TRIFORCE_PIECE_PREVIOUS;
                        }

                        if (randoItemId == RI_TRAP) {
                            prefix = "";
                            message = GetTrapMessage();
                            // We need to remove the Color Codes if the player is skipping Item Get Cutscenes as the
                            // Notification Emit doesnt support it.
                            if (CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 0) >= 2) {
                                message = CustomMessage::RemoveColorCodes(message);
                            }
                        }

                        CustomMessage::Entry entry = {
                            .textboxType = 2,
                            .icon = Rando::StaticData::GetIconForZMessage(randoItemId),
                            .msg = (prefix == "" ? "" : prefix + " ") + message + (randoItemId == RI_TRAP ? "" : "!"),
                        };

                        if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                            CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                        } else if (Rando::StaticData::ShouldShowGetItemCutscene(randoItemId)) {
                            CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
                        } else {
                            if (Rando::StaticData::Items[randoItemId].randoItemType != RITYPE_JUNK) {
                                Notification::Emit({
                                    .itemIcon = Rando::StaticData::GetIconTexturePath(randoItemId),
                                    .message = prefix,
                                    .suffix = message,
                                });
                            }
                        }
                        Rando::GiveItem(randoItemId);
                        randoSaveCheck.cycleObtained = true;
                        randoSaveCheck.obtained = true;
                        randoSaveCheck.eligible = false;
                        queued = false;
                        CUSTOM_ITEM_PARAM = randoItemId;
                    },
                .drawItem =
                    [](Actor* actor, PlayState* play) {
                        RandoItemId randoItemId;

                        // If the item has been given, the CUSTOM_ITEM_PARAM is set to the RI, prior to that it's the RC
                        if (CUSTOM_ITEM_FLAGS & CustomItem::CALLED_ACTION) {
                            if ((RandoItemId)CUSTOM_ITEM_PARAM == RI_TRAP) {
                                randoItemId = RI_MAX_TRAP;
                            } else {
                                randoItemId = (RandoItemId)CUSTOM_ITEM_PARAM;
                            }
                        } else {
                            auto& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                            randoItemId =
                                Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)CUSTOM_ITEM_PARAM);
                        }

                        MM_Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                        Rando::DrawItem(randoItemId);
                    } });
            return;
        }
    }
}

void Rando::MiscBehavior::CheckQueueReset() {
    queued = false;
    MM_GameEvents_Current() = GIEventNone{};
    MM_GameEvents_Queue().clear();
}
