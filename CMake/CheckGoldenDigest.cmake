# CMake/CheckGoldenDigest.cmake
#
# THE GOLDEN ORACLE (#688). The pre-existing determinism rows — SeedDeterminism,
# RandoDeterminism, MMPairedAttemptDeterminism — run one seed TWICE and diff the
# two runs against each other. That proves REPRODUCIBILITY and nothing else: a
# change that moves every placement deterministically passes all three. Yet every
# ADR 0010/0011 acceptance bar is phrased "the pinned determinism digests do not
# move", and three code comments claimed such pins existed. They did not; this
# script is them.
#
# WHAT IT DOES. Run the digest dispatch ONCE and compare its output against a
# digest text checked into `tests/golden/`. A mismatch fails the row with a
# FIELD-BY-FIELD diff, because "foreignOoTHash 3F2A1B07 -> 91CC04DE" is not a
# reviewable failure and "the digests moved" is not a review. Fields are reported
# as INPUT (the seed and the settings fingerprints — moving one means the PROFILE
# changed, so the world was asked a different question) or OUTPUT (everything the
# fill decided — moving one means the world CHANGED under the same question),
# because the two demand different reviews.
#
# WHY A SEPARATE ROW RATHER THAN FOLDING THIS INTO CheckSeedDeterminism. The
# self-diff rows still catch something this one cannot (nondeterminism: two runs
# disagreeing), and this one catches what they cannot (a moved world). Neither
# subsumes the other, so both exist, and this one runs the generation ONCE rather
# than twice.
#
# CROSS-PLATFORM. The golden is resolved as, in order:
#   tests/golden/<NAME>.<host-system>.txt   (Windows/Linux/Darwin — per-platform)
#   tests/golden/<NAME>.txt                 (portable, the normal case)
# A per-platform file exists ONLY where the digest was MEASURED to differ between
# platforms, and docs/determinism-goldens.md must say which field differs and why.
# Do not add one to make a row go green.
#
# Comparison is line-by-line after CR stripping, never a byte compare: the digest
# writers use stdio text mode, so the same world writes LF on Linux and CRLF on
# Windows. A byte compare would report every field as moved on one of the two
# platforms and teach everyone to distrust the row.
#
# THE ARCHIVE SET IS PART OF THE PIN (SKIP_IF_ROM_ARCHIVES), and that is a
# measurement, not a precaution. The OoT seed digest moves when the ROM-DERIVED
# archives (oot.o2r / mm.o2r) are mounted beside the binary, for a reason that has
# nothing to do with the platform: with them present, the 2,449 per-area
# exclude-location options (option groups RSG_EXCLUDES_KOKIRI_FOREST ..
# RSG_EXCLUDES_GANONS_CASTLE) contribute NOTHING to the settings string
# Playthrough_Init hashes, and without them all 2,449 do. Every other option is
# character-identical between the two runs — measured by dumping the per-option
# text both ways, 3087 lines against 638, with the 638 shared lines byte-equal.
# Since the fill is re-seeded with Hash(seed + settingsStr), that difference moves
# the entire world.
#
# Hosted CI can never have ROM-derived archives, so the goldens pin the
# ARCHIVE-FREE world and a ROM-staged local run is a DIFFERENT, unpinned world.
# Such a run SKIPS this row rather than failing it: a red row there would say
# "you moved the world" when nothing moved, and a permanently red row in the
# operator's local merge gate is worse than no row at all.
# The mm-paired-attempt digest is NOT archive-sensitive (measured: a ROM-staged
# Windows golden passed unchanged on archive-free Linux CI), so its row carries no
# such guard.
#
# WHICH GATES ACTUALLY RUN THESE ROWS — the Linux CI leg and the Windows CI leg,
# and the wording here is careful because it was wrong once in each direction. The
# original text claimed "every PR's Linux and Windows CI legs" while only Linux ran
# them: all three rows carry `LABEL rando`, and the Windows job runs
# `ctest --label-regex '^redship$'`. The obvious correction was to write down that
# Windows CANNOT run them, on the reasoning that every `rando` row brings up a
# Fast3dWindow and a hosted runner's OpenGL is the GDI generic 1.1 implementation.
# That reasoning was never measured, and it is wrong: the three rows run and pass on
# windows-latest in 5.4s (run 35648094332, job 106493621321). So the Windows job now
# carries a `--tests-regex '^Golden'` step and both legs check the SAME committed
# bytes, which is what makes cross-platform agreement a thing CI re-verifies rather
# than folklore (docs/determinism-goldens.md, "Platform portability").
#
# Still true and still worth knowing: the rows are selected by NAME on Windows, not
# by label — the `rando` tier as a whole remains untried there — and the two
# archive-sensitive rows are NOT enforced in the operator's ROM-staged local run
# (see the SKIP above). Two CI gates, no local gate.
#
# Usage (see the rows in CMake/SingleExecutable.cmake):
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DDISPATCH=rando-determinism
#         -DDIGEST_ENV=RSBS_SEED_DIGEST_OUT -DGOLDEN_DIR=<repo>/tests/golden
#         -DGOLDEN_NAME=seed-digest-default [-DSKIP_IF_ROM_ARCHIVES=ON] [-DREGEN=ON]
#         -P CheckGoldenDigest.cmake
# REGEN=ON writes the generated digest over the golden instead of comparing. It is
# the ONLY supported way to re-pin — see docs/determinism-goldens.md and the
# `regen-golden-digests` target. A REGEN of an archive-SENSITIVE golden is REFUSED
# while oot.o2r/mm.o2r sit in WORK_DIR: that re-pin would silently record a world
# CI cannot reproduce and turn the Linux leg red for this PR and every PR after it,
# and "the machinery leaves this to a human" is exactly the kind of prose guarantee
# #688 exists to disbelieve. -DALLOW_ROM_ARCHIVE_REGEN=ON is the deliberate
# override for an author who really means it.

