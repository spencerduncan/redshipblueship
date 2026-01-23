# Unified Build System for RedShipBlueShip
#
# Produces a SINGLE executable containing both OoT and MM.
# Both games are compiled as OBJECT libraries and linked together.
#
# Usage:
#   cmake -B build -S pc
#   cmake --build build
#   ./build/redship --game oot
#   ./build/redship --game mm

cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

project(RedShipBlueShip VERSION 9.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Get source root (parent of pc/)
get_filename_component(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Include cmake modules from root project
list(APPEND CMAKE_MODULE_PATH "${SOURCE_ROOT}/CMake")
include(${SOURCE_ROOT}/CMake/Utils.cmake OPTIONAL)

# Suppress warnings for decomp code
option(SUPPRESS_WARNINGS "Suppress warnings in decomp code" ON)
if(SUPPRESS_WARNINGS)
    if(MSVC)
        set(WARNING_OVERRIDE /w)
    else()
        set(WARNING_OVERRIDE -w)
    endif()
endif()

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(${SOURCE_ROOT}/CMake/automate-vcpkg.cmake)
    set(VCPKG_TRIPLET x64-windows-static)
    vcpkg_bootstrap()
    vcpkg_install_packages(zlib bzip2 libzip libpng sdl2 sdl2-net glew glfw3
        nlohmann-json tinyxml2 spdlog libogg libvorbis opus opusfile)
endif()

# Find required packages
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)

# SDL2 - try multiple methods
find_package(SDL2 QUIET)
if(NOT TARGET SDL2::SDL2 AND NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()
endif()

# SDL2 is required but we defer the error to link time if not found
if(NOT TARGET SDL2::SDL2 AND NOT SDL2_FOUND)
    message(WARNING "SDL2 not found - build will fail at link time. Install libsdl2-dev.")
endif()

# GBI configuration
set(GBI_UCODE F3DEX_GBI_2)
add_compile_definitions(
    F3DEX_GBI_2
    CONTROLLERBUTTONS_T=uint32_t
)

# ==============================================================================
# Dependencies (libultraship, ZAPDTR, rsbs)
# ==============================================================================
if(NOT TARGET libultraship)
    add_subdirectory(${SOURCE_ROOT}/libultraship ${CMAKE_BINARY_DIR}/libultraship)
endif()

if(NOT TARGET ZAPDLib)
    add_subdirectory(${SOURCE_ROOT}/ZAPDTR/ZAPD ${CMAKE_BINARY_DIR}/ZAPD)
endif()

if(NOT TARGET rsbs)
    add_subdirectory(${SOURCE_ROOT}/rsbs ${CMAKE_BINARY_DIR}/rsbs)
endif()

# ==============================================================================
# OoT OBJECT Library
# ==============================================================================

# Collect OoT sources
file(GLOB_RECURSE OOT_SOH_SOURCES
    "${SOURCE_ROOT}/games/oot/soh/*.c"
    "${SOURCE_ROOT}/games/oot/soh/*.cpp"
)
file(GLOB_RECURSE OOT_SRC_SOURCES
    "${SOURCE_ROOT}/games/oot/src/*.c"
)
file(GLOB OOT_INCLUDE_HEADERS
    "${SOURCE_ROOT}/games/oot/include/*.h"
)

# Filter out excluded OoT sources
list(FILTER OOT_SRC_SOURCES EXCLUDE REGEX "src/dmadata/")
list(FILTER OOT_SRC_SOURCES EXCLUDE REGEX "src/elf_message/")
list(FILTER OOT_SRC_SOURCES EXCLUDE REGEX "src/libultra/io/")
list(FILTER OOT_SRC_SOURCES EXCLUDE REGEX "src/libultra/libc/")
list(FILTER OOT_SRC_SOURCES EXCLUDE REGEX "src/libultra/os/")
list(FILTER OOT_SRC_SOURCES EXCLUDE REGEX "src/libultra/rmon/")
list(APPEND OOT_SRC_SOURCES "${SOURCE_ROOT}/games/oot/src/libultra/libc/sprintf.c")

# Filter out GameExports.cpp (we use our adapter instead)
list(FILTER OOT_SOH_SOURCES EXCLUDE REGEX "GameExports\\.cpp")

# Platform-specific filtering for speech synthesizer
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(FILTER OOT_SOH_SOURCES EXCLUDE REGEX "speechsynthesizer/Darwin")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(FILTER OOT_SOH_SOURCES EXCLUDE REGEX "speechsynthesizer/SAPI")
else()
    list(FILTER OOT_SOH_SOURCES EXCLUDE REGEX "speechsynthesizer/(Darwin|SAPI)")
endif()

add_library(oot_decomp OBJECT
    ${OOT_SOH_SOURCES}
    ${OOT_SRC_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/oot/GameAdapter.cpp
)

target_include_directories(oot_decomp PRIVATE
    ${SOURCE_ROOT}/games/oot/include
    ${SOURCE_ROOT}/games/oot/src
    ${SOURCE_ROOT}/games/oot/soh
    ${SOURCE_ROOT}/games/oot/assets
    ${SOURCE_ROOT}/games/oot
    ${SOURCE_ROOT}/libultraship/include
    ${SOURCE_ROOT}/ZAPDTR/ZAPD/resource/type
)

target_compile_definitions(oot_decomp PRIVATE
    INCLUDE_GAME_PRINTF
    F3DEX_GBI_2
    ENABLE_OPENGL
)

target_link_libraries(oot_decomp PRIVATE
    libultraship
    rsbs
)

if(SUPPRESS_WARNINGS)
    target_compile_options(oot_decomp PRIVATE ${WARNING_OVERRIDE})
endif()

# ==============================================================================
# MM OBJECT Library
# ==============================================================================

# Collect MM sources
file(GLOB_RECURSE MM_2S2H_SOURCES
    "${SOURCE_ROOT}/games/mm/2s2h/*.c"
    "${SOURCE_ROOT}/games/mm/2s2h/*.cpp"
)
file(GLOB_RECURSE MM_SRC_SOURCES
    "${SOURCE_ROOT}/games/mm/src/*.c"
)
file(GLOB MM_INCLUDE_HEADERS
    "${SOURCE_ROOT}/games/mm/include/*.h"
)

# Filter out excluded MM sources
list(FILTER MM_SRC_SOURCES EXCLUDE REGEX "src/dmadata/")
list(FILTER MM_SRC_SOURCES EXCLUDE REGEX "src/elf_message/")
list(FILTER MM_SRC_SOURCES EXCLUDE REGEX "src/libultra/")

# Filter out GameExports.cpp (we use our adapter instead)
list(FILTER MM_2S2H_SOURCES EXCLUDE REGEX "GameExports\\.cpp")

add_library(mm_decomp OBJECT
    ${MM_2S2H_SOURCES}
    ${MM_SRC_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/mm/GameAdapter.cpp
)

target_include_directories(mm_decomp PRIVATE
    ${SOURCE_ROOT}/games/mm/include
    ${SOURCE_ROOT}/games/mm/include/PR
    ${SOURCE_ROOT}/games/mm/src
    ${SOURCE_ROOT}/games/mm/2s2h
    ${SOURCE_ROOT}/games/mm/assets
    ${SOURCE_ROOT}/games/mm
    ${SOURCE_ROOT}/libultraship/include
    ${SOURCE_ROOT}/ZAPDTR/ZAPD/resource/type
)

target_compile_definitions(mm_decomp PRIVATE
    INCLUDE_GAME_PRINTF
    F3DEX_GBI_2
    ENABLE_OPENGL
    NON_MATCHING
    NON_EQUIVALENT
)

target_link_libraries(mm_decomp PRIVATE
    libultraship
    rsbs
)

if(SUPPRESS_WARNINGS)
    target_compile_options(mm_decomp PRIVATE ${WARNING_OVERRIDE})
endif()

# ==============================================================================
# Unified Executable
# ==============================================================================

add_executable(redship
    ${CMAKE_CURRENT_SOURCE_DIR}/common/main.cpp
    $<TARGET_OBJECTS:oot_decomp>
    $<TARGET_OBJECTS:mm_decomp>
)

target_include_directories(redship PRIVATE
    ${SOURCE_ROOT}/libultraship/include
)

# Link with SDL2 (handles both modern cmake config and pkg-config fallback)
if(TARGET SDL2::SDL2)
    set(SDL2_LINK SDL2::SDL2)
else()
    set(SDL2_LINK ${SDL2_LIBRARIES})
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

target_link_libraries(redship PRIVATE
    libultraship
    ZAPDLib
    rsbs
    ${SDL2_LINK}
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# Platform-specific linking
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(glfw3 REQUIRED)
    find_package(Ogg CONFIG REQUIRED)
    find_package(Vorbis CONFIG REQUIRED)
    find_package(Opus CONFIG REQUIRED)
    find_package(OpusFile CONFIG REQUIRED)
    target_link_libraries(redship PRIVATE
        glfw
        Ogg::ogg
        Vorbis::vorbis
        Vorbis::vorbisfile
        Opus::opus
        OpusFile::opusfile
        glu32 winmm imm32 version setupapi
    )
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(Ogg REQUIRED)
    find_package(Vorbis REQUIRED)
    find_package(Opus REQUIRED)
    find_package(OpusFile REQUIRED)
    target_link_libraries(redship PRIVATE
        Ogg::ogg
        Vorbis::vorbis
        Vorbis::vorbisfile
        Opus::opus
        Opusfile::Opusfile
    )
    target_link_options(redship PRIVATE -rdynamic)
endif()

set_target_properties(redship PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Install target
install(TARGETS redship RUNTIME DESTINATION .)

message(STATUS "Unified build configured:")
message(STATUS "  OoT sources: ${CMAKE_CURRENT_SOURCE_DIR}/oot/GameAdapter.cpp + decomp")
message(STATUS "  MM sources:  ${CMAKE_CURRENT_SOURCE_DIR}/mm/GameAdapter.cpp + decomp")
message(STATUS "  Output:      ${CMAKE_BINARY_DIR}/redship")
