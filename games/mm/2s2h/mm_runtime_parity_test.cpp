/**
 * @file mm_runtime_parity_test.cpp
 * ROM-free, display-free lock for #1426: two Majora's Mask runtime items OoTMM has on by default.
 * CTest label "redship", row MMRuntimeParity in CMake/SingleExecutable.cmake,
 * dispatch "mm-runtime-parity" in src/common/test_runner.cpp.
 *
 * SKIP BOSS INTRODUCTIONS (legs 1-2). SoH reads gEnhancements.TimeSavers.SkipCutscene.BossIntro with an IS_RANDO
 * default (timesaver_hook_handlers.cpp), so an untouched key skips Hyrule's boss intros on a rando file. MM's reader,
 * Cutscenes/SkipEnemyCutscenes.cpp, read the same key with a 0 default and re-armed only when the key changed, so on
 * the same paired file Termina's intros played. OoTMM skips them by default (cutsceneIntro*). The fix reads the key
 * with MM's IS_RANDO default and adds "IS_RANDO" to the three registrars' update paths. This leg drives the production
 * arm path (saveType, then ShipInit::Init("IS_RANDO"), what Rando.cpp's OnSaveLoadHandler runs on every MM load) with
 * the key CLEARED, and requires each of the TU's three registrars to arm:
 *   - RegisterSkipEnemyIntros: OnActorInit[ACTOR_EN_BIGSLIME] rises by exactly one;
 *   - RegisterSkipEnemyCutscenes: ShouldVanillaBehavior[VB_SET_CAMERA_AT_EYE] rises by exactly one;
 *   - RegisterSkipBossWarpCutscenes: VB_START_CUTSCENE for Mountain Village (spring) cutscene 13, the warp from Goht's
 *     lair, flips from play to skip.
 * The two buckets have no other registrant the "IS_RANDO" drive arms (grep games/mm/2s2h; the rando actor behaviours on
 * ACTOR_EN_BIGSLIME arm from OnFileLoad, not from ShipInit), and no other "IS_RANDO" registrar handles that scene.
 * Disarmed first, the non-vacuity baseline. Then an explicit 0 under rando must disarm (the player's off wins), and an
 * explicit 1 on a vanilla file must arm. Red before the fix: the "IS_RANDO" drive ran none of the three registrars, and
 * with the old 0 default a cleared key armed nothing anyway.
 *
 * LENIENT GORON SPIKES (legs 3-7). OoTMM's lenientSpikes (default true) is three byte patches in ovl_player_actor;
 * Masks/GoronRollingLenientSpikes.cpp documents the disassembly. The legs call the three VBs as z_player.c dispatches
 * them, with the vanilla condition as the incoming verdict, over a calloc'd play state whose player slot holds a
 * calloc'd Player and a stand-in sPlayerControlInput:
 *   3. Default: with the key cleared, a vanilla file answers every probe as vanilla (non-vacuity), a rando file as
 *      OoTMM does.
 *   4. VB_GORON_ROLL_INCREASE_SPIKE_LEVEL: a roll speed of 0x2000 steps the charge (vanilla needs 0x36B0), 0x0800 does
 *      not, and the magic terms still hold (magic 1, or a magic state other than idle, refuses).
 *   5. VB_GORON_ROLL_DISABLE_SPIKE_MODE: spikes out, A held, magic left and charge 4 stay out at speed 6 (vanilla drops
 *      them under 12); a negative speed, releasing A or an empty meter still drops them.
 *   6. VB_GORON_ROLL_RESET_SPIKE_LEVEL_ON_LANDING: the landing reset is refused (vanilla's true is the off control).
 *   7. With Goron Rolling Ignores Magic on too, the answer does not depend on which of the two registered last
 *      (driving a key re-registers its hooks at the end of the bucket): an empty meter still charges at 0x2000 and
 * still keeps the spikes out. Red against Ignores Magic's old assigning hook when it ran last. Goron Rolling Fast
 * Spikes is held off throughout: it adds to the charge on every dispatch.
 *
 * Plus the play-state guard: with no play state each hook leaves the vanilla verdict (leg 8).
 *
 * CLEANUP. The SaveContext image, MM_gPlayState, sPlayerControlInput and every key this row touches are put back, each
 * key re-driven and the "IS_RANDO" drive re-run under the restored saveType, so the row is order-free.
 *
 * WHAT THIS DOES NOT COVER. That the camera stops wiggling with Disable Crit Wiggle on (#1426's third item) needs a
 * camera and a play state; the classification lock's leg (3e-2) holds MM's read of SoH's key, and the rest is the
 * playtest in the PR. So is how a lenient roll feels.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The S2H registries and the ShipInit split this lock drives exist only in the single-exe build. test_runner.cpp
 * declares this unconditionally.
 */
