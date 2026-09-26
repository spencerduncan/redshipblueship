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
    # MM's per-trick table, same seam one id space over (#578 part 1). Separate
    # from the option registry because MMRT_* is a different id space indexing a
    # different save array, and the MMRandoOptions lock asserts the option
    # descriptor set covers RandoOptionId exactly.
    ${CMAKE_SOURCE_DIR}/src/common/combo_mm_tricks_view.c
    ${CMAKE_SOURCE_DIR}/src/common/ComboMmOptionsWindow.cpp
    # The tier-4 combo settings (ADR 0011 increment 2): the five gCombo.Rando.*
    # keys' authoring surface — the one reader the resolver uses and the one
    # writer pair that refuses once the record is frozen — and the pane that
    # renders it. C++ because the CVar store hangs off the Ship::Context
    # singleton and the read must be guarded on its existence.
    ${CMAKE_SOURCE_DIR}/src/common/combo_settings_view.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ComboSettingsWindow.cpp
    # The combo-logic coordinator (ADR 0010 increment 3, #645; O4 = composition):
    # the union bag, the two origin-keyed placement tables and the round loop,
    # over a registered per-game engine vtable. Game-header-free like
    # foreign_items.c, and NOT wired into any production path at this commit —
    # no shipping TU registers an engine, so the coordinator is reachable only
    # from its own locks.
    ${CMAKE_SOURCE_DIR}/src/common/combo_logic.c
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
    # The creation event's FILL BUDGET and PROGRESS SURFACE (#582's operator
    # decision; ADR 0010 increment 2). Host-speed calibration, the ~30 s floor
    # and the phase channel the creation seam reports into. APPENDED, never
    # reordered.
    ${CMAKE_SOURCE_DIR}/src/common/gen_budget.c
    # The ON-SCREEN creation-progress surface's state machine (#582). The
    # presentation half lives in games/oot/soh/SohGui/CreationProgressOverlay.cpp;
    # this half is game-header-free C so a headless row can drive it. APPENDED,
    # never reordered.
    ${CMAKE_SOURCE_DIR}/src/common/gen_progress_overlay.c
    # The combo triforce hunt (ADR 0010 answer O10): the frozen record, the
    # arming gate of the one shared piece count, and the win decision both
    # ports' piece-give arms call. Game-header-free. APPENDED, never reordered.
    ${CMAKE_SOURCE_DIR}/src/common/triforce_hunt.c
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
    ${CMAKE_SOURCE_DIR}/src/common/combo_settings_view.h
    ${CMAKE_SOURCE_DIR}/src/common/ComboSettingsWindow.h
    # Header for combo_logic.c above — it also carries the ENGINE CONTRACT the
    # two follow-on lanes implement (#645)
    ${CMAKE_SOURCE_DIR}/src/common/combo_logic.h
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
    # Header for triforce_hunt.c above (ADR 0010 O10)
    ${CMAKE_SOURCE_DIR}/src/common/triforce_hunt.h
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

# The curated-archive-generator lock (--test curated-archive-generator, #605)
# drives scripts/make_redship_otr.py as a real subprocess, so it needs an
# absolute interpreter path at runtime -- not a PATH-searched "python3", which
# ZapdSubprocess_Run's CreateProcessA/execv cannot resolve. This module is
# include()'d before the top-level CMakeLists.txt's own
# find_package(Python3 COMPONENTS Interpreter) call, so it is repeated here;
# find_package is idempotent and CMake caches the result either way. The test
# SKIPs (not fails) when no interpreter was found.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_EXECUTABLE)
    target_compile_definitions(redship_common PRIVATE REDSHIP_PYTHON3_EXECUTABLE="${Python3_EXECUTABLE}")
