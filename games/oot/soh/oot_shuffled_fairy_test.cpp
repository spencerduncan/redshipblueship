/**
 * @file oot_shuffled_fairy_test.cpp
 * Locks for #1289: a randomized fairy (gossip stone, fountain, fairy spot, bean)
 * is an item pickup, not a heal. Rando-tier rows `shuffled-fairy-no-heal` (CTest
 * ShuffledFairyNoHeal) and `shuffled-fairy-bottle-guard` (CTest
 * ShuffledFairyBottleGuard), dispatch in src/common/test_runner.cpp, same
 * bring-up as master-sword-respawn (OTRGlobals' randomizer context).
 *
 * WHAT WAS BROKEN. EnElf's touch handler func_80A0329C
 * (games/oot/src/overlays/actors/ovl_En_Elf/z_en_elf.c) heals 8 hearts behind
 * VB_FAIRY_HEAL and refills magic for a big fairy with no VB at all.
 * ShuffleFairies.cpp's VB_FAIRY_HEAL hook set the check's RandomizerInf (which
 * gives the item) but never answered false, so a shuffled fairy gave its item
 * AND healed, and a shuffled big fairy (Song of Storms stone, fairy spot) also
 * refilled magic. OoTMM's EnElf_GiveItem heals only when the location still
 * holds the fairy itself. Separately (SoH 5e13570b8, #6405), the VB_BOTTLE_ACTOR
 * hook took the fairy's bottle-swipe path for ANY actor carrying a
 * CheckIdentity (pots, crates, grass, beehives, trees), not only EnElf.
 *
 * WHAT IT DRIVES. ShuffleFairies' REAL hooks, registered by the real
 * RegisterShuffleFairies on a randomizer file with Shuffle Gossip Stone Fairies
 * on, reached through the real GameInteractor_Should dispatch: the fairy's REAL
 * touch handler for one frame (Link standing under it, so the touch fires that
 * frame), and the bottle VB with the actor Player_Action hands it.
 *
 * shuffled-fairy-no-heal, Link at one heart of ten and zero magic (magic owned):
 *   (a) FAIRY_HEAL holding RC_TOT_LEFTMOST_GOSSIP_STONE_FAIRY: touched, the
 *       check's RandomizerInf is set, health stays one heart, no magic refill.
 *   (b) FAIRY_HEAL_BIG holding RC_TOT_LEFTMOST_GOSSIP_STONE_FAIRY_BIG: the same,
 *       and no magic refill (magicState never goes to MAGIC_STATE_FILL).
 *   (c) controls, the same hooks registered: a FAIRY_HEAL with no identity (a
 *       vanilla fairy, or one spawned after its check was collected) heals 8
 *       hearts and leaves magic alone; a FAIRY_HEAL_BIG with no identity heals
 *       and starts the magic refill. These prove the stub frame reaches the
 *       heal, so (a) and (b) are not vacuous.
 *
 * shuffled-fairy-bottle-guard:
 *   (a) a clay pot (ACTOR_OBJ_TSUBO) carrying a CheckIdentity: the bottle VB
 *       stays true, its RandomizerInf stays clear and its parent stays NULL.
 *   (b) control: an EnElf carrying a CheckIdentity is bottled as its item (VB
 *       false, RandomizerInf set, parent = Link).
 *
 * The randomizer option, the hook registries the legs dispatch, OoT_gPlayState,
 * gGameInfo and the whole save are put back before returning.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"

#include "context.h"
#include "game.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/OTRGlobals.h"

#include <cstdio>
#include <memory>

extern "C" {
#include "src/overlays/actors/ovl_En_Elf/z_en_elf.h"
extern PlayState* OoT_gPlayState;
// z_en_elf.c: the heal fairy's idle-and-touch action, and the post-touch circle
// it hands over to (both external linkage there).
void func_80A0329C(EnElf* thisx, PlayState* play);
void func_80A03610(EnElf* thisx, PlayState* play);
}

// ShuffleFairies.cpp: the ShipInit registrar ("IS_RANDO") for every fairy hook.
void RegisterShuffleFairies();

namespace {

#define FAIRY_FLAG_TIMED_TEST (1 << 8)
#define FAIRY_FLAG_BIG_TEST (1 << 9)

/** Every registry of one hook type set aside for a scope, so only the hooks a leg
 *  registers itself are dispatched (the bring-up, or an earlier row inside
 *  AllTests, can leave gameplay hooks registered that read a live play state). */
