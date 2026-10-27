/**
 * @file mm_minigame_offer_hooks_test.cpp
 * ROM-free, display-free locks for the MM rando minigame reward hooks under
 * games/mm/2s2h/Rando/ActorBehavior/. CTest label "redship"; rows and dispatch
 * names in CMake/SingleExecutable.cmake and src/common/test_runner.cpp:
 *
 *   MMGalleryRupeeStandIn  (--test mm-gallery-rupee-stand-in, #1216)
 *   MMDoggyRaceReward      (--test mm-doggy-race-reward, #1219)
 *
 * HOW THE HOOKS ARE DRIVEN. Through the production arm path: saveType set to
 * SAVETYPE_RANDO and Rando::ActorBehavior::OnFileLoad(), what Rando.cpp's
 * OnSaveLoadHandler runs on every MM rando save load. Each probe then
 * dispatches the VB id the vanilla actor dispatches (VB_GIVE_ITEM_FROM_OFFER
 * from MM_Actor_OfferGetItem, z_actor.c) with a calloc'd stand-in actor whose
 * id names the hook, against a calloc'd PlayState whose only Player is a
 * calloc'd heap Player. The stand-in actor's textId is 0xFFFF, so
 * MM_Player_StartTalking (which the hooks call to hand the talk back) stays on
 * its no-text branch; no probe reaches a hook path that opens a textbox, which
 * would need the ROM's message table.
 *
 * NON-VACUITY. Every row first dispatches its probes with rando disarmed and
 * requires the vanilla verdict, so a hook left armed by an earlier row of a
 * shared `--test all` process cannot satisfy the armed legs.
 *
 * CLEANUP. The SaveContext image is copied back, OnFileLoad re-runs under the
 * restored saveType, MM_gPlayState is put back and the talk cutscene the hooks
 * queued is dropped, so every row is order-free.
 *
 * ---- MMGalleryRupeeStandIn (#1216) -----------------------------------------
 * The shooting galleries' high-score checks (RC_CLOCK_TOWN_EAST_SHOOTING_
 * GALLERY_HIGH_SCORE, RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE) use the galleries'
 * quiver flags as their own flags (Rando/StaticData/Checks.cpp). The vanilla
 * actor (z_en_syateki_man.c, *_SetupGiveReward) sets that flag in its HasParent
 * branch after whatever reward was taken while it was clear, and the selector
 * falls through to a rupee once the quiver is maxed. EnSyatekiMan.cpp hijacked
 * only offers above GI_RUPEE_HUGE, so the rupee was given AND the flag set
 * armed the check through OnFlagSet: two items for one check.
 *   - Town: GI_RUPEE_PURPLE is offered only by the new-record (0x407) branch,
 *     the one that sets the Town quiver flag. With the flag clear it must be
 *     hijacked; with it set (a true repeat) it is kept. GI_RUPEE_HUGE (the
 *     perfect-score repeat) is always kept. Probed on day 3, whose hijack hands
 *     the talk back instead of opening an extra textbox.
 *   - Swamp: the HasParent branch sets the quiver flag first whenever it is
 *     clear, so GI_RUPEE_RED and GI_RUPEE_PURPLE are hijacked while it is clear
 *     and kept once it is set.
 *   - An unshuffled check is never armed by OnFlagSet, so its rupee is kept.
 * The beaver race (EnAz.cpp) has the same shape: race 1's reward is chosen at
 * text 0x10F1, which first sets WEEKEVENTREG_93_01, the check's own flag
 * (cleared by the rando every cycle), and the persistent
 * WEEKEVENTREG_RECEIVED_BEAVER_RACE_BOTTLE turns the bottle into a Red Rupee
 * from the second cycle on. Its hijack opens a textbox, so the beaver legs read
 * the hook's own predicate (Rando::ActorBehavior::EnAzOfferStandsInForCheck)
 * instead of dispatching.
 *
 * ---- MMDoggyRaceReward (#1219) ---------------------------------------------
 * The Doggy Racetrack owner (z_en_aob_01.c) offers the heart piece while
 * WEEKEVENTREG_RECEIVED_DOGGY_RACETRACK_HEART_PIECE is clear and a Red Rupee
 * once it is set, and sets it in the HasParent branch. That flag is
 * RC_DOGGY_RACETRACK_PIECE_OF_HEART's own flag, so the first win arms the check
 * through OnFlagSet. EnAob01.cpp read the selector backwards: on the first win
 * it gave an unplaced Red Rupee itself (+20 rupees on top of the check), and on
 * every repeat win it re-armed the check with no cycleObtained guard (the
 * check's item again). Legs: the first win is hijacked and gives nothing
 * itself (rupees and the rupee accumulator unchanged); a repeat win with the
 * check spent keeps the vanilla Red Rupee and leaves the check unarmed; an
 * unshuffled check keeps its vanilla heart piece (OnFlagSet would not arm it).
 *
 * WHAT THIS DOES NOT COVER. That the gallery owner and the beavers then carry
 * on talking normally after the hand-off, and that CheckQueue gives the seed's
 * item, need a real play state; they are the operator playtest in the PR.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The S2H registries and the single-exe rando hooks exist only in the
 * single-exe build. test_runner.cpp declares these unconditionally.
 */
