#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/ShipUtils.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "Rando/Foreign.h" // ForeignNameForCheck: a crossing host's hint (#575 item 2)
#include <cstdio>          // snprintf in the test bridge below
#endif

#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"

#include "overlays/actors/ovl_En_Gs/z_en_gs.h"
}

#define FIRST_GS_MESSAGE 0x20D1
#define SECOND_GS_MESSAGE 0x20C0

std::vector<std::string> flavorText = {
    "Good luck on your journey ...",
    "I hope you find what you're looking for ...",
    "... Evil is afoot",
    "Beware the moon's gaze",
    " .. It's dangerous to go alone",
};

s32 GetNormalizedCost() {
    s32 obtainedChecks = 0;
    s32 maxChecks = 0;
    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        if (saveCheck.shuffled) {
            maxChecks++;
            if (saveCheck.obtained) {
                obtainedChecks++;
            }
        }
    }

    return MAX(10, MIN(250, 10 + (obtainedChecks * (250 - 10)) / (maxChecks)));
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #575 item 2: which checks a gossip stone may hint, and the item name it says
// for one. The stone picks among these when it is READ (GetRandomCheck below),
// not at generation, so nothing here feeds a generated world. Upstream's
// inline filter stays under GetRandomCheck's #else; the item-name call sites sit
// inside COND_ID_HOOK macro arguments, where a preprocessor directive is
// ill-formed (MSVC C2121), so they call GossipHintItemName in both builds and
// the #else below defines it as upstream's exact expression.
// excludeObtained: the purchasable-hint path skips checks already collected.
static std::vector<RandoCheckId> GossipHintCandidates(bool excludeObtained) {
    std::vector<RandoCheckId> availableChecks;
    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        // A crossing host holds MM's junk cover (the item itself is the other
        // game's, in the crossing store), so the junk filter alone would never
        // hint it. Every crossing item is progression (the single-bag composer
        // puts only progression in the bag), so a host is always worth a hint.
        bool hintable = Rando::StaticData::Items[saveCheck.randoItemId].randoItemType != RITYPE_JUNK ||
                        Rando::Foreign::ForeignNameForCheck(randoCheckId) != nullptr;
        if (saveCheck.shuffled && hintable && (!excludeObtained || !saveCheck.obtained)) {
            availableChecks.push_back(randoCheckId);
        }
    }
    return availableChecks;
}

static std::string GossipHintItemName(RandoCheckId randoCheckId) {
    // Name the crossed item, never the cover, with its article like a native
    // one and its game after it: an MM host only ever holds an OoT item, and
    // many names exist in both games ("the Lens of Truth"). " (OoT)" is the
    // marker the check trackers use and OoTMM's in-game text uses.
    if (const char* foreignName = Rando::Foreign::ForeignNameForCheck(randoCheckId)) {
        return std::string(Rando::Foreign::ForeignArticleForCheck(randoCheckId)) + foreignName + " (OoT)";
    }
    return Rando::StaticData::GetItemName(RANDO_SAVE_CHECKS[randoCheckId].randoItemId);
}
#else
static std::string GossipHintItemName(RandoCheckId randoCheckId) {
    return Rando::StaticData::GetItemName(RANDO_SAVE_CHECKS[randoCheckId].randoItemId);
}
#endif // RSBS_SINGLE_EXECUTABLE

RandoCheckId GetRandomCheck(bool repeatableOnlyObtained = false) {
    Player* player = GET_PLAYER(MM_gPlayState);
    if (player->talkActor == nullptr || player->talkActor->id != ACTOR_EN_GS) {
        return RC_UNKNOWN;
    }
    EnGs* enGs = (EnGs*)player->talkActor;

#ifdef RSBS_SINGLE_EXECUTABLE
    std::vector<RandoCheckId> availableChecks = GossipHintCandidates(repeatableOnlyObtained);
#else
    std::vector<RandoCheckId> availableChecks;
    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        if (saveCheck.shuffled && Rando::StaticData::Items[saveCheck.randoItemId].randoItemType != RITYPE_JUNK &&
            (!repeatableOnlyObtained || !saveCheck.obtained)) {
            availableChecks.push_back(randoCheckId);
        }
    }
#endif

    if (availableChecks.empty()) {
        return RC_UNKNOWN;
    }

    if (repeatableOnlyObtained) {
        Ship_Random_Seed(MM_gGameState->frames);
    } else {
        uint32_t seed = MM_gPlayState->sceneId + enGs->actor.home.pos.x + enGs->actor.home.pos.z;
        Ship_Random_Seed(gSaveContext.save.shipSaveInfo.rando.finalSeed + seed);
    }
    return availableChecks[Ship_Random(0, availableChecks.size() - 1)];
}

