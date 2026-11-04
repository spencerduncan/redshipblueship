/**
 * @file mm_clock_shuffle_first_arrival_test.cpp
 * ROM-free, display-free lock for #1415's first defect: with Shuffle Time on, a
 * half-day found in Ocarina of Time is owned when Majora's Mask's arrival picks
 * the cycle's starting half-day. CTest label "redship", row
 * MMClockShuffleFirstArrival in CMake/SingleExecutable.cmake, dispatch
 * "mm-clock-shuffle-first-arrival" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#1415). A paired file's MM half is created with MM's
 * new-file clock (day 0, 05:59). On the first arrival MM_Play_ConsumeStartupEntrance
 * redeems the shared items (MM_ConsumeSharedItems), and with no PlayState yet
 * MM_ForeignItem_Give queues every give for the first gameplay frame. The
 * arrival's OnSaveLoad re-dispatch then runs ClockShuffle::OnFileLoad, whose
 * CorrectInitialTime moves a day-0 05:59 clock to the earliest owned half-day.
 * A half-day crossed from Hyrule was not owned yet at that point, so a file
 * that started on Day 3 with Day 2 found in Hyrule began its first MM cycle at
 * Dawn of the Third Day, and nothing moved the clock back. OoTMM writes the
 * half-day at the pickup (addItemClockMm) before Play_FixupSpawnTime reads it;
 * the fix writes the flag at the redemption and still queues the give for its
 * toast (ForeignItemsSingleExe.cpp, OwnCrossedHalfDay).
 *
 * SEAMS (production paths only). MM_Rando_InitCore registers Rando::Init's
 * OnSaveLoad handler, as every real boot does before Play. Each leg freezes an
 * MM half (Combo_FreezeState), records what crossed
 * (Combo_RecordSharedItemCrossing for a pickup in Hyrule, Combo_SubmitSourcedGrant
 * for a netplay grant), runs the boot chain's OnSaveLoad against MM's vanilla
 * bootstrap (which takes the IS_RANDO hooks down, asserted), then drives the
 * real arrival: Combo_SetStartupEntrance + MM_Play_ConsumeStartupEntrance with
 * MM_gPlayState NULL, as mm_rando_gen_test.cpp's switch-entry row does. The
 * first gameplay frame's drain is MM_ForeignItem_TestFlushWithStandInPlay,
 * which admits the six half-day items because their give reads no play state.
 * Toasts are read from the overlay's test record.
 *
 * NOT STAGED. No paired OoT world is live, so MM_Rando_GateCrossGameArrival logs
 * "skipped-because-no-paired-oot-world" and the hydrate half returns early;
 * neither reads or writes the clock or the half-day flags. A .redsave write and
 * reload is not driven either: leg 8 stages the reloaded half directly.
 *
 * LEGS. Legs 1-8 freeze a rando save in the RANDOM clock mode, isOwlSave
 * clear. "day N" is gSaveContext.save.day; DayTelop adds one, so day 1 at 05:59
 * is Dawn of the Second Day.
 *   1  Control: Day 3 owned, nothing crossed, day 0 05:59 -> day 2 05:59. Shows
 *      the re-dispatch reaches CorrectInitialTime.
 *   2  The report: Day 3 owned, Day 2 crossed. Right after the arrival, before
 *      any drain, Day 2 is owned (and Night 1, which `riId - RI_TIME_DAY_1`
 *      would name, is not) and the cycle starts on day 1 05:59. Before the fix:
 *      Day 2 not owned, day 2 05:59.
 *   3  The first gameplay frame's drain gives the queued Day 2: the clock does
 *      not move and a crossing pickup is not toasted (#878).
 *   4  Night 3 crossed with Day 3 owned: owned at once, and the start stays on
 *      day 2 05:59.
 *   5  A mid-cycle half (day 3, 07:00) with Day 2 crossed keeps its time: no
 *      owned half-day rewinds a running cycle.
 *   6  A sourced Time (Day 1) grant: owned at once, the cycle starts on day 0
 *      05:59, the arrival toasts nothing and the drain toasts exactly once.
 *      Before the fix: day 2 05:59.
 *   7  Shuffle Time off: a crossed Day 2 leaves day 0 05:59 and ClockShuffle
 *      stays disarmed.
 *   8  The reload after a Song of Time: the half was committed at day 0 05:59
 *      with Days 2 and 3 owned; Night 1 crosses and the cycle starts at the
 *      Night of the First Day (day 1, 18:00). Before the fix: day 1 05:59.
 *
 * PROGRESSIVE TIME (#1437). Time Progression ships Ascending (OoTMM's
 * progressiveClocks), and a Progressive Time may now be placed in an OoT check
 * (RSBS_GIVECAP_CLOCKS_PROGRESSIVE). MM_ForeignItem_Give resolves it at the
 * redemption into the next half-day in the world's order and queues that
 * half-day (ResolveCrossedClock). These legs freeze the half in a progressive
 * mode; each start owns what StartingItems.cpp grants (Day 1 Ascending, Night 3
 * Descending).
 *   9  Ascending, Day 1 owned, one Progressive Time crossed: Night 1 is owned
 *      right after the arrival, the queue holds Time (Night 1), the start stays
 *      day 0 05:59, and the drain owns nothing more (no second resolution).
 *      Before the fix: nothing owned until the drain, and the drain refused the
 *      progressive id (-1).
 *  10  Ascending, two crossed in one arrival: Night 1 and Day 2 in slot order.
 *  11  Descending, Night 3 owned, one crossed: Day 3 is owned when the start is
 *      picked, so the cycle starts at Dawn of the Third Day (day 2 05:59).
 *      Before the fix: the Night of the Third Day (day 3, 18:00).
 *  12  Ascending with all six owned: the surplus copy resolves to RI_JUNK, owns
 *      nothing new and its drain gives nothing.
 *  13  The give order itself, no arrival: seven gives of Progressive Time hand
 *      out Day 1 .. Night 3 then junk Ascending, and the reverse Descending.
 *   Cleanup: the pending queue, the crossings, gComboCtx, the frozen and
 *   startup slots, the cross-game latches, MM_gPlayState, gSceneSeqState and
 *   the SaveContext image come back, and the hooks are taken down against a
 *   vanilla bootstrap, so the row is order-free under --test all (it must stay
 *   after mm-registrar-coverage, which needs MM_Rando_Init not yet run).
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The arrival, the combo context and S2H::GameHooks this row drives exist only
 * in the single-exe build. test_runner.cpp declares this unconditionally.
 */
