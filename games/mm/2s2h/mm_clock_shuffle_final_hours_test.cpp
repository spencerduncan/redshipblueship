/**
 * @file mm_clock_shuffle_final_hours_test.cpp
 * ROM-free, display-free lock for #1229: with Shuffle Time on, a half-day the
 * file does not own is never playable, and when no owned half-day is left the
 * moon crashes at the end of the last owned one (OoTMM's rule).
 * CTest label "redship", row MMClockShuffleFinalHours in
 * CMake/SingleExecutable.cmake, dispatch "mm-clock-shuffle-final-hours" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#1229). 2Ship's Rando/MiscBehavior/ClockShuffle.cpp kept a
 * "Final Hours" terminal window: when the owned half-days ran out, its EnTest4
 * pre-update hook jumped the clock to Day 3 at the "Final Hours Start Time"
 * (00:00 by default) instead of letting the moon fall, so a file without
 * Time (Night 3) played 00:00-06:00 of a half-day it does not own, with the
 * Clock Tower open. The same window was handed out by the ownership query the
 * Song of Double Time selector asks, by the Scarecrow's dance, and by the
 * time-left text. OoTMM has no such window: its EnTest4_TimeSkip targets Day 4
 * at 06:00 with nothing owned ahead, then steps the day back so vanilla's
 * dawn-of-Day-3 branch crashes the moon on that frame.
 *
 * SEAMS (production paths only). Armed with Rando::ClockShuffle::OnFileLoad(),
 * the call Rando.cpp's save-load handler makes. Driven through
 * S2H::GameHooks::ExecuteForID<ShouldActorUpdate>(ACTOR_EN_TEST4, ...), the ID
 * leg of MM_GameHooks_ExecuteShouldActorUpdate that z_actor.c dispatches
 * before EnTest4 updates; the VB legs go through MM_GameHooks_ExecuteVBShould,
 * which every MM GameInteractor_Should call site binds to.
 *
 * STAGING. The clock sits at 0xC000 - CLOCK_TIME(0, 1) (49107): the hook looks
 * one minute ahead, and 49107 + 45 is exactly 0xC000, the first night tick.
 * CLOCK_TIME(17, 59) is 49106 and its lookahead (49151) is still day, so
 * nothing would skip. The play state is a zeroed stand-in with a time speed,
 * an idle cutscene context, a zeroed Player (MM_Play_InCsMode reads it) and a
 * scene that is not on ClockShuffle's reload list.
 *
 * LEGS.
 *   0  Non-vacuity: shuffle off, OnFileLoad leaves ShouldActorUpdate
 *      [ACTOR_EN_TEST4] empty; armed, exactly one registrant.
 *   1  Control (green before the fix): Day 1 at the edge of night, Night 1
 *      unowned, Day 2 owned -> the dawn path lands on Day 2's 06:00 (day 1,
 *      the vanilla day-- trick).
 *   2  The report: Days 1-3 owned, Nights unowned, Day 3 at the edge of night.
 *      No Night 3 time may be handed out, and the clock lands on OoTMM's crash
 *      instant (day 3, 06:00, prevTime 05:59, daytimeIndex NIGHT), from which
 *      vanilla EnTest4_HandleEvents calls Interface_StartMoonCrash.
 *      Before the fix: day 3, 00:00, prevTime 0xFFD3, daytimeIndex 1.
 *   3  The ownership query and its description: Day 3 at 02:00 without Night 3
 *      is not owned, and is described as the Night of the Third Day.
 *   4  The Scarecrow's dance from Day 3 at noon has no owned half-day ahead:
 *      it must not land in Night 3 (before the fix: 00:00, respawnFlag -8).
 *      The clock stays where it is.
 *   5  The time-left text counts owned half-days only (no Final Hours term).
 *   6  The "Final Hours Start Time" row is retired: drawn dormant with the
 *      retirement reason, its stored byte kept and ignored.
 *   Cleanup: the options and save type come back, OnFileLoad runs again and
 *   must leave 0 registrants, then the SaveContext image and MM_gPlayState
 *   are restored, so the row is order-free under --test all.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * S2H::GameHooks and the single-exe dispatchers this row drives exist only in
 * the single-exe build. test_runner.cpp declares this unconditionally.
 */