void Rando::ActorBehavior::InitEnGsBehavior() {
    bool shouldRegister =
        IS_RANDO && (RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONES] || RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]);

    COND_VB_SHOULD(VB_GS_CONSIDER_MASK_OF_TRUTH_EQUIPPED, shouldRegister, { *should = true; });

    // Override the message ID so that we can control the text
    COND_VB_SHOULD(VB_GS_CONTINUE_TEXTBOX, shouldRegister, {
        *should = false;
        MM_Message_ContinueTextbox(MM_gPlayState, SECOND_GS_MESSAGE);
    });

    COND_ID_HOOK(OnOpenText, FIRST_GS_MESSAGE, shouldRegister, [](u16* textId, bool* loadFromMessageTable) {
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

        if (RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONES]) {
            RandoCheckId randoCheckId = GetRandomCheck();
            if (randoCheckId == RC_UNKNOWN) {
                return;
            }

            entry.autoFormat = false;

            entry.msg = "They say %g{{item}}%w is hidden at %y{{location}}%w.";

            CustomMessage::Replace(&entry.msg, "{{item}}", GossipHintItemName(randoCheckId));
            CustomMessage::Replace(&entry.msg, "{{location}}",
                                   Ship_GetSceneName(Rando::StaticData::Checks[randoCheckId].sceneId));

            // Replace colors before line break calculation
            CustomMessage::ReplaceColorChars(&entry.msg);

            CustomMessage::AddLineBreaks(&entry.msg);

            if (RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]) {
                entry.msg += "\x10...\x13\x12";
            }
        } else {
            entry.msg = "";
        }

        if (RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]) {
            entry.msg += "Trade %r{{rupees}} Rupees%w for a hint?\x02\x11\xC2No\x11Yes";
            s32 cost = GetNormalizedCost();
            CustomMessage::Replace(&entry.msg, "{{rupees}}", std::to_string(cost));

            CustomMessage::ReplaceColorChars(&entry.msg);
        }

        CustomMessage::EnsureMessageEnd(&entry.msg);

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    COND_ID_HOOK(OnOpenText, SECOND_GS_MESSAGE, shouldRegister, [](u16* textId, bool* loadFromMessageTable) {
        MessageContext* msgCtx = &MM_gPlayState->msgCtx;
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

        if (RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]) {
            if (msgCtx->choiceIndex == 1) {
                s32 cost = GetNormalizedCost();

                RandoCheckId randoCheckId = GetRandomCheck(true);
                if (gSaveContext.save.saveInfo.playerData.rupees < cost) {
                    entry.msg = "Foolish... You don't have enough rupees...";
                } else if (randoCheckId == RC_UNKNOWN) {
                    entry.msg = "I have no more hints for you...";
                } else {
                    entry.msg = "Wise choice... They say %g{{item}}%w is hidden at %y{{location}}%w.";

                    CustomMessage::Replace(&entry.msg, "{{item}}", GossipHintItemName(randoCheckId));
                    CustomMessage::Replace(&entry.msg, "{{location}}",
                                           Ship_GetSceneName(Rando::StaticData::Checks[randoCheckId].sceneId));

                    gSaveContext.rupeeAccumulator -= cost;
                    cost *= 2;
                }
            } else {
                entry.msg = "Foolish... Come back later when you have more sense.";
            }
        } else {
            entry.msg = flavorText[Ship_Random(0, flavorText.size() - 1)];
        }

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Four Gossip Stone Grottos Heart Piece item grant behavior override
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* refActor = va_arg(args, Actor*);
        Player* player = GET_PLAYER(MM_gPlayState);

        if (refActor->id != ACTOR_EN_GS || *item != GI_HEART_PIECE) {
            return;
        }

        *should = false;

        refActor->parent = &player->actor;
    });
}

#ifdef RSBS_SINGLE_EXECUTABLE
// ROM-free test bridge (redship tier; src/common/tests/test_foreign_items.c,
// row foreign-host-gossip-hint, #575 item 2). Drives the SAME candidate filter
// and item-name substitution the two gossip-stone text hooks above use.

/** 1 when `randoCheckId` is in the stone's candidate list. */
extern "C" int MM_Rando_Hints_TestGossipCandidate(uint16_t randoCheckId, int excludeObtained) {
    for (RandoCheckId candidate : GossipHintCandidates(excludeObtained != 0)) {
        if (candidate == (RandoCheckId)randoCheckId) {
            return 1;
        }
    }
    return 0;
}

/** The item name a stone would say for `randoCheckId`, copied into `out`.
 *  Returns the full length, or -1 for an id that is not a real check. */
extern "C" int MM_Rando_Hints_TestGossipItemName(uint16_t randoCheckId, char* out, int cap) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX || out == nullptr || cap <= 0) {
        return -1;
    }
    const std::string name = GossipHintItemName((RandoCheckId)randoCheckId);
    snprintf(out, (size_t)cap, "%s", name.c_str());
    return (int)name.size();
}

/** Write one check's save row (item, shuffled, obtained) as MM's fill would,
 *  reporting the prior values so the lock can put them back. */
extern "C" void MM_Rando_Hints_TestSetCheck(uint16_t randoCheckId, uint16_t randoItemId, int shuffled, int obtained,
                                            uint16_t* priorItemId, int* priorShuffled, int* priorObtained) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX) {
        return;
    }
    RandoSaveCheck& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
    if (priorItemId != nullptr) {
        *priorItemId = (uint16_t)saveCheck.randoItemId;
    }
    if (priorShuffled != nullptr) {
        *priorShuffled = saveCheck.shuffled ? 1 : 0;
    }
    if (priorObtained != nullptr) {
        *priorObtained = saveCheck.obtained ? 1 : 0;
    }
    saveCheck.randoItemId = (RandoItemId)randoItemId;
    saveCheck.shuffled = (shuffled != 0);
    saveCheck.obtained = (obtained != 0);
}
#endif // RSBS_SINGLE_EXECUTABLE