foreach(_required REDSHIP_EXE WORK_DIR DISPATCH DIGEST_ENV GOLDEN_DIR GOLDEN_NAME)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckGoldenDigest: -D${_required}=<value> is required")
    endif()
endforeach()
if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckGoldenDigest: redship binary not found: ${REDSHIP_EXE}")
endif()

# ----------------------------------------------------------------------------
# The archive-environment guard (see the header). Checked BEFORE generating, so a
# ROM-staged run does not spend a generation to reach a verdict it cannot give.
#
# It guards BOTH directions, because the two failure modes are not symmetric:
#   - COMPARE in a ROM-staged tree would be red about a move that did not happen,
#     so it SKIPS. The row's SKIP_REGULAR_EXPRESSION matches the marker below, so
#     CTest reports SKIPPED with this reason rather than PASSED — a silent pass
#     here would be the same vacuity #688 is about.
#   - REGEN in a ROM-staged tree writes a world CI can never reproduce and is
#     REFUSED. That direction is the dangerous one and it used to be unguarded:
#     the one documented re-pin command pins its cwd to ${CMAKE_BINARY_DIR}, which
#     in the operator's tree IS the ROM-staged directory, the local rando tier
#     then SKIPS the very rows that would object, and the damage first appears as
#     a red Linux leg on this PR and on every PR after it. A human-only rule was
#     the whole of the defense; now it is a check, with an explicit override.
# ----------------------------------------------------------------------------
if(SKIP_IF_ROM_ARCHIVES)
    set(_rom_archives "")
    foreach(_rom oot.o2r mm.o2r)
        if(EXISTS "${WORK_DIR}/${_rom}")
            list(APPEND _rom_archives "${_rom}")
        endif()
    endforeach()
    if(_rom_archives)
        string(REPLACE ";" ", " _rom_list "${_rom_archives}")
        if(REGEN AND ALLOW_ROM_ARCHIVE_REGEN)
            message(WARNING
                "CheckGoldenDigest(${GOLDEN_NAME}): re-pinning an ARCHIVE-SENSITIVE golden with ${_rom_list} mounted "
                "in ${WORK_DIR}, because -DALLOW_ROM_ARCHIVE_REGEN=ON was passed.\n"
                "  The resulting golden pins the ROM-MOUNTED world, which hosted CI cannot reproduce: the Linux leg "
                "will go red on this PR and on every PR after it until the golden is re-pinned archive-free. Only do "
                "this if that is genuinely what you meant.")
        elseif(REGEN)
            message(FATAL_ERROR
                "CheckGoldenDigest(${GOLDEN_NAME}): RE-PIN REFUSED — this golden is ARCHIVE-SENSITIVE and the "
                "ROM-derived archive(s) ${_rom_list} are present in ${WORK_DIR}.\n"
                "  Move these two files out of that directory and re-run the target:\n"
                "      ${WORK_DIR}/oot.o2r\n"
                "      ${WORK_DIR}/mm.o2r\n"
                "  Why this is refused rather than warned about: with oot.o2r mounted the per-area exclude-location "
                "option groups drop out of the settings string Playthrough_Init hashes, the fill is re-seeded "
                "differently, and the whole OoT world moves. A golden re-pinned here therefore pins a world hosted CI "
                "can NEVER reproduce — and because the local rando tier SKIPS these same rows in a ROM-staged tree, "
                "nothing local would object; the first symptom would be a red Linux leg on this PR and on every PR "
                "after it.\n"
                "  If you really mean to pin the ROM-mounted world, pass -DALLOW_ROM_ARCHIVE_REGEN=ON. See "
                "docs/determinism-goldens.md.")
        else()
            message(STATUS
                "RSBS_GOLDEN_SKIP: CheckGoldenDigest(${GOLDEN_NAME}) needs an ARCHIVE-FREE run and found "
                "${_rom_list} in ${WORK_DIR}.\n"
                "  The goldens pin the world generated with the PORT archives only, because that is the world hosted "
                "CI can reproduce (a runner never has ROM-derived archives). With oot.o2r mounted the per-area "
                "exclude-location option groups drop out of the settings string Playthrough_Init hashes, the fill is "
                "re-seeded differently, and this run generates a DIFFERENT, unpinned world — so comparing it to the "
                "golden would report a move that did not happen.\n"
                "  This row IS enforced on every PR, by both archive-free CI legs: Linux runs it as part of the "
                "`rando` tier under xvfb-run, and Windows runs it by name in its own `--tests-regex '^Golden'` step. "
                "It is not enforced HERE, in a ROM-staged tree. To run it here, move oot.o2r and mm.o2r out of "
                "${WORK_DIR} first. See docs/determinism-goldens.md.")
            return()
        endif()
    endif()
