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
    # Cross-game shared-item producers/consumers over gComboCtx.sharedItemsTagged
    # (ADR 0002, Lane A1) — read/written by both games' suspend + consumption hooks
    ${CMAKE_SOURCE_DIR}/src/common/shared_items.c
    # Foreign-item placement table over gComboCtx.foreignPlacements (Lane C1,
    # #392) — written by MM's paired-world generation, read by MM's give path
    # and both spoiler surfaces; the pinned pool itself is OoT-side
    # (soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp)
    ${CMAKE_SOURCE_DIR}/src/common/foreign_items.c
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

# ============================================================================
# Netplay grant relay (ADR 0007, #460) — OFF by default, and OFF means ABSENT
#
# These sources are appended ONLY when RSBS_NETPLAY is ON. The default build
# therefore gains no translation unit and links no new symbol, which is the
# strongest verifiable form of "byte-unaffected" (byte-identical binaries are
# not reproducible across toolchain nondeterminism; "no new linked symbols" is
# checkable, and the netplay-default-off CI job checks it).
#
# No submodule and no transport library: the wire format is length-prefixed
# binary over plain TCP, so a small socket shim beats putting a dependency in
# every CI checkout for code that is inert by default (ADR 0006 §7).
# ============================================================================
if(RSBS_NETPLAY)
    message(STATUS "Netplay grant relay: ENABLED (experimental, ADR 0007)")
    list(APPEND REDSHIP_COMMON_SOURCES
        ${CMAKE_SOURCE_DIR}/src/common/netplay/relay_protocol.c
        ${CMAKE_SOURCE_DIR}/src/common/netplay/relay_client.c
    )
endif()

