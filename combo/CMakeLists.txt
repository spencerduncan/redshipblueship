cmake_minimum_required(VERSION 3.14)

# On Windows with shared game builds, combo must be a DLL so both the launcher
# and game DLLs can link against it. On other platforms, static is fine since
# -rdynamic exports symbols from the executable.
if(WIN32 AND REDSHIP_BUILD_SHARED)
    add_library(combo SHARED
        src/placeholder.cpp
        src/GameArchives.cpp
        src/DynamicLibrary.cpp
        src/ComboContextBridge.cpp
        src/CrossGameEntrance.cpp
        src/FrozenState.cpp
        src/SharedGraphics.cpp
        src/test/TestRunner.cpp
    )
    # Export symbols from this DLL
    target_compile_definitions(combo PRIVATE COMBO_BUILDING_DLL)
else()
    add_library(combo STATIC
        src/placeholder.cpp
        src/GameArchives.cpp
        src/DynamicLibrary.cpp
        src/ComboContextBridge.cpp
        src/CrossGameEntrance.cpp
        src/FrozenState.cpp
        src/SharedGraphics.cpp
        src/test/TestRunner.cpp
    )
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

    # Boot tests - verify each game can initialize
    add_test(NAME BootOoT COMMAND redship --test boot-oot)
    add_test(NAME BootMM COMMAND redship --test boot-mm)

    # Switch tests - verify cross-game transitions
    add_test(NAME SwitchOoTMM COMMAND redship --test switch-oot-mm)
    add_test(NAME SwitchMMOoT COMMAND redship --test switch-mm-oot)

    # Full round-trip test
    add_test(NAME RoundTrip COMMAND redship --test roundtrip)

    # Mark all tests as expected to fail until infrastructure is complete
    set_tests_properties(
        BootOoT BootMM SwitchOoTMM SwitchMMOoT RoundTrip
        PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT 60
    )
endif()
