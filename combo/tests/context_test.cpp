/**
 * @file context_test.cpp
 * @brief Tests for cross-game context and state management
 *
 * Tests the FrozenStateManager, ComboContext, and related APIs
 * for MM first-entry SaveContext initialization.
 */

#include <gtest/gtest.h>
#include "context.h"
#include "entrance.h"
#include <cstring>

// ============================================================================
// FrozenStateManager Tests
// ============================================================================

class FrozenStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the frozen state manager before each test
        Context_InitFrozenStates();
        Context_ClearAllFrozenStates();
    }

    void TearDown() override {
        Context_ClearAllFrozenStates();
    }
};

TEST_F(FrozenStateTest, InitialStateIsNotFrozen) {
    EXPECT_FALSE(Context_HasFrozenState(GAME_OOT));
    EXPECT_FALSE(Context_HasFrozenState(GAME_MM));
}

TEST_F(FrozenStateTest, FreezeOoTState) {
    // Create a mock save context
    uint8_t mockSaveContext[OOT_SAVE_CONTEXT_SIZE];
    memset(mockSaveContext, 0xAB, sizeof(mockSaveContext));

    // Freeze the state
    Context_FreezeState(GAME_OOT, 0x0433, mockSaveContext, sizeof(mockSaveContext));

    EXPECT_TRUE(Context_HasFrozenState(GAME_OOT));
    EXPECT_FALSE(Context_HasFrozenState(GAME_MM));
    EXPECT_EQ(Context_GetFrozenReturnEntrance(GAME_OOT), 0x0433);
}

TEST_F(FrozenStateTest, FreezeMMState) {
    // Create a mock save context
    uint8_t mockSaveContext[MM_SAVE_CONTEXT_SIZE];
    memset(mockSaveContext, 0xCD, sizeof(mockSaveContext));

    // Freeze the state
    Context_FreezeState(GAME_MM, 0xC010, mockSaveContext, sizeof(mockSaveContext));

    EXPECT_FALSE(Context_HasFrozenState(GAME_OOT));
    EXPECT_TRUE(Context_HasFrozenState(GAME_MM));
    EXPECT_EQ(Context_GetFrozenReturnEntrance(GAME_MM), 0xC010);
}

TEST_F(FrozenStateTest, RestoreOoTState) {
    // Create and freeze a mock save context
    uint8_t originalContext[OOT_SAVE_CONTEXT_SIZE];
    memset(originalContext, 0xAB, sizeof(originalContext));
    originalContext[0] = 0x12;
    originalContext[100] = 0x34;

    Context_FreezeState(GAME_OOT, 0x0433, originalContext, sizeof(originalContext));

    // Restore to a different buffer
    uint8_t restoredContext[OOT_SAVE_CONTEXT_SIZE];
    memset(restoredContext, 0, sizeof(restoredContext));

    int result = Context_RestoreState(GAME_OOT, restoredContext, sizeof(restoredContext));

    EXPECT_EQ(result, 1);
    EXPECT_EQ(restoredContext[0], 0x12);
    EXPECT_EQ(restoredContext[100], 0x34);
    EXPECT_EQ(memcmp(originalContext, restoredContext, sizeof(originalContext)), 0);
}

TEST_F(FrozenStateTest, RestoreMMState) {
    // Create and freeze a mock save context
    uint8_t originalContext[MM_SAVE_CONTEXT_SIZE];
    memset(originalContext, 0xCD, sizeof(originalContext));
    originalContext[0] = 0x56;
    originalContext[1000] = 0x78;

    Context_FreezeState(GAME_MM, 0xD800, originalContext, sizeof(originalContext));

    // Restore to a different buffer
    uint8_t restoredContext[MM_SAVE_CONTEXT_SIZE];
    memset(restoredContext, 0, sizeof(restoredContext));

    int result = Context_RestoreState(GAME_MM, restoredContext, sizeof(restoredContext));

    EXPECT_EQ(result, 1);
    EXPECT_EQ(restoredContext[0], 0x56);
    EXPECT_EQ(restoredContext[1000], 0x78);
}

TEST_F(FrozenStateTest, RestoreWithoutFreezeReturnsFalse) {
    uint8_t buffer[OOT_SAVE_CONTEXT_SIZE];
    int result = Context_RestoreState(GAME_OOT, buffer, sizeof(buffer));
    EXPECT_EQ(result, 0);
}