extern "C" int MM_RuntimeParity_RunHeadless(void) {
    printf("[TEST] mm-runtime-parity: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
extern Input* sPlayerControlInput;
}

namespace {

#define RP_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

constexpr const char* kBossIntroCVar = "gEnhancements.TimeSavers.SkipCutscene.BossIntro";
constexpr const char* kLenientCVar = "gEnhancements.Masks.GoronRollingLenientSpikes";
constexpr const char* kIgnoresMagicCVar = "gEnhancements.Masks.GoronRollingIgnoresMagic";
constexpr const char* kFastSpikesCVar = "gEnhancements.Masks.GoronRollingFastSpikes";

// Settled counts: S2H::GameHooks unregistration is deferred until the next Execute of that hook type, and the COND_*
// macros unregister-then-register on every drive, so the raw maps can credit a hook already slated for removal.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

size_t BigslimeInitCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_BIGSLIME);
}
size_t CameraAtEyeCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_SET_CAMERA_AT_EYE);
}

/** One key's state on entry, put back (and re-driven) on the way out. */
struct SavedKey {
    const char* key;
    bool existed;
    int32_t value;

    explicit SavedKey(const char* k) : key(k), existed(CVarExists(k)), value(CVarGetInteger(k, 0)) {
    }
    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
        ShipInit::Init(key);
    }
};

void DriveKey(const char* key, int32_t value) {
    CVarSetInteger(key, value);
    ShipInit::Init(key);
}

void ClearKey(const char* key) {
    CVarClear(key);
    ShipInit::Init(key);
}

// The production rando arm path (Rando/Rando.cpp OnSaveLoadHandler).
void DriveRandoArmState(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    ShipInit::Init("IS_RANDO");
}

// VB_START_CUTSCENE as the cutscene manager dispatches it: Mountain Village (spring), cutscene 13, the arrival from
// Goht's lair that RegisterSkipBossWarpCutscenes skips.
bool GohtWarpCutscenePlays(PlayState* play) {
    play->sceneId = SCENE_10YUKIYAMANOMURA2;
    s16 csId = 13;
    return GameInteractor_Should(VB_START_CUTSCENE, true, &csId, (Actor*)nullptr);
}