endif()

# ----------------------------------------------------------------------------
# Generate. One process, one run, its own output path.
# ----------------------------------------------------------------------------
set(_actual "${WORK_DIR}/golden-${GOLDEN_NAME}-actual.txt")
# A stale artifact from a previous invocation must never be mistaken for this
# run's output, so "the run succeeded but wrote nothing" stays an unambiguous
# error rather than a silent pass against week-old bytes.
file(REMOVE "${_actual}")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "${DIGEST_ENV}=${_actual}"
            "${REDSHIP_EXE}" --test ${DISPATCH}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "CheckGoldenDigest(${GOLDEN_NAME}): '--test ${DISPATCH}' exited ${_rc} — generation failed before any "
        "comparison could happen, so this row says NOTHING about whether the world moved.\n"
        "stdout:\n${_out}\nstderr:\n${_err}")
endif()
if(NOT EXISTS "${_actual}")
    message(FATAL_ERROR
        "CheckGoldenDigest(${GOLDEN_NAME}): '--test ${DISPATCH}' reported success but wrote no digest at "
        "${_actual} (is ${DIGEST_ENV} the right variable for this dispatch?).\n"
        "stdout:\n${_out}\nstderr:\n${_err}")
endif()

# ----------------------------------------------------------------------------
# Normalise: CR-stripped, blank-free, ordered list of `key=value` lines.
# ----------------------------------------------------------------------------
function(_digest_lines path out_var)
    file(READ "${path}" _raw)
    string(REPLACE "\r" "" _raw "${_raw}")
    string(REGEX REPLACE "\n+$" "" _raw "${_raw}")
    string(REPLACE "\n" ";" _lines "${_raw}")
    set(${out_var} "${_lines}" PARENT_SCOPE)
