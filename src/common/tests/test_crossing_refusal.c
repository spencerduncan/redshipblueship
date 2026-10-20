/**
 * @file test_crossing_refusal.c
 * @brief Headless lock for #954: a switch into a game this session cannot run
 *        is refused while the running game is still in its frame loop, and a
 *        refusal the launcher makes after the loop returned re-enters the game
 *        through its suspend/resume contract.
 *
 * THE BUG. The launcher (rsbs/src/main.cpp) used to be the only place that
 * checked whether the target's archives exist (#317). By then the Happy Mask
 * Shop door had already made OoT's frame loop exit with the switch pending;
 * the refusal cleared the switch and ran OoT_Game_Run again. Its Main()
 * re-initialized the system arena under the gamestate the graph coroutine
 * still pointed at, and the first frame read the GraphicsContext out of freed
 * memory (OoT_GameState_SetFrameBuffer). The end-to-end locks are the
 * IntCrossingRefused* integration rows (CMake/CheckCrossingRefused.cmake):
 * NoMMArchive and NoPortArchive drive leg 1's door, NoOoTArchive leg 2's.
 *
 * THE LEGS. Every target status is forced through Switch_SetTargetStatusForTest,
 * so the row is the same with or without archives staged.
 *
 *   1. The OoT door into an unplayable MM (no game archive, no port archive):
 *      the REAL Combo_CheckEntranceSwitch hands back 0x01D1, outside the door,
 *      with no switch pending or requested and OoT's half not frozen: the
 *      refusal comes before the departure freeze.
 *   2. The MM door into an unplayable OoT (not booted, no game archive): 0xD800,
 *      outside the Clock Tower, with nothing pending and MM's half not frozen.
 *   3. A plain entrance with every target unplayable is handed back unchanged:
 *      the gate is asked only for a crossing.
 *   4. Control: the same OoT door into a playable MM crosses as before (the
 *      entrance comes back unchanged, the switch to MM is pending, OoT's half
 *      is frozen with its return entrance). Without it legs 1 and 2 would pass
 *      for a link table that never matched the door.
 *   5. F10 (Switch_RequestHotSwap): refused, no switch is requested; playable,
 *      it is.
 *   6. GameRunner_ReenterActive, the launcher's road back after a late
 *      refusal: the active game's suspend, then its resume, once each, and
 *      nothing else of either game's (no init, no run, no shutdown, the other
 *      game untouched); the game stays active and RUNNING. No active game:
 *      -1 and no call.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like test_roundtrip_integrity.c: ComboEntrance_Init and
 * Entrance_RegisterDefaultLinks are declared in entrance.h's C++ section.
 */

#include "../context.h"
#include "../entrance.h"
#include "../game_lifecycle.h"
#include "../save.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex);

#define CR_ASSERT(cond, msg)                                                    \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("[TEST] FAIL (crossing-refusal): %s  [%s]\n", (msg), #cond); \
            return TEST_FAIL;                                                   \
        }                                                                       \
    } while (0)

