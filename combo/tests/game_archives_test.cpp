#include <gtest/gtest.h>
#include "combo/GameArchives.h"

using namespace Combo;

class GameArchivesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any registered archives before each test
        GameArchiveManager::Instance().Clear();
    }

    void TearDown() override {
        GameArchiveManager::Instance().Clear();
    }
};

TEST_F(GameArchivesTest, InstanceIsSingleton) {
    auto& instance1 = GameArchiveManager::Instance();
    auto& instance2 = GameArchiveManager::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(GameArchivesTest, InitiallyNoGamesLoaded) {
    EXPECT_FALSE(GameArchiveManager::Instance().IsGameLoaded(Game::OOT));
    EXPECT_FALSE(GameArchiveManager::Instance().IsGameLoaded(Game::MM));
}

TEST_F(GameArchivesTest, GetArchivesReturnsEmptyWhenNoneRegistered) {
    auto archives = GameArchiveManager::Instance().GetArchives(Game::OOT);
    EXPECT_TRUE(archives.empty());
}

TEST_F(GameArchivesTest, HasFileReturnsFalseWhenNoArchives) {
    EXPECT_FALSE(GameArchiveManager::Instance().HasFile(Game::OOT, "objects/object_link_boy/some_asset"));
    EXPECT_FALSE(GameArchiveManager::Instance().HasFile(Game::MM, "objects/object_link/some_asset"));
}

TEST_F(GameArchivesTest, LoadFileReturnsNullWhenNoArchives) {
    auto file = GameArchiveManager::Instance().LoadFile(Game::OOT, "objects/object_link_boy/some_asset");
    EXPECT_EQ(file, nullptr);
}

TEST_F(GameArchivesTest, GetFileOwnerThrowsWhenNotFound) {
    EXPECT_THROW(
        GameArchiveManager::Instance().GetFileOwner("nonexistent/path"),
        std::runtime_error
    );
}

TEST_F(GameArchivesTest, RegisterNullArchiveIsNoOp) {
    GameArchiveManager::Instance().RegisterArchive(Game::OOT, nullptr);
    EXPECT_FALSE(GameArchiveManager::Instance().IsGameLoaded(Game::OOT));
}

TEST_F(GameArchivesTest, ClearRemovesAllArchives) {
    // Even without registering real archives, Clear should work
    GameArchiveManager::Instance().Clear();
    EXPECT_FALSE(GameArchiveManager::Instance().IsGameLoaded(Game::OOT));
    EXPECT_FALSE(GameArchiveManager::Instance().IsGameLoaded(Game::MM));
}

// Convenience function tests
TEST_F(GameArchivesTest, LoadOoTFileReturnsNullWhenNoArchives) {
    auto file = LoadOoTFile("test/path");
    EXPECT_EQ(file, nullptr);
}

TEST_F(GameArchivesTest, LoadMMFileReturnsNullWhenNoArchives) {
    auto file = LoadMMFile("test/path");
    EXPECT_EQ(file, nullptr);
}
