#include <gtest/gtest.h>
#include "game.h"  // src/common/game.h

// Sanity tests to verify GoogleTest framework is working

TEST(SanityTest, TrueIsTrue) {
    EXPECT_TRUE(true);
}

TEST(SanityTest, OnePlusOneIsTwo) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(SanityTest, StringComparison) {
    std::string hello = "Hello";
    EXPECT_EQ(hello, "Hello");
    EXPECT_NE(hello, "World");
}

// Test the game.h API from src/common/
TEST(GameTest, GameFromString) {
    EXPECT_EQ(Game_FromString("oot"), GAME_OOT);
    EXPECT_EQ(Game_FromString("mm"), GAME_MM);
    EXPECT_EQ(Game_FromString("invalid"), GAME_NONE);
}

TEST(GameTest, GameToString) {
    EXPECT_STREQ(Game_ToString(GAME_OOT), "oot");
    EXPECT_STREQ(Game_ToString(GAME_MM), "mm");
    EXPECT_EQ(Game_ToString(GAME_NONE), nullptr);
}

TEST(GameTest, GameGetOther) {
    EXPECT_EQ(Game_GetOther(GAME_OOT), GAME_MM);
    EXPECT_EQ(Game_GetOther(GAME_MM), GAME_OOT);
    EXPECT_EQ(Game_GetOther(GAME_NONE), GAME_NONE);
}