template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForID) forId;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForPtr) forPtr;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    HooksAside() {
        Swap();
    }
    ~HooksAside() {
        Swap();
    }
    void Swap() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        forId.swap(GameInteractor::RegisteredGameHooks<H>::functionsForID);
        forPtr.swap(GameInteractor::RegisteredGameHooks<H>::functionsForPtr);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

// File-static: PlayState and Player are large.
PlayState sPlay;
Player sPlayer;
EnElf sFairy;
GameInfo sGameInfo;

int sFailures = 0;
const char* sRow = "";

#define SF_CHECK(cond, ...)                                             \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("[TEST] FAIL (%s): ", sRow);                    \
            std::printf(__VA_ARGS__);                                   \
            std::printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

// The fairy's skeleton is never loaded here: its per-frame update is a no-op.
s32 NoSkelUpdate(SkelAnime* skelAnime) {
    (void)skelAnime;
    return 0;
}

// The fairy's wander picker (func_2C8) held still, so the frame is deterministic:
// no random dart away from Link and no sparkle effect spawn.
void NoWander(EnElf* thisx, PlayState* play) {
    (void)thisx;
    (void)play;
}

constexpr s16 kOneHeart = 0x10;
constexpr s16 kTenHearts = 0xA0;
constexpr s16 kFairyHeal = 128;

/** A randomizer save, child Link at one heart of ten with an empty,
 *  owned magic meter, standing at the origin of the ToT exterior. */
void ArmSave(void) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.healthCapacity = kTenHearts;
    gSaveContext.health = kOneHeart;
    gSaveContext.isMagicAcquired = true;
    gSaveContext.magicLevel = 1;
    gSaveContext.magic = 0;
    gSaveContext.magicState = MAGIC_STATE_IDLE;

    std::memset(&sPlay, 0, sizeof(PlayState));
    std::memset(&sPlayer, 0, sizeof(Player));
    sPlay.sceneNum = SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY;
    sPlay.actorCtx.actorLists[ACTORCAT_PLAYER].head = &sPlayer.actor;
    sPlayer.actor.id = ACTOR_PLAYER;
    sPlayer.actor.category = ACTORCAT_PLAYER;
    sPlayer.bodyPartsPos[0].y = 30.0f;
}

/** A heal fairy of @p type hovering 30 units over Link, in func_80A0329C, as
 *  OoT_EnElf_Init leaves FAIRY_HEAL / FAIRY_HEAL_BIG. */
void ArmFairy(FairyType type) {
    std::memset(&sFairy, 0, sizeof(EnElf));
    sFairy.actor.id = ACTOR_EN_ELF;
    sFairy.actor.category = ACTORCAT_ITEMACTION;
    sFairy.actor.params = type;
    sFairy.actor.world.pos.y = 30.0f;
    sFairy.actor.scale.x = sFairy.actor.scale.y = sFairy.actor.scale.z = 0.008f;
    if (type == FAIRY_HEAL_BIG) {
        sFairy.fairyFlags |= FAIRY_FLAG_BIG_TEST | FAIRY_FLAG_TIMED_TEST;
    }
    sFairy.skelAnime.update.normal = NoSkelUpdate;
    sFairy.unk_28C = sFairy.actor.world.pos;
    sFairy.unk_2B4 = 10.0f;
    sFairy.unk_2C0 = 100;
    sFairy.func_2C8 = NoWander;
    sFairy.disappearTimer = 240;
    sFairy.actionFunc = func_80A0329C;
}

struct Touch {
    bool touched; // the fairy moved on to its post-touch circle (func_80A03610)
    s16 health;
    s16 magicState;
    s16 magicFillTarget;
    bool flagSet;
};

Touch RunTouch(RandomizerInf inf) {
    func_80A0329C(&sFairy, &sPlay);
    Touch t = {};
    t.touched = sFairy.actionFunc == func_80A03610;
    t.health = gSaveContext.health;
    t.magicState = gSaveContext.magicState;
    t.magicFillTarget = gSaveContext.magicFillTarget;
    t.flagSet = inf != RAND_INF_MAX && Flags_GetRandomizerInf(inf);
    std::printf("[TEST] %s: touched=%d health=0x%X magicState=%u fillTarget=0x%X flag=%d\n", sRow, t.touched ? 1 : 0,
                (unsigned)(u16)t.health, (unsigned)(u16)t.magicState, (unsigned)(u16)t.magicFillTarget,
                t.flagSet ? 1 : 0);
    return t;
}

