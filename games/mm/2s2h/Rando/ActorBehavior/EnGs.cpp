#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/ShipUtils.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Rando/HintArea.h" // #575 H2: a hint names the check's area (scene name outside single-exe)
#ifdef RSBS_SINGLE_EXECUTABLE
#include "Rando/Foreign.h" // ForeignNameForCheck: a crossing host's hint (#575 item 2)
#include "game.h"          // src/common: GAME_MM / GAME_OOT, a candidate's host game (#575 H3c)
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
//
// #575 H3c: a candidate is {hostGame, check}: one of MM's own checks, or an OoT
// check that hosts an MM item (the crossing store's OoT-hosted rows). It
// converts from and compares with a RandoCheckId as an MM check, so the text
// hooks below read the same in both builds (the #else makes it RandoCheckId).
struct GossipHintTarget {
    uint8_t hostGame;
    uint16_t check;
    GossipHintTarget(RandoCheckId randoCheckId) : hostGame((uint8_t)GAME_MM), check((uint16_t)randoCheckId) {
    }
    GossipHintTarget(uint8_t game, uint16_t hostCheck) : hostGame(game), check(hostCheck) {
    }
    bool operator==(RandoCheckId randoCheckId) const {
        return hostGame == (uint8_t)GAME_MM && check == (uint16_t)randoCheckId;
    }
};