// ---- Legs 1-2: Skip Boss Introductions ------------------------------------
int RunBossIntroLegs(PlayState* play) {
    MM_gPlayState = play;

    // Leg 1: key untouched, vanilla file. The non-vacuity baseline: nothing armed.
    ClearKey(kBossIntroCVar);
    DriveRandoArmState(false);
    const size_t bigslimeBefore = BigslimeInitCount();
    const size_t cameraBefore = CameraAtEyeCount();
    RP_ASSERT(GohtWarpCutscenePlays(play), 2,
              "the Goht warp cutscene is skipped on a vanilla file with Skip Boss Introductions untouched - the leg-2 "
              "probe would be vacuous");

    // Leg 2: the same untouched key on a rando file arms all three registrars.
    DriveRandoArmState(true);
    if (BigslimeInitCount() != bigslimeBefore + 1 || CameraAtEyeCount() != cameraBefore + 1) {
        printf("[TEST] FAIL(3): with Skip Boss Introductions untouched, arming rando moved OnActorInit[EN_BIGSLIME] "
               "%zu -> %zu and VB_SET_CAMERA_AT_EYE %zu -> %zu, expected exactly +1 each - MM's reader does not take "
               "SoH's IS_RANDO default or is not re-armed on \"IS_RANDO\", so Termina's boss intros play on a rando "
               "file where Hyrule's are skipped (#1426)\n",
               bigslimeBefore, BigslimeInitCount(), cameraBefore, CameraAtEyeCount());
        return 3;
    }
    RP_ASSERT(!GohtWarpCutscenePlays(play), 3,
              "with Skip Boss Introductions untouched on a rando file the Goht warp cutscene still plays - "
              "RegisterSkipBossWarpCutscenes did not arm on \"IS_RANDO\" (#1426)");

    // An explicit off wins under rando.
    DriveKey(kBossIntroCVar, 0);
    if (BigslimeInitCount() != bigslimeBefore || CameraAtEyeCount() != cameraBefore || !GohtWarpCutscenePlays(play)) {
        printf("[TEST] FAIL(4): Skip Boss Introductions set to 0 on a rando file left the skips armed - the player's "
               "explicit off must win over the IS_RANDO default (#1426)\n");
        return 4;
    }

    // An explicit on still arms on a vanilla file.
    DriveRandoArmState(false);
    DriveKey(kBossIntroCVar, 1);
    if (BigslimeInitCount() != bigslimeBefore + 1 || CameraAtEyeCount() != cameraBefore + 1 ||
        GohtWarpCutscenePlays(play)) {
        printf("[TEST] FAIL(5): Skip Boss Introductions set to 1 on a vanilla file did not arm exactly once (#1426)\n");
        return 5;
    }
    DriveKey(kBossIntroCVar, 0);
    return 0;
}

// ---- Legs 3-8: Lenient Goron Spikes ---------------------------------------
struct RollProbe {
    PlayState* play;
    Player* player;
    Input* input;
};

void SetMagic(s16 magicState, s8 magic) {
    gSaveContext.magicState = magicState;
    gSaveContext.save.saveInfo.playerData.magic = magic;
}

// z_player.c's vanilla condition for each VB, as the incoming verdict.
bool IncreaseSpikeLevel(const RollProbe& p, s16 rollSpeed) {
    p.player->av2.actionVar2 = rollSpeed;
    const bool vanilla = (gSaveContext.magicState == MAGIC_STATE_IDLE) &&
                         (gSaveContext.save.saveInfo.playerData.magic >= 2) && (p.player->av2.actionVar2 >= 0x36B0);
    return GameInteractor_Should(VB_GORON_ROLL_INCREASE_SPIKE_LEVEL, vanilla);
}

bool DisableSpikeMode(const RollProbe& p, f32 speed, bool holdA) {
    p.player->stateFlags3 |= PLAYER_STATE3_80000;
    p.player->av1.actionVar1 = 4;
    p.player->unk_B08 = speed;
    p.input->cur.button = holdA ? BTN_A : 0;
    const bool vanilla =
        (p.player->stateFlags3 & PLAYER_STATE3_80000) &&
        (!CHECK_BTN_ALL(p.input->cur.button, BTN_A) || (gSaveContext.save.saveInfo.playerData.magic == 0) ||
         ((p.player->av1.actionVar1 == 4) && (p.player->unk_B08 < 12.0f)));
    return GameInteractor_Should(VB_GORON_ROLL_DISABLE_SPIKE_MODE, vanilla);
}

bool ResetOnLanding() {
    return GameInteractor_Should(VB_GORON_ROLL_RESET_SPIKE_LEVEL_ON_LANDING, true);
}

