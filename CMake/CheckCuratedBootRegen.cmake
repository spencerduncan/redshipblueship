# CMake/CheckCuratedBootRegen.cmake
#
# #806 lock on the REAL boot path: rsbs/src/main.cpp calls
# Combo_EnsureCuratedArchives() before Combo_MountCuratedArchive(), so a game
# that boots with its curated halves missing or stale carves them out of the
# extracted oot.o2r / mm.o2r itself and mounts the fresh one. The
# CuratedArchiveInApp row drives the library directly; this row is the one that
# turns red when the boot call is deleted or moved after the mount.
#
# One run, in the build directory (the staged archives live there):
#   1. Set the staged halves aside (restored at the end, pass or fail), then
#      leave redship-oot.o2r STALE (a zip with no stamp, which a mount accepts)
#      and redship-mm.o2r ABSENT.
#   2. Boot OoT (--integration-test MODE). The output must carry
#      "[RSBS] Curated cross-game archives generated: redship-oot.o2r stale ...;
#      redship-mm.o2r absent" BEFORE "[RSBS] Cross-game archive ready (OoT
#      identity)", and the run must pass. Both halves must exist afterwards.
#   3. Boot again. The output must carry "Curated cross-game archives
#      up-to-date": the halves the first boot wrote carry the stamp this binary
#      computes from its embedded manifest and the extracted sources.
#
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DMODE=<int-* dispatch>
#         [-DRUN_TIMEOUT=<s>] -P CheckCuratedBootRegen.cmake

foreach(_required REDSHIP_EXE WORK_DIR MODE)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckCuratedBootRegen: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckCuratedBootRegen: redship binary not found: ${REDSHIP_EXE}")
endif()
foreach(_source oot.o2r mm.o2r)
    if(NOT EXISTS "${WORK_DIR}/${_source}")
        message(FATAL_ERROR "CheckCuratedBootRegen: ${WORK_DIR}/${_source} is not staged; this integration row "
                            "needs both extracted archives")
    endif()
endforeach()
if(NOT DEFINED RUN_TIMEOUT)
    set(RUN_TIMEOUT 110)
endif()

set(_halves redship-oot.o2r redship-mm.o2r)
set(_suffix ".curated-boot-regen-backup")

# A backup left by an interrupted earlier run is the staged original: keep it.
foreach(_half ${_halves})
    if(EXISTS "${WORK_DIR}/${_half}" AND NOT EXISTS "${WORK_DIR}/${_half}${_suffix}")
        file(RENAME "${WORK_DIR}/${_half}" "${WORK_DIR}/${_half}${_suffix}")
    elseif(EXISTS "${WORK_DIR}/${_half}")
        file(REMOVE "${WORK_DIR}/${_half}")
    endif()
endforeach()

# Functions, not macros: a macro would splice the run's output into the
# command text.
function(_restore_halves)
    foreach(_half ${_halves})
        if(EXISTS "${WORK_DIR}/${_half}${_suffix}")
            file(REMOVE "${WORK_DIR}/${_half}")
            file(RENAME "${WORK_DIR}/${_half}${_suffix}" "${WORK_DIR}/${_half}")
        endif()
    endforeach()
    file(REMOVE "${WORK_DIR}/curated-boot-regen-stale.txt")
endfunction()

# Restores the staged halves, then fails with the caller's _out. The reason may
# be given as several quoted pieces; they are joined.
function(_fail)
    string(JOIN "" _why ${ARGV})
    _restore_halves()
    message(FATAL_ERROR "CheckCuratedBootRegen(${MODE}): ${_why}\noutput:\n${_out}")
endfunction()

# The stale OoT half: a valid zip with no archive comment (a pre-#806 half).
file(WRITE "${WORK_DIR}/curated-boot-regen-stale.txt" "stale")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf redship-oot.o2r --format=zip curated-boot-regen-stale.txt
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE _tar_rc
)
if(NOT _tar_rc EQUAL 0 OR NOT EXISTS "${WORK_DIR}/redship-oot.o2r")
    _fail("could not write the stale redship-oot.o2r (rc ${_tar_rc})")
endif()

# 2. The boot that must regenerate.
execute_process(
    COMMAND "${REDSHIP_EXE}" --integration-test ${MODE}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${RUN_TIMEOUT}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    _fail("the first boot exited '${_rc}'")
endif()
set(_generated_re "\\[RSBS\\] Curated cross-game archives generated: redship-oot.o2r stale [^\n]*redship-mm.o2r absent")
string(REGEX MATCH "${_generated_re}" _generated "${_out}")
if(NOT _generated)
    _fail("the first boot did not regenerate the stale OoT half and the absent MM half (no line matched "
          "'${_generated_re}'): is Combo_EnsureCuratedArchives still called at boot?")
endif()
string(FIND "${_out}" "${_generated}" _generated_at)
string(FIND "${_out}" "[RSBS] Cross-game archive ready (oot identity)" _mounted_at)
if(_mounted_at EQUAL -1)
    _fail("the first boot mounted no curated OoT half")
endif()
if(_mounted_at LESS _generated_at)
    _fail("the first boot mounted the curated OoT half BEFORE regenerating it: Combo_EnsureCuratedArchives must "
          "run before Combo_MountCuratedArchive")
endif()
foreach(_half ${_halves})
    if(NOT EXISTS "${WORK_DIR}/${_half}")
        _fail("${_half} does not exist after the first boot")
    endif()
endforeach()

# 3. The next boot finds both halves current.
execute_process(
    COMMAND "${REDSHIP_EXE}" --integration-test ${MODE}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${RUN_TIMEOUT}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    _fail("the second boot exited '${_rc}'")
endif()
string(REGEX MATCH "\\[RSBS\\] Curated cross-game archives up-to-date: [^\n]*" _current "${_out}")
if(NOT _current)
    _fail("the second boot did not find the halves the first boot wrote up to date")
endif()

_restore_halves()
message(STATUS "CheckCuratedBootRegen(${MODE}): first boot '${_generated}'")
message(STATUS "CheckCuratedBootRegen(${MODE}): second boot '${_current}'")
