/**
 * @file mm_clock_shuffle_songs_test.cpp
 * ROM-free, display-free lock for #678: the two Enhancements TUs that carry
 * RO_CLOCK_SHUFFLE's ownership checks must survive the single-exe link, and
 * their registrars must actually RUN when their CVar is on.
 * CTest label "redship", row MMClockShuffleSongs in CMake/SingleExecutable.cmake,
 * dispatch "mm-clock-shuffle-songs" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#678). games/mm/2s2h/Enhancements/Songs/
 * BetterSongOfDoubleTime.cpp and SkipSoTCutscenes.cpp register only through a
 * file-scope `RegisterShipInitFunc` and export nothing anything else
 * references. They lived in `2ship_enh`, a plain STATIC archive, so the linker
 * dropped both members outright -- neither object appears in the operator's
 * redship.map. Two whole enhancements were dead (toggling their CVars did
 * nothing at all), and with them went the only callers of
 * `Rando::ClockShuffle::IsTimeOwnedForClockShuffle`,
 * `GetTimeDescriptionForMessage` and `SetTimeToHalfDayStart`: the checks that
 * stop Song of Double Time's free-form selector from warping into a half-day
 * the file does not own, and that land a Song of Time reset on the earliest
 * OWNED half-day instead of the vanilla dawn. That is why `RO_CLOCK_SHUFFLE`
 * was the one row PR #677's re-measure could not promote. The fix carves both
 * TUs into `2ship_enh_clockshuffle` and links it WHOLE_ARCHIVE
 * (games/mm/CMakeLists.txt).
 *
 * WHY THIS ROW EXISTS ALONGSIDE THE CI SYMBOL GATE. .github/scripts/
 * check-registrar-elision.sh audits the new archive as `required`, which proves
 * every member's `_GLOBAL__sub_I_*` reached the binary. It cannot prove the
 * registrar RAN with a live gate: a registrar whose `ShipInit` entry never gets
 * driven, or whose CVar path was misspelled, keeps the symbol and leaves the
 * registry as empty as full elision did. That is the #516 failure shape, and it
 * is what legs 2-4 below measure. The script is also nm-based and Linux-only;
 * this row runs on every platform.
 *
 * WHAT THIS ROW DELIBERATELY DOES NOT NAME, AND WHY. It never mentions
 * `IsTimeOwnedForClockShuffle`, `GetTimeDescriptionForMessage`,
 * `SetTimeToHalfDayStart`, `RegisterBetterSongOfDoubleTime` or
 * `RegisterSkipSoTCutscenes`. A reference from this always-linked TU would
 * itself pull the objects in and keep the functions past /OPT:REF, so the row
 * would pass with the CMake change reverted -- the same discipline
 * mm_registrar_coverage_test.cpp and soh_menu_registrar_test.cpp follow. What
 * it reads instead is MM's own `S2H::ShipInit` map and the `S2H::GameHooks`
 * registries, which only those TUs' static initializers and registrars can
 * populate. The three ClockShuffle functions need no separate probe: their only
 * callers are inside these two objects, so "the objects linked" IS the evidence
 * that the calls exist to keep them.
 *
 * ATTRIBUTION -- why each probe names exactly one registrar.
 *   - ShipInit path "gEnhancements.Songs.BetterSongOfDoubleTime": the only
 *     `RegisterShipInitFunc` keyed on it in the whole tree is
 *     BetterSongOfDoubleTime.cpp's. (2s2h/BenGui/BenMenu.cpp mentions the same
 *     CVar, but it builds a menu widget, registers nothing on this map, and is
 *     filtered out of the single exe entirely.) Same for
 *     "gEnhancements.Songs.SkipSoTCutscenes" and SkipSoTCutscenes.cpp.
 *   - ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT]: sole
 *     registrant tree-wide is BetterSongOfDoubleTime.cpp.
 *   - OnActorUpdate[ACTOR_EN_TEST6]: sole registrant tree-wide is
 *     SkipSoTCutscenes.cpp. BetterSongOfDoubleTime.cpp also keys on
 *     ACTOR_EN_TEST6, but on OnActorKill and only from inside a live
 *     Song-of-Double-Time selection, which this headless row never enters.
 *   Each is asserted to hold EXACTLY ONE registrant rather than merely a
 *   non-zero count, so a future second registrant fails loudly as a
 *   precondition violation instead of silently propping the probe up.
 *
 * NON-VACUITY. Leg 1 drives both registrars with their CVar forced OFF and
 * requires both registries to settle EMPTY first, so a registry populated by an
 * earlier row in a shared `--test all` process cannot satisfy leg 3. Leg 2 is
 * exact against elision on its own: revert the CMake carve-out and the ShipInit
 * map has no entry under either path, so it goes red at FAIL(2)/FAIL(3) before
 * anything else runs. Verified red before green that way.
 *
 * LEG 4 IS ALSO THE CLEANUP. Setting both CVars back to 0 and re-driving leaves
 * the process exactly as this row found it, which matters because `--test all`
 * shares one process: an armed VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT would
 * follow every later row. It is a real assertion as well -- it is what shows the
 * COND_* gate is evaluated on each drive rather than latched at first boot,
 * which is exactly the #539 defect one layer down.
 *
 * WHAT THIS DOES NOT COVER. That the ownership check produces the right answer
 * in play -- Song of Double Time refusing an unowned half-day and saying so --
 * needs a real play state, an ocarina and a rando save. No headless row reaches
 * it; it is an operator playtest, the same gap mm_hook_dispatch_test.cpp records
 * for its own call-site leg.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * S2H::ShipInit, S2H::GameHooks and the plain-archive elision this lock is
 * about all exist only in the single-exe build. A standalone 2ship build
 * compiles both TUs into one shared library where nothing is elided, so the row
 * reports pass rather than failing to link. test_runner.cpp declares this
 * unconditionally.
 */
