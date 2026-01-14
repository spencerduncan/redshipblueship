include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent overriding parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(combo_tests
    sanity_test.cpp
)

target_link_libraries(combo_tests
    combo
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(combo_tests)