extern "C" int MM_ClockShuffleFinalHours_RunHeadless(void) {
    printf("[TEST] mm-clock-shuffle-final-hours: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/MiscBehavior/ClockShuffle.h"
#include "2s2h/Rando/Rando.h"
#include "mm_game_hooks.h"

// src/common: the MM Randomizer page's descriptor table.
#include "combo_mm_options_view.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Test4/z_en_test4.h"
}

namespace {

#define FH_ASSERT(cond, code, ...)                    \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL(%d): ", (code));      \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            return (code);                            \
        }                                             \
    } while (0)

// The lookahead edge: one hook tick from here reaches 0xC000 (18:00).
constexpr u16 kEdgeOfNight = (u16)(0xC000 - CLOCK_TIME(0, 1));
constexpr u16 kDawn = (u16)CLOCK_TIME(6, 0);
constexpr u16 kDawnPrev = (u16)(CLOCK_TIME(6, 0) - CLOCK_TIME(0, 1));

constexpr u8 kD1 = 1u << Rando::ClockItems::HALF_DAY1_DAY;
constexpr u8 kD2 = 1u << Rando::ClockItems::HALF_DAY2_DAY;
constexpr u8 kD3 = 1u << Rando::ClockItems::HALF_DAY3_DAY;

// Settled counts: S2H::GameHooks unregistration is deferred until the next
// Execute of that hook type, and the COND_* macros unregister-then-register on
// every drive, so the raw maps can credit a hook already slated for removal.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

size_t Test4Registrants() {
    return SettledCountForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_TEST4);
}

/** The six clock bits, written directly (not through Flags_SetRandoInf). */
void SetOwnedHalfDays(u8 mask) {
    for (int i = 0; i < Rando::ClockItems::HALF_COUNT; i++) {
        const s32 flag = RANDO_INF_OBTAINED_CLOCK_DAY_1 + i;
        u16& word = gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4];
        const u16 bit = (u16)(1u << (flag & 0xF));
        word = (mask & (1u << i)) ? (u16)(word | bit) : (u16)(word & ~bit);
    }
}

void Arm(bool shuffleOn) {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = shuffleOn ? RO_GENERIC_ON : RO_GENERIC_OFF;
    RANDO_SAVE_OPTIONS[RO_CLOCK_TERMINAL_TIME] = 0; // the shipped default (00:00)
    Rando::ClockShuffle::OnFileLoad();
}

void StageClock(s32 day, u16 time) {
    gSaveContext.save.day = day;
    gSaveContext.save.eventDayCount = day;
    gSaveContext.save.time = time;
    gSaveContext.save.isNight = (time >= 0xC000 || time < kDawn);
}

void StageTest4(EnTest4* test4) {
    memset(test4, 0, sizeof(*test4));
    test4->actor.id = ACTOR_EN_TEST4;
    test4->daytimeIndex = THREEDAY_DAYTIME_DAY;
    test4->prevTime = kDawn;
}

bool DriveTest4(EnTest4* test4) {
    bool should = true;
    S2H::GameHooks::ExecuteForID<GameInteractor::ShouldActorUpdate>(ACTOR_EN_TEST4, &test4->actor, &should);
    return should;
}

bool InUnownedNight3(s32 day, u16 time) {
    return day == 3 && (time >= 0xC000 || time < kDawn);
}

