#include <gtest/gtest.h>
#include "combo/FrozenState.h"
#include <cstring>

using namespace Combo;

class FrozenStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Re-initialize for each test
        gFrozenStates = FrozenStateManager();
        gFrozenStates.Initialize();
    }
};

// ============================================================================
// FrozenStateManager basic tests
// ============================================================================

TEST_F(FrozenStateTest, InitialStateNotFrozen) {
    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::OoT));
    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::MM));
}

TEST_F(FrozenStateTest, InitialReturnEntranceIsZero) {
    EXPECT_EQ(gFrozenStates.GetReturnEntrance(Game::OoT), 0);
    EXPECT_EQ(gFrozenStates.GetReturnEntrance(Game::MM), 0);
}

TEST_F(FrozenStateTest, SaveContextPointersValidAfterInit) {
    // Both SaveContexts should be valid (zero-padded) after init
    const void* ootCtx = gFrozenStates.GetOoTSaveContext();
    const void* mmCtx = gFrozenStates.GetMMSaveContext();

    EXPECT_NE(ootCtx, nullptr);
    EXPECT_NE(mmCtx, nullptr);
}

TEST_F(FrozenStateTest, SaveContextSizesCorrect) {
    EXPECT_EQ(gFrozenStates.GetOoTSaveContextSize(), OOT_SAVE_CONTEXT_SIZE);
    EXPECT_EQ(gFrozenStates.GetMMSaveContextSize(), MM_SAVE_CONTEXT_SIZE);
}

// ============================================================================
// Freeze/Restore tests
// ============================================================================

TEST_F(FrozenStateTest, FreezeOoTState) {
    // Create test data
    std::vector<uint8_t> testData(OOT_SAVE_CONTEXT_SIZE, 0);
    testData[0] = 0xDE;
    testData[1] = 0xAD;
    testData[2] = 0xBE;
    testData[3] = 0xEF;
    testData[OOT_SAVE_CONTEXT_SIZE - 1] = 0xFF;

    gFrozenStates.FreezeState(Game::OoT, 0x1D1, testData.data(), testData.size());

    EXPECT_TRUE(gFrozenStates.HasFrozenState(Game::OoT));
    EXPECT_EQ(gFrozenStates.GetReturnEntrance(Game::OoT), 0x1D1);

    // Verify data was stored
    const uint8_t* storedData = static_cast<const uint8_t*>(gFrozenStates.GetOoTSaveContext());
    EXPECT_EQ(storedData[0], 0xDE);
    EXPECT_EQ(storedData[1], 0xAD);
    EXPECT_EQ(storedData[2], 0xBE);
    EXPECT_EQ(storedData[3], 0xEF);
    EXPECT_EQ(storedData[OOT_SAVE_CONTEXT_SIZE - 1], 0xFF);
}

TEST_F(FrozenStateTest, FreezeMMState) {
    // Create test data
    std::vector<uint8_t> testData(MM_SAVE_CONTEXT_SIZE, 0);
    testData[0] = 0xCA;
    testData[1] = 0xFE;
    testData[MM_SAVE_CONTEXT_SIZE - 1] = 0xAA;

    gFrozenStates.FreezeState(Game::MM, 0xC010, testData.data(), testData.size());

    EXPECT_TRUE(gFrozenStates.HasFrozenState(Game::MM));
    EXPECT_EQ(gFrozenStates.GetReturnEntrance(Game::MM), 0xC010);

    // Verify data was stored
    const uint8_t* storedData = static_cast<const uint8_t*>(gFrozenStates.GetMMSaveContext());
    EXPECT_EQ(storedData[0], 0xCA);
    EXPECT_EQ(storedData[1], 0xFE);
    EXPECT_EQ(storedData[MM_SAVE_CONTEXT_SIZE - 1], 0xAA);
}

TEST_F(FrozenStateTest, RestoreOoTState) {
    // Freeze first
    std::vector<uint8_t> testData(OOT_SAVE_CONTEXT_SIZE, 0);
    testData[0] = 0x12;
    testData[100] = 0x34;
    testData[1000] = 0x56;

    gFrozenStates.FreezeState(Game::OoT, 0x530, testData.data(), testData.size());

    // Restore to a new buffer
    std::vector<uint8_t> restoredData(OOT_SAVE_CONTEXT_SIZE, 0xFF);
    bool success = gFrozenStates.RestoreState(Game::OoT, restoredData.data(), restoredData.size());

    EXPECT_TRUE(success);
    EXPECT_EQ(restoredData[0], 0x12);
    EXPECT_EQ(restoredData[100], 0x34);
    EXPECT_EQ(restoredData[1000], 0x56);
}

