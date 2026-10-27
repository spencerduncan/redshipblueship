cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(2ship LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")

# This repo always builds shared libraries for the unified combo launcher
# For standalone executables, use upstream 2Ship repo instead

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
endif()

set (BUILD_UTILS OFF CACHE STRING "no utilities")
set (BUILD_SHARED_LIBS OFF CACHE STRING "install/link shared instead of static libs")

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if(NOT CMAKE_VS_PLATFORM_NAME)
		set(CMAKE_VS_PLATFORM_NAME "x64")
	endif()
	message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

	if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"
		OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
		message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
	endif()
endif()


FetchContent_Declare(
    dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
	GIT_TAG da35f9d6c7374a95353fd1df1d394d44ab66cf01
)
FetchContent_MakeAvailable(dr_libs)

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Sub-projects
################################################################################
if (NOT TARGET libultraship)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../libultraship ${CMAKE_BINARY_DIR}/libultraship)
endif()

if (NOT TARGET ZAPDLib)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ZAPDTR/ZAPD ${CMAKE_BINARY_DIR}/ZAPD)
endif()

set(PROJECT_NAME 2ship)

################################################################################
# Sources
################################################################################
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/build.c.in ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/build.c @ONLY)
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/windows/properties.h.in ${CMAKE_CURRENT_SOURCE_DIR}/windows/properties.h @ONLY)

#set(Header_Files "resource.h")
source_group("headers" FILES ${Header_Files})

# include {{{
file(GLOB Header_Files__include "include/*.h" "include/*.inc")
list(APPEND Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/libc/stdarg.h)
list(REMOVE_ITEM Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/bgm.h)
list(REMOVE_ITEM Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/math_n64.h)
list(REMOVE_ITEM Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/stdbool_n64.h)
list(REMOVE_ITEM Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/stddef_n64.h)
list(REMOVE_ITEM Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/stdlib_n64.h)
list(REMOVE_ITEM Header_Files__include ${CMAKE_CURRENT_SOURCE_DIR}/include/ultra64.h)
source_group("include" FILES ${Header_Files__include})
# }}}

# CONFIGURE_DEPENDS (#379): without it an existing build directory never
# re-globs, so pulling a branch that adds a 2s2h/ TU (the ROM-free *_test.cpp
# locks all land here) links the stale file list and fails with an
# unresolved-symbol error that names the symbol, not the missing TU. This glob
# is recursive over all of 2s2h/, so a change anywhere under it forces the
# reconfigure that also refreshes the Enhancements/Rando globs below.
file(GLOB_RECURSE ship__ RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS "2s2h/*.c" "2s2h/*.cpp" "2s2h/*.h" "2s2h/*.hpp")
# Collect Enhancements separately (will be 2ship_enh lib)
file(GLOB_RECURSE ship__Enhancements RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "2s2h/Enhancements/*.c" "2s2h/Enhancements/*.cpp" "2s2h/Enhancements/*.h" "2s2h/Enhancements/*.hpp")
# Collect Rando separately (will be 2ship_rando lib) - it's 2.1MB source and helps avoid 4GB limit
file(GLOB_RECURSE ship__Rando RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "2s2h/Rando/*.c" "2s2h/Rando/*.cpp" "2s2h/Rando/*.h" "2s2h/Rando/*.hpp")
# Lane C0 (#392): split the BenGui-coupled UI TUs out of 2ship_rando so the
# core randomizer can be WHOLE_ARCHIVE'd without dragging MM's menu surface
# (BenGui::mBenMenu / BenMenu::Add* / the check-tracker windows — the four
# BenGui symbols in PR #422's measured link gap) into the single-exe link.
# 2ship_rando_ui stays compiled (bit-rot protection) but links with plain
# archive semantics: nothing references its symbols in single-exe builds
# (Rando::Init's CheckTracker calls are compiled out there), so it is elided
# exactly like the rest of MM's menu until that surface is ported (Lane B's
# surface note on #392).
#
# BenGui/UIWidgets.cpp rides along here (#383): MM's UIWidgets lives in
# namespace S2H in single-exe builds so its option structs stop sharing
# mangled names with OoT's differently-laid-out UIWidgets, and this TU is the
# matching S2H implementation. Plain archive semantics keep it elided until
# an MM menu TU actually references it; before this, a pulled-in MM menu TU
# silently bound OoT's UIWidgets implementations against MM's 32-byte
# WidgetOptions layout (the FlagTable incident class) — now that same mistake
# is a loud unresolved-external link error at worst, and resolves correctly
# once this TU is pulled.
#
# BenGui/Menu.cpp is the same story for Ship::Menu itself (#446): MM's
# Ship::Menu and MenuTypes family live in namespace S2H in single-exe builds
# (OoT's Ship::Menu has a divergent layout — std::map vs unordered_map
# backends map, std::function vs raw-pointer WidgetInfo callbacks — under
# identical include guards), and this TU is the matching S2H implementation.
# Same plain-archive semantics: elided today, resolves layout-correct once a
# ported MM menu TU references it, loud unresolved external if one is pulled
# without it.
set(ship__Rando_ui
    "2s2h/Rando/Menu.cpp"
    "2s2h/Rando/CheckTracker/CheckTracker.cpp"
    "2s2h/BenGui/UIWidgets.cpp"
    "2s2h/BenGui/Menu.cpp"
)
list(FILTER ship__Rando EXCLUDE REGEX "2s2h/Rando/Menu\\.cpp$")
list(FILTER ship__Rando EXCLUDE REGEX "2s2h/Rando/CheckTracker/CheckTracker\\.cpp$")
# Remove Enhancements and Rando from ship__ (will be 2ship_port lib)
list(FILTER ship__ EXCLUDE REGEX "2s2h/Enhancements/.*")
list(FILTER ship__ EXCLUDE REGEX "2s2h/Rando/.*")

# Create source groups that match the real file directory paths
foreach(source IN LISTS ship__ ship__Enhancements)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_adjusted "${source_path}")
    source_group("${source_path_adjusted}" FILES "${source}")
endforeach()