int RunLegs(EnTest4* test4) {
    // ---- Leg 0: the gate (non-vacuity) -----------------------------------
    StageClock(1, kDawn);
    Arm(false);
    FH_ASSERT(Test4Registrants() == 0, 1,
              "ShouldActorUpdate[ACTOR_EN_TEST4] holds %zu registrants with Shuffle Time off; the legs below "
              "would not be attributable to ClockShuffle",
              Test4Registrants());
    Arm(true);
    FH_ASSERT(Test4Registrants() == 1, 2,
              "ShouldActorUpdate[ACTOR_EN_TEST4] holds %zu registrants with Shuffle Time on, expected exactly the "
              "ClockShuffle hook",
              Test4Registrants());

    // ---- Leg 1: control, the dawn path into the next owned half-day --------
    SetOwnedHalfDays(kD1 | kD2);
    StageClock(1, kEdgeOfNight);
    StageTest4(test4);
    FH_ASSERT(DriveTest4(test4), 3, "the ClockShuffle hook vetoed EnTest4's update");
    printf("[TEST] mm-clock-shuffle-final-hours: leg 1: day=%d time=0x%04X prevTime=0x%04X daytimeIndex=%d\n",
           (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time, (unsigned)test4->prevTime,
           (int)test4->daytimeIndex);
    FH_ASSERT(gSaveContext.save.day == 1 && gSaveContext.save.time == kDawn && test4->prevTime == kDawnPrev &&
                  test4->daytimeIndex == THREEDAY_DAYTIME_NIGHT,
              4,
              "Night 1 unowned, Day 2 owned: expected the dawn path (day 1, 06:00, prevTime 0x%04X, daytimeIndex "
              "0); the seam does not reach the real skip",
              (unsigned)kDawnPrev);

    // ---- Leg 2: no owned half-day left -> the moon falls (#1229) ----------
    SetOwnedHalfDays(kD1 | kD2 | kD3);
    StageClock(3, kEdgeOfNight);
    StageTest4(test4);
    FH_ASSERT(DriveTest4(test4), 5, "the ClockShuffle hook vetoed EnTest4's update");
    printf("[TEST] mm-clock-shuffle-final-hours: leg 2: day=%d time=0x%04X prevTime=0x%04X daytimeIndex=%d\n",
           (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time, (unsigned)test4->prevTime,
           (int)test4->daytimeIndex);
    FH_ASSERT(!InUnownedNight3(gSaveContext.save.day, gSaveContext.save.time), 6,
              "Day 3 ran out with Night 3 unowned and the clock was handed day %d 0x%04X, a Night 3 time the file "
              "does not own (the 2Ship Final Hours window, #1229)",
              (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
    FH_ASSERT(gSaveContext.save.day == 3 && gSaveContext.save.time == kDawn && test4->prevTime == kDawnPrev &&
                  test4->daytimeIndex == THREEDAY_DAYTIME_NIGHT,
              7,
              "expected OoTMM's crash instant (day 3, 06:00, prevTime 0x%04X, daytimeIndex NIGHT), from which "
              "EnTest4_HandleEvents starts the moon crash",
              (unsigned)kDawnPrev);

    // ---- Leg 3: the ownership query and its description ------------------
    const u16 twoAm = (u16)CLOCK_TIME(2, 0);
    FH_ASSERT(!Rando::ClockShuffle::IsTimeOwnedForClockShuffle(3, twoAm), 8,
              "IsTimeOwnedForClockShuffle(3, 02:00) is true without Night 3: the Song of Double Time selector "
              "would accept the Final Hours (#1229)");
    const std::string described = Rando::ClockShuffle::GetTimeDescriptionForMessage(3, twoAm);
    FH_ASSERT(described == "%rNight of the Third Day%w", 9,
              "Day 3 at 02:00 is described as '%s', expected the Night of the Third Day", described.c_str());

    // ---- Leg 4: the Scarecrow's dance with nothing owned ahead -----------
    StageClock(3, (u16)CLOCK_TIME(12, 0));
    gSaveContext.respawnFlag = 0;
    const bool vanillaDance = MM_GameHooks_ExecuteVBShould(VB_SCARECROW_DANCE_SET_TIME, true);
    printf("[TEST] mm-clock-shuffle-final-hours: leg 4: verdict=%d day=%d time=0x%04X respawnFlag=%d\n",
           (int)vanillaDance, (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time,
           (int)gSaveContext.respawnFlag);
    FH_ASSERT(!vanillaDance, 10,
              "the Scarecrow's vanilla time set ran with Shuffle Time on; it would land on 18:00 of Night 3");
    FH_ASSERT(!(gSaveContext.save.day == 3 && gSaveContext.save.time < kDawn), 11,
              "the Scarecrow's dance landed on day 3 at 0x%04X, a Night 3 time the file does not own (#1229)",
              (unsigned)gSaveContext.save.time);
    FH_ASSERT(gSaveContext.save.day == 3 && gSaveContext.save.time == (u16)CLOCK_TIME(12, 0), 12,
              "with no owned half-day ahead the Scarecrow's dance must leave the clock alone (day %d 0x%04X)",
              (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);

    // ---- Leg 5: the time-left text counts owned half-days only -----------
    u32 timeLeft = 0;
    const bool vanillaTimeLeft = MM_GameHooks_ExecuteVBShould(VB_TIME_UNTIL_MOON_CRASH_CALCULATION, true, &timeLeft);
    const u32 expectedLeft = 3u * 12u * (u32)CLOCK_TIME_HOUR + (u32)CLOCK_TIME_MINUTE;
    FH_ASSERT(!vanillaTimeLeft, 13, "the vanilla time-left calculation ran with Shuffle Time on");
    FH_ASSERT(timeLeft == expectedLeft, 14,
              "three owned half-days give a time-left total of 0x%X, expected 0x%X (36 hours, no Final Hours "
              "term)",
              (unsigned)timeLeft, (unsigned)expectedLeft);

    // ---- Leg 6: the Final Hours Start Time row is retired -----------------
    MM_RandoOptionsUi_Register();
    const ComboMMOptionDesc* row = Combo_MMOptionById((uint16_t)RO_CLOCK_TERMINAL_TIME);
    FH_ASSERT(row != nullptr, 15, "RO_CLOCK_TERMINAL_TIME has no descriptor row");
    FH_ASSERT(row->liveness == (uint8_t)COMBO_MM_LIVENESS_DORMANT && row->disabledReason != nullptr &&
                  strstr(row->disabledReason, "Retired") != nullptr,
              16,
              "the Final Hours Start Time row is liveness %u with reason '%s'; with the window gone it must be "
              "drawn retired",
              (unsigned)row->liveness, row->disabledReason ? row->disabledReason : "(null)");
    return 0;
}

} // namespace

extern "C" int MM_ClockShuffleFinalHours_RunHeadless(void) {
    printf("[TEST] mm-clock-shuffle-final-hours: with Shuffle Time on an unowned half-day is never playable; the "
           "moon falls at the end of the last owned one (#1229)\n");

    auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    Player* player = static_cast<Player*>(calloc(1, sizeof(Player)));
    EnTest4* test4 = static_cast<EnTest4*>(calloc(1, sizeof(EnTest4)));
    if (play == nullptr || player == nullptr || test4 == nullptr) {
        free(play);
        free(player);
        free(test4);
        printf("[TEST] FAIL(1): could not allocate the fixtures\n");
        return 1;
    }
    play->envCtx.sceneTimeSpeed = 3;
    play->csCtx.state = CS_STATE_IDLE;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
    play->sceneId = SCENE_00KEIKOKU; // Termina Field: not on ClockShuffle's reload list
    MM_gPlayState = play;
    for (int i = 0; i < (int)ARRAY_COUNT(gSaveContext.eventInf); i++) {
        gSaveContext.eventInf[i] = 0; // keeps the dog-race guard out of the way
    }
    gSaveContext.save.isOwlSave = false;

    int rc = RunLegs(test4);

    // ---- Cleanup (pass and fail) -------------------------------------------
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = RO_GENERIC_OFF;
    Rando::ClockShuffle::OnFileLoad();
    const size_t left = Test4Registrants();
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));
    free(play);
    free(player);
    free(test4);
    if (rc == 0 && left != 0) {
        printf("[TEST] FAIL(17): ShouldActorUpdate[ACTOR_EN_TEST4] kept %zu registrants after disarming; every later "
               "row in this process would inherit the hook\n",
               left);
        rc = 17;
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-clock-shuffle-final-hours: PASS (no unowned half-day is handed out; the moon falls at the end "
           "of the last owned one)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