extern "C" int MM_ClockShuffleFirstArrival_RunHeadless(void) {
    printf("[TEST] mm-clock-shuffle-first-arrival: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/MiscBehavior/ClockShuffle.h"
#include "2s2h/Rando/Rando.h"
#include "mm_game_hooks.h"

// src/common. Outside extern "C": each header manages its own linkage.
#include "context.h"
#include "notification_bridge.h"
#include "shared_items.h"

extern "C" {
#include "variables.h"
#include "functions.h"

// games/mm/src/code/z_play.c: the arrival under test.
void MM_Play_ConsumeStartupEntrance(void);
// games/mm/2s2h/GameExports_SingleExe.cpp: the registrars and Rando::Init.
void MM_Rando_InitCore(void);
// src/common/entrance.cpp: the startup slot and the cross-game latches.
void Combo_SetStartupEntrance(uint16_t entrance);
void Combo_ClearStartupEntrance(void);
void Combo_NoteCrossGameArrival(const char* gameId);
bool Combo_IsCrossGameHalf(const char* gameId);
void Combo_ClearCrossGameHalves(void);
// games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp: the deferred give's test surface.
int MM_ForeignItem_TestPendingCount(void);
uint16_t MM_ForeignItem_TestPendingAt(int index);
void MM_ForeignItem_TestResetPending(void);
int MM_ForeignItem_TestFlushWithStandInPlay(void);
}

namespace {

#define FA_ASSERT(cond, code, ...)                    \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL(%d): ", (code));      \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            return (code);                            \
        }                                             \
    } while (0)

// ENTRANCE(SOUTH_CLOCK_TOWN, 0), the OoT -> MM arrival spawn.
constexpr uint16_t kArrival = 0xD800;
// MM_Sram_InitNewSave's clock: day 0 at CLOCK_TIME(6, 0) - 1 (0x3FFF), the new
// cycle CorrectInitialTime acts on.
constexpr u16 kNewCycleTime = (u16)(CLOCK_TIME(6, 0) - 1);
constexpr u16 kDusk = (u16)CLOCK_TIME(18, 0);
constexpr u16 kSevenAm = (u16)CLOCK_TIME(7, 0);
// The netplay relay's source key for the sourced-grant leg.
constexpr uint32_t kSourceKey = 0x1415u;

constexpr u8 kD1 = 1u << Rando::ClockItems::HALF_DAY1_DAY;
constexpr u8 kN1 = 1u << Rando::ClockItems::HALF_DAY1_NIGHT;
constexpr u8 kD2 = 1u << Rando::ClockItems::HALF_DAY2_DAY;
constexpr u8 kD3 = 1u << Rando::ClockItems::HALF_DAY3_DAY;
constexpr u8 kN3 = 1u << Rando::ClockItems::HALF_DAY3_NIGHT;
constexpr u8 kAllSix = 0x3F;

// The toast record's count just before the current leg's arrival.
int sToastMark = 0;

// Settled counts, as mm_clock_shuffle_final_hours_test.cpp reads them:
// S2H::GameHooks unregistration is deferred until the next Execute of that hook
// type, and the COND_* macros unregister-then-register on every drive.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

/** ClockShuffle's EnTest4 pre-update hook: 1 while ClockShuffle::OnFileLoad
 *  armed it for a Shuffle Time rando save, 0 otherwise. */
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

u8 OwnedHalfDays() {
    return Rando::ClockItems::GetAllOwnedHalfDaysMask();
}

int ToastsSinceMark() {
    const int total = OoT_Notification_EmittedCountForTest();
    for (int i = sToastMark; i < total; i++) {
        char toast[256] = { 0 };
        OoT_Notification_EmittedAtForTest(i, toast, sizeof(toast));
        printf("[TEST] mm-clock-shuffle-first-arrival: toasted \"%s\"\n", toast);
    }
    return total - sToastMark;
}

void PrintClock(const char* leg, const char* when) {
    printf("[TEST] mm-clock-shuffle-first-arrival: %s: %s: day=%d time=0x%04X owned=0x%02X pending=%d\n", leg, when,
           (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time, (unsigned)OwnedHalfDays(),
           MM_ForeignItem_TestPendingCount());
}

/** One arrival's inputs. `item` RI_NONE means nothing crossed. */
struct Arrival {
    const char* leg;
    bool clockShuffle;
    u8 owned;
    s32 day;
    u16 time;
    RandoItemId item;
    bool sourced;
    u16 threeDayResetCount;
    // #1437: the frozen half's Time Progression, how many copies of `item`
    // crossed, and what the queue must hold in slot order (RI_NONE: `item`).
    int32_t mode = RO_CLOCK_SHUFFLE_RANDOM;
    int copies = 1;
    RandoItemId queued[2] = { RI_NONE, RI_NONE };
};

/**
 * Freeze the MM half, record the crossing, run the boot chain's OnSaveLoad and
 * the real arrival. Returns 0, or `code` with the reason printed.
 */
int Arrive(const Arrival& a, int code) {
    // No live pairing, no crossings, no frozen half, nothing queued.
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();

    // The MM half as the creation authored it (MM_Sram_InitNewSave's new file
    // under the paired world's options), or as the player left it.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = a.clockShuffle ? RO_GENERIC_ON : RO_GENERIC_OFF;
    RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] = (uint32_t)a.mode;
    gSaveContext.save.isOwlSave = false;
    gSaveContext.save.day = a.day;
    gSaveContext.save.eventDayCount = a.day;
    gSaveContext.save.time = a.time;
    gSaveContext.save.saveInfo.playerData.threeDayResetCount = a.threeDayResetCount;
    SetOwnedHalfDays(a.owned);
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));

    for (int copy = 0; a.item != RI_NONE && copy < a.copies; copy++) {
        if (a.sourced) {
            FA_ASSERT(Combo_SubmitSourcedGrant(kSourceKey + (uint32_t)copy, 1, GAME_MM, (uint16_t)a.item) ==
                          RSBS_GRANT_ACCEPTED,
                      code, "%s: the sourced grant of RI id %u was not accepted", a.leg, (unsigned)a.item);
        } else {
            FA_ASSERT(Combo_RecordSharedItemCrossing(GAME_MM, (uint16_t)a.item) >= 0, code,
                      "%s: the crossing of RI id %u was not recorded", a.leg, (unsigned)a.item);
        }
    }

    // The boot chain: MM_SaveContext_Init's wipe, then TitleSetup's vanilla
    // bootstrap and its OnSaveLoad, which takes every IS_RANDO hook down.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    FA_ASSERT(Test4Registrants() == 0, code,
              "%s: the boot chain's OnSaveLoad left ClockShuffle's EnTest4 hook armed; the arrival's re-arm below "
              "would prove nothing",
              a.leg);

    // The arrival, before `MM_gPlayState = this` (z_play.c).
    sToastMark = OoT_Notification_EmittedCountForTest();
    Combo_SetStartupEntrance(kArrival);
    MM_gPlayState = nullptr;
    MM_Play_ConsumeStartupEntrance();

    FA_ASSERT(gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO &&
                  RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] == (a.clockShuffle ? RO_GENERIC_ON : RO_GENERIC_OFF),
              code, "%s: the arrival did not restore the frozen rando half", a.leg);
    FA_ASSERT(Test4Registrants() == (a.clockShuffle ? 1u : 0u), code,
              "%s: after the arrival ClockShuffle's EnTest4 hook has %zu registrants, expected %u; the OnSaveLoad "
              "re-dispatch did not run ClockShuffle::OnFileLoad against the restored half",
              a.leg, Test4Registrants(), a.clockShuffle ? 1u : 0u);
    FA_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0, code,
              "%s: the arrival left an MM item unredeemed", a.leg);
    const int expectedPending = (a.item != RI_NONE) ? a.copies : 0;
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == expectedPending, code,
              "%s: %d gives queued for the first gameplay frame, expected %d (the give still waits for its toast)",
              a.leg, MM_ForeignItem_TestPendingCount(), expectedPending);
    for (int i = 0; i < expectedPending; i++) {
        const RandoItemId want = (i < 2 && a.queued[i] != RI_NONE) ? a.queued[i] : a.item;
        FA_ASSERT(MM_ForeignItem_TestPendingAt(i) == (uint16_t)want, code,
                  "%s: queue slot %d holds RI id %u, expected %u (#1437: a crossed Progressive Time is queued as "
                  "the half-day it resolved to)",
                  a.leg, i, (unsigned)MM_ForeignItem_TestPendingAt(i), (unsigned)want);
    }
    FA_ASSERT(OoT_Notification_EmittedCountForTest() == sToastMark, code,
              "%s: the arrival itself toasted; a give's toast waits for the first gameplay frame", a.leg);
    PrintClock(a.leg, "after the arrival");
    return 0;
}

