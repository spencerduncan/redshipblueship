# CMake/CheckSeedDeterminism.cmake
#
# Lane B unified-seed determinism lock (Phase 3.0). Proves that generating the
# SAME rando seed under the SAME pinned settings profile TWICE produces a
# byte-identical world — the first output-EQUALITY test in the "rando" tier (the
# existing RandoGen rows only assert generation-succeeds).
#
# Why two SEPARATE process invocations, not one: re-entering the 3drando
# generator (Settings::CreateOptions / SetAllToContext) within a single process
# has never been exercised, so a fresh process per run is the trustworthy
# comparison rather than an unverified re-entry. Each run writes its digest (the
# unified-seed fields plus a placement hash computed in-memory in fixed
# RandomizerCheck order) to its OWN path via RSBS_SEED_DIGEST_OUT, so the
# same-seed spoiler-log OVERWRITE — both runs land on the same
# Randomizer/<hash>.json — can never make the diff compare a file against itself.
#
# Determinism scope: the headless harness drives generation with EMPTY
# excluded-location / enabled-trick sets, so this locks reproducibility for the
# pinned settings profile only; exclusion/trick-driven fills are not covered.
#
# Run as a CTest row in the "rando" tier (under xvfb-run):
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -P CheckSeedDeterminism.cmake
# The pinned settings profile (RSBS_DIAG_CVARS) and the display-free env
# (SDL_AUDIODRIVER / RSBS_DISABLE_OTR_INIT) come from the row's CTest ENVIRONMENT
# and are inherited by both child runs; -E env only ADDS RSBS_SEED_DIGEST_OUT on
# top, so DISPLAY (from xvfb-run) and the pinned profile pass through unchanged.

if(NOT DEFINED REDSHIP_EXE)
    message(FATAL_ERROR "CheckSeedDeterminism: -DREDSHIP_EXE=<path> is required")
endif()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckSeedDeterminism: redship binary not found: ${REDSHIP_EXE}")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "CheckSeedDeterminism: -DWORK_DIR=<dir> is required (where the digests are written)")
endif()

set(_digest1 "${WORK_DIR}/seed-determinism-run1.txt")
set(_digest2 "${WORK_DIR}/seed-determinism-run2.txt")
# A stale digest from a previous invocation must never be mistaken for this run's
# output — a missing file after a "successful" run is then an unambiguous error.
file(REMOVE "${_digest1}" "${_digest2}")

function(_run_gen out_path label)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "RSBS_SEED_DIGEST_OUT=${out_path}"
                "${REDSHIP_EXE}" --test rando-determinism
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "CheckSeedDeterminism: ${label} run of '--test rando-determinism' exited ${_rc} "
            "(generation failed or the live producer did not stamp gComboCtx).\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
    if(NOT EXISTS "${out_path}")
        message(FATAL_ERROR
            "CheckSeedDeterminism: ${label} run reported success but wrote no digest at ${out_path}.\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
endfunction()

_run_gen("${_digest1}" "first")
_run_gen("${_digest2}" "second")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${_digest1}" "${_digest2}"
    RESULT_VARIABLE _cmp
)
if(NOT _cmp EQUAL 0)
    file(READ "${_digest1}" _d1)
    file(READ "${_digest2}" _d2)
    message(FATAL_ERROR
        "CheckSeedDeterminism: the SAME seed produced DIFFERENT worlds across two runs — "
        "rando generation is not deterministic under the pinned settings profile.\n"
        "run1:\n${_d1}\nrun2:\n${_d2}")
endif()

file(READ "${_digest1}" _digest)
message(STATUS "Seed determinism verified — two same-seed runs agree:\n${_digest}")
