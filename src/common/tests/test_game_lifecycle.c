/**
 * @file test_game_lifecycle.c
 * @brief Unit tests for GameRunner lifecycle transitions
 *
 * These tests use mock GameOps to verify state machine correctness
 * without requiring actual game initialization.
 */

#include "../game_lifecycle.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * Mock game ops with call counters
 * ======================================================================== */

static int sInitCountA = 0, sInitCountB = 0;
static int sRunCountA = 0, sRunCountB = 0;
static int sSuspendCountA = 0, sSuspendCountB = 0;
static int sResumeCountA = 0, sResumeCountB = 0;
static int sShutdownCountA = 0, sShutdownCountB = 0;

static void ResetCounters(void) {
    sInitCountA = sInitCountB = 0;
    sRunCountA = sRunCountB = 0;
    sSuspendCountA = sSuspendCountB = 0;
    sResumeCountA = sResumeCountB = 0;
    sShutdownCountA = sShutdownCountB = 0;
}

static int MockA_Init(int argc, char** argv) { (void)argc; (void)argv; sInitCountA++; return 0; }
static void MockA_Run(void) { sRunCountA++; }
static void MockA_Suspend(void) { sSuspendCountA++; }
static void MockA_Resume(void) { sResumeCountA++; }
static void MockA_Shutdown(void) { sShutdownCountA++; }

static int MockB_Init(int argc, char** argv) { (void)argc; (void)argv; sInitCountB++; return 0; }
static void MockB_Run(void) { sRunCountB++; }
static void MockB_Suspend(void) { sSuspendCountB++; }
static void MockB_Resume(void) { sResumeCountB++; }
static void MockB_Shutdown(void) { sShutdownCountB++; }

static int MockFail_Init(int argc, char** argv) { (void)argc; (void)argv; return -1; }

static GameOps sMockOpsA = { "oot", "Mock OoT", MockA_Init, MockA_Run, MockA_Suspend, MockA_Resume, MockA_Shutdown };
static GameOps sMockOpsB = { "mm",  "Mock MM",  MockB_Init, MockB_Run, MockB_Suspend, MockB_Resume, MockB_Shutdown };
static GameOps sMockOpsFail = { "fail", "Fail", MockFail_Init, NULL, NULL, NULL, NULL };

/* ========================================================================
 * Tests
 * ======================================================================== */

#define TEST(name) static int name(void)
#define ASSERT(cond) do { if (!(cond)) { printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); return 1; } } while(0)

TEST(test_runner_init) {
    GameRunner r;
    GameRunner_Init(&r);
    ASSERT(GameRunner_GetActive(&r) == GAME_NONE);
    ASSERT(GameRunner_GetOps(&r, GAME_OOT) == NULL);
    ASSERT(GameRunner_GetOps(&r, GAME_MM) == NULL);
    ASSERT(GameRunner_GetState(&r, GAME_OOT) == GAME_LIFECYCLE_STATE_NONE);
    return 0;
}

TEST(test_register_games) {
    GameRunner r;
    GameRunner_Init(&r);
    ASSERT(GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA) == 0);
    ASSERT(GameRunner_RegisterGame(&r, GAME_MM, &sMockOpsB) == 0);
    ASSERT(GameRunner_GetOps(&r, GAME_OOT) == &sMockOpsA);
    ASSERT(GameRunner_GetOps(&r, GAME_MM) == &sMockOpsB);
    ASSERT(GameRunner_GetState(&r, GAME_OOT) == GAME_LIFECYCLE_STATE_READY);
    ASSERT(GameRunner_GetState(&r, GAME_MM) == GAME_LIFECYCLE_STATE_READY);
    return 0;
}

TEST(test_register_duplicate_rejected) {
    GameRunner r;
    GameRunner_Init(&r);
    ASSERT(GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA) == 0);
    ASSERT(GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA) == -1);  /* Duplicate */
    return 0;
}

TEST(test_register_invalid_id) {
    GameRunner r;
    GameRunner_Init(&r);
    ASSERT(GameRunner_RegisterGame(&r, GAME_NONE, &sMockOpsA) == -1);
    ASSERT(GameRunner_RegisterGame(&r, (GameId)99, &sMockOpsA) == -1);
    return 0;
}

TEST(test_start_game) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);

    ASSERT(GameRunner_StartGame(&r, GAME_OOT, 0, NULL) == 0);
    ASSERT(sInitCountA == 1);
    ASSERT(GameRunner_GetActive(&r) == GAME_OOT);
    ASSERT(GameRunner_GetState(&r, GAME_OOT) == GAME_LIFECYCLE_STATE_RUNNING);
    return 0;
}

TEST(test_start_unregistered_fails) {
    GameRunner r;
    GameRunner_Init(&r);
    ASSERT(GameRunner_StartGame(&r, GAME_OOT, 0, NULL) == -1);
    return 0;
}

TEST(test_start_while_active_fails) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);
    GameRunner_RegisterGame(&r, GAME_MM, &sMockOpsB);
    GameRunner_StartGame(&r, GAME_OOT, 0, NULL);

    ASSERT(GameRunner_StartGame(&r, GAME_MM, 0, NULL) == -1);
    ASSERT(sInitCountB == 0);  /* Should not have been called */
    return 0;
}

TEST(test_init_failure_propagated) {
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsFail);
    ASSERT(GameRunner_StartGame(&r, GAME_OOT, 0, NULL) == -1);
    ASSERT(GameRunner_GetActive(&r) == GAME_NONE);
    return 0;
}

