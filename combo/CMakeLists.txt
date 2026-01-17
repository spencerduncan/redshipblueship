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

    # On Unix, also add -rdynamic to ensure symbols are exported
    if(UNIX)
        target_link_options(redship PRIVATE -rdynamic)
    endif()

    # Install the launcher
    install(TARGETS redship RUNTIME DESTINATION bin)
endif()
