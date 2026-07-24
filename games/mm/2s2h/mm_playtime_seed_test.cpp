/**
 * @file mm_playtime_seed_test.cpp
 * ROM-free, display-free lock for the filePlaytime epoch-injection bug (#513).
 * CTest label "redship", row MMPlaytimeSeed in CMake/SingleExecutable.cmake,
 * dispatch "mm-playtime-seed" in src/common/test_runner.cpp.
 *
 * What was broken: SavingEnhancements_AdvancePlaytime accrues
 *   filePlaytime += now - lastTimeLog
 * where filePlaytime is a PERSISTED field (z64save.h ShipSaveInfo) and
 * lastTimeLog is runtime-only (ShipSaveContext, "not persisted"). lastTimeLog
 * is seeded only by RegisterSavingEnhancements' OnSaveLoad hook, which the
 * single exe elided entirely (#516; revival is Phase 2), and it is re-zeroed on
 * new-file / continue paths (z_sram_NES.c:1033/1265). So AdvancePlaytime — which
 * is called from plain C (z_sram_NES.c:2212, z_kaleido_scope_NES.c:3583)
 * regardless of any hook wiring — ran with lastTimeLog == 0 and wrote
 * `now - 0`, a whole Unix epoch (~1.7e9 s, ~56 years), into the saved playtime.
 *
 * The fix treats lastTimeLog == 0 as the seed: set it, accrue nothing this tick.
 * Robust whether or not the seeder runs, and it survives the re-zeroing.
 *
 * Three checks, none needing a display or ROM:
 *   1. The bug. fileCompletedAt == 0, lastTimeLog == 0, filePlaytime == 0. One
 *      AdvancePlaytime must leave filePlaytime near zero (the seed tick accrues
 *      nothing) and lastTimeLog non-zero. Pre-fix this wrote ~1.7e9; the check
 *      fails if filePlaytime exceeds a generous sane bound.
 *   2. Accrual still works. lastTimeLog == 1 (non-zero, ancient), so the guard
 *      must NOT suppress it — filePlaytime must grow. Proves the fix narrows the
 *      zero case only, rather than disabling playtime accrual outright.
 *   3. The completed-file gate is intact. fileCompletedAt != 0 must block all
 *      accrual, leaving a pre-stamped filePlaytime untouched.
 */

#include "global.h"

#include <cstdio>
#include <cstring>

namespace {

#define PLAYTIME_ASSERT(cond, code, msg)                                                \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// A filePlaytime above this is unreachable by honest accrual in a unit test and
// is the epoch-injection signature. ~1.7e9 s (the bug) is orders of magnitude
// past it; a real few-second test delta is far below it.
constexpr uint64_t kSaneMaxSeconds = 100000000ULL; // ~3.2 years

} // namespace

extern "C" void SavingEnhancements_AdvancePlaytime(void);

extern "C" int MM_PlaytimeSeed_RunHeadless(void) {
    // ---------------------------------------------------------------- 1
    // The bug: no prior observation. Seed, do not accrue the epoch.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.save.shipSaveInfo.fileCompletedAt = 0;
    gSaveContext.save.shipSaveInfo.filePlaytime = 0;
    gSaveContext.shipSaveContext.lastTimeLog = 0;

    SavingEnhancements_AdvancePlaytime();

    PLAYTIME_ASSERT(gSaveContext.save.shipSaveInfo.filePlaytime < kSaneMaxSeconds, 1,
                    "AdvancePlaytime injected an epoch into filePlaytime from lastTimeLog == 0");
    PLAYTIME_ASSERT(gSaveContext.shipSaveContext.lastTimeLog != 0, 1,
                    "AdvancePlaytime did not seed lastTimeLog, so the next tick would inject the epoch too");

    // ---------------------------------------------------------------- 2
    // Accrual still works when lastTimeLog is a real prior observation. Using 1
    // (an ancient non-zero) makes the accrued delta the current epoch, so the
    // assertion is just "it grew" — the point is the guard did NOT suppress it.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.save.shipSaveInfo.fileCompletedAt = 0;
    gSaveContext.save.shipSaveInfo.filePlaytime = 0;
    gSaveContext.shipSaveContext.lastTimeLog = 1;

    SavingEnhancements_AdvancePlaytime();

    PLAYTIME_ASSERT(gSaveContext.save.shipSaveInfo.filePlaytime > 0, 2,
                    "AdvancePlaytime stopped accruing entirely — the zero-guard is too broad");

    // ---------------------------------------------------------------- 3
    // Completed file: the gate must block all accrual and preserve the value.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.save.shipSaveInfo.fileCompletedAt = 0x1234;
    gSaveContext.save.shipSaveInfo.filePlaytime = 42;
    gSaveContext.shipSaveContext.lastTimeLog = 0;

    SavingEnhancements_AdvancePlaytime();

    PLAYTIME_ASSERT(gSaveContext.save.shipSaveInfo.filePlaytime == 42, 3,
                    "AdvancePlaytime accrued past the fileCompletedAt gate");

    // Leave gSaveContext clean for whatever runs next.
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    printf("[TEST] mm-playtime-seed: PASS\n");
    return 0;
}
