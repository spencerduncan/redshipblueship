# CMake/RedshipTests.cmake
# Test registration helpers for the single-executable build.
#
# Why this module exists (#376): registering one test used to take three edits,
# one of which was a SHARED LINE. `add_test(NAME ...)` appends cleanly, but the
# test's name also had to be appended to a single five-line
# `set_tests_properties(<30 names> PROPERTIES TIMEOUT ... LABELS "redship")`
# block. Every parallel lane that added a test rewrote those same lines, so
# every lane after the first conflicted — four Wave 1 lanes and both Wave 2
# lanes collided there over 2026-07-19/20.
#
# The conflict is worse than a rebase: GitHub builds `pull_request` checks
# against `refs/pull/N/merge`, which it cannot create while a PR conflicts, so a
# stale PR gets ZERO check runs rather than failing ones — indistinguishable
# from an Actions outage until someone looks at the mergeability flag.
#
# `redship_add_test()` folds the properties into the registration, so a test is
# one appended line and the shared name list is gone.

include_guard(GLOBAL)

# ============================================================================
# redship_add_test(NAME <name> COMMAND <argv...>
#                  [LABEL <label>...] [TIMEOUT <seconds>] [ENVIRONMENT <var=val>...])
#
# Registers a CTest row and its properties together.
#
# LABEL defaults to "redship" (the ROM-free tier hosted CI runs) and TIMEOUT
# defaults to ${REDSHIP_TEST_TIMEOUT}, so a plain unit test needs neither.
# ============================================================================
function(redship_add_test)
    cmake_parse_arguments(PARSE_ARGV 0 RSBS_T "" "NAME;TIMEOUT" "COMMAND;LABEL;ENVIRONMENT")

    if(NOT RSBS_T_NAME)
        message(FATAL_ERROR "redship_add_test: NAME is required")
    endif()
    if(NOT RSBS_T_COMMAND)
        message(FATAL_ERROR "redship_add_test(${RSBS_T_NAME}): COMMAND is required")
    endif()
    # A typo'd keyword would otherwise be silently swallowed and the row would
    # quietly get default properties — the exact failure class this module exists
    # to remove.
    if(RSBS_T_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "redship_add_test(${RSBS_T_NAME}): unrecognized argument(s): ${RSBS_T_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT RSBS_T_LABEL)
        set(RSBS_T_LABEL "redship")
    endif()
    if(NOT RSBS_T_TIMEOUT)
        if(NOT DEFINED REDSHIP_TEST_TIMEOUT)
            message(FATAL_ERROR
                "redship_add_test(${RSBS_T_NAME}): no TIMEOUT given and REDSHIP_TEST_TIMEOUT is not set yet")
        endif()
        set(RSBS_T_TIMEOUT ${REDSHIP_TEST_TIMEOUT})
    endif()

    # A caller writing ENVIRONMENT "A=1;B=2" hands cmake_parse_arguments ONE
    # argument, which it stores with the semicolons escaped ("A=1\;B=2") to keep
    # it a single list element. Forwarded as-is, CTest receives one malformed
    # variable named A whose value is "1;B=2" instead of two variables — a
    # silent environment regression in a tier that only runs under xvfb. Undo the
    # escaping so both spellings (one ;-joined string, or several separate
    # arguments) produce the same list.
    string(REPLACE "\\;" ";" _labels "${RSBS_T_LABEL}")
    string(REPLACE "\\;" ";" _environment "${RSBS_T_ENVIRONMENT}")

    add_test(NAME ${RSBS_T_NAME} COMMAND ${RSBS_T_COMMAND})
    set_tests_properties(${RSBS_T_NAME} PROPERTIES
        TIMEOUT "${RSBS_T_TIMEOUT}"
        LABELS "${_labels}"
    )
    if(RSBS_T_ENVIRONMENT)
        set_tests_properties(${RSBS_T_NAME} PROPERTIES ENVIRONMENT "${_environment}")
    endif()

    # Record which dispatch-table entry this row drives, for the completeness
    # guard below. The CTest row name is NOT the dispatch name: RandoGen,
    # RandoGenSongsDungeonRewards and RandoGenSongsAnywhere are three rows over
    # the one "rando-gen" entry, differing only by ENVIRONMENT.
    set(_kind "meta")
    set(_dispatch "")
    set(_take_next FALSE)
    foreach(_arg IN LISTS RSBS_T_COMMAND)
        if(_take_next)
            set(_dispatch "${_arg}")
            set(_take_next FALSE)
        elseif(_arg STREQUAL "--test")
            set(_kind "unit")
            set(_take_next TRUE)
        elseif(_arg STREQUAL "--integration-test")
            set(_kind "integration")
            set(_take_next TRUE)
        endif()
    endforeach()

    # Manifest fields are '|'-separated and one-per-line, so flatten any
    # multi-label value rather than letting its ';' split the line.
    string(REPLACE ";" "," _manifest_labels "${_labels}")
    set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST
        "register|${_manifest_labels}|${RSBS_T_NAME}|${_kind}|${_dispatch}")
