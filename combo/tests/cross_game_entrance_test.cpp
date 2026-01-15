#include <gtest/gtest.h>
#include "combo/CrossGameEntrance.h"

using namespace Combo;

class CrossGameEntranceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing state
        gCrossGameEntrances.Clear();
        Combo_ClearPendingSwitch();
    }

    void TearDown() override {
        gCrossGameEntrances.Clear();
        Combo_ClearPendingSwitch();
    }
};

// ============================================================================
// CrossGameEntranceTable tests
// ============================================================================

TEST_F(CrossGameEntranceTest, EmptyTableReturnsNoLink) {
    EXPECT_FALSE(gCrossGameEntrances.IsCrossGameEntrance(Game::OoT, 0x530));
    EXPECT_FALSE(gCrossGameEntrances.GetLink(Game::OoT, 0x530).has_value());
}

TEST_F(CrossGameEntranceTest, RegisterAndLookupLink) {
    CrossGameEntranceLink link{
        .sourceGame = Game::OoT,
        .sourceEntrance = 0x530,
        .targetGame = Game::MM,
        .targetEntrance = 0xC010,
        .returnEntrance = 0x1D1
    };

    gCrossGameEntrances.RegisterLink(link);

    EXPECT_TRUE(gCrossGameEntrances.IsCrossGameEntrance(Game::OoT, 0x530));

    auto result = gCrossGameEntrances.GetLink(Game::OoT, 0x530);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->sourceGame, Game::OoT);
    EXPECT_EQ(result->sourceEntrance, 0x530);
    EXPECT_EQ(result->targetGame, Game::MM);
    EXPECT_EQ(result->targetEntrance, 0xC010);
    EXPECT_EQ(result->returnEntrance, 0x1D1);
}

TEST_F(CrossGameEntranceTest, WrongGameReturnsNoLink) {
    CrossGameEntranceLink link{
        .sourceGame = Game::OoT,
        .sourceEntrance = 0x530,
        .targetGame = Game::MM,
        .targetEntrance = 0xC010,
        .returnEntrance = 0x1D1
    };

    gCrossGameEntrances.RegisterLink(link);

    // Should not find it when querying for MM
    EXPECT_FALSE(gCrossGameEntrances.IsCrossGameEntrance(Game::MM, 0x530));
}

TEST_F(CrossGameEntranceTest, WrongEntranceReturnsNoLink) {
    CrossGameEntranceLink link{
        .sourceGame = Game::OoT,
        .sourceEntrance = 0x530,
        .targetGame = Game::MM,
        .targetEntrance = 0xC010,
        .returnEntrance = 0x1D1
    };

    gCrossGameEntrances.RegisterLink(link);

    // Should not find it with a different entrance
    EXPECT_FALSE(gCrossGameEntrances.IsCrossGameEntrance(Game::OoT, 0x999));
}

TEST_F(CrossGameEntranceTest, BidirectionalLinkRegistration) {
    gCrossGameEntrances.RegisterBidirectionalLink(
        Game::OoT, 0x530, 0x1D1,  // OoT: entrance, return
        Game::MM, 0xC010, 0xD800  // MM: entrance, return
    );

    // Should have 2 links
    EXPECT_EQ(gCrossGameEntrances.GetLinkCount(), 2);

    // Forward link: OoT entrance → MM
    auto forward = gCrossGameEntrances.GetLink(Game::OoT, 0x530);
    ASSERT_TRUE(forward.has_value());
    EXPECT_EQ(forward->targetGame, Game::MM);
    EXPECT_EQ(forward->targetEntrance, 0xC010);
    EXPECT_EQ(forward->returnEntrance, 0x1D1);

    // Reverse link: MM exit → OoT
    auto reverse = gCrossGameEntrances.GetLink(Game::MM, 0xD800);
    ASSERT_TRUE(reverse.has_value());
    EXPECT_EQ(reverse->targetGame, Game::OoT);
    EXPECT_EQ(reverse->targetEntrance, 0x1D1);
    EXPECT_EQ(reverse->returnEntrance, 0xC010);
}

TEST_F(CrossGameEntranceTest, DefaultLinksRegistration) {
    gCrossGameEntrances.RegisterDefaultLinks();

    // Should have 2 links (bidirectional)
    EXPECT_EQ(gCrossGameEntrances.GetLinkCount(), 2);

    // OoT Happy Mask Shop → MM Clock Tower
    auto ootToMm = gCrossGameEntrances.GetLink(Game::OoT, COMBO_OOT_ENTR_HAPPY_MASK_SHOP);
    ASSERT_TRUE(ootToMm.has_value());
    EXPECT_EQ(ootToMm->targetGame, Game::MM);
    EXPECT_EQ(ootToMm->targetEntrance, COMBO_MM_ENTR_CLOCK_TOWER_INTERIOR_1);

    // MM South Clock Town → OoT Market
    auto mmToOot = gCrossGameEntrances.GetLink(Game::MM, COMBO_MM_ENTR_SOUTH_CLOCK_TOWN_0);
    ASSERT_TRUE(mmToOot.has_value());
    EXPECT_EQ(mmToOot->targetGame, Game::OoT);
    EXPECT_EQ(mmToOot->targetEntrance, COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP);
}

