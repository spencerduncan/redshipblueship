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
    ${CMAKE_SOURCE_DIR}/src/common/archive_check.cpp
    # Unattended-safe crash handling: replaces libultraship's modal crash
    # dialog with stderr + immediate non-zero exit on headless runs (#388)
    ${CMAKE_SOURCE_DIR}/src/common/headless_crash.cpp
    # Bundled ZAPD subprocess driver shared by both games' in-app extraction (#325)
    ${CMAKE_SOURCE_DIR}/src/common/zapd_subprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/common/context.cpp
    ${CMAKE_SOURCE_DIR}/src/common/switch.cpp
    ${CMAKE_SOURCE_DIR}/src/common/entrance.cpp
    ${CMAKE_SOURCE_DIR}/src/common/test_runner.cpp
    ${CMAKE_SOURCE_DIR}/src/common/integration_test_hooks.cpp
    # Note: game_stubs.cpp is NOT included - real implementations come from
    # games/oot/soh/GameExports_SingleExe.cpp and games/mm/2s2h/GameExports_SingleExe.cpp
    # Unified SaveContext storage for both games
    ${CMAKE_SOURCE_DIR}/src/common/unified_save.c
    # Unified cross-game save file (.redsave) — Phase 2 T6 (#35)
    ${CMAKE_SOURCE_DIR}/src/common/save.cpp
    # SharedGraphics for cross-game graphics context sharing
    ${CMAKE_SOURCE_DIR}/src/common/SharedGraphics.cpp
    # Unified menu bar for single executable
    ${CMAKE_SOURCE_DIR}/src/common/ComboMenuBar.cpp
    # MM stubs and aliases for single-exe mode
    ${CMAKE_SOURCE_DIR}/src/common/mm_stubs.c
    ${CMAKE_SOURCE_DIR}/src/common/mm_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/common/game_lifecycle.c
)

# Windows-specific: import thunks for libultraship compatibility
if(WIN32)
    list(APPEND REDSHIP_COMMON_SOURCES
        ${CMAKE_SOURCE_DIR}/src/common/shared_graphics_win.cpp
    )
endif()

set(REDSHIP_COMMON_HEADERS
    ${CMAKE_SOURCE_DIR}/src/common/game.h
    ${CMAKE_SOURCE_DIR}/src/common/archive_check.h
    ${CMAKE_SOURCE_DIR}/src/common/headless_crash.h
    ${CMAKE_SOURCE_DIR}/src/common/zapd_subprocess.h
    ${CMAKE_SOURCE_DIR}/src/common/context.h
    ${CMAKE_SOURCE_DIR}/src/common/entrance.h
    ${CMAKE_SOURCE_DIR}/src/common/test_runner.h
    ${CMAKE_SOURCE_DIR}/src/common/integration_test_hooks.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboMenuBar.h
    ${CMAKE_SOURCE_DIR}/src/common/game_lifecycle.h
    ${CMAKE_SOURCE_DIR}/src/common/SharedGraphics.h
    ${CMAKE_SOURCE_DIR}/src/common/save.h
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
    # GameInteractor headers for integration test hooks
    ${CMAKE_SOURCE_DIR}/games/oot/soh/Enhancements
    ${CMAKE_SOURCE_DIR}/games/oot/soh
    ${CMAKE_SOURCE_DIR}/games/mm/2s2h
)

# PRIVATE: test_runner.cpp's mm-scene-parse test (#344) includes MM's S2H
# resource headers by their in-tree "2s2h/..." spelling.
target_include_directories(redship_common PRIVATE
    ${CMAKE_SOURCE_DIR}/games/mm
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
)