TEST_F(FrozenStateTest, ClearFrozenState) {
    uint8_t mockContext[OOT_SAVE_CONTEXT_SIZE];
    memset(mockContext, 0xAB, sizeof(mockContext));

    Context_FreezeState(GAME_OOT, 0x0433, mockContext, sizeof(mockContext));
    EXPECT_TRUE(Context_HasFrozenState(GAME_OOT));

    Context_ClearFrozenState(GAME_OOT);
    EXPECT_FALSE(Context_HasFrozenState(GAME_OOT));
}

TEST_F(FrozenStateTest, BothGamesCanBeFrozenIndependently) {
    uint8_t ootContext[OOT_SAVE_CONTEXT_SIZE];
    uint8_t mmContext[MM_SAVE_CONTEXT_SIZE];
    memset(ootContext, 0xAB, sizeof(ootContext));
    memset(mmContext, 0xCD, sizeof(mmContext));

    Context_FreezeState(GAME_OOT, 0x0433, ootContext, sizeof(ootContext));
    Context_FreezeState(GAME_MM, 0xC010, mmContext, sizeof(mmContext));

    EXPECT_TRUE(Context_HasFrozenState(GAME_OOT));
    EXPECT_TRUE(Context_HasFrozenState(GAME_MM));
    EXPECT_EQ(Context_GetFrozenReturnEntrance(GAME_OOT), 0x0433);
    EXPECT_EQ(Context_GetFrozenReturnEntrance(GAME_MM), 0xC010);
}

// ============================================================================
// ComboContext Tests
// ============================================================================

class ComboContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        ComboContext_Init();
    }
};

TEST_F(ComboContextTest, InitializesWithCorrectMagic) {
    EXPECT_EQ(strncmp(gComboCtx.magic, "OoT+MM<3", 8), 0);
}

TEST_F(ComboContextTest, InitializesWithNoSwitchPending) {
    EXPECT_FALSE(ComboContext_IsSwitchPending());
    EXPECT_EQ(gComboCtx.targetGame, GAME_NONE);
    EXPECT_EQ(gComboCtx.sourceGame, GAME_NONE);
}

TEST_F(ComboContextTest, RequestSwitchSetsFields) {
    ComboContext_RequestSwitch(GAME_MM, 0xC010);

    EXPECT_TRUE(ComboContext_IsSwitchPending());
    EXPECT_EQ(gComboCtx.targetGame, GAME_MM);
    EXPECT_EQ(gComboCtx.targetEntrance, 0xC010);
}

TEST_F(ComboContextTest, ClearSwitchResetsFields) {
    ComboContext_RequestSwitch(GAME_MM, 0xC010);
    ComboContext_ClearSwitch();

    EXPECT_FALSE(ComboContext_IsSwitchPending());
    EXPECT_EQ(gComboCtx.targetGame, GAME_NONE);
    EXPECT_EQ(gComboCtx.targetEntrance, 0);
}

TEST_F(ComboContextTest, RandoStateFields) {
    // Test the new rando state propagation fields
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 12345;

    EXPECT_TRUE(gComboCtx.sourceIsRando);
    EXPECT_EQ(gComboCtx.sharedRandoSeed, 12345u);

    // Reset and verify initialization
    ComboContext_Init();
    EXPECT_FALSE(gComboCtx.sourceIsRando);
    EXPECT_EQ(gComboCtx.sharedRandoSeed, 0u);
}

// ============================================================================
// High-Level Context API Tests
// ============================================================================

class ContextAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        Context_Init();
    }
};

TEST_F(ContextAPITest, InitializesBothSystems) {
    // Frozen states should be clear
    EXPECT_FALSE(Context_HasFrozenState(GAME_OOT));
    EXPECT_FALSE(Context_HasFrozenState(GAME_MM));

    // No switch should be pending
    EXPECT_FALSE(Context_HasPendingSwitch());

    // Current game should be NONE initially
    EXPECT_EQ(Context_GetCurrentGame(), GAME_NONE);
}

TEST_F(ContextAPITest, SetAndGetCurrentGame) {
    Context_SetCurrentGame(GAME_OOT);
    EXPECT_EQ(Context_GetCurrentGame(), GAME_OOT);

    Context_SetCurrentGame(GAME_MM);
    EXPECT_EQ(Context_GetCurrentGame(), GAME_MM);
}

