/**
 * @file mm_minigame_offer_hooks_test.cpp
 * ROM-free, display-free locks for the MM rando minigame reward hooks under
 * games/mm/2s2h/Rando/ActorBehavior/. CTest label "redship"; rows and dispatch
 * names in CMake/SingleExecutable.cmake and src/common/test_runner.cpp:
 *
 *   MMGalleryRupeeStandIn  (--test mm-gallery-rupee-stand-in, #1216)
 *   MMDoggyRaceReward      (--test mm-doggy-race-reward, #1219)
 *   MMMinigameRepeatGuards (--test mm-minigame-repeat-guards, #1220)
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
 * ---- MMMinigameRepeatGuards (#1220) ------------------------------------------
 * Three hooks armed their check on every trigger with no cycleObtained guard,
 * and CheckQueue gives any eligible check without consulting it, so a repeat in
 * the same cycle gave the check's item again (renewables farmable, traps
 * re-fired, one-time items turned to junk).
 *   - Deku Scrub Playground (EnLiftNuts.cpp, RC_DEKU_PLAYGROUND_ANY_DAY): a
 *     second win in the cycle. Legs: the first win arms the check (control); a
 *     win with the check spent leaves it unarmed and keeps the vanilla Purple
 *     Rupee (OoTMM gives the vanilla junk on repeats); the day-3 all-days heart
 *     piece is still hijacked (its flag set arms RC_DEKU_PLAYGROUND_ALL_DAYS)
 *     and its repeat keeps the vanilla Purple Rupee.
 *   - Marine Research Lab fish (EnFish2.cpp,
 *     RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART): a re-fed fish after a
 *     lab re-entry. The hook spawns its pickup through MM_Actor_Spawn, which is
 *     not ROM-free, so the legs read its own gate
 *     (Rando::ActorBehavior::Fish2ShouldOfferCheck) and then dispatch
 *     VB_FISH2_SPAWN_HEART_PIECE only with the check spent, where the fixed hook
 *     spawns nothing: the vanilla Col_Man must stay blocked too, because the
 *     rando never sets WEEKEVENTREG_RECEIVED_MARINE_RESEARCH_LAB_FISH_HEART_PIECE
 *     and Col_Man would offer a vanilla heart piece, so a repeat gives nothing.
 *   - Song of Storms (StoryCutscenes/SkipLearningSongOfStorms.cpp,
 *     RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS): Bg_Haka_Tomb starts csId 13
 *     each time its room loads and the tomb is targeted while the room is clear,
 *     so a room re-entry re-triggers it. This hook is a ShipInit "IS_RANDO"
 *     registrar, so this leg arms it the way Rando.cpp's OnSaveLoadHandler does
 *     (saveType + ShipInit::Init("IS_RANDO"), Story CVar held at 0). The
 *     cutscene stays skipped on a repeat; only the re-arm is guarded.
 *   Not reproducing, so not changed: EnKnight.cpp (Igos du Ikana: the room's
 *   clear flag keeps him from being fought again in the cycle) and EnMnk.cpp
 *   (the Sonata hand-off sets WEEKEVENTREG_09_80 on the same trigger, which
 *   kills the tied-up monkey on every later load in the cycle).
 *
 * ---- Every row, again with every minigame row on (W6-MINIGAMES, #1236) ------
 * The single exe links 2Ship's minigame TUs (archery scores, Honey & Darling,
 * the beaver and Doggy races, Swordsman School, Cucco Shack, Romani, the
 * Gorman and Goron races, ...). Each row then runs its legs a second time with
 * every minigame key at its most generous value (RunWithMinigameRowsOn): the
 * TUs' own hooks must arm exactly once each, no reward-path registry may grow,
 * the verdicts must not change, and clearing the keys must take every hook
 * back out. Red (40) while a minigame TU is out of the link.
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
extern "C" int MM_MinigameRepeatGuards_RunHeadless(void) {
    printf("[TEST] mm-minigame-repeat-guards: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "src/overlays/actors/ovl_En_Fish2/z_en_fish2.h"
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

// ---- #1220 -------------------------------------------------------------------

// The feeding flags EnFish2.cpp clears once a feeding completes.
const u16 kFishFeedingFlags[] = {
    WEEKEVENTREG_81_10, WEEKEVENTREG_81_20, WEEKEVENTREG_81_40,
    WEEKEVENTREG_81_80, WEEKEVENTREG_82_01, WEEKEVENTREG_82_02,
};

void SetPlaygroundDay(s32 day, bool wonDay1, bool wonDay2, bool heartPieceFlag) {
    gSaveContext.save.day = day;
    SetWeekEventReg(WEEKEVENTREG_WON_DEKU_PLAYGROUND_DAY_1, wonDay1);
    SetWeekEventReg(WEEKEVENTREG_WON_DEKU_PLAYGROUND_DAY_2, wonDay2);
    SetWeekEventReg(WEEKEVENTREG_RECEIVED_DEKU_PLAYGROUND_HEART_PIECE, heartPieceFlag);
}

int RunPlaygroundLegs(HookFixture& fx) {
    RandoSaveCheck& anyDay = RANDO_SAVE_CHECKS[RC_DEKU_PLAYGROUND_ANY_DAY];
    fx.play->sceneId = SCENE_DEKUTES;

    // Rando disarmed (non-vacuity).
    fx.Arm(false);
    SetPlaygroundDay(1, false, false, false);
    SetCheck(RC_DEKU_PLAYGROUND_ANY_DAY, true, false);
    if (!fx.OfferVerdict(ACTOR_EN_LIFT_NUTS, GI_RUPEE_PURPLE) || anyDay.eligible) {
        return Fail(2, "the playground offer is already hijacked with rando disarmed - the armed probes would be "
                       "vacuous");
    }

    fx.Arm(true);

    // First win of the cycle (control).
    SetCheck(RC_DEKU_PLAYGROUND_ANY_DAY, true, false);
    if (fx.OfferVerdict(ACTOR_EN_LIFT_NUTS, GI_RUPEE_PURPLE) || !anyDay.eligible) {
        return Fail(3, "first playground win: RC_DEKU_PLAYGROUND_ANY_DAY was not armed in place of the vanilla Purple "
                       "Rupee");
    }

    // A second win with the check spent this cycle.
    SetCheck(RC_DEKU_PLAYGROUND_ANY_DAY, true, true);
    const bool vanilla = fx.OfferVerdict(ACTOR_EN_LIFT_NUTS, GI_RUPEE_PURPLE);
    if (anyDay.eligible) {
        return Fail(10, "repeat playground win: RC_DEKU_PLAYGROUND_ANY_DAY was armed again although it was already "
                        "obtained this cycle - the check's item would be given a second time (#1220)");
    }
    if (!vanilla) {
        return Fail(11, "repeat playground win: the vanilla Purple Rupee was blocked; a repeat with the check spent "
                        "gives the vanilla junk, as OoTMM does (#1220)");
    }

    // Day 3 with both earlier days won: the all-days heart piece stands in for
    // RC_DEKU_PLAYGROUND_ALL_DAYS (the HasParent flag set arms it).
    SetPlaygroundDay(3, true, true, false);
    if (fx.OfferVerdict(ACTOR_EN_LIFT_NUTS, GI_HEART_PIECE) || anyDay.eligible) {
        return Fail(12, "day-3 all-days win: the vanilla heart piece was let through, or the any-day check was armed "
                        "(#1220)");
    }
    SetPlaygroundDay(3, true, true, true);
    if (!fx.OfferVerdict(ACTOR_EN_LIFT_NUTS, GI_RUPEE_PURPLE) || anyDay.eligible) {
        return Fail(13, "repeat day-3 all-days win: the vanilla Purple Rupee was blocked or a check was armed (#1220)");
    }
    return 0;
}

int RunFishLegs(HookFixture& fx) {
    RandoSaveCheck& fish = RANDO_SAVE_CHECKS[RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART];
    fx.play->sceneId = SCENE_LABO;

    SetCheck(RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART, true, false);
    if (!Rando::ActorBehavior::Fish2ShouldOfferCheck()) {
        return Fail(4, "lab fish: the open check is not offered on a completed feeding");
    }
    SetCheck(RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART, true, true);
    if (Rando::ActorBehavior::Fish2ShouldOfferCheck()) {
        return Fail(20, "lab fish: a completed feeding offers RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART "
                        "again although it was already obtained this cycle - a re-fed fish gives the check's item "
                        "a second time (#1220)");
    }

    // The armed dispatch with the check spent: no pickup, no vanilla Col_Man
    // (it would offer a vanilla heart piece), the feeding flags still cleared.
    EnFish2* fishActor = static_cast<EnFish2*>(calloc(1, sizeof(EnFish2)));
    if (fishActor == nullptr) {
        return Fail(1, "could not allocate the fish");
    }
    fishActor->actor.id = ACTOR_EN_FISH2;
    for (u16 flag : kFishFeedingFlags) {
        SET_WEEKEVENTREG(flag);
    }
    const bool vanilla = GameInteractor_Should(VB_FISH2_SPAWN_HEART_PIECE, true, fishActor);
    free(fishActor);
    if (vanilla) {
        return Fail(21, "lab fish: with the check spent the vanilla Col_Man was let through; the rando never sets "
                        "WEEKEVENTREG_RECEIVED_MARINE_RESEARCH_LAB_FISH_HEART_PIECE, so it would offer a vanilla heart "
                        "piece (#1220)");
    }
    if (fish.eligible) {
        return Fail(22, "lab fish: the spent check was armed again (#1220)");
    }
    for (u16 flag : kFishFeedingFlags) {
        if (CHECK_WEEKEVENTREG(flag)) {
            return Fail(23, "lab fish: the feeding flags were not cleared after a completed feeding");
        }
    }
    return 0;
}

constexpr const char* kStoryCVar = "gEnhancements.TimeSavers.SkipCutscene.Story";

// The production arm path for the ShipInit "IS_RANDO" registrars.
void ArmShipInit(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    ShipInit::Init("IS_RANDO");
}

// VB_START_CUTSCENE as CutsceneManager_Start dispatches it, for Flat's tomb.
bool StormsCutsceneVerdict(HookFixture& fx) {
    fx.play->sceneId = SCENE_HAKASHITA;
    s16 csId = 13;
    return GameInteractor_Should(VB_START_CUTSCENE, true, &csId, (Actor*)nullptr);
}

int RunStormsLegs(HookFixture& fx) {
    RandoSaveCheck& storms = RANDO_SAVE_CHECKS[RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS];

    ArmShipInit(false);
    SetCheck(RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS, true, false);
    if (!StormsCutsceneVerdict(fx) || storms.eligible) {
        return Fail(2, "the Song of Storms cutscene is already skipped with rando disarmed - the armed probes would be "
                       "vacuous");
    }

    ArmShipInit(true);
    SetCheck(RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS, true, false);
    if (StormsCutsceneVerdict(fx) || !storms.eligible) {
        return Fail(5, "first Song of Storms trigger: the cutscene was not skipped or the check not armed");
    }
    SetCheck(RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS, true, true);
    const bool played = StormsCutsceneVerdict(fx);
    if (storms.eligible) {
        return Fail(30, "Song of Storms re-trigger (tomb room re-entered): RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS was "
                        "armed again although it was already obtained this cycle (#1220)");
    }
    if (played) {
        return Fail(31, "Song of Storms re-trigger: the vanilla song cutscene played; it must stay skipped under "
                        "rando (#1220)");
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

// ---- W6-MINIGAMES (#1236): every leg again with every minigame row on -------
// The single exe links 2Ship's minigame TUs and SoH's Enhancements > Minigames page writes their keys. Each one only
// moves a win condition (a lower target score, an instant win, a skipped race) and lets the vanilla actor run its own
// reward branch, so a rando check's item must still come from the hooks above, once, and never a vanilla reward in its
// place. This pass proves the part of that a ROM-free row can reach:
//   1. arming every minigame key (a one-point target, Always, Skip) arms each TU's own hook exactly once (non-vacuity:
//      a pass over unlinked TUs would prove nothing);
//   2. it adds no hook on a reward path: VB_GIVE_ITEM_FROM_OFFER, which every hook above rides, and the unkeyed
//      ShouldVanillaBehavior and OnFlagSet registries (CheckQueue arms a check on its flag) do not grow;
//   3. every leg of the row gives the same verdicts with the rows on;
//   4. clearing the keys takes every hook they armed back out.
// That the shortcut win then reaches the hooked offer in a real scene is the operator playtest in the PR.
template <typename H> size_t MinigameSettledForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

template <typename H> size_t MinigameSettledUnkeyed() {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    return S2H::GameHooks::Registry<H>::functions.size();
}

struct MinigameRowOn {
    const char* key;
    int32_t on;
};

// The most generous value of each key: the lowest target, Always (2) for the Doggy Race, Skip (2) for the Goron Race.
const MinigameRowOn kMinigameRowsOn[] = {
    { "gEnhancements.Minigames.BombersHideAndSeek", 1 },
    { "gEnhancements.Minigames.SwordsmanSchoolScore", 1 },
    { "gEnhancements.Minigames.HoneyAndDarlingDay1", 1 },
    { "gEnhancements.Minigames.HoneyAndDarlingDay2", 1 },
    { "gEnhancements.Minigames.HoneyAndDarlingDay3", 1 },
    { "gEnhancements.Minigames.TownArcheryScore", 1 },
    { "gEnhancements.Minigames.SwampArcheryScore", 1000 },
    { "gEnhancements.Minigames.RomaniTargetPractice", 1 },
    { "gEnhancements.Minigames.AlwaysWinDoggyRace", 2 },
    { "gEnhancements.Minigames.CuccoShackCuccoCount", 1 },
    { "gEnhancements.Minigames.SkipHorseRace", 1 },
    { "gEnhancements.Minigames.BeaverRaceRingsCollected", 1 },
    { "gEnhancements.Minigames.SkipLittleBeaver", 1 },
    { "gEnhancements.DifficultyOptions.GoronRace", 2 },
    { "gEnhancements.Minigames.BoatArcheryScore", 1 },
    { "gEnhancements.Minigames.BoatArcheryHealth", 1 },
    { "gEnhancements.Minigames.BoatArcheryInvincible", 1 },
    { "gEnhancements.Minigames.TreasureChestShopShowFullMaze", 1 },
    { "gEnhancements.Minigames.MarkShootingGalleryOctoroks", 1 },
};

struct MinigameBucket {
    const char* what;
    size_t (*count)();
    size_t armed; // the exact growth with every row on
};

// One bucket per TU (attributed by registry content), and the reward-path registries, which must not move.
const MinigameBucket kMinigameBuckets[] = {
    { "ShouldActorUpdate[EN_BOMJIMB] (Timesavers/BombersHideAndSeek.cpp)",
      []() { return MinigameSettledForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_BOMJIMB); }, 1 },
    { "ShouldActorUpdate[EN_KENDO_JS] (Minigames/SwordsmanSchool.cpp)",
      []() { return MinigameSettledForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_KENDO_JS); }, 1 },
    { "VB_HONEY_AND_DARLING_MINIGAME_FINISH (Minigames/HoneyAndDarling.cpp)",
      []() {
          return MinigameSettledForId<GameInteractor::ShouldVanillaBehavior>(VB_HONEY_AND_DARLING_MINIGAME_FINISH);
      },
      1 },
    { "ShouldActorUpdate[EN_SYATEKI_MAN] (Minigames/Archery.cpp, Town and Swamp)",
      []() { return MinigameSettledForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_SYATEKI_MAN); }, 2 },
    { "VB_KOUME_TAKE_DAMAGE (Minigames/Archery.cpp, Invincible)",
      []() { return MinigameSettledForId<GameInteractor::ShouldVanillaBehavior>(VB_KOUME_TAKE_DAMAGE); }, 1 },
    { "VB_WIN_ROMANI_PRACTICE (Minigames/RomaniTargetPractice.cpp)",
      []() { return MinigameSettledForId<GameInteractor::ShouldVanillaBehavior>(VB_WIN_ROMANI_PRACTICE); }, 1 },
    { "VB_DOGGY_RACE_SET_MAX_SPEED (Minigames/AlwaysWinDoggyRace.cpp)",
      []() { return MinigameSettledForId<GameInteractor::ShouldVanillaBehavior>(VB_DOGGY_RACE_SET_MAX_SPEED); }, 1 },
    { "ShouldActorUpdate[EN_HS] (Minigames/CuccoShackCuccoCount.cpp)",
      []() { return MinigameSettledForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_HS); }, 1 },
    { "VB_PLAY_TRANSITION_CS (Minigames/SkipHorseRace.cpp, DifficultyOptions/GoronRace.cpp at Skip)",
      []() { return MinigameSettledForId<GameInteractor::ShouldVanillaBehavior>(VB_PLAY_TRANSITION_CS); }, 2 },
    { "ShouldActorUpdate[EN_AZ] (Minigames/BeaverRace.cpp, rings and speed-up)",
      []() { return MinigameSettledForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_AZ); }, 2 },
    { "OnActorUpdate[OBJ_TAKARAYA_WALL] (Minigames/TreasureChestShopFullMaze.cpp)",
      []() { return MinigameSettledForId<GameInteractor::OnActorUpdate>(ACTOR_OBJ_TAKARAYA_WALL); }, 1 },
    { "OnActorDraw[EN_SYATEKI_OKUTA] (Minigames/MarkShootingGalleryOctoroks.cpp)",
      []() { return MinigameSettledForId<GameInteractor::OnActorDraw>(ACTOR_EN_SYATEKI_OKUTA); }, 1 },
    { "VB_GIVE_ITEM_FROM_OFFER (the reward path: must not move)",
      []() { return MinigameSettledForId<GameInteractor::ShouldVanillaBehavior>(VB_GIVE_ITEM_FROM_OFFER); }, 0 },
    { "ShouldVanillaBehavior, unkeyed (must not move)",
      []() { return MinigameSettledUnkeyed<GameInteractor::ShouldVanillaBehavior>(); }, 0 },
    { "OnFlagSet, unkeyed (must not move)", []() { return MinigameSettledUnkeyed<GameInteractor::OnFlagSet>(); }, 0 },
};

void SetMinigameRows(bool on) {
    for (const auto& row : kMinigameRowsOn) {
        if (on) {
            CVarSetInteger(row.key, row.on);
        } else {
            CVarClear(row.key);
        }
        ShipInit::Init(row.key);
    }
}

/** Runs @p legs on a fresh fixture with every minigame row on (see above). Returns 0, 40 (a TU's hook did not arm
 *  exactly once, or a reward-path registry moved), 41 (a key left a hook behind), or the legs' own code. Leaves every
 *  key cleared. */
