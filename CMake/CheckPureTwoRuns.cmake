# CMake/CheckPureTwoRuns.cmake
#
# #1047: a value MM decides while you play must be a pure function of the frozen
# world, so two PROCESSES must decide it alike. Runs `redship --test <TEST_NAME>`
# twice, requires both runs to pass, and requires the "[pure] " lines each run
# printed to be the same, line for line, and not empty. One process cannot show
# this: a value seeded by something a process carries (a clock, a frame counter,
# where a shared stream happened to be, an uninitialised read) can repeat inside
# one process and still differ in the next.
#
# Run as a CTest row:
#   cmake -DREDSHIP_EXE=<redship> -DTEST_NAME=<dispatch> -P CheckPureTwoRuns.cmake
# The row's environment (SDL_AUDIODRIVER=dummy and the no-activation hint, which
# redship_add_test gives every row) is inherited by both children.

if(NOT DEFINED REDSHIP_EXE OR NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckPureTwoRuns: -DREDSHIP_EXE=<path> must name the redship binary (got '${REDSHIP_EXE}')")
endif()
if(NOT DEFINED TEST_NAME OR TEST_NAME STREQUAL "")
    message(FATAL_ERROR "CheckPureTwoRuns: -DTEST_NAME=<dispatch> is required")
endif()

function(_pure_run label out_var)
    execute_process(
        COMMAND "${REDSHIP_EXE}" --test "${TEST_NAME}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "CheckPureTwoRuns: the ${label} run of '--test ${TEST_NAME}' exited ${_rc}.\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
    string(REPLACE "\r" "" _out "${_out}")
    string(REGEX MATCHALL "\\[pure\\] [^\n]*" _lines "${_out}")
    if(NOT _lines)
        message(FATAL_ERROR
            "CheckPureTwoRuns: the ${label} run of '--test ${TEST_NAME}' printed no [pure] lines, so there is "
            "nothing to compare.\nstdout:\n${_out}")
    endif()
    set(${out_var} "${_lines}" PARENT_SCOPE)
endfunction()

_pure_run("first" _first)
_pure_run("second" _second)

list(LENGTH _first _n1)
list(LENGTH _second _n2)
if(NOT _first STREQUAL _second)
    set(_diff "")
    set(_i 0)
    foreach(_line IN LISTS _first)
        if(_i LESS _n2)
            list(GET _second ${_i} _other)
        else()
            set(_other "<missing>")
        endif()
        if(NOT _line STREQUAL _other)
            string(APPEND _diff "  first:  ${_line}\n  second: ${_other}\n")
        endif()
        math(EXPR _i "${_i} + 1")
    endforeach()
    message(FATAL_ERROR
        "CheckPureTwoRuns: two processes of '--test ${TEST_NAME}' decided differently "
        "(${_n1} and ${_n2} [pure] lines):\n${_diff}")
endif()

message(STATUS "CheckPureTwoRuns: '--test ${TEST_NAME}': two processes printed the same ${_n1} [pure] lines")
