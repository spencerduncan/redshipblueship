cmake_minimum_required(VERSION 3.14)

# Build mode options:
# - REDSHIP_BUILD_SHARED: Build games as DLLs, combo as shared library
# - REDSHIP_UNIFIED_BUILD: Build single executable with both games statically linked
#
# After symbol namespacing (issue #29), unified build is now possible.

# Common source files for the combo library
set(COMBO_SOURCES
    src/placeholder.cpp
    src/GameArchives.cpp
    src/DynamicLibrary.cpp
    src/ComboContextBridge.cpp
    src/ComboContext.cpp
    src/CrossGameEntrance.cpp
    src/FrozenState.cpp
    src/SharedGraphics.cpp
    src/test/TestRunner.cpp
)

# On Windows with shared game builds, combo must be a DLL so both the launcher
# and game DLLs can link against it. On other platforms, static is fine since
# -rdynamic exports symbols from the executable.
if(WIN32 AND REDSHIP_BUILD_SHARED)
    add_library(combo SHARED ${COMBO_SOURCES})
    # Export symbols from this DLL
    target_compile_definitions(combo PRIVATE COMBO_BUILDING_DLL)
else()
    add_library(combo STATIC ${COMBO_SOURCES})
endif()

target_include_directories(combo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against libultraship for Archive/File types
target_link_libraries(combo PUBLIC
    libultraship
)

# Link against dl for dynamic loading on Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(combo PUBLIC ${CMAKE_DL_LIBS})
endif()

set_target_properties(combo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Match MSVC runtime library with games (static runtime)
if(MSVC)
    set_target_properties(combo PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# Set output directory to match game DLLs on Windows
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(combo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>"
    )
endif()

# Tests - only for static builds since tests use internal C++ classes
# When combo is a DLL, internal classes aren't exported
if(BUILD_TESTING AND NOT (WIN32 AND REDSHIP_BUILD_SHARED))
    add_subdirectory(tests)
endif()

# Launcher executable (only when building shared libraries)
if(REDSHIP_BUILD_SHARED)
    add_executable(redship
        src/main.cpp
    )

    target_link_libraries(redship PRIVATE
        combo
    )

    # On Unix, define COMBO_BUILDING_EXE so symbols are exported from the exe
    if(NOT WIN32)
        target_compile_definitions(combo PRIVATE COMBO_BUILDING_EXE)
    endif()

    set_target_properties(redship PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        # Export symbols so loaded games can call Combo_* functions (Unix)
        ENABLE_EXPORTS TRUE
    )

    # Match MSVC runtime library with games (static runtime)
    if(MSVC)
        set_target_properties(redship PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()

    # Set output directory to match game DLLs on Windows
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_target_properties(redship PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>"
        )
    endif()

    # On Unix, also add -rdynamic to ensure symbols are exported
    if(UNIX)
        target_link_options(redship PRIVATE -rdynamic)
    endif()

    # Install the launcher
    install(TARGETS redship RUNTIME DESTINATION bin)
endif()

# =============================================================================
# CTest Integration Tests
# =============================================================================
# These tests use the --test flag to run automated game tests.
# They require game assets (OTR files) to be present, so they're marked
# as WILL_FAIL until the full infrastructure is in place.

if(BUILD_TESTING AND REDSHIP_BUILD_SHARED)
    enable_testing()

    # Configurable timeout for CI environments with varying performance
    set(REDSHIP_TEST_TIMEOUT 60 CACHE STRING "Test timeout in seconds")

    # Boot tests - verify each game can initialize
    add_test(NAME BootOoT COMMAND redship --test boot-oot)
    add_test(NAME BootMM COMMAND redship --test boot-mm)

    # Switch tests - verify cross-game transitions
    add_test(NAME SwitchOoTMM COMMAND redship --test switch-oot-mm)
    add_test(NAME SwitchMMOoT COMMAND redship --test switch-mm-oot)

    # Full round-trip test
    add_test(NAME RoundTrip COMMAND redship --test roundtrip)

    # TODO(#29): Remove WILL_FAIL when unified build provides game integration
    # Exit criteria for removing WILL_FAIL:
    #   1. BootOoT: OoT boots to main menu within timeout in headless mode
    #   2. BootMM: MM boots to main menu within timeout in headless mode
    #   3. SwitchOoTMM: Cross-game transition completes without crash
    #   4. SwitchMMOoT: Reverse transition completes without crash
    #   5. RoundTrip: Full OoT->MM->OoT cycle with state verification
    set_tests_properties(
        BootOoT BootMM SwitchOoTMM SwitchMMOoT RoundTrip
        PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT ${REDSHIP_TEST_TIMEOUT}
    )
endif()

# =============================================================================
# Unified Build (issue #30)
# =============================================================================
# Single executable containing both OoT and MM, enabled by REDSHIP_UNIFIED_BUILD.
# This is possible after symbol namespacing (issue #29) resolved all collisions.
#
# The unified build:
# - Compiles both games' decomp sources as object libraries
# - Links them into a single executable with the combo infrastructure
# - Provides seamless switching without DLL loading overhead

if(REDSHIP_UNIFIED_BUILD)
    message(STATUS "Unified build enabled - building single executable with both games")

    # Add unified build compile definition to combo library
    target_compile_definitions(combo PRIVATE COMBO_UNIFIED_BUILD)

    # Unified executable entry point
    add_executable(redship_unified
        src/unified_main.cpp
    )

    target_link_libraries(redship_unified PRIVATE
        combo
    )

    target_compile_definitions(redship_unified PRIVATE COMBO_UNIFIED_BUILD)

    set_target_properties(redship_unified PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        OUTPUT_NAME "redship"
    )

    # TODO(#30): Link against game object libraries once they're available
    # target_link_libraries(redship_unified PRIVATE
    #     $<TARGET_OBJECTS:oot_decomp>
    #     $<TARGET_OBJECTS:mm_decomp>
    # )

    # CTest for unified build
    if(BUILD_TESTING)
        enable_testing()

        set(REDSHIP_TEST_TIMEOUT 60 CACHE STRING "Test timeout in seconds")

        add_test(NAME UnifiedBootOoT COMMAND redship --game oot --test boot-oot)
        add_test(NAME UnifiedBootMM COMMAND redship --game mm --test boot-mm)
        add_test(NAME UnifiedSwitch COMMAND redship --test switch-oot-mm)
        add_test(NAME UnifiedRoundTrip COMMAND redship --test roundtrip)

        # Mark as WILL_FAIL until full game integration is complete
        set_tests_properties(
            UnifiedBootOoT UnifiedBootMM UnifiedSwitch UnifiedRoundTrip
            PROPERTIES
            WILL_FAIL TRUE
            TIMEOUT ${REDSHIP_TEST_TIMEOUT}
        )
    endif()

    install(TARGETS redship_unified RUNTIME DESTINATION bin)
endif()