TEST(test_switch_suspends_current) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);
    GameRunner_RegisterGame(&r, GAME_MM, &sMockOpsB);
    GameRunner_StartGame(&r, GAME_OOT, 0, NULL);

    ASSERT(GameRunner_SwitchTo(&r, GAME_MM, 0, NULL) == 0);
    ASSERT(sSuspendCountA == 1);  /* OoT suspended */
    ASSERT(sInitCountB == 1);     /* MM initialized */
    ASSERT(GameRunner_GetActive(&r) == GAME_MM);
    ASSERT(GameRunner_GetState(&r, GAME_OOT) == GAME_LIFECYCLE_STATE_SUSPENDED);
    ASSERT(GameRunner_GetState(&r, GAME_MM) == GAME_LIFECYCLE_STATE_RUNNING);
    return 0;
}

TEST(test_switch_back_resumes) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);
    GameRunner_RegisterGame(&r, GAME_MM, &sMockOpsB);
    GameRunner_StartGame(&r, GAME_OOT, 0, NULL);

    /* Switch OoT -> MM */
    GameRunner_SwitchTo(&r, GAME_MM, 0, NULL);
    /* Switch back MM -> OoT */
    GameRunner_SwitchTo(&r, GAME_OOT, 0, NULL);

    ASSERT(sResumeCountA == 1);  /* OoT resumed (not re-inited) */
    ASSERT(sInitCountA == 1);    /* Still only 1 init call */
    ASSERT(sSuspendCountB == 1); /* MM suspended */
    ASSERT(GameRunner_GetActive(&r) == GAME_OOT);
    ASSERT(GameRunner_GetState(&r, GAME_OOT) == GAME_LIFECYCLE_STATE_RUNNING);
    ASSERT(GameRunner_GetState(&r, GAME_MM) == GAME_LIFECYCLE_STATE_SUSPENDED);
    return 0;
}

TEST(test_switch_to_same_noop) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);
    GameRunner_StartGame(&r, GAME_OOT, 0, NULL);

    ASSERT(GameRunner_SwitchTo(&r, GAME_OOT, 0, NULL) == 0);
    ASSERT(sSuspendCountA == 0);  /* Not suspended */
    ASSERT(sResumeCountA == 0);   /* Not resumed */
    ASSERT(sInitCountA == 1);     /* Still just the original init */
    return 0;
}

TEST(test_shutdown_all) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);
    GameRunner_RegisterGame(&r, GAME_MM, &sMockOpsB);
    GameRunner_StartGame(&r, GAME_OOT, 0, NULL);
    GameRunner_SwitchTo(&r, GAME_MM, 0, NULL);

    GameRunner_ShutdownAll(&r);
    ASSERT(sShutdownCountA == 1);  /* OoT (was suspended) */
    ASSERT(sShutdownCountB == 1);  /* MM (was running) */
    ASSERT(GameRunner_GetActive(&r) == GAME_NONE);
    ASSERT(GameRunner_GetState(&r, GAME_OOT) == GAME_LIFECYCLE_STATE_STOPPED);
    ASSERT(GameRunner_GetState(&r, GAME_MM) == GAME_LIFECYCLE_STATE_STOPPED);
    return 0;
}

TEST(test_is_suspended) {
    ResetCounters();
    GameRunner r;
    GameRunner_Init(&r);
    GameRunner_RegisterGame(&r, GAME_OOT, &sMockOpsA);
    GameRunner_RegisterGame(&r, GAME_MM, &sMockOpsB);
    GameRunner_StartGame(&r, GAME_OOT, 0, NULL);

    ASSERT(!GameRunner_IsSuspended(&r, GAME_OOT));
    ASSERT(!GameRunner_IsSuspended(&r, GAME_MM));

    GameRunner_SwitchTo(&r, GAME_MM, 0, NULL);
    ASSERT(GameRunner_IsSuspended(&r, GAME_OOT));
    ASSERT(!GameRunner_IsSuspended(&r, GAME_MM));
    return 0;
}

/* ========================================================================
 * Test runner
 * ======================================================================== */

typedef struct { const char* name; int (*func)(void); } TestEntry;

static const TestEntry sLifecycleTests[] = {
    {"runner_init", test_runner_init},
    {"register_games", test_register_games},
    {"register_duplicate_rejected", test_register_duplicate_rejected},
    {"register_invalid_id", test_register_invalid_id},
    {"start_game", test_start_game},
    {"start_unregistered_fails", test_start_unregistered_fails},
    {"start_while_active_fails", test_start_while_active_fails},
    {"init_failure_propagated", test_init_failure_propagated},
    {"switch_suspends_current", test_switch_suspends_current},
    {"switch_back_resumes", test_switch_back_resumes},
    {"switch_to_same_noop", test_switch_to_same_noop},
    {"shutdown_all", test_shutdown_all},
    {"is_suspended", test_is_suspended},
    {NULL, NULL}
};

int TestLifecycle_RunAll(void) {
    int pass = 0, fail = 0;
    printf("[TEST] === Game Lifecycle Unit Tests ===\n\n");

    for (int i = 0; sLifecycleTests[i].name; i++) {
        printf("[TEST] %s... ", sLifecycleTests[i].name);
        int rc = sLifecycleTests[i].func();
        if (rc == 0) {
            printf("PASS\n");
            pass++;
        } else {
            fail++;
        }
    }

    printf("\n[TEST] Lifecycle tests: %d passed, %d failed\n", pass, fail);
    return fail;
}