endfunction()

# ============================================================================
# redship_test_exempt(<dispatch-name> <reason>)
#
# Declares a dispatch-table entry that deliberately has NO dedicated CTest row.
# The guard hard-fails on a stale exemption (name gone from the table, or since
# registered), so this list cannot rot into a rubber stamp — same rule the
# clang-format path allowlist uses.
# ============================================================================
function(redship_test_exempt name reason)
    set_property(GLOBAL APPEND PROPERTY REDSHIP_TEST_MANIFEST "exempt|${name}|${reason}")
endfunction()

# ============================================================================
# redship_check_test_sources()
#
# Every src/common/tests/*.c must be #included by test_runner.cpp.
#
# These files are deliberately NOT separate translation units: each is pulled
# into test_runner.cpp either inside an `extern "C"` block or at file scope,
# and which one is load-bearing (test_roundtrip_integrity.c must compile as C++
# or its Entrance_Init call binds to OoT's C-linkage randomizer symbol instead
# of the combo entrance system). Compiling them as their own TUs as well would
# define every symbol twice.
#
# So this globs to DETECT rather than to build: CONFIGURE_DEPENDS re-runs
# configure when a file is added to the directory, and a file that is never
# #included is a test that compiles into nothing and can never run.
# ============================================================================
function(redship_check_test_sources)
    file(GLOB _test_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/common/tests/*.c")

    # A glob that matches nothing would make this check pass vacuously, which is
    # the failure mode it exists to prevent.
    list(LENGTH _test_sources _count)
    if(_count EQUAL 0)
        message(FATAL_ERROR
            "redship_check_test_sources: src/common/tests/*.c matched no files. "
            "If the test sources moved, update this glob — do not leave it matching nothing.")
    endif()

    file(READ "${CMAKE_SOURCE_DIR}/src/common/test_runner.cpp" _runner)
    set(_orphans "")
    foreach(_src IN LISTS _test_sources)
        get_filename_component(_name "${_src}" NAME)
        string(FIND "${_runner}" "#include \"tests/${_name}\"" _pos)
        if(_pos EQUAL -1)
            list(APPEND _orphans "${_name}")
        endif()
    endforeach()

    if(_orphans)
        string(REPLACE ";" "\n  - " _pretty "${_orphans}")
        message(FATAL_ERROR
            "Test source(s) in src/common/tests/ are not #included by "
            "src/common/test_runner.cpp:\n  - ${_pretty}\n"
            "They compile into nothing and can never run. Add "
            "#include \"tests/<file>\" to test_runner.cpp (inside an extern \"C\" block "
            "if the test only touches C-linkage symbols; at file scope if it drives "
            "C++-linkage APIs).")
    endif()

    message(STATUS "Test sources: ${_count} file(s) in src/common/tests/, all #included")
endfunction()

# ============================================================================
# redship_finalize_tests()
#
# Writes the manifest the completeness guard reads. Call AFTER every
# redship_add_test()/redship_test_exempt() call.
# ============================================================================
function(redship_finalize_tests manifest_path)
    get_property(_entries GLOBAL PROPERTY REDSHIP_TEST_MANIFEST)
    if(NOT _entries)
        message(FATAL_ERROR "redship_finalize_tests: no tests were registered")
    endif()
    string(REPLACE ";" "\n" _body "${_entries}")
    file(WRITE "${manifest_path}"
        "# Generated by redship_finalize_tests() — do not edit.\n"
        "# register|<labels>|<ctest-name>|<kind>|<dispatch-name>\n"
        "# exempt|<dispatch-name>|<reason>\n"
        "${_body}\n")
endfunction()