endfunction()

_digest_lines("${_actual}" _actual_lines)

# The digest as the CI LOG will carry it. Requirement, not decoration: the Linux
# leg's digest is only knowable from its log, and "are Windows and Linux the same
# world?" is a question about two logs. Printed before any comparison so it is
# present even when the row fails.
string(REPLACE ";" "\n  " _pretty "${_actual_lines}")
message(STATUS "[golden-digest] ${GOLDEN_NAME} host=${CMAKE_HOST_SYSTEM_NAME} dispatch=${DISPATCH}\n  ${_pretty}")

# ----------------------------------------------------------------------------
# Resolve the golden: per-platform file first, portable file second.
# ----------------------------------------------------------------------------
set(_golden_platform "${GOLDEN_DIR}/${GOLDEN_NAME}.${CMAKE_HOST_SYSTEM_NAME}.txt")
set(_golden_portable "${GOLDEN_DIR}/${GOLDEN_NAME}.txt")
if(EXISTS "${_golden_platform}")
    set(_golden "${_golden_platform}")
    set(_golden_kind "per-platform (${CMAKE_HOST_SYSTEM_NAME})")
else()
    set(_golden "${_golden_portable}")
    set(_golden_kind "portable")
endif()

if(REGEN)
    get_filename_component(_golden_dir "${_golden}" DIRECTORY)
    file(MAKE_DIRECTORY "${_golden_dir}")
    # Write through _digest_lines' normalisation rather than copying the raw
    # file, so a stray blank line or a free-text line cannot reach a golden.
    # Line endings are NOT guaranteed here — CMake's file(WRITE) still lands CRLF
    # on Windows (measured) — and deliberately do not have to be: the comparison
    # above strips CR before diffing, and .gitattributes' `* text=auto eol=lf`
    # normalises the committed bytes, so a golden re-pinned on Windows and one
    # re-pinned on Linux produce the same blob and the same reviewable diff.
    string(REPLACE ";" "\n" _normalised "${_actual_lines}")
    file(WRITE "${_golden}" "${_normalised}\n")
    message(STATUS
        "CheckGoldenDigest(${GOLDEN_NAME}): RE-PINNED ${_golden_kind} golden ${_golden}.\n"
        "  The DIFF of that file is the review artifact. Commit it with a body that states which fields moved and "
        "why the new world is the intended one (docs/determinism-goldens.md).")
    return()
endif()

if(NOT EXISTS "${_golden}")
    message(FATAL_ERROR
        "CheckGoldenDigest(${GOLDEN_NAME}): no golden digest at ${_golden_portable} "
        "(nor a ${CMAKE_HOST_SYSTEM_NAME} override at ${_golden_platform}).\n"
        "A missing golden is a FAILURE, not a free pass: a row that silently pins nothing is the exact defect #688 "
        "was filed about. Generate it with the documented command (docs/determinism-goldens.md) and commit it.\n"
        "This run produced:\n${_pretty}")
endif()

_digest_lines("${_golden}" _golden_lines)

# ----------------------------------------------------------------------------
# Compare, field by field.
#
# INPUTS are the question the fill was asked; OUTPUTS are the answer it gave. A
# moved input means somebody changed the pinned profile or the pinned seed (or a
# settings default, or the canonical form the fingerprint hashes) — review the
# profile. A moved output with every input intact means THE GENERATED WORLD
# CHANGED — review the world. Anything not named here is treated as an output,
# so a field added to a digest later defaults to the strict reading.
# ----------------------------------------------------------------------------
set(_input_fields seed settingsHash sourceIsRando comboSettingsHash comboSettings ladderMasterSeed)

