# CMake/CheckHandRunDigest.cmake
#
# #710 lock, over the REAL digest dispatches: a hand-run can never be mistaken for
# a fresh artifact.
#
# The trap it closes. `redship --test rando-determinism` digests to STDOUT unless
# RSBS_SEED_DIGEST_OUT names a file, and only the CTest rows set that variable. So
# after a hand-run the seed-determinism-run1.txt on disk is still whatever the last
# ctest run wrote, with a plausible mtime, size and contents, and a "main vs
# branch" comparison of it compares one stale file to itself and reports no
# difference (PR #703's first revision made two wrong sensitivity claims that way).
# mm-paired-attempt / RSBS_ATTEMPT_DIGEST_OUT behaves the same way.
#
# What this row asserts, each in a private directory nothing else writes to:
#   U1 rando-determinism with RSBS_SEED_DIGEST_OUT unset exits 0, prints the
#      one-line notice naming the variable and the rows that write a file (at the
#      start and again at the end), prints the digest (placementHash=) on stdout,
#      leaves a planted STALE seed-determinism-run1.txt byte-identical, and
#      creates no new .txt file there;
#   S1 the same dispatch with RSBS_SEED_DIGEST_OUT set writes that file, with the
#      digest in it, and prints no notice;
#   U2 mm-paired-attempt with RSBS_ATTEMPT_DIGEST_OUT unset: the same as U1 for
#      its variable and its paired-attempt-run1.txt.
# The display-free half (the resolver itself, the empty-string case, and the
# source check that every digest dispatch goes through it) is the DigestOutHandRun
# row in the redship tier.
#
# Run as a CTest row in the "rando" tier (under xvfb-run on Linux):
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -P CheckHandRunDigest.cmake
# SDL_AUDIODRIVER / RSBS_DISABLE_OTR_INIT / RSBS_DIAG_CVARS come from the row's
# CTest ENVIRONMENT and pass through; `cmake -E env --unset=` removes only the two
# digest variables, so a caller that exported one cannot turn U1/U2 into a set run.

if(NOT DEFINED REDSHIP_EXE)
    message(FATAL_ERROR "CheckHandRunDigest: -DREDSHIP_EXE=<path> is required")
endif()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckHandRunDigest: redship binary not found: ${REDSHIP_EXE}")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "CheckHandRunDigest: -DWORK_DIR=<dir> is required")
endif()

set(_dir "${WORK_DIR}/hand-run-digest")
file(REMOVE_RECURSE "${_dir}")
file(MAKE_DIRECTORY "${_dir}")
# The run happens IN the private directory so "no new file appeared" is a claim
# about a directory nothing else writes to. The archives are still found: the
# resource locator searches the executable's own directory too. The config comes
# along so the window backend is the one the tree is configured for.
if(EXISTS "${WORK_DIR}/shipofharkinian.json")
    file(COPY "${WORK_DIR}/shipofharkinian.json" DESTINATION "${_dir}")
endif()

set(_stale_text "STALE-SENTINEL written by CheckHandRunDigest before the hand-run; a digest run must not touch it\n")

# Text files only: a run legitimately creates its own config, imgui.ini, logs and
# spoiler directories in its working directory; every digest writer writes .txt.
function(_top_files out_var)
    file(GLOB _all LIST_DIRECTORIES false RELATIVE "${_dir}" "${_dir}/*.txt")
    list(SORT _all)
    set(${out_var} "${_all}" PARENT_SCOPE)
endfunction()