TEST_F(ContextAPITest, RequestSwitchSetsSourceGame) {
    Context_SetCurrentGame(GAME_OOT);
    Context_RequestSwitch(GAME_MM, 0xC010);

    EXPECT_TRUE(Context_HasPendingSwitch());
    EXPECT_EQ(gComboCtx.sourceGame, GAME_OOT);
    EXPECT_EQ(gComboCtx.targetGame, GAME_MM);
}

// ============================================================================
// Entrance System Tests
// ============================================================================

class EntranceTest : public ::testing::Test {
protected:
    void SetUp() override {
        Entrance_Init();
    }

    void TearDown() override {
        Entrance_ClearLinks();
        Entrance_ClearPendingSwitch();
    }
};

TEST_F(EntranceTest, InitialStateHasNoLinks) {
    EXPECT_EQ(Entrance_GetLinkCount(), 0u);
    EXPECT_FALSE(Entrance_IsCrossGameSwitch());
}

TEST_F(EntranceTest, RegisterTestLinksAddsTwoLinks) {
    Entrance_RegisterTestLinks();
    // Bidirectional = 2 links
    EXPECT_EQ(Entrance_GetLinkCount(), 2u);
}

TEST_F(EntranceTest, RegisterDefaultLinksAddsTwoLinks) {
    Entrance_RegisterDefaultLinks();
    // Bidirectional = 2 links
    EXPECT_EQ(Entrance_GetLinkCount(), 2u);
}

TEST_F(EntranceTest, CheckCrossGameTriggersSwitch) {
    Entrance_RegisterTestLinks();

    // Check entrance from OoT Mido's House
    uint16_t result = Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);

    // Should have set up pending switch
    EXPECT_TRUE(Entrance_IsCrossGameSwitch());
    EXPECT_EQ(Entrance_GetSwitchTargetGame(), GAME_MM);
    EXPECT_EQ(Entrance_GetSwitchTargetEntrance(), MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    EXPECT_EQ(Entrance_GetSwitchReturnEntrance(), OOT_ENTR_KOKIRI_FROM_MIDOS);
}

TEST_F(EntranceTest, CheckNonCrossGameEntranceReturnsSame) {
    Entrance_RegisterTestLinks();

    // Check a non-cross-game entrance
    uint16_t testEntrance = 0x1234;
    uint16_t result = Entrance_CheckCrossGame(GAME_OOT, testEntrance);

    // Should return the same entrance, no switch pending
    EXPECT_EQ(result, testEntrance);
    EXPECT_FALSE(Entrance_IsCrossGameSwitch());
}

TEST_F(EntranceTest, StartupEntranceAPI) {
    EXPECT_EQ(Entrance_GetStartupEntrance(), 0u);

    Entrance_SetStartupEntrance(0xC010);
    EXPECT_EQ(Entrance_GetStartupEntrance(), 0xC010);

    Entrance_ClearStartupEntrance();
    EXPECT_EQ(Entrance_GetStartupEntrance(), 0u);
}

TEST_F(EntranceTest, ClearPendingSwitch) {
    Entrance_RegisterTestLinks();
    Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);
    EXPECT_TRUE(Entrance_IsCrossGameSwitch());

    Entrance_ClearPendingSwitch();
    EXPECT_FALSE(Entrance_IsCrossGameSwitch());
}

// ============================================================================
// C API Compatibility Tests (Combo_* functions)
// ============================================================================

class CAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        Context_Init();
        Entrance_Init();
    }

    void TearDown() override {
        Entrance_ClearLinks();
    }
};

TEST_F(CAPITest, ComboFreezeStateWithStringId) {
    uint8_t mockContext[OOT_SAVE_CONTEXT_SIZE];
    memset(mockContext, 0xAB, sizeof(mockContext));

    Combo_FreezeState("oot", 0x0433, mockContext, sizeof(mockContext));

    EXPECT_TRUE(Combo_HasFrozenState("oot"));
    EXPECT_FALSE(Combo_HasFrozenState("mm"));
    EXPECT_EQ(Combo_GetFrozenReturnEntrance("oot"), 0x0433);
}

TEST_F(CAPITest, ComboRestoreStateWithStringId) {
    uint8_t originalContext[MM_SAVE_CONTEXT_SIZE];
    memset(originalContext, 0xCD, sizeof(originalContext));
    originalContext[0] = 0x99;

    Combo_FreezeState("mm", 0xD800, originalContext, sizeof(originalContext));

    uint8_t restoredContext[MM_SAVE_CONTEXT_SIZE];
    int result = Combo_RestoreState("mm", restoredContext, sizeof(restoredContext));

    EXPECT_EQ(result, 1);
    EXPECT_EQ(restoredContext[0], 0x99);
}