set(REDSHIP_COMMON_HEADERS
    ${CMAKE_SOURCE_DIR}/src/common/game.h
    ${CMAKE_SOURCE_DIR}/src/common/archive_check.h
    ${CMAKE_SOURCE_DIR}/src/common/headless_crash.h
    ${CMAKE_SOURCE_DIR}/src/common/zapd_subprocess.h
    ${CMAKE_SOURCE_DIR}/src/common/context.h
    ${CMAKE_SOURCE_DIR}/src/common/shared_items.h
    ${CMAKE_SOURCE_DIR}/src/common/foreign_items.h
    ${CMAKE_SOURCE_DIR}/src/common/entrance.h
    ${CMAKE_SOURCE_DIR}/src/common/test_runner.h
    ${CMAKE_SOURCE_DIR}/src/common/integration_test_hooks.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboMenuBar.h
    ${CMAKE_SOURCE_DIR}/src/common/game_lifecycle.h
    ${CMAKE_SOURCE_DIR}/src/common/SharedGraphics.h
    ${CMAKE_SOURCE_DIR}/src/common/save.h
    # The cross-game CVar classification manifest (ADR 0003 + the
    # enhancement-classification inventory), consumed by OoT's version-7
    # config updater, the 2Ship importer, and the classification lock.
    ${CMAKE_SOURCE_DIR}/src/common/cvar_shared_keys.h
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

# The CVar classification lock (--test cvar-classification, #34) scans games/
# for retired and must-stay-distinct key literals, so it needs to find the
# source tree at runtime. The test degrades to a loud WARNING (not a silent
# pass) when the path is absent, which is what happens if the binary is run
# from a relocated artifact rather than its build tree.
target_compile_definitions(redship_common PRIVATE RSBS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Netplay relay (ADR 0007). PUBLIC so test_runner.cpp's guarded #include of
# tests/test_netplay_relay.c compiles in the same configuration the relay
# sources do — a PRIVATE define here would silently drop the tests while the
# relay itself built, which is the worst of both.
if(RSBS_NETPLAY)
    target_compile_definitions(redship_common PUBLIC RSBS_NETPLAY)
endif()

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
    # The gameplay round-trip repro and its multi-cycle soak run far longer than
    # a boot check, so they keep their own knobs rather than the shared
    # integration value — but they are STILL cache variables, not literals baked
    # into the rows (#376 item 5): raising REDSHIP_INTEGRATION_TEST_TIMEOUT to
    # debug a slow runner used to leave the two longest tests untouched.
    set(REDSHIP_GAMEPLAY_TEST_TIMEOUT 300 CACHE STRING "Gameplay round-trip integration test timeout in seconds")
    set(REDSHIP_GAMEPLAY_SOAK_TIMEOUT 900 CACHE STRING "Gameplay round-trip soak (multi-cycle) test timeout in seconds")

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
    # Arrival-rehydration lock (#482): two more instances of the #441 class. A
    # switch INTO OoT cold-boots the title chain, whose Save_InitFile(true)
    # dispatches SaveManager initFuncs that blank the check tracker's areasSpoiled
    # and the item tracker's typed notes -- both OUTSIDE gSaveContext. The frozen
    # save is restored at Play_ConsumeStartupEntrance without re-running
    # Save_LoadFile, so nothing rehydrates them; the next save then persists the
    # blanked values. Guarded with Combo_HasStartupEntranceForGame("oot"). Drives
    # the real init functions with and without the arrival flag (survive vs. still
    # clear). Pure -- no display -- so it runs in this redship tier.
    redship_add_test(NAME TrackerArrivalRehydration COMMAND redship --test tracker-arrival-rehydration)
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
    # Cross-game CVar classification lock (#34). Keeps ADR 0003 and
    # docs/enhancement-classification.md from decaying into prose: fails when a
    # converged key diverges again, OR when a key the inventory marks per-game
    # gets merged because the names looked equivalent. The second direction is
    # the dangerous one — merging OoT's 1-5x text-speed slider with MM's
    # boolean yields a control that persists a value and does nothing.
    redship_add_test(NAME CVarClassification COMMAND redship --test cvar-classification)
    redship_add_test(NAME Roundtrip COMMAND redship --test roundtrip)
    redship_add_test(NAME RoundtripIntegrity COMMAND redship --test roundtrip-integrity)
    redship_add_test(NAME SharedRoundtrip COMMAND redship --test shared-roundtrip)
    # Origin-tagged cross-game items (ADR 0002 / Lane A1): a recorded item must
    # survive the full suspend->switch->resume->switch->resume round trip through
    # the real producer/consumer + freeze/consume hooks, be visible in both
    # directions, and be awarded exactly once per crossing (covers the F10 path).
    redship_add_test(NAME SharedItemRoundtrip COMMAND redship --test shared-item-roundtrip)
    # Lane C1 (#392): foreign-item give path lands correctly tagged in the
    # shared structure, the crossing awards exactly once on the OoT side, and
    # gComboCtx.foreignPlacements round-trips through the .redsave record.
    redship_add_test(NAME ForeignItemGive COMMAND redship --test foreign-item-give)
    # Cross-game session invalidation (#440). A soft reset or a new game must
    # retire the previous session's frozen blobs, shadows and gComboCtx
    # crossings — while a cross-game arrival and a legitimate existing-slot
    # load must still restore. Also the single-player half of the netplay
    # blocker (#460): a stale sharedItemsTagged carries another player's grants
    # from a dead room into a fresh seed.
    redship_add_test(NAME SessionInvalidation COMMAND redship --test session-invalidation)
    # Netplay 1a (ADR 0005, #460): sourced-grant model locks — cursor
    # idempotency, switch-free received-order redemption, loud overflow with
    # redeemed-slot reclamation, and .redsave durability + reset atomicity.
    redship_add_test(NAME GrantIdempotency COMMAND redship --test grant-idempotency)
    redship_add_test(NAME GrantRedeemNoSwitch COMMAND redship --test grant-redeem-no-switch)
    redship_add_test(NAME GrantOverflow COMMAND redship --test grant-overflow)
    redship_add_test(NAME GrantPersistence COMMAND redship --test grant-persistence)

    # Netplay grant relay (ADR 0007, #460). Registered only when the relay is
    # built, because with RSBS_NETPLAY=OFF the code under test does not exist —
    # a row here would fail rather than skip. The netplay-default-off CI job
    # builds with the flag ON and runs these; every other job stays default.
    #
    # What these lock, and the honest limit, is stated at the top of
    # src/common/tests/test_netplay_relay.c: they drive the real codec and the
    # real state machine over a mock ledger with multi-server's semantics, so
    # they prove our SEMANTICS but not our admissibility to the real wire.
    # Unlike the Archipelago case (ADR 0006 §2b) there is no external
    # gatekeeper, so a framing mismatch is a defect we can fix, not a wall.
    if(RSBS_NETPLAY)
        redship_add_test(NAME RelayWireFormat COMMAND redship --test relay-wire-format)
        redship_add_test(NAME RelayLoopback COMMAND redship --test relay-loopback)
        redship_add_test(NAME RelayCatchup COMMAND redship --test relay-catchup)
        redship_add_test(NAME RelayBackpressure COMMAND redship --test relay-backpressure)
        redship_add_test(NAME RelaySuspendLatch COMMAND redship --test relay-suspend-latch)
    endif()
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
    # Origin-tagged shared items (ADR 0002): the array carved out of reserved[]
    # must round-trip byte-exact, and SaveComboLegacyRecord's crafted "legacy"
    # length is pinned to the pre-carve prefix (RSBS_COMBO_CONTEXT_PRECARVE_SIZE)
    # so the carve cannot silently widen what that test calls legacy.
    redship_add_test(NAME SaveTaggedItems COMMAND redship --test save-tagged-items)
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
    # OoT audio producer init-guard (#365). The shared OTRAudio_Thread runs
    # OoT_AudioMgr_CreateNextAudioBuffer whenever MM's synth is inactive — every
    # MM frame before MM audio boots, and the mid-switch window where OoT is
    # suspended with gAudioContextInitalized == false. Without the guard that
    # drove the DMA/load/synth path against a torn-down context; the guard makes
    # it the no-op the thread's silence contract already assumes. Locks the
    # no-op via the producer's task counter (see test_oot_audio_init_guard.c).
    redship_add_test(NAME OoTAudioInitGuard COMMAND redship --test oot-audio-init-guard)
    # Gameplay round-trip phase watchdog (#376 item 4). The round-trip repro it
    # guards is ROM-gated, but the watchdog decision is pure — this row proves,
    # ROM-free, that the budget is wall-clock and stays under the CTest TIMEOUT
    # so its diagnostic dump can fire before the hard wall-clock kill (the old
    # frame budget of 4080 frames could not).
    redship_add_test(NAME GpWatchdog COMMAND redship --test gp-watchdog)
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
    # MM GameInteractor shim (#395): MM-side hook registration must go through
    # the MM-owned extern "C" shim; registering through MM's larger view of
    # the shared class writes ~60-92 bytes past OoT's 4-byte allocation.
    redship_add_test(NAME MMGIShim COMMAND redship --test mm-gi-shim)
    # MM Notification::Emit cross-bind (#427 item 1): MM's BenGui/Notification.cpp
    # is excluded, so MM's Rando pickup toast binds OoT's Notification::Emit —
    # safe only while both ports' Notification::Options stay layout-identical.
    # This compares their layout fingerprints and fails on drift (one Emit
    # definition survives, so no link error can catch a divergence).
    redship_add_test(NAME MMNotificationBinding COMMAND redship --test mm-notification-binding)
    # MM scaled framebuffer draw (#386): MM's framebuffer_effects.c is excluded,
    # so MM's FB_DrawFromFramebufferScaled bound OoT's surviving body, which
    # reads OoT_gScreenWidth/Height. MM's dimensions diverge (HiRes 576 with the
    # Bombers' Notebook open vs OoT's 320), so the shared body mis-scales MM's
    # VisFbuf draw. This locks MM's call to its own MM_-dimension body (only one
    # definition survived, so no link error could catch the cross-bind).
    redship_add_test(NAME MMFbEffectsBinding COMMAND redship --test mm-fb-effects-binding)
    # MM tracker registration surface (#392): the four MM tracker windows must
    # register on the shared Gui under "MM "-prefixed names (SoH owns the
    # unprefixed ones; Gui::AddGuiWindow rejects duplicates) and their
    # Draw/Update path must be inert unless MM is the active game — unified
    # gSaveContext storage means an ungated MM tracker reads OoT bytes through
    # MM's SaveContext layout.
    redship_add_test(NAME MMTrackersGui COMMAND redship --test mm-trackers-gui)
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
    # That makes this tier every redship_add_test row above, unchanged, plus
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

    # Hint validity (#441): a gossip stone read "catching Big Poes leads to No
    # Item" while that seed's spoiler named a real item, so the fill was fine
    # and only hint-side resolution was broken. Locks that no generated hint
    # resolves to the no-item sentinel. Same display-but-no-archives tier.
    redship_add_test(NAME RandoHintValidity COMMAND redship --test rando-hint-validity
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # Hint reload (#441), the RUNTIME complement to RandoHintValidity. That lock
    # re-renders hints against the same live, fully-filled context, so it cannot
    # see the operator-visible break: an item hint resolves its item at runtime
    # from the placement table, and a fresh game AND a reload both rebuild that
    # table from the save (FileChoose_LoadGame -> Save_LoadFile -> LoadRandomizer).
    # This drives the real save -> Rando::Context reset -> reload cycle and locks
    # that item hints still name their real item afterward. Same tier.
    redship_add_test(NAME RandoHintReload COMMAND redship --test rando-hint-reload
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # Cross-game arrival lock (#441) — THE root-cause lock. A switch into OoT
    # cold-boots the title chain, whose Title_Destroy -> OoT_Sram_InitSram ->
    # Save_Init wiped the Rando::Context placement table while the frozen save was
    # restored without re-running LoadRandomizer, so every item hint stranded on
    # "No Item" though its location phrasing survived (ClearItemLocations never
    # touches hintTable). Drives the real Save_Init with and without the arrival
    # flag. Same display-but-no-archives tier.
    redship_add_test(NAME RandoHintCrossGame COMMAND redship --test rando-hint-crossgame
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

    # Lane C0 (#392): MM's 2ship_rando is un-elided and actually generates —
    # region graph populated via ShipInit registrars, OnFileCreate runs
    # GeneratePools + the glitchless logic apply headlessly, spoiler JSON
    # written with the 2S2H_RANDO_SPOILER tag. Same harness shape as the OoT
    # rando-gen rows (display via xvfb, no game archives).
    redship_add_test(NAME MMRandoGen COMMAND redship --test mm-rando-gen
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # #439: the paired MM world must activate on the flow a player actually
    # takes. MMRandoGen above drives the OnSaveInit chain DIRECTLY, which is
    # why CI stayed green through an entire playtest in which MM never paired:
    # entering MM through the Happy Mask Shop performs a cold gamestate-chain
    # boot (ConsoleLogo -> TitleSetup -> Play_Init) that authors a vanilla
    # bootstrap save and never dispatches OnSaveInit at all. This row drives
    # that boot + the real MM_Play_ConsumeStartupEntrance consumption point,
    # and additionally locks that an existing MM save (vanilla or already
    # paired) is never regenerated. MMRandoGen stays as direct-chain
    # regression cover. Same display requirement, hence the same label/env.
    redship_add_test(NAME MMPairSwitchEntry COMMAND redship --test mm-pair-switch-entry
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # #439 follow-up: MMPairSwitchEntry locks the cross-game arrival convergence;
    # this row locks the OTHER paths into MM gameplay the arrival fix does not
    # touch. The IS_RANDO COND_HOOKs (Rando.h: saveType == SAVETYPE_RANDO, re-
    # evaluated on every OnSaveLoad) must match the save at every convergence:
    # the file-select LOAD must RE-ARM after the boot chain's disarming
    # OnSaveLoad (the disarm-then-rearm ordering #439 got wrong on the arrival
    # path, and the ordering the owl-save reload relies on), and an in-session
    # reload (Song of Time / cycle reset / DayTelop) must leave a live rando
    # session's armed hooks untouched. Reads arm state through
    # S2H::GameHooks::CountForTest<OnFlagSet>. Same display requirement, hence
    # the same label/env.
    redship_add_test(NAME MMReloadArmState COMMAND redship --test mm-reload-arm-state
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # The operator-confirmed P0. A moon crash (three-day clock expiring, e.g.
    # while AFK) runs Sram_ResetSaveFromMoonCrash, which re-reads the file from
    # flash and memcpy's sizeof(Save) over gSaveContext.save. ShipSaveInfo --
    # saveType AND the whole rando block -- is a MEMBER of Save, and a cross-game
    # paired world lives only in memory until the player saves, so the reload
    # stripped the randomizer identity: every IS_RANDO COND_HOOK unregistered and
    # MM played vanilla, permanently, because the next switch-out froze that save.
    # Probes arm state through a VB verdict, not a hook count (COND_HOOK's
    # Unregister is deferred, so a count lags a disarm and would pass vacuously).
    # Same display requirement, hence the same label/env.
    redship_add_test(NAME MMMoonCrashArmState COMMAND redship --test mm-moon-crash-arm-state
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # ========================================================================
    # Lane B — unified seed -> paired world (Phase 3.0)
    #
    # The pinned settings profile is the second half of the "one seed -> paired
    # world" contract: Playthrough_Init re-seeds the RNG with Hash(seed +
    # settings-string), so a seed reproduces a fill only under identical settings.
    # RSBS_DIAG_CVARS is that pinning vehicle; both rows below pin the same
    # profile (stock rando defaults + ShuffleSongs=2, the "RSBS unified pinned
    # profile v1").
    # ========================================================================
    #
    # Single-run lock: generation succeeds AND the LIVE producer stamps
    # gComboCtx.sourceIsRando/sharedRandoSeed at generation time (the dispatch
    # fails if it does not). Also the --test row the completeness guard requires
    # for the rando-determinism dispatch entry.
    redship_add_test(NAME RandoDeterminism COMMAND redship --test rando-determinism
        LABEL rando
        TIMEOUT 180
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1;RSBS_DIAG_CVARS=gRandoSettings.ShuffleSongs=2")
    # Determinism diff: run the same seed twice in TWO processes and assert the
    # worlds are byte-identical (see CMake/CheckSeedDeterminism.cmake — two
    # processes because generator re-entry is unverified; distinct digest paths
    # because the same-seed spoiler log overwrites itself). A meta row (its
    # COMMAND drives no --test entry). Larger timeout than the single-run rows: it
    # spins up the windowed bring-up twice under llvmpipe.
    redship_add_test(NAME SeedDeterminism
        COMMAND ${CMAKE_COMMAND}
                -DREDSHIP_EXE=$<TARGET_FILE:redship>
                -DWORK_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_CURRENT_LIST_DIR}/CheckSeedDeterminism.cmake
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1;RSBS_DIAG_CVARS=gRandoSettings.ShuffleSongs=2")

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
    # These two carry their own timeout knobs rather than the shared integration
    # value: the repro is far longer than a boot check, and the soak runs it
    # three times over. Both are cache variables (see above), so a slow runner
    # can be debugged by raising REDSHIP_GAMEPLAY_TEST_TIMEOUT /
    # REDSHIP_GAMEPLAY_SOAK_TIMEOUT instead of editing these rows (#376 item 5).
    redship_add_test(NAME IntGameplayRoundtrip
        COMMAND redship --integration-test int-gameplay-roundtrip
        LABEL integration
        TIMEOUT ${REDSHIP_GAMEPLAY_TEST_TIMEOUT})
    redship_add_test(NAME IntGameplayRoundtripSoak
        COMMAND redship --integration-test int-gameplay-roundtrip
        LABEL integration-soak
        TIMEOUT ${REDSHIP_GAMEPLAY_SOAK_TIMEOUT}
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
