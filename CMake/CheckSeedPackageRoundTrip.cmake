# CheckSeedPackageRoundTrip.cmake — the SeedPackageRoundTrip row (#1044).
#
# A seed package written by one process and loaded by another holds the same
# world. Two processes, never one: a load in the process that generated would find
# the generated world still resident, and a field the package lost would read as
# present.
#
#   1. `--test seed-package-write` generates the pinned seed, completes the paired
#      world as Generate does, reports it from the LIVE state, and only then writes
#      its package (so a field the writer drops cannot also be missing from the
#      report it is compared with).
#   2. `--test seed-package-load` never generates: it loads the package through the
#      production load and reports the STAGED world with the same writer.
#
# The two reports (identity and records, OoT's placement and content, the
# crossings, the give capabilities, MM's half byte for byte with its world and
# content, the spoiler's bytes) must be equal, line for line, CR stripped.
#
# Usage: cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -P CheckSeedPackageRoundTrip.cmake

foreach(_required REDSHIP_EXE WORK_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckSeedPackageRoundTrip: -D${_required}=<value> is required")
    endif()
endforeach()

set(_package "${WORK_DIR}/seed-package-roundtrip.rsbs-seed.zip")
set(_write_report "${WORK_DIR}/seed-package-roundtrip-write.txt")
set(_load_report "${WORK_DIR}/seed-package-roundtrip-load.txt")
file(REMOVE "${_package}" "${_write_report}" "${_load_report}")

function(_spk_run dispatch report)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "RSBS_SEED_PACKAGE_PATH=${_package}" "RSBS_SEED_PACKAGE_REPORT=${report}"
                "${REDSHIP_EXE}" --test ${dispatch}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0 OR NOT EXISTS "${report}")
        message(FATAL_ERROR
            "CheckSeedPackageRoundTrip: '--test ${dispatch}' exited ${_rc} (report expected at ${report}).\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
endfunction()

function(_spk_lines path out_var)
    file(READ "${path}" _raw)
    string(REPLACE "\r" "" _raw "${_raw}")
    string(REGEX REPLACE "\n+$" "" _raw "${_raw}")
    string(REPLACE ";" "\\;" _raw "${_raw}")
    string(REPLACE "\n" ";" _lines "${_raw}")
    set(${out_var} "${_lines}" PARENT_SCOPE)
endfunction()

_spk_run(seed-package-write "${_write_report}")
if(NOT EXISTS "${_package}")
    message(FATAL_ERROR "CheckSeedPackageRoundTrip: '--test seed-package-write' succeeded but wrote no package at "
                        "${_package}")
endif()
_spk_run(seed-package-load "${_load_report}")

_spk_lines("${_write_report}" _written)
_spk_lines("${_load_report}" _loaded)
list(LENGTH _written _n_written)
list(LENGTH _loaded _n_loaded)
if(_n_written LESS 30)
    message(FATAL_ERROR "CheckSeedPackageRoundTrip: the live world's report has only ${_n_written} line(s); a report "
                        "that small pins nothing")
endif()

set(_diff "")
math(EXPR _last "${_n_written} - 1")
if(_n_loaded GREATER _n_written)
    math(EXPR _last "${_n_loaded} - 1")
endif()
foreach(_i RANGE ${_last})
    set(_w "(absent)")
    set(_l "(absent)")
    if(_i LESS _n_written)
        list(GET _written ${_i} _w)
    endif()
    if(_i LESS _n_loaded)
        list(GET _loaded ${_i} _l)
    endif()
    if(NOT _w STREQUAL _l)
        string(APPEND _diff "\n  generated: ${_w}\n  loaded:    ${_l}")
    endif()
endforeach()

if(_diff)
    message(FATAL_ERROR
        "CheckSeedPackageRoundTrip: the world a second process loaded from the package is NOT the world the first "
        "process generated:${_diff}\n"
        "  package: ${_package}\n  reports: ${_write_report} / ${_load_report}")
endif()
message(STATUS "CheckSeedPackageRoundTrip: ${_n_written} field(s) of the generated world are the loaded package's "
               "(${_package})")