TEST_F(CAPITest, ComboCheckCrossGameEntrance) {
    Entrance_RegisterTestLinks();

    uint16_t result = Combo_CheckCrossGameEntrance("oot", OOT_ENTR_MIDOS_HOUSE);

    EXPECT_TRUE(Combo_IsCrossGameSwitch());
    EXPECT_STREQ(Combo_GetSwitchTargetGameId(), "mm");
    EXPECT_EQ(Combo_GetSwitchTargetEntrance(), MM_ENTR_CLOCK_TOWER_INTERIOR_1);
}

TEST_F(CAPITest, ComboStartupEntranceAPI) {
    EXPECT_EQ(Combo_GetStartupEntrance(), 0u);

    Combo_SetStartupEntrance(0xC010);
    EXPECT_EQ(Combo_GetStartupEntrance(), 0xC010);

    Combo_ClearStartupEntrance();
    EXPECT_EQ(Combo_GetStartupEntrance(), 0u);
}

// ============================================================================
// Integration Tests for First-Entry Flow
// ============================================================================

TEST(IntegrationTest, FirstSwitchToMMSetsUpCorrectState) {
    // Initialize everything
    Context_Init();
    Entrance_Init();
    Entrance_RegisterTestLinks();

    // Set OoT as current game (simulating startup)
    Context_SetCurrentGame(GAME_OOT);

    // Simulate OoT triggering a cross-game entrance
    Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);

    // Verify switch is ready
    EXPECT_TRUE(Entrance_IsCrossGameSwitch());
    EXPECT_EQ(Entrance_GetSwitchTargetGame(), GAME_MM);

    // MM should NOT have frozen state (first entry)
    EXPECT_FALSE(Context_HasFrozenState(GAME_MM));

    // OoT should be able to freeze its state
    uint8_t ootContext[OOT_SAVE_CONTEXT_SIZE];
    memset(ootContext, 0xAB, sizeof(ootContext));
    Context_FreezeState(GAME_OOT, Entrance_GetSwitchReturnEntrance(), ootContext, sizeof(ootContext));
    EXPECT_TRUE(Context_HasFrozenState(GAME_OOT));

    // Cleanup
    Context_ClearAllFrozenStates();
    Entrance_ClearLinks();
}

TEST(IntegrationTest, ReturnSwitchToMMRestoresFrozenState) {
    // Initialize everything
    Context_Init();
    Entrance_Init();

    // Pre-freeze MM state (simulating previous session)
    uint8_t mmContext[MM_SAVE_CONTEXT_SIZE];
    memset(mmContext, 0xCD, sizeof(mmContext));
    mmContext[0] = 0x42;  // Marker byte
    Context_FreezeState(GAME_MM, 0xD800, mmContext, sizeof(mmContext));

    // MM should have frozen state
    EXPECT_TRUE(Context_HasFrozenState(GAME_MM));

    // Restore and verify
    uint8_t restoredContext[MM_SAVE_CONTEXT_SIZE];
    int result = Context_RestoreState(GAME_MM, restoredContext, sizeof(restoredContext));

    EXPECT_EQ(result, 1);
    EXPECT_EQ(restoredContext[0], 0x42);

    // Cleanup
    Context_ClearAllFrozenStates();
}

// ============================================================================
// Return-path regression tests for issue #170
// ============================================================================
//
// Before #170, Combo_CheckEntranceSwitch hardcoded "oot" as the source game
// even when invoked from MM's z_play.c. That meant MM→OoT transitions never
// resolved a matching cross-game link, and MM's SaveContext was never frozen.
// These tests lock in the fix by exercising the C API that the game code
// actually calls, asserting it works from both sides.

TEST(ReturnPathTest, MMSideCheckFindsReturnEntrance) {
    Context_Init();
    Entrance_Init();
    Entrance_RegisterTestLinks();

    // Simulate MM stepping onto the return entrance (South Clock Town spawn 0).
    // Using the C API with an explicit "mm" game id — this is the pathway the
    // production fix takes via Context_GetCurrentGame() dispatch.
    Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);

    EXPECT_TRUE(Combo_IsCrossGameSwitch());
    EXPECT_STREQ(Combo_GetSwitchTargetGameId(), "oot");
    EXPECT_EQ(Combo_GetSwitchTargetEntrance(), OOT_ENTR_KOKIRI_FROM_MIDOS);

    Entrance_ClearPendingSwitch();
    Entrance_ClearLinks();
}