extern "C" int MM_GalleryRupeeStandIn_RunHeadless(void) {
    printf("[TEST] mm-gallery-rupee-stand-in: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
extern "C" int MM_DoggyRaceReward_RunHeadless(void) {
    printf("[TEST] mm-doggy-race-reward: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Rando.h"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL(%d): ", code);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return code;
}

// The stand-in play state, Player and talking actor every row drives the hooks
// with, plus everything a row writes, saved so the destructor puts it back.
class HookFixture {
  public:
    HookFixture() {
        saved_ = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
        memcpy(saved_.get(), &gSaveContext, sizeof(gSaveContext));
        savedPlay_ = MM_gPlayState;
        play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
        player = static_cast<Player*>(calloc(1, sizeof(Player)));
        actor = static_cast<Actor*>(calloc(1, sizeof(Actor)));
        if (play != nullptr && player != nullptr) {
            play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
            MM_gPlayState = play;
        }
    }

    ~HookFixture() {
        MM_gPlayState = savedPlay_;
        memcpy(&gSaveContext, saved_.get(), sizeof(gSaveContext));
        Rando::ActorBehavior::OnFileLoad();
        // MM_Player_StartTalking queues CS_ID_GLOBAL_TALK; nothing plays it here.
        CutsceneManager_ClearWaiting();
        free(actor);
        free(player);
        free(play);
    }

    bool Ok() const {
        return play != nullptr && player != nullptr && actor != nullptr;
    }

    // The production rando arm path for the ActorBehavior hooks.
    void Arm(bool rando) {
        gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
        Rando::ActorBehavior::OnFileLoad();
    }

    // A fresh talking actor `id` (the hooks parent it and hand it the talk).
    void ResetActor(s16 id) {
        memset(actor, 0, sizeof(Actor));
        actor->id = id;
        actor->textId = 0xFFFF;
        memset(player, 0, sizeof(Player));
    }

    // VB_GIVE_ITEM_FROM_OFFER as MM_Actor_OfferGetItem dispatches it: true lets
    // the vanilla give through.
    bool OfferVerdict(s16 actorId, GetItemId gi) {
        ResetActor(actorId);
        GetItemId item = gi;
        return GameInteractor_Should(VB_GIVE_ITEM_FROM_OFFER, true, &item, actor);
    }

    PlayState* play = nullptr;
    Player* player = nullptr;
    Actor* actor = nullptr;

  private:
    std::unique_ptr<uint8_t[]> saved_;
    PlayState* savedPlay_ = nullptr;
};

void SetWeekEventReg(u16 flag, bool set) {
    if (set) {
        SET_WEEKEVENTREG(flag);
    } else {
        CLEAR_WEEKEVENTREG(flag);
    }
}

void SetCheck(RandoCheckId rc, bool shuffled, bool cycleObtained) {
    RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
    check.shuffled = shuffled;
    check.cycleObtained = cycleObtained;
    check.obtained = cycleObtained;
    check.eligible = false;
}

// ---- #1216 -------------------------------------------------------------------

struct GalleryProbe {
    const char* what;
    s16 sceneId;
    u16 quiverFlag;
    RandoCheckId check;
    bool flagSet;
    bool shuffled;
    GetItemId gi;
    bool expectVanilla; // the verdict once fixed: true keeps the vanilla give
};

const GalleryProbe kGalleryProbes[] = {
    // The quiver offer itself has always been hijacked (control).
    { "Town quiver, flag clear", SCENE_SYATEKI_MIZU, WEEKEVENTREG_RECEIVED_TOWN_SHOOTING_GALLERY_QUIVER_UPGRADE,
      RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE, false, true, GI_QUIVER_40, false },
    // The defect: a maxed quiver's Purple Rupee while the check's flag is clear.
    { "Town new record, maxed quiver, flag clear: Purple Rupee", SCENE_SYATEKI_MIZU,
      WEEKEVENTREG_RECEIVED_TOWN_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE, false,
      true, GI_RUPEE_PURPLE, false },
    { "Town new record, flag set (repeat): Purple Rupee", SCENE_SYATEKI_MIZU,
      WEEKEVENTREG_RECEIVED_TOWN_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE, true,
      true, GI_RUPEE_PURPLE, true },
    { "Town perfect-score repeat: Huge Rupee", SCENE_SYATEKI_MIZU,
      WEEKEVENTREG_RECEIVED_TOWN_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE, false,
      true, GI_RUPEE_HUGE, true },
    { "Town, check not shuffled: Purple Rupee", SCENE_SYATEKI_MIZU,
      WEEKEVENTREG_RECEIVED_TOWN_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE, false,
      false, GI_RUPEE_PURPLE, true },
    { "Swamp perfect, maxed quiver, flag clear: Red Rupee", SCENE_SYATEKI_MORI,
      WEEKEVENTREG_RECEIVED_SWAMP_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE, false, true,
      GI_RUPEE_RED, false },
    { "Swamp perfect, maxed quiver, flag clear: Purple Rupee", SCENE_SYATEKI_MORI,
      WEEKEVENTREG_RECEIVED_SWAMP_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE, false, true,
      GI_RUPEE_PURPLE, false },
    { "Swamp, flag set (repeat): Red Rupee", SCENE_SYATEKI_MORI,
      WEEKEVENTREG_RECEIVED_SWAMP_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE, true, true,
      GI_RUPEE_RED, true },
    { "Swamp, flag set (repeat): Purple Rupee", SCENE_SYATEKI_MORI,
      WEEKEVENTREG_RECEIVED_SWAMP_SHOOTING_GALLERY_QUIVER_UPGRADE, RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE, true, true,
      GI_RUPEE_PURPLE, true },
};

bool GalleryVerdict(HookFixture& fx, const GalleryProbe& probe) {
    fx.play->sceneId = probe.sceneId;
    gSaveContext.save.day = 3; // Town: the day-3 hijack hands the talk back, no extra textbox
    SetWeekEventReg(probe.quiverFlag, probe.flagSet);
    SetCheck(probe.check, probe.shuffled, false);
    return fx.OfferVerdict(ACTOR_EN_SYATEKI_MAN, probe.gi);
}

int RunGalleryLegs(HookFixture& fx) {
    // ---- Leg 1: rando disarmed (non-vacuity) -------------------------------
    fx.Arm(false);
    for (const auto& probe : kGalleryProbes) {
        if (!GalleryVerdict(fx, probe)) {
            return Fail(2, "%s: the offer is already hijacked with rando disarmed - the armed probe would be vacuous",
                        probe.what);
        }
    }

    // ---- Leg 2: rando armed ------------------------------------------------
    fx.Arm(true);
    for (const auto& probe : kGalleryProbes) {
        const bool vanilla = GalleryVerdict(fx, probe);
        if (vanilla != probe.expectVanilla) {
            return Fail(10,
                        "%s: the vanilla give was %s, expected %s. While the gallery's quiver flag (the high-score "
                        "check's own flag) is clear, the vanilla HasParent branch sets it after any reward and "
                        "OnFlagSet arms the check, so the offer stands in for the check and its rupee must not be "
                        "given as well (#1216)",
                        probe.what, vanilla ? "kept" : "blocked", probe.expectVanilla ? "kept" : "blocked");
        }
    }

    // ---- Leg 3: the beaver race's stand-in predicate ------------------------
    struct BeaverProbe {
        const char* what;
        GetItemId gi;
        bool shuffled;
        bool standsIn;
    };
    const BeaverProbe beaverProbes[] = {
        { "race 1 bottle", GI_BOTTLE, true, true },
        { "race 2 heart piece", GI_HEART_PIECE, true, true },
        { "race 2 repeat Purple Rupee", GI_RUPEE_PURPLE, true, false },
        { "race 1 Red Rupee (second cycle on), check shuffled", GI_RUPEE_RED, true, true },
        { "race 1 Red Rupee, check not shuffled", GI_RUPEE_RED, false, false },
    };
    for (const auto& probe : beaverProbes) {
        SetCheck(RC_WATERFALL_RAPIDS_BEAVER_RACE_01, probe.shuffled, false);
        const bool standsIn = Rando::ActorBehavior::EnAzOfferStandsInForCheck(probe.gi);
        if (standsIn != probe.standsIn) {
            return Fail(11,
                        "beaver %s: stands in for its check = %d, expected %d. Race 1's reward is picked at text "
                        "0x10F1 right after it sets WEEKEVENTREG_93_01, RC_WATERFALL_RAPIDS_BEAVER_RACE_01's own "
                        "flag, so its Red Rupee arrives on top of the re-armed check (#1216)",
                        probe.what, standsIn, probe.standsIn);
        }
    }
    return 0;
}

// ---- #1219 -------------------------------------------------------------------

int32_t RupeeTotal() {
    return gSaveContext.save.saveInfo.playerData.rupees + gSaveContext.rupeeAccumulator;
}

int RunDoggyLegs(HookFixture& fx) {
    RandoSaveCheck& check = RANDO_SAVE_CHECKS[RC_DOGGY_RACETRACK_PIECE_OF_HEART];
    fx.play->sceneId = SCENE_F01_B;

    // ---- Leg 1: rando disarmed (non-vacuity) -------------------------------
    fx.Arm(false);
    SetWeekEventReg(WEEKEVENTREG_RECEIVED_DOGGY_RACETRACK_HEART_PIECE, false);
    SetCheck(RC_DOGGY_RACETRACK_PIECE_OF_HEART, true, false);
    int32_t before = RupeeTotal();
    if (!fx.OfferVerdict(ACTOR_EN_AOB_01, GI_HEART_PIECE) || RupeeTotal() != before) {
        return Fail(2, "the racetrack's heart piece offer is already hijacked with rando disarmed - the armed probes "
                       "would be vacuous");
    }

    fx.Arm(true);

    // ---- Leg 2: first win of the cycle (flag clear) -------------------------
    SetCheck(RC_DOGGY_RACETRACK_PIECE_OF_HEART, true, false);
    before = RupeeTotal();
    if (fx.OfferVerdict(ACTOR_EN_AOB_01, GI_HEART_PIECE)) {
        return Fail(10, "first win: the vanilla heart piece was let through with RC_DOGGY_RACETRACK_PIECE_OF_HEART "
                        "shuffled (#1219)");
    }
    if (RupeeTotal() != before) {
        return Fail(11,
                    "first win: the hook gave %d rupees of its own on top of the check, which the HasParent flag set "
                    "arms through OnFlagSet - an unplaced Red Rupee (#1219)",
                    (int)(RupeeTotal() - before));
    }
    if (check.eligible) {
        return Fail(12, "first win: the hook armed the check itself; the HasParent flag set arms it (#1219)");
    }

    // ---- Leg 3: a repeat win, check already given this cycle ----------------
    SetWeekEventReg(WEEKEVENTREG_RECEIVED_DOGGY_RACETRACK_HEART_PIECE, true);
    SetCheck(RC_DOGGY_RACETRACK_PIECE_OF_HEART, true, true);
    const bool vanilla = fx.OfferVerdict(ACTOR_EN_AOB_01, GI_RUPEE_RED);
    if (check.eligible) {
        return Fail(13, "repeat win: RC_DOGGY_RACETRACK_PIECE_OF_HEART was armed again although it was already "
                        "obtained this cycle - the check's item would be given a second time (#1219)");
    }
    if (!vanilla) {
        return Fail(14, "repeat win: the vanilla Red Rupee was blocked; a repeat with the check spent gives the "
                        "vanilla junk, as OoTMM does (#1219)");
    }

    // ---- Leg 4: an unshuffled check keeps its vanilla heart piece ------------
    SetWeekEventReg(WEEKEVENTREG_RECEIVED_DOGGY_RACETRACK_HEART_PIECE, false);
    SetCheck(RC_DOGGY_RACETRACK_PIECE_OF_HEART, false, false);
    if (!fx.OfferVerdict(ACTOR_EN_AOB_01, GI_HEART_PIECE)) {
        return Fail(15, "unshuffled check: the vanilla heart piece was blocked, but OnFlagSet arms only a shuffled "
                        "check, so nothing would be given (#1219)");
    }
    return 0;
}

bool BringUpOk(const char* name) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        Fail(1, "%s: Ship::Context or its ConsoleVariables are missing; run the shared bring-up first", name);
        return false;
    }
    return true;
}

} // namespace

