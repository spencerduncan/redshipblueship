cmake_minimum_required(VERSION 3.14)

# DEPRECATED: This directory contains the old dynamic library architecture.
# New code should use src/common/ and the single executable architecture.
# This CMakeLists.txt is kept for backwards compatibility during transition.
#
# The following files have been ported to src/common/:
#   combo/src/FrozenState.cpp     -> src/common/context.cpp
#   combo/src/CrossGameEntrance.cpp -> src/common/entrance.cpp
#   combo/include/combo/Game.h    -> src/common/game.h
#   combo/src/test/TestRunner.cpp -> src/common/test_runner.cpp
#   combo/src/main.cpp            -> rsbs/src/main.cpp

# On Windows, combo must be a DLL so both the launcher and game DLLs can link
# against it. On other platforms, static is fine since -rdynamic exports symbols
# from the executable.
if(WIN32)
    add_library(combo SHARED
        src/placeholder.cpp
        src/GameArchives.cpp
        src/DynamicLibrary.cpp
        src/ComboContextBridge.cpp
        src/SharedGraphics.cpp
    )
    # Export symbols from this DLL
    target_compile_definitions(combo PRIVATE COMBO_BUILDING_DLL)
else()
    add_library(combo STATIC
        src/placeholder.cpp
        src/GameArchives.cpp
        src/DynamicLibrary.cpp
        src/ComboContextBridge.cpp
        src/SharedGraphics.cpp
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
