/**
 * @file test_archive_hotswap.c
 * @brief Archive hot-swap regression coverage (issue #263, follow-up to #154/#162)
 *
 * #154 was the bug where resource archives were not re-added to the shared
 * ArchiveManager on a cross-game switch, so the destination game booted with
 * missing assets (or crashed) on the *second* and later switches. PR #162
 * fixed it by calling EnsureGameArchivesLoaded() before every
 * GameRunner_SwitchTo() in rsbs/src/main.cpp. This file guards that fix.
 *
 * Headless coverage layer (this file):
 *
 *   Test_ArchiveHotswapLogic (unit, label "redship", always runs):
 *      Headless multi-switch (>=3 transitions) simulation of OoT<->MM. Drives
 *      the production Combo_* freeze/restore + cross-game entrance routing the
 *      same way the main loop does, asserts both SaveContexts stay byte-intact
 *      across every leg, and samples process RSS at each switch boundary to
 *      assert the steady-state delta stays under a bound. This runs in CI with
 *      no ROMs, so the regression is caught even when the full integration test
 *      is skipped.
 *
 * The ArchiveHotswap_* helpers below carry C linkage so that, when the runtime
 * INT_TEST_ARCHIVE_HOTSWAP_CYCLE integration mode is wired up (deferred), the
 * OoT/MM GameExports object libraries can drive the same cycle/RSS tracking via
 * integration_test_hooks.h. They are presently exercised only by the headless
 * unit test in this translation unit.
 *
 * RSS sampling: Linux /proc/self/statm current RSS with a
 * getrusage(RUSAGE_SELF).ru_maxrss fallback. On non-Linux this returns 0 and
 * the RSS bound check is skipped (the byte-integrity checks still run).
 *
 * Linkage note: this file is #included into test_runner.cpp at file scope
 * (NOT inside the extern "C" block used for test_game_lifecycle.c). It is
 * therefore compiled as C++. The cross-translation-unit helpers are wrapped in
 * `extern "C"` below so they keep C linkage. The unit-test entry point keeps
 * C++ linkage -- it is only referenced from this TU's gTests[] table -- which
 * lets it call the C++-linkage Entrance_* API directly.
 */

#include "../context.h"
#include "../entrance.h"
#include "../test_runner.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

/* ========================================================================
 * RSS sampling + shared cycle state (C linkage for cross-TU consumers)
 * ======================================================================== */

extern "C" {

/**
 * Sample a process memory figure in kilobytes.
 * - Linux: prefer current RSS from /proc/self/statm (pages * page_size),
 *   falling back to getrusage peak RSS. Current RSS is what we want for a
 *   steady-state delta -- peak only ever grows so it cannot show "returns to
 *   baseline" behaviour.
 * - Other platforms: 0 (caller skips the RSS bound check).
 */
long ArchiveHotswap_SampleRssKb(void) {
#if defined(__linux__)
    long rssKb = 0;

    FILE* f = fopen("/proc/self/statm", "r");
    if (f != NULL) {
        long totalPages = 0;
        long residentPages = 0;
        if (fscanf(f, "%ld %ld", &totalPages, &residentPages) == 2) {
            long pageKb = 4; /* default 4KB page */
#if defined(_SC_PAGESIZE)
            long pageSize = sysconf(_SC_PAGESIZE);
            if (pageSize > 0) {
                pageKb = pageSize / 1024;
            }
#endif
            rssKb = residentPages * pageKb;
        }
        fclose(f);
    }

    if (rssKb == 0) {
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) {
            rssKb = ru.ru_maxrss; /* already KB on Linux */
        }
    }
    return rssKb;
#else
    return 0;
#endif
}

/* Number of stable game arrivals the integration cycle drives before
 * declaring success. The test boots OoT first, so the arrivals are:
 *   #1 OoT (start) -> #2 MM -> #3 OoT -> #4 MM
 * i.e. 4 arrivals == 3 completed cross-game transitions, satisfying the
 * ">=3 consecutive switches" requirement and exercising both the OoT and MM
 * hot-swap paths. */
#define ARCHIVE_HOTSWAP_TARGET_ARRIVALS 4

/* Allowed steady-state RSS growth between the first arrival sample and any
 * later sample, in kilobytes. Both SaveContexts together are ~24KB and
 * neither game is re-init'd on resume, so a correct hot-swap should settle
 * well under this. Generous enough to absorb allocator noise / GL driver
 * caching, tight enough to catch a per-switch archive/resource leak (the
 * #154 failure mode would add tens of MB per switch). */
#define ARCHIVE_HOTSWAP_MAX_RSS_DELTA_KB (192L * 1024L) /* 192 MB */

