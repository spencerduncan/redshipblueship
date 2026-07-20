/**
 * ROM-free lock for the MM GameInteractor shim (#395 / #383 GameInteractor
 * item). CTest label "redship", row mm-gi-shim in src/common/test_runner.cpp.
 *
 * What was broken: both ports define a global-namespace `class GameInteractor`
 * with different layouts — OoT sizeof 4 / nextHookId at 0; MM sizeof 104 /
 * nextHookId at 96 under MSVC, 72 / 64 under Linux GCC — and the single
 * shared allocation is OoT's. MM's four integration-test hook registrations
 * went through MM's view of the class: compiled as MM's body, each one writes
 * nextHookId ~60-92 bytes past the end of the 4-byte block.
 *
 * What the PR #415 diagnostic run measured (Linux, production flags): all
 * four probe variants — OoT direct, OoT out-of-line, MM direct, MM
 * out-of-line — wrote OoT's offset [0,4). I.e. on that build the MM call
 * sites were NOT inlined and the linker folded every
 * RegisterGameHook<OnGameStateMainStart> COMDAT to OoT's copy, so the OOB
 * write was latent rather than firing. That is luck, not safety: the #383
 * FlagTable incident showed fold winners FLIP when an unrelated link-set
 * change lands, one inlining decision (LTO, /O2 heuristics, a bigger caller)
 * executes MM's body regardless of the fold, and MM's events/currentEvent
 * data members are out-of-bounds on the shared instance whenever touched.
 * MM registrations therefore go through the MM-owned extern "C" shim
 * (games/mm/include/mm_game_hooks.h), dispatched from MM's own frame loop,
 * and this test pins the folded copy to OoT's layout.
 *
 * Three locks, in order of strength:
 *
 * 1. LAYOUT PREMISE (machine-verified, not assumed). The two ports' sizeof /
 *    offsetof(nextHookId) are compared at runtime, each reported from its own
 *    TU with its own production headers and flags. If they ever agree, the
 *    fault class evaporates and this test should be revisited.
 *
 * 2. FOLD/CALL-SITE BEHAVIOR (the lock linker ICF cannot defeat). OoT-side
 *    registration is run against an oversized zeroed buffer twice — once as a
 *    normal (inlinable) call, once through a member-function pointer, which
 *    is exactly the linker-selected COMDAT copy of
 *    RegisterGameHook<OnGameStateMainStart>. Passing requires every byte
 *    written to lie inside OoT's sizeof — i.e. no surviving registration path
 *    executes MM's offset-96 body. If MM code ever reintroduces a
 *    RegisterGameHook instantiation into the link and it wins the fold (the
 *    hazard Lane C's 2ship_enh/2ship_rando un-elision can trigger), this
 *    fails loudly. The compile-time half lives in
 *    games/mm/include/mm_gi_hook_guard.h, which poisons the Register* member
 *    names in 2ship_port/2ship_src TUs — including this one, which is why the
 *    MM side of this test only exports layout facts.
 *
 * 3. SHIM CONTRACT + FOUR-MODE ARMING. The MM_GameHooks_* registry must
 *    register/dispatch/deferred-unregister as documented, and the REAL
 *    per-mode arming path (MM_RegisterIntegrationTestHooks, reached via
 *    IntegrationTest_SetMode + the C test entry) must register exactly one
 *    MM-frame hook for each of the four hook-driven integration modes
 *    (boot-mm, T1, T2, T4) and none for the direct-tick round-trip mode.
 *    Live arming on a real boot is the operator-batch check on #392.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mm_game_hooks.h"
#include "integration_test_hooks.h"
#include "context.h"

extern "C" {
// games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp — OoT's
// own view of its layout plus registration probes compiled in OoT's TU.
size_t OoT_GI_InstanceSize(void);
size_t OoT_GI_NextHookIdOffset(void);
uint32_t OoT_GI_ProbeRegisterOnMainStartDirect(void* storage, void (*fn)(void));
uint32_t OoT_GI_ProbeRegisterOnMainStartOutOfLine(void* storage, void (*fn)(void));
void OoT_GI_ProbeUnregisterOnMainStart(uint32_t hookId);
void OoT_GI_ProbePumpOnMainStart(void);
// games/mm/2s2h/GameExports_SingleExe.cpp — MM's view of the layout (facts
// only; MM registration members are compile-time poisoned) and the C entry
// that runs the real integration-mode arming path.
size_t MM_GI_InstanceSize(void);
size_t MM_GI_NextHookIdOffset(void);
void MM_GIShim_TestArmIntegrationHooks(void);
}

namespace {

constexpr size_t kProbeBufSize = 256;

#define GIS_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

void GIShimNoopHook(void) {
}

int sHookACalls = 0;
int sHookBCalls = 0;

void GIShimHookA(void) {
    sHookACalls++;
}

void GIShimHookB(void) {
    sHookBCalls++;
}

// True iff every nonzero byte after a probe against a zeroed buffer lies
// inside [0, limit). Prints any violating range so a regression names the
// layout that wrote it.
bool WritesConfinedBelow(const char* label, const unsigned char* buf, size_t n, size_t limit) {
    bool ok = true;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] != 0 && i >= limit) {
            printf("[TEST] %s wrote byte at offset %zu (allowed range is [0,%zu)) — an MM-layout "
                   "registration body is back in the link\n",
                   label, i, limit);
            ok = false;
        }
    }
    return ok;
}

} // namespace

extern "C" int MM_GIShim_RunHeadless(void) {
    // ---- 1. Layout premise: the two ports really do disagree -------------
    const size_t ootSize = OoT_GI_InstanceSize();
    const size_t ootOff = OoT_GI_NextHookIdOffset();
    const size_t mmSize = MM_GI_InstanceSize();
    const size_t mmOff = MM_GI_NextHookIdOffset();
    printf("[TEST] GameInteractor layout — OoT sizeof=%zu nextHookId@%zu, MM sizeof=%zu nextHookId@%zu\n", ootSize,
           ootOff, mmSize, mmOff);
    GIS_ASSERT(ootSize != mmSize || ootOff != mmOff,
               "MM and OoT GameInteractor layouts agree — the premise of #395 no longer holds, revisit this lock");
    GIS_ASSERT(mmOff + sizeof(uint32_t) > ootSize,
               "MM's nextHookId lies inside OoT's allocation — probe confinement below would be vacuous, revisit");

    // ---- 2. Registration writes stay inside OoT's allocation -------------
    // Direct = what OoT's real call sites execute (inlining included).
    // OutOfLine = the linker-selected COMDAT copy, the body any non-inlined
    // call site in EITHER game binds.
    struct ProbeCase {
        const char* label;
        uint32_t (*probe)(void*, void (*)(void));
    };
    const ProbeCase cases[] = {
        { "OoT direct-call registration", OoT_GI_ProbeRegisterOnMainStartDirect },
        { "COMDAT-fold registration", OoT_GI_ProbeRegisterOnMainStartOutOfLine },
    };
    uint32_t probeIds[2] = { 0, 0 };
    for (int c = 0; c < 2; c++) {
        unsigned char* buf = static_cast<unsigned char*>(calloc(1, kProbeBufSize));
        GIS_ASSERT(buf != nullptr, "probe buffer allocation failed");
        probeIds[c] = cases[c].probe(buf, GIShimNoopHook);
        const bool confined = WritesConfinedBelow(cases[c].label, buf, kProbeBufSize, ootSize);
        const bool touched = buf[ootOff] != 0 || buf[ootOff + 1] != 0 || buf[ootOff + 2] != 0 || buf[ootOff + 3] != 0;
        free(buf);
        GIS_ASSERT(probeIds[c] != 0, "probe registration returned id 0");
        GIS_ASSERT(confined, "registration wrote outside OoT's GameInteractor — MM's layout is being executed");
        GIS_ASSERT(touched, "registration did not write OoT's nextHookId — probe is not measuring the real path");
    }
    GIS_ASSERT(probeIds[0] != probeIds[1], "probe registrations returned the same hook id");
    // Clean the shared inline-static hook map back up: queue both probe hooks
    // for unregistration, then pump once (the flush runs at the head of
    // ExecuteHooks, so nothing actually executes here).
    OoT_GI_ProbeUnregisterOnMainStart(probeIds[0]);
    OoT_GI_ProbeUnregisterOnMainStart(probeIds[1]);
    OoT_GI_ProbePumpOnMainStart();

    // ---- 3. Shim contract: register / dispatch / deferred unregister -----
    MM_GameHooks_ResetForTest();
    sHookACalls = 0;
    sHookBCalls = 0;

    GIS_ASSERT(MM_GameHooks_RegisterOnGameStateMainStart(nullptr) == 0, "NULL hook must be rejected with id 0");
    const uint32_t idA = MM_GameHooks_RegisterOnGameStateMainStart(GIShimHookA);
    const uint32_t idB = MM_GameHooks_RegisterOnGameStateMainStart(GIShimHookB);
    GIS_ASSERT(idA != 0 && idB != 0 && idA != idB, "shim registration ids must be nonzero and unique");
    GIS_ASSERT(MM_GameHooks_CountOnGameStateMainStart() == 2, "two hooks registered, count != 2");

    MM_GameHooks_ExecuteOnGameStateMainStart();
    GIS_ASSERT(sHookACalls == 1 && sHookBCalls == 1, "dispatch did not run both hooks exactly once");

    MM_GameHooks_UnregisterOnGameStateMainStart(idA);
    GIS_ASSERT(MM_GameHooks_CountOnGameStateMainStart() == 2,
               "unregister must be deferred to the next dispatch, not applied immediately");
    MM_GameHooks_ExecuteOnGameStateMainStart();
    GIS_ASSERT(sHookACalls == 1, "unregistered hook still ran");
    GIS_ASSERT(sHookBCalls == 2, "surviving hook did not run");
    GIS_ASSERT(MM_GameHooks_CountOnGameStateMainStart() == 1, "count did not drop after the deferred flush");
    MM_GameHooks_UnregisterOnGameStateMainStart(0);      // ignored by contract
    MM_GameHooks_UnregisterOnGameStateMainStart(0xDEAD); // unknown id: no-op
    MM_GameHooks_ExecuteOnGameStateMainStart();
    GIS_ASSERT(sHookBCalls == 3, "hook lost after no-op unregisters");

    // ---- 4. Four-mode arming registers through the shim ------------------
    // Pin the harness state the real lambdas gate on: with no game active,
    // dispatching them is a headless no-op (MM_SceneLoadComplete and the
    // watchdog both require GAME_MM).
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_NONE);

    struct ModeCase {
        IntegrationTestMode mode;
        uint32_t expectedHooks;
        const char* label;
    };
    const ModeCase modes[] = {
        { INT_TEST_BOOT_MM, 1, "boot-mm" },
        { INT_TEST_SWITCH_OOT_HMS_TO_MM, 1, "switch-oot-hms-to-mm (T1)" },
        { INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT, 1, "switch-mm-clocktown-south-to-oot (T2)" },
        { INT_TEST_ARCHIVE_HOTSWAP_CYCLE, 1, "archive-hotswap-cycle (T4)" },
        { INT_TEST_GAMEPLAY_ROUNDTRIP, 0, "gameplay-roundtrip (direct tick, no hooks)" },
        { INT_TEST_NONE, 0, "none (arming must early-out)" },
    };
    for (const ModeCase& m : modes) {
        MM_GameHooks_ResetForTest();
        IntegrationTest_SetMode(m.mode);
        MM_GIShim_TestArmIntegrationHooks();
        const uint32_t got = MM_GameHooks_CountOnGameStateMainStart();
        if (got != m.expectedHooks) {
            printf("[TEST] FAIL: mode %s armed %u shim hooks, expected %u\n", m.label, got, m.expectedHooks);
            IntegrationTest_SetMode(INT_TEST_NONE);
            MM_GameHooks_ResetForTest();
            Context_SetCurrentGame(prevGame);
            return 1;
        }
        // The armed hooks must be dispatchable headlessly (their scene-load
        // predicates gate on GAME_MM and fail closed with no game running).
        MM_GameHooks_ExecuteOnGameStateMainStart();
    }

    IntegrationTest_SetMode(INT_TEST_NONE);
    MM_GameHooks_ResetForTest();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: mm-gi-shim — MM hook registration stays off the shared 4-byte instance, all four "
           "hook-driven integration modes arm through the MM-owned shim\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