extern "C" int MM_ClockShuffleSongs_RunHeadless(void) {
    printf("[TEST] mm-clock-shuffle-songs: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "mm_game_hooks.h"

extern "C" {
// Production accessors over MM's registrar map (2s2h/ShipInitBridge_SingleExe.cpp).
// Declared, never defined here. RegistrarCountForPath is non-inserting, unlike
// ShipInit::Init's operator[]; OnCVarChanged is the exact entry point OoT's
// unified menu reaches MM's registrars through (#539), so leg 3 drives the real
// path a player's click takes rather than a test-only shortcut.
void MM_ShipInit_OnCVarChanged(const char* path);
int MM_ShipInit_RegistrarCountForPath(const char* path);
}

namespace {

#define CS_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// Settled counts. Unregistration in S2H::GameHooks is DEFERRED -- Unregister
// only queues an id, drained at the next Execute of that hook type -- and the
// COND_* macros unregister-then-register on every drive. Counting the raw maps
// without flushing would credit a hook already slated for removal, which is
// exactly what leg 4 must not do.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

constexpr const char* kBsodtCVar = "gEnhancements.Songs.BetterSongOfDoubleTime";
constexpr const char* kSkipSotCVar = "gEnhancements.Songs.SkipSoTCutscenes";

// Set a CVar and re-drive MM's registrars for that key through the production
// path, mirroring what a unified-menu widget does on a click (#539).
void SetAndDrive(const char* path, int value) {
    CVarSetInteger(path, value);
    MM_ShipInit_OnCVarChanged(path);
}

size_t BsodtPromptRegistrants() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT);
}

size_t SkipSotEnTest6Registrants() {
    return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_EN_TEST6);
}

} // namespace