TEST_F(CrossGameEntranceTest, ClearRemovesAllLinks) {
    gCrossGameEntrances.RegisterDefaultLinks();
    EXPECT_EQ(gCrossGameEntrances.GetLinkCount(), 2);

    gCrossGameEntrances.Clear();
    EXPECT_EQ(gCrossGameEntrances.GetLinkCount(), 0);
}

TEST_F(CrossGameEntranceTest, DuplicateLinkReplaces) {
    CrossGameEntranceLink link1{
        .sourceGame = Game::OoT,
        .sourceEntrance = 0x530,
        .targetGame = Game::MM,
        .targetEntrance = 0xC010,
        .returnEntrance = 0x1D1
    };

    CrossGameEntranceLink link2{
        .sourceGame = Game::OoT,
        .sourceEntrance = 0x530,  // Same source
        .targetGame = Game::MM,
        .targetEntrance = 0xDEAD,  // Different target
        .returnEntrance = 0xBEEF
    };

    gCrossGameEntrances.RegisterLink(link1);
    gCrossGameEntrances.RegisterLink(link2);

    // Should still have 1 link (replaced)
    EXPECT_EQ(gCrossGameEntrances.GetLinkCount(), 1);

    auto result = gCrossGameEntrances.GetLink(Game::OoT, 0x530);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->targetEntrance, 0xDEAD);  // New value
}

// ============================================================================
// C API tests
// ============================================================================

TEST_F(CrossGameEntranceTest, GameIdConversion) {
    EXPECT_EQ(Combo_GameFromId("oot"), Game::OoT);
    EXPECT_EQ(Combo_GameFromId("mm"), Game::MM);
    EXPECT_EQ(Combo_GameFromId("invalid"), Game::None);
    EXPECT_EQ(Combo_GameFromId(nullptr), Game::None);

    EXPECT_STREQ(Combo_GameToId(Game::OoT), "oot");
    EXPECT_STREQ(Combo_GameToId(Game::MM), "mm");
    EXPECT_EQ(Combo_GameToId(Game::None), nullptr);
}

TEST_F(CrossGameEntranceTest, CheckCrossGameEntrance_NoLink) {
    // No links registered
    uint16_t result = Combo_CheckCrossGameEntrance("oot", 0x530);

    // Should return original entrance
    EXPECT_EQ(result, 0x530);
    // No switch should be pending
    EXPECT_FALSE(Combo_IsCrossGameSwitch());
}

TEST_F(CrossGameEntranceTest, CheckCrossGameEntrance_WithLink) {
    gCrossGameEntrances.RegisterDefaultLinks();

    uint16_t result = Combo_CheckCrossGameEntrance("oot", COMBO_OOT_ENTR_HAPPY_MASK_SHOP);

    // Should return original entrance (game handles the switch)
    EXPECT_EQ(result, COMBO_OOT_ENTR_HAPPY_MASK_SHOP);

    // Switch should be pending
    EXPECT_TRUE(Combo_IsCrossGameSwitch());
    EXPECT_STREQ(Combo_GetSwitchTargetGameId(), "mm");
    EXPECT_EQ(Combo_GetSwitchTargetEntrance(), COMBO_MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    EXPECT_EQ(Combo_GetSwitchReturnEntrance(), COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP);
}

TEST_F(CrossGameEntranceTest, CheckCrossGameEntrance_InvalidGame) {
    gCrossGameEntrances.RegisterDefaultLinks();

    uint16_t result = Combo_CheckCrossGameEntrance("invalid", 0x530);

    // Should return original entrance
    EXPECT_EQ(result, 0x530);
    // No switch should be pending
    EXPECT_FALSE(Combo_IsCrossGameSwitch());
}

TEST_F(CrossGameEntranceTest, PendingSwitchLifecycle) {
    gCrossGameEntrances.RegisterDefaultLinks();

    // Initially no switch
    EXPECT_FALSE(Combo_IsCrossGameSwitch());

    // Trigger a switch
    Combo_CheckCrossGameEntrance("oot", COMBO_OOT_ENTR_HAPPY_MASK_SHOP);
    EXPECT_TRUE(Combo_IsCrossGameSwitch());

    // Signal ready
    Combo_SignalReadyToSwitch();
    EXPECT_TRUE(gPendingSwitch.readyToSwitch);

    // Clear the switch
    Combo_ClearPendingSwitch();
    EXPECT_FALSE(Combo_IsCrossGameSwitch());
    EXPECT_EQ(Combo_GetSwitchTargetGameId(), nullptr);
}