static std::vector<GossipHintTarget> GossipHintCandidates(bool excludeObtained) {
    std::vector<GossipHintTarget> availableChecks;
    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        // A crossing host holds MM's junk cover (the item itself is the other
        // game's, in the crossing store), so the junk filter alone would never
        // hint it. Every crossing item is progression (the single-bag composer
        // puts only progression in the bag), so a host is always worth a hint.
        bool hintable = Rando::StaticData::Items[saveCheck.randoItemId].randoItemType != RITYPE_JUNK ||
                        Rando::Foreign::ForeignNameForCheck(randoCheckId) != nullptr;
        // #575 H2: the stone names the check's area, so a check with none (the
        // 55 enemy drops) has nowhere to name: OoTMM's hints skip region-less
        // checks the same way.
        hintable = hintable && Rando::HintArea::HasArea(randoCheckId);
        if (saveCheck.shuffled && hintable && (!excludeObtained || !saveCheck.obtained)) {
            availableChecks.push_back(randoCheckId);
        }
    }
    // #575 H3c: then the MM items hosted in OoT, in the crossing store's
    // insertion order, so the list stays a pure function of the frozen world.
    // Every crossing item is progression, so each is worth a hint.
    for (uint16_t ootHost : Rando::HintArea::OoTHostedHintCandidates(excludeObtained)) {
        availableChecks.push_back(GossipHintTarget((uint8_t)GAME_OOT, ootHost));
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

// #575 H2: what a stone says for a check, before colours and line breaks: the
// free stone's text (purchased == false) or the purchased hint's. Both text
// hooks below call it, and so does the test bridge, so the lock reads the very
// string the player is shown. The check is named by its area in the shared
// vocabulary, with the area's OoTMM preposition (Rando/HintArea.h).
static std::string GossipHintText(const GossipHintTarget& target, bool purchased) {
    std::string msg = purchased ? "Wise choice... They say %g{{item}}%w is hidden {{prepos}} %y{{location}}%w."
                                : "They say %g{{item}}%w is hidden {{prepos}} %y{{location}}%w.";
    if (target.hostGame == (uint8_t)GAME_OOT) {
        // #575 H3c: an MM item on an OoT check: MM's own item, so its own name
        // and no game marker, at the OoT check's area.
        CustomMessage::Replace(&msg, "{{item}}",
                               Rando::StaticData::GetItemName(Rando::HintArea::ItemOnOoTHost(target.check)));
        Rando::HintArea::ReplaceOoTHostLocation(&msg, "{{location}}", target.check);
        return msg;
    }
    const RandoCheckId randoCheckId = (RandoCheckId)target.check;
    CustomMessage::Replace(&msg, "{{item}}", GossipHintItemName(randoCheckId));
    Rando::HintArea::ReplaceLocation(&msg, "{{location}}", randoCheckId);
    return msg;
}
#else
using GossipHintTarget = RandoCheckId;

static std::string GossipHintItemName(RandoCheckId randoCheckId) {
    return Rando::StaticData::GetItemName(RANDO_SAVE_CHECKS[randoCheckId].randoItemId);
}

static std::string GossipHintText(RandoCheckId randoCheckId, bool purchased) {
    std::string msg = purchased ? "Wise choice... They say %g{{item}}%w is hidden at %y{{location}}%w."
                                : "They say %g{{item}}%w is hidden at %y{{location}}%w.";
    CustomMessage::Replace(&msg, "{{item}}", GossipHintItemName(randoCheckId));
    CustomMessage::Replace(&msg, "{{location}}", Ship_GetSceneName(Rando::StaticData::Checks[randoCheckId].sceneId));
    return msg;
}
#endif // RSBS_SINGLE_EXECUTABLE

GossipHintTarget GetRandomCheck(bool repeatableOnlyObtained = false) {
    Player* player = GET_PLAYER(MM_gPlayState);
    if (player->talkActor == nullptr || player->talkActor->id != ACTOR_EN_GS) {
        return RC_UNKNOWN;
    }
    EnGs* enGs = (EnGs*)player->talkActor;

#ifdef RSBS_SINGLE_EXECUTABLE
    std::vector<GossipHintTarget> availableChecks = GossipHintCandidates(repeatableOnlyObtained);
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

#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/HintFoolish.h" // #575 H5a: the free stone's foolish variant

// #575 H5a: what the FREE stone says, given its item pick and its stone seed:
// the foolish variant one time in Rando::HintFoolish::kFreeStoneOneIn when the
// world has a foolish MM area (HintFoolishSingleExe.cpp), else the item hint
// exactly as before. The text hook below calls FreeStoneHintText in both builds
// (a directive is ill-formed inside its COND_ID_HOOK argument); the #else keeps
// upstream's expression.
static std::string FreeStoneHintTextSeeded(const GossipHintTarget& target, uint32_t stoneSeed) {
    const std::string foolish = Rando::HintFoolish::FreeStoneText(stoneSeed);
    return foolish.empty() ? GossipHintText(target, false) : foolish;
}

// The stone seed is GetRandomCheck's own for the free stone (the same expression,
// read off the same stone), so the variant is a pure function of the frozen world
// and the stone, like the item pick.
static std::string FreeStoneHintText(const GossipHintTarget& target) {
    Player* player = GET_PLAYER(MM_gPlayState);
    if (player->talkActor == nullptr || player->talkActor->id != ACTOR_EN_GS) {
        return GossipHintText(target, false);
    }
    EnGs* enGs = (EnGs*)player->talkActor;
    uint32_t seed = MM_gPlayState->sceneId + enGs->actor.home.pos.x + enGs->actor.home.pos.z;
    return FreeStoneHintTextSeeded(target, gSaveContext.save.shipSaveInfo.rando.finalSeed + seed);
}

/** TEST BRIDGE (redship tier; src/common/tests/test_foolish_hints.c, #575 H5a):
 *  the text the FREE stone's hook shows when its item pick is `randoCheckId` and
 *  the stone it is reading seeds `stoneSeed`, before colours and line breaks.
 *  Returns the full length, or -1 for an id that is not a real check. */
extern "C" int MM_Rando_Hints_TestFreeStoneText(uint16_t randoCheckId, uint32_t stoneSeed, char* out, int cap) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX || out == nullptr || cap <= 0) {
        return -1;
    }
    const std::string msg = FreeStoneHintTextSeeded((RandoCheckId)randoCheckId, stoneSeed);
    snprintf(out, (size_t)cap, "%s", msg.c_str());
    return (int)msg.size();
}
#else
static std::string FreeStoneHintText(GossipHintTarget randoCheckId) {
    return GossipHintText(randoCheckId, false);
}
#endif // RSBS_SINGLE_EXECUTABLE

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
            GossipHintTarget randoCheckId = GetRandomCheck();
            if (randoCheckId == RC_UNKNOWN) {
                return;
            }

            entry.autoFormat = false;

            entry.msg = FreeStoneHintText(randoCheckId);

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

                GossipHintTarget randoCheckId = GetRandomCheck(true);
                if (gSaveContext.save.saveInfo.playerData.rupees < cost) {
                    entry.msg = "Foolish... You don't have enough rupees...";
                } else if (randoCheckId == RC_UNKNOWN) {
                    entry.msg = "I have no more hints for you...";
                } else {
                    entry.msg = GossipHintText(randoCheckId, true);

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
    for (const GossipHintTarget& candidate : GossipHintCandidates(excludeObtained != 0)) {
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

/** The text a stone says for `randoCheckId` before colours and line breaks:
 *  the free stone's (purchased == 0) or the purchased hint's (purchased != 0).
 *  It is GossipHintText's own result, the function both text hooks above call.
 *  Returns the full length, or -1 for an id that is not a real check. */
extern "C" int MM_Rando_Hints_TestGossipText(uint16_t randoCheckId, int purchased, char* out, int cap) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX || out == nullptr || cap <= 0) {
        return -1;
    }
    const std::string msg = GossipHintText((RandoCheckId)randoCheckId, purchased != 0);
    snprintf(out, (size_t)cap, "%s", msg.c_str());
    return (int)msg.size();
}