TEST_F(FrozenStateTest, RestoreWithoutFreezeReturnsFalse) {
    // No freeze has happened
    std::vector<uint8_t> buffer(OOT_SAVE_CONTEXT_SIZE, 0);

    bool success = gFrozenStates.RestoreState(Game::OoT, buffer.data(), buffer.size());

    EXPECT_FALSE(success);
}

TEST_F(FrozenStateTest, RestoreDoesNotAffectOtherGame) {
    // Freeze OoT
    std::vector<uint8_t> ootData(OOT_SAVE_CONTEXT_SIZE, 0xAA);
    gFrozenStates.FreezeState(Game::OoT, 0x530, ootData.data(), ootData.size());

    // MM should still not be frozen
    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::MM));

    // Attempting to restore MM should fail
    std::vector<uint8_t> buffer(MM_SAVE_CONTEXT_SIZE, 0);
    bool success = gFrozenStates.RestoreState(Game::MM, buffer.data(), buffer.size());
    EXPECT_FALSE(success);
}

// ============================================================================
// Clear tests
// ============================================================================

TEST_F(FrozenStateTest, ClearOoTFrozenState) {
    // Freeze
    std::vector<uint8_t> testData(OOT_SAVE_CONTEXT_SIZE, 0xBB);
    gFrozenStates.FreezeState(Game::OoT, 0x530, testData.data(), testData.size());
    EXPECT_TRUE(gFrozenStates.HasFrozenState(Game::OoT));

    // Clear
    gFrozenStates.ClearFrozenState(Game::OoT);

    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::OoT));
    EXPECT_EQ(gFrozenStates.GetReturnEntrance(Game::OoT), 0);
}

TEST_F(FrozenStateTest, ClearAllFrozenStates) {
    // Freeze both
    std::vector<uint8_t> ootData(OOT_SAVE_CONTEXT_SIZE, 0x11);
    std::vector<uint8_t> mmData(MM_SAVE_CONTEXT_SIZE, 0x22);

    gFrozenStates.FreezeState(Game::OoT, 0x530, ootData.data(), ootData.size());
    gFrozenStates.FreezeState(Game::MM, 0xC010, mmData.data(), mmData.size());

    EXPECT_TRUE(gFrozenStates.HasFrozenState(Game::OoT));
    EXPECT_TRUE(gFrozenStates.HasFrozenState(Game::MM));

    // Clear all
    gFrozenStates.ClearAll();

    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::OoT));
    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::MM));
}

// ============================================================================
// Shadow copy tests (for trackers)
// ============================================================================

TEST_F(FrozenStateTest, UpdateShadowCopyDoesNotSetFrozen) {
    // Shadow copy should update data without marking as frozen
    std::vector<uint8_t> testData(OOT_SAVE_CONTEXT_SIZE, 0x77);

    gFrozenStates.UpdateShadowCopy(Game::OoT, testData.data(), testData.size());

    // Data should be updated
    const uint8_t* storedData = static_cast<const uint8_t*>(gFrozenStates.GetOoTSaveContext());
    EXPECT_EQ(storedData[0], 0x77);

    // But state should not be marked as frozen
    EXPECT_FALSE(gFrozenStates.HasFrozenState(Game::OoT));
}

TEST_F(FrozenStateTest, ShadowCopyAndFreezeAreIndependent) {
    // First update shadow
    std::vector<uint8_t> shadowData(MM_SAVE_CONTEXT_SIZE, 0x33);
    gFrozenStates.UpdateShadowCopy(Game::MM, shadowData.data(), shadowData.size());

    // Then freeze with different data
    std::vector<uint8_t> freezeData(MM_SAVE_CONTEXT_SIZE, 0x44);
    gFrozenStates.FreezeState(Game::MM, 0xD800, freezeData.data(), freezeData.size());

    // Should be frozen now
    EXPECT_TRUE(gFrozenStates.HasFrozenState(Game::MM));

    // Data should be from the freeze, not shadow update
    const uint8_t* storedData = static_cast<const uint8_t*>(gFrozenStates.GetMMSaveContext());
    EXPECT_EQ(storedData[0], 0x44);
}

// ============================================================================
// C API tests
// ============================================================================

