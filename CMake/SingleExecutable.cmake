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
    # Shared cross-game RESOURCES over gComboCtx.sharedResources (#525) — one
    # quantity spanning both games (rupees, wallet tier, hearts, current health,
    # double defense), harvested at each game's suspend and applied at its
    # startup entrance. Distinct from shared_items.c: that carries one-way
    # single-use crossings, this carries a continuously shared value.
    ${CMAKE_SOURCE_DIR}/src/common/shared_resources.c
    # Foreign-item placement table over gComboCtx.foreignPlacements (Lane C1,
    # #392) — written by MM's paired-world generation, read by MM's give path
    # and both spoiler surfaces; the pinned pool itself is OoT-side
    # (soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp)
    ${CMAKE_SOURCE_DIR}/src/common/foreign_items.c
    # Read-only view model over those placements (#496) — the in-game answer to
    # "which MM check hosts which OoT item, and has it been collected", so the
    # spoiler stops being a JSON path the operator has to be told
    ${CMAKE_SOURCE_DIR}/src/common/combo_spoiler_view.c
    # The window that renders it — the first common-owned Gui element (ADR 0008)
    ${CMAKE_SOURCE_DIR}/src/common/ComboSpoilerWindow.cpp
    # Combo tracker (#458): both games' check progress through per-game
    # adapters — MM's frozen shadow via a registered offset descriptor, OoT's
    # suspended heap via a registered accessor vtable — staleness-labelled,
    # plus the window that renders it
    ${CMAKE_SOURCE_DIR}/src/common/combo_tracker_view.c
    ${CMAKE_SOURCE_DIR}/src/common/ComboTrackerWindow.cpp
    # MM randomizer options: the registry + value accessors over the descriptor
    # table MM publishes (#497 step 4, #499), and the pane that draws it. The
    # pane is common-owned because it must be reachable while OoT is running —
    # the paired MM profile snapshots at MM's arrival and is never regenerated
    ${CMAKE_SOURCE_DIR}/src/common/combo_mm_options_view.c
    ${CMAKE_SOURCE_DIR}/src/common/ComboMmOptionsWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/common/entrance.cpp
    # Per-game registry of the user mod archives each port mounted (#593), so
    # the base-archive re-add on every cross-game switch can put them back on
    # top instead of silently revoking every mod override
    ${CMAKE_SOURCE_DIR}/src/common/mod_archives.cpp
    ${CMAKE_SOURCE_DIR}/src/common/test_runner.cpp
    ${CMAKE_SOURCE_DIR}/src/common/integration_test_hooks.cpp
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
    # Header for shared_resources.c above (#525)
    ${CMAKE_SOURCE_DIR}/src/common/shared_resources.h
    ${CMAKE_SOURCE_DIR}/src/common/foreign_items.h
    ${CMAKE_SOURCE_DIR}/src/common/combo_spoiler_view.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboSpoilerWindow.h
    ${CMAKE_SOURCE_DIR}/src/common/combo_tracker_view.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboTrackerWindow.h
    ${CMAKE_SOURCE_DIR}/src/common/combo_mm_options_view.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboMmOptionsWindow.h
    ${CMAKE_SOURCE_DIR}/src/common/entrance.h
    # Header for mod_archives.cpp above (#593)
    ${CMAKE_SOURCE_DIR}/src/common/mod_archives.h
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
    # DLL export/import macros (Phase 2 T10, #265); the only live consumer is
    # SharedGraphics.h above.
    ${CMAKE_SOURCE_DIR}/src/common/Export.h
    # RSBS's own release identity (#319), independent of the upstream Ship
    # VERSION baked into the archive/save validation.
    ${CMAKE_SOURCE_DIR}/src/common/rsbs_version.h
    # The plain-C cross-game notification interface (#427 item 1) and its
    # runtime layout-equality lock — see their header comments for why both
    # are still needed even though MM no longer hands OoT its own Options.
    ${CMAKE_SOURCE_DIR}/src/common/notification_bridge.h
    ${CMAKE_SOURCE_DIR}/src/common/notification_layout_probe.h
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
    redship_add_test(NAME ForeignItemGiveReverse COMMAND redship --test foreign-item-give-reverse)
    # #488: a foreign item may only be hosted by a check class the GAME arms.
    # CheckQueue's foreign branch is nested inside `if (eligible)`, so an
    # unarmed host strands a pinned OoT progression item and makes the paired
    # world unwinnable with no error. Drives the real selection predicate over
    # MM's real check table; also prints the eligible-host supply count.
    redship_add_test(NAME ForeignHostEligibility COMMAND redship --test foreign-host-eligibility)
    # #502: MM's award callback was still the Lane A1 logging stub, so the whole
    # consumer walk landed on a no-op. Drives the REAL MM_ConsumeSharedItems ->
    # MM_AwardSharedItem -> MM_ForeignItem_Give chain and asserts one award per
    # crossing, order preservation, origin filtering, and that a NULL PlayState
    # defers the give instead of dereferencing it.
    redship_add_test(NAME ForeignAwardMM COMMAND redship --test foreign-award-mm)
    # #510: the reverse direction's SOURCE pool (kForeignPoolMMV1). Display-free
    # — the table is a static in the WHOLE_ARCHIVE'd 2ship_rando and its
    # registrar runs before main() — so this row doubles as the runtime proof
    # that the registrar survived the link. A dropped file-scope initializer is
    # silent at compile and link time and would leave OoT unable to place
    # anything (#516's dead-registrar class).
    redship_add_test(NAME ForeignPoolMM COMMAND redship --test foreign-pool-mm)
    # Shared cross-game resources (#525): rupees and hearts are ONE quantity
    # spanning both games. Locks the delta-harvest watermark that survives MM's
    # 500-rupee tier-3 wallet against OoT's 999 (a naive copy costs the player
    # 300 rupees per round trip), the monotonic/consumable split, the
    # first-harvest seed that stops a .redsave load from doubling the balance,
    # and the canonical heart quantity with its 20-heart clamp. Display-free,
    # ROM-free and save-free, so it runs in this redship tier.
    redship_add_test(NAME SharedResources COMMAND redship --test shared-resources)
    redship_add_test(NAME ComboSpoilerView COMMAND redship --test combo-spoiler-view)
    redship_add_test(NAME ComboSpoilerWindow COMMAND redship --test combo-spoiler-window)
    # Combo tracker (#458). Display-free: the MM adapter is driven over an
    # AUTHORED shadow blob at the offsets the real MM TU registered, and the
    # OoT adapter over an authored heap Rando::Context — no fill, no archives.
    # The window row is the ADR 0008 inertness tripwire (no ImGui context, so
    # an ungated draw path aborts the process).
    redship_add_test(NAME ComboTrackerView COMMAND redship --test combo-tracker-view)
    redship_add_test(NAME ComboTrackerWindow COMMAND redship --test combo-tracker-window)
    # MM randomizer options (#497 step 4, #499). Display-free: the option table
    # is a static global in the WHOLE_ARCHIVE'd 2ship_rando, the profile resolver
    # runs over a zeroed MM SaveContext with no fill, and the pane's window lock
    # never reaches ImGui.
    redship_add_test(NAME MMRandoOptions COMMAND redship --test mm-rando-options)
    redship_add_test(NAME MMPairedProfile COMMAND redship --test mm-paired-profile)
    redship_add_test(NAME ComboMMOptionsWindow COMMAND redship --test combo-mm-options-window)
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
    # #560 archive-handle contention lock: 4 loader threads + 1 hot reader
    # through the REAL ResourceManager against the soh.o2r that #562 mounts
    # in-tier, asserting no concurrent cold load ever returns null / init-data
    # type 0 / bytes differing from a single-threaded read. Deterministic
    # regression tripwire for the per-archive mutex in the libultraship fork —
    # on the unfixed archive layer (one shared, unlocked zip_t) it fails within
    # a few hundred loads. Display-free: the test's own threads are the
    # concurrency, so it needs no Fast3dWindow and runs in this tier.
    #
    # SKIP_RETURN_CODE: the netplay-relay job re-runs this label WITHOUT
    # archives on purpose (the tier's archive-less control, #562); there the
    # row self-skips (exit 77) instead of going red. That skip cannot mask a
    # staging regression — RandoGenFullInit hard-fails on exactly that
    # condition in the rando tier.
    redship_add_test(NAME ZipContention COMMAND redship --test zip-contention)
    set_tests_properties(ZipContention PROPERTIES SKIP_RETURN_CODE 77)
    # #577 cross-game model resolution lock: with ONLY OoT brought up, mount the
    # curated cross-game archive and assert an MM-exclusive model's display list
    # parses and every reference in its command stream resolves out of a non-OoT
    # archive, with no raw segmented texture reference left (those resolve
    # against the HOST game's segment table — the OoTMM kObjectPatches[] hazard,
    # and the one real objection to cross-game rendering).
    #
    # SKIP_RETURN_CODE: redship.o2r only exists once GenerateRedshipOtr has run,
    # which needs BOTH games extracted. A single-game or archive-less tree skips
    # instead of going red, same convention as ZipContention above.
    #
    # RSBS_CROSSGAME_MODEL_PATH overrides which curated display list is walked.
    # It exists so the raw-segmented-texture assertion can be shown to fire
    # without a rebuild — curate an object that has one, point the variable at
    # it, watch the row go red. Unset in the shipped row, on purpose.
    redship_add_test(NAME CrossGameModel COMMAND redship --test crossgame-model)
    set_tests_properties(CrossGameModel PROPERTIES SKIP_RETURN_CODE 77)

    # #595 curated-archive mount-order lock: soh.o2r and 2ship.o2r — the two
    # archives WE generate from in-tree custom assets — collided on 595 paths,
    # 21 differing in content, in the one flat ArchiveManager both are mounted
    # into. Last-added-wins meant chest-corner textures (and three accessibility
    # text banks, and Fast3D's four default shaders) depended on which game
    # booted first. This row mounts both archives BOTH WAYS ROUND and requires
    # every path to resolve to identical bytes either way, with anti-vacuity
    # guards on the archive sizes and on the collision set being non-empty.
    #
    # #593 mod-survival lock: EnsureGameArchivesLoaded re-adds the destination
    # game's base archives on every switch, which put them back on top of the
    # player's mods and silently revoked every override. This row drives the
    # production Combo_EnsureGameArchivesLoaded and carries its own
    # empty-registry negative control.
    #
    # Same SKIP_RETURN_CODE policy as ZipContention: the netplay-relay job
    # re-runs this label without archives on purpose.
    redship_add_test(NAME CuratedArchiveOrder COMMAND redship --test curated-archive-order)
    set_tests_properties(CuratedArchiveOrder PROPERTIES SKIP_RETURN_CODE 77)
    redship_add_test(NAME ModArchiveSurvivesSwitch COMMAND redship --test mod-survives-switch)
    set_tests_properties(ModArchiveSurvivesSwitch PROPERTIES SKIP_RETURN_CODE 77)
    # Unified save (.redsave) headless tests — Phase 2 T6 (#35)
    redship_add_test(NAME SaveRoundtripTiers COMMAND redship --test save-roundtrip-tiers)
    redship_add_test(NAME SaveHeader COMMAND redship --test save-header)
    redship_add_test(NAME SaveHasDelete COMMAND redship --test save-has-delete)
    redship_add_test(NAME SaveVersionReject COMMAND redship --test save-version-reject)
    redship_add_test(NAME SaveSizeMismatch COMMAND redship --test save-size-mismatch)
    redship_add_test(NAME SaveLegacySize COMMAND redship --test save-legacy-size)
    redship_add_test(NAME SaveCrcCorrupt COMMAND redship --test save-crc-corrupt)
    # REFUSED-state locks (#533): a .redsave that fails validation (CRC,
    # truncation, wrong header.slot, future version) is QUARANTINED (renamed
    # aside byte-exact with a reason suffix, never overwritten), the refusing
    # session takes a per-slot write latch so the next autosave cannot destroy
    # the evidence, file-create quarantines before its first write, and the
    # slot surface reports ABSENT / VALID / REFUSED as three different facts.
    # Counterfactual: revert the latch and SaveWriteLatch's direct Save() call
    # rename-overwrites the corrupt fixture — the exact #533 data loss.
    redship_add_test(NAME SaveRefusedQuarantine COMMAND redship --test save-refused-quarantine)
    redship_add_test(NAME SaveWriteLatch COMMAND redship --test save-write-latch)
    redship_add_test(NAME SaveArmOnCreate COMMAND redship --test save-arm-on-create)
    redship_add_test(NAME SaveRefusedMeta COMMAND redship --test save-refused-meta)
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
    # The .redsave commit choke point (#537/#531): every commit is a
    # game-thread-marshalled snapshot with a monotonic generation stamped into
    # both durable artifacts; the write phase reads no live state (the torn
    # .redsave becomes unrepresentable), and load compares the two artifacts'
    # stamps to detect freshness divergence.
    redship_add_test(NAME CommitGenerationMonotonic COMMAND redship --test commit-generation-monotonic)
    redship_add_test(NAME CommitTornWrite COMMAND redship --test commit-torn-write)
    redship_add_test(NAME CommitGenerationSkew COMMAND redship --test commit-generation-skew)
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
    # MM notification bridge (#427 item 1): MM's BenGui/Notification.cpp is
    # excluded, so MM's toasts render on OoT's overlay. They used to bind OoT's
    # identically-mangled Notification::Emit with MM's own Options — safe only
    # while both ports' structs stayed layout-identical, and unlockable at link
    # time (one Emit definition survives, so no link error can catch a
    # divergence). This drives the explicit ComboNotification bridge that
    # replaced that coincidence and checks the toast that lands in OoT's store,
    # and still compares the two ports' Options layouts: both trees declare the
    # type, so their implicit ctor/dtor COMDAT-fold whether or not the struct
    # itself crosses.
    redship_add_test(NAME MMNotificationBinding COMMAND redship --test mm-notification-binding)
    # MM scaled framebuffer draw (#386): MM's framebuffer_effects.c is excluded,
    # so MM's FB_DrawFromFramebufferScaled bound OoT's surviving body, which
    # reads OoT_gScreenWidth/Height. MM's dimensions diverge (HiRes 576 with the
    # Bombers' Notebook open vs OoT's 320), so the shared body mis-scales MM's
    # VisFbuf draw. This locks MM's call to its own MM_-dimension body (only one
    # definition survived, so no link error could catch the cross-bind).
    redship_add_test(NAME MMFbEffectsBinding COMMAND redship --test mm-fb-effects-binding)
    # MM flash page-table OOB from the 0xFF fileNum sentinel
    # (games/mm/2s2h/mm_flash_filenum_test.cpp). A cross-game MM session runs
    # with gSaveContext.fileNum == 0xFF (no real file slot); the moon-crash reset
    # and the owl-delete write it fires index gFlashSave*Pages /
    # gFlashOwlSave*Pages by fileNum * FLASH_SAVE_MAIN_MULTIPLIER, so 0xFF runs
    # hundreds of entries past the tables' end, and the reset copies the garbage
    # over the live save (spawn-as-Fierce-Deity / all-Ocarina corruption).
    # Display-free and ROM-free, so it runs in this redship tier.
    redship_add_test(NAME MMFlashFileNumOob COMMAND redship --test mm-flash-filenum-oob)
    # MM's redship-native unified-save capture (#35 follow-up,
    # games/mm/2s2h/mm_unified_save_test.cpp). MM persists nothing in
    # single-exe: 2s2h/SaveManager/*.cpp is filtered out of the link and the
    # replacement flash stubs are a -1 read plus an EMPTY write, so the only
    # thing that ever deposited MM bytes into the cross-game shadow was the
    # departure freeze on a portal crossing. An operator .redsave confirmed it:
    # Tier-2 had 16647 non-zero bytes, Tier-3 was 65536 zeros. Locks slot
    # normalization (0xFF must mean "no slot", never clamp), the no-slot no-op,
    # the round trip through a real file, sourceGame == GAME_MM, and that the
    # capture is FULL-WIDTH rather than the sizeof(Save) prefix the excluded
    # file used. Display-free and ROM-free, so it runs in this redship tier.
    redship_add_test(NAME MMUnifiedSaveCapture COMMAND redship --test mm-unified-save-capture)
    # MM's capture must not advance the SHARED-RESOURCE POOL for a commit the
    # write latch refuses (#591, games/mm/2s2h/mm_capture_harvest_gate_test.cpp).
    # MM_Combo_CaptureSaveToUnifiedSlot harvested rupees/hearts/magic/ammo into
    # gComboCtx.sharedResources BEFORE RsbsSave_Save checked the #533/#568
    # armed-session latch, so an un-established or REFUSED slot still moved the
    # pool and the RAM watermark table for a record that never reached disk.
    # Apply ASSIGNS the consumable kinds, so the far game materialized that
    # phantom balance verbatim at the next arrival; the monotonic tiers it also
    # raised could never decay back out. Display-free and ROM-free.
    redship_add_test(NAME MMCaptureHarvestGate COMMAND redship --test mm-capture-harvest-gate)
    # MM single-exe hook dispatch (#511, #438): the COND_HOOK/COND_ID_HOOK
    # macros park registrations in the MM-owned S2H::GameHooks registry, but
    # ShouldActorInit / OnActorInit / OnActorDraw / OnOpenText dispatched
    # through the upstream GameInteractor_Execute* names, which reach OoT's
    # active-game-gated wrappers or src/common/mm_stubs.c no-ops. 21 TUs'
    # ShouldActorInit registrants (EnBox's chest-content rewrite among them) and
    # 24 TUs' OnOpenText registrants were registered and never run -- MM items
    # randomized while their models and dialog stayed vanilla. Display-free and
    # ROM-free, so it runs in this redship tier.
    redship_add_test(NAME MMHookDispatch COMMAND redship --test mm-hook-dispatch)
    # filePlaytime epoch injection (#513): SavingEnhancements_AdvancePlaytime is
    # called from plain C (z_sram_NES.c, z_kaleido_scope_NES.c) regardless of
    # hook wiring, and accrued now - lastTimeLog with lastTimeLog unseeded (0) --
    # the OnSaveLoad seeder was elided (#516) and the value is re-zeroed on
    # new-file paths -- writing a ~56-year Unix epoch into the PERSISTED
    # filePlaytime. Display-free and ROM-free, so it runs in this redship tier.
    redship_add_test(NAME MMPlaytimeSeed COMMAND redship --test mm-playtime-seed)
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

    # #560 ("Why CI never saw it"): every OTHER row in this tier sets
    # RSBS_DISABLE_OTR_INIT=1, which skips the whole OTRMessage_Init /
    # OTRAudio_Init / OTRExtScanner / VanillaItemTable_Init / DebugConsole_Init
    # block (games/oot/soh/OTRGlobals.cpp:1811-1823). The flag started life as a
    # bisect knob for init crashes (#199, e2a181c1) and reached this tier as
    # diagnostic scaffolding in the row above (#339, 8dc2786d) — it was then
    # copied verbatim onto 13 more rows. No row ever justified it independently —
    # but it turned out to be load-bearing for a reason nobody had written down,
    # found by adding this row and watching it hang for 300s on Linux CI:
    #
    #   CI staged soh.o2r where only the install rules look
    #   (${CMAKE_BINARY_DIR}/soh), while ctest rows run with cwd =
    #   ${CMAKE_BINARY_DIR} and resolve archives via LocateFileAcrossAppDirs,
    #   which probes the app-config dir, then the binary's own directory, then
    #   "./" — all three being ${CMAKE_BINARY_DIR} for a ctest row, and none of
    #   them ${CMAKE_BINARY_DIR}/soh. So every row in this tier was booting
    #   with ZERO archives mounted. With none loaded, libultraship PAUSES the
    #   ResourceManager thread pool forever (ResourceManager.cpp:58-61, "Nothing
    #   ever unpauses the thread pool"), and OTRAudio_Init's synchronous
    #   ResourceMgr_LoadDirectory("audio") then deadlocks on .get().
    #
    # The archives are now staged where the rows look, in both places they can
    # come from: the build itself (copy-existing-otrs.cmake and the Generate*Otr
    # targets also copy into ${CMAKE_BINARY_DIR}, which is what makes a plain
    # local `ctest --test-dir <dir>` work), and a staging step in each workflow
    # (CI downloads the archives as artifacts and never runs those targets in the
    # test job). So the tier runs with soh.o2r mounted and the pool live —
    # locally and on CI alike. With that fixed nothing in the block
    # needs the flag: soh.o2r carries no `audio/` entries so the precache is a
    # no-op and the audio thread parks on audio.cv_to_thread (no frame loop
    # signals it); OTRExtScanner / VanillaItemTable_Init / DebugConsole_Init are
    # pure in-process work; and OTRMessage_Init self-skips on its inner
    # hasGameArchive gate, which is false wherever oot.o2r is absent. The row
    # additionally asserts a mounted archive BEFORE bring-up, so a staging
    # regression fails in a second instead of hanging, and pairs OTRAudio_Init
    # with OTRAudio_Exit (defensively — `--test` mode ends at _Exit and never runs
    # static destructors today).
    #
    # This is the ONE row whose bring-up matches a player's, and the only one
    # whose fill runs on a worker thread (the 1-arg GenerateRandomizer overload
    # that z_file_choose.c:824 reaches, not the synchronous 3-arg one). It
    # asserts the un-masking before it generates, so re-adding the flag here
    # fails the row instead of quietly turning it into a RandoGen clone.
    #
    # It does NOT reproduce #560's crash and is not meant to: both sides of that
    # race read oot.o2r-only entries and one of them is the render thread, so a
    # ROM-free display-only tier structurally cannot host it. See #560.
    #
    # Deliberately NOT folded into RandoGen: that row's masked configuration is
    # the one 13 siblings share, so it stays as-is and this row is the diff.
    redship_add_test(NAME RandoGenFullInit COMMAND redship --test rando-gen-full-init
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

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

    # #510: the reverse foreign pool over a REAL OoT fill. In the `rando` tier
    # rather than the display-free one for a correctness reason, not a
    # convenience one: OoT's host predicate reads GetPlacedRandomizerGet(), a
    # FILL RESULT, so with no fill every location is RG_NONE, the predicate
    # accepts nothing, and a "only chests are hosts" assertion passes with a
    # count of zero. The row asserts a NON-ZERO eligible-host count and prints
    # it, so host supply is visible before it becomes a shortfall.
    redship_add_test(NAME ForeignPlacementOoT COMMAND redship --test foreign-placement-oot
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

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

    # ADR 0010 increment 1.2, lock (b), single-run half: a paired generation
    # whose first ladder attempt fails DETERMINISTICALLY climbs one rung,
    # converges through the real OnSaveInit chain, reports its winning
    # attempt, stamps the provenance record, and writes the world digest.
    # The failing rung is INJECTED (one short foreign-placement pass) rather
    # than scan-pinned: measured 2026-07-31, every natural first-attempt
    # failure under the dead-end-prone profile was the fill's 10s WALL-CLOCK
    # abort, and pinning one of those would pin a race that flips on machine
    # speed. Rationale, the scan recipe and the wall-clock discriminator live
    # at kLadderMasterSeed in games/mm/2s2h/mm_rando_gen_test.cpp. Also the
    # --test row the completeness guard requires for the mm-paired-attempt
    # dispatch entry. Timeout above its siblings: the fill runs twice by
    # construction.
    redship_add_test(NAME MMPairedAttemptGen COMMAND redship --test mm-paired-attempt
        LABEL rando
        TIMEOUT 240
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # ADR 0010 increment 1.2, lock (b), two-process half: the SAME pinned seed
    # must produce a byte-identical digest — final seed, winning attempt,
    # placement hash, every foreign placement — across two fresh processes
    # (CMake/CheckPairedAttemptDeterminism.cmake; two processes for the same
    # generator-re-entry reason SeedDeterminism uses them). This is the row
    # that goes red if the ladder derivation ever consumes runtime state the
    # documented hash recipe does not name. A meta row (its COMMAND drives no
    # --test entry); larger timeout because it runs the windowed bring-up twice.
    redship_add_test(NAME MMPairedAttemptDeterminism
        COMMAND ${CMAKE_COMMAND}
                -DREDSHIP_EXE=$<TARGET_FILE:redship>
                -DWORK_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_CURRENT_LIST_DIR}/CheckPairedAttemptDeterminism.cmake
        LABEL rando
        TIMEOUT 480
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # ADR 0010 increment 1.2, lock (c): a profile that CANNOT converge (every
    # check excluded, so every attempt dies pre-fill) exhausts the bounded
    # ladder at the real switch-entry arrival and refuses LOUDLY — save
    # reverted to vanilla, slot latched REFUSED(generation) through the #533
    # machinery, no leaked placements, no stale provenance — while the same
    # arrival with the fixture undone generates and latches nothing. The
    # anti-regression row for the silent-vanilla-revert class (#564 V7).
    # Its third leg is the determinism boundary: a WALL-CLOCK fill abort (the
    # one failure that depends on the machine rather than the seed) must stop
    # the ladder at attempt 1 rather than climb to a different world, so the
    # same seed + settings cannot generate differently on a slow machine.
    redship_add_test(NAME MMPairedExhaustion COMMAND redship --test mm-paired-exhaustion
        LABEL rando
        TIMEOUT 240
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # #439 follow-up: MMPairSwitchEntry locks the cross-game arrival convergence;
    # this row locks the OTHER paths into MM gameplay the arrival fix does not
    # touch. The IS_RANDO COND_HOOKs (Rando.h: saveType == SAVETYPE_RANDO, re-
    # evaluated on every OnSaveLoad) must match the save at every convergence:
    # the file-select LOAD must RE-ARM after the boot chain's disarming
    # OnSaveLoad (the disarm-then-rearm ordering #439 got wrong on the arrival
    # path, and the ordering the owl-save reload relies on), and an in-session
    # reload (Song of Time / cycle reset / DayTelop) must leave a live rando
    # session's armed hooks untouched. Same display requirement, hence the same
    # label/env.
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

    # #487, the same class on the save a player performs every cycle. An owl
    # save ends in Sram_UpdateWriteToFlashOwlSave re-reading the file it just
    # wrote and memcpy'ing it over gSaveContext for offsetof(SaveContext,
    # fileNum) -- all of struct Save, ShipSaveInfo included. MM's flash read is
    # a no-op stub in single-exe (games/mm/2s2h/mm_save_manager_stubs.c), so
    # that commit writes ZEROS over the paired world's randomizer identity, in
    # live gameplay. Also covers the file-copy leg (func_80147414).
    #
    # Tier: `rando`, not `redship`, and the choice was made on evidence rather
    # than by copying the neighbours (#491 step 1). The display requirement
    # comes from the DISPATCHER, not from the save code: every bridge in
    # mm_rando_gen_test.cpp runs InitOTRForMMFirstBoot, whose OTRGlobals ctor
    # constructs a Fast3dWindow when no window exists (games/oot/soh/
    # OTRGlobals.cpp). The probe additionally needs MM_Rando_Init and a
    # populated Rando::Logic::Regions. A display-free row IS possible for the
    # flash code alone -- MMFlashFileNumOob drives Sram_ResetSaveFromMoonCrash
    # with no window -- but it cannot carry the VB arm-state probe, which is
    # this row's entire point.
    redship_add_test(NAME MMOwlSaveArmState COMMAND redship --test mm-owl-save-arm-state
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
