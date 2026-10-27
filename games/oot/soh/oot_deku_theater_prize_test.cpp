/**
 * @file oot_deku_theater_prize_test.cpp
 * ROM-free, display-free lock for #1217: in a randomizer file the Deku Theater
 * leader (ovl_En_Dnt_Jiji) gives the seed's item for the theater check and no
 * vanilla Deku Stick or Deku Nut capacity upgrade. CTest label "redship", row
 * OoTDekuTheaterPrize in CMake/SingleExecutable.cmake, dispatch
 * "oot-deku-theater-prize" in src/common/test_runner.cpp.
 *
 * HOW THE LEADER IS REACHED. The randomizer's timesaver
 * (EnDntDemo_JudgeSkipToReward, timesaver_hook_handlers.cpp) sets the prize's
 * ItemGetInf directly, but it hands over to the vanilla judge when Shuffle
 * Jabber Nuts (RSK_SHUFFLE_SPEAK) is on. The vanilla judge arms the prize and
 * the leader walks up (Skull Mask) or is talked to (Mask of Truth), then
 * EnDntJiji_Talk offers this->getItemId, a vanilla capacity upgrade. Its
 * EnDntJiji_GivePrize sets the ItemGetInf, which RandomizerOnFlagSetHandler maps
 * to the theater check and queues: the player got both.
 *
 * WHAT IT DRIVES. The leader's REAL EnDntJiji_Talk on the frame the player
 * closes his textbox (TEXT_STATE_EVENT and A), then, if the leader moved on to
 * EnDntJiji_GivePrize, that REAL action for one frame. The randomizer's REAL
 * vanilla-behaviour and flag-set handlers are registered on the GameInteractor
 * exactly as RandomizerRegisterHooks registers them on a randomizer file load
 * (hook_handlers.cpp), and the leader reaches them through the real
 * GameInteractor_Should / Flags_SetItemGetInf dispatch.
 *
 * THE LEGS (Shuffle Jabber Nuts on, both bag tiers 0, as under bag shuffle).
 *   (a) Skull Mask: the leader offers nothing (player->getItemId stays GI_NONE),
 *       goes straight to giving the prize, sets
 *       ITEMGETINF_OBTAINED_STICK_UPGRADE_FROM_STAGE, and exactly
 *       RC_DEKU_THEATER_SKULL_MASK is queued. The stick tier is still 0.
 *   (b) Mask of Truth: the same with ITEMGETINF_OBTAINED_NUT_UPGRADE_FROM_STAGE
 *       and RC_DEKU_THEATER_MASK_OF_TRUTH; the nut tier is still 0.
 *   (c) control, a vanilla file (no randomizer hooks, as a vanilla load leaves
 *       them): the same frame offers GI_STICK_UPGRADE_30 and waits for the
 *       player to take it. This proves the stub play state reaches the offer, so
 *       leg (a)'s "no offer" is not vacuous.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "game.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

extern "C" {
#include "src/overlays/actors/ovl_En_Dnt_Jiji/z_en_dnt_jiji.h"
extern SaveContext gSaveContext;
// z_en_dnt_jiji.c's actions (external linkage there).
void EnDntJiji_Talk(EnDntJiji* thisx, PlayState* play);
void EnDntJiji_SetupGivePrize(EnDntJiji* thisx, PlayState* play);
void EnDntJiji_GivePrize(EnDntJiji* thisx, PlayState* play);
// hook_handlers.cpp: the test bridge that empties the check queue the flag-set
// handler feeds.
int OoT_Rando_TestTakeQueuedChecks(uint16_t* out, int cap);
}

// hook_handlers.cpp: the two handlers RandomizerRegisterHooks registers on a
// randomizer file load.
void RandomizerOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs);
void RandomizerOnFlagSetHandler(int16_t flagType, int16_t flag);

namespace {

int sFailures = 0;

void DtFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-deku-theater-prize: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define DT_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            DtFail(__VA_ARGS__); \
        }                        \
    } while (0)

// The leader's skeleton is never loaded here: its per-frame update is a no-op.
s32 NoSkelUpdate(SkelAnime* skelAnime) {
    (void)skelAnime;
    return 0;
}

// Player_SetCsActionWithHaltedActors's partner in play (Player_Init sets it).
void NoHaltActors(Player* player, PlayState* play) {
    (void)player;
    (void)play;
}

std::shared_ptr<Rando::Context> sWorld;

bool AuthorWorld(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    sWorld = Rando::Context::CreateInstance();
    if (sWorld == nullptr) {
        return false;
    }
    sWorld->SetSeed(0x1217);
    if (Rando::StaticData::GetLocation(RC_DEKU_THEATER_SKULL_MASK)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    if (Rando::StaticData::RetrieveItem(RG_PIECE_OF_HEART).GetName().GetEnglish().empty()) {
        Rando::StaticData::InitItemTable();
    }
    sWorld->GetOption(RSK_SHUFFLE_SPEAK).Set(1);
    for (RandomizerCheck rc : { RC_DEKU_THEATER_SKULL_MASK, RC_DEKU_THEATER_MASK_OF_TRUTH }) {
        sWorld->GetItemLocation(rc)->SetPlacedItem(RG_PIECE_OF_HEART);
        sWorld->GetItemLocation(rc)->SetCheckStatus(RCSHOW_UNCHECKED);
    }
    return true;
}

void ReleaseWorld(void) {
    // Context and its Logic hold each other by shared_ptr; cut the back edge
    // first (oot_check_state_load_test.cpp).
    if (sWorld != nullptr) {
        sWorld->GetLogic()->SetContext(nullptr);
    }
    sWorld.reset();
}

struct Stage {
    PlayState* play;
    Player* player;
    EnDntJiji* jiji;
};

// The frame the leader's prize textbox (0x104D, or 0x10DB for the nut) is closed
// with A, the player standing next to him, on file @p questId.
void ArmFrame(Stage& s, u8 questId, s32 getItemId) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = questId;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    // Both bag tiers 0: the bag-shuffle start (logic.cpp), where a vanilla
    // capacity upgrade would hand over the bag the seed placed elsewhere.

    memset(s.play, 0, sizeof(PlayState));
    memset(s.player, 0, sizeof(Player));
    memset(s.jiji, 0, sizeof(EnDntJiji));

    PlayState* play = s.play;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &s.player->actor;
    play->mainCamera.play = play;
    play->mainCamera.thisIdx = MAIN_CAM;
    play->cameraPtrs[MAIN_CAM] = &play->mainCamera;
    play->activeCamera = MAIN_CAM;
    play->func_11D54 = NoHaltActors;
    play->msgCtx.msgLength = 1;
    play->msgCtx.msgMode = MSGMODE_TEXT_DONE;
    play->msgCtx.textboxEndType = TEXTBOX_ENDTYPE_EVENT;
    play->state.input[0].press.button = BTN_A;

    s.player->actor.id = ACTOR_PLAYER;
    s.player->getItemId = GI_NONE;

    EnDntJiji* jiji = s.jiji;
    jiji->actor.id = ACTOR_EN_DNT_JIJI;
    jiji->actor.xzDistToPlayer = 50.0f;
    jiji->actor.yDistToPlayer = 0.0f;
    jiji->skelAnime.update.normal = NoSkelUpdate;
    jiji->getItemId = getItemId;
    jiji->stage = NULL;
    jiji->actionFunc = EnDntJiji_Talk;
}

struct Outcome {
    s16 offered;       // player->getItemId after the talk frame
    bool wentToGive;   // the leader moved straight to EnDntJiji_GivePrize
    bool waitsForTake; // the leader moved to EnDntJiji_SetupGivePrize (vanilla)
    bool flagSet;      // the prize's ItemGetInf after the give frame
    s16 offeredAfter;  // player->getItemId after the give frame
    int queued;        // checks queued by the flag set
    uint16_t firstQueued;
    u8 stickTier;
    u8 nutTier;
};

Outcome RunPrize(Stage& s, s32 itemGetInf) {
    Outcome out = {};
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);

    EnDntJiji_Talk(s.jiji, s.play);
    out.offered = s.player->getItemId;
    out.wentToGive = s.jiji->actionFunc == EnDntJiji_GivePrize;
    out.waitsForTake = s.jiji->actionFunc == EnDntJiji_SetupGivePrize;

    if (out.wentToGive) {
        EnDntJiji_GivePrize(s.jiji, s.play);
    }
    out.offeredAfter = s.player->getItemId;
    out.flagSet = Flags_GetItemGetInf(itemGetInf) != 0;
    uint16_t queued[4] = {};
    out.queued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
    out.firstQueued = queued[0];
    out.stickTier = (u8)CUR_UPG_VALUE(UPG_STICKS);
    out.nutTier = (u8)CUR_UPG_VALUE(UPG_NUTS);
    return out;
}

void PrintOutcome(const char* leg, const Outcome& o) {
    printf("[TEST] oot-deku-theater-prize: %s: offered 0x%02X (after give 0x%02X), straight to give %d, waits for "
           "take %d, flag %d, queued %d (first %u), stick tier %u, nut tier %u\n",
           leg, (unsigned)(u16)o.offered, (unsigned)(u16)o.offeredAfter, o.wentToGive ? 1 : 0, o.waitsForTake ? 1 : 0,
           o.flagSet ? 1 : 0, o.queued, (unsigned)o.firstQueued, (unsigned)o.stickTier, (unsigned)o.nutTier);
}

void RandoLeg(Stage& s, const char* leg, s32 getItemId, s32 itemGetInf, RandomizerCheck rc) {
    ArmFrame(s, QUEST_RANDOMIZER, getItemId);
    const Outcome o = RunPrize(s, itemGetInf);
    PrintOutcome(leg, o);
    DT_CHECK(o.offered == GI_NONE && o.offeredAfter == GI_NONE,
             "%s: the leader offered the vanilla capacity upgrade 0x%02X in a randomizer file (#1217)", leg,
             (unsigned)(u16)(o.offered != GI_NONE ? o.offered : o.offeredAfter));
    DT_CHECK(o.wentToGive, "%s: the leader did not go straight to giving the prize", leg);
    DT_CHECK(o.flagSet, "%s: the prize's ItemGetInf was not set (the theater check would never be given)", leg);
    DT_CHECK(o.queued == 1 && o.firstQueued == (uint16_t)rc,
             "%s: the flag set queued %d check(s), first %u; want exactly the theater check %u", leg, o.queued,
             (unsigned)o.firstQueued, (unsigned)rc);
    DT_CHECK(o.stickTier == 0 && o.nutTier == 0, "%s: a bag tier moved (stick %u, nut %u)", leg, (unsigned)o.stickTier,
             (unsigned)o.nutTier);
}

int RunLegs(void) {
    Stage s = {};
    s.play = (PlayState*)calloc(1, sizeof(PlayState));
    s.player = (Player*)calloc(1, sizeof(Player));
    s.jiji = (EnDntJiji*)calloc(1, sizeof(EnDntJiji));
    if (s.play == nullptr || s.player == nullptr || s.jiji == nullptr) {
        DtFail("could not allocate the stub play state");
        free(s.play);
        free(s.player);
        free(s.jiji);
        return 1;
    }

    DT_CHECK(sWorld->GetOption(RSK_SHUFFLE_SPEAK).Get() != 0, "setup: Shuffle Jabber Nuts is not on");

    // The randomizer's handlers, as RandomizerRegisterHooks registers them on a
    // randomizer file load.
    const HOOK_ID vbHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(
        RandomizerOnVanillaBehaviorHandler);
    const HOOK_ID flagHook =
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFlagSet>(RandomizerOnFlagSetHandler);

    // (a) Skull Mask: Walk set GI_STICK_UPGRADE_30 at stick tier 0.
    RandoLeg(s, "(a) Skull Mask", GI_STICK_UPGRADE_30, ITEMGETINF_OBTAINED_STICK_UPGRADE_FROM_STAGE,
             RC_DEKU_THEATER_SKULL_MASK);
    // (b) Mask of Truth: Cower set GI_NUT_UPGRADE_30 at nut tier 0.
    RandoLeg(s, "(b) Mask of Truth", GI_NUT_UPGRADE_30, ITEMGETINF_OBTAINED_NUT_UPGRADE_FROM_STAGE,
             RC_DEKU_THEATER_MASK_OF_TRUTH);

    // (c) control: a vanilla file, where a load leaves no randomizer hook.
    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnVanillaBehavior>(vbHook);
    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnFlagSet>(flagHook);
    ArmFrame(s, QUEST_NORMAL, GI_STICK_UPGRADE_30);
    const Outcome c = RunPrize(s, ITEMGETINF_OBTAINED_STICK_UPGRADE_FROM_STAGE);
    PrintOutcome("(c) vanilla control", c);
    DT_CHECK(c.offered == GI_STICK_UPGRADE_30,
             "(c): the vanilla leader did not offer the stick upgrade (the stub frame does not reach the offer, so "
             "legs (a) and (b) prove nothing)");
    DT_CHECK(c.waitsForTake, "(c): the vanilla leader did not wait for the player to take the prize");
    DT_CHECK(!c.flagSet && c.queued == 0, "(c): the vanilla leader set the flag before the prize was taken");

    free(s.play);
    free(s.player);
    free(s.jiji);
    return 0;
}

} // namespace

extern "C" int OoT_DekuTheaterPrize_RunHeadless(void) {
    printf("[TEST] oot-deku-theater-prize: under Shuffle Jabber Nuts the Deku Theater leader gives the seed's item "
           "and no vanilla capacity upgrade (#1217)\n");
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);

    int setup = 1;
    if (!AuthorWorld()) {
        printf("[TEST] FAIL (oot-deku-theater-prize): could not create a Rando::Context\n");
    } else {
        setup = RunLegs();
    }

    // Leave process state clean for whatever runs next, on pass or fail.
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    memset(&gSaveContext, 0, sizeof(SaveContext));
    ReleaseWorld();
    Context_SetCurrentGame(prevGame);

    if (setup != 0 || sFailures != 0) {
        printf("[TEST] FAIL (oot-deku-theater-prize): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-deku-theater-prize\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
