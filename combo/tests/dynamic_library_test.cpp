#include <gtest/gtest.h>
#include "combo/DynamicLibrary.h"
#include "combo/GameExports.h"

using namespace Combo;

TEST(DynamicLibraryTest, LoadNonexistentReturnsNull) {
    LibraryHandle handle = DynamicLibrary::Load("nonexistent_library_12345.so");
    EXPECT_FALSE(DynamicLibrary::IsValid(handle));
}

TEST(DynamicLibraryTest, GetErrorReturnsMessageAfterFailure) {
    // Try to load nonexistent library to generate an error
    DynamicLibrary::Load("nonexistent_library_12345.so");
    std::string error = DynamicLibrary::GetError();
    // Error message should be non-empty after a failure
    EXPECT_FALSE(error.empty());
}

TEST(DynamicLibraryTest, UnloadNullIsNoOp) {
    // Should not crash
    DynamicLibrary::Unload(LIBRARY_HANDLE_NULL);
}

TEST(DynamicLibraryTest, GetSymbolFromNullReturnsNull) {
    void* sym = DynamicLibrary::GetSymbol(LIBRARY_HANDLE_NULL, "any_symbol");
    EXPECT_EQ(sym, nullptr);
}

TEST(DynamicLibraryTest, IsValidReturnsFalseForNull) {
    EXPECT_FALSE(DynamicLibrary::IsValid(LIBRARY_HANDLE_NULL));
}

// ScopedLibrary tests
TEST(ScopedLibraryTest, DefaultConstructorCreatesInvalid) {
    ScopedLibrary lib;
    EXPECT_FALSE(lib.IsValid());
    EXPECT_FALSE(static_cast<bool>(lib));
    EXPECT_EQ(lib.Get(), LIBRARY_HANDLE_NULL);
}

TEST(ScopedLibraryTest, ConstructWithBadPathCreatesInvalid) {
    ScopedLibrary lib("nonexistent_library_12345.so");
    EXPECT_FALSE(lib.IsValid());
}

TEST(ScopedLibraryTest, MoveConstructorTransfersOwnership) {
    ScopedLibrary lib1;
    ScopedLibrary lib2(std::move(lib1));
    // Both should be invalid (since lib1 was default constructed)
    EXPECT_FALSE(lib1.IsValid());
    EXPECT_FALSE(lib2.IsValid());
}

TEST(ScopedLibraryTest, MoveAssignmentTransfersOwnership) {
    ScopedLibrary lib1;
    ScopedLibrary lib2;
    lib2 = std::move(lib1);
    EXPECT_FALSE(lib1.IsValid());
    EXPECT_FALSE(lib2.IsValid());
}

TEST(ScopedLibraryTest, GetSymbolOnInvalidReturnsNull) {
    ScopedLibrary lib;
    EXPECT_EQ(lib.GetSymbol("any_symbol"), nullptr);
}

// GameExports struct tests
TEST(GameExportsTest, DefaultConstructorHasNullPointers) {
    GameExports exports;
    EXPECT_EQ(exports.Init, nullptr);
    EXPECT_EQ(exports.Run, nullptr);
    EXPECT_EQ(exports.Shutdown, nullptr);
    EXPECT_EQ(exports.GetName, nullptr);
    EXPECT_EQ(exports.GetId, nullptr);
}

TEST(GameExportsTest, HasRequiredExportsReturnsFalseWhenEmpty) {
    GameExports exports;
    EXPECT_FALSE(exports.HasRequiredExports());
}

TEST(GameExportsTest, HasHotSwitchExportsReturnsFalseWhenEmpty) {
    GameExports exports;
    EXPECT_FALSE(exports.HasHotSwitchExports());
}

TEST(GameExportsTest, HasStateExportsReturnsFalseWhenEmpty) {
    GameExports exports;
    EXPECT_FALSE(exports.HasStateExports());
}

// Test with mock function pointers
namespace {
    int MockInit(int, char**) { return 0; }
    void MockRun() {}
    void MockShutdown() {}
    void MockPause() {}
    void MockResume() {}
    void* MockSaveState(size_t*) { return nullptr; }
    int MockLoadState(void*, size_t) { return 0; }
    const char* MockGetName() { return "Test"; }
    const char* MockGetId() { return "test"; }
}

TEST(GameExportsTest, HasRequiredExportsReturnsTrueWhenSet) {
    GameExports exports;
    exports.Init = MockInit;
    exports.Run = MockRun;
    exports.Shutdown = MockShutdown;
    exports.GetName = MockGetName;
    exports.GetId = MockGetId;
    EXPECT_TRUE(exports.HasRequiredExports());
}

TEST(GameExportsTest, HasHotSwitchExportsReturnsTrueWhenSet) {
    GameExports exports;
    exports.Pause = MockPause;
    exports.Resume = MockResume;
    EXPECT_TRUE(exports.HasHotSwitchExports());
}

TEST(GameExportsTest, HasStateExportsReturnsTrueWhenSet) {
    GameExports exports;
    exports.SaveState = MockSaveState;
    exports.LoadState = MockLoadState;
    EXPECT_TRUE(exports.HasStateExports());
}