/** The first gameplay frame's drain. Returns 0, or `code`. */
int Drain(const char* leg, s32 day, u16 time, int code, int expectedGiven = 1) {
    const int given = MM_ForeignItem_TestFlushWithStandInPlay();
    PrintClock(leg, "after the first frame's drain");
    FA_ASSERT(given == expectedGiven, code,
              "%s: the stand-in drain returned %d, expected the %d queued half-day(s) (-1 means the queue holds an id "
              "the drain does not admit, such as an unresolved Progressive Time, and this leg is vacuous)",
              leg, given, expectedGiven);
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 0, code, "%s: the drain left a give queued", leg);
    FA_ASSERT(gSaveContext.save.day == day && gSaveContext.save.time == time, code,
              "%s: the drain moved the clock to day %d 0x%04X (expected day %d 0x%04X)", leg,
              (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time, (int)day, (unsigned)time);
    return 0;
}

bool ClockIs(s32 day, u16 time) {
    return gSaveContext.save.day == day && gSaveContext.save.time == time;
}

int RunLegs() {
    // ---- Leg 1: control, nothing crossed ---------------------------------
    {
        const Arrival a = { "leg 1", true, kD3, 0, kNewCycleTime, RI_NONE, false, 0 };
        int rc = Arrive(a, 1);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == kD3, 2, "leg 1: the arrival changed the owned half-days to 0x%02X",
                  (unsigned)OwnedHalfDays());
        FA_ASSERT(ClockIs(2, kNewCycleTime), 3,
                  "leg 1: Day 3 owned, day 0 05:59 -> expected day 2 05:59 (CorrectInitialTime), got day %d 0x%04X; "
                  "the legs below would not observe the start being picked",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
    }

    // ---- Legs 2 and 3: Day 2 crossed, then the first frame ----------------
    {
        const Arrival a = { "leg 2", true, kD3, 0, kNewCycleTime, RI_TIME_DAY_2, false, 0 };
        int rc = Arrive(a, 4);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD2 | kD3), 5,
                  "leg 2: right after the arrival the file owns 0x%02X, expected Days 2 and 3 (0x%02X): the crossed "
                  "Day 2 is not owned when the cycle's start is picked (#1415)",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD2 | kD3));
        FA_ASSERT(ClockIs(1, kNewCycleTime), 6,
                  "leg 2: Day 3 owned and Day 2 found in Hyrule: expected the cycle to start at Dawn of the Second "
                  "Day (day 1 05:59), got day %d 0x%04X (#1415)",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 3", 1, kNewCycleTime, 7);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD2 | kD3), 8, "leg 3: after the drain the file owns 0x%02X",
                  (unsigned)OwnedHalfDays());
        FA_ASSERT(ToastsSinceMark() == 0, 9,
                  "leg 3: a half-day picked up in Hyrule was toasted at the crossing; it was presented at its pickup "
                  "(#878)");
    }

    // ---- Leg 4: Night 3 crossed, the start stays on Day 3 -----------------
    {
        const Arrival a = { "leg 4", true, kD3, 0, kNewCycleTime, RI_TIME_NIGHT_3, false, 0 };
        int rc = Arrive(a, 10);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD3 | kN3), 11,
                  "leg 4: right after the arrival the file owns 0x%02X, expected Day 3 and Night 3 (0x%02X)",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD3 | kN3));
        FA_ASSERT(ClockIs(2, kNewCycleTime), 12,
                  "leg 4: a crossed Night 3 is later than Day 3 and must leave the start on day 2 05:59, got day %d "
                  "0x%04X",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 4", 2, kNewCycleTime, 13);
        if (rc != 0) {
            return rc;
        }
    }

    // ---- Leg 5: a running cycle is not rewound ----------------------------
    {
        const Arrival a = { "leg 5", true, kD3, 3, kSevenAm, RI_TIME_DAY_2, false, 0 };
        int rc = Arrive(a, 14);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD2 | kD3), 15, "leg 5: right after the arrival the file owns 0x%02X",
                  (unsigned)OwnedHalfDays());
        FA_ASSERT(ClockIs(3, kSevenAm), 16,
                  "leg 5: a half frozen mid-cycle (day 3, 07:00) must keep its time when an earlier half-day "
                  "crosses, got day %d 0x%04X",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 5", 3, kSevenAm, 17);
        if (rc != 0) {
            return rc;
        }
    }

    // ---- Leg 6: a sourced Day 1 grant --------------------------------------
    {
        const Arrival a = { "leg 6", true, kD3, 0, kNewCycleTime, RI_TIME_DAY_1, true, 0 };
        int rc = Arrive(a, 18);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD1 | kD3), 19,
                  "leg 6: right after the arrival the file owns 0x%02X, expected Days 1 and 3 (0x%02X)",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD1 | kD3));
        FA_ASSERT(ClockIs(0, kNewCycleTime), 20,
                  "leg 6: a granted Day 1 must start the cycle at Dawn of the First Day (day 0 05:59), got day %d "
                  "0x%04X (#1415)",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 6", 0, kNewCycleTime, 21);
        if (rc != 0) {
            return rc;
        }
        const int toasts = ToastsSinceMark();
        FA_ASSERT(toasts == 1, 22,
                  "leg 6: a sourced grant was never presented at a pickup, so its first-frame give toasts exactly "
                  "once; got %d toast(s)",
                  toasts);
    }

    // ---- Leg 7: Shuffle Time off -------------------------------------------
    {
        const Arrival a = { "leg 7", false, kD3, 0, kNewCycleTime, RI_TIME_DAY_2, false, 0 };
        int rc = Arrive(a, 23);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(ClockIs(0, kNewCycleTime), 24,
                  "leg 7: with Shuffle Time off the arrival must leave the new-file clock (day 0 05:59), got day %d "
                  "0x%04X",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 7", 0, kNewCycleTime, 25);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD2 | kD3), 26, "leg 7: after the drain the file owns 0x%02X",
                  (unsigned)OwnedHalfDays());
    }

    // ---- Leg 8: the reload after a Song of Time ---------------------------
    // The Song of Time committed the .redsave with the MM half at day 0 05:59
    // and Days 2 and 3 owned from earlier visits; the player reloads (a load
    // opens in Hyrule), finds Night 1 there and crosses.
    {
        const Arrival a = { "leg 8", true, kD2 | kD3, 0, kNewCycleTime, RI_TIME_NIGHT_1, false, 1 };
        int rc = Arrive(a, 27);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kN1 | kD2 | kD3), 28,
                  "leg 8: right after the arrival the file owns 0x%02X, expected Night 1, Day 2 and Day 3 (0x%02X)",
                  (unsigned)OwnedHalfDays(), (unsigned)(kN1 | kD2 | kD3));
        FA_ASSERT(ClockIs(1, kDusk), 29,
                  "leg 8: Night 1 is now the earliest owned half-day: expected the Night of the First Day (day 1, "
                  "18:00), got day %d 0x%04X (#1415)",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 8", 1, kDusk, 30);
        if (rc != 0) {
            return rc;
        }
    }

    // ---- Leg 9: Ascending, one Progressive Time crossed (#1437) -------------
    {
        const Arrival a = { "leg 9",
                            true,
                            kD1,
                            0,
                            kNewCycleTime,
                            RI_TIME_PROGRESSIVE,
                            false,
                            0,
                            RO_CLOCK_SHUFFLE_ASCENDING,
                            1,
                            { RI_TIME_NIGHT_1, RI_NONE } };
        int rc = Arrive(a, 32);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD1 | kN1), 33,
                  "leg 9: right after the arrival the file owns 0x%02X, expected Day 1 and Night 1 (0x%02X): a "
                  "Progressive Time found in Hyrule must be the next half-day, owned before the start is picked",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD1 | kN1));
        FA_ASSERT(ClockIs(0, kNewCycleTime), 34,
                  "leg 9: Day 1 owned, the cycle must start at Dawn of the First Day (day 0 05:59), got day %d 0x%04X",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 9", 0, kNewCycleTime, 35);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD1 | kN1), 36,
                  "leg 9: after the drain the file owns 0x%02X, expected 0x%02X: the drain resolved the crossed clock "
                  "a second time",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD1 | kN1));
        FA_ASSERT(ToastsSinceMark() == 0, 37,
                  "leg 9: a Progressive Time picked up in Hyrule was toasted at the crossing (#878)");
    }

    // ---- Leg 10: Ascending, two crossed in one arrival ----------------------
    {
        const Arrival a = { "leg 10",
                            true,
                            kD1,
                            0,
                            kNewCycleTime,
                            RI_TIME_PROGRESSIVE,
                            false,
                            0,
                            RO_CLOCK_SHUFFLE_ASCENDING,
                            2,
                            { RI_TIME_NIGHT_1, RI_TIME_DAY_2 } };
        int rc = Arrive(a, 38);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD1 | kN1 | kD2), 39,
                  "leg 10: two crossed Progressive Times own 0x%02X, expected Day 1, Night 1 and Day 2 (0x%02X) in "
                  "ascending order",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD1 | kN1 | kD2));
        rc = Drain("leg 10", 0, kNewCycleTime, 40, 2);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD1 | kN1 | kD2), 41, "leg 10: after the drain the file owns 0x%02X",
                  (unsigned)OwnedHalfDays());
    }

    // ---- Leg 11: Descending, the start moves to Day 3 -----------------------
    {
        const Arrival a = { "leg 11",
                            true,
                            kN3,
                            0,
                            kNewCycleTime,
                            RI_TIME_PROGRESSIVE,
                            false,
                            0,
                            RO_CLOCK_SHUFFLE_DESCENDING,
                            1,
                            { RI_TIME_DAY_3, RI_NONE } };
        int rc = Arrive(a, 42);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD3 | kN3), 43,
                  "leg 11: right after the arrival the file owns 0x%02X, expected Day 3 and Night 3 (0x%02X)",
                  (unsigned)OwnedHalfDays(), (unsigned)(kD3 | kN3));
        FA_ASSERT(ClockIs(2, kNewCycleTime), 44,
                  "leg 11: a Descending world with Day 3 found in Hyrule must start at Dawn of the Third Day (day 2 "
                  "05:59), got day %d 0x%04X: the start was picked before the crossed clock was owned",
                  (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time);
        rc = Drain("leg 11", 2, kNewCycleTime, 45);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == (kD3 | kN3), 46, "leg 11: after the drain the file owns 0x%02X",
                  (unsigned)OwnedHalfDays());
    }

    // ---- Leg 12: a surplus copy owns nothing --------------------------------
    {
        const Arrival a = { "leg 12",
                            true,
                            kAllSix,
                            0,
                            kNewCycleTime,
                            RI_TIME_PROGRESSIVE,
                            false,
                            0,
                            RO_CLOCK_SHUFFLE_ASCENDING,
                            1,
                            { RI_JUNK, RI_NONE } };
        int rc = Arrive(a, 47);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == kAllSix, 48, "leg 12: the surplus copy changed the owned half-days to 0x%02X",
                  (unsigned)OwnedHalfDays());
        rc = Drain("leg 12", 0, kNewCycleTime, 49);
        if (rc != 0) {
            return rc;
        }
        FA_ASSERT(OwnedHalfDays() == kAllSix, 50, "leg 12: after the drain the file owns 0x%02X",
                  (unsigned)OwnedHalfDays());
    }

    // ---- Leg 13: the give order, no arrival ---------------------------------
    {
        static const RandoItemId kAscending[] = { RI_TIME_DAY_1, RI_TIME_NIGHT_1, RI_TIME_DAY_2, RI_TIME_NIGHT_2,
                                                  RI_TIME_DAY_3, RI_TIME_NIGHT_3, RI_JUNK };
        static const RandoItemId kDescending[] = { RI_TIME_NIGHT_3, RI_TIME_DAY_3, RI_TIME_NIGHT_2, RI_TIME_DAY_2,
                                                   RI_TIME_NIGHT_1, RI_TIME_DAY_1, RI_JUNK };
        struct Order {
            int32_t mode;
            const RandoItemId* gives;
            const char* name;
        };
        const Order orders[] = { { RO_CLOCK_SHUFFLE_ASCENDING, kAscending, "Ascending" },
                                 { RO_CLOCK_SHUFFLE_DESCENDING, kDescending, "Descending" } };
        for (const Order& order : orders) {
            memset(&gSaveContext, 0, sizeof(gSaveContext));
            MM_Sram_InitNewSave();
            gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
            RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = RO_GENERIC_ON;
            RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] = (uint32_t)order.mode;
            SetOwnedHalfDays(0);
            for (int i = 0; i < 7; i++) {
                const RandoItemId got = Rando::ConvertItem(RI_TIME_PROGRESSIVE);
                FA_ASSERT(got == order.gives[i], 51,
                          "leg 13: %s give %d of Progressive Time resolved to RI id %u, expected %u", order.name, i + 1,
                          (unsigned)got, (unsigned)order.gives[i]);
                Rando::GiveItem(got);
            }
            FA_ASSERT(OwnedHalfDays() == kAllSix, 52, "leg 13: %s: seven gives own 0x%02X, expected all six",
                      order.name, (unsigned)OwnedHalfDays());
        }
    }
    return 0;
}

} // namespace