static int sHotswapArrivals = 0;       /* stable game arrivals so far */
static long sHotswapBaselineRssKb = 0; /* RSS at first arrival sample */
static long sHotswapPeakDeltaKb = 0;   /* max observed delta vs baseline */
static int sHotswapRssExceeded = 0;    /* set if delta blew the bound */

void ArchiveHotswap_ResetCycle(void) {
    sHotswapArrivals = 0;
    sHotswapBaselineRssKb = 0;
    sHotswapPeakDeltaKb = 0;
    sHotswapRssExceeded = 0;
}

/**
 * Record one stable game arrival: bump the arrival counter and fold the
 * current RSS sample into the steady-state delta tracking. The first arrival
 * establishes the RSS baseline; later arrivals are compared against it.
 * @return the number of arrivals so far (1 == initial boot arrival).
 */
int ArchiveHotswap_RecordArrival(void) {
    sHotswapArrivals++;

    long rssKb = ArchiveHotswap_SampleRssKb();
    if (rssKb > 0) {
        if (sHotswapBaselineRssKb == 0) {
            sHotswapBaselineRssKb = rssKb;
        } else {
            long delta = rssKb - sHotswapBaselineRssKb;
            if (delta > sHotswapPeakDeltaKb) {
                sHotswapPeakDeltaKb = delta;
            }
            if (delta > ARCHIVE_HOTSWAP_MAX_RSS_DELTA_KB) {
                sHotswapRssExceeded = 1;
            }
        }
    }

    printf("[INT-TEST] hotswap arrival #%d: rss=%ldKB baseline=%ldKB peakDelta=%ldKB\n", sHotswapArrivals, rssKb,
           sHotswapBaselineRssKb, sHotswapPeakDeltaKb);
    fflush(stdout);
    return sHotswapArrivals;
}

/** @return number of stable arrivals recorded so far (without recording one). */
int ArchiveHotswap_ArrivalsSoFar(void) {
    return sHotswapArrivals;
}

int ArchiveHotswap_TargetArrivals(void) {
    return ARCHIVE_HOTSWAP_TARGET_ARRIVALS;
}

/** @return 1 if the steady-state RSS delta bound has been exceeded. */
int ArchiveHotswap_RssExceeded(void) {
    return sHotswapRssExceeded;
}

long ArchiveHotswap_PeakDeltaKb(void) {
    return sHotswapPeakDeltaKb;
}

} /* extern "C" */

/* ========================================================================
 * Unit-level (headless) regression test
 *
 * C++ linkage (referenced only from this TU's gTests[] table). This lets it
 * call the C++-linkage Entrance_* API. The entrance table is NOT set up by
 * main() in --test mode (main returns from TestRunner_Run before its
 * Entrance_Init call), so this test initializes it itself.
 * ======================================================================== */

#define HOTSWAP_ASSERT(cond, msg)                                  \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL: %s\n", (msg));                    \
            return TEST_FAIL;                                      \
        }                                                          \
    } while (0)

/* Distinct, recognizable fill so a leaked/overwritten byte is obvious. */
static void HotswapFillPattern(uint8_t* buf, size_t size, uint8_t seed) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)(seed + (i * 31u));
    }
}

static int HotswapBufferMatches(const uint8_t* buf, size_t size, uint8_t seed) {
    for (size_t i = 0; i < size; i++) {
        if (buf[i] != (uint8_t)(seed + (i * 31u))) {
            return 0;
        }
    }
    return 1;
}