/** The scene name upstream's hint said for `randoCheckId` (so the lock can find
 *  the grotto rows by what they used to read). Returns the full length, or -1. */
extern "C" int MM_Rando_Hints_TestSceneName(uint16_t randoCheckId, char* out, int cap) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX || out == nullptr || cap <= 0) {
        return -1;
    }
    const std::string name = Ship_GetSceneName(Rando::StaticData::Checks[(RandoCheckId)randoCheckId].sceneId);
    snprintf(out, (size_t)cap, "%s", name.c_str());
    return (int)name.size();
}

/** #575 H3c: entry `index` of the stone's candidate list, in the order the
 *  stone draws from: its host game and check. Returns the list's length (so
 *  index -1 just counts), or -1 for a null out-parameter. */
extern "C" int MM_Rando_Hints_TestGossipCandidateAt(int excludeObtained, int index, uint8_t* outGame,
                                                     uint16_t* outCheck) {
    if (outGame == nullptr || outCheck == nullptr) {
        return -1;
    }
    const std::vector<GossipHintTarget> candidates = GossipHintCandidates(excludeObtained != 0);
    if (index >= 0 && index < (int)candidates.size()) {
        *outGame = candidates[(size_t)index].hostGame;
        *outCheck = candidates[(size_t)index].check;
    }
    return (int)candidates.size();
}

/** #575 H3c: the text a stone says for a candidate of either host game (an MM
 *  check, or an OoT check hosting an MM item), before colours and line breaks.
 *  Returns the full length, or -1 for a target the stone cannot name. */
extern "C" int MM_Rando_Hints_TestGossipHostText(uint8_t hostGame, uint16_t check, int purchased, char* out,
                                                  int cap) {
    if (hostGame == (uint8_t)GAME_MM) {
        return MM_Rando_Hints_TestGossipText(check, purchased, out, cap);
    }
    if (hostGame != (uint8_t)GAME_OOT || Rando::HintArea::ItemOnOoTHost(check) == RI_UNKNOWN || out == nullptr ||
        cap <= 0) {
        return -1;
    }
    const std::string msg = GossipHintText(GossipHintTarget(hostGame, check), purchased != 0);
    snprintf(out, (size_t)cap, "%s", msg.c_str());
    return (int)msg.size();
}
#endif // RSBS_SINGLE_EXECUTABLE