file(GLOB_RECURSE libultra_headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "include/PR/*.h"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_source_files_properties(2s2h/BenPort.cpp PROPERTIES COMPILE_FLAGS "/utf-8")
endif()

# Note: GameExports.cpp was removed - single-exe uses GameExports_SingleExe.cpp instead

list(FILTER ship__ EXCLUDE REGEX "2s2h/Extractor/*")

if(NOT CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch|CafeOS")
    file(GLOB_RECURSE ship__Extractor RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "2s2h/Extractor/*.c"
        "2s2h/Extractor/*.cpp"
        "2s2h/Extractor/*.h"
        "2s2h/Extractor/*.hpp"
    )
else()
    file(GLOB_RECURSE ship__Extractor RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "2s2h/Extractor/*.h"
        "2s2h/Extractor/*.hpp"
    )
endif()

# src (decomp) {{{
file(GLOB_RECURSE src__ RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.c" "src/*.h")

# Skip Resource.rc in single-exe mode to avoid duplicate VERSION resource (LNK1123)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT SINGLE_EXECUTABLE_BUILD)
    list(APPEND src__ ${CMAKE_CURRENT_SOURCE_DIR}/windows/Resource.rc)
endif()

list(FILTER src__ EXCLUDE REGEX "src/dmadata/*")
list(FILTER src__ EXCLUDE REGEX "src/elf_message/*")
list(FILTER src__ EXCLUDE REGEX "src/libultra/io/*")
list(FILTER src__ EXCLUDE REGEX "src/libultra/libc/*")
list(FILTER src__ EXCLUDE REGEX "src/libultra/os/*")
list(FILTER src__ EXCLUDE REGEX "src/libultra/rmon/*")
list(FILTER src__ EXCLUDE REGEX "src/libultra/*")
#list(APPEND src__ "src/libultra/libc/sprintf.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/cosf.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/lookat.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/lookathil.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/perspective.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/position.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/sinf.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/sinf.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/sqrtf.c")
#list(REMOVE_ITEM src__ "src/libultra/gu/us2dex.c")

source_group("src" REGULAR_EXPRESSION "src/*")
source_group("src\\boot" REGULAR_EXPRESSION "src/boot/*")
source_group("src\\buffers" REGULAR_EXPRESSION "src/buffers/*")
source_group("src\\code" REGULAR_EXPRESSION "src/code/*")
#source_group("src\\libultra" REGULAR_EXPRESSION "src/libultra/*")
source_group("src\\overlays\\actors" REGULAR_EXPRESSION "src/overlays/actors/*")
source_group("src\\overlays\\effects" REGULAR_EXPRESSION "src/overlays/effects/*")
source_group("src\\overlays\\fbdemos" REGULAR_EXPRESSION "src/overlays/fbdemos/*")
source_group("src\\overlays\\gamestates" REGULAR_EXPRESSION  "src/overlays/gamestates/*")
source_group("src\\overlays\\misc" REGULAR_EXPRESSION  "src/overlays/misc/*")
# }}}

set(ALL_FILES
    ${Header_Files}
    ${Header_Files__include}
    ${ship__}
	${libultra_headers}
    ${ship__Extractor}
    ${src__}
)

# Note: GameExports.cpp removed - single-exe uses GameExports_SingleExe.cpp

################################################################################
# Target - OBJECT library for single-exe, SHARED for dynamic loading
################################################################################
if(SINGLE_EXECUTABLE_BUILD)
    # Aggressively exclude MM's enhancement layer - will use OoT's versions
    # These can be re-enabled/customized later
    # NOTE: Filters applied to ship__ and src__ directly (not ALL_FILES)
    # because OBJECT libs are created from these lists, not ALL_FILES.

    # Main port glue (OTRGlobals equivalent)
    list(FILTER ship__ EXCLUDE REGEX "2s2h/BenPort\\.cpp$")

    # All GUI components. Exceptions: BenGui/UIWidgets.cpp (#383) and
    # BenGui/Menu.cpp (#446) are compiled into 2ship_rando_ui (see the
    # ship__Rando_ui block above) as the S2H-namespaced implementations behind
    # MM's UIWidgets/Menu headers — the filter here only shapes 2ship_port, so
    # no duplication results.
    list(FILTER ship__ EXCLUDE REGEX "2s2h/BenGui/.*\\.cpp$")

    # All developer tools
    list(FILTER ship__ EXCLUDE REGEX "2s2h/DeveloperTools/.*\\.cpp$")

    # All enhancements - entire directory
    list(FILTER ship__ EXCLUDE REGEX "2s2h/Enhancements/.*\\.cpp$")

    # MM's AudioEditor UI depends on BenGui/BenMenu (excluded below) and
    # defined the same mangled + extern "C" symbols as SoH's
    # soh/Enhancements/audio/AudioEditor.cpp, so it is dropped from 2ship_enh
    # in single-exe builds. Its extern "C" entry points
    # (MM_AudioEditor_Get{Replacement,Original}Seq after the
    # include/mm_audio_prefix.h rename) are identity-stubbed in
    # 2s2h/GameExports_SingleExe.cpp; AudioCollection.cpp stays compiled as
    # MM's real (S2H-namespaced) collection.
    list(FILTER ship__Enhancements EXCLUDE REGEX "2s2h/Enhancements/Audio/AudioEditor\\.cpp$")

    # #678 - the two Enhancements TUs that are the ONLY callers of
    # Rando::ClockShuffle's public API, carved into their own archive so they
    # can be WHOLE_ARCHIVE'd without flipping 2ship_enh whole (#516).
    #
    # THE FAILURE. Rando::ClockShuffle::IsTimeOwnedForClockShuffle,
    # GetTimeDescriptionForMessage and SetTimeToHalfDayStart have their only
    # callers in BetterSongOfDoubleTime.cpp and SkipSoTCutscenes.cpp. Both
    # register purely through file-scope RegisterShipInitFunc objects and
    # export nothing anything else references, so plain archive semantics
    # dropped both members outright: neither object appears in the operator's
    # redship.map. With them went two whole enhancements - toggling
    # gEnhancements.Songs.BetterSongOfDoubleTime or ...SkipSoTCutscenes did
    # nothing at all - and with those, the RO_CLOCK_SHUFFLE ownership checks
    # they carry: Song of Double Time's free-form time selector refusing an
    # unowned half-day, and a Song of Time reset landing on the earliest OWNED
    # half-day instead of the vanilla dawn. This is the #516 elided-registrar
    # class exactly.
    #
    # WHY A CARVE-OUT AND NOT A HAND-ADDED REFERENCE. PR #651 measured that
    # question on soh_port and answered it: whether a plain-archive member is
    # pulled can turn on which TU the linker happens to take a COMDAT
    # instantiation from, so an incidental reference "is not a link contract".
    # This tree has a live example - Songs/PauseOwlWarp.cpp survives only
    # because z_kaleido_*.c calls its one extern "C" accessor. WHOLE_ARCHIVE is
    # the contract; the archive boundary is what keeps it targeted.
    #
    # WHY NOT ALL OF 2ship_enh (#516). Flipping the whole archive arms
    # every enhancement PR #673 wired a dispatcher for, in one step, and
    # .github/scripts/check-registrar-elision.sh records why that answer has
    # twice been "no": most 2ship_enh members must legitimately stay dead in
    # single-exe (BenGui's second menu surface, DebugConsole_Init's ODR twin,
    # OTRAudio_Init's second SDL audio thread, ...). Two named TUs, with their
    # play-state guards landed in the same change, is the reviewable unit.
    #
    # WHY NOT THE WHOLE Songs DIRECTORY. The other six TUs under it are not
    # part of #678 and arming them is blast radius with nobody to pay for it.
    #
    # Duplicate-symbol exposure was measured, not assumed: no name these two
    # TUs define at namespace scope (RegisterBetterSongOfDoubleTime,
    # RegisterSkipSoTCutscenes, OnPlayerUpdate(Actor*), UpdateDayTexture,
    # DrawTexRectI4, DrawTexRectIA8, DrawIndicators,
    # UpdateStickDirectionPromptAnim, sArrowAnimColor) has a twin anywhere in
    # games/oot or games/mm. soh_enh's ?OnPlayerUpdate@@YAXXZ takes no
    # arguments, so it does not mangle to MM's ?OnPlayerUpdate@@YAXPEAUActor@@@Z.
    set(ship__Enh_clockshuffle
        "2s2h/Enhancements/Songs/BetterSongOfDoubleTime.cpp"
        "2s2h/Enhancements/Songs/SkipSoTCutscenes.cpp"
    )
    list(FILTER ship__Enhancements EXCLUDE REGEX "2s2h/Enhancements/Songs/BetterSongOfDoubleTime\\.cpp$")
    list(FILTER ship__Enhancements EXCLUDE REGEX "2s2h/Enhancements/Songs/SkipSoTCutscenes\\.cpp$")

    # #1071 - 2ship_enh_registrars: enhancement TUs whose ONLY anchor in the
    # link is a file-scope RegisterShipInitFunc, carved out of 2ship_enh so
    # they can be WHOLE_ARCHIVE'd one deliberate file at a time.
    #
    # THE CLASS (#516's elided provider). A TU that registers purely through a
    # static RegisterShipInitFunc object and exports nothing anything else
    # references is dropped outright by plain archive semantics: its object
    # never reaches redship.map, its static initializer never runs, and every
    # hook it would have registered stays empty. When that hook is the rando
    # hand-off for a check, the check silently plays vanilla. #678 hit it with
    # the two ClockShuffle Songs TUs above (they keep their own archive, which
    # predates this one); #1071 hit it with the nine song teachers below.
    #
    # WHY THIS ARCHIVE AND NOT THE WHOLE OF 2ship_enh: the reasons the #678
    # block above gives, unchanged. Each member of this list is a decision to
    # ARM that TU in the single exe, and check-registrar-elision.sh audits the
    # archive as `required`, so every member must survive the link.
    #
    # TO ADD A FILE (later lanes of the same class): append it to the list
    # below, ONE FILE PER LINE, under a `# #<issue>` comment of its own, after
    # the last group, so two lanes' appends do not touch the same lines. In the
    # same change: land the play-state guards PR #673's criterion asks for in
    # every hook the TU registers, confirm each of those hooks is DISPATCHED in
    # the single exe (the #512/#517 registered-but-not-dispatched class), check
    # the TU's other definitions for a twin anywhere in games/oot or games/mm,
    # and add a ROM-free row that attributes the TU by registry content without
    # naming any symbol it defines (a reference from a test would un-elide it
    # by itself). The REMOVE_ITEM below takes every listed file out of
    # ship__Enhancements, so there is nothing else to edit here.
    set(ship__Enh_registrars
        # #1071: the song-teacher hooks. Forced on under IS_RANDO because they
        # are the rando hand-off for the song checks (the Sonata hook is the
        # only caller of VB_GIVE_ITEM_FROM_MNK, which
        # Rando/ActorBehavior/EnMnk.cpp waits on). SkipLearningSongOfTime.cpp
        # has no IS_RANDO gate and is not part of #1071; it stays in 2ship_enh.
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningElegyOfEmptiness.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningEponasSong.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningGoronLullaby.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningGoronLullabyIntro.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningNewWaveBossaNova.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSonataOfAwakening.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSongOfHealing.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSongOfSoaring.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSongOfStorms.cpp"
        # #1005-#1009: the Cheats rows the SoH menu draws (kSharedIntentKeys / kConvergedKeys):
        # Infinite Health/Magic/Money(/Consumables), Climb Everything, Hookshot Everything,
        # Moon Jump on L and Easy Frame Advancing. Each is CVar-gated, so a TU only arms when
        # its row is on; the play-state guards landed with the carve-out.
        "2s2h/Enhancements/Cheats/Infinite.cpp"
        "2s2h/Enhancements/Cheats/ClimbAnywhere.cpp"
        "2s2h/Enhancements/Cheats/HookshotAnywhere.cpp"
        "2s2h/Enhancements/Cheats/MoonJump.cpp"
        "2s2h/Enhancements/Cheats/EasyFrameAdvance.cpp"
        # #1018-#1022: the Skips & Speed-ups rows the SoH menu draws. Every file keys on the converged
        # gEnhancements.TimeSavers.* key SoH writes (kConvergedKeys), registers only through a file-scope
        # RegisterShipInitFunc, and rides a VB/actor hook the single exe dispatches (z_demo.c, z_eventmgr.c,
        # z_camera.c, the actor overlays). Play-state guards landed with the carve-out. The song-teacher
        # SkipLearning* files above are #1071's; SkipLearningSongOfTime.cpp (excluded by the comment there) is
        # carried here with the rest of the Story set, and SkipGiantsChamber.cpp stays linked through its own path.
        # #1018
        "2s2h/Enhancements/Cutscenes/HideTitleCards.cpp"
        # #1019
        "2s2h/Enhancements/Cutscenes/SkipIntroSequence.cpp"
        # #1020
        "2s2h/Enhancements/Cutscenes/SkipEntranceCutscenes.cpp"
        # #1123 (Skip Boss Introductions, keyed on SoH's gEnhancements.TimeSavers.SkipCutscene.BossIntro via
        # kConvergedKeys). Hooks, all dispatched: VB_START_CUTSCENE (z_eventmgr.c), VB_SET_CAMERA_AT_EYE and
        # VB_SET_CAMERA_FOV (z_play.c), VB_PLAYER_CUTSCENE_ACTION (z_actor.c) via MM_GameHooks_ExecuteVBShould;
        # ShouldActorInit / ShouldActorUpdate / OnActorInit / OnActorUpdate via MM_GameHooks_Execute* (z_actor.c).
        # Play-state guards in every body that reads MM_gPlayState; its three registrars have no twin in games/oot.
        "2s2h/Enhancements/Cutscenes/SkipEnemyCutscenes.cpp"
        # #1021
        "2s2h/Enhancements/Cutscenes/SkipOnePointCutscenes.cpp"
        # #1022: the Story set (the remaining StoryCutscenes TUs, HealingMikauAudio included)
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/HealingMikauAudio.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipBiggoronSnowheadLullaby.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipBombBagTheftCutscene.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipClockTowerOpen.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipClockTowerSkullKidEncounter.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipDefeatCaptainSequence.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipFirstSongOfTimeFlashbacks.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipHealingDarmani.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipHealingMikau.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipIkanaCurseCutscenes.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSongOfTime.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipMilkRunCutscenes.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipMoonCrash.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipPrincessDelivery.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipRaisingWoodfall.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipStoppingMoonCutscene.cpp"
        "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipWakingAndRidingTurtle.cpp"
        # W6-TIMESAVERS (#1236, #1240): the dialogue, time-saver and pause-save rows SoH's Enhancements pages host
        # (SohMenuHostedMmTimesavers.cpp), each keyed on the MM key its row writes. Hooks, all dispatched:
        # OnActorUpdate / OnActorInit / OnFlagSet / OnOpenText via MM_GameHooks_Execute* (z_actor.c, z_message.c);
        # VB_SETUP_TRANSITION (z_play.c), VB_EXEC_MSG_EVENT (z_msgevent.c), VB_BALLAD_PLAYED_FORM (z_en_toto.c),
        # VB_DISCARD_EXCESS_RUPEES (z_parameter.c), VB_SAVE_ON_B_BUTTON_IN_PAUSE_MENU (z_kaleido_scope_NES.c) and
        # VB_PLAY_SCENE_SEQUENCE (code_8019AF00.c) via MM_GameHooks_ExecuteVBShould. Play-state guards in every body
        # that reads MM_gPlayState. BgmReplay.cpp is required: it keeps MM's main BGM player valid under Faster Scene
        # Transitions and Pause Menu Save (and, as in 2Ship, under the shared Debug Mode key). No twin in games/oot.
        "2s2h/Enhancements/Dialogue/FastBankSelection.cpp"
        "2s2h/Enhancements/Dialogue/AutoBombersCode.cpp"
        "2s2h/Enhancements/Timesavers/SwampBoatSkip.cpp"
        "2s2h/Enhancements/Timesavers/GalleryTwofer.cpp"
        "2s2h/Enhancements/Timesavers/MarineLabHP.cpp"
        "2s2h/Enhancements/Timesavers/DampeDiggingSkip.cpp"
        "2s2h/Enhancements/Timesavers/FasterSceneTransitions.cpp"
        "2s2h/Enhancements/Minigames/SkipPowderKegCertification.cpp"
        "2s2h/Enhancements/Minigames/SkipBalladOfWindfish.cpp"
        "2s2h/Enhancements/Timesavers/AutoBankDeposit.cpp"
        "2s2h/Enhancements/Saving/PauseSave.cpp"
        "2s2h/Enhancements/Fixes/BgmReplay.cpp"
        # #1010-#1017: eight enhancement rows the SoH menu draws. Each keys on the converged
        # gEnhancements.* key SoH's row writes (kConvergedKeys; DpadEquips for ItemUnequip's second
        # half), registers only through a file-scope RegisterShipInitFunc, and rides a hook the single
        # exe dispatches (VB_* via MM_GameHooks_ExecuteVBShould; OnInterfaceDrawStart, OnPlayerPostLimbDraw,
        # OnActorUpdate, OnActorInit, OnGameStateMainStart via MM_GameHooks_Execute*). Play-state guards
        # landed with the carve-out. Duplicate-symbol exposure measured: RegisterItemUnequip and
        # RegisterPauseBufferInputs exist in games/oot too, and MM's copies are MM_-prefixed in the source;
        # Interface_DrawEnemyHealthBar is a C symbol in OoT's z_parameter.c and a C++ one (Attention*) here.
        # #1010
        "2s2h/Enhancements/Timesavers/FastChests.cpp"
        # #1011
        "2s2h/Enhancements/Graphics/EnemyHealthBars.cpp"
        # #1012
        "2s2h/Enhancements/Graphics/BowReticle.cpp"
        # #1013
        "2s2h/Enhancements/Graphics/DisableBlackBars.cpp"
        # #1014
        "2s2h/Enhancements/Player/InstantPutaway.cpp"
        # #1015
        "2s2h/Enhancements/Equipment/ItemUnequip.cpp"
        # #1016
        "2s2h/Enhancements/Restorations/PauseBufferInputs.cpp"
        # #1017
        "2s2h/Enhancements/DifficultyOptions/HyperEnemies.cpp"
        # #1024-#1026: the three Audio Editor rows the SoH menu draws (kSharedIntentKeys: gAudioEditor.LowHpAlarm,
        # .EnemyBGMDisable, .SeqNameNotification and its Duration). Each registers only through a file-scope
        # RegisterShipInitFunc, and rides a hook the single exe dispatches: VB_PLAY_LOW_HP_ALARM (z_lifemeter.c) and
        # VB_PLAY_ENEMY_PROXIMITY_MUSIC (z_player.c) via MM_GameHooks_ExecuteVBShould, OnSeqPlayerInit (audio/lib/load.c)
        # via MM_GameHooks_ExecuteOnSeqPlayerInit. None reads the play state unguarded (AudioHook null-checks
        # MM_gPlayState). AudioHook's NotifySequenceName / RegisterAudioNotificationHooks exist in OoT too and are
        # MM_-prefixed by include/mm_audio_prefix.h (pulled in through AudioCollection.h); the toast goes out through
        # the MM_Notify_Emit bridge to OoT's overlay, and AudioCollection::Instance is created by GameExports_SingleExe.
        # #1024
        "2s2h/Enhancements/Audio/Sfx/MuteLowHpAlarm.cpp"
        # #1025
        "2s2h/Enhancements/Audio/Sfx/DisableEnemyProximityMusic.cpp"
        # W6-AUDIO-SFX (#1239): the Audio Editor's four other Sfx TUs, each registering only through a file-scope
        # RegisterShipInitFunc. Hooks, all dispatched in the single exe: ShouldActorInit keyed on ACTOR_OBJ_SOUND
        # (z_actor.c via MM_GameHooks_ExecuteShouldActorInit's ForID leg), and VB_PLAY_GORON_CHILD_CRY (ovl_En_Gk),
        # VB_PLAY_TATL_CALL_AUDIO (z_parameter.c) and VB_LINK_VOICE_PITCH_MULTIPLIER (z_actor.c) via
        # MM_GameHooks_ExecuteVBShould. The carpenter and voice-pitch bodies null-check MM_gPlayState. No stub in
        # mm_stubs.c, no twin of their Register* functions in games/oot (OoT's z_actor.c freqMultiplier is static).
        "2s2h/Enhancements/Audio/Sfx/MuteCarpenterSfx.cpp"
        "2s2h/Enhancements/Audio/Sfx/MuteCryingGoronChild.cpp"
        "2s2h/Enhancements/Audio/Sfx/DisableTatlCallAudio.cpp"
        "2s2h/Enhancements/Audio/Sfx/LinksVoicePitchMultiplier.cpp"
        # #1026
        "2s2h/Enhancements/Audio/AudioHook.cpp"
        # #1023: the Free Look, Camera Distance and Camera Transition Speed rows (Settings > Controls). FreeLook.cpp
        # registers only through a file-scope RegisterShipInitFunc keyed on gSettings.FreeLook.Enabled (kConvergedKeys),
        # and rides ShouldVanillaBehavior[VB_USE_CUSTOM_CAMERA] (Camera_Update) and OnCameraChangeModeFlags
        # (Camera_ChangeMode), both dispatched by z_camera.c via MM_GameHooks_*. The play-state guards landed with the
        # carve-out (Camera_CanFreeLook). It reads the right stick from the play state input, which PadMgr copies from
        # libultraship's pad whole (right_stick_x/y included). Camera/DebugCam.cpp shares the VB leg but keys on its own
        # CVar and stays out. IsBombchuFocused is fenced to false in the TU so RemoteBombchu.cpp is not dragged in.
        # Its definitions (Camera_FreeLook, Camera_CanFreeLook, UpdateFreeLookState, RegisterCameraFreeLook) have no
        # twin in games/oot or games/mm.
        "2s2h/Enhancements/Camera/FreeLook.cpp"
        # #1030-#1034 (#966 key-name gaps, the link half; PR #1046 mapped the keys): the five SoH rows whose MM twin
        # was mapped onto SoH's key and still did nothing because its provider was dropped from the link. Each
        # registers only through a file-scope RegisterShipInitFunc keyed on the converged key SoH's row writes
        # (kConvergedKeys), and rides a hook the single exe dispatches: VB_* via MM_GameHooks_ExecuteVBShould,
        # OnActorInit / OnActorUpdate / OnActorDestroy / ShouldActorInit / OnOpenText via MM_GameHooks_Execute*. Play-state
        # guards landed with the carve-out (RSBS_SINGLE_EXE_REQUIRE in the hook bodies, CanCycleArrows). Equipment/ChuDrops.cpp
        # reads gEnhancements.NewDrops inline but is its own row (gEnhancements.Equipment.ChuDrops) and stays out.
        # #1030
        "2s2h/Enhancements/Accessibility/NoScreenFlashForEnemyKill.cpp"
        # #1031
        "2s2h/Enhancements/Graphics/3DItemDrops.cpp"
        # #1032: the 25 Skip Misc Interactions TUs, all keyed on gEnhancements.TimeSavers.SkipMiscInteractions
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipAlienStartAndFail.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipAlienWarning.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipBankerDialogue.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipBossWarpPrompt.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipBothersomeMonkey.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipDekuSalesman.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipDekuTelescope.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipEponaReveal.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipEvanSong.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipFallingMoonsTear.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipGateGuards.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipGivingBombersNotebook.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipGreatFairyCutscene.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipHungryGoronDialogue.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipKafeiReveal.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipKamaroTeachDance.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipKotakeFlying.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipMayorsOfficeCutscene.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipOwlInteractions.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipPushingMikau.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipRosaSistersDance.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipScarecrowDance.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipTatlInterrupts.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipTingleTime.cpp"
        "2s2h/Enhancements/Cutscenes/MiscInteractions/SkipTurtleGoodbye.cpp"
        # #1033
        "2s2h/Enhancements/Songs/SkipScarecrowSong.cpp"
        # #1034
        "2s2h/Enhancements/Equipment/ArrowCycle.cpp"
        # #1035 (Skip Magic Arrow Equip Animation, OnKaleidoUpdate: dispatched through MM_GameHooks_ExecuteOnKaleidoUpdate;
        # the body touches only the kaleido equip-state globals and the pauseCtx the dispatcher passes)
        "2s2h/Enhancements/Equipment/SkipMagicArrowEquip.cpp"
        # #1036 (Permanent Heart Loss, OnActorUpdate[PLAYER]; MM_RegisterPermanentHeartLoss is MM_-prefixed because OoT's
        # PermanentLosses.cpp has a same-named static registrar the CI symbol probe could not tell apart)
        "2s2h/Enhancements/DifficultyOptions/PermanentHeartLoss.cpp"
        # #1038 (N64 Weird Frames, VB_LOAD_PLAYER_ANIMATION_FRAME from z_skelanime; OoT ships an identically named
        # WeirdAnimation class, which MM's copy fences into S2H under RSBS_SINGLE_EXECUTABLE, so both files link)
        "2s2h/Enhancements/Restorations/N64WeirdFrames/N64WeirdFrames.cpp"
        "2s2h/Enhancements/Restorations/N64WeirdFrames/WeirdAnimation.cpp"
        # #1037 (Damage Multiplier, VB_MULTIPLY_INFLICTED_DMG from func_808339D4 in z_player.c; dispatched through
        # MM_GameHooks_ExecuteVBShould. The body only shifts the damage the hook is handed, so it needs no play-state
        # guard. Held APART from SoH's gEnhancements.DamageMult, whose value space differs: the row that writes MM's
        # key is hosted beside SoH's on Enhancements > Difficulty. RegisterDamageMultiplier has no twin in games/oot)
        "2s2h/Enhancements/DifficultyOptions/DamageMultiplier.cpp"
        # #1215, #1221: deliberately NOT listed: Cheats/UnbreakableRazorSword.cpp, Cycle/EndOfCycle.cpp,
        # Cycle/OceansideWalletAnyDay.cpp and Cycle/OceansideSpiderHouseSquatter.cpp. Each registers a `CVAR || IS_RANDO`
        # gate, but its CVar has no shared-menu row (the #966 class), so only the IS_RANDO half is ported, into the
        # WHOLE_ARCHIVE rando archive: Rando/MiscBehavior/ForcedGatesSingleExe.cpp and OnCycleSave.cpp (the Razor
        # restore). Linking one of the four here would register its rando gate a second time.
    )
    list(REMOVE_ITEM ship__Enhancements ${ship__Enh_registrars})

    # GameInteractor (use OoT's)
    list(FILTER ship__ EXCLUDE REGEX "2s2h/GameInteractor/.*\\.cpp$")

    # ObjectExtension is COMPILED as of Lane C0 (#392): the randomizer's
    # ActorBehavior layer needs the real extension store, and binding OoT's
    # identically-named class cross-registered MM actors into OoT's instance.
    # MM's class lives in namespace S2H with an MM_-prefixed extern "C"
    # surface (see ObjectExtension.h / ActorListIndex.h).

    # CrashHandlerExt stays excluded. ShipUtils.cpp is COMPILED as of Lane C0
    # (#392): the un-elided randomizer hard-depends on its MM-unique surface
    # (Ship_Random/_Seed, Ship_Hash, Ship_GetSceneName, item tints, NES font
    # helpers). The OoT-colliding subset is renamed MM_ via
    # include/mm_ship_utils_prefix.h, and the extended-culling bodies remain
    # owned by GameExports_SingleExe.cpp (mm-culling-binding lock) — guarded
    # out of ShipUtils.cpp for single-exe.
    list(FILTER ship__ EXCLUDE REGEX "2s2h/CrashHandlerExt\\.cpp$")

    # Framebuffer effects
    list(FILTER ship__ EXCLUDE REGEX "2s2h/framebuffer_effects\\.c$")

    # gu_pc (graphics utils)
    list(FILTER ship__ EXCLUDE REGEX "2s2h/gu_pc\\.c$")

    # Audio mixer
    list(FILTER ship__ EXCLUDE REGEX "2s2h/mixer\\.c$")

    # CustomMessage / CustomItem are COMPILED as of Lane C0 (#392): the
    # un-elided randomizer's give/hint paths hard-depend on both (they were
    # 9 of the 21 unresolved externals in PR #422's measured link gap). MM's
    # CustomMessage namespace nests under S2H in single-exe builds — its
    # LoadVanillaMessageTableEntry otherwise mangles identically to OoT's
    # global-scope class CustomMessage static (see CustomMessage.h).

    # Rando
    list(FILTER ship__ EXCLUDE REGEX "2s2h/Rando/.*\\.cpp$")

    # Save Manager
    list(FILTER ship__ EXCLUDE REGEX "2s2h/SaveManager/.*\\.cpp$")

    # Resource importers and types. MM's resource layer lives in namespace S2H
    # (renamed from upstream's SOH for issue #344), so nothing here collides
    # with OoT at link time anymore. What stays excluded is dead code in
    # single-exe mode: formats that are byte-identical between the games and
    # parsed by OoT's registered factories (Animation, Audio*, Background,
    # PlayerAnimation, Skeleton*, CollisionHeader).
    # COMPILED for the MM scene loader (issue #344): SceneFactory,
    # CutsceneFactory (MM's cutscene wire format is NOT compatible with
    # OoT's — the 'Cutscene' loader slot is dispatched per archive, see
    # GameExports_SingleExe.cpp), all scenecommand importers/types, Scene,
    # and the CollisionHeader/Cutscene TYPE classes (cast targets of the
    # scenecommand layer). Also kept per issue #159: PathFactory/
    # TextMMFactory/TextureAnimationFactory/KeyFrameFactory and their types.
    # COMPILED for #604: ArrayFactory and the Array type. The 'Array' slot is
    # dispatched per archive like Room/Cutscene/Path (OoT's reader skips every
    # scalar width but S16/U16, which desynced MM's X8 arrays), and
    # src/common/array_reader_agreement.cpp compares the two readers' vertex
    # output at boot.
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/AnimationFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/AudioSampleFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/AudioSequenceFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/AudioSoundFontFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/BackgroundFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/CollisionHeaderFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/PlayerAnimationFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/SkeletonFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/importer/SkeletonLimbFactory\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/Animation\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/AudioSample\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/AudioSequence\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/AudioSoundFont\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/Background\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/PlayerAnimation\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/Skeleton\\.cpp$")
    list(FILTER ship__ EXCLUDE REGEX "2s2h/resource/type/SkeletonLimb\\.cpp$")

    # Extractor: Extract.cpp is compiled (in-app mm.o2r generation, #317) —
    # its class is named MMExtractor so it can coexist with OoT's Extractor,
    # and in single-exe mode its CallZapd spawns the bundled ZAPD_MM
    # subprocess instead of the in-process zapd_main (#325). FastCrc32C.c
    # stays excluded: it defines the unprefixed CRC32C symbol, which OoT's
    # identical soh/Extractor/FastCrc32C.c already provides.
    list(FILTER ship__Extractor EXCLUDE REGEX "2s2h/Extractor/.*\\.c$")

    # OTR-specific scene/play/message glue (z_play_2SH.cpp, z_scene_2SH.cpp,
    # z_message_OTR.cpp) is COMPILED as of issue #344 — MM's scene loader,
    # room loader, and message tables. Their entry points carry MM_ prefixes
    # (MM_OTRPlay_SpawnScene etc.) so they cannot alias OoT's OTR glue.

    # Ship init
    list(FILTER ship__ EXCLUDE REGEX "2s2h/ShipInit\\.cpp$")

    # Preset manager
    list(FILTER ship__ EXCLUDE REGEX "2s2h/PresetManager/.*\\.cpp$")

    # NameTag (in its own directory)
    list(FILTER ship__ EXCLUDE REGEX "2s2h/NameTag/.*\\.cpp$")

    # Core colliding files - use OoT versions
    list(FILTER src__ EXCLUDE REGEX "src/code/__osMalloc\\.c$")
    list(FILTER src__ EXCLUDE REGEX "src/libc/printutils\\.c$")
    list(FILTER src__ EXCLUDE REGEX "src/boot/fault_drawer\\.c$")

    # Split into three STATIC libraries to avoid:
    # - MSVC LNK1170: response file line >131K chars with 2500+ .obj paths
    # - MSVC LNK1248: COFF .lib archive >4GB
    # Split: src/, 2s2h/ (w/o Enhancements/Rando), 2s2h/Enhancements/, 2s2h/Rando/
    add_library(2ship_src STATIC ${src__} ${Header_Files__include})
    add_library(2ship_port STATIC
        ${ship__} ${ship__Extractor}
        "${CMAKE_CURRENT_SOURCE_DIR}/2s2h/GameExports_SingleExe.cpp"
    )
    add_library(2ship_enh STATIC ${ship__Enhancements})
    # #678: the ClockShuffle-consuming Songs TUs, carved out above so they can
    # be WHOLE_ARCHIVE'd on their own. Listed in TWOSHIP_COMPILE_TARGETS below
    # so it inherits every per-target loop in this file unchanged (the
    # RSBS_SINGLE_EXECUTABLE define, the redship_common/rsbs/libultraship/
    # ZAPDLib deps, the include dirs, and the mm_gi_hook_guard.h poison prelude
    # that #427 item 2 made whole-target).
    add_library(2ship_enh_clockshuffle STATIC ${ship__Enh_clockshuffle})
    # #1071: the RegisterShipInitFunc-only TUs, carved out above, with the same
    # TWOSHIP_COMPILE_TARGETS inheritance as 2ship_enh_clockshuffle.
    add_library(2ship_enh_registrars STATIC ${ship__Enh_registrars})
    add_library(2ship_rando STATIC ${ship__Rando})
    add_library(2ship_rando_ui STATIC ${ship__Rando_ui})
    set(TWOSHIP_COMPILE_TARGETS 2ship_src 2ship_port 2ship_enh 2ship_enh_clockshuffle 2ship_enh_registrars
        2ship_rando 2ship_rando_ui)
    foreach(_t ${TWOSHIP_COMPILE_TARGETS})
        target_compile_definitions(${_t} PRIVATE RSBS_SINGLE_EXECUTABLE)
    endforeach()

    # Propagate compile-time deps to STATIC libs
    foreach(_t ${TWOSHIP_COMPILE_TARGETS})
        target_link_libraries(${_t} PRIVATE redship_common rsbs libultraship ZAPDLib)
    endforeach()

    # Export static targets for root CMakeLists to link into redship.
    #
    # 2ship_rando links with WHOLE_ARCHIVE as of Lane C0 (#392), mirroring
    # soh_rando (#341): its Logic/Regions graph registers purely through
    # RegisterShipInitFunc static initializers and exports nothing anything
    # references, so plain archive semantics silently dropped the whole
    # randomizer. The dependency gap that used to forbid this (the "~27
    # unresolved" note; measured precisely as 21 in PR #422's diagnostic
    # link) was closed by compiling in CustomMessage/CustomItem/
    # ObjectExtension/ShipUtils (renamed per the S2H/MM_ conventions),
    # porting UpdateGameTime, and splitting the BenGui-coupled UI TUs into
    # 2ship_rando_ui, which — like 2ship_enh — deliberately keeps plain
    # archive semantics until MM's menu surface ports. Elision in those two
    # is surfaced (report-only) by .github/scripts/check-registrar-elision.sh,
    # which also now REQUIRES 2ship_rando's registrars to survive the link.
    #
    # 2ship_enh_clockshuffle links WHOLE_ARCHIVE as of #678, for the same
    # reason and with a much smaller radius: two Enhancements TUs whose only
    # effect is a file-scope RegisterShipInitFunc, and which are the sole
    # callers of Rando::ClockShuffle's public API. Both were dropped by plain
    # archive semantics; see the carve-out block above for why this is a
    # separate archive rather than an added reference or the whole-archive
    # flip of 2ship_enh itself (#516). check-registrar-elision.sh audits it as
    # required.
    #
    # 2ship_enh_registrars links WHOLE_ARCHIVE as of #1071, for the same reason:
    # every member registers only through a file-scope RegisterShipInitFunc (the
    # carve-out block above lists them and says how to add one). Also audited as
    # required by check-registrar-elision.sh.
    set(TWOSHIP_STATIC_TARGETS 2ship_src 2ship_port 2ship_enh
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,2ship_enh_clockshuffle>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,2ship_enh_registrars>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,2ship_rando>"
        2ship_rando_ui PARENT_SCOPE)
else()
    # Build as SHARED library for dynamic loading
    add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
    set(TWOSHIP_COMPILE_TARGETS ${PROJECT_NAME})
    target_compile_definitions(${PROJECT_NAME} PRIVATE GAME_BUILDING_DLL)

    # Symbol visibility (only for shared library)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    # Platform-specific naming for shared library
    if(WIN32)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            PREFIX ""
            SUFFIX ".dll"
        )
    elseif(APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            PREFIX ""
            SUFFIX ".dylib"
        )
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            PREFIX ""
            SUFFIX ".so"
        )
    endif()

    # Link deps (only for SHARED — OBJECT libs don't link, deps go on redship)
    target_link_libraries(${PROJECT_NAME} PRIVATE redship_common rsbs)
endif()

set(ROOT_NAMESPACE 2s2h)

################################################################################
# Apply compile properties to all MM targets (foreach handles OBJECT split)
################################################################################
foreach(_t ${TWOSHIP_COMPILE_TARGETS})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
use_props(${_t} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set_target_properties(${_t} PROPERTIES
		VS_GLOBAL_KEYWORD "Win32Proj"
	)
	if(NOT SINGLE_EXECUTABLE_BUILD)
		set_target_properties(${_t} PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
		)
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(${_t} PROPERTIES
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    )
endif()
################################################################################
# MSVC runtime library
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${_t} PROPERTY MSVC_RUNTIME_LIBRARY)
	string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
		$<$<CONFIG:Debug>:
			MultiThreadedDebug
		>
		$<$<CONFIG:Release>:
			MultiThreaded
		>
		$<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
	)
	set_target_properties(${_t} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
endif()
################################################################################
# Compile definitions
################################################################################
find_package(SDL2)
set(SDL2-INCLUDE ${SDL2_INCLUDE_DIRS})

if (BUILD_CROWD_CONTROL)
    find_package(SDL2_net)
    set(SDL2-NET-INCLUDE ${SDL_NET_INCLUDE_DIRS})
endif()

target_include_directories(${_t} PRIVATE assets
	${CMAKE_CURRENT_SOURCE_DIR}/include/
	${CMAKE_CURRENT_SOURCE_DIR}/include/PR
	${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libultraship/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ZAPDTR/ZAPD/resource/type
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/2s2h/
	${SDL2-INCLUDE}
    ${SDL2-NET-INCLUDE}
	${CMAKE_CURRENT_SOURCE_DIR}/assets/
	${dr_libs_SOURCE_DIR}
	.
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(${_t} PRIVATE
		"$<$<CONFIG:Debug>:"
			"_DEBUG;"
			"_CRT_SECURE_NO_WARNINGS;"
			"ENABLE_DX11;"
		">"
		"$<$<CONFIG:Release>:"
			"NDEBUG;"
		">"
           #"$<$<BOOL:${BUILD_CROWD_CONTROL}>:ENABLE_CROWD_CONTROL>"
		"INCLUDE_GAME_PRINTF;"
           #"ENABLE_CROWD_CONTROL;"
		"F3DEX_GBI_2"
		"UNICODE;"
		"_UNICODE"
        SPDLOG_ACTIVE_LEVEL=${SPDLOG_MIN_CUTOFF}
        LOG_LEVEL_GAME_PRINTS=${SPDLOG_LEVEL_OFF}
		STORMLIB_NO_AUTO_LINK
		"_CRT_SECURE_NO_WARNINGS;"
		NOMINMAX
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "CafeOS")
	target_compile_definitions(${_t} PRIVATE
		"$<$<CONFIG:Debug>:"
			"_DEBUG;"
		">"
		"$<$<CONFIG:Release>:"
			"NDEBUG;"
		">"
		"F3DEX_GBI_2;"
		"SPDLOG_NO_THREAD_ID;"
        "SPDLOG_NO_TLS;"
		"STBI_NO_THREAD_LOCALS;"
        SPDLOG_ACTIVE_LEVEL=${SPDLOG_MIN_CUTOFF}
        LOG_LEVEL_GAME_PRINTS=${SPDLOG_LEVEL_OFF}
	)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU|Clang|AppleClang")
	target_compile_definitions(${_t} PRIVATE
		"$<$<CONFIG:Debug>:"
			"_DEBUG;"
		">"
		"$<$<CONFIG:Release>:"
			"NDEBUG;"
		">"
        "F3DEX_GBI_2;"
		# "$<$<BOOL:${BUILD_CROWD_CONTROL}>:ENABLE_CROWD_CONTROL>"
		"_CONSOLE;"
		"_CRT_SECURE_NO_WARNINGS;"
		"ENABLE_OPENGL;"
		"UNICODE;"
		"_UNICODE;"
        SPDLOG_ACTIVE_LEVEL=${SPDLOG_MIN_CUTOFF}
        LOG_LEVEL_GAME_PRINTS=${SPDLOG_LEVEL_OFF}
        "NON_MATCHING;"
        "NON_EQUIVALENT;"
	)
endif()
################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${_t} PRIVATE
        $<$<CONFIG:Debug>:
            /w;
            /Od
        >
        $<$<CONFIG:Release>:
            /Oi
            /Gy;
            /W3
        >
	
		/wd4024 #different types for formal and actual parameter [x]. Usually seen along side 4047.
		/wd4047 #'uintptr_t' differs in levels of indirection from 'Gfx *'. Usually caused by passing a uintptr_t into a function expecting a pointer-types
		/wd4090 #'initializing': different 'const' qualifiers. Caused when initializing a non const struct member with a const char*.		
		/wd4101 # unreferenced local variable. Things like stack pads.
		/wd4133 #incompatible types. Usually caused by passing a c string into a function that takes a pointer.
		/wd4244 #conversion from [a] to [b]. Used to warn when copying to smaller type without a cast.
		/we4013 #RSBS: implicit function declaration -> hard error. This is the ABI-scramble class that
		        #broke ALL MM 3D rendering (dead prototypes in PR/gu.h, f46730f6) and hid five more
		        #implicit decls behind vendoring-era `#if 0` blocks in PR/os_*.h / gs2dex.h. A float or
		        #pointer param through the unprototyped MSVC x64 convention is a silent runtime scramble;
		        #make the whole class a build break. (docs/ci-gameplay-repro-postmortem.md §8 item 7)
        /sdl-;
        /permissive-;
        /MP;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING}
    )
    target_compile_options(${_t} PRIVATE  $<$<CONFIG:Debug>:/ZI;>)
    target_link_options(${_t} PRIVATE
        $<$<CONFIG:Debug>:
            /INCREMENTAL
        >
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO;
            /FORCE:MULTIPLE
        >
        /MANIFEST:NO;
        /DEBUG;
        /SUBSYSTEM:WINDOWS
    )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_compile_options(${_t} PRIVATE
            -Wall -Wextra -Wno-error
            -Wno-return-type
            -Wno-unused-parameter
            -Wno-unused-function
            -Wno-unused-variable
            -Wno-missing-field-initializers
            -Wno-parentheses
            -Wno-narrowing
            -Wno-missing-braces
            -Wno-unknown-pragmas
            $<$<COMPILE_LANGUAGE:C>:
                #-Werror-implicit-function-declaration
                -Wno-ignored-qualifiers
                -Wno-incompatible-pointer-types
                -Wno-int-conversion
            >
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:
                -Wno-c++11-narrowing
                -Wno-deprecated-enum-enum-conversion
            >
            -pthread
        )

        target_link_options(${_t} PRIVATE
            -pthread
        )
    elseif (CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
        target_compile_options(${_t} PRIVATE
            -Wall -Wextra -Wno-error
            -Wno-return-type
            -Wno-unused-parameter
            -Wno-unused-function
            -Wno-unused-variable
            -Wno-missing-field-initializers
            -Wno-parentheses
            -Wno-narrowing
            -Wno-missing-braces
            -Wno-unknown-pragmas
            $<$<COMPILE_LANGUAGE:C>:
                -Werror-implicit-function-declaration
                -Wno-incompatible-pointer-types
            >
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:
                -Wno-c++11-narrowing
                -Wno-deprecated-enum-enum-conversion
            >
            -pthread
        )

        target_link_options(${_t} PRIVATE
            -pthread
        )
    elseif (CMAKE_SYSTEM_NAME STREQUAL "CafeOS")
        target_compile_options(${_t} PRIVATE
            -O2

            # disable some warnings to not clutter output
            -Wno-multichar
            -Wno-return-type
            -Wno-narrowing
            -Wno-switch-outside-range
            $<$<COMPILE_LANGUAGE:C>:
                -Werror-implicit-function-declaration
                -Wno-incompatible-pointer-types
                -Wno-discarded-array-qualifiers
                -Wno-discarded-qualifiers
                -Wno-int-conversion
                -Wno-builtin-declaration-mismatch
                -Wno-switch-unreachable
                -Wno-stringop-overflow
            >
        )
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
		set(CPU_OPTION -msse2 -mfpmath=sse)
        endif()

        target_compile_options(${_t} PRIVATE
            -Wall -Wextra -Wno-error
            -Wno-unused-parameter
            -Wno-unused-function
            -Wno-unused-variable
            -Wno-missing-field-initializers
            -Wno-parentheses
            -Wno-narrowing
            -Wno-missing-braces
            -Wno-unknown-pragmas
            $<$<COMPILE_LANGUAGE:C>:
                -Wno-discarded-qualifiers
                -Wno-discarded-array-qualifiers
                -Wno-int-conversion
                #-Werror-implicit-function-declaration
                -Wno-incompatible-pointer-types
            >
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
            -pthread
	    ${CPU_OPTION}
        )

        target_link_options(${_t} PRIVATE
            -pthread
            -Wl,-export-dynamic
        )
    endif()
endif()

# This file contains a few SIMD functions. Those functions are much more inefficient without optimizations. MSVC and GCC allow for optimizing a single function, but clang does not.
set_source_files_properties(2s2h/mixer.c PROPERTIES
COMPILE_FLAGS "-O2"
SKIP_PRECOMPILE_HEADERS ON
)

################################################################################
# Precompiled Headers
# For OBJECT library builds, we use -include instead of PCH to avoid .gch linker issue
################################################################################
if(SINGLE_EXECUTABLE_BUILD)
    # Use force-include instead of PCH to avoid .gch files being passed to linker
    # GCC/Clang use -include, MSVC uses /FI
    if(MSVC)
        set(_fi "/FI")
    else()
        set(_fi "-include")
    endif()

    set(_force_include_cxx
        "${_fi}libultraship/bridge/consolevariablebridge.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/2s2h/GameInteractor/GameInteractor.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/2s2h/BenPort.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/variables.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/functions.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/macros.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/z64.h"
    )
    # #395 compile-time lock: poison GameInteractor's Register* member names in
    # MM C++ TUs so hook registration cannot bind MM's 104-byte layout to the
    # shared 4-byte OoT-owned instance again (MM code uses the S2H::GameHooks
    # registry / extern "C" shim in include/mm_game_hooks.h instead). Appended
    # AFTER GameInteractor.h so the class definition itself parses with the
    # real names; only later USES in a TU fail. Coverage history: 2ship_src/
    # 2ship_port from #415; 2ship_rando/2ship_rando_ui from Lane C0 (#392) —
    # COND_* macro sites route through the single-exe macro redirection at the
    # bottom of MM's GameInteractor.h, direct Register* sites migrated by hand;
    # 2ship_enh from #427 item 2, which retired the last exemption. Measured
    # against the tree at that point, 2ship_enh carried 15 direct Register*
    # sites across 8 files and 10 matching Unregister* sites, plus 34 live
    # `Instance->events` enqueues across 22 files (and no `currentEvent`
    # reads); all of them now go through S2H::GameHooks / MM_GameEvents_Queue.
    # The one file still holding upstream raw registrations,
    # 2s2h/Enhancements/Audio/AudioEditor.cpp, is filtered out of
    # ship__Enhancements above and so is never compiled in single-exe builds.
    #
    # Every MM C++ target is now guarded: a raw Instance registration anywhere
    # in MM code is a compile error, not a latent out-of-bounds write.
    set(_force_include_cxx_guarded
        ${_force_include_cxx}
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/mm_gi_hook_guard.h"
    )
    # #442's interim per-source carve-out (_mm_gi_hook_guard_linked_enh_sources:
    # guard only the six 2ship_enh TUs the link census showed were non-elided,
    # since flipping the whole target would then have failed on the raw sites in
    # elided TUs) is retired here with #427 item 2. Whole-target coverage
    # strictly subsumes it, and dropping the list removes a census that had to
    # be kept in sync by hand — a newly un-elided 2ship_enh TU can no longer
    # arrive carrying raw registrations that the list happened not to name.
    set(_force_include_c
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/ultra64.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/global.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/z64.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/macros.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/functions.h"
        "${_fi}${CMAKE_CURRENT_SOURCE_DIR}/include/variables.h"
    )

    # Attach the prelude per source file rather than through
    # $<COMPILE_LANGUAGE:...>. MSBuild models force-includes as a single
    # target-level ForcedIncludeFiles property, so the Visual Studio generator
    # resolves COMPILE_LANGUAGE once per target from its linker language, keeps
    # that language's list and silently drops the other one. Every MM target
    # links C++ libultraship, so the linker language is CXX and the .c sources
    # were handed the C++ prelude - consolevariablebridge.h ahead of the z64.h
    # that pulls in <stdbool.h> - failing with "C2061: syntax error: identifier
    # 'CVarExists'" on its bool declaration. Ninja emits per-source flags and so
    # never hit this, which is why CI stayed green while local Visual Studio
    # generator builds broke. Per-source properties hand both generators the
    # command lines Ninja was already producing.
    get_target_property(_target_sources ${_t} SOURCES)
    # No per-target and no per-source exemptions (#427 item 2): the
    # mm_gi_hook_guard.h poison applies to every MM C++ TU in every MM target.
    foreach(_src IN LISTS _target_sources)
        if(_src MATCHES "\\.c$")
            set_source_files_properties(${_src} PROPERTIES COMPILE_OPTIONS "${_force_include_c}")
        elseif(_src MATCHES "\\.(cpp|cxx|cc)$")
            set_source_files_properties(${_src} PROPERTIES COMPILE_OPTIONS "${_force_include_cxx_guarded}")
        endif()
    endforeach()
else()
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<libultraship/bridge/consolevariablebridge.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/Context.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/resource/ResourceManager.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/window/Window.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<spdlog/spdlog.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<map$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<variant$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/2s2h/GameInteractor/GameInteractor.h>"
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/variables.h>"
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/functions.h>"
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/macros.h>"
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/z64.h>"
        "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/include/ultra64.h>"
        "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/include/global.h>"
        "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/include/z64.h>"
        "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/include/macros.h>"
        "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/include/functions.h>"
        "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/include/variables.h>"
    )
endif()

endforeach() # TWOSHIP_COMPILE_TARGETS

# Falling off the end of a non-void function is a hard error in
# games/mm/src/boot/fault.c and games/mm/src/code/stubs.c (#403), mirroring
# the games/oot/CMakeLists.txt / soh/stubs.c precedent (#385/#401). Scoped to
# these two files on purpose: MM passes explicit -Wno-return-type on
# Darwin/Switch/CafeOS and -Wno-error on Linux, and SUPPRESS_WARNINGS'
# WARNING_OVERRIDE is never applied to MM's vendored decomp tree generally —
# a target-wide escalation would fight that and break the build.
#
# Must run after the TWOSHIP_COMPILE_TARGETS loop above (and use APPEND, not
# a plain PROPERTIES assignment): that loop's force-include block
# (set_source_files_properties(${_src} PROPERTIES COMPILE_OPTIONS
# "${_force_include_c}")) sets COMPILE_OPTIONS on every .c source, including
# these two, and a plain assignment here — before or after, in either
# direction — would just replace whichever ran second. set_property(...
# APPEND ...) is the only form that keeps both the force-includes and the
# escalation flag on the same two files.
if(MSVC)
    set_property(SOURCE src/boot/fault.c src/code/stubs.c APPEND PROPERTY COMPILE_OPTIONS "/we4716;/we4715")
else()
    set_property(SOURCE src/boot/fault.c src/code/stubs.c APPEND PROPERTY COMPILE_OPTIONS "-Werror=return-type")
endif()

################################################################################
# Pre build events (only for SHARED library builds)
################################################################################
if(NOT SINGLE_EXECUTABLE_BUILD)
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set(VS_COPY_ASSETS_CMD ${CMAKE_COMMAND} -E copy_directory_if_different $<TARGET_FILE_DIR:2ship>/assets ${CMAKE_BINARY_DIR}/mm/assets)
    endif()
    if(NOT CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch|CafeOS")
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMENT "Copying asset xmls..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/games/mm/assets/extractor $<TARGET_FILE_DIR:2ship>/assets
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/games/mm/assets/xml $<TARGET_FILE_DIR:2ship>/assets/xml
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:2ship>/assets/symbols
            COMMAND ${VS_COPY_ASSETS_CMD}
        )
    endif()
endif()
################################################################################
# Dependencies
################################################################################
foreach(_t ${TWOSHIP_COMPILE_TARGETS})
    add_dependencies(${_t} libultraship)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch|CafeOS")
        add_dependencies(${_t} ZAPDLib)
    endif()
endforeach()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(glfw3 REQUIRED)

    find_package(Ogg CONFIG REQUIRED)
    link_libraries(Ogg::ogg)

    find_package(Vorbis CONFIG REQUIRED)
    link_libraries(Vorbis::vorbisfile)
    find_package(Opus CONFIG REQUIRED)
    link_libraries(Opus::opus)
	find_package(OpusFile CONFIG REQUIRED)
    link_libraries(OpusFile::opusfile CONFIG REQUIRED)

	set(ADDITIONAL_LIBRARY_DEPENDENCIES
		"libultraship;"
		"ZAPDLib;"
		"glu32;"
		"SDL2::SDL2;"
		"SDL2::SDL2main;"
        #"$<$<BOOL:${BUILD_CROWD_CONTROL}>:SDL2_net::SDL2_net-static>"
		"glfw;"
		"winmm;"
		"imm32;"
		"version;"
		"setupapi;"
        "Ogg::ogg"
        "Opus::opus"
        "Vorbis::vorbis"
        "Vorbis::vorbisenc"
        "Vorbis::vorbisfile"
		"OpusFile::opusfile"
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    find_package(SDL2)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "libultraship;"
        SDL2::SDL2
        -lglad
        Threads::Threads
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "CafeOS")
    find_package(SDL2 REQUIRED)
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "libultraship;"
        SDL2::SDL2-static

        "$<$<CONFIG:Debug>:-Wl,--wrap=abort>"
    )
    foreach(_t ${TWOSHIP_COMPILE_TARGETS})
        target_include_directories(${_t} PRIVATE
            ${DEVKITPRO}/portlibs/wiiu/include/
        )
    endforeach()
else()
    find_package(SDL2)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    
    find_package(Threads REQUIRED)
    find_package(Ogg REQUIRED)
    find_package(Vorbis REQUIRED)
    find_package(Opus REQUIRED)
    find_package(OpusFile REQUIRED)
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "libultraship;"
		"ZAPDLib;"
        "Ogg::ogg"
        "Vorbis::vorbis"
        "Vorbis::vorbisenc"
        "Vorbis::vorbisfile"
        "Opus::opus"
        "Opusfile::Opusfile"
		SDL2::SDL2
 #       "$<$<BOOL:${BUILD_CROWD_CONTROL}>:SDL2_net::SDL2_net>"
		${CMAKE_DL_LIBS}
		Threads::Threads
	)

endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin|NintendoSwitch|CafeOS" AND NOT SINGLE_EXECUTABLE_BUILD)
INSTALL(TARGETS 2ship DESTINATION . COMPONENT 2s2h)
endif()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows" AND NOT SINGLE_EXECUTABLE_BUILD)
INSTALL(FILES $<TARGET_PDB_FILE:2ship> DESTINATION ./debug COMPONENT 2s2h)
INSTALL(FILES ${CMAKE_BINARY_DIR}/mm/2ship.o2r DESTINATION . COMPONENT 2s2h)
endif()

find_program(CURL NAMES curl DOC "Path to the curl program.  Used to download files.")
execute_process(COMMAND ${CURL} -sSfL https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt -o ${CMAKE_BINARY_DIR}/gamecontrollerdb.txt OUTPUT_VARIABLE RESULT)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/macosx/Info.plist.in ${CMAKE_BINARY_DIR}/macosx/Info.plist @ONLY)
INSTALL(TARGETS 2ship DESTINATION ../MacOS COMPONENT 2s2h)
INSTALL(FILES ${CMAKE_BINARY_DIR}/gamecontrollerdb.txt DESTINATION ../MacOS COMPONENT 2s2h)
INSTALL(FILES ${CMAKE_BINARY_DIR}/mm/2ship.o2r DESTINATION ../Resources COMPONENT 2s2h)
elseif(NOT "${CMAKE_SYSTEM_NAME}" MATCHES "NintendoSwitch|CafeOS")
INSTALL(FILES ${CMAKE_BINARY_DIR}/gamecontrollerdb.txt DESTINATION . COMPONENT 2s2h)
endif()

if(SINGLE_EXECUTABLE_BUILD)
    # For OBJECT libs: propagate compile-time include dirs from all deps
    foreach(_t ${TWOSHIP_COMPILE_TARGETS})
        target_link_libraries(${_t} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")
    endforeach()
else()
    if(CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch|CafeOS")
        if (NOT TARGET pathconf)
            add_library(pathconf OBJECT platform/pathconf.c)
        endif()
        target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}" $<TARGET_OBJECTS:pathconf> )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")
    endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch")

nx_generate_nacp(2s2h.nacp
   NAME "2s2h of Harkinian"
   AUTHOR "${PROJECT_TEAM}"
   VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
)

nx_create_nro(2s2h
    NACP 2s2h.nacp
    ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg
)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/2s2h.nro DESTINATION . COMPONENT 2s2h)

elseif(CMAKE_SYSTEM_NAME MATCHES "CafeOS")

wut_create_rpx(${PROJECT_NAME})

wut_create_wuhb(${PROJECT_NAME}
	NAME       "Two Ship Two Harkinian"
	SHORTNAME  "2S2H"
	AUTHOR     "${PROJECT_TEAM}"
	ICON       ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg
)

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/2s2h.rpx ${CMAKE_CURRENT_BINARY_DIR}/2s2h.wuhb DESTINATION . COMPONENT 2s2h)

endif()
