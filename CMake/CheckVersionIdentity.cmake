# CMake/CheckVersionIdentity.cmake
#
# #880 lock on the two surfaces a person reads, through the real binary rather
# than the rsbs_version.c seam the VersionShortCommit row checks:
#   1. `redship --version` exits 0 and its first line is
#      "RedShipBlueShip <release><build identity> (<codename>)", where the build
#      identity is "+<SHORT_COMMIT>" plus "-dirty" when the configure read the
#      tree dirty, and nothing at all for a tree without git (EXPECT_SHORT
#      "none"); the release part carries no '+' of its own.
#   2. main()'s Ship::Context is created with that same name: the window title
#      (libultraship appends the backend), the crash dialog and the log file
#      name. rsbs/src/main.cpp prints "[RSBS] Ship::Context name: <name>" right
#      after creating it and, with RSBS_INT_EXIT_AFTER_CONTEXT=1 in integration
#      mode, exits there, before any archive is read or a window exists. So the
#      row stays ROM-free and display-free.
# Reverting either call site in main.cpp to the bare RSBS_* macros turns the
# matching half red. The dirty bit's own oracle is the BuildIdentityDirty row.
#
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DEXPECT_SHORT=<hash|none>
#         -DEXPECT_DIRTY=<0|1> -P CheckVersionIdentity.cmake

foreach(_required REDSHIP_EXE WORK_DIR EXPECT_SHORT EXPECT_DIRTY)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckVersionIdentity: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckVersionIdentity: redship binary not found: ${REDSHIP_EXE}")
endif()

if(EXPECT_SHORT STREQUAL "none")
    set(_suffix "")
else()
    set(_suffix "+${EXPECT_SHORT}")
    if(EXPECT_DIRTY)
        string(APPEND _suffix "-dirty")
    endif()
endif()

set(_failures 0)
# <name> must read "RedShipBlueShip <release><_suffix> (<codename>)" with no '+'
# in <release>.
function(_check_identity _what _name)
    set(_ok FALSE)
    if(_name MATCHES "^RedShipBlueShip ([^ ]+) \\(([^()]+)\\)$")
        set(_version "${CMAKE_MATCH_1}")
        string(LENGTH "${_version}" _vlen)
        string(LENGTH "${_suffix}" _slen)
        if(_vlen GREATER _slen)
            math(EXPR _rlen "${_vlen} - ${_slen}")
            string(SUBSTRING "${_version}" 0 ${_rlen} _release)
            string(SUBSTRING "${_version}" ${_rlen} -1 _tail)
            string(FIND "${_release}" "+" _plus)
            if(_tail STREQUAL "${_suffix}" AND _plus EQUAL -1)
                set(_ok TRUE)
            endif()
        endif()
    endif()
    if(_ok)
        message(STATUS "CheckVersionIdentity: ${_what}: '${_name}' carries '${_suffix}' ok")
    else()
        message(SEND_ERROR "CheckVersionIdentity: FAIL ${_what}: '${_name}' is not "
                           "'RedShipBlueShip <release>${_suffix} (<codename>)'")
        math(EXPR _n "${_failures} + 1")
        set(_failures ${_n} PARENT_SCOPE)
    endif()
endfunction()

message(STATUS "CheckVersionIdentity: configured short='${EXPECT_SHORT}' dirty='${EXPECT_DIRTY}' "
               "-> build identity '${_suffix}'")

# 1. --version
execute_process(
    COMMAND "${REDSHIP_EXE}" --version
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT 30
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
string(REPLACE "\r" "" _out "${_out}")
string(REGEX MATCH "^[^\n]*" _version_line "${_out}")
if(NOT _rc STREQUAL "0")
    message(SEND_ERROR "CheckVersionIdentity: FAIL --version exited '${_rc}'\nstdout:\n${_out}\nstderr:\n${_err}")
    math(EXPR _failures "${_failures} + 1")
endif()
_check_identity("--version first line" "${_version_line}")

# 2. The Ship::Context main() creates
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env RSBS_INT_EXIT_AFTER_CONTEXT=1
            "${REDSHIP_EXE}" --integration-test int-boot-oot
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT 60
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
string(REPLACE "\r" "" _out "${_out}")
string(REGEX MATCH "\\[RSBS\\] Ship::Context name: [^\n]*" _context_line "${_out}")
string(REPLACE "[RSBS] Ship::Context name: " "" _context_name "${_context_line}")
if(NOT _rc STREQUAL "0" OR NOT _context_line)
    message(SEND_ERROR "CheckVersionIdentity: FAIL the context run exited '${_rc}' with "
                       "context line '${_context_line}'\noutput:\n${_out}")
    math(EXPR _failures "${_failures} + 1")
else()
    _check_identity("Ship::Context name (window title)" "${_context_name}")
    if(NOT _context_name STREQUAL "${_version_line}")
        message(SEND_ERROR "CheckVersionIdentity: FAIL the context name '${_context_name}' differs "
                           "from the --version first line '${_version_line}'")
        math(EXPR _failures "${_failures} + 1")
    endif()
endif()

if(_failures GREATER 0)
    message(FATAL_ERROR "CheckVersionIdentity: ${_failures} failure(s)")
endif()
message(STATUS "CheckVersionIdentity: PASS")
