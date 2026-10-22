# CMake/CheckTestAudioEnv.cmake
#
# S0 lock: every CTest row carries SDL_AUDIODRIVER= in its ENVIRONMENT, so a test
# process starts with SDL's audio on the dummy driver whatever the caller's shell
# or the staged config say. redship_add_test() (CMake/RedshipTests.cmake) puts the
# entry on every row it registers; this reads the registered rows back through
# `ctest --show-only=json-v1` and fails on any that lack it, which catches both the
# PREPEND line being deleted and a row registered with a bare add_test().
#
# The --integration-test rows are the ones that used to lack the variable, and they
# need ROMs, so this ROM-free check is what holds them in hosted CI. (The in-code
# half, TestWindowFocus_ArmSilentAudio, is held by the integration probe: see
# rsbs/src/main.cpp.)
#
#   cmake -DCTEST_EXE=<ctest> -DBUILD_DIR=<build dir> [-DEXEMPT="<row>;<row>"]
#         -P CheckTestAudioEnv.cmake
#
# EXEMPT names rows that deliberately run no test process of ours (libultraship's own
# `prism` row, which runs no redship). A name that is not a registered row in this
# tree is only warned about, since the exempt rows come from other projects.

foreach(_required CTEST_EXE BUILD_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckTestAudioEnv: -D${_required}=<value> is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CTEST_EXE}" --test-dir "${BUILD_DIR}" --show-only=json-v1
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _json
    ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "CheckTestAudioEnv: `ctest --show-only=json-v1` exited ${_rc}:\n${_err}")
endif()

string(JSON _count LENGTH "${_json}" tests)
if(_count LESS 20)
    message(FATAL_ERROR
        "CheckTestAudioEnv: only ${_count} rows listed; the check would be vacuous. "
        "Is BUILD_DIR '${BUILD_DIR}' the configured build tree?")
endif()

set(_missing "")
set(_seen_exempt "")
set(_checked 0)
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    string(JSON _name GET "${_json}" tests ${_i} name)
    if(_name IN_LIST EXEMPT)
        list(APPEND _seen_exempt "${_name}")
        continue()
    endif()
    math(EXPR _checked "${_checked} + 1")
    set(_has FALSE)
    string(JSON _nprops LENGTH "${_json}" tests ${_i} properties)
    if(_nprops GREATER 0)
        math(EXPR _plast "${_nprops} - 1")
        foreach(_p RANGE ${_plast})
            string(JSON _pname GET "${_json}" tests ${_i} properties ${_p} name)
            if(_pname STREQUAL "ENVIRONMENT")
                string(JSON _nenv LENGTH "${_json}" tests ${_i} properties ${_p} value)
                if(_nenv GREATER 0)
                    math(EXPR _elast "${_nenv} - 1")
                    foreach(_e RANGE ${_elast})
                        string(JSON _entry GET "${_json}" tests ${_i} properties ${_p} value ${_e})
                        if(_entry MATCHES "^SDL_AUDIODRIVER=.+")
                            set(_has TRUE)
                        endif()
                    endforeach()
                endif()
            endif()
        endforeach()
    endif()
    if(NOT _has)
        list(APPEND _missing "${_name}")
    endif()
endforeach()

foreach(_x IN LISTS EXEMPT)
    if(NOT _x IN_LIST _seen_exempt)
        message(WARNING "CheckTestAudioEnv: EXEMPT names '${_x}', which is not a registered row here.")
    endif()
endforeach()

if(_missing)
    list(LENGTH _missing _nm)
    string(REPLACE ";" "\n  - " _pretty "${_missing}")
    message(FATAL_ERROR
        "CheckTestAudioEnv: ${_nm} of ${_checked} CTest rows have no SDL_AUDIODRIVER in their "
        "ENVIRONMENT, so a run of them depends on the caller's shell or the staged config to be "
        "silent:\n  - ${_pretty}\n"
        "Register rows with redship_add_test() (CMake/RedshipTests.cmake adds it), or add the "
        "row to EXEMPT in CMake/SingleExecutable.cmake with a reason.")
endif()
message(STATUS "CheckTestAudioEnv: all ${_checked} rows carry SDL_AUDIODRIVER (${_count} listed)")