extern "C" int MM_ClockShuffleSongs_RunHeadless(void) {
    printf("[TEST] mm-clock-shuffle-songs: the Songs TUs carrying RO_CLOCK_SHUFFLE's ownership checks survive "
           "the link and their registrars run (#678)\n");

    auto ctx = Ship::Context::GetInstance();
    CS_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing - run the shared bring-up first");
    CS_ASSERT(ctx->GetConsoleVariables() != nullptr, 1, "ConsoleVariables missing - the CVar drives below need them");

    // ---- Leg 1: known-disarmed baseline (non-vacuity) ----------------------
    // Drive both registrars with their gate OFF. If either TU is in the link,
    // this unregisters whatever an earlier row in a shared --test all process
    // left armed; if neither is, it does nothing. Either way both registries
    // must read empty before leg 3 can mean anything.
    SetAndDrive(kBsodtCVar, 0);
    SetAndDrive(kSkipSotCVar, 0);
    CS_ASSERT(BsodtPromptRegistrants() == 0, 4,
              "ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT] is populated with the "
              "BetterSongOfDoubleTime CVar OFF - leg 3 would be vacuous");
    CS_ASSERT(SkipSotEnTest6Registrants() == 0, 4,
              "OnActorUpdate[ACTOR_EN_TEST6] is populated with the SkipSoTCutscenes CVar OFF - leg 3 would "
              "be vacuous");

    // ---- Leg 2: both TUs reached the link (#678, the elision itself) -------
    // A RegisterShipInitFunc entry under a CVar path can only be there because
    // that TU's file-scope static initializer ran, which can only happen if the
    // object is in the binary. This is the leg that goes red if the
    // 2ship_enh_clockshuffle WHOLE_ARCHIVE carve-out is reverted.
    CS_ASSERT(MM_ShipInit_RegistrarCountForPath(kBsodtCVar) == 1, 2,
              "no ShipInit registrar under gEnhancements.Songs.BetterSongOfDoubleTime - the TU was elided "
              "from the link, taking Song of Double Time's half-day ownership check with it (#678)");
    CS_ASSERT(MM_ShipInit_RegistrarCountForPath(kSkipSotCVar) == 1, 3,
              "no ShipInit registrar under gEnhancements.Songs.SkipSoTCutscenes - the TU was elided from the "
              "link, so a Song of Time reset ignores which half-days the file owns (#678)");

    // ---- Leg 3: the registrars RUN, and arm their hooks --------------------
    // What the symbol gate cannot see. Driven through MM_ShipInit_OnCVarChanged,
    // the same entry point a unified-menu click reaches (#539).
    SetAndDrive(kBsodtCVar, 1);
    CS_ASSERT(BsodtPromptRegistrants() == 1, 5,
              "ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT] did not gain exactly one "
              "registrant after arming the CVar - RegisterBetterSongOfDoubleTime linked but never ran, or a "
              "second registrant has appeared and this probe is no longer attributable (#678)");

    SetAndDrive(kSkipSotCVar, 1);
    CS_ASSERT(SkipSotEnTest6Registrants() == 1, 6,
              "OnActorUpdate[ACTOR_EN_TEST6] did not gain exactly one registrant after arming the CVar - "
              "RegisterSkipSoTCutscenes linked but never ran, or a second registrant has appeared and this "
              "probe is no longer attributable (#678)");

    // ---- Leg 4: the gate is re-evaluated, and the process is left clean ----
    SetAndDrive(kBsodtCVar, 0);
    SetAndDrive(kSkipSotCVar, 0);
    CS_ASSERT(BsodtPromptRegistrants() == 0, 7,
              "ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT] survived disarming the CVar - the "
              "COND_VB_SHOULD gate is latched, and every later row in this process inherits the hook");
    CS_ASSERT(SkipSotEnTest6Registrants() == 0, 7,
              "OnActorUpdate[ACTOR_EN_TEST6] survived disarming the CVar - the COND_ID_HOOK gate is latched, "
              "and every later row in this process inherits the hook");

    printf("[TEST] mm-clock-shuffle-songs: PASS (both Songs TUs linked, both registrars ran, both gates "
           "re-evaluated and disarmed)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