function(_digest_keys lines out_keys prefix)
    set(_keys "")
    foreach(_line IN LISTS lines)
        if(_line MATCHES "^([A-Za-z0-9_]+)=(.*)$")
            list(APPEND _keys "${CMAKE_MATCH_1}")
            set(${prefix}${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR
                "CheckGoldenDigest(${GOLDEN_NAME}): digest line is not `key=value`: '${_line}'. The golden format is "
                "one field per line; a writer that emits free text cannot be diffed field by field.")
        endif()
    endforeach()
    set(${out_keys} "${_keys}" PARENT_SCOPE)
endfunction()

_digest_keys("${_golden_lines}" _gkeys "_g_")
_digest_keys("${_actual_lines}" _akeys "_a_")

set(_moved_outputs "")
set(_moved_inputs "")
set(_missing "")
set(_added "")

foreach(_key IN LISTS _gkeys)
    if(_key IN_LIST _akeys)
        if(NOT "${_a_${_key}}" STREQUAL "${_g_${_key}}")
            if(_key IN_LIST _input_fields)
                list(APPEND _moved_inputs "    ${_key}: golden '${_g_${_key}}' -> this build '${_a_${_key}}'")
            else()
                list(APPEND _moved_outputs "    ${_key}: golden '${_g_${_key}}' -> this build '${_a_${_key}}'")
            endif()
        endif()
    else()
        list(APPEND _missing "    ${_key} (was '${_g_${_key}}')")
    endif()
endforeach()
foreach(_key IN LISTS _akeys)
    if(NOT _key IN_LIST _gkeys)
        list(APPEND _added "    ${_key} = '${_a_${_key}}'")
    endif()
endforeach()

if(_moved_outputs OR _moved_inputs OR _missing OR _added)
    set(_report "")
    if(_moved_outputs)
        string(REPLACE ";" "\n" _block "${_moved_outputs}")
        string(APPEND _report
            "\n  OUTPUT FIELDS MOVED — the generated world is DIFFERENT under the same pinned seed and profile:\n"
            "${_block}\n")
    endif()
    if(_moved_inputs)
        string(REPLACE ";" "\n" _block "${_moved_inputs}")
        string(APPEND _report
            "\n  INPUT FIELDS MOVED — the fill was asked a DIFFERENT question (pinned seed, pinned settings profile, "
            "a settings default, or the canonical form a fingerprint hashes):\n"
            "${_block}\n")
    endif()
    if(_missing)
        string(REPLACE ";" "\n" _block "${_missing}")
        string(APPEND _report "\n  FIELDS THE GOLDEN HAS AND THIS BUILD DID NOT EMIT:\n${_block}\n")
    endif()
    if(_added)
        string(REPLACE ";" "\n" _block "${_added}")
        string(APPEND _report "\n  FIELDS THIS BUILD EMITTED THAT THE GOLDEN DOES NOT HAVE:\n${_block}\n")
    endif()
    message(FATAL_ERROR
        "CheckGoldenDigest(${GOLDEN_NAME}): this build does not reproduce the pinned world.\n"
        "  golden: ${_golden} [${_golden_kind}]\n"
        "  actual: ${_actual}\n"
        "  host:   ${CMAKE_HOST_SYSTEM_NAME}"
        "${_report}"
        "\n  IF THE MOVE IS INTENDED, re-pin DELIBERATELY — the golden's diff is the review artifact:\n"
        "      cmake --build <build-dir> --target regen-golden-digests\n"
        "  and commit the changed file(s) stating which fields moved and why (docs/determinism-goldens.md).\n"
        "  IF IT IS NOT INTENDED, this row is the bug report: a change in this PR moved the world.\n"
        "  Note: the self-diff rows (SeedDeterminism / MMPairedAttemptDeterminism) stay GREEN through a moved "
        "world by construction — they only compare two runs of THIS binary to each other. Their being green is "
        "not evidence against this failure.")
endif()

list(LENGTH _gkeys _field_count)
message(STATUS
    "CheckGoldenDigest(${GOLDEN_NAME}): ${_field_count} field(s) match the ${_golden_kind} golden "
    "${_golden} on ${CMAKE_HOST_SYSTEM_NAME} — the pinned world did not move.")