# Runs one dispatch with its digest variable unset and asserts the whole U contract.
function(_hand_run label dispatch env_var stale_name)
    set(_stale "${_dir}/${stale_name}")
    file(WRITE "${_stale}" "${_stale_text}")
    _top_files(_before)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env --unset=RSBS_SEED_DIGEST_OUT --unset=RSBS_ATTEMPT_DIGEST_OUT
                "${REDSHIP_EXE}" --test ${dispatch}
        WORKING_DIRECTORY "${_dir}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "CheckHandRunDigest ${label}: '--test ${dispatch}' with ${env_var} unset exited ${_rc}, "
                            "so this row says nothing about what a hand-run writes.\nstdout:\n${_out}\nstderr:\n${_err}")
    endif()
    string(CONCAT _notice "[digest-out] NOTICE: ${env_var} is unset, so '--test ${dispatch}' prints its digest to "
                          "STDOUT and writes NO digest file")
    string(FIND "${_out}" "${_notice}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "CheckHandRunDigest ${label}: '--test ${dispatch}' ran with ${env_var} unset and printed "
                            "no notice that it wrote NO digest file. A hand-runner comparing ${stale_name} would "
                            "compare an earlier run's file and not know it (#710). Expected a line starting:\n"
                            "  ${_notice}\nstdout:\n${_out}")
    endif()
    string(FIND "${_out}" "${_notice}" _last REVERSE)
    if(_last EQUAL _at)
        message(FATAL_ERROR "CheckHandRunDigest ${label}: the notice was printed once; it must be repeated when the "
                            "dispatch returns so it is the last thing a hand-runner sees after the generation log.")
    endif()
    file(READ "${_stale}" _after_text)
    if(NOT _after_text STREQUAL _stale_text)
        message(FATAL_ERROR "CheckHandRunDigest ${label}: the planted stale ${stale_name} changed during a run with "
                            "${env_var} unset; the notice says no file is written.\nnow:\n${_after_text}")
    endif()
    _top_files(_after)
    if(NOT _after STREQUAL _before)
        message(FATAL_ERROR "CheckHandRunDigest ${label}: a run with ${env_var} unset created .txt file(s) in its "
                            "working directory.\nbefore: ${_before}\nafter:  ${_after}")
    endif()
    set(_hand_run_out "${_out}" PARENT_SCOPE)
    message(STATUS "CheckHandRunDigest ${label}: '--test ${dispatch}' with ${env_var} unset printed the notice twice, "
                   "wrote no file, and left the stale ${stale_name} untouched")
endfunction()

# U1
_hand_run("U1" rando-determinism RSBS_SEED_DIGEST_OUT seed-determinism-run1.txt)
string(FIND "${_hand_run_out}" "placementHash=" _digest_at)
if(_digest_at EQUAL -1)
    message(FATAL_ERROR "CheckHandRunDigest U1: the unset run printed no digest (placementHash=) on stdout; the notice "
                        "says the digest went there.\nstdout:\n${_hand_run_out}")
endif()

# S1
set(_set_path "${_dir}/hand-run-set.txt")
file(REMOVE "${_set_path}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env --unset=RSBS_ATTEMPT_DIGEST_OUT "RSBS_SEED_DIGEST_OUT=${_set_path}"
            "${REDSHIP_EXE}" --test rando-determinism
    WORKING_DIRECTORY "${_dir}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "CheckHandRunDigest S1: '--test rando-determinism' with RSBS_SEED_DIGEST_OUT set exited "
                        "${_rc}.\nstdout:\n${_out}\nstderr:\n${_err}")
endif()
if(NOT EXISTS "${_set_path}")
    message(FATAL_ERROR "CheckHandRunDigest S1: RSBS_SEED_DIGEST_OUT=${_set_path} but no file was written.\n"
                        "stdout:\n${_out}")
endif()
file(READ "${_set_path}" _set_text)
string(FIND "${_set_text}" "placementHash=" _digest_at)
if(_digest_at EQUAL -1)
    message(FATAL_ERROR "CheckHandRunDigest S1: ${_set_path} holds no placementHash= line:\n${_set_text}")
endif()
string(FIND "${_out}" "[digest-out] NOTICE:" _at)
if(NOT _at EQUAL -1)
    message(FATAL_ERROR "CheckHandRunDigest S1: RSBS_SEED_DIGEST_OUT was set and the run still printed the "
                        "no-file notice, which would train people to ignore it.\nstdout:\n${_out}")
endif()
message(STATUS "CheckHandRunDigest S1: with RSBS_SEED_DIGEST_OUT set the digest file was written and no notice printed")

# U2
_hand_run("U2" mm-paired-attempt RSBS_ATTEMPT_DIGEST_OUT paired-attempt-run1.txt)

file(REMOVE_RECURSE "${_dir}")
message(STATUS "Hand-run digest honesty verified: an unset digest variable is announced and writes nothing; a set "
               "one writes the file.")