extern "C" int MM_GalleryRupeeStandIn_RunHeadless(void) {
    printf("[TEST] mm-gallery-rupee-stand-in: a minigame rupee that stands in for an open check is not given on top "
           "of it (#1216)\n");
    if (!BringUpOk("mm-gallery-rupee-stand-in")) {
        return 1;
    }
    int rc;
    {
        HookFixture fx;
        if (!fx.Ok()) {
            return Fail(1, "could not allocate the fixtures");
        }
        rc = RunGalleryLegs(fx);
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-gallery-rupee-stand-in: PASS (both galleries hijack the rupee only while the high-score check "
           "is open, keep it on repeats; the beavers' race-1 Red Rupee stands in for its shuffled check)\n");
    return 0;
}

extern "C" int MM_DoggyRaceReward_RunHeadless(void) {
    printf("[TEST] mm-doggy-race-reward: the Doggy Racetrack reward follows the vanilla selector and is given once "
           "per cycle (#1219)\n");
    if (!BringUpOk("mm-doggy-race-reward")) {
        return 1;
    }
    int rc;
    {
        HookFixture fx;
        if (!fx.Ok()) {
            return Fail(1, "could not allocate the fixtures");
        }
        rc = RunDoggyLegs(fx);
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-doggy-race-reward: PASS (first win hijacked with no extra rupee, a spent check keeps the "
           "vanilla Red Rupee and is not re-armed, an unshuffled check keeps its heart piece)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
