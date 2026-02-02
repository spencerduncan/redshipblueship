cmake_minimum_required(VERSION 3.14)

# MINIMAL: This directory now only provides:
#   - Export.h: COMBO_API macro for DLL symbol visibility
#   - SharedGraphics.h/cpp: Cross-game graphics context sharing
#
# The following have been removed (replaced by GameOps in src/common/):
#   - DynamicLibrary.cpp/h: Dynamic library loading (single-exe doesn't need this)
#   - ComboContextBridge.cpp/h: Game lifecycle (replaced by GameRunner)
#   - FrozenState.h: State serialization (replaced by suspend/resume)
#   - CrossGameEntrance.h, Game.h: Compat headers (use src/common/ directly)
#   - GameArchives.cpp/h, IGame.h, GameAPI.h: Dead code from DLL architecture
#   - GameExports.h: Legacy DLL function pointer interface (replaced by GameOps)
#   - placeholder.cpp: Unused Combo::GetVersion() stub
#
# For new code, use src/common/ headers directly:
#   - game.h, context.h, entrance.h, game_lifecycle.h

# Common sources from src/common/ (implementations for compatibility headers)
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/game.c
    ${CMAKE_SOURCE_DIR}/src/common/context.cpp
    ${CMAKE_SOURCE_DIR}/src/common/entrance.cpp
)

if(WIN32)
    add_library(combo SHARED
        src/SharedGraphics.cpp
        ${COMMON_SOURCES}
    )
    # Export symbols from this DLL
    target_compile_definitions(combo PRIVATE COMBO_BUILDING_DLL)
else()
    add_library(combo STATIC
        src/SharedGraphics.cpp
        ${COMMON_SOURCES}
    )
endif()

target_include_directories(combo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/common  # For compatibility headers to find new game.h, entrance.h, context.h
)

# Link against libultraship for Archive/File types
target_link_libraries(combo PUBLIC
    libultraship
)

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

# Unit tests - only for static builds since tests use internal C++ classes
# When combo is a DLL (Windows), internal classes aren't exported
if(BUILD_TESTING AND NOT WIN32)
    add_subdirectory(tests)
endif()

# NOTE: The launcher executable and CTest integration tests have been moved to
# the single executable architecture. See:
#   - rsbs/src/main.cpp (entry point)
#   - CMake/SingleExecutable.cmake (CTest configuration)
#   - src/common/ (shared infrastructure)
