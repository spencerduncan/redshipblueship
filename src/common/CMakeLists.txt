# CMakeLists.txt for src/common
# This can be used standalone to test the common infrastructure

cmake_minimum_required(VERSION 3.14)

project(redship_common_test VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Sources
set(COMMON_SOURCES
    game.c
    context.cpp
    entrance.cpp
    game_lifecycle.c
    test_runner.cpp
    integration_test_hooks.cpp
    game_stubs.cpp
)

set(COMMON_HEADERS
    game.h
    context.h
    entrance.h
    game_lifecycle.h
    test_runner.h
    integration_test_hooks.h
)

# Build as library
add_library(redship_common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

target_include_directories(redship_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test executable (standalone)
if(BUILD_STANDALONE_TEST)
    add_executable(redship_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../rsbs/src/main.cpp
    )

    target_include_directories(redship_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(redship_test PRIVATE
        redship_common
    )

    # Enable testing
    enable_testing()
    add_test(NAME CommonTests COMMAND redship_test --test all)
endif()