/** Every lenient verdict against a vanilla answer: true when all three VBs answer as vanilla does. */
bool AllVanilla(const RollProbe& p) {
    SetMagic(MAGIC_STATE_IDLE, 48);
    return !IncreaseSpikeLevel(p, 0x2000) && DisableSpikeMode(p, 6.0f, true) && ResetOnLanding();
}

/** Every lenient verdict as OoTMM's patch answers it. */
bool AllLenient(const RollProbe& p) {
    SetMagic(MAGIC_STATE_IDLE, 48);
    return IncreaseSpikeLevel(p, 0x2000) && !DisableSpikeMode(p, 6.0f, true) && !ResetOnLanding();
}

int RunLenientLegs(const RollProbe& p) {
    MM_gPlayState = p.play;
    sPlayerControlInput = p.input;
    DriveKey(kFastSpikesCVar, 0);
    DriveKey(kIgnoresMagicCVar, 0);

    // Leg 3: the default. Untouched key, vanilla file: vanilla answers (the non-vacuity control); rando file: OoTMM's.
    ClearKey(kLenientCVar);
    DriveRandoArmState(false);
    RP_ASSERT(AllVanilla(p), 10,
              "with Lenient Goron Spikes untouched on a vanilla file a Goron-roll VB already answers leniently - the "
              "default probe would be vacuous");
    DriveRandoArmState(true);
    RP_ASSERT(AllLenient(p), 11,
              "with Lenient Goron Spikes untouched on a rando file the Goron-roll VBs answer as vanilla - the TU is "
              "not in the link, or its key does not default to IS_RANDO / re-arm on \"IS_RANDO\" (#1426)");
    DriveKey(kLenientCVar, 0);
    RP_ASSERT(AllVanilla(p), 11, "Lenient Goron Spikes set to 0 on a rando file still answers leniently (#1426)");
    DriveRandoArmState(false);
    DriveKey(kLenientCVar, 1);

    // Leg 4: the charge step (OoTMM patch 1).
    SetMagic(MAGIC_STATE_IDLE, 48);
    RP_ASSERT(IncreaseSpikeLevel(p, 0x2000), 12, "a 0x2000 roll does not step the spike charge (OoTMM: 0x1000)");
    RP_ASSERT(IncreaseSpikeLevel(p, 0x1000), 12, "a 0x1000 roll does not step the spike charge (OoTMM: 0x1000)");
    RP_ASSERT(!IncreaseSpikeLevel(p, 0x0800), 12, "a 0x0800 roll steps the spike charge (OoTMM's floor is 0x1000)");
    SetMagic(MAGIC_STATE_IDLE, 1);
    RP_ASSERT(!IncreaseSpikeLevel(p, 0x2000), 12, "the spike charge steps with one point of magic (vanilla needs 2)");
    SetMagic(MAGIC_STATE_CONSUME_SETUP, 48);
    RP_ASSERT(!IncreaseSpikeLevel(p, 0x2000), 12, "the spike charge steps while the magic meter is busy");

    // Leg 5: spikes out (OoTMM patch 2).
    SetMagic(MAGIC_STATE_IDLE, 48);
    RP_ASSERT(!DisableSpikeMode(p, 6.0f, true), 13, "spikes drop at speed 6 with A held (OoTMM: no minimum speed)");
    RP_ASSERT(!DisableSpikeMode(p, 0.0f, true), 13, "spikes drop at speed 0 with A held (OoTMM: no minimum speed)");
    RP_ASSERT(DisableSpikeMode(p, -1.0f, true), 13, "spikes stay out at a negative speed (OoTMM drops them below 0)");
    RP_ASSERT(DisableSpikeMode(p, 6.0f, false), 13, "spikes stay out with A released");
    SetMagic(MAGIC_STATE_IDLE, 0);
    RP_ASSERT(DisableSpikeMode(p, 6.0f, true), 13, "spikes stay out with an empty magic meter");

    // Leg 6: the landing (OoTMM patch 3).
    RP_ASSERT(!ResetOnLanding(), 14, "a landing still resets the spike charge (OoTMM keeps it)");

    // Leg 7: with Ignores Magic on too, one answer whichever registered last. Driving a key re-registers its hooks at
    // the end of each bucket, so the two drives below put each TU last in turn.
    for (int order = 0; order < 2; order++) {
        if (order == 0) {
            DriveKey(kIgnoresMagicCVar, 1); // Ignores Magic last
        } else {
            DriveKey(kLenientCVar, 1); // Lenient last
        }
        SetMagic(MAGIC_STATE_IDLE, 0);
        if (!IncreaseSpikeLevel(p, 0x2000) || DisableSpikeMode(p, 6.0f, true) || !DisableSpikeMode(p, 6.0f, false)) {
            printf("[TEST] FAIL(15): with Goron Rolling Ignores Magic and Lenient Goron Spikes both on and %s "
                   "registered last, an empty meter did not charge at 0x2000 and keep the spikes out at speed 6 (or "
                   "releasing A kept them) - the two hooks' answers depend on their order (#1426)\n",
                   order == 0 ? "Ignores Magic" : "Lenient");
            return 15;
        }
    }
    DriveKey(kIgnoresMagicCVar, 0);
    SetMagic(MAGIC_STATE_IDLE, 48);
    RP_ASSERT(AllLenient(p), 15, "turning Ignores Magic back off left Lenient Goron Spikes answering differently");

    // Leg 8: no play state, vanilla verdicts (the landing hook reads none and still answers).
    MM_gPlayState = nullptr;
    RP_ASSERT(!GameInteractor_Should(VB_GORON_ROLL_INCREASE_SPIKE_LEVEL, false), 16,
              "VB_GORON_ROLL_INCREASE_SPIKE_LEVEL changed the verdict with no play state");
    RP_ASSERT(GameInteractor_Should(VB_GORON_ROLL_DISABLE_SPIKE_MODE, true), 16,
              "VB_GORON_ROLL_DISABLE_SPIKE_MODE changed the verdict with no play state");
    MM_gPlayState = p.play;
    return 0;
}

} // namespace

