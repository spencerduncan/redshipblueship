/**
 * @file integration_test_hooks.cpp
 * @brief Integration test state management
 *
 * This file provides the state management for integration tests.
 * Hook registration happens in each game's init code (OoT/MM) using their
 * respective GameInteractor implementations.
 *
 * See:
 *   - games/oot/soh/GameExports_SingleExe.cpp: OoT_RegisterIntegrationTestHooks()
 *   - games/mm/2s2h/GameExports_SingleExe.cpp: MM_RegisterIntegrationTestHooks()
 */

#include "integration_test_hooks.h"
#include "context.h"        // gComboCtx: the paired identity's carriers
#include "crossing_store.h" // the crossing store: the only truth for foreign placements
#include "foreign_items.h"  // Combo_ForeignPairingActive
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// Game switch request (to signal game to exit)
extern "C" {
    void Combo_RequestGameSwitch(void);

    // Archive hot-swap cycle helpers (defined in src/common/tests/test_archive_hotswap.c,
    // compiled into redship_common via test_runner.cpp). Reset the cycle counters
    // when the archive-hotswap integration test mode is selected (#263).
    void ArchiveHotswap_ResetCycle(void);
}

namespace {

// Integration test state
std::atomic<IntegrationTestMode> sTestMode{INT_TEST_NONE};
std::atomic<bool> sBootPassed{false};
std::atomic<bool> sExitRequested{false};
std::atomic<GameId> sBootedGame{GAME_NONE};

// Gameplay round-trip state (INT_TEST_GAMEPLAY_ROUNDTRIP)
std::atomic<GameplayPhase> sGameplayPhase{GP_PHASE_BOOT};
std::atomic<int> sGameplayCyclesDone{0};
GameplayTestConfig sGameplayConfig = {};
std::atomic<int> sGameplayVariant{GP_VARIANT_ROUNDTRIP};

// ---------------------------------------------------------------------------
// stderr tee (int-paired-first-crossing). fd 2 is pointed at a pipe's write
// end; a reader thread copies every byte to the ORIGINAL stderr (a dup of the
// old fd 2) as it arrives, and keeps the lines the paired row asserts on. The
// kept set is filtered so a long session costs a few kilobytes, not the whole
// log: a line is kept when it names a pairing, a creation, the crossing store
// or a refusal.
//
// Lifetime: the state is heap-allocated and never freed, and the reader thread
// is DETACHED, so no exit path (exit(), _Exit(), a crash) can run a joinable
// std::thread's destructor (std::terminate) or a destructor under a reader that
// is still blocked in read(). Stop waits for the reader a BOUNDED time: a child
// process that inherited fd 2 (the pipe's write end) keeps the pipe open, and a
// verdict must never wait on it. The pipe's own ends are created
// non-inheritable (_O_NOINHERIT / FD_CLOEXEC), so only fd 2 itself can leak.
// ---------------------------------------------------------------------------
struct CaptureState {
    std::mutex mutex;
    std::vector<std::string> lines;
    std::string partial;
    std::mutex doneMutex;
    std::condition_variable doneCv;
    bool readerDone = false;
};
CaptureState* sCapture = nullptr; // allocated once, never freed
std::atomic<bool> sCaptureActive{false};
int sCaptureOrigFd = -1; // dup of the original fd 2
int sCapturePipeRead = -1;
int sCapturePipeWrite = -1;
// How long Stop waits for the reader to drain after the write end is closed.
constexpr int kCaptureDrainBoundMs = 2000;

bool CaptureKeeps(const std::string& line) {
    static const char* const kTags[] = { "pairing", "creation", "[Crossings]", "REFUSED",
                                         "Not paired", "Not saved", "[PFC" };
    for (const char* tag : kTags) {
        if (line.find(tag) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void CaptureAppend(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(sCapture->mutex);
    std::string& partial = sCapture->partial;
    for (size_t i = 0; i < len; i++) {
        const char c = data[i];
        if (c == '\n') {
            if (!partial.empty() && partial.back() == '\r') {
                partial.pop_back();
            }
            if (CaptureKeeps(partial)) {
                sCapture->lines.push_back(partial);
            }
            partial.clear();
        } else if (partial.size() < 4096) {
            partial.push_back(c);
        }
    }
}

void CaptureReaderMain() {
    char buf[4096];
    for (;;) {
#ifdef _WIN32
        const int n = _read(sCapturePipeRead, buf, (unsigned int)sizeof(buf));
#else
        const ssize_t n = read(sCapturePipeRead, buf, sizeof(buf));
#endif
        if (n <= 0) {
            break; // write end closed (stop) or a broken pipe
        }
#ifdef _WIN32
        _write(sCaptureOrigFd, buf, (unsigned int)n);
#else
        ssize_t ignored = write(sCaptureOrigFd, buf, (size_t)n);
        (void)ignored;
#endif
        CaptureAppend(buf, (size_t)n);
    }
    std::lock_guard<std::mutex> lock(sCapture->doneMutex);
    sCapture->readerDone = true;
    sCapture->doneCv.notify_all();
}

// ---------------------------------------------------------------------------
// Progress word (#793): bumped by every frame either game starts and by every
// stage main enters between frames. `sProgressStage` names where the run was
// when it last made progress (string literals only).
// ---------------------------------------------------------------------------
std::atomic<uint64_t> sProgress{0};
std::atomic<const char*> sProgressStage{"boot: first game init, before its first frame"};
std::atomic<uint32_t> sProgressFrames[3] = {}; // indexed by GameId

// RSBS_INT_WEDGE fault injection, parsed once.
enum WedgeSite { WEDGE_NONE, WEDGE_OOT, WEDGE_MM, WEDGE_HANDOFF };
struct WedgeConfig {
    WedgeSite site;
    uint32_t frame;
    int secs;
};

int GameplayEnvInt(const char* name, int defaultValue, int minValue);

const WedgeConfig& Wedge(void) {
    static const WedgeConfig config = [] {
        WedgeConfig c = { WEDGE_NONE, 30, 600 };
        const char* raw = std::getenv("RSBS_INT_WEDGE");
        if (raw == nullptr || raw[0] == '\0') {
            return c;
        }
        if (strcmp(raw, "oot") == 0) {
            c.site = WEDGE_OOT;
        } else if (strcmp(raw, "mm") == 0) {
            c.site = WEDGE_MM;
        } else if (strcmp(raw, "handoff") == 0) {
            c.site = WEDGE_HANDOFF;
        } else {
            fprintf(stderr, "[INT-WEDGE] WARNING: ignoring RSBS_INT_WEDGE='%s' (oot, mm or handoff)\n", raw);
            fflush(stderr);
            return c;
        }
        c.frame = (uint32_t)GameplayEnvInt("RSBS_INT_WEDGE_FRAME", 30, 1);
        c.secs = GameplayEnvInt("RSBS_INT_WEDGE_SECS", 600, 1);
        return c;
    }();
    return config;
}

void WedgeNow(const char* where, uint32_t frame) {
    fprintf(stderr, "[INT-WEDGE] RSBS_INT_WEDGE: sleeping %d s inside %s (frame %u); no frame completes\n",
            Wedge().secs, where, (unsigned)frame);
    fflush(stderr);
    std::this_thread::sleep_for(std::chrono::seconds(Wedge().secs));
}

// The recorded paired identity (int-paired-first-crossing).
PairedIdentity sPairedIdentity = {};
bool sPairedIdentityRecorded = false;
uint32_t sPairedMMGenerationBaseline = 0;

const char* GameplayPhaseName(GameplayPhase phase) {
    switch (phase) {
        case GP_PHASE_BOOT:          return "boot";
        case GP_PHASE_OOT_PRE:       return "oot-pre-switch";
        case GP_PHASE_MM_STABILIZE:  return "mm-stabilize";
        case GP_PHASE_MM_PLAY:       return "mm-play";
        case GP_PHASE_OOT_RETURN:    return "oot-return";
        case GP_PHASE_OOT_WARP:      return "oot-warp";
        case GP_PHASE_OOT_EXIT:      return "oot-exit";
        case GP_PHASE_DONE:          return "done";
    }
    return "unknown";
}

// OoT's gEntranceTable bound (z64scene.h ENTR_MAX). Kept as a literal because
// src/common cannot include OoT headers; the OoT-side driver re-checks against
// the real ENTR_MAX at consumption time.
constexpr unsigned long kOoTEntranceMax = 0x0614;

// Per-phase watchdog budget default, in wall-clock seconds (#376 item 4). 60 s
// is far under the 300 s IntGameplayRoundtrip TIMEOUT and the 900 s soak, and
// no default-config phase runs anywhere near that long, so it fires only on a
// genuine wedge and always before the hard kill. Restated by the gp-watchdog
// test as the contract.
constexpr int kGpWatchdogDefaultSecs = 60;

int GameplayEnvInt(const char* name, int defaultValue, int minValue) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || raw[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    long value = strtol(raw, &end, 0);
    if (end == raw || *end != '\0' || value < minValue) {
        fprintf(stderr, "[GP-TEST] WARNING: ignoring invalid %s='%s' (using %d)\n", name, raw, defaultValue);
        return defaultValue;
    }
    return (int)value;
}

uint16_t GameplayEnvEntrance(const char* name, uint16_t defaultValue) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || raw[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    unsigned long value = strtoul(raw, &end, 0);
    if (end == raw || *end != '\0' || value >= kOoTEntranceMax) {
        fprintf(stderr, "[GP-TEST] WARNING: ignoring invalid %s='%s' (must be < 0x%04lX; using 0x%04X)\n",
                name, raw, kOoTEntranceMax, defaultValue);
        return defaultValue;
    }
    return (uint16_t)value;
}

void GameplayParseConfig(void) {
    sGameplayConfig.framesPerPhase = GameplayEnvInt("RSBS_GP_FRAMES", 120, 1);
    sGameplayConfig.cycles = GameplayEnvInt("RSBS_GP_CYCLES", 1, 1);
    // Defaults: boot outside the Happy Mask Shop (the return-leg spawn), warp
    // to map-select Market (ENTR_MARKET_SOUTH_EXIT), exit through Market's
    // south gate (ENTR_MARKET_ENTRANCE_NORTH_EXIT).
    sGameplayConfig.bootEntrance = GameplayEnvEntrance("RSBS_GP_BOOT_ENTRANCE", 0x01D1);
    sGameplayConfig.warpEntrance = GameplayEnvEntrance("RSBS_GP_WARP_ENTRANCE", 0x00B1);
    sGameplayConfig.exitEntrance = GameplayEnvEntrance("RSBS_GP_EXIT_ENTRANCE", 0x0033);
    const char* bootAge = getenv("RSBS_GP_BOOT_AGE");
    sGameplayConfig.bootAdult = bootAge != NULL && strcmp(bootAge, "adult") == 0;
    // Warp-phase-only frame budget: a time-related fault (bug 1c class) needs
    // a long soak INSIDE the warp target, not longer windows everywhere.
    sGameplayConfig.warpFrames = GameplayEnvInt("RSBS_GP_WARP_FRAMES", sGameplayConfig.framesPerPhase, 1);
    sGameplayConfig.cameraAssert = GameplayEnvInt("RSBS_GP_CAMERA_ASSERT", 1, 0);
    sGameplayConfig.watchdogSecs = IntegrationTest_GameplayWatchdogBudgetSecs();
    sGameplayPhase = GP_PHASE_BOOT;
    sGameplayCyclesDone = 0;
    printf("[GP-TEST] config: frames/phase=%d cycles=%d boot=0x%04X warp=0x%04X exit=0x%04X bootAge=%s "
           "warpFrames=%d camAssert=%d watchdogSecs=%d\n",
           sGameplayConfig.framesPerPhase, sGameplayConfig.cycles, sGameplayConfig.bootEntrance,
           sGameplayConfig.warpEntrance, sGameplayConfig.exitEntrance,
           sGameplayConfig.bootAdult ? "adult" : "child", sGameplayConfig.warpFrames,
           sGameplayConfig.cameraAssert, sGameplayConfig.watchdogSecs);
    fflush(stdout);
}

} // anonymous namespace

extern "C" {

void IntegrationTest_SetMode(IntegrationTestMode mode) {
    sTestMode = mode;
    sGameplayVariant = GP_VARIANT_ROUNDTRIP;
    sBootPassed = false;
    sExitRequested = false;
    sBootedGame = GAME_NONE;

    const char* modeName = "unknown";
    switch (mode) {
        case INT_TEST_NONE: modeName = "none"; break;
        case INT_TEST_BOOT_OOT: modeName = "boot-oot"; break;
        case INT_TEST_BOOT_MM: modeName = "boot-mm"; break;
        case INT_TEST_SWITCH_OOT_HMS_TO_MM: modeName = "switch-oot-hms-to-mm"; break;
        case INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT: modeName = "switch-mm-clocktown-south-to-oot"; break;
        case INT_TEST_ARCHIVE_HOTSWAP_CYCLE: modeName = "archive-hotswap-cycle"; break;
        case INT_TEST_GAMEPLAY_ROUNDTRIP: modeName = "gameplay-roundtrip"; break;
    }

    // Gameplay round-trip: parse env parameters and reset the phase machine.
    if (mode == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        GameplayParseConfig();
    }

    // Archive hot-swap cycle keeps a running arrival count / RSS baseline across
    // multiple OoT<->MM switches; reset it whenever this mode is (re)selected (#263).
    if (mode == INT_TEST_ARCHIVE_HOTSWAP_CYCLE) {
        ArchiveHotswap_ResetCycle();
    }

    if (mode != INT_TEST_NONE) {
        printf("[INT-TEST] Integration test mode: %s\n", modeName);
    }
}

IntegrationTestMode IntegrationTest_GetMode(void) {
    return sTestMode;
}

bool IntegrationTest_IsActive(void) {
    return sTestMode != INT_TEST_NONE;
}

bool IntegrationTest_BootPassed(void) {
    return sBootPassed.load();
}

void IntegrationTest_SignalBootComplete(GameId game, const char* reason) {
    printf("[INT-TEST] Boot complete: %s (%s)\n",
           Game_ToString(game), reason);
    fflush(stdout);
    sBootPassed = true;
    sBootedGame = game;
    sExitRequested = true;

    // Signal the game to exit by requesting a "switch"
    // This causes the game loop to exit cleanly
    printf("[INT-TEST] Requesting game exit...\n");
    fflush(stdout);
    Combo_RequestGameSwitch();
}

void IntegrationTest_RequestExit(void) {
    sExitRequested = true;
}

bool IntegrationTest_ExitRequested(void) {
    return sExitRequested.load();
}

void IntegrationTest_FrameProgress(GameId game) {
    if (game != GAME_OOT && game != GAME_MM) {
        return;
    }
    const uint32_t frame = ++sProgressFrames[game];
    sProgressStage = (game == GAME_OOT) ? "OoT frame" : "MM frame";
    sProgress++;
    const WedgeConfig& wedge = Wedge();
    if (frame == wedge.frame && ((game == GAME_OOT && wedge.site == WEDGE_OOT) ||
                                 (game == GAME_MM && wedge.site == WEDGE_MM))) {
        WedgeNow(game == GAME_OOT ? "an OoT frame" : "an MM frame", frame);
    }
}

void IntegrationTest_StageProgress(const char* stage) {
    sProgressStage = stage;
    sProgress++;
}

void IntegrationTest_HandoffWedgeIfArmed(void) {
    static bool sWedged = false;
    if (Wedge().site == WEDGE_HANDOFF && !sWedged) {
        sWedged = true;
        WedgeNow("main's cross-game hand-off", 0);
    }
}

// ============================================================================
// Gameplay round-trip repro (INT_TEST_GAMEPLAY_ROUNDTRIP)
// ============================================================================

GameplayPhase IntegrationTest_GetGameplayPhase(void) {
    return sGameplayPhase.load();
}

void IntegrationTest_SetGameplayPhase(GameplayPhase phase) {
    GameplayPhase previous = sGameplayPhase.exchange(phase);
    if (previous != phase) {
        printf("[GP-TEST] phase: %s -> %s\n", GameplayPhaseName(previous), GameplayPhaseName(phase));
        fflush(stdout);
    }
}

const GameplayTestConfig* IntegrationTest_GetGameplayConfig(void) {
    return &sGameplayConfig;
}

int IntegrationTest_GameplayWatchdogParse(const char* raw) {
    if (raw == nullptr || raw[0] == '\0') {
        return kGpWatchdogDefaultSecs;
    }
    char* end = nullptr;
    long value = strtol(raw, &end, 0);
    // Below 1 s is meaningless (the phase machine ticks per game frame), so
    // reject it the same as garbage rather than arming a watchdog that fires
    // on the first tick of every phase.
    if (end == raw || *end != '\0' || value < 1) {
        fprintf(stderr, "[GP-TEST] WARNING: ignoring invalid RSBS_GP_WATCHDOG_SECS='%s' (using %d)\n", raw,
                kGpWatchdogDefaultSecs);
        return kGpWatchdogDefaultSecs;
    }
    return (int)value;
}

int IntegrationTest_GameplayWatchdogBudgetSecs(void) {
    return IntegrationTest_GameplayWatchdogParse(std::getenv("RSBS_GP_WATCHDOG_SECS"));
}

bool IntegrationTest_GameplayWatchdogExpired(double elapsedSecs, int budgetSecs) {
    if (budgetSecs <= 0) {
        return false;
    }
    return elapsedSecs >= (double)budgetSecs;
}

int IntegrationTest_GameplayCyclesDone(void) {
    return sGameplayCyclesDone.load();
}

void IntegrationTest_GameplayRecordCycle(void) {
    int done = ++sGameplayCyclesDone;
    printf("[GP-TEST] round-trip %d/%d complete\n", done, sGameplayConfig.cycles);
    fflush(stdout);
}

void IntegrationTest_LogGameplayState(const char* tag) {
    fprintf(stderr,
            "[GP-TEST] state (%s): phase=%s cycles=%d/%d frames/phase=%d "
            "boot=0x%04X warp=0x%04X exit=0x%04X\n",
            tag ? tag : "-", GameplayPhaseName(sGameplayPhase.load()), sGameplayCyclesDone.load(),
            sGameplayConfig.cycles, sGameplayConfig.framesPerPhase, sGameplayConfig.bootEntrance,
            sGameplayConfig.warpEntrance, sGameplayConfig.exitEntrance);
    fflush(stderr);
}

void IntegrationTest_GameplayFail(const char* reason) {
    fprintf(stderr, "[GP-TEST] FAIL: %s\n", reason ? reason : "(no reason)");
    IntegrationTest_LogGameplayState("fail");
    // RequestExit does NOT set the pass flag, so the run returns non-zero;
    // Combo_RequestGameSwitch unblocks the main loop promptly (#263 pattern).
    IntegrationTest_RequestExit();
    Combo_RequestGameSwitch();
}

// ============================================================================
// int-paired-first-crossing: the variant, the stderr capture, the identity
// ============================================================================

void IntegrationTest_SetGameplayVariant(GameplayVariant variant) {
    sGameplayVariant = variant;
    if (variant == GP_VARIANT_PAIRED_FIRST_CROSSING) {
        // Before the first game boots, so the creation's own lines are kept.
        if (!IntegrationTest_StderrCaptureStart()) {
            fprintf(stderr, "[PFC] WARNING: the stderr capture could not start; every line assertion will fail\n");
            fflush(stderr);
        }
        fprintf(stderr, "[PFC] variant: paired first crossing (creation %s)\n",
                IntegrationTest_PairedSkipCreation() ? "SKIPPED by RSBS_PFC_SKIP_CREATION=1: the red half"
                                                     : "through the production event");
        fflush(stderr);
    }
}

GameplayVariant IntegrationTest_GetGameplayVariant(void) {
    return (GameplayVariant)sGameplayVariant.load();
}

bool IntegrationTest_PairedFirstCrossing(void) {
    return sTestMode == INT_TEST_GAMEPLAY_ROUNDTRIP && sGameplayVariant == GP_VARIANT_PAIRED_FIRST_CROSSING;
}

bool IntegrationTest_PairedSkipCreation(void) {
    const char* raw = std::getenv("RSBS_PFC_SKIP_CREATION");
    return raw != nullptr && strcmp(raw, "1") == 0;
}

bool IntegrationTest_StderrCaptureStart(void) {
    if (sCaptureActive.load()) {
        return true;
    }
    if (sCapture == nullptr) {
        sCapture = new CaptureState();
    }
    fflush(stderr);
    int fds[2] = { -1, -1 };
#ifdef _WIN32
    // Both ends non-inheritable; fd 2 below is a separate (inheritable, as any
    // stderr is) duplicate of the write end.
    if (_pipe(fds, 1 << 16, _O_BINARY | _O_NOINHERIT) != 0) {
        return false;
    }
    sCaptureOrigFd = _dup(2);
    if (sCaptureOrigFd < 0 || _dup2(fds[1], 2) != 0) {
        _close(fds[0]);
        _close(fds[1]);
        return false;
    }
#else
    // pipe() + FD_CLOEXEC rather than pipe2(): macOS has no pipe2.
    if (pipe(fds) != 0) {
        return false;
    }
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    sCaptureOrigFd = fcntl(2, F_DUPFD_CLOEXEC, 0);
    if (sCaptureOrigFd < 0 || dup2(fds[1], 2) < 0) {
        close(fds[0]);
        close(fds[1]);
        return false;
    }
#endif
    sCapturePipeRead = fds[0];
    sCapturePipeWrite = fds[1];
    {
        std::lock_guard<std::mutex> lock(sCapture->doneMutex);
        sCapture->readerDone = false;
    }
    sCaptureActive = true;
    std::thread(CaptureReaderMain).detach();
    return true;
}

void IntegrationTest_StderrCaptureStop(void) {
    if (!sCaptureActive.exchange(false)) {
        return;
    }
    fflush(stderr);
    // Point fd 2 back at the original stderr, then close the pipe's write end
    // (both of this process's references to it are then gone), so the reader
    // drains what is left and sees EOF.
#ifdef _WIN32
    _dup2(sCaptureOrigFd, 2);
    _close(sCapturePipeWrite);
#else
    dup2(sCaptureOrigFd, 2);
    close(sCapturePipeWrite);
#endif
    sCapturePipeWrite = -1;
    bool drained = false;
    {
        std::unique_lock<std::mutex> lock(sCapture->doneMutex);
        drained = sCapture->doneCv.wait_for(lock, std::chrono::milliseconds(kCaptureDrainBoundMs),
                                            [] { return sCapture->readerDone; });
    }
    if (!drained) {
        // Something else still holds the write end (a child that inherited fd
        // 2). Leave the reader and its read end alone: the process is about to
        // exit, and the verdict must not wait on a pipe it does not control.
        fprintf(stderr,
                "[PFC] stderr tee: the reader did not see EOF within %d ms (another process holds the pipe); "
                "continuing without it\n",
                kCaptureDrainBoundMs);
        fflush(stderr);
        return;
    }
#ifdef _WIN32
    _close(sCapturePipeRead);
#else
    close(sCapturePipeRead);
#endif
    sCapturePipeRead = -1;
}

void IntegrationTest_StderrCaptureRestoreForCrash(void) {
    if (!sCaptureActive.load() || sCaptureOrigFd < 0) {
        return;
    }
#ifdef _WIN32
    _dup2(sCaptureOrigFd, 2);
#else
    dup2(sCaptureOrigFd, 2);
#endif
}

int IntegrationTest_StderrCaptureCount(const char* needle) {
    if (needle == nullptr || sCapture == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(sCapture->mutex);
    int count = 0;
    for (const std::string& line : sCapture->lines) {
        if (line.find(needle) != std::string::npos) {
            count++;
        }
    }
    return count;
}

bool IntegrationTest_StderrCaptureLast(const char* needle, char* out, size_t cap) {
    if (needle == nullptr || out == nullptr || cap == 0) {
        return false;
    }
    out[0] = '\0';
    if (sCapture == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(sCapture->mutex);
    for (auto it = sCapture->lines.rbegin(); it != sCapture->lines.rend(); ++it) {
        if (it->find(needle) != std::string::npos) {
            snprintf(out, cap, "%s", it->c_str());
            return true;
        }
    }
    return false;
}

void IntegrationTest_PairedIdentityCapture(PairedIdentity* out) {
    if (out == nullptr) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->masterSeed = gComboCtx.sharedRandoSeed;
    out->settingsHash = gComboCtx.sharedRandoSettingsHash;
    out->mmProfileDigest = gComboCtx.mmProfileDigest;
    out->comboFingerprint = gComboCtx.comboSettingsHash;
    out->crossingDigest = Combo_Crossings_Digest();
    out->crossingsInHyrule = Combo_Crossings_Count(GAME_OOT);
    out->crossingsInTermina = Combo_Crossings_Count(GAME_MM);
    out->crossingsFrozen = Combo_Crossings_IsFrozen();
    out->pairingActive = Combo_ForeignPairingActive();
}

void IntegrationTest_PairedIdentityRecord(void) {
    IntegrationTest_PairedIdentityCapture(&sPairedIdentity);
    sPairedIdentityRecorded = true;
    char desc[256];
    IntegrationTest_PairedIdentityDescribe(&sPairedIdentity, desc, sizeof(desc));
    fprintf(stderr, "[PFC] identity recorded at creation (before the load): %s\n", desc);
    fflush(stderr);
}

void IntegrationTest_PairedSetMMGenerationBaseline(uint32_t dispatches) {
    sPairedMMGenerationBaseline = dispatches;
}

uint32_t IntegrationTest_PairedMMGenerationBaseline(void) {
    return sPairedMMGenerationBaseline;
}

const PairedIdentity* IntegrationTest_PairedIdentityRecorded(void) {
    return sPairedIdentityRecorded ? &sPairedIdentity : nullptr;
}

void IntegrationTest_PairedIdentityDescribe(const PairedIdentity* id, char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return;
    }
    if (id == nullptr) {
        snprintf(out, cap, "(none)");
        return;
    }
    snprintf(out, cap,
             "paired=%d masterSeed=%u settingsHash=%08X mmProfileDigest=%08X comboFingerprint=%08X "
             "crossingStore{frozen=%d inHyrule=%d inTermina=%d digest=%08X}",
             id->pairingActive ? 1 : 0, (unsigned)id->masterSeed, (unsigned)id->settingsHash,
             (unsigned)id->mmProfileDigest, (unsigned)id->comboFingerprint, id->crossingsFrozen ? 1 : 0,
             id->crossingsInHyrule, id->crossingsInTermina, (unsigned)id->crossingDigest);
}

bool IntegrationTest_PairedIdentityMatches(char* msg, size_t cap) {
    if (msg != nullptr && cap > 0) {
        msg[0] = '\0';
    }
    if (!sPairedIdentityRecorded) {
        if (msg != nullptr && cap > 0) {
            snprintf(msg, cap, "no identity was recorded after the creation");
        }
        return false;
    }
    PairedIdentity live;
    IntegrationTest_PairedIdentityCapture(&live);
    std::string diff;
    auto field = [&diff](const char* name, unsigned long long was, unsigned long long now) {
        if (was != now) {
            char part[96];
            snprintf(part, sizeof(part), "%s%s %llX -> %llX", diff.empty() ? "" : ", ", name, was, now);
            diff += part;
        }
    };
    field("paired", sPairedIdentity.pairingActive, live.pairingActive);
    field("masterSeed", sPairedIdentity.masterSeed, live.masterSeed);
    field("settingsHash", sPairedIdentity.settingsHash, live.settingsHash);
    field("mmProfileDigest", sPairedIdentity.mmProfileDigest, live.mmProfileDigest);
    field("comboFingerprint", sPairedIdentity.comboFingerprint, live.comboFingerprint);
    field("crossingDigest", sPairedIdentity.crossingDigest, live.crossingDigest);
    field("crossingsInHyrule", (unsigned long long)sPairedIdentity.crossingsInHyrule,
          (unsigned long long)live.crossingsInHyrule);
    field("crossingsInTermina", (unsigned long long)sPairedIdentity.crossingsInTermina,
          (unsigned long long)live.crossingsInTermina);
    field("crossingsFrozen", sPairedIdentity.crossingsFrozen, live.crossingsFrozen);
    if (diff.empty()) {
        return true;
    }
    if (msg != nullptr && cap > 0) {
        snprintf(msg, cap, "%s", diff.c_str());
    }
    return false;
}

} // extern "C"