CheckIdentity IdentityOf(RandomizerCheck rc) {
    CheckIdentity identity;
    identity.randomizerCheck = rc;
    identity.randomizerInf = static_cast<RandomizerInf>(Rando::StaticData::GetLocation(rc)->GetCollectionCheck().flag);
    return identity;
}

void ShuffledLeg(const char* leg, FairyType type, RandomizerCheck rc) {
    const CheckIdentity identity = IdentityOf(rc);
    SF_CHECK(identity.randomizerInf != RAND_INF_MAX, "%s: check %d has no RandomizerInf\n", leg, (int)rc);
    ArmSave();
    ArmFairy(type);
    ObjectExtension::GetInstance().Set<CheckIdentity>(&sFairy.actor, CheckIdentity(identity));
    std::printf("[TEST] %s: %s\n", sRow, leg);
    const Touch t = RunTouch(identity.randomizerInf);
    ObjectExtension::GetInstance().Remove<CheckIdentity>(&sFairy.actor);

    SF_CHECK(t.touched, "%s: the fairy was not touched (the stub frame does not reach the touch)\n", leg);
    SF_CHECK(t.flagSet, "%s: the check's RandomizerInf was not set (its item was not given)\n", leg);
    SF_CHECK(t.health == kOneHeart, "%s: the shuffled fairy healed Link to 0x%X on top of its item (#1289)\n", leg,
             (unsigned)(u16)t.health);
    SF_CHECK(t.magicState != MAGIC_STATE_FILL && t.magicFillTarget == 0,
             "%s: the shuffled fairy started a magic refill on top of its item (#1289)\n", leg);
}

void VanillaLeg(const char* leg, FairyType type) {
    ArmSave();
    ArmFairy(type);
    std::printf("[TEST] %s: %s\n", sRow, leg);
    const Touch t = RunTouch(RAND_INF_MAX);

    SF_CHECK(t.touched, "%s: the fairy was not touched (the stub frame does not reach the touch)\n", leg);
    SF_CHECK(t.health == kOneHeart + kFairyHeal, "%s: a fairy with no check must heal 8 hearts (health 0x%X)\n", leg,
             (unsigned)(u16)t.health);
    if (type == FAIRY_HEAL_BIG) {
        SF_CHECK(t.magicState == MAGIC_STATE_FILL && t.magicFillTarget == MAGIC_NORMAL_METER,
                 "%s: a big fairy with no check must refill magic\n", leg);
    } else {
        SF_CHECK(t.magicState == MAGIC_STATE_IDLE, "%s: a small fairy must not touch magic\n", leg);
    }
}

void BottleLegs(void) {
    const CheckIdentity identity = IdentityOf(RC_TOT_LEFTMOST_GOSSIP_STONE_FAIRY);

    // (a) #6405: a clay pot carrying a check identity (pot shuffle) is not a fairy.
    ArmSave();
    Actor pot;
    std::memset(&pot, 0, sizeof(Actor));
    pot.id = ACTOR_OBJ_TSUBO;
    ObjectExtension::GetInstance().Set<CheckIdentity>(&pot, CheckIdentity(identity));
    const bool potShould = GameInteractor_Should(VB_BOTTLE_ACTOR, true, &pot);
    const bool potFlag = Flags_GetRandomizerInf(identity.randomizerInf);
    ObjectExtension::GetInstance().Remove<CheckIdentity>(&pot);
    std::printf("[TEST] %s: (a) pot with an identity: should=%d flag=%d parent=%p\n", sRow, potShould ? 1 : 0,
                potFlag ? 1 : 0, (void*)pot.parent);
    SF_CHECK(potShould, "(a): the bottle VB answered false for a clay pot (the fairy's bottle path leaked, #6405)\n");
    SF_CHECK(!potFlag, "(a): bottling at a clay pot set its check's RandomizerInf\n");
    SF_CHECK(pot.parent == nullptr, "(a): the clay pot was parented to Link\n");

    // (b) control: a shuffled fairy is bottled as its item.
    ArmSave();
    ArmFairy(FAIRY_HEAL);
    ObjectExtension::GetInstance().Set<CheckIdentity>(&sFairy.actor, CheckIdentity(identity));
    const bool fairyShould = GameInteractor_Should(VB_BOTTLE_ACTOR, true, &sFairy.actor);
    const bool fairyFlag = Flags_GetRandomizerInf(identity.randomizerInf);
    ObjectExtension::GetInstance().Remove<CheckIdentity>(&sFairy.actor);
    std::printf("[TEST] %s: (b) shuffled fairy: should=%d flag=%d parented=%d\n", sRow, fairyShould ? 1 : 0,
                fairyFlag ? 1 : 0, sFairy.actor.parent == &sPlayer.actor ? 1 : 0);
    SF_CHECK(!fairyShould, "(b): a shuffled fairy must not be bottled as a fairy\n");
    SF_CHECK(fairyFlag, "(b): bottling a shuffled fairy must give its check\n");
    SF_CHECK(sFairy.actor.parent == &sPlayer.actor, "(b): a bottled shuffled fairy must be parented to Link\n");
}