TEST(ReturnPathTest, OoTSideCheckDoesNotMatchMMEntrance) {
    // Regression guard: the buggy implementation checked "oot" links for MM
    // entrance 0xD800, which would silently match nothing. Verify that when
    // the right game id ("oot") is paired with an MM entrance, no switch
    // triggers — that's the correct negative behavior.
    Context_Init();
    Entrance_Init();
    Entrance_RegisterTestLinks();

    Combo_CheckCrossGameEntrance("oot", MM_ENTR_SOUTH_CLOCK_TOWN_0);
    EXPECT_FALSE(Combo_IsCrossGameSwitch());

    Entrance_ClearPendingSwitch();
    Entrance_ClearLinks();
}

TEST(ReturnPathTest, FullRoundtripPreservesOoTSaveContext) {
    Context_Init();
    Entrance_Init();
    Entrance_RegisterTestLinks();

    // --- Leg 1: OoT → MM ---
    // Populate OoT SaveContext with a recognizable fingerprint.
    uint8_t ootSave[OOT_SAVE_CONTEXT_SIZE];
    memset(ootSave, 0x11, sizeof(ootSave));
    ootSave[0] = 'O'; ootSave[1] = 'o'; ootSave[2] = 'T'; ootSave[3] = '!';

    // OoT's entrance hook fires while OoT is current.
    Context_SetCurrentGame(GAME_OOT);
    Combo_CheckCrossGameEntrance("oot", OOT_ENTR_MIDOS_HOUSE);
    EXPECT_TRUE(Combo_IsCrossGameSwitch());
    uint16_t ootReturn = Combo_GetSwitchReturnEntrance();
    Combo_FreezeState("oot", ootReturn, ootSave, sizeof(ootSave));
    Entrance_ClearPendingSwitch();

    // --- Leg 2: pretend to run MM and freeze MM state ---
    Context_SetCurrentGame(GAME_MM);
    uint8_t mmSave[MM_SAVE_CONTEXT_SIZE];
    memset(mmSave, 0x22, sizeof(mmSave));
    mmSave[0] = 'M'; mmSave[1] = 'M';

    Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);
    EXPECT_TRUE(Combo_IsCrossGameSwitch());
    EXPECT_STREQ(Combo_GetSwitchTargetGameId(), "oot");
    uint16_t mmReturn = Combo_GetSwitchReturnEntrance();
    Combo_FreezeState("mm", mmReturn, mmSave, sizeof(mmSave));
    Entrance_ClearPendingSwitch();

    // --- Leg 3: restore OoT on return, verify data integrity ---
    uint8_t restoredOoT[OOT_SAVE_CONTEXT_SIZE] = {0};
    int result = Combo_RestoreState("oot", restoredOoT, sizeof(restoredOoT));
    EXPECT_EQ(result, 1);
    EXPECT_EQ(restoredOoT[0], 'O');
    EXPECT_EQ(restoredOoT[1], 'o');
    EXPECT_EQ(restoredOoT[2], 'T');
    EXPECT_EQ(restoredOoT[3], '!');
    EXPECT_EQ(Combo_GetFrozenReturnEntrance("oot"), OOT_ENTR_KOKIRI_FROM_MIDOS);

    // MM's frozen state still sits alongside OoT's — both coexist.
    uint8_t restoredMM[MM_SAVE_CONTEXT_SIZE] = {0};
    EXPECT_EQ(Combo_RestoreState("mm", restoredMM, sizeof(restoredMM)), 1);
    EXPECT_EQ(restoredMM[0], 'M');

    // Cleanup
    Context_ClearAllFrozenStates();
    Entrance_ClearLinks();
}

TEST(ReturnPathTest, RestoreWithoutFreezeIsSafe) {
    // Edge case: first boot of a game — OoT_Game_Resume could be reached
    // without any prior freeze (e.g. if GameRunner misbehaves). The restore
    // must refuse rather than memcpy zeros over a live SaveContext.
    Context_Init();
    Context_ClearAllFrozenStates();

    uint8_t buffer[OOT_SAVE_CONTEXT_SIZE];
    memset(buffer, 0xFF, sizeof(buffer));
    int result = Context_RestoreState(GAME_OOT, buffer, sizeof(buffer));
    EXPECT_EQ(result, 0);
    EXPECT_EQ(buffer[0], 0xFF);  // Untouched
}
