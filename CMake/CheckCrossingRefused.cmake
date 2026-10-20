# CMake/CheckCrossingRefused.cmake
#
# #954 lock: a crossing that is refused because one of the other game's
# archives is missing must leave the running game running. Before #954 the
# Happy Mask Shop door let OoT's frame loop exit with the switch pending; main
# refused the switch only then and ran OoT_Game_Run again, whose Main()
# re-initialized the system arena under the gamestate the graph coroutine still
# pointed at, and the first frame faulted in OoT_GameState_SetFrameBuffer. The
# Clock Tower door is the same gate seen from Majora's Mask.
#
# One run, in the build directory (the staged archives live there):
#   1. Set ${HIDE} aside (restored at the end, pass or fail), so the other game
#      cannot be entered: mm.o2r for the missing-game-archive refusal (#317),
#      2ship.o2r for the missing-port-archive one, oot.o2r for MM's door into
#      an Ocarina of Time this session cannot run.
#   2. Run --integration-test MODE. The game-side driver walks into the door,
#      then makes a raw switch request, and asserts the game plays on after
#      both: int-crossing-refused boots OoT (games/oot/soh/GameExports_SingleExe.cpp),
#      int-crossing-refused-mm boots MM (games/mm/2s2h/GameExports_SingleExe.cpp).
#      The run must exit 0, carry the driver's PASS line, carry the report that
#      names ${HIDE}'s refusal (EXPECT_REPORT) and, when given, the door's own
#      refusal line (EXPECT_REFUSAL, which Combo_CheckEntranceSwitch prints
#      only when it refuses inside the running game).
#
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DMODE=<int-* dispatch>
#         -DHIDE=<archive> -DEXPECT_REPORT=<text> [-DEXPECT_REFUSAL=<text>]
#         [-DRUN_TIMEOUT=<s>] -P CheckCrossingRefused.cmake
#
# The row carries RUN_SERIAL: while ${HIDE} is aside no other row may run.

foreach(_required REDSHIP_EXE WORK_DIR MODE HIDE EXPECT_REPORT)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckCrossingRefused: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckCrossingRefused: redship binary not found: ${REDSHIP_EXE}")
endif()
if(NOT DEFINED RUN_TIMEOUT)
    set(RUN_TIMEOUT 110)
endif()

set(_suffix ".crossing-refused-backup")
set(_staged "${WORK_DIR}/${HIDE}")
set(_backup "${WORK_DIR}/${HIDE}${_suffix}")

# Each game archive has accepted alternates (src/common/archive_check.cpp):
# mm.zip / mm.otr for MM, oot-mq.o2r for OoT. Any of them would let the
# crossing go ahead, and the row would prove nothing.
set(_alts "")
if(HIDE STREQUAL "mm.o2r")
    set(_alts mm.zip mm.otr)
    set(_game "Majora's Mask")
elseif(HIDE STREQUAL "oot.o2r")
    set(_alts oot-mq.o2r)
    set(_game "Ocarina of Time")
endif()
foreach(_alt IN LISTS _alts)
    if(EXISTS "${WORK_DIR}/${_alt}")
        message(FATAL_ERROR "CheckCrossingRefused: ${WORK_DIR}/${_alt} is staged too, so ${_game} stays "
                            "available with ${HIDE} aside; move it out of the build directory")
    endif()
endforeach()

# A backup left by an interrupted earlier run is the staged original: keep it.
if(EXISTS "${_staged}" AND NOT EXISTS "${_backup}")
    file(RENAME "${_staged}" "${_backup}")
elseif(EXISTS "${_staged}")
    file(REMOVE "${_staged}")
endif()
if(NOT EXISTS "${_backup}")
    message(FATAL_ERROR "CheckCrossingRefused: ${_staged} is not staged; this integration row sets the staged archive "
                        "aside and needs the rest of a staged tree (oot.o2r, soh.o2r, mm.o2r, 2ship.o2r)")
endif()

function(_restore)
    if(EXISTS "${_backup}")
        file(REMOVE "${_staged}")
        file(RENAME "${_backup}" "${_staged}")
    endif()
endfunction()

# Restores the archive, then fails with the caller's _out. The reason may be
# given as several quoted pieces; they are joined.
function(_fail)
    string(JOIN "" _why ${ARGV})
    _restore()
    message(FATAL_ERROR "CheckCrossingRefused(${MODE}, ${HIDE} aside): ${_why}\noutput:\n${_out}")
endfunction()

execute_process(
    COMMAND "${REDSHIP_EXE}" --integration-test ${MODE}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${RUN_TIMEOUT}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
_restore()

if(NOT _rc EQUAL 0)
    _fail("the run exited '${_rc}'")
endif()
string(FIND "${_out}" "${EXPECT_REPORT}" _report_at)
if(_report_at EQUAL -1)
    _fail("no '${EXPECT_REPORT}' report: the crossing was not refused for ${HIDE}")
endif()
if(DEFINED EXPECT_REFUSAL)
    string(FIND "${_out}" "${EXPECT_REFUSAL}" _refusal_at)
    if(_refusal_at EQUAL -1)
        _fail("no '${EXPECT_REFUSAL}' line: the door's crossing was not refused inside the running game")
    endif()
endif()
# The driver's tag names the game it runs in. MODE is a dispatch name, whose
# only regex metacharacter is '-', which means itself outside brackets.
string(REGEX MATCH "\\[(OoT|MM)-INT-TEST\\] ${MODE}: PASS[^\n]*" _pass "${_out}")
if(NOT _pass)
    _fail("the driver never reached its PASS line")
endif()
message(STATUS "CheckCrossingRefused(${MODE}, ${HIDE} aside): ${_pass}")