TEST_F(FrozenStateTest, CAPI_FreezeAndRestore) {
    // Test through C API
    std::vector<uint8_t> testData(OOT_SAVE_CONTEXT_SIZE, 0);
    testData[0] = 0xAB;
    testData[1] = 0xCD;

    Combo_FreezeState("oot", 0x530, testData.data(), testData.size());

    EXPECT_EQ(Combo_HasFrozenState("oot"), 1);
    EXPECT_EQ(Combo_GetFrozenReturnEntrance("oot"), 0x530);

    // Restore
    std::vector<uint8_t> restored(OOT_SAVE_CONTEXT_SIZE, 0);
    int result = Combo_RestoreState("oot", restored.data(), restored.size());

    EXPECT_EQ(result, 1);
    EXPECT_EQ(restored[0], 0xAB);
    EXPECT_EQ(restored[1], 0xCD);
}

TEST_F(FrozenStateTest, CAPI_InvalidGameId) {
    std::vector<uint8_t> data(100, 0);

    // Invalid game ID should be no-op
    Combo_FreezeState("invalid", 0x100, data.data(), data.size());

    EXPECT_EQ(Combo_HasFrozenState("invalid"), 0);
    EXPECT_EQ(Combo_GetFrozenReturnEntrance("invalid"), 0);

    int result = Combo_RestoreState("invalid", data.data(), data.size());
    EXPECT_EQ(result, 0);
}

TEST_F(FrozenStateTest, CAPI_NullGameId) {
    std::vector<uint8_t> data(100, 0);

    // Null game ID should be safe
    Combo_FreezeState(nullptr, 0x100, data.data(), data.size());

    EXPECT_EQ(Combo_HasFrozenState(nullptr), 0);
    EXPECT_EQ(Combo_GetFrozenReturnEntrance(nullptr), 0);
}

TEST_F(FrozenStateTest, CAPI_GetSaveContextPointers) {
    const void* ootCtx = Combo_GetOoTSaveContext();
    const void* mmCtx = Combo_GetMMSaveContext();

    EXPECT_NE(ootCtx, nullptr);
    EXPECT_NE(mmCtx, nullptr);
}

TEST_F(FrozenStateTest, CAPI_ClearFrozenState) {
    std::vector<uint8_t> testData(MM_SAVE_CONTEXT_SIZE, 0xEE);

    Combo_FreezeState("mm", 0xC010, testData.data(), testData.size());
    EXPECT_EQ(Combo_HasFrozenState("mm"), 1);

    Combo_ClearFrozenState("mm");
    EXPECT_EQ(Combo_HasFrozenState("mm"), 0);
}

TEST_F(FrozenStateTest, CAPI_UpdateShadowCopy) {
    std::vector<uint8_t> testData(OOT_SAVE_CONTEXT_SIZE, 0x99);

    Combo_UpdateShadowCopy("oot", testData.data(), testData.size());

    // Check data was copied
    const uint8_t* storedData = static_cast<const uint8_t*>(Combo_GetOoTSaveContext());
    EXPECT_EQ(storedData[0], 0x99);

    // But not frozen
    EXPECT_EQ(Combo_HasFrozenState("oot"), 0);
}

// ============================================================================
// Round-trip test
// ============================================================================

TEST_F(FrozenStateTest, FullRoundTrip) {
    // Simulate a full cross-game switch cycle

    // 1. Create OoT SaveContext with some data
    std::vector<uint8_t> ootSave(OOT_SAVE_CONTEXT_SIZE, 0);
    ootSave[0x10] = 0x03;  // Some health value
    ootSave[0x20] = 0x14;  // Some item
    ootSave[0x100] = 0xFF; // Some flag

    // 2. Freeze OoT state before switching to MM
    Combo_FreezeState("oot", COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP,
                      ootSave.data(), ootSave.size());

    // 3. Verify OoT is frozen
    EXPECT_EQ(Combo_HasFrozenState("oot"), 1);
    EXPECT_EQ(Combo_GetFrozenReturnEntrance("oot"), COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP);

    // 4. Later, when returning to OoT, restore state
    std::vector<uint8_t> restoredOot(OOT_SAVE_CONTEXT_SIZE, 0);
    int result = Combo_RestoreState("oot", restoredOot.data(), restoredOot.size());

    EXPECT_EQ(result, 1);

    // 5. Verify data integrity
    EXPECT_EQ(restoredOot[0x10], 0x03);
    EXPECT_EQ(restoredOot[0x20], 0x14);
    EXPECT_EQ(restoredOot[0x100], 0xFF);
}