extern "C" int MM_ClockShuffleFirstArrival_RunHeadless(void) {
    printf("[TEST] mm-clock-shuffle-first-arrival: a half-day found in Ocarina of Time is owned when Majora's Mask's "
           "arrival picks the cycle's start (#1415)\n");

    auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
    auto savedCombo = std::make_unique<uint8_t[]>(sizeof(gComboCtx));
    memcpy(savedCombo.get(), &gComboCtx, sizeof(gComboCtx));
    PlayState* const savedPlay = MM_gPlayState;
    const u8 savedSceneSeqState = gSceneSeqState;
    const bool savedOoTHalf = Combo_IsCrossGameHalf("oot");
    const bool savedMMHalf = Combo_IsCrossGameHalf("mm");

    // Production registers Rando::Init's OnSaveLoad handler before any Play
    // (MM_Game_Init -> MM_Rando_Init). Once per process; a no-op when
    // mm-registrar-coverage already ran it under --test all.
    MM_Rando_InitCore();
    OoT_Notification_RecordForTest(1);

    int rc = RunLegs();

    // ---- Cleanup (pass and fail) -------------------------------------------
    MM_ForeignItem_TestResetPending();
    Combo_ClearSharedItemOutbox();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    // The hooks the last arrival armed come down the way the next boot chain
    // takes them down: OnSaveLoad against a vanilla bootstrap.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    const size_t left = Test4Registrants();
    OoT_Notification_RecordForTest(0);
    Combo_ClearCrossGameHalves();
    if (savedOoTHalf) {
        Combo_NoteCrossGameArrival("oot");
    }
    if (savedMMHalf) {
        Combo_NoteCrossGameArrival("mm");
    }
    memcpy(&gComboCtx, savedCombo.get(), sizeof(gComboCtx));
    MM_gPlayState = savedPlay;
    gSceneSeqState = savedSceneSeqState;
    memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));
    if (rc == 0 && left != 0) {
        printf("[TEST] FAIL(31): ShouldActorUpdate[ACTOR_EN_TEST4] kept %zu registrants after disarming; every later "
               "row in this process would inherit the hook\n",
               left);
        rc = 31;
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-clock-shuffle-first-arrival: PASS (a crossed half-day is owned before the arrival picks the "
           "start; the drain moves nothing and toasts only a sourced grant)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
