# CMake/SingleExecutable.cmake
# Single executable architecture for RedShip
#
# This module configures the build to produce a single `redship` executable
# that contains both OoT and MM compiled as object libraries with namespaced
# symbols (OoT_* and MM_*).

if(NOT SINGLE_EXECUTABLE_BUILD)
    return()
endif()

message(STATUS "=== Single Executable Architecture Enabled ===")

# ============================================================================
# Common sources for the single executable
# ============================================================================

set(REDSHIP_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/game.c
    ${CMAKE_SOURCE_DIR}/src/common/context.cpp
    ${CMAKE_SOURCE_DIR}/src/common/entrance.cpp
    ${CMAKE_SOURCE_DIR}/src/common/test_runner.cpp
    # Stub implementations for game entry points (until full integration)
    ${CMAKE_SOURCE_DIR}/src/common/game_stubs.cpp
    # SharedGraphics for cross-game graphics context sharing
    ${CMAKE_SOURCE_DIR}/combo/src/SharedGraphics.cpp
    # Unified menu bar for single executable
    ${CMAKE_SOURCE_DIR}/src/common/ComboMenuBar.cpp
)

# Windows-specific: import thunks for libultraship compatibility
if(WIN32)
    list(APPEND REDSHIP_COMMON_SOURCES
        ${CMAKE_SOURCE_DIR}/src/common/shared_graphics_win.cpp
    )
endif()

set(REDSHIP_COMMON_HEADERS
    ${CMAKE_SOURCE_DIR}/src/common/game.h
    ${CMAKE_SOURCE_DIR}/src/common/context.h
    ${CMAKE_SOURCE_DIR}/src/common/entrance.h
    ${CMAKE_SOURCE_DIR}/src/common/test_runner.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboMenuBar.h
)

# ============================================================================
# Common library (shared between OoT and MM)
# ============================================================================

add_library(redship_common STATIC
    ${REDSHIP_COMMON_SOURCES}
    ${REDSHIP_COMMON_HEADERS}
)

target_include_directories(redship_common PUBLIC
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/rsbs/include
    ${CMAKE_SOURCE_DIR}/combo/include
)

target_link_libraries(redship_common PUBLIC
    libultraship
)

# Define COMBO_BUILDING_DLL so SharedGraphics exports symbols with __declspec(dllexport)
target_compile_definitions(redship_common PRIVATE COMBO_BUILDING_DLL)

set_target_properties(redship_common PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# MSVC runtime library - must match the games (static runtime)
if(MSVC)
    set_target_properties(redship_common PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# ============================================================================
# Single executable (redship)
# ============================================================================

# Note: The actual game object libraries (OoT_objects, MM_objects) are
# created by their respective CMakeLists.txt files when SINGLE_EXECUTABLE_BUILD
# is enabled. They use symbol prefixing to avoid conflicts.

add_executable(redship
    ${CMAKE_SOURCE_DIR}/rsbs/src/main.cpp
)

target_include_directories(redship PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/rsbs/include
    ${CMAKE_SOURCE_DIR}/combo/include
)

target_link_libraries(redship PRIVATE
    redship_common
    rsbs
)

# Game object libraries will be linked when they are available
# This is deferred because the games are added as subdirectories later

set_target_properties(redship PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Platform-specific settings
if(MSVC)
    set_target_properties(redship PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_options(redship PRIVATE -rdynamic)
endif()

# ============================================================================
# CTest Integration
# ============================================================================

if(BUILD_TESTING)
    # Add test targets
    add_test(NAME BootOoT COMMAND redship --test boot-oot)
    add_test(NAME BootMM COMMAND redship --test boot-mm)
    add_test(NAME SwitchOoTMM COMMAND redship --test switch-oot-mm)
    add_test(NAME SwitchMMOoT COMMAND redship --test switch-mm-oot)
    add_test(NAME Roundtrip COMMAND redship --test roundtrip)
    add_test(NAME Context COMMAND redship --test context)
    add_test(NAME AllTests COMMAND redship --test all)

    # Set reasonable timeout
    set(REDSHIP_TEST_TIMEOUT 60 CACHE STRING "Test timeout in seconds")
    set_tests_properties(
        BootOoT BootMM SwitchOoTMM SwitchMMOoT Roundtrip Context AllTests
        PROPERTIES
        TIMEOUT ${REDSHIP_TEST_TIMEOUT}
    )
endif()

# ============================================================================
# AppImage packaging (Linux)
# ============================================================================

if(UNIX AND NOT APPLE)
    set_property(TARGET redship PROPERTY APPIMAGE_DESKTOP_FILE_TERMINAL YES)
    set_property(TARGET redship PROPERTY APPIMAGE_DESKTOP_FILE "${CMAKE_SOURCE_DIR}/scripts/linux/appimage/soh.desktop")
    set_property(TARGET redship PROPERTY APPIMAGE_ICON_FILE "${CMAKE_BINARY_DIR}/sohIcon.png")
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS redship RUNTIME DESTINATION . COMPONENT ship)

message(STATUS "Single executable 'redship' will be built")
message(STATUS "Use --test option to run integration tests")
