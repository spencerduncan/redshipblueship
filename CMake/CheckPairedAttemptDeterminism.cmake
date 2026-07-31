# CMake/CheckPairedAttemptDeterminism.cmake
#
# ADR 0010 increment 1.2, lock (b), two-process half: the paired MM ATTEMPT
# LADDER is deterministic. The mm-paired-attempt dispatch generates the paired
# MM world for a pinned master seed with one deterministic ladder rung injected
# (kLadderMasterSeed, games/mm/2s2h/mm_rando_gen_test.cpp) and writes a digest
# of (winning attempt, final seed, placement hash, every foreign placement) to
# RSBS_ATTEMPT_DIGEST_OUT. Running it twice in two FRESH processes and diffing
# proves the ladder re-converges on the identical world via the identical
# derivation — the "same seed + settings must produce identical worlds" rule
# with the attempt index included, which is exactly what the recipe's
# counter-as-hash-input discipline promises.
#
# Two separate process invocations, not one, for the same reason
# CheckSeedDeterminism gives: re-entering the generator inside one process is
# an unverified path, and a fresh process per run is the trustworthy
# comparison. Each run writes to its OWN digest path so the diff can never
# compare a file against itself.
#
# Run as a CTest row in the "rando" tier (under xvfb-run):
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -P CheckPairedAttemptDeterminism.cmake
# The display-free env (SDL_AUDIODRIVER / RSBS_DISABLE_OTR_INIT) comes from the
# row's CTest ENVIRONMENT and is inherited by both child runs; -E env only ADDS
# RSBS_ATTEMPT_DIGEST_OUT on top.

if(NOT DEFINED REDSHIP_EXE)
    message(FATAL_ERROR "CheckPairedAttemptDeterminism: -DREDSHIP_EXE=<path> is required")
endif()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckPairedAttemptDeterminism: redship binary not found: ${REDSHIP_EXE}")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "CheckPairedAttemptDeterminism: -DWORK_DIR=<dir> is required (where the digests are written)")
endif()

set(_digest1 "${WORK_DIR}/paired-attempt-run1.txt")
set(_digest2 "${WORK_DIR}/paired-attempt-run2.txt")
# A stale digest from a previous invocation must never be mistaken for this
# run's output — a missing file after a "successful" run is then an unambiguous
# error.
file(REMOVE "${_digest1}" "${_digest2}")

function(_run_attempt_gen out_path label)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "RSBS_ATTEMPT_DIGEST_OUT=${out_path}"
                "${REDSHIP_EXE}" --test mm-paired-attempt
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "CheckPairedAttemptDeterminism: ${label} run of '--test mm-paired-attempt' exited ${_rc} "
            "(the injected ladder rung no longer produces exactly one retry, or the winning attempt no longer "
            "converges — see kLadderMasterSeed in games/mm/2s2h/mm_rando_gen_test.cpp).\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
    if(NOT EXISTS "${out_path}")
        message(FATAL_ERROR
            "CheckPairedAttemptDeterminism: ${label} run reported success but wrote no digest at ${out_path}.\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
endfunction()

_run_attempt_gen("${_digest1}" "first")
_run_attempt_gen("${_digest2}" "second")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${_digest1}" "${_digest2}"
    RESULT_VARIABLE _cmp
)
if(NOT _cmp EQUAL 0)
    file(READ "${_digest1}" _d1)
    file(READ "${_digest2}" _d2)
    message(FATAL_ERROR
        "CheckPairedAttemptDeterminism: the SAME pinned master seed produced DIFFERENT worlds (or converged on a "
        "different ladder attempt) across two runs — the attempt ladder consumed state the documented derivation "
        "does not name.\n"
        "run1:\n${_d1}\nrun2:\n${_d2}")
endif()

file(READ "${_digest1}" _digest)
message(STATUS "Paired attempt-ladder determinism verified — two runs agree:\n${_digest}")
