# CMake/CheckTestRegistration.cmake
#
# Registration-completeness guard. Run as a CTest row in the tier it polices:
#   cmake -DREDSHIP_EXE=<redship> -DMANIFEST=<manifest> -P CheckTestRegistration.cmake
#
# Fails when the two halves of test registration disagree:
#   (1) a dispatch-table entry in test_runner.cpp with no CTest row  — a test
#       that exists but is never scheduled;
#   (2) a CTest row whose `--test <name>` is not in the dispatch table — a row
#       that can only ever error out.
#
# The table side is read from the BINARY (`redship --test list`), not from the
# source text, so it reflects what actually linked.
#
# This file is deliberately paranoid about its own inputs. A completeness guard
# that silently checks nothing is worse than no guard: this repo has shipped two
# of those already (#366's clang-format target pointed at a directory that did
# not exist; #375's collision baseline was never committed, so it "could not
# fail"). Every way this script could end up comparing empty sets against empty
# sets is an explicit FATAL_ERROR below.

if(NOT DEFINED REDSHIP_EXE)
    message(FATAL_ERROR "CheckTestRegistration: -DREDSHIP_EXE=<path> is required")
endif()
if(NOT DEFINED MANIFEST)
    message(FATAL_ERROR "CheckTestRegistration: -DMANIFEST=<path> is required")
endif()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckTestRegistration: redship binary not found: ${REDSHIP_EXE}")
endif()
if(NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "CheckTestRegistration: manifest not found: ${MANIFEST}")
endif()

# ============================================================================
# Side A — what CMake registered (manifest written by redship_finalize_tests)
# ============================================================================

file(STRINGS "${MANIFEST}" _manifest_lines)

set(_reg_unit "")          # dispatch names driven by a --test row
set(_reg_int "")           # dispatch names driven by an --integration-test row
set(_exempt "")            # dispatch names declared row-less on purpose
set(_row_for "")           # parallel to _reg_unit/_reg_int: owning CTest row
set(_meta_rows "")         # rows that drive neither table (this guard itself)

foreach(_line IN LISTS _manifest_lines)
    if(_line MATCHES "^#")
        continue()
    elseif(_line MATCHES "^register\\|([^|]*)\\|([^|]*)\\|([^|]*)\\|(.*)$")
        set(_ctest_name "${CMAKE_MATCH_2}")
        set(_kind "${CMAKE_MATCH_3}")
        set(_dispatch "${CMAKE_MATCH_4}")
        if(_kind STREQUAL "unit")
            list(APPEND _reg_unit "${_dispatch}")
            list(APPEND _row_for "${_dispatch}=${_ctest_name}")
        elseif(_kind STREQUAL "integration")
            list(APPEND _reg_int "${_dispatch}")
            list(APPEND _row_for "${_dispatch}=${_ctest_name}")
        else()
            list(APPEND _meta_rows "${_ctest_name}")
        endif()
    elseif(_line MATCHES "^exempt\\|([^|]*)\\|(.*)$")
        list(APPEND _exempt "${CMAKE_MATCH_1}")
    endif()
endforeach()

if(NOT _reg_unit)
    message(FATAL_ERROR
        "CheckTestRegistration: the manifest lists no --test rows at all (${MANIFEST}).\n"
        "Either registration broke or the manifest format changed; either way this "
        "guard would pass vacuously. Refusing.")
endif()

# ============================================================================
# Side B — what the binary actually contains (`redship --test list`)
# ============================================================================

execute_process(
    COMMAND "${REDSHIP_EXE}" --test list
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "CheckTestRegistration: '${REDSHIP_EXE} --test list' exited ${_rc}.\n"
        "stdout:\n${_out}\nstderr:\n${_err}")
endif()

string(REPLACE "\r\n" "\n" _out_n "${_out}")
string(REPLACE ";" "\\;" _out_n "${_out_n}")
string(REPLACE "\n" ";" _lines "${_out_n}")

set(_section "")
set(_saw_unit_header FALSE)
set(_saw_special_header FALSE)
set(_saw_int_header FALSE)
set(_found_unit "")
set(_found_special "")
set(_found_integration "")

foreach(_line IN LISTS _lines)
    if(_line MATCHES "Available unit tests")
        set(_section "unit")
        set(_saw_unit_header TRUE)
    elseif(_line MATCHES "^Special commands:")
        set(_section "special")
        set(_saw_special_header TRUE)
    elseif(_line MATCHES "^Integration tests \\(")
        set(_section "integration")
        set(_saw_int_header TRUE)
    elseif(_section AND _line MATCHES "^  ([A-Za-z0-9._-]+)[ \t]")
        # Entry rows are "  <name><padding><description>". The parenthetical
        # note under the integration header starts with '(' and is skipped by
        # the character class.
        list(APPEND _found_${_section} "${CMAKE_MATCH_1}")
    endif()
endforeach()