template <typename Legs> int RunWithMinigameRowsOn(const char* name, Legs legs) {
    SetMinigameRows(false); // from MM's defaults, whatever an earlier row of a shared process left
    std::vector<size_t> before;
    for (const auto& bucket : kMinigameBuckets) {
        before.push_back(bucket.count());
    }
    SetMinigameRows(true);
    int rc = 0;
    for (size_t i = 0; i < std::size(kMinigameBuckets) && rc == 0; i++) {
        const size_t now = kMinigameBuckets[i].count();
        if (now != before[i] + kMinigameBuckets[i].armed) {
            rc = Fail(40,
                      "%s, minigame rows on: %s went %zu -> %zu, expected +%zu. Either a minigame TU is not in the "
                      "link (its row on SoH's Minigames page would do nothing) or turning a minigame row on adds a "
                      "hook on the reward path, where a shortcut win could hand a vanilla reward in place of the "
                      "check's item (#1236)",
                      name, kMinigameBuckets[i].what, before[i], now, kMinigameBuckets[i].armed);
        }
    }
    if (rc == 0) {
        HookFixture fx;
        rc = fx.Ok() ? legs(fx) : Fail(1, "could not allocate the fixtures");
        if (rc != 0) {
            printf("[TEST]       (with every minigame row on, #1236)\n");
        }
    }
    SetMinigameRows(false);
    for (size_t i = 0; i < std::size(kMinigameBuckets) && rc == 0; i++) {
        const size_t now = kMinigameBuckets[i].count();
        if (now != before[i]) {
            rc = Fail(41, "%s: %s went %zu -> %zu after every minigame row was cleared; a key left its hook behind",
                      name, kMinigameBuckets[i].what, before[i], now);
        }
    }
    return rc;
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
    if (rc == 0) {
        rc = RunWithMinigameRowsOn("mm-gallery-rupee-stand-in", [](HookFixture& fx) { return RunGalleryLegs(fx); });
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
    if (rc == 0) {
        rc = RunWithMinigameRowsOn("mm-doggy-race-reward", [](HookFixture& fx) { return RunDoggyLegs(fx); });
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-doggy-race-reward: PASS (first win hijacked with no extra rupee, a spent check keeps the "
           "vanilla Red Rupee and is not re-armed, an unshuffled check keeps its heart piece)\n");
    return 0;
}

extern "C" int MM_MinigameRepeatGuards_RunHeadless(void) {
    printf("[TEST] mm-minigame-repeat-guards: the Deku Playground, the lab fish and Flat's tomb arm their check once "
           "per cycle (#1220)\n");
    if (!BringUpOk("mm-minigame-repeat-guards")) {
        return 1;
    }
    const int32_t savedStory = CVarGetInteger(kStoryCVar, 0);
    if (savedStory != 0) {
        CVarSetInteger(kStoryCVar, 0);
    }
    int rc;
    {
        HookFixture fx;
        if (!fx.Ok()) {
            rc = Fail(1, "could not allocate the fixtures");
        } else {
            rc = RunPlaygroundLegs(fx);
            if (rc == 0) {
                rc = RunFishLegs(fx);
            }
            if (rc == 0) {
                rc = RunStormsLegs(fx);
            }
        }
    }
    if (rc == 0) {
        rc = RunWithMinigameRowsOn("mm-minigame-repeat-guards", [](HookFixture& fx) {
            int legsRc = RunPlaygroundLegs(fx);
            if (legsRc == 0) {
                legsRc = RunFishLegs(fx);
            }
            if (legsRc == 0) {
                legsRc = RunStormsLegs(fx);
            }
            return legsRc;
        });
    }
    // The fixture put the SaveContext image back; re-run the ShipInit drive
    // under the restored saveType, as mm-song-teacher-hooks does.
    ShipInit::Init("IS_RANDO");
    if (savedStory != 0) {
        CVarSetInteger(kStoryCVar, savedStory);
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-minigame-repeat-guards: PASS (playground, lab fish and Song of Storms arm their check only "
           "while it is open this cycle; repeats keep the vanilla Purple Rupee, nothing, and a skipped cutscene)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