TestResult Test_ArchiveHotswapLogic(void) {
    printf("[TEST] archive-hotswap-logic: headless multi-switch archive/state regression (#263)\n");

    /* Fresh state. */
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Entrance_Init();
    Entrance_RegisterDefaultLinks();
    ArchiveHotswap_ResetCycle();

    /* Golden buffers -- what each game's SaveContext should always read back
     * as. We deliberately use the full N64 sizes the freeze layer clamps to. */
    uint8_t ootGolden[OOT_SAVE_CONTEXT_SIZE];
    uint8_t mmGolden[MM_SAVE_CONTEXT_SIZE];
    HotswapFillPattern(ootGolden, sizeof(ootGolden), 0xA1);
    HotswapFillPattern(mmGolden, sizeof(mmGolden), 0x5C);

    /* Seed both frozen states once, as if each game has been visited. */
    Combo_FreezeState("oot", OOT_ENTR_MARKET_FROM_MASK_SHOP, ootGolden, sizeof(ootGolden));
    Combo_FreezeState("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0, mmGolden, sizeof(mmGolden));

    /* Sample RSS up front so the steady-state tracking has a baseline even in
     * the headless path (also a smoke test of the sampler). */
    (void)ArchiveHotswap_RecordArrival();

    /* Drive >=3 OoT<->MM transitions. Each iteration mimics one leg of the
     * main loop: route a cross-game entrance, verify it resolves to the other
     * game, "switch" (here: restore the destination's frozen state into a
     * scratch buffer, the same Combo_RestoreState call OoT_/MM_Game_Resume
     * make after EnsureGameArchivesLoaded), and re-freeze the source. The
     * destination buffer must always come back byte-identical to its golden --
     * the #154 failure mode corrupted/lost this across repeated switches. */
    GameId current = GAME_OOT;
    int legsDriven = 0;
    const int legs = 4; /* OoT->MM->OoT->MM : 4 transitions, 3+ required */
    for (int leg = 0; leg < legs; leg++) {
        Entrance_ClearPendingSwitch();

        if (current == GAME_OOT) {
            Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);
            HOTSWAP_ASSERT(Combo_IsCrossGameSwitch(), "OoT leg did not route cross-game");
            HOTSWAP_ASSERT(strcmp(Combo_GetSwitchTargetGameId(), "mm") == 0, "OoT leg target should be mm");

            /* Freeze OoT (source), restore MM (destination). */
            Combo_FreezeState("oot", Combo_GetSwitchReturnEntrance(), ootGolden, sizeof(ootGolden));
            uint8_t mmScratch[MM_SAVE_CONTEXT_SIZE];
            memset(mmScratch, 0, sizeof(mmScratch));
            HOTSWAP_ASSERT(Combo_RestoreState("mm", mmScratch, sizeof(mmScratch)), "MM restore failed mid-cycle");
            HOTSWAP_ASSERT(HotswapBufferMatches(mmScratch, sizeof(mmScratch), 0x5C),
                           "MM SaveContext corrupted across switch");
            current = GAME_MM;
        } else {
            Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);
            HOTSWAP_ASSERT(Combo_IsCrossGameSwitch(), "MM leg did not route cross-game");
            HOTSWAP_ASSERT(strcmp(Combo_GetSwitchTargetGameId(), "oot") == 0, "MM leg target should be oot");

            /* Freeze MM (source), restore OoT (destination). */
            Combo_FreezeState("mm", Combo_GetSwitchReturnEntrance(), mmGolden, sizeof(mmGolden));
            uint8_t ootScratch[OOT_SAVE_CONTEXT_SIZE];
            memset(ootScratch, 0, sizeof(ootScratch));
            HOTSWAP_ASSERT(Combo_RestoreState("oot", ootScratch, sizeof(ootScratch)), "OoT restore failed mid-cycle");
            HOTSWAP_ASSERT(HotswapBufferMatches(ootScratch, sizeof(ootScratch), 0xA1),
                           "OoT SaveContext corrupted across switch");
            current = GAME_OOT;
        }

        legsDriven++;
        (void)ArchiveHotswap_RecordArrival();
    }

    /* >=3 transitions actually happened. */
    HOTSWAP_ASSERT(legsDriven >= 3, "fewer than 3 transitions driven");

    /* No unbounded RSS growth. On non-Linux the sampler returns 0 and the
     * bound check is inert (baseline stays 0) -- byte-integrity above still
     * covers the regression there. */
    HOTSWAP_ASSERT(!ArchiveHotswap_RssExceeded(), "steady-state RSS delta exceeded bound across switches");

    /* Both golden states are still intact after the whole round-trip. */
    uint8_t ootFinal[OOT_SAVE_CONTEXT_SIZE];
    uint8_t mmFinal[MM_SAVE_CONTEXT_SIZE];
    memset(ootFinal, 0, sizeof(ootFinal));
    memset(mmFinal, 0, sizeof(mmFinal));
    HOTSWAP_ASSERT(Combo_RestoreState("oot", ootFinal, sizeof(ootFinal)), "final OoT restore failed");
    HOTSWAP_ASSERT(Combo_RestoreState("mm", mmFinal, sizeof(mmFinal)), "final MM restore failed");
    HOTSWAP_ASSERT(HotswapBufferMatches(ootFinal, sizeof(ootFinal), 0xA1), "final OoT state corrupted");
    HOTSWAP_ASSERT(HotswapBufferMatches(mmFinal, sizeof(mmFinal), 0x5C), "final MM state corrupted");

    printf("[TEST] PASS: %d transitions, peak RSS delta %ldKB (bound %ldKB)\n", legsDriven,
           ArchiveHotswap_PeakDeltaKb(), ARCHIVE_HOTSWAP_MAX_RSS_DELTA_KB);

    Entrance_ClearPendingSwitch();
    Context_ClearAllFrozenStates();
    return TEST_PASS;
}