namespace {

// Fresh production links, nothing pending, no frozen halves.
bool CrResetCrossingState() {
    ComboEntrance_Init();
    Context_ClearAllFrozenStates();
    return Entrance_GetLinkCount() == 0 && !Combo_IsCrossGameSwitch() && !Combo_IsGameSwitchRequested() &&
           Entrance_RegisterDefaultLinks();
}

// ---- Mock games for GameRunner_ReenterActive ---------------------------------
// One call log for both games: each callback appends "<game><op>", so the
// order across games is in the string.
char sCrCalls[64];

void CrLog(const char* call) {
    strncat(sCrCalls, call, sizeof(sCrCalls) - strlen(sCrCalls) - 1);
}
int CrInitOoT(int, char**) {
    CrLog("oI ");
    return 0;
}
void CrRunOoT(void) {
    CrLog("oX ");
}
void CrSuspendOoT(void) {
    CrLog("oS ");
}
void CrResumeOoT(void) {
    CrLog("oR ");
}
void CrShutdownOoT(void) {
    CrLog("oD ");
}
int CrInitMM(int, char**) {
    CrLog("mI ");
    return 0;
}
void CrRunMM(void) {
    CrLog("mX ");
}
void CrSuspendMM(void) {
    CrLog("mS ");
}
void CrResumeMM(void) {
    CrLog("mR ");
}
void CrShutdownMM(void) {
    CrLog("mD ");
}

GameOps sCrOoT = { "oot", "Mock OoT", CrInitOoT, CrRunOoT, CrSuspendOoT, CrResumeOoT, CrShutdownOoT };
GameOps sCrMM = { "mm", "Mock MM", CrInitMM, CrRunMM, CrSuspendMM, CrResumeMM, CrShutdownMM };

TestResult CrRun(void) {
    const SwitchTargetStatus kUnplayable[] = { SWITCH_TARGET_NO_GAME_ARCHIVE, SWITCH_TARGET_NO_PORT_ARCHIVE,
                                               SWITCH_TARGET_OOT_NOT_BOOTED };

    // ---- 1. The OoT door into an unplayable MM --------------------------------
    Context_SetCurrentGame(GAME_OOT);
    for (SwitchTargetStatus status : kUnplayable) {
        CR_ASSERT(CrResetCrossingState(), "setup: the production links register on a clean table");
        Switch_SetTargetStatusForTest(status);
        const uint16_t loaded = Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
        printf("[TEST] OoT door, target status %d: entrance 0x%04X loads 0x%04X\n", (int)status,
               OOT_ENTR_HAPPY_MASK_SHOP, loaded);
        CR_ASSERT(loaded == OOT_ENTR_MARKET_FROM_MASK_SHOP,
                  "#954: a refused Happy Mask Shop door must hand back 0x01D1, outside the door");
        CR_ASSERT(!Combo_IsCrossGameSwitch() && !Combo_IsGameSwitchRequested(),
                  "#954: a refused door must leave no switch pending: OoT's loop exits on one");
        CR_ASSERT(Context_HasFrozenState(GAME_OOT) == 0,
                  "#954: the refusal must come before the departure freeze: OoT's half was frozen");
    }

    // ---- 2. The MM door into an unplayable OoT --------------------------------
    Context_SetCurrentGame(GAME_MM);
    for (SwitchTargetStatus status : kUnplayable) {
        CR_ASSERT(CrResetCrossingState(), "setup: the production links register on a clean table");
        Switch_SetTargetStatusForTest(status);
        const uint16_t loaded = Combo_CheckEntranceSwitch(MM_ENTR_CLOCK_TOWER_INTERIOR_1);
        printf("[TEST] MM door, target status %d: entrance 0x%04X loads 0x%04X\n", (int)status,
               MM_ENTR_CLOCK_TOWER_INTERIOR_1, loaded);
        CR_ASSERT(loaded == MM_ENTR_SOUTH_CLOCK_TOWN_0,
                  "#954: a refused Clock Tower door must hand back 0xD800, outside the Clock Tower");
        CR_ASSERT(!Combo_IsCrossGameSwitch() && !Combo_IsGameSwitchRequested(),
                  "#954: a refused door must leave no switch pending: MM's loop exits on one");
        CR_ASSERT(Context_HasFrozenState(GAME_MM) == 0,
                  "#954: the refusal must come before the departure freeze: MM's half was frozen");
    }

    // ---- 3. A plain entrance is never asked about ------------------------------
    Context_SetCurrentGame(GAME_OOT);
    CR_ASSERT(CrResetCrossingState(), "setup: the production links register on a clean table");
    Switch_SetTargetStatusForTest(SWITCH_TARGET_NO_GAME_ARCHIVE);
    CR_ASSERT(Combo_CheckEntranceSwitch(OOT_ENTR_MARKET_FROM_MASK_SHOP) == OOT_ENTR_MARKET_FROM_MASK_SHOP,
              "an entrance that is not a crossing must come back unchanged");
    CR_ASSERT(!Combo_IsCrossGameSwitch(), "an entrance that is not a crossing must not register a switch");

    // ---- 4. Control: the same door into a playable MM crosses -----------------
    CR_ASSERT(CrResetCrossingState(), "setup: the production links register on a clean table");
    Switch_SetTargetStatusForTest(SWITCH_TARGET_PLAYABLE);
    const int prevSlot = RsbsSave_GetActiveSlot(); // the crossing publishes OoT's fileNum
    const uint16_t crossed = Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
    RsbsSave_SetActiveSlot(prevSlot);
    CR_ASSERT(crossed == OOT_ENTR_HAPPY_MASK_SHOP, "control: a crossing that goes ahead hands its entrance back");
    CR_ASSERT(Combo_IsCrossGameSwitch() && Entrance_GetSwitchTargetGame() == GAME_MM,
              "control: the Happy Mask Shop door into a playable MM must leave the switch to MM pending");
    CR_ASSERT(Context_HasFrozenState(GAME_OOT) == 1 &&
                  Context_GetFrozenReturnEntrance(GAME_OOT) == OOT_ENTR_MARKET_FROM_MASK_SHOP,
              "control: the crossing must freeze OoT's half with its return entrance");

    // ---- 5. F10 ---------------------------------------------------------------
    CR_ASSERT(CrResetCrossingState(), "setup: the production links register on a clean table");
    Switch_SetTargetStatusForTest(SWITCH_TARGET_NO_PORT_ARCHIVE);
    CR_ASSERT(Switch_RequestHotSwap() == 0 && !Combo_IsGameSwitchRequested(),
              "#954: F10 into an unplayable game must be refused while the running game is in its loop");
    Switch_SetTargetStatusForTest(SWITCH_TARGET_PLAYABLE);
    CR_ASSERT(Switch_RequestHotSwap() == 1 && Combo_IsGameSwitchRequested(),
              "control: F10 into a playable game must request the switch");
    Combo_ClearGameSwitchRequest();

    // ---- 6. GameRunner_ReenterActive ------------------------------------------
    GameRunner runner;
    GameRunner_Init(&runner);
    sCrCalls[0] = '\0';
    CR_ASSERT(GameRunner_ReenterActive(&runner) == -1 && sCrCalls[0] == '\0',
              "with no active game, re-entry must fail and call nothing");
    GameRunner_RegisterGame(&runner, GAME_OOT, &sCrOoT);
    GameRunner_RegisterGame(&runner, GAME_MM, &sCrMM);
    CR_ASSERT(GameRunner_StartGame(&runner, GAME_OOT, 0, nullptr) == 0, "setup: the mock OoT starts");
    sCrCalls[0] = '\0';
    CR_ASSERT(GameRunner_ReenterActive(&runner) == 0, "re-entry of the active game must succeed");
    printf("[TEST] GameRunner_ReenterActive called: %s\n", sCrCalls);
    CR_ASSERT(strcmp(sCrCalls, "oS oR ") == 0,
              "#954: re-entry must be the active game's suspend then its resume, and nothing else of either game");
    CR_ASSERT(GameRunner_GetActive(&runner) == GAME_OOT &&
                  GameRunner_GetState(&runner, GAME_OOT) == GAME_LIFECYCLE_STATE_RUNNING &&
                  GameRunner_GetState(&runner, GAME_MM) == GAME_LIFECYCLE_STATE_READY,
              "re-entry must leave the same game active and RUNNING, and the other game as it was");
    return TEST_PASS;
}

} // namespace

TestResult Test_CrossingRefusal(void) {
    const GameId prevGame = Context_GetCurrentGame();
    const TestResult result = CrRun();
    // Leave the process as the next row expects it, on pass or fail.
    Switch_SetTargetStatusForTest(-1);
    Context_SetCurrentGame(prevGame);
    ComboEntrance_Init();
    (void)Entrance_RegisterDefaultLinks();
    Context_ClearAllFrozenStates();
    if (result == TEST_PASS) {
        printf("[TEST] crossing-refusal: PASS\n");
    }
    return result;
}