endif()

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
    # #495 / ADR 0011 decision 3: the cross-game item class is a RULE over both
    # pools and the RSBS_ITEMCLASS_* bitset in the frozen combo record is the
    # setting that selects it. Three claims, in the order they can fail: the
    # DEFAULT bitset draws the pinned tables byte-identically (the parity pin —
    # this row is the ROM-free half of it; the half that notices a MOVED draw is
    # GoldenSeedDigestDefault, not SeedDeterminism, which as #688 established
    # diffs two runs of one binary and stays green through any deterministic
    # move); a
    # NARROWED bitset draws only members of the armed classes (red before the
    # rule engine, when the bitset was stored and compared but consumed by
    # nothing); and the name inverse stays TOTAL over every item any class can
    # name even with ZERO classes armed, which is why the class carries no seed
    # term (accepted answer O3) and why the spoiler-LOAD path can run in a
    # process that never generated. Display-free and ROM-free like its siblings.
    redship_add_test(NAME ForeignItemClass COMMAND redship --test foreign-item-class)
    # Shared cross-game resources (#525): rupees and hearts are ONE quantity
    # spanning both games. Locks the delta-harvest watermark that survives MM's
    # 500-rupee tier-3 wallet against OoT's 999 (a naive copy costs the player
    # 300 rupees per round trip), the monotonic/consumable split, the
    # first-harvest seed that stops a .redsave load from doubling the balance,
    # and the canonical heart quantity with its 20-heart clamp. Display-free,
    # ROM-free and save-free, so it runs in this redship tier.
    redship_add_test(NAME SharedResources COMMAND redship --test shared-resources)
    # The shared OCARINA (#668): one instrument across both games, and the FIRST
    # shared kind whose existence a player chooses — armed by the
    # gCombo.Rando.SharedOcarina bit frozen into ComboSettingsRecord.comboFlags
    # at file creation. Locks the two claims the row above cannot make: with the
    # option OFF nothing is harvested, applied or even slotted (so an existing
    # world's .redsave is byte-identical to one written before the option
    # existed), and the arming gate is consulted by BOTH the harvest and the
    # apply, because a one-sided gate on a pair that is not inverse leaks rather
    # than under-shares. Display-free, ROM-free and save-free.
    redship_add_test(NAME SharedOcarina COMMAND redship --test shared-ocarina)
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
    # Spoiler-drop identity gate (#610). MM's spoiler-LOAD path rebuilt
    # gComboCtx.foreignPlacements from ANY dropped spoiler with no comparison
    # against the #570 identity terms, and the pickup that followed authored a
    # durable shared-item record a later genuine pair would redeem. Display-free
    # for the same reasons as the two rows above — the spoiler writer/loader and
    # the placement table need no fill, no display, and no ROM.
    redship_add_test(NAME MMSpoilerIdentity COMMAND redship --test mm-spoiler-identity)
    redship_add_test(NAME MMForeignPickupGate COMMAND redship --test mm-foreign-pickup-gate)
    # Combo-level arrival identity gate (#498, ADR 0011 increment 1). Drives the
    # REAL MM_Rando_PairOnCrossGameArrival. A divergent frozen combo record must
    # refuse through the #533/#568 surface AND NAME the diverged rule — the
    # capability that justifies carving a 12-byte record instead of spending ADR
    # 0009 claim 2's 4-byte digest alone; without it the record buys display
    # only. The legacy legs pass hadFrozenState=1 so the gate returns before any
    # generation dispatch, which keeps this row ROM-free while exercising the
    # exact early-return path the O5 transitional writer has to sit ahead of.
    redship_add_test(NAME MMComboSettingsGate COMMAND redship --test mm-combo-settings-gate)
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

    # #605 curated-archive GENERATOR lock: make_redship_otr.py must refuse a
    # curated model whose display list carries a raw segmented texture
    # reference, not just a path collision (#602) or a dispatched resource
    # type (#603). Drives the real generator script as a subprocess against a
    # known-bad object (objects/object_slime/gChuchuEyesDL, the #577 spike's
    # own counterfactual) and, as a positive control, the real shipped
    # manifest. Same SKIP_RETURN_CODE policy: needs both extracted archives
    # and a configured Python interpreter.
    redship_add_test(NAME CuratedArchiveGenerator COMMAND redship --test curated-archive-generator)
    set_tests_properties(CuratedArchiveGenerator PROPERTIES SKIP_RETURN_CODE 77)
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
    # Whole-file commit (#589, operator ruling 2026-08-04) — the read half of
    # the same machinery. A durable save-and-quit in EITHER half commits BOTH
    # halves at ONE generation, and on load the newest WHOLE commit wins: its
    # Tier-2 is armed and delivered over OoT's older .sav. That is what
    # structurally retires #531 (a durable RSBS_SHARED_ITEM_REDEEMED record
    # outliving the item it accounts for = permanent loss of a progression
    # item). Counterfactual: drop the Tier-2 arming from LoadSlot and
    # WholeFileRedeemedItem goes red on the assertion that names the loss.
    redship_add_test(NAME WholeFileCommit COMMAND redship --test whole-file-commit)
    redship_add_test(NAME WholeFileRedeemedItem COMMAND redship --test whole-file-redeemed-item)
    # The frozen COMBO-LEVEL rule record (#498, ADR 0011 increment 1): 16 bytes
    # out of reserved[124] -> reserved[108], carved at .redsave offsets 880
    # (comboSettingsHash, ADR 0009 claim 2 spent exactly as reserved) and 884
    # (ComboSettingsRecord, ADR 0011 claim 10). ComboSettingsCanonical is the
    # golden-vector row and it is the one #574's cross-peer identity handshake
    # will inherit: the digest input is pinned byte-for-byte precisely so a
    # struct-layout or endianness change is a red test rather than a silently
    # different world identity on one peer's build.
    redship_add_test(NAME ComboSettingsFormat COMMAND redship --test combo-settings-format)
    redship_add_test(NAME ComboSettingsCanonical COMMAND redship --test combo-settings-canonical)
    redship_add_test(NAME ComboSettingsDivergence COMMAND redship --test combo-settings-divergence)
    redship_add_test(NAME ComboSettingsLegacyFreeze COMMAND redship --test combo-settings-legacy-freeze)
    # ADR 0011 increment 2 (+ #668): the six tier-4 gCombo.Rando.* keys and the pane.
    # ComboSettingsAuthoring proves the keys reach the record BEFORE the freeze
    # (the frozen record is what the player authored), that the defaults still
    # reproduce the shipped record and its pinned fingerprint byte for byte
    # (which is why the GOLDEN rows do not move — this parenthesis used to name
    # "SeedDeterminism / MMRandoGen / HeadlessForeignDigest", of which the first
    # cannot detect a move at all and the last is not a CTest row; #688), that an
    # out-of-space store value resolves to the default and never
    # to a new enumerator, and that the writers refuse once frozen — the gate
    # is on the writers, not the widget (ADR 0004 §6). ComboSettingsWindow is
    # the common-owned pane's headless lock (ADR 0008). Both need the
    # display-free shared bring-up (the keys live in the CVar store) and run in
    # this ROM-free tier.
    redship_add_test(NAME ComboSettingsAuthoring COMMAND redship --test combo-settings-authoring)
    redship_add_test(NAME ComboSettingsWindow COMMAND redship --test combo-settings-window)
    # #655 moved the PRESENTATION into the one live menu: the six keys are rows —
    # in the tier-4 Combo section's Cross-Game Rules page since #497 step 6 moved
    # them off their interim host in SohMenuRandomizer.cpp — and the pane above is
    # registered but no longer opened by anything. ComboSettingsRows
    # builds a SohMenu headless, calls the real AddMenuCombo(), and drives
    # the rows' PreFuncs/Callbacks — so it locks the three ways a presentation
    # move goes wrong silently: a missing row, a CVar-typed row that writes the
    # store itself and never reaches the freeze gate, and a frozen row that shows
    # the CVar instead of the record the world was built from (ADR 0004 §4.2,
    # §6 state 4).
    redship_add_test(NAME ComboSettingsRows COMMAND redship --test combo-settings-rows)
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
    # Same class as MMCaptureHarvestGate above (#591/#600), on OoT's OnExitGame
    # seam (#606, games/oot/soh/oot_exit_harvest_gate_test.cpp): the
    # SaveManager.cpp OnExitGame GameInteractor hook harvested OoT's shared
    # cross-game resources BEFORE RsbsSave_Save checked the #533/#568
    # armed-session latch, so an un-established or REFUSED slot still moved
    # the pool and the RAM watermark table for a record that never reached
    # disk. OoT's OTHER staging seam (SaveSection, ~:1553) already guarded the
    # identical sequence; OnExitGame was the one seam left unguarded.
    # Display-free and ROM-free, so it runs in this redship tier.
    redship_add_test(NAME OoTExitHarvestGate COMMAND redship --test oot-exit-harvest-gate)
    # ADR 0009 decision 4b (#590/#625, games/mm/2s2h/mm_death_decline_autosave_test.cpp):
    # MM's cross-game game-over "don't continue" exit is an AUTOSAVE POINT iff
    # the Autosave enhancement is on -- a whole-file commit through the #569
    # choke point taken AFTER the #625 revive, and the enhancement's interval
    # clock reset -- and writes NOTHING at the death moment with it off. Locks
    # both halves of the iff against the real MM_Combo_GameOverExitToOoT, the
    # gameMode (not fileNum) gate, the #533/#568 latch (a refused commit is not
    # an autosave: no file, no clock reset, no pool movement), and the
    # cross-game gate that keeps standalone 2ship vanilla. Display-free and
    # ROM-free; needs the shared bring-up only for CVarSetInteger.
    redship_add_test(NAME MMDeathDeclineAutosave COMMAND redship --test mm-death-decline-autosave)
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
    # One layer earlier than the row above (#539): a hook is only as armed as the
    # registrar that (un)registers it, and MM's registrar map — S2H::ShipInit,
    # separate from OoT's since the #375 COMDAT-fold split — had no CVar-change
    # driver in the link. The unified menu's widgets call OoT's
    # ShipInit::Init(cvar), so a converged key like
    # gEnhancements.RememberSaveLocation re-armed OoT on the click and left MM
    # latched at its first-boot value for the rest of the process.
    # Display-free and ROM-free, so it runs in this redship tier.
    redship_add_test(NAME MMShipInitDriver COMMAND redship --test mm-shipinit-driver)
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
    # Registrar coverage (#516): games/mm/2s2h/BenPort.cpp is excluded from the
    # single exe and was the SOLE caller of InitOTR's registration sequence, so
    # 2ship_enh (a plain STATIC archive) lost the TUs entirely -- CustomItem and
    # CustomMessage's RegisterHooks, RegisterSavingEnhancements and
    # RegisterAutosave were all 0-hit in redship.map, leaving every randomized
    # custom item uncollectable and every rando textbox on vanilla message
    # 0x004B. They are re-homed into MM_Rando_Init; this row drives that real
    # entry point and asserts each registry actually filled.
    #
    # It is NOT redundant with check-registrar-elision.sh's per-symbol
    # allowlist: that grep proves the symbols LINKED, which a call moved under a
    # never-true condition would still satisfy. It is also the only possible
    # gate on RegisterAutosave, whose name the script cannot attribute (OoT
    # ships a static twin at Enhancements/QoL/Autosave.cpp). Display-free and
    # ROM-free -- MM_Rando_Init gates its one asset-dependent call behind
    # MM_Rando_AssetsReady() -- so it runs in this redship tier.
    redship_add_test(NAME MMRegistrarCoverage COMMAND redship --test mm-registrar-coverage)
    # #618 (#516 Phase 3): the two entries BenPort's InitOTR list ends with,
    # OTRExtScanner and PlayerCustomFlipbooks_Patch, both depend on the SHARED
    # ExtensionCache having been re-scanned once MM's archives are mounted.
    # OTRExtScanner runs once, from OoT's InitOTR, with only OoT's archives
    # mounted, and MM's own copy is in the excluded BenPort.cpp — so MM's paths
    # were absent from the map ResourceMgr_FileExists answers from and MM's HD
    # gfxprint font and its static FD/Deku/Goron faces silently stayed vanilla.
    #
    # MMRegistrarCoverage above proves MM_Rando_Init REACHES both entries, in
    # order; it is ROM-free, so it cannot show the rescan finds anything. This row
    # mounts a real MM-side archive (2ship.o2r — mm.o2r is ROM-derived and never
    # staged on CI) and asserts the content contract: MM-owned paths appear, the
    # cache size grows by EXACTLY the reported number of new keys, a sampled set
    # of OoT-owned paths still resolves, a second rescan is a no-op, and both
    # scope directions add nothing where they should not.
    #
    # Same SKIP_RETURN_CODE policy as the other archive rows: the netplay-relay
    # job re-runs this label archive-less on purpose (#562), and the row also
    # self-skips when the staged archive cannot support a non-vacuous comparison.
    redship_add_test(NAME MMExtensionRescan COMMAND redship --test mm-extension-rescan)
    set_tests_properties(MMExtensionRescan PROPERTIES SKIP_RETURN_CODE 77)
    redship_add_test(NAME MMResumeArena COMMAND redship --test mm-resume-arena)
    redship_add_test(NAME MMStartupRestore COMMAND redship --test mm-startup-restore)
    # The cross-game arrival IS MM's intro event (#654, operator ruling
    # 2026-09-16). Vanilla MM proxies "the intro has not happened yet" off "no
    # Ocarina of Time" and degrades two first-cycle behaviours from it -- Termina
    # Field's EMPTY scene layer 5 (no enemies, no BGM) and the 5x first-cycle
    # clock -- and a combo arrival skips the intro that grants the ocarina, so an
    # MM half whose pairing is not a live rando one got the empty field forever,
    # with no Song of Time to leave the cycle. This row drives the extracted gate
    # (MM_Play_ShouldEmptyFirstCycleTerminaField), the grants
    # (MM_Play_GrantComboArrivalIntroRewards) for both a vanilla and a rando
    # pairing, and the real MM_Play_ConsumeStartupEntrance for the first-entry /
    # restored-return-leg split. Display-free and ROM-free, so it runs in this
    # redship tier.
    redship_add_test(NAME MMComboFirstCycle COMMAND redship --test mm-combo-first-cycle)
    # Pre-freeze discipline (#638, the agent tracker for #635; and #626). Both
    # games keep the CURRENT scene's flags in the live PlayState and copy them
    # into gSaveContext only on a scene transition (Actor_CleanupContext ->
    # Play_SaveCycleSceneFlags / Play_SaveSceneFlags). A cross-game departure
    # never reaches that copy -- the entrance switch freezes the instant
    # nextEntrance is assigned and then kills the gamestate without
    # Play_Destroy; F10 breaks the graph loop the same way -- so every flag set
    # during the final scene visit was frozen as unset and the pickup respawned
    # on the return leg. The MM row also locks #626: an F10 during MM's
    # game-over screen bypasses the kaleido death exit PR #625 guarded, froze
    # health == 0, and the suspend harvest handed OoT the shared CONSUMABLE bar
    # at zero. Each row drives the two REAL freeze drivers
    # (Combo_CheckEntranceSwitch, Combo_FreezeActiveGameForHotSwap) against a
    # calloc'd PlayState and reads the frozen blob back. Display-free and
    # ROM-free, so they run in this redship tier.
    redship_add_test(NAME MMSceneFlagFreeze COMMAND redship --test mm-scene-flag-freeze)
    redship_add_test(NAME OoTSceneFlagFreeze COMMAND redship --test oot-scene-flag-freeze)
    # soh_port registrar elision (#640): Network/Anchor/Menu.cpp and
    # SohGui/ResolutionEditor.cpp register their menu widgets purely through
    # RegisterMenuInitFunc static initializers and export nothing anything
    # references, and soh_port was the one OoT archive linked without
    # WHOLE_ARCHIVE, so the linker dropped both members. The Anchor page shipped
    # widget-less and its unconsumed SetNextWindowPos undocked the game window;
    # the resolution editor simply never appeared in Settings / Graphics, on
    # Windows included (measured, see the test TU). games/oot/CMakeLists.txt now
    # whole-archives soh_port; this row asserts the registrars actually RAN, by
    # the exact size of the MenuInit registries they populate, without naming
    # either symbol (a reference would un-elide them by itself). Runtime
    # complement of check-registrar-elision.sh's libsoh_port.a gate, which is
    # nm-based and Linux-only; this row runs on every platform. Its Anchor arm is
    # compiled in only under ENABLE_REMOTE_CONTROL, which CI sets everywhere and
    # a local build without SDL2_net does not -- see the test TU's header for
    # what that means for red-before-green. Pure (no display, no ROM, no
    # Ship::Context), so it runs in this redship tier.
    redship_add_test(NAME OoTMenuRegistrars COMMAND redship --test oot-menu-registrars)
    # The MM twin of the row above, and the same elided-registrar class (#678).
    # games/mm/2s2h/Enhancements/Songs/BetterSongOfDoubleTime.cpp and
    # SkipSoTCutscenes.cpp register only through a file-scope
    # RegisterShipInitFunc and export nothing anything references, so
    # plain-archive 2ship_enh dropped both objects: two enhancements that did
    # nothing when toggled, and with them the only callers of
    # Rando::ClockShuffle's half-day ownership API -- which is why
    # RO_CLOCK_SHUFFLE was the one row PR #677's re-measure could not promote.
    # They are carved into the WHOLE_ARCHIVE'd 2ship_enh_clockshuffle
    # (games/mm/CMakeLists.txt); this row asserts both TUs reached the link
    # (their ShipInit map entries exist) AND that their registrars RUN when the
    # CVar is driven through the production MM_ShipInit_OnCVarChanged path --
    # the half check-registrar-elision.sh's nm gate cannot see, and which it
    # also cannot see on Windows at all. Names no symbol from either TU (a
    # reference would un-elide them by itself). Pure (no display, no ROM);
    # needs the shared bring-up only for CVarSetInteger.
    redship_add_test(NAME MMClockShuffleSongs COMMAND redship --test mm-clock-shuffle-songs)
    # The entrance -> region cache's registration boundary (#659).
    # Rando::Logic::Regions is filled by eighteen independent ShipInit
    # registrars whose order is link order, and the cache inside
    # GetRegionIdFromEntrance used to be guarded on its own emptiness -- so a
    # lookup made while the graph was PARTIALLY populated built a non-empty map
    # and froze it, leaving every entrance owned by a not-yet-run registrar at
    # RR_MAX for the life of the process (silently: CrawlReachableRegions seeds
    # from it, so the world merely looks less reachable). This row is both the
    # probe #659 asked for -- drive the real MM_Rando_Init and read whether
    # anything looked an entrance up mid-registration, which no static reading of
    # link order can answer -- and the lock on the fix, by reducing Regions to
    # one region, looking up an entrance it does not own, restoring the graph and
    # demanding the right answer. Display-free and ROM-free (MM_Rando_Init's
    # asset phase defers with no archives mounted), so it runs in this redship
    # tier.
    redship_add_test(NAME MMEntranceRegionCache COMMAND redship --test mm-entrance-region-cache)
    # #497's SohMenu remainder, the three rows ADR 0004 asks for.
    #
    # MenuCapabilityGating is the lock #497 named `menu-capability-gating` and
    # recorded as not existing. §5's rule is that an entry appears enabled only
    # when its TU links AND its registrar ran AND its hook has a dispatch, and
    # everything else is disabled-WITH-REASON — so the one thing this row must not
    # do is check only the enabled case. It drives a REAL capability through both
    # answers: SOH_MENU_CAP_MM_HOSTED observes the foreign-item pool registry
    # (never a symbol in MM's TU, which would un-elide it and pass vacuously), and
    # the row un-registers that pool, watches a gated widget grey itself with the
    # capability's reason, then re-registers it and watches the widget recover its
    # NAME as well as its enabled state. Plus §6's four presentation states and
    # every transition between them, and §4.2's marker pass run over a PRODUCTION
    # section (Dev Tools, the only one that registers ROM-free).
    #
    # MenuComboSection owns step 6's shape: the Combo header and its own sidebar
    # CVar, both shipped pages non-empty (an empty multi-column page is #640's
    # undocked-window failure), every row inside a column the page draws, the seven
    # window rows' CVar/WindowName pairs with EmbedWindow(false), the pointer row
    # left on Randomizer / Cross-Game so a persisted sidebar name is not stranded,
    # and the contributed-page extension point lane G hangs #682's MM enhancement
    # rows off.
    #
    # SetMenuCount is #497 step 2's second half, which shipped as prose: ADR 0004
    # §3's "one shell" proviso. Ship::Gui holds a SINGLE menu slot, so a second
    # SetMenu call replaces the first with no error — the count of first-party call
    # sites, and MM's BenMenu.cpp staying out of every target, are the invariant.
    # Source-level and therefore platform-independent, unlike
    # check-registrar-elision.sh's nm gate.
    #
    # All three are display-free and ROM-free; the first two need the shared
    # bring-up for the CVar store but no window.
    redship_add_test(NAME MenuCapabilityGating COMMAND redship --test menu-capability-gating)
    redship_add_test(NAME MenuComboSection COMMAND redship --test menu-combo-section)
    redship_add_test(NAME SetMenuCount COMMAND redship --test setmenu-count)
    # #658: MM_GOAL's "Majora defeated" conjunct (ADR 0010 D1). Three things in
    # one row, all display-free and ROM-free: the predicate answers from the save
    # at all (it used to fall through CanKillEnemy's default assert), its polarity
    # matches all three phases' damage tables in ovl_Boss_07 — notably that a
    # bow-only or hookshot-only kit can stun Majora forever and never kill it —
    # and the lair region carries NO fill-visible goal term, because a region
    # event or check there would bump the glitchless fill's `weight` and move
    # every generated world. Needs the shared bring-up for MM_Rando_InitCore's
    # registrars (they read the CVar store), which is also what populates the
    # region graph leg 2 reads.
    redship_add_test(NAME MMMajoraGoal COMMAND redship --test mm-majora-goal)
    # MM's per-trick vocabulary substrate (#578 part 1): the MMRT_* table's
    # integrity and mirror, the 20 reserved-and-inert keys, the frozen-save
    # predicate, the trick term in the profile identity digest, and finding (a)'s
    # Powder Keg gate proved on the real CanKillEnemy table. Graph-free, so it
    # runs in this display-free tier; finding (b)'s probe needs the region graph
    # and is MMTrickGbtGate in the rando tier below.
    redship_add_test(NAME MMTrickTable COMMAND redship --test mm-trick-table)

    # #661: the Happy Mask Shop interior door IS the OoT<->MM crossing, and OoT's
    # own entrance shuffle lists it as an ordinary EntranceType::Interior pair.
    # This row probes the POOL CANDIDATE LIST: it builds the Interior pool twice
    # off the same live region graph, unfiltered and pinned, and asserts the pair
    # is offered by the first and not by the second — so it fails if the pin is
    # removed AND if upstream ever stops offering the pair (which would make the
    # lock vacuous). It also asserts the pinned pair's entrance indices are
    # src/common/entrance.h's OOT_ENTR_HAPPY_MASK_SHOP / _MARKET_FROM_MASK_SHOP,
    # tying the region-keyed predicate to the index-keyed crossing.
    #
    # Display-free and ROM-free: it brings up only the rando region graph
    # (RegionTable_Init over a fresh Rando::Context), no window and no archives.
    # The end-to-end complement, which needs a real fill, is RandoEntrancePin in
    # the rando tier below.
    redship_add_test(NAME OoTEntrancePin COMMAND redship --test oot-entrance-pin)

    # #682: the curated MM enhancement toggles, in two halves that fail
    # differently on purpose.
    #
    # MenuMmEnhancementRows is the PRESENTATION half. The contributed Combo page
    # is registered (a file-scope registrar in a WHOLE_ARCHIVE'd OoT archive, but
    # #516/#640 are what "should not be elidable" looked like last time), every
    # manifest key has a row bound to exactly that key, every row is inside the
    # one column the page draws, and each row's ADR 0004 §5 presentation matches
    # its manifest liveness class — driven TWICE per row, because ApplyPresentation
    # is called from a PreFunc with its own name as the base and a non-idempotent
    # one compounds the suffix (the defect #497's lane found). All four shipped
    # rows are Live, so the leg that matters most — what a Partial or Dormant row
    # renders — is driven with synthetic rows through the same call.
    #
    # MMEnhancementToggles is the EVIDENCE half, MM-side because the registries
    # are. Per key: exactly one `S2H::ShipInit` registrar under the CVar the menu
    # writes (provider linked AND its initializer ran, in one observation), the
    # registry settles empty with the key off, exactly one registrant with it on,
    # and NOTHING in the other keys' registries — arming one at a time is what
    # makes the attribution exact rather than "something registered". It is also
    # the only possible gate for MM's `RegisterAutosave`, whose symbol name OoT's
    # own static twin satisfies, which is why check-registrar-elision.sh
    # deliberately leaves it off its allowlist. Its last leg is the one #653's
    # triage asked for: drive the real `MM_GameOver_Update` from
    # GAMEOVER_DEATH_FADE_OUT with `gEnhancements.Kaleido.GameOver` cleared and
    # then set, and assert the branch — vanilla reload versus the kaleido prompt
    # arm. That key has no registrar at all (two inline CVar reads in MM's decomp),
    # so its read site IS its liveness evidence.
    #
    # Both display-free and ROM-free; both need the shared bring-up for the CVar
    # store but no window.
    redship_add_test(NAME MenuMmEnhancementRows COMMAND redship --test menu-mm-enhancement-rows)
    redship_add_test(NAME MMEnhancementToggles COMMAND redship --test mm-enhancement-toggles)

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

    # ADR 0010 increment 2 (#644): THE MERGED CREATION EVENT, end to end. The
    # freeze precedes OoT's Fill(), the whole MM half is authored and armed at
    # the file-create seam with MM never booted, OoT's own in-progress save
    # survives the shared-gSaveContext bracket byte-exact, the arrival hydrates
    # or refuses and NEVER generates (a dispatch counter, not a comment), the
    # #582 budget is the ruled one, the pair writes ONE spoiler carrying both
    # crossing directions (#660), #585's join is in force, and (#582) the
    # on-screen progress overlay paints frames from INSIDE the blocking creation
    # call — which needs both a real window and a real creation, so this row is
    # the only place in the suite where it can be observed at all.
    #
    # In the `rando` tier for the same correctness reason as the row above: the
    # creation event refuses to run without a live pairing identity, and every
    # table it authors reads a fill result. The timeout is generous because this
    # row runs TWO real generations (OoT's plus the paired MM half) plus the
    # #585 probe's repeated graph traversals.
    redship_add_test(NAME ComboCreationEvent COMMAND redship --test combo-creation-event
        LABEL rando
        TIMEOUT 420
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

    # #578 finding (b): the Great Bay Temple boss-key connection is trick-gated.
    # Tier `rando` for one reason, stated so nobody "tidies" it into the cheap
    # tier: the edge is a std::function inside Rando::Logic::Regions, and that map
    # is populated by ShipInit registrars reached only through
    # InitOTRForMMFirstBoot, whose OTRGlobals ctor constructs a Fast3dWindow. The
    # probe evaluates the REAL lambda rather than re-stating its condition, which
    # is the whole point — a re-statement would pass with the gate deleted. The
    # table lock and finding (a)'s probe are graph-free and live in MMTrickTable.
    redship_add_test(NAME MMTrickGbtGate COMMAND redship --test mm-trick-gbt-gate
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
    # #688 — THE GOLDEN ROWS. Stored digests, so a MOVED world fails.
    #
    # The three rows above and MMPairedAttemptDeterminism all diff two runs of
    # the SAME binary against each other. They detect NONDETERMINISM and nothing
    # else: a change that moves every placement deterministically passes every
    # one of them. These rows compare ONE run against a digest checked into
    # `tests/golden/`, which is the only thing in this tree that can make "the
    # generated world did not change" fail. They do not replace the self-diffs —
    # neither property implies the other — and they run the generation once
    # rather than twice, so the tier cost is one extra generation per row.
    #
    # Re-pinning is deliberate and reviewable: `cmake --build <dir> --target
    # regen-golden-digests` rewrites the files and the DIFF is the review
    # artifact. Policy and the field-by-field reading: docs/determinism-goldens.md.
    #
    # PROFILE COVERAGE. Two OoT profiles, because a single golden would pin one
    # settings profile's fill and say nothing about whether a change is
    # settings-sensitive:
    #   - `default` is the shipped SETTINGS profile (no RSBS_DIAG_CVARS at all),
    #     generated ARCHIVE-FREE. It is NOT the world a player gets, and saying so
    #     here would contradict the archive note 20 lines below: a player runs with
    #     oot.o2r/mm.o2r mounted, which changes the settings string and therefore
    #     the whole fill (#702). It is a world THIS generator produces under the
    #     shipped settings, and a change to the fill moves it the same way it would
    #     move a player's — which is what makes it a usable oracle. When #702 lands,
    #     one golden covers both environments and this distinction goes away;
    #   - `profile-v1` is the pinned "RSBS unified pinned profile v1"
    #     (ShuffleSongs=2) the self-diff rows above use, so the golden and the
    #     self-diff describe the same world and can be read against each other.
    # Both resolve combo direction BOTH at the shipped defaults, so both cover
    # BOTH crossing directions: the reverse table (foreignOoTHash plus its
    # per-slot foreignOoT<n> lines, OoT checks hosting MM items) and the forward
    # table (foreignCount plus its per-slot foreign<n> lines, MM checks hosting
    # OoT items) are in every one of these digests.
    #
    # Timeout is 300, which is the number SeedDeterminism carries too — the
    # clause that used to sit here ("not SeedDeterminism's") described the same
    # literal it was distinguishing itself from. What a golden row does IS the
    # cheaper half of what SeedDeterminism does: one windowed bring-up and one
    # generation, against two of each. 300 is headroom for a loaded hosted runner
    # under llvmpipe rather than a derived figure. Measured on the merged tip (run
    # 35650187916): all three rows together take 10.9 s on the Windows leg and
    # 4.5 s on the Linux leg.
    # ========================================================================
    # ONE TABLE, THREE CONSUMERS: the CTest rows below, and BOTH re-pin targets
    # generated from REDSHIP_GOLDEN_REGEN_TARGETS further down
    # (`regen-golden-digests` and `regen-golden-digests-rom-mounted`, one inner loop
    # over this table each). A golden therefore can never be RE-PINNED under a
    # different profile than the row CHECKS it under. That drift would be silent and
    # green — the worst possible failure for an oracle — and it is the reason this is
    # a table instead of nine hand-written blocks. The guarantee is only worth as
    # much as the fields every consumer actually reads: the regen loop below reads
    # all of 1, 2, 3, 4 and 5, and a field added here must be wired into all three
    # consumers in the same commit. (This header said "TWO CONSUMERS" for one PR
    # after the second re-pin target was added, in the comment whose whole job is to
    # tell the next author how many places a new field must reach.)
    #
    # WHERE THESE ROWS ARE ACTUALLY ENFORCED: on BOTH CI legs and in the operator's
    # local ROM-staged run, all three by `LABEL rando`. Linux runs the `rando` tier
    # under xvfb-run; Windows runs the same label directly (#709 — measured 28/28 on
    # windows-latest, against the old expectation that a hosted runner could not
    # bring up the Fast3dWindow these rows need; before #709 the Windows job
    # selected only these three rows, by `--tests-regex '^Golden'`). The two
    # archive-sensitive rows used to SKIP in a ROM-staged local
    # tree, leaving the merge gate with no golden coverage at all; they now run their
    # dispatch from an archive-free sandbox instead (CheckGoldenDigest.cmake), so the
    # local gate enforces them too — and a sandbox that cannot be built FAILS the row
    # rather than skipping it, so no path is left on which one of these rows reports
    # nothing.
    #
    # If you add a golden row here it gets `LABEL rando` from the loop below and so
    # runs on both CI legs and locally; nothing else selects it. (Until #709 the
    # Windows leg selected golden rows by name, and this loop FATAL_ERRORed on a row
    # not named `Golden...` to keep that selection honest; with the leg running the
    # label, the name carries no enforcement and the check was retired with it.)
    # Full picture: the header of CMake/CheckGoldenDigest.cmake and
    # docs/determinism-goldens.md.
    #
    # THE ARCHIVE SET IS PART OF THE PIN, and field 4 says which goldens depend on
    # it. MEASURED, not assumed: with the ROM-derived oot.o2r mounted,
    # the 2,449 per-area exclude-location options drop out of the settings string
    # Playthrough_Init hashes (3087 per-option lines against 638; the 638 shared
    # lines are byte-equal), the fill is re-seeded differently and the whole OoT
    # world moves. Hosted CI can never have ROM-derived archives, so the goldens
    # pin the archive-free world, and a ROM-staged local run neither compares
    # against it (that would be red about a move that did not happen) nor skips:
    # it hard-links the binary and the PORT archives into
    # <build>/golden-archive-free/<name>/ and runs the dispatch there, because the
    # binary resolves archives from its own directory as well as from the cwd. The
    # port archives are resolved from the build directory AND from the binary's own
    # directory, and a sandbox that ends up with none of them fails rather than
    # pinning a no-archive world. `mods/`, `assets/` and the rest of the build
    # directory do NOT travel into the sandbox — the goldens pin the world a hosted
    # runner reproduces, and the sandbox is not "the build directory minus the ROM
    # archives" (CheckGoldenDigest.cmake, _archive_free_sandbox). The
    # mm-paired-attempt digest is archive-INSENSITIVE (measured: a ROM-staged
    # Windows golden passed unchanged on archive-free Linux CI), so it carries no
    # guard and is enforced everywhere.
    # Fields: <ctest-name>|<golden-name>|<dispatch>|<digest-env-var>|<archive-free-only>|<extra-env>
    set(REDSHIP_GOLDEN_DIGESTS
        "GoldenSeedDigestDefault|seed-digest-default|rando-determinism|RSBS_SEED_DIGEST_OUT|ON|"
        "GoldenSeedDigestProfileV1|seed-digest-profile-v1|rando-determinism|RSBS_SEED_DIGEST_OUT|ON|RSBS_DIAG_CVARS=gRandoSettings.ShuffleSongs=2"
        # The paired MM world's own golden. Its digest carries the ladder rung the
        # world converged through (winningAttempt / mmPairedAttempt), so this row
        # pins not just the world but the DERIVATION that reached it.
        "GoldenPairedAttemptDigest|paired-attempt-digest|mm-paired-attempt|RSBS_ATTEMPT_DIGEST_OUT|OFF|"
        # #681 review: the ARMED reverse draw. The three goldens above run the
        # shipped profile, which arms no give-capability family, so the
        # capability rows and the per-family budget never ran under any pin. The
        # dispatch arms all four families through MM's option CVars itself and
        # stops after the OoT half (the armed MM fill rides its wall-clock abort
        # on this seed). Same seed and OoT settings as seed-digest-default, so the
        # two files differ only in what the capability narrowing changed.
        "GoldenSeedDigestArmedCaps|seed-digest-armed-caps|rando-armed-caps-digest|RSBS_SEED_DIGEST_OUT|ON|"
    )
    set(REDSHIP_GOLDEN_DIR "${CMAKE_SOURCE_DIR}/tests/golden")

    foreach(_golden_spec IN LISTS REDSHIP_GOLDEN_DIGESTS)
        string(REPLACE "|" ";" _golden_fields "${_golden_spec}")
        list(GET _golden_fields 0 _golden_row)
        list(GET _golden_fields 1 _golden_name)
        list(GET _golden_fields 2 _golden_dispatch)
        list(GET _golden_fields 3 _golden_env_var)
        list(GET _golden_fields 4 _golden_archive_free_only)
        list(GET _golden_fields 5 _golden_extra_env)
        set(_golden_env "SDL_AUDIODRIVER=dummy" "RSBS_DISABLE_OTR_INIT=1")
        if(_golden_extra_env)
            list(APPEND _golden_env "${_golden_extra_env}")
        endif()
        redship_add_test(NAME ${_golden_row}
            COMMAND ${CMAKE_COMMAND}
                    -DREDSHIP_EXE=$<TARGET_FILE:redship>
                    -DWORK_DIR=${CMAKE_BINARY_DIR}
                    -DDISPATCH=${_golden_dispatch}
                    -DDIGEST_ENV=${_golden_env_var}
                    -DGOLDEN_DIR=${REDSHIP_GOLDEN_DIR}
                    -DGOLDEN_NAME=${_golden_name}
                    -DARCHIVE_FREE_ONLY=${_golden_archive_free_only}
                    -P ${CMAKE_CURRENT_LIST_DIR}/CheckGoldenDigest.cmake
            LABEL rando
            TIMEOUT 300
            ENVIRONMENT ${_golden_env})
        # NO SKIP PROPERTY, DELIBERATELY. These two rows carried
        # `SKIP_REGULAR_EXPRESSION "RSBS_GOLDEN_SKIP:"` for the fallback where the
        # archive-free sandbox could not be built, which meant the local merge gate
        # was one silent step — a sandbox that fails to build for any reason — from
        # the zero-coverage state this machinery exists to end, with prose asking a
        # human to read the skip reason as the only thing in the way. A sandbox that
        # cannot be built is a broken harness, not a false world move, so
        # CheckGoldenDigest FATAL_ERRORs there instead and names both ways out. Every
        # golden row is now green or red on every gate; a SKIPPED golden row means
        # somebody re-added a skip path.
    endforeach()

    # #661 end to end: the OoTEntrancePin row above proves the pair is not a POOL
    # candidate; this one proves a REAL fill with entrance shuffle on never writes
    # an override that names it. The settings are the strongest configuration that
    # would otherwise swallow the pair (Interior = All, overworld entrances on,
    # mixed pools over interiors + overworld, decoupled), set by the dispatch
    # itself rather than via RSBS_DIAG_CVARS so the profile travels with the lock.
    # It also asserts the table is NOT near-empty, so "no row names the crossing"
    # cannot be true of a generation where the settings failed to take.
    #
    # In the rando tier for the same correctness reason as ForeignPlacementOoT:
    # the assertion reads a fill result, and with no fill there are no override
    # rows at all and the lock would pass vacuously. Timeout is generous because
    # decoupled + mixed pools makes entrance shuffle retry more than stock.
    redship_add_test(NAME RandoEntrancePin COMMAND redship --test rando-entrance-pin
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # #578 part 2: every trick binding this pass authored, as a table of edges
    # each probed with the frozen trick bit off and then on. Tier `rando` for the
    # same reason as MMTrickGbtGate above, stated again because it is the reason
    # somebody would get wrong: these are std::functions inside
    # Rando::Logic::Regions, populated by ShipInit registrars reached only through
    # InitOTRForMMFirstBoot, whose OTRGlobals ctor constructs a Fast3dWindow. The
    # row evaluates the REAL lambdas — a re-statement of each condition would pass
    # with every binding deleted.
    #
    # Timeout 300, not 180: after the per-edge pairs it generates one glitchless
    # world and runs a full reachability crawl once per bound key to assert
    # monotonicity (a binding may only ever widen reach).
    redship_add_test(NAME MMTrickBindings COMMAND redship --test mm-trick-bindings
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # #582: the on-screen creation-progress surface. Default `redship` tier — it
    # installs a COUNTING painter rather than a renderer, so the whole row runs
    # with no window, no ImGui and no archives. The painting half (SohGui's
    # RunGuiOnly pump) genuinely needs a GPU and is verified by playtest; what
    # this row protects is everything that goes wrong silently: a bar that
    # rewinds across the attempt ladder, a channel leg displacing the other, and
    # a terminal edge that never reaches the presenter.
    redship_add_test(NAME GenProgressOverlay COMMAND redship --test gen-progress-overlay)

    # #670: MM mounted NO mod archives in single-exe — its whole mod-mount
    # sequence is in the excluded games/mm/2s2h/BenPort.cpp, so
    # Combo_GetModArchiveCount(GAME_MM) was structurally always 0 and #593's
    # switch-time re-apply loop was a permanent no-op for MM. This row drives
    # MM's REAL glob/mount (MountMMModArchives) against a privately staged
    # mods/ tree and the REAL Combo_EnsureGameArchivesLoaded, and asserts:
    # the shared mods/ tree is partitioned (MM takes mods/mm, OoT the
    # complement, total and disjoint over the path spellings that actually
    # occur); both registries are fed; the mod beats MM's base archive; an
    # OoT-owned path carried BY an MM mod is reclaimed by soh.o2r on the switch
    # to OoT; and the override returns on the switch back to MM.
    #
    # Same SKIP_RETURN_CODE policy as the other archive rows: it needs staged
    # soh.o2r/2ship.o2r as the base archives and as the byte sources for the
    # stand-in mods, and the netplay-relay job re-runs this label archive-less
    # on purpose (#562).
    redship_add_test(NAME MMModsMount COMMAND redship --test mm-mods-mount)
    set_tests_properties(MMModsMount PROPERTIES SKIP_RETURN_CODE 77)

    # #670, the archive-free half of that row. Combo_ModPathIsForGame is the ONE
    # predicate both globs consult, and Combo_ModArchiveExtensionIsValid is the one
    # rule for which files are archives at all; neither touches the disk. This row
    # asserts both — the partition is total and disjoint over the path spellings
    # that actually occur, and the extension set is the same on both sides of the
    # shared tree — with nothing staged, so it also runs in the netplay-relay job
    # that re-runs this label archive-less (#562), where MMModsMount SKIPs. It was
    # part of MMModsMount and therefore skipped there, which is precisely the gap.
    # No SKIP_RETURN_CODE: this row has no reason to skip, ever.
    redship_add_test(NAME MMModsPartition COMMAND redship --test mm-mods-partition)

    # The font-licensing invariant (license follow-up to #578). Three facts that
    # were prose in THIRD_PARTY_NOTICES.md and are now tree state: the ungranted
    # "All rights reserved" font named in that file's "Resolved by removal"
    # section is gone from both custom-asset trees and from every source and
    # build file under games/, src/, rsbs/ and CMake/ — this comment deliberately
    # does not name it, because the row below scans CMake/*.cmake too and would
    # fail on this line; the SIL OFL 1.1 text sits beside both font sets with
    # each shipped font's own name-table copyright line; and 2Ship2Harkinian's
    # CC0-1.0 grant is visible at games/mm/LICENSE. Plus the runtime consequence
    # of removing a SELECTABLE font: SOH::ResolveOverlayFontName maps a stale
    # CVAR_GAME_OVERLAY_FONT (gSettings.OverlayFont in this build) back to a
    # loaded name before Ship::GameOverlay::SetCurrentFont's mFonts[name] can
    # insert a null-valued dead row. Default `redship` tier: a source/asset scan
    # plus one pure function, no window, no archive, no ROM.
    redship_add_test(NAME FontLicense COMMAND redship --test font-license)

    # The combo-logic coordinator (ADR 0010 increment 3, #645). All three rows are
    # the default `redship` tier: they drive the coordinator over two SYNTHETIC
    # STUB ENGINES the test authors, so they need no ROM, no display and no
    # generated world. That is the whole reason the engine surface is a registered
    # vtable instead of fixed extern "C" symbols (src/common/combo_logic.h).
    redship_add_test(NAME ComboLogicEngineSurface COMMAND redship --test combo-logic-engine-surface)
    redship_add_test(NAME ComboLogicFixpoint COMMAND redship --test combo-logic-fixpoint)
    redship_add_test(NAME ComboLogicFill COMMAND redship --test combo-logic-fill)
    # The increment-3 review follow-up (#701): the host-enumeration cap, the
    # failed-bracket ownership rule, and RunFill's table reset. Same fixture, same
    # `redship` tier — but it drives a SYNTHETIC host pool of a few thousand ids,
    # because the quantity under test is the size of a game's check pool and six
    # authored hosts cannot express it.
    redship_add_test(NAME ComboLogicContractEdges COMMAND redship --test combo-logic-contract-edges)

    # The OoT ENGINE behind that coordinator (#645, lane K2a). `rando` tier, and
    # for a correctness reason rather than a convenience one: every fact this row
    # asserts is a function of a FILL RESULT and of the region graph. The reached
    # set comes from ReachabilitySearch over areaTable, the host lists read
    # GetPlacedRandomizerGet(), and goalReached looks for the check holding
    # RG_TRIFORCE — so in a ROM-free process with no generation every count is zero
    # and "the closure did not shrink" passes as 0 == 0. The row therefore runs a
    # REAL headless generation first and asserts STRICT inequalities wherever a
    # constant would otherwise satisfy it.
    #
    # Timeout 300 like the other real-generation rows: one generation, then about a
    # dozen full reachability closures over the OoT graph (each query expands to a
    # fixpoint, and the monotonicity leg runs two more over a deliberately
    # collapsed world).
    redship_add_test(NAME OoTLogicExport COMMAND redship --test oot-logic-export
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # MM's REAL engine behind that surface (#645 increment 3, lane K2b):
    # games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp driven through
    # Combo_Logic_GetEngine(GAME_MM) — which makes the first leg a
    # registrar-elision lock too, since MM publishes the vtable from a file-scope
    # static in a TU nothing else references (#516/#678).
    #
    # Tier `rando`, not `redship`, and the reason is the same one MMTrickGbtGate
    # and MMTrickBindings give: every answer comes from a std::function inside the
    # ShipInit-populated Rando::Logic::Regions, and the row additionally runs
    # CrawlReachableRegions and the REAL Rando::GiveItem path a dozen times. Audit
    # §6.3 lists "can MM's crawl run without a display" as undetermined, so this
    # row is not the place to find out; MMMajoraGoal shows a display-free MM graph
    # row is possible, and moving this one down a tier is a follow-up with a
    # measurement attached, not a tidy-up.
    #
    # Timeout above the 180 its siblings use: the round is a full MM crawl plus a
    # reachable-check evaluation, and the monotonicity leg runs one round per
    # granted item (nine) plus the repeat-grant pair.
    redship_add_test(NAME MMComboLogicEngine COMMAND redship --test mm-combo-logic-engine
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # Increment 3's FIRST TWO WORK ITEMS, which are measurements rather than
    # features (#645; solver-inventory audit §6.3): what one linked round costs
    # against the #582 30 s floor, and whether a single-bag assumed fill over MM's
    # forward-authored graph converges. Both are questions about
    # Combo_Logic_RunRound / Combo_Logic_RunFill driven over BOTH REAL engines, and
    # neither is answerable any other way — which is why this row exists at all.
    #
    # IT ASSERTS NO TIMING. PR #581 §2a's rule is that a wall clock never decides
    # anything here, and a row that went red when this shared build host got busy
    # would be deleted and its measurement lost with it. What it asserts is that
    # every round and fill TERMINATES inside the coordinator's watchdog, that the
    # same coordinator seed reproduces the same placement digest while a different
    # seed does not, that OoT's world placement digest comes back equal, and that
    # the whole unified save buffer comes back byte-identical TO THE STATE MM's
    # SHIPPED PROFILE LEFT IT IN — compared before the teardown's restoring memcpy,
    # not after it. The row's first four commits compared it after, i.e. against the
    # buffer it had just been copied from, which could not fail; the ordering is the
    # assertion and the file header's S4 says so at length. The numbers are printed
    # for the epic.
    #
    # Tier `rando`, for the correctness reason the two rows above give: a ROM-free
    # process has no generated OoT world and no populated Rando::Logic::Regions, so
    # the bag would be empty and every sanity assertion would pass as 0 == 0.
    #
    # TIMEOUT 1800, which is high on purpose and is not a symptom of a slow test.
    # The row runs one real OoT generation, then five assumed FILLS, and a fill
    # runs one reachability round PER BAG ITEM (audit §4.3) where each round is a
    # full OoT ReachabilitySearch plus a full MM CrawlReachableRegions plus a
    # whole-SaveContext memcpy. The default bag is deliberately a stride SAMPLE of
    # the real union bag so that the row fits a CI budget at all; the four
    # RSBS_COMBO_MEASURE_* variables open it up to the whole bag for the local run
    # whose numbers the epic quotes. If this row ever needs to be cheap, shrink the
    # sample through those variables — do not shrink the timeout.
    redship_add_test(NAME ComboLogicMeasure COMMAND redship --test combo-logic-measure
        LABEL rando
        TIMEOUT 1800
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

    # The give probe (#645, lane K3) is a DIAGNOSTIC and gets no row on purpose: its
    # intended outcome on a bad id is an access violation, so a green/red verdict
    # would be a verdict on whether MM's item table currently contains one — not a
    # property worth pinning, and not one a shared build host should abort over.
    #
    # It exists as its own dispatch rather than as an env mode of the row above
    # because as an env mode it was a hole in that row: `RSBS_COMBO_MEASURE_PROBE`
    # made combo-logic-measure return TEST_PASS before the generation and before
    # every assertion but S1, and CTest does not scrub the inherited environment, so
    # a developer shell or a CI image that exported it got a green row that measured
    # nothing. Two dispatch names cannot collide that way.
    # The armed-capability golden's dispatch has no plain --test row on purpose:
    # the GoldenSeedDigestArmedCaps row in REDSHIP_GOLDEN_DIGESTS drives it
    # through CheckGoldenDigest.cmake, and that row is a cmake -P wrapper, which
    # the manifest records as a meta row rather than as this dispatch's owner.
    redship_test_exempt(rando-armed-caps-digest
        "Driven by the GoldenSeedDigestArmedCaps golden row (cmake -P CheckGoldenDigest.cmake), which the manifest \
records as a meta row. A plain --test row would only regenerate the same world without comparing it (#681).")

    redship_test_exempt(combo-logic-give-probe
        "Diagnostic, not a lock: walks MM's giveable vanilla ids through Rando::GiveItem headlessly so an id whose \
give dereferences a NULL MM_gPlayState/gRegEditor names itself on stderr. Its intended outcome on a bad id is a \
process abort, which is how the RI_TINGLE_MAP_* set and the gRegEditor stand-in were derived (#645). Run by hand: \
redship --test combo-logic-give-probe, RSBS_COMBO_PROBE_FROM=<n> to resume past a known fault.")

    # The single-owner item classification table (ADR 0010 answer O8; #645 lane K5).
    # Default `redship` tier: it needs no generation, because what it locks is a
    # property of the two item TABLES — OoT's itemTable, which a bridge brings up
    # display- and ROM-free, and MM's static Rando::StaticData::Items — and of the
    # two registered sources that classify them. Anti-vacuity is asserted in the row
    # (at least 200 fill items per game and every class present in each).
    redship_add_test(NAME SharedItemClass COMMAND redship --test shared-item-class)
    # The multiplicity ruling (2026-09-26; combo_logic.h ABI 3, #645). The bag
    # model over stub engines is ROM-free and display-free, so it is `redship`
    # tier: the coordinator's one-call-per-copy shape, and the surplus / filler /
    # exact-fit shapes of the bag, with the drop rule's determinism.
    redship_add_test(NAME ComboLogicBagModel COMMAND redship --test combo-logic-bag-model)
    # The same ruling over BOTH REAL engines: OoT's progressive top-tier and
    # counter clamps (and the wraps they prevent, observed with the clamp off),
    # order independence with its red half, and MM's counter maxima and per-host
    # harvest. One OoT generation and a handful of rounds, so the real-generation
    # rows' 300 s.
    redship_add_test(NAME ComboLogicMultiplicity COMMAND redship --test combo-logic-multiplicity
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")
    # #705: loose (unpacked) asset mods for both games. Nothing mounted a directory
    # as an archive, although libultraship's FolderArchive does exactly that for an
    # extension-less AddArchive path; each game now mounts `<mods>/loose` (OoT) or
    # `<mods>/mm/loose` (MM) after its packed mods through one shared helper.
    # LooseModsDiscovery pins which folders count, over a staged directory tree, with
    # nothing else staged — it never skips. LooseModsMount drives both ports' real
    # mod paths and the real switch-time re-apply over a staged tree with a loose
    # file under each partition and asserts, by the bytes a load returns, that each
    # overrides its game's base archive and packed mod, is registered only to its own
    # game, and is shadowed on arrival in the other game on every path that game's
    # base archives ship (a path they do not ship stays resolvable, as for a packed
    # mod). It SKIPs (77) when soh.o2r/2ship.o2r is unstaged, the #670 policy.
    redship_add_test(NAME LooseModsDiscovery COMMAND redship --test loose-mods-discovery)
    redship_add_test(NAME LooseModsMount COMMAND redship --test loose-mods-mount)
    set_tests_properties(LooseModsMount PROPERTIES SKIP_RETURN_CODE 77)
    # ADR 0010 answer O6's CI GROW-CHECK (#645, #500): the third of O6's three
    # mechanisms (the review rule and the static probe,
    # .github/scripts/check-monotonicity-negations.py, are the other two). Over
    # both REAL engines, from the shipped profile's starting state: the bag
    # granted one copy at a time in four orders, tricks off and every trick on,
    # asserting the reached check and region sets never shrink; order
    # independence; tricks only add; coordinator prefixes never lose a host; and
    # a planted negated edge observed red on both engines. `rando` tier for the
    # same reason as its siblings: without a generated world every set is empty.
    redship_add_test(NAME ComboLogicMonotonicity COMMAND redship --test combo-logic-monotonicity
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")
    # ADR 0010 answer O10: ONE shared triforce piece count across both worlds.
    # ComboTriforceHunt is display-free and ROM-free: the frozen record's rule and
    # its refusal, the MONOTONIC discipline pin (a lower harvest after a full
    # apply keeps the count), collect k in OoT and m in MM through both games'
    # REAL shims and read k+m in both, the win decision both give arms call, and
    # the coordinator's triforce-hunt predicate over stub engines.
    # RandoTriforceHuntWin drives both games' REAL piece-give arms (OoT's needs a
    # generated context, MM's dispatches GameInteractor hooks), so it is `rando`.
    redship_add_test(NAME ComboTriforceHunt COMMAND redship --test combo-triforce-hunt)
    redship_add_test(NAME RandoTriforceHuntWin COMMAND redship --test rando-triforce-hunt-win
        LABEL rando
        TIMEOUT 300
        ENVIRONMENT "SDL_AUDIODRIVER=dummy;RSBS_DISABLE_OTR_INIT=1")

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

    # ========================================================================
    # #688 — THE ONE DOCUMENTED RE-PIN COMMAND.
    #
    #     cmake --build <build-dir> --target regen-golden-digests
    #
    # Regenerates every golden in `tests/golden/` from THIS binary, under exactly
    # the dispatch, the environment AND the archive-sensitivity its CTest row
    # checks it with — all three come from the REDSHIP_GOLDEN_DIGESTS table above,
    # so the row and the re-pin cannot disagree about any of them. The resulting
    # `git diff` of those files IS the review artifact, and a re-pin commit must
    # say which fields moved and why — docs/determinism-goldens.md.
    #
    # NOT part of `all`, and deliberately not a CTest row: re-pinning is an act
    # of authorship, and a target that regenerated goldens as a side effect of a
    # build would turn the oracle back into the no-op #688 was filed about.
    #
    # RUN IT WITH THE PORT ARCHIVES ONLY (soh.o2r / 2ship.o2r / redship.o2r) and
    # a GL-capable display; on a headless Linux box, under xvfb-run. Move oot.o2r
    # and mm.o2r out of the build directory first: they change the OoT settings
    # string and therefore the whole fill (see the table above), and a golden
    # re-pinned with them mounted pins a world CI can never reproduce, so every
    # archive-free run would then go red.
    #
    # THAT IS NOW ENFORCED, NOT REQUESTED. The archive-sensitivity field is passed
    # through below, and CheckGoldenDigest REFUSES a REGEN of an archive-sensitive
    # golden while oot.o2r/mm.o2r are in WORK_DIR (the error names both paths). The
    # previous version of this block asked a human to remember instead, which was
    # the weakest possible defense for the most damaging mistake this target can
    # make: WORKING_DIRECTORY below is ${CMAKE_BINARY_DIR}, i.e. exactly the
    # ROM-staged tree the operator builds in, and the first symptom of a bad re-pin
    # is a red Linux leg on this PR and on every PR after it.
    #
    # TWO TARGETS, BECAUSE AN OVERRIDE NOBODY CAN REACH IS NOT AN OVERRIDE. The
    # refusal documents `-DALLOW_ROM_ARCHIVE_REGEN=ON` as the deliberate way out,
    # and for one PR that sentence stood in four places while the command line
    # generated here did not carry the variable at all: `cmake -P` has no cache and
    # imports no environment, so `cmake --build <dir> --target regen-golden-digests`
    # could not set it by any spelling. It is forwarded now, explicitly on both
    # targets — OFF on the normal one, ON on `regen-golden-digests-rom-mounted` —
    # and CheckGoldenDigest FATAL_ERRORs on a REGEN caller that leaves it undefined,
    # so the next caller that forgets finds out immediately. A target rather than a
    # cache entry: the override is then per-invocation and named at the point of
    # use, instead of a variable somebody sets once and forgets while every later
    # re-pin in that tree silently pins the ROM-mounted world.
    # ========================================================================
    set(REDSHIP_GOLDEN_REGEN_TARGETS
        "regen-golden-digests|OFF|Re-pinning the golden determinism digests in tests/golden/ (#688)"
        "regen-golden-digests-rom-mounted|ON|Re-pinning the golden determinism digests WITH the ROM archives mounted: pins a world hosted CI cannot reproduce (#688)"
    )
    foreach(_regen_spec IN LISTS REDSHIP_GOLDEN_REGEN_TARGETS)
        string(REPLACE "|" ";" _regen_fields "${_regen_spec}")
        list(GET _regen_fields 0 _regen_target)
        list(GET _regen_fields 1 _regen_allow_rom)
        list(GET _regen_fields 2 _regen_comment)
        add_custom_target(${_regen_target} COMMENT "${_regen_comment}")
        foreach(_golden_spec IN LISTS REDSHIP_GOLDEN_DIGESTS)
            string(REPLACE "|" ";" _golden_fields "${_golden_spec}")
            list(GET _golden_fields 1 _golden_name)
            list(GET _golden_fields 2 _golden_dispatch)
            list(GET _golden_fields 3 _golden_env_var)
            # Field 4 is read HERE too, not only by the CTest row above. Reading it
            # in one consumer and not the other is what let the regen target behave
            # identically for an archive-sensitive and an archive-insensitive golden
            # while this block claimed the table made that impossible.
            list(GET _golden_fields 4 _golden_archive_free_only)
            list(GET _golden_fields 5 _golden_extra_env)
            set(_golden_env "SDL_AUDIODRIVER=dummy" "RSBS_DISABLE_OTR_INIT=1")
            if(_golden_extra_env)
                list(APPEND _golden_env "${_golden_extra_env}")
            endif()
            add_custom_command(TARGET ${_regen_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env ${_golden_env}
                        ${CMAKE_COMMAND}
                        -DREDSHIP_EXE=$<TARGET_FILE:redship>
                        -DWORK_DIR=${CMAKE_BINARY_DIR}
                        -DDISPATCH=${_golden_dispatch}
                        -DDIGEST_ENV=${_golden_env_var}
                        -DGOLDEN_DIR=${REDSHIP_GOLDEN_DIR}
                        -DGOLDEN_NAME=${_golden_name}
                        -DARCHIVE_FREE_ONLY=${_golden_archive_free_only}
                        -DALLOW_ROM_ARCHIVE_REGEN=${_regen_allow_rom}
                        -DREGEN=ON
                        -P ${CMAKE_CURRENT_LIST_DIR}/CheckGoldenDigest.cmake
                # Explicit, because the binary resolves oot.o2r/mm.o2r/soh.o2r
                # relative to its working directory and a re-pin against a
                # half-staged directory would pin a world nobody can reproduce.
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                VERBATIM)
        endforeach()
        add_dependencies(${_regen_target} redship)
    endforeach()

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