# If TestRunner_ListTests' output format ever changes, the parse above can go
# quiet and start comparing nothing to nothing. Treat a missing section marker
# or an empty section as a hard failure of THIS guard, not as a pass.
if(NOT _saw_unit_header OR NOT _saw_special_header OR NOT _saw_int_header)
    message(FATAL_ERROR
        "CheckTestRegistration: could not parse '--test list' output — expected the "
        "'Available unit tests' / 'Special commands:' / 'Integration tests (' section "
        "markers (found: unit=${_saw_unit_header} special=${_saw_special_header} "
        "integration=${_saw_int_header}).\n"
        "TestRunner_ListTests' format probably changed; update this parser.\n"
        "Raw output:\n${_out}")
endif()
if(NOT _found_unit OR NOT _found_integration)
    message(FATAL_ERROR
        "CheckTestRegistration: parsed zero entries from a '--test list' section "
        "(unit=${_found_unit} / integration=${_found_integration}). Refusing to pass vacuously.\n"
        "Raw output:\n${_out}")
endif()

# ============================================================================
# Compare
# ============================================================================

set(_problems "")

# Split on the first '=' rather than interpolating the dispatch name into a
# regex — an error path must not itself misfire on an unexpected name.
function(_row_owning dispatch out_var)
    foreach(_pair IN LISTS _row_for)
        string(REGEX REPLACE "=.*$" "" _d "${_pair}")
        if(_d STREQUAL "${dispatch}")
            string(REGEX REPLACE "^[^=]*=" "" _owner "${_pair}")
            set(${out_var} "${_owner}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} "<none>" PARENT_SCOPE)
endfunction()

# (1) every dispatch-table entry needs a CTest row (or a declared exemption)
set(_used_exempt "")
foreach(_entry IN LISTS _found_unit)
    if(_entry IN_LIST _reg_unit)
        continue()
    elseif(_entry IN_LIST _exempt)
        list(APPEND _used_exempt "${_entry}")
    else()
        list(APPEND _problems
            "dispatch entry '${_entry}' (test_runner.cpp gTests[]) has no CTest row -- \
add redship_add_test(NAME <Row> COMMAND redship --test ${_entry}), or declare it \
row-less with redship_test_exempt(${_entry} \"<why>\")")
    endif()
endforeach()

foreach(_entry IN LISTS _found_integration)
    if(NOT _entry IN_LIST _reg_int)
        list(APPEND _problems
            "integration entry '${_entry}' (test_runner.cpp gIntegrationTests[]) has no CTest row")
    endif()
endforeach()

# (2) every registered row must resolve to a real dispatch entry.
#     "all"/"list" are TestRunner_Run special commands, not table rows; they are
#     read out of the same --test list output rather than hardcoded here.
set(_valid_unit_targets ${_found_unit} ${_found_special})
foreach(_dispatch IN LISTS _reg_unit)
    if(NOT _dispatch IN_LIST _valid_unit_targets)
        _row_owning("${_dispatch}" _owner)
        list(APPEND _problems
            "CTest row '${_owner}' runs '--test ${_dispatch}', which is not in the \
dispatch table and would fail with \"Unknown test\"")
    endif()
endforeach()
foreach(_dispatch IN LISTS _reg_int)
    if(NOT _dispatch IN_LIST _found_integration)
        _row_owning("${_dispatch}" _owner)
        list(APPEND _problems
            "CTest row '${_owner}' runs '--integration-test ${_dispatch}', which is not \
in the integration dispatch table")
    endif()
endforeach()

# (3) exemptions must stay honest — a stale one is a hard error, so this list
#     cannot quietly become a rubber stamp (same rule as
#     .github/clang-format-paths.txt).
foreach(_e IN LISTS _exempt)
    if(NOT _e IN_LIST _found_unit)
        list(APPEND _problems
            "stale exemption: redship_test_exempt(${_e}) names a dispatch entry that no \
longer exists -- drop the exemption")
    elseif(_e IN_LIST _reg_unit)
        list(APPEND _problems
            "needless exemption: '${_e}' now has its own CTest row -- drop the \
redship_test_exempt(${_e}) line")
    endif()
endforeach()

if(_problems)
    list(LENGTH _problems _n)
    string(REPLACE ";" "\n  * " _pretty "${_problems}")
    message(FATAL_ERROR
        "Test registration is incomplete (${_n} problem(s)):\n  * ${_pretty}\n\n"
        "Registration lives in CMake/SingleExecutable.cmake (redship_add_test) and "
        "src/common/test_runner.cpp (gTests[]/gIntegrationTests[]). Both sides must agree.")
endif()

list(LENGTH _found_unit _n_unit)
list(LENGTH _reg_unit _n_reg)
list(LENGTH _found_integration _n_int)
list(LENGTH _used_exempt _n_ex)
list(LENGTH _meta_rows _n_meta)
message(STATUS
    "Test registration complete: ${_n_unit} dispatch entries covered by ${_n_reg} "
    "--test rows (${_n_ex} exempt), ${_n_int} integration entries, ${_n_meta} meta row(s).")
