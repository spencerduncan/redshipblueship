# CMake/CheckCrossingRefused.cmake
#
# #954 lock: a crossing into Majora's Mask that is refused because one of its
# archives is missing must leave Ocarina of Time running. Before #954 the
# Happy Mask Shop door let OoT's frame loop exit with the switch pending; main
# refused the switch only then and ran OoT_Game_Run again, whose Main()
# re-initialized the system arena under the gamestate the graph coroutine still
# pointed at, and the first frame faulted in OoT_GameState_SetFrameBuffer.
#
# One run, in the build directory (the staged archives live there):
#   1. Set ${HIDE} aside (restored at the end, pass or fail), so MM cannot be
#      entered: mm.o2r for the missing-game-archive refusal (#317), 2ship.o2r
#      for the missing-port-archive one.
#   2. Run OoT (--integration-test MODE). The game-side driver
#      (games/oot/soh/GameExports_SingleExe.cpp, int-crossing-refused) walks
#      into the door, then makes a raw switch request, and asserts OoT plays on
#      after both. The run must exit 0, carry the driver's PASS line, and carry
#      the report that names ${HIDE}'s refusal (EXPECT_REPORT).
#
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DMODE=<int-* dispatch>
#         -DHIDE=<archive> -DEXPECT_REPORT=<text> [-DRUN_TIMEOUT=<s>]
#         -P CheckCrossingRefused.cmake
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

# MM's game archive is also accepted as mm.zip / mm.otr: either would let the
# crossing go ahead, and the row would prove nothing.
if(HIDE STREQUAL "mm.o2r")
    foreach(_alt mm.zip mm.otr)
        if(EXISTS "${WORK_DIR}/${_alt}")
            message(FATAL_ERROR "CheckCrossingRefused: ${WORK_DIR}/${_alt} is staged too, so Majora's Mask stays "
                                "available with ${HIDE} aside; move it out of the build directory")
        endif()
    endforeach()
endif()

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
string(REGEX MATCH "\\[OoT-INT-TEST\\] int-crossing-refused: PASS[^\n]*" _pass "${_out}")
if(NOT _pass)
    _fail("the driver never reached its PASS line")
endif()
message(STATUS "CheckCrossingRefused(${MODE}, ${HIDE} aside): ${_pass}")