extern "C" int MM_RuntimeParity_RunHeadless(void) {
    printf("[TEST] mm-runtime-parity: Skip Boss Introductions' rando default and Lenient Goron Spikes (#1426)\n");

    auto ctx = Ship::Context::GetInstance();
    RP_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing - run the shared bring-up first");
    RP_ASSERT(ctx->GetConsoleVariables() != nullptr, 1, "ConsoleVariables missing - the registrars read CVars");

    // Everything the legs write, saved so the cleanup can put it back.
    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;
    Input* const savedInput = sPlayerControlInput;
    const SavedKey savedKeys[] = { SavedKey(kBossIntroCVar), SavedKey(kLenientCVar), SavedKey(kIgnoresMagicCVar),
                                   SavedKey(kFastSpikesCVar) };

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    Player* player = static_cast<Player*>(calloc(1, sizeof(Player)));
    Input* input = static_cast<Input*>(calloc(1, sizeof(Input)));
    int rc = 1;
    if (play != nullptr && player != nullptr && input != nullptr) {
        play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
        rc = RunBossIntroLegs(play);
        if (rc == 0) {
            rc = RunLenientLegs(RollProbe{ play, player, input });
        }
    } else {
        printf("[TEST] FAIL(1): could not allocate the stand-in play state, player and input\n");
    }

    // ---- Cleanup (runs on pass and on fail) ----------------------------------
    MM_gPlayState = savedPlay;
    sPlayerControlInput = savedInput;
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    for (const SavedKey& key : savedKeys) {
        key.Restore();
    }
    ShipInit::Init("IS_RANDO");
    free(input);
    free(player);
    free(play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-runtime-parity: PASS (boss intros arm on an untouched key under rando and an explicit off "
           "wins; Lenient Goron Spikes charges from 0x1000, keeps spikes out at any speed and the charge through a "
           "landing, defaults on under rando, and agrees with Ignores Magic in either order)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