# Find additional libraries needed by game code
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
find_package(opusfile CONFIG QUIET)
if(NOT opusfile_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(opusfile REQUIRED IMPORTED_TARGET opusfile)
endif()

# Library dependencies are linked AFTER game OBJECT libraries in the root
# CMakeLists.txt to ensure correct link order on Linux (ld requires libraries
# after the objects that reference them).
# Store them in a variable for the root CMakeLists.txt to use.
#
# NOTE: Both ZAPDLib and OTRExporter variants are needed for ROM extraction:
# - OTRExporter_OoT extracts OoT ROM assets
# - OTRExporter_MM extracts MM ROM assets
# On Windows, duplicate symbols are handled with /FORCE:MULTIPLE (safe because
# the variants only differ by compile-time GAME_OOT/GAME_MM defines).
set(REDSHIP_LIBRARY_DEPS
    redship_common
    rsbs
    libultraship
    ZAPDLib_OoT
    ZAPDLib_MM
    OTRExporter_OoT
    OTRExporter_MM
    Ogg::ogg
    Vorbis::vorbis
    Vorbis::vorbisfile
    $<TARGET_NAME_IF_EXISTS:OpusFile::opusfile>
    $<$<NOT:$<TARGET_EXISTS:OpusFile::opusfile>>:PkgConfig::opusfile>
)

# SDL2_net is needed by OoT's Network.cpp when BUILD_REMOTE_CONTROL is enabled.
# Find it here; linking happens in root CMakeLists.txt after OBJECT files.
if(BUILD_REMOTE_CONTROL)
    find_package(SDL2_net)
    if(SDL2_net_FOUND)
        if(TARGET SDL2_net::SDL2_net-static)
            list(APPEND REDSHIP_LIBRARY_DEPS SDL2_net::SDL2_net-static)
        elseif(TARGET SDL2_net::SDL2_net)
            list(APPEND REDSHIP_LIBRARY_DEPS SDL2_net::SDL2_net)
        endif()
    endif()
endif()

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
    # 8 MB stack reserve (upstream SoH's value), all configs. The randomizer
    # table-builder functions (InitLocationTable, RegisterPotLocations, ...)
    # are too large for MSVC to optimize (C4883), so every Location temporary
    # gets its own stack slot and each function carries a several-hundred-KB
    # frame. With the MSVC default 1 MB reserve the boot-time rando init
    # overflows the stack (0xc00000fd). The game targets' own /STACK flags
    # never reach this link: they sit on STATIC libraries, whose link options
    # do not propagate to the final executable. Reserve is virtual address
    # space, not committed memory, and applies to every thread created with
    # a default stack size.
    target_link_options(redship PRIVATE /STACK:8777216)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(redship PRIVATE -rdynamic)
endif()

# ============================================================================
# CTest Integration
# ============================================================================

include(${CMAKE_CURRENT_LIST_DIR}/RedshipTests.cmake)

if(BUILD_TESTING)
    # Cache variables so a slow runner can raise these without editing the file.
    # They must be set before the first redship_add_test(): a row that passes no
    # TIMEOUT inherits REDSHIP_TEST_TIMEOUT.
    set(REDSHIP_TEST_TIMEOUT 60 CACHE STRING "Test timeout in seconds")
    set(REDSHIP_INTEGRATION_TEST_TIMEOUT 120 CACHE STRING "Integration test timeout in seconds")

    # A test source that is never #included compiles into nothing and can never
    # run. The glob notices the file; this asserts it is actually wired in.
    redship_check_test_sources()

    # ========================================================================
    # Unit tests (no display required)
    #
    # redship_add_test() performs add_test + set_tests_properties together and
    # defaults to LABELS "redship" / TIMEOUT ${REDSHIP_TEST_TIMEOUT}, so adding
    # a test is ONE appended line — there is no shared name list to edit (#376).
    # ========================================================================
    redship_add_test(NAME BootOoT COMMAND redship --test boot-oot)
    redship_add_test(NAME BootMM COMMAND redship --test boot-mm)
    redship_add_test(NAME SwitchOoTMM COMMAND redship --test switch-oot-mm)
    redship_add_test(NAME SwitchMMOoT COMMAND redship --test switch-mm-oot)
    # Startup-entrance flow incl. game-affinity regression: an MM-tagged
    # entrance (0xC010) must be invisible to OoT, whose entranceIndex is a
    # linear gEntranceTable index — the OOB-read crash behind the Market
    # cutscene 0xC0000005.
    redship_add_test(NAME StartupEntrance COMMAND redship --test startup-entrance)
    # Entrance-link dedup regression (#374): the default (mask shop) and test
    # (Mido's House) links both return through MM 0xC010, and both were
    # registered unconditionally. Entrance_CheckCrossGame resolves first-match,
    # so the default silently shadowed the test link — enter MM from Mido's
    # House, walk back into the Clock Tower, exit in Hyrule Market instead of
    # Kokiri Forest. Locks that a duplicate (sourceGame, sourceEntrance) is
    # rejected atomically and that each portal face routes to its own return.
    redship_add_test(NAME EntranceDedup COMMAND redship --test entrance-dedup)
    # VB-affinity regression: MM's GameInteractor_* calls bind to OoT's
    # extern "C" wrappers in single-exe builds, and the two games' vanilla-
    # behavior ordinals alias (MM VB_SETUP_TRANSITION == OoT
    # VB_PLAY_RAINBOW_BRIDGE_CS == 206). The wrappers must return vanilla
    # behavior while MM is active — the Market -> Clock Tower NULL-call crash.
    redship_add_test(NAME VBAffinity COMMAND redship --test vb-affinity)
    # MM HUD gfx-wrapper contract: the CosmeticEditor Override wrappers must
    # write commands and return the advanced display-list pointer. The old
    # void stubs in mm_stubs.c fed MM_Interface_DrawItemButtons a garbage
    # write pointer (WRITE AV at 0xA7, first HUD-visible MM frame — caught by
    # int-gameplay-roundtrip on the OoT->MM leg).
    redship_add_test(NAME CosmeticGfxStub COMMAND redship --test cosmetic-gfx-stub)
    redship_add_test(NAME Roundtrip COMMAND redship --test roundtrip)
    redship_add_test(NAME RoundtripIntegrity COMMAND redship --test roundtrip-integrity)
    redship_add_test(NAME SharedRoundtrip COMMAND redship --test shared-roundtrip)
    redship_add_test(NAME ArchiveHotswapLogic COMMAND redship --test archive-hotswap-logic)
    # Unified save (.redsave) headless tests — Phase 2 T6 (#35)
    redship_add_test(NAME SaveRoundtripTiers COMMAND redship --test save-roundtrip-tiers)
    redship_add_test(NAME SaveHeader COMMAND redship --test save-header)
    redship_add_test(NAME SaveHasDelete COMMAND redship --test save-has-delete)
    redship_add_test(NAME SaveVersionReject COMMAND redship --test save-version-reject)
    redship_add_test(NAME SaveSizeMismatch COMMAND redship --test save-size-mismatch)
    redship_add_test(NAME SaveLegacySize COMMAND redship --test save-legacy-size)
    redship_add_test(NAME SaveCrcCorrupt COMMAND redship --test save-crc-corrupt)
    # Tier-1 (ComboContext) format headroom — Phase 3 Wave 1. The loader used to
    # demand comboSize == sizeof(ComboContext) exactly, so the moment Lane A
    # widens sharedItems to carry an origin-game tag, every existing .redsave
    # would stop loading with no message to the user. These lock the migration:
    # a pre-headroom Tier-1 still loads and zero-extends, the record size is
    # fixed and padded so growth does not move it, and an oversized record is
    # refused rather than truncated.
    redship_add_test(NAME SaveComboLegacyRecord COMMAND redship --test save-combo-legacy-record)
    redship_add_test(NAME SaveComboRecordFixed COMMAND redship --test save-combo-record-fixed)
    redship_add_test(NAME SaveComboOversize COMMAND redship --test save-combo-oversize)
    redship_add_test(NAME Context COMMAND redship --test context)
    # F10 hot-swap freeze/consume contract (#364): the hotkey path must freeze
    # the DEPARTING game (or refuse the switch), and a consumed frozen state
    # must be retired so it can never be re-applied. Before the fix, one
    # entrance switch left a blob that every later F10 return silently rolled
    # the player back to.
    redship_add_test(NAME HotSwapFreeze COMMAND redship --test hotswap-freeze)
    # MM scene-command parse + execute regressions — display-free, no ROM
    # archives (#344). Parse checks the wire format; execute runs the commands
    # against a PlayState and asserts the spawn-path pointers/fields populate.
    redship_add_test(NAME MMSceneParse COMMAND redship --test mm-scene-parse)
    redship_add_test(NAME MMSceneExecute COMMAND redship --test mm-scene-execute)
    # Sequence-map capacity bounds (#371, #378). The rest of AudioLoad_Init
    # needs a booted audio heap and real archives, but both bugs were
    # bound-arithmetic bugs, so the capacity computation was factored into pure
    # helpers this test calls directly — display-free, ROM-free.
    redship_add_test(NAME SeqMapBounds COMMAND redship --test seq-map-bounds)
    # Active-thread-queue contract (#385). soh/stubs.c's empty-bodied
    # __osGetActiveQueue returned the return register, and both games' fault
    # handlers walk that as a thread list — so the crash handler was itself
    # liable to crash. Locks non-NULL, bounded termination at the priority == -1
    # sentinel, and call-to-call stability.
    redship_add_test(NAME ActiveQueue COMMAND redship --test active-queue)
    # MM cross-game resume contracts (games/mm/2s2h/mm_resume_state_test.cpp):
    # a resume must re-arm the (by-design leaked) system arena for the cold
    # gamestate-chain boot, and Play_Init's startup-entrance consumption must
    # restore the frozen save the boot chain wiped — the cycle-2 re-entry
    # crash + save-continuity faults caught by the int-gameplay-roundtrip
    # soak (docs/ci-gameplay-repro-postmortem.md).
    # MM must bind its OWN Ship_ExtendedCulling* bodies (#382) — OoT's index
    # Actor::projectedPos 8 bytes earlier than MM's Actor puts it, and only one
    # definition survived the link, so there was no ODR error to catch it.
    redship_add_test(NAME MMCullingBinding COMMAND redship --test mm-culling-binding)
    redship_add_test(NAME MMResumeArena COMMAND redship --test mm-resume-arena)
    redship_add_test(NAME MMStartupRestore COMMAND redship --test mm-startup-restore)
    redship_add_test(NAME AllTests COMMAND redship --test all)

    # Registration-completeness guard (#376). Diffs the dispatch table the
    # binary actually links (`redship --test list`) against the rows registered
    # above, and FAILS on a disagreement in either direction: a gTests[] entry
    # with no row, or a row naming an entry that does not exist.
    #
    # It carries the "redship" label on purpose, so it runs in the tier it
    # polices — CI's `ctest -L "^redship$"` picks it up with no workflow change.
    # That makes this tier 31 rows: the 30 pre-existing tests, unchanged, plus
    # this guard.
    redship_add_test(NAME TestRegistrationComplete
        COMMAND ${CMAKE_COMMAND}
                -DREDSHIP_EXE=$<TARGET_FILE:redship>
                -DMANIFEST=${CMAKE_BINARY_DIR}/redship-test-manifest.txt
                -P ${CMAKE_CURRENT_LIST_DIR}/CheckTestRegistration.cmake)

    # Dispatch-table entries that deliberately have no dedicated CTest row.
    # Both predate this refactor and both still execute inside AllTests, which
    # runs the whole table; they simply never got a row of their own. Listing
    # them here is what lets the guard treat every OTHER row-less entry as an
    # error. The guard hard-fails if one of these leaves gTests[] or later gains
    # a row, so the list cannot rot into a rubber stamp.
    redship_test_exempt(lifecycle "Game lifecycle unit tests — runs inside AllTests only")
    redship_test_exempt(midos-house "Test-entrance (--test-entrance) path — runs inside AllTests only")

    # ========================================================================
    # Rando seed-generation test (requires a display for the Fast3dWindow
    # bring-up but NO game archives — CI runs it under xvfb-run; issue #337).
    # Not in the "redship" label because that tier runs display-free.
    # ========================================================================
    redship_add_test(NAME RandoGen COMMAND redship --test rando-gen
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # Song shuffle modes. Stock (RandoGen above) already covers mode 1 — "Song
    # Locations" is the default — which is the mode that regressed when the
    # linker dropped ShuffleSongs.cpp.o from the static soh_rando archive and
    # left the fill with 12 songs and 0 song locations.
    redship_add_test(NAME RandoGenSongsDungeonRewards COMMAND redship --test rando-gen
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1;RSBS_DIAG_CVARS=gRandoSettings.ShuffleSongs=2")
    redship_add_test(NAME RandoGenSongsAnywhere COMMAND redship --test rando-gen
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1;RSBS_DIAG_CVARS=gRandoSettings.ShuffleSongs=3")

    # ========================================================================
    # Integration tests (requires display - use Xvfb in CI)
    # These tests actually boot the games and verify boot completion
    # ========================================================================
    redship_add_test(NAME IntBootOoT COMMAND redship --integration-test int-boot-oot
        LABEL integration TIMEOUT ${REDSHIP_INTEGRATION_TEST_TIMEOUT})
    redship_add_test(NAME IntBootMM COMMAND redship --integration-test int-boot-mm
        LABEL integration TIMEOUT ${REDSHIP_INTEGRATION_TEST_TIMEOUT})
    redship_add_test(NAME IntSwitchOoTHmsToMm
        COMMAND redship --integration-test int-switch-oot-hms-to-mm
        LABEL integration TIMEOUT ${REDSHIP_INTEGRATION_TEST_TIMEOUT})
    redship_add_test(NAME IntSwitchMmClockTownSouthToOoT
        COMMAND redship --integration-test int-switch-mm-clocktown-south-to-oot
        LABEL integration TIMEOUT ${REDSHIP_INTEGRATION_TEST_TIMEOUT})
    redship_add_test(NAME IntArchiveHotswapCycle
        COMMAND redship --integration-test int-archive-hotswap-cycle
        LABEL integration TIMEOUT ${REDSHIP_INTEGRATION_TEST_TIMEOUT})

    # Gameplay round-trip crash repro (docs/ci-gameplay-repro-postmortem.md):
    # the programmatic version of the operator's manual repro — debug save,
    # live gameplay, production cross-game round trip (SaveContext
    # freeze/restore + the OoT resume leg where the 2026-07 crash class
    # detonated), post-return debug warp, and a door transition. Requires
    # oot.o2r/mm.o2r (ROM-derived) + a GL-capable display (Xvfb+llvmpipe in
    # CI). Scene/frame parameters come from RSBS_GP_* env vars, so a soak
    # matrix can sweep scenes without new CTest rows. The soak variant runs
    # three round trips before the warp.
    #
    # These two carry their own timeouts rather than the shared integration
    # value: the repro is far longer than a boot check, and the soak runs it
    # three times over.
    redship_add_test(NAME IntGameplayRoundtrip
        COMMAND redship --integration-test int-gameplay-roundtrip
        LABEL integration
        TIMEOUT 300)
    redship_add_test(NAME IntGameplayRoundtripSoak
        COMMAND redship --integration-test int-gameplay-roundtrip
        LABEL integration-soak
        TIMEOUT 900
        ENVIRONMENT "RSBS_GP_CYCLES=3")

    # Must come after every redship_add_test()/redship_test_exempt() above —
    # writes the manifest TestRegistrationComplete reads.
    redship_finalize_tests(${CMAKE_BINARY_DIR}/redship-test-manifest.txt)
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