/** Runs @p legs on a randomizer save with Shuffle Gossip Stone Fairies on and
 *  only ShuffleFairies' hooks registered; puts every piece of global state back. */
int RunRow(const char* row, void (*legs)(void)) {
    sRow = row;
    sFailures = 0;
    if (OTRGlobals::Instance == nullptr || Rando::Context::GetInstance() == nullptr) {
        std::printf("[TEST] FAIL: %s needs OTRGlobals' randomizer context\n", row);
        return 1;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    if (Rando::StaticData::GetLocation(RC_TOT_LEFTMOST_GOSSIP_STONE_FAIRY)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    const uint8_t priorStone = ctx->GetOption(RSK_SHUFFLE_STONE_FAIRIES).Get();
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const priorPlay = OoT_gPlayState;
    GameInfo* const priorGameInfo = gGameInfo;
    const GameId priorGame = Context_GetCurrentGame();

    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    // Actor_MoveXZGravity reads GET_PLAYER(OoT_gPlayState) and R_UPDATE_RATE.
    std::memset(&sGameInfo, 0, sizeof(GameInfo));
    gGameInfo = &sGameInfo;
    R_UPDATE_RATE = 3;
    OoT_gPlayState = &sPlay;
    {
        HooksAside<GameInteractor::OnVanillaBehavior> vb;
        HooksAside<GameInteractor::OnFlagSet> flagSet;
        HooksAside<GameInteractor::OnPlayerHealthChange> health;

        ArmSave();
        ctx->GetOption(RSK_SHUFFLE_STONE_FAIRIES).Set(RO_GENERIC_ON);
        RegisterShuffleFairies();
        SF_CHECK(!GameInteractor::RegisteredGameHooks<GameInteractor::OnVanillaBehavior>::functionsForID.empty(),
                 "setup: RegisterShuffleFairies registered no hook on a randomizer save with stone fairies on\n");

        legs();

        // Unregister them again: a non-randomizer save turns every condition off.
        gSaveContext.ship.quest.id = QUEST_NORMAL;
        RegisterShuffleFairies();
    }
    ctx->GetOption(RSK_SHUFFLE_STONE_FAIRIES).Set(priorStone);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    OoT_gPlayState = priorPlay;
    gGameInfo = priorGameInfo;
    Context_SetCurrentGame(priorGame);

    if (sFailures != 0) {
        std::printf("[TEST] FAIL (%s): %d failure(s)\n", row, sFailures);
        return 1;
    }
    std::printf("[TEST] PASS: %s\n", row);
    return 0;
}

void NoHealLegs(void) {
    ShuffledLeg("(a) shuffled FAIRY_HEAL", FAIRY_HEAL, RC_TOT_LEFTMOST_GOSSIP_STONE_FAIRY);
    ShuffledLeg("(b) shuffled FAIRY_HEAL_BIG", FAIRY_HEAL_BIG, RC_TOT_LEFTMOST_GOSSIP_STONE_FAIRY_BIG);
    VanillaLeg("(c) vanilla FAIRY_HEAL", FAIRY_HEAL);
    VanillaLeg("(c) vanilla FAIRY_HEAL_BIG", FAIRY_HEAL_BIG);
}

} // namespace

/** @return 0 when every leg holds, 1 otherwise. */
extern "C" int OoT_ShuffledFairyNoHealTest_Run(void) {
    return RunRow("shuffled-fairy-no-heal", NoHealLegs);
}

/** @return 0 when every leg holds, 1 otherwise. */
extern "C" int OoT_ShuffledFairyBottleGuardTest_Run(void) {
    return RunRow("shuffled-fairy-bottle-guard", BottleLegs);
}
