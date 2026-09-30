# CMake/CheckIntWatchdogExit.cmake
#
# #793 lock, both halves of the wall-clock watchdog's contract on one wedged run:
#   1. the watchdog names the stage the run wedged in
#      ("[INT-WATCHDOG] FAIL ... last stage: <EXPECT_STAGE>"), and
#   2. it ends the process with INT_WATCHDOG_EXIT_CODE (3,
#      src/common/integration_test_hooks.h).
#
# A plain PASS_REGULAR_EXPRESSION row checks only (1): CTest ignores the exit
# code once a pass regex is set. So a watchdog that printed its FAIL line and
# then exited 0 would keep such a row green, while an ordinary integration row
# (no pass regex) that really wedged would print FAIL, exit 0 and pass. This
# script asserts the exact exit code as well as the line.
#
# Run as a CTest row in the "integration" tier:
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DMODE=<int-* dispatch>
#         -DEXPECT_STAGE=<stage text> -DEXPECT_RC=3 [-DRUN_TIMEOUT=<s>]
#         -P CheckIntWatchdogExit.cmake
# RSBS_INT_WEDGE* and RSBS_INT_WATCHDOG_SECS come from the row's CTest
# ENVIRONMENT and pass through to the child.

foreach(_required REDSHIP_EXE WORK_DIR MODE EXPECT_STAGE EXPECT_RC)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckIntWatchdogExit: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckIntWatchdogExit: redship binary not found: ${REDSHIP_EXE}")
endif()

# Integration rows run in the build directory: the staged config, the archives
# and the logs live there. RUN_TIMEOUT (the row's CTest TIMEOUT minus 10 s) keeps
# the child under the row's wall, so a run the watchdog never ends is reported
# here, with its output, and the child is killed rather than orphaned.
if(NOT DEFINED RUN_TIMEOUT)
    set(RUN_TIMEOUT 110)
endif()
execute_process(
    COMMAND "${REDSHIP_EXE}" --integration-test ${MODE}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${RUN_TIMEOUT}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)

set(_regex "\\[INT-WATCHDOG\\] FAIL [^\n]*last stage: ${EXPECT_STAGE}")
string(REGEX MATCH "${_regex}" _line "${_out}")

if(NOT _rc STREQUAL "${EXPECT_RC}")
    message(FATAL_ERROR
        "CheckIntWatchdogExit(${MODE}): the run exited '${_rc}', expected ${EXPECT_RC} "
        "(INT_WATCHDOG_EXIT_CODE). A watchdog that ends a wedged run with any other code "
        "can turn a wedge into a pass.\noutput:\n${_out}")
endif()
if(NOT _line)
    message(FATAL_ERROR
        "CheckIntWatchdogExit(${MODE}): exit code ${_rc} as expected, but no line matched "
        "'${_regex}'.\noutput:\n${_out}")
endif()

message("${_out}")
message(STATUS "CheckIntWatchdogExit(${MODE}): exit code ${_rc} and '${_line}'")
