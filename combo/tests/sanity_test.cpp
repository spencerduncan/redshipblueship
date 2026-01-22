#include <gtest/gtest.h>
#include "combo/Game.h"

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

// Placeholder test for combo library
TEST(ComboTest, VersionExists) {
    const char* version = Combo::GetVersion();
    EXPECT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
}
