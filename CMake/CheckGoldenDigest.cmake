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
# THE ARCHIVE SET IS PART OF THE PIN (ARCHIVE_FREE_ONLY). Hosted CI can never have
# ROM-derived archives, so the goldens pin the ARCHIVE-FREE world. Until #702 the
# ROM-mounted world was a different one: with oot.o2r mounted the SoH menu reached
# Settings::CreateOptions() before the per-area exclude-location lists were filled,
# the 32 RSG_EXCLUDES_* groups copied empty lists, and the settings string
# Playthrough_Init hashes folded 638 option lines instead of 3087 — a different
# fill seed and a different world from the same binary. Since #702 both
# environments fold the same set, so a ROM-staged COMPARE runs the dispatch in BOTH
# (the sandbox below, and the build directory with the ROM archives mounted) and
# requires both digests to equal the golden and each other; see "THE SECOND
# ENVIRONMENT" further down. ARCHIVE_FREE_ONLY keeps its name from when the two
# worlds differed; it now means "pinned archive-free, and proven equal ROM-mounted
# wherever both environments exist".
#
# A ROM-STAGED RUN THEREFORE BUILDS AN ARCHIVE-FREE SANDBOX RATHER THAN SKIPPING
# (the review follow-up to #688). Before it, these rows ALWAYS skipped in the
# operator's ROM-staged tree — the gate this project's policy calls the merge gate
# — so the two seed rows had neither a green half nor a red half locally, and the
# skip was the only thing anyone had looked for. The binary resolves every archive
# through Ship::Context::LocateFileAcrossAppDirs, which searches the app-config dir
# (the cwd, in a portable build), then THE EXECUTABLE'S OWN DIRECTORY, then "./",
# so changing the working directory is not enough on its own: the executable has to
# live in the sandbox too. `_archive_free_sandbox` below hard-links the binary and
# an ALLOWLIST of port archives (soh.o2r / 2ship.o2r / redship-oot.o2r /
# redship-mm.o2r) into
# ${WORK_DIR}/golden-archive-free/<NAME>/ and runs the dispatch there. An allowlist,
# not a denylist, so a ROM-derived archive this file has never heard of cannot leak
# in by being unlisted.
#
# WHAT THE SANDBOX ASSERTS, and what it used to only claim. Because the sandbox is
# wiped and then populated from that allowlist, re-scanning it afterwards for
# ROM-derived names — the "the defining property is asserted, not assumed" check the
# first version of this carried — could not fail for any input: dead code where a
# safety property was advertised. The checks that CAN fail are the two the code now
# makes: the port set is RESOLVED from ${WORK_DIR} and from the executable's own
# directory and an empty result is a refusal (a sandbox holding the binary alone
# generates a no-archive world and would fail the row about a move that never
# happened), and $SHIP_HOME — the one directory the loader probes that the allowlist
# cannot control — is refused when a ROM-derived archive sits in it. Each placement
# is checked as it is made, so reaching the end of the population loop means every
# resolved file is in the sandbox. If the sandbox cannot be built, the row FAILS: a
# broken harness is a finding, not a skip (see the guard below).
#
# REGEN DELIBERATELY DOES NOT USE THE SANDBOX — see the refusal below. The two
# directions fail differently: a sandbox that is subtly wrong turns a COMPARE row
# RED and a human reads the field diff, but would make a REGEN silently pin a world
# nobody asked for, which is the exact accident the refusal exists to prevent.
#
# WHICH GATES ACTUALLY RUN THESE ROWS — both CI legs, and (since the sandbox) the
# operator's local ROM-staged run too. The wording here is careful because it was
# wrong once in each direction. The original text claimed "every PR's Linux and
# Windows CI legs" while only Linux ran them: all three rows carry `LABEL rando`,
# and the Windows job runs `ctest --label-regex '^redship$'`. The obvious correction
# was to write down that Windows CANNOT run them, on the reasoning that every
# `rando` row brings up a Fast3dWindow and a hosted runner's OpenGL is the GDI
# generic 1.1 implementation. That reasoning was never measured, and it is wrong:
# the three rows run and pass on windows-latest in 5.4s (run 35648094332, job
# 106493621321), and so, measured next (#709), does the whole `rando` tier: 28/28
# on two attempts of PR #723's run. The Windows job now runs `--label-regex
# '^rando$'` like Linux, both legs check the SAME committed bytes, and that is what
# makes cross-platform agreement a thing CI re-verifies rather than folklore
# (docs/determinism-goldens.md, "Platform portability").
#
# Usage (see the rows in CMake/SingleExecutable.cmake):
#   cmake -DREDSHIP_EXE=<redship> -DWORK_DIR=<dir> -DDISPATCH=rando-determinism
#         -DDIGEST_ENV=RSBS_SEED_DIGEST_OUT -DGOLDEN_DIR=<repo>/tests/golden
#         -DGOLDEN_NAME=seed-digest-default [-DARCHIVE_FREE_ONLY=ON]
#         [-DREGEN=ON -DALLOW_ROM_ARCHIVE_REGEN=ON|OFF]
#         -P CheckGoldenDigest.cmake
# REGEN=ON writes the generated digest over the golden instead of comparing. It is
# the ONLY supported way to re-pin — see docs/determinism-goldens.md and the
# `regen-golden-digests` target. A REGEN of an archive-SENSITIVE golden is REFUSED
# while oot.o2r/mm.o2r sit in WORK_DIR: that re-pin would silently record a world
# CI cannot reproduce and turn the Linux leg red for this PR and every PR after it,
# and "the machinery leaves this to a human" is exactly the kind of prose guarantee
# #688 exists to disbelieve. -DALLOW_ROM_ARCHIVE_REGEN=ON is the deliberate
# override for an author who really means it, and every REGEN caller must pass the
# variable one way or the other (see the contract check below) — the first version
# of this script documented the override in four places while no caller forwarded
# it, so it could not be reached at all.

foreach(_required REDSHIP_EXE WORK_DIR DISPATCH DIGEST_ENV GOLDEN_DIR GOLDEN_NAME)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckGoldenDigest: -D${_required}=<value> is required")
    endif()
endforeach()

# The REGEN caller contract, checked before anything expensive so a caller that
# gets it wrong learns immediately and without a binary. `cmake -P` has no cache
# and imports no environment, so a variable this script reads is reachable ONLY if
# the caller puts it on the command line: an override the callers do not forward is
# not an override, it is prose. Requiring it to be DEFINED (either value) makes a
# new REGEN caller that forgets fail loudly instead of inheriting the refusal
# silently — which is how -DALLOW_ROM_ARCHIVE_REGEN=ON came to be documented in
# four places while being unreachable through the one documented re-pin command.
if(REGEN AND NOT DEFINED ALLOW_ROM_ARCHIVE_REGEN)
    message(FATAL_ERROR
        "CheckGoldenDigest(${GOLDEN_NAME}): a REGEN caller must pass -DALLOW_ROM_ARCHIVE_REGEN=ON or =OFF.\n"
        "  It is the documented override for the ROM-staged re-pin refusal below, and `cmake -P` cannot read it "
        "from a cache or the environment — an unforwarded override is unreachable. The re-pin targets generated in "
        "CMake/SingleExecutable.cmake forward it: `regen-golden-digests` passes OFF, "
        "`regen-golden-digests-rom-mounted` passes ON.")
endif()

if(NOT EXISTS "${REDSHIP_EXE}")
    message(FATAL_ERROR "CheckGoldenDigest: redship binary not found: ${REDSHIP_EXE}")
endif()

# ROM-derived archives: the set whose presence beside the binary changes the world
# an archive-sensitive golden pins. Port archives ship with the build and are part
# of every environment, hosted CI included.
set(_rom_archive_names oot.o2r oot-mq.o2r mm.o2r mm.otr mm.zip)
set(_port_archive_names soh.o2r 2ship.o2r redship-oot.o2r redship-mm.o2r)

# ----------------------------------------------------------------------------
# Build an archive-free sandbox: a directory holding the binary and the PORT
# archives only, so a run whose cwd and whose executable both live in it resolves
# no ROM-derived archive anywhere. Hard links where the filesystem allows them
# (the binary is ~68 MB and this runs per row, per ctest invocation), a copy
# otherwise. Returns the directory, the executable to run and the port archives it
# actually holds through out_dir / out_exe / out_ports, or leaves them empty and
# explains why in out_reason.
#
# WHAT TRAVELS INTO THE SANDBOX, exhaustively: the binary, whichever of the four
# PORT archives the source environment actually has, and `shipofharkinian.json`.
# WHAT DELIBERATELY DOES NOT: `mods/` (resolved through the same
# LocateFileAcrossAppDirs probe list since #670/#704, so it IS load-bearing for the
# resource set), `assets/`, `gamecontrollerdb.txt`, `imgui.ini`, `Randomizer/`,
# `randomizer-mm/`, `Save/`, and any previous run's output. Dropping them is the
# intended reading of these goldens — they pin the world a hosted runner can
# reproduce and a runner has none of those — but it is NOT the same statement as
# "the archive set is the only difference from a run in the build directory", which
# is what this comment used to make and which any local tree with mods staged
# falsifies. A golden that covers a modded resource set would be a new golden with
# its own pinned inputs, not this sandbox.
# ----------------------------------------------------------------------------
function(_archive_free_sandbox work_dir exe golden_name rom_names port_names
                               out_dir out_exe out_ports out_reason)
    set(${out_dir} "" PARENT_SCOPE)
    set(${out_exe} "" PARENT_SCOPE)
    set(${out_ports} "" PARENT_SCOPE)
    set(${out_reason} "" PARENT_SCOPE)

    string(REPLACE ";" ", " _port_list "${port_names}")

    set(_dir "${work_dir}/golden-archive-free/${golden_name}")
    # Rebuilt from scratch every run: a stale link to a previous build's binary,
    # or a file somebody dropped in by hand, would make this row report on an
    # environment nobody chose.
    file(REMOVE_RECURSE "${_dir}")
    # No "if it does not exist, return a reason" check here, and that is measured
    # rather than assumed: `file(MAKE_DIRECTORY)` without the RESULT keyword (which
    # this project's 3.26 floor predates) emits a FATAL error of its own when it
    # cannot create the path — verified by putting a regular file where
    # golden-archive-free/ has to be, which aborts the script at this line. So a
    # reason branch here could never run, and the row goes red through CMake's own
    # message either way.
    file(MAKE_DIRECTORY "${_dir}")

    get_filename_component(_exe_name "${exe}" NAME)
    get_filename_component(_exe_dir "${exe}" DIRECTORY)

    # WHERE THE PORT ARCHIVES COME FROM: ${work_dir} first, then the EXECUTABLE'S
    # OWN DIRECTORY — the two places the loader itself looks — because they are not
    # always the same directory. A multi-config MSVC build puts
    # $<TARGET_FILE:redship> in <build>/<Config>/ while WORK_DIR is <build>, and a
    # tree where the ROM archives were staged before the archive-generating target
    # ran has them split the same way. The first version of this function resolved
    # from ${work_dir} only, with no else branch and no post-condition: in such a
    # tree it built a sandbox holding the BINARY ALONE, announced it as "binary and
    # port archives only", generated a world with no archives mounted at all and
    # failed the row about a move that never happened — the same false red the old
    # SKIP existed to avoid, now under a message asserting the environment was
    # correct. So which port archives this environment HAS is measured here, and an
    # empty answer is a refusal rather than a quiet sandbox.
    set(_want_ports "")
    set(_port_sources "")
    foreach(_port IN LISTS port_names)
        if(EXISTS "${work_dir}/${_port}")
            list(APPEND _want_ports "${_port}")
            list(APPEND _port_sources "${work_dir}/${_port}")
        elseif(EXISTS "${_exe_dir}/${_port}")
            list(APPEND _want_ports "${_port}")
            list(APPEND _port_sources "${_exe_dir}/${_port}")
        endif()
    endforeach()
    if(NOT _want_ports)
        # string(CONCAT) rather than several arguments to set(): set() with more than
        # one value makes a LIST, and the reason then reaches the failure message with
        # a literal `;` at every line break.
        string(CONCAT _reason
            "no port archive (${_port_list}) was found in ${work_dir} or in the binary's own directory ${_exe_dir}, so "
            "the sandbox would hold the binary alone and generate a world with NO archives mounted — which is not the "
            "world these goldens pin (they pin the PORT-archive world hosted CI runs). Build the archive targets, or "
            "stage the port archives beside the binary, and re-run")
        set(${out_reason} "${_reason}" PARENT_SCOPE)
        return()
    endif()

    set(_sources "${exe}")
    set(_targets "${_dir}/${_exe_name}")
    list(APPEND _sources ${_port_sources})
    foreach(_port IN LISTS _want_ports)
        list(APPEND _targets "${_dir}/${_port}")
    endforeach()

    # Every placement is checked HERE, which is what makes the sandbox's contents
    # an assertion rather than a hope: this loop returns a reason for the first
    # file that did not land, so reaching its end means the binary and every one of
    # ${_want_ports} is in ${_dir}.
    list(LENGTH _sources _count)
    math(EXPR _last "${_count} - 1")
    foreach(_i RANGE ${_last})
        list(GET _sources ${_i} _src)
        list(GET _targets ${_i} _dst)
        file(CREATE_LINK "${_src}" "${_dst}" RESULT _link_result COPY_ON_ERROR)
        if(NOT _link_result STREQUAL "0" OR NOT EXISTS "${_dst}")
            set(${out_reason} "could not place ${_src} in the sandbox: ${_link_result}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # The window backend and every other CVar the local tree is configured with
    # travel into the sandbox. Copied rather than linked: the run rewrites its
    # config on exit, and a hard link would write that back into the build
    # directory's own config. See this function's header for what does and does not
    # travel — this is not "everything except the archives".
    if(EXISTS "${work_dir}/shipofharkinian.json")
        file(COPY_FILE "${work_dir}/shipofharkinian.json" "${_dir}/shipofharkinian.json" ONLY_IF_DIFFERENT)
    endif()

    # THE ONE LEAK THE ALLOWLIST CANNOT PREVENT. Everything placed above comes from
    # a fixed allowlist into a directory that was just wiped, so re-scanning the
    # sandbox for ROM-derived names — which is what this spot used to do — could not
    # fail for any input: it was dead code standing in for the safety property,
    # while the property that CAN be violated went unchecked.
    #
    # It is this one. The binary resolves an archive through
    # Ship::Context::LocateFileAcrossAppDirs: the app-CONFIG directory first, then
    # the executable's own directory, then "./". The last two are the sandbox. The
    # first is "." in a portable build — the sandbox again — EXCEPT that
    # libultraship reads $SHIP_HOME instead when it is set
    # (libultraship/src/ship/Context.cpp, the __linux__/__APPLE__ branches of
    # GetAppDirectoryPath), so a ROM-derived archive sitting in $SHIP_HOME is
    # resolved BEFORE the sandbox's own directory and the sandbox is not
    # archive-free at all. Checked host-independently, and therefore conservative on
    # Windows where the port ignores SHIP_HOME: a ROM archive in a SHIP_HOME this
    # row cannot rule out makes the row say so rather than pin an environment it
    # cannot describe.
    #
    # A NON_PORTABLE build (libultraship's option, OFF by default and OFF in every
    # configuration this repo uses) resolves the app-config dir through SDL's pref
    # path, which this script cannot compute; such a build is outside what the
    # sandbox claims.
    if(DEFINED ENV{SHIP_HOME})
        foreach(_rom IN LISTS rom_names)
            if(EXISTS "$ENV{SHIP_HOME}/${_rom}")
                string(CONCAT _reason
                    "SHIP_HOME=$ENV{SHIP_HOME} contains ${_rom}, and the loader searches the app-config directory "
                    "BEFORE the executable's own directory, so that ROM-derived archive would be mounted from inside "
                    "the sandbox — the sandbox would not be archive-free. Move it out of SHIP_HOME, or unset SHIP_HOME "
                    "for this run")
                set(${out_reason} "${_reason}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endif()

    string(REPLACE ";" ", " _got_ports "${_want_ports}")
    set(${out_dir} "${_dir}" PARENT_SCOPE)
    set(${out_exe} "${_dir}/${_exe_name}" PARENT_SCOPE)
    set(${out_ports} "${_got_ports}" PARENT_SCOPE)
endfunction()

# ----------------------------------------------------------------------------
# The archive-environment guard (see the header). Checked BEFORE generating, so a
# ROM-staged run does not spend a generation to reach a verdict it cannot give.
#
# It guards BOTH directions, because the two failure modes are not symmetric:
#   - COMPARE in a ROM-staged tree would be red about a move that did not happen,
#     so it runs in an archive-free SANDBOX instead, and FAILS when that sandbox
#     cannot be built. There is no skip path any more: these rows are green or red
#     everywhere. The skip this replaced carried the wrong justification — "a
#     COMPARE here would be red about a move that did not happen" is an argument
#     about the ROM-staged tree, not about a sandbox that could not be built, and
#     the latter is a harness fault whose fix is named in the failure message. It
#     also left the local gate one silent step from the zero-coverage state #688 is
#     about, with prose asking a human to read the skip reason as the only thing in
#     the way.
#   - REGEN in a ROM-staged tree writes a world CI can never reproduce and is
#     REFUSED. That direction is the dangerous one and it used to be unguarded:
#     the one documented re-pin command pins its cwd to ${CMAKE_BINARY_DIR}, which
#     in the operator's tree IS the ROM-staged directory, the local rando tier
#     then SKIPPED the very rows that would object, and the damage first appears as
#     a red Linux leg on this PR and on every PR after it. A human-only rule was
#     the whole of the defense; now it is a check, with an explicit override.
# ----------------------------------------------------------------------------
set(_run_dir "${WORK_DIR}")
set(_run_exe "${REDSHIP_EXE}")
set(_run_env "work-dir")
# Which ROM-derived archives WORK_DIR holds, measured whatever ARCHIVE_FREE_ONLY
# says: the closing STATUS line of an OFF row has to say whether its single run was
# ROM-mounted, and it used to claim "no ROM-derived archive is staged here" without
# looking.
set(_rom_archives "")
foreach(_rom IN LISTS _rom_archive_names)
    if(EXISTS "${WORK_DIR}/${_rom}")
        list(APPEND _rom_archives "${_rom}")
    endif()
endforeach()
string(REPLACE ";" ", " _rom_list "${_rom_archives}")
if(ARCHIVE_FREE_ONLY)
    if(_rom_archives)
        if(REGEN AND ALLOW_ROM_ARCHIVE_REGEN)
            message(WARNING
                "CheckGoldenDigest(${GOLDEN_NAME}): re-pinning an archive-free golden with ${_rom_list} mounted "
                "in ${WORK_DIR}, because -DALLOW_ROM_ARCHIVE_REGEN=ON was passed.\n"
                "  The resulting golden pins the ROM-MOUNTED world. Since #702 that should be the archive-free world "
                "too; if it is not, hosted CI cannot reproduce it and both CI legs go red on this PR and on every PR "
                "after it until the golden is re-pinned archive-free. Only do this if that is genuinely what you "
                "meant.")
        elseif(REGEN)
            message(FATAL_ERROR
                "CheckGoldenDigest(${GOLDEN_NAME}): RE-PIN REFUSED — this golden is pinned ARCHIVE-FREE and the "
                "ROM-derived archive(s) ${_rom_list} are present in ${WORK_DIR}.\n"
                "  Move these files out of that directory and re-run the target:\n"
                "      ${WORK_DIR}/oot.o2r\n"
                "      ${WORK_DIR}/mm.o2r\n"
                "  Why this is refused rather than warned about: a re-pin records ONE run, and the only environment "
                "hosted CI can reproduce is the archive-free one. Since #702 the ROM-mounted world should be the same "
                "world, but that is what the COMPARE rows establish, not something a re-pin may assume: until #702 a "
                "golden re-pinned here pinned a different world (the exclude-location option groups dropped out of "
                "the settings string), and the first symptom was a red Linux leg on that PR and every PR after it.\n"
                "  The COMPARE direction does NOT refuse: it runs the dispatch in an archive-free sandbox under "
                "${WORK_DIR}/golden-archive-free/. REGEN deliberately does not use that sandbox — a sandbox that is "
                "subtly wrong turns a COMPARE row red and a human reads the field diff, but would make a re-pin "
                "silently record the wrong world, which is what this refusal is for.\n"
                "  If you really mean to pin the ROM-mounted world, build the "
                "`regen-golden-digests-rom-mounted` target (it passes -DALLOW_ROM_ARCHIVE_REGEN=ON). See "
                "docs/determinism-goldens.md.")
        else()
            _archive_free_sandbox("${WORK_DIR}" "${REDSHIP_EXE}" "${GOLDEN_NAME}"
                                  "${_rom_archive_names}" "${_port_archive_names}"
                                  _sandbox_dir _sandbox_exe _sandbox_ports _sandbox_reason)
            if(_sandbox_dir)
                set(_run_dir "${_sandbox_dir}")
                set(_run_exe "${_sandbox_exe}")
                set(_run_env "archive-free-sandbox")
                message(STATUS
                    "CheckGoldenDigest(${GOLDEN_NAME}): ${_rom_list} are present in ${WORK_DIR} — running the "
                    "dispatch in the archive-free sandbox ${_sandbox_dir} (the binary plus the port archives "
                    "${_sandbox_ports}, and shipofharkinian.json; NOT mods/, assets/ or any other build-directory "
                    "content — see CheckGoldenDigest.cmake's _archive_free_sandbox header) AND in ${WORK_DIR} with "
                    "those archives mounted, and comparing BOTH to the golden and to each other (#702: one golden "
                    "covers both environments).")
            else()
                # A BROKEN HARNESS IS RED, NOT SKIPPED. This used to emit an
                # `RSBS_GOLDEN_SKIP:` marker that the row's SKIP_REGULAR_EXPRESSION
                # turned into SKIPPED with exit 0 — which put the local merge gate
                # one silent step away from the zero-coverage state this whole
                # mechanism exists to leave behind, and inherited the justification
                # for a DIFFERENT condition: skipping was defensible when the
                # alternative was a COMPARE in a ROM-staged tree going red about a
                # move that did not happen, and it is not defensible for "the
                # sandbox could not be built", which is a harness fault with a
                # one-line fix. So it fails, and the message carries both ways out.
                message(FATAL_ERROR
                    "CheckGoldenDigest(${GOLDEN_NAME}): needs an ARCHIVE-FREE run, found ${_rom_list} in ${WORK_DIR}, "
                    "and could not build the archive-free sandbox — ${_sandbox_reason}.\n"
                    "  THIS ROW SAYS NOTHING ABOUT WHETHER THE WORLD MOVED. It is failing on the harness, not on a "
                    "digest: no generation ran. It does not skip, because a SKIPPED row here would leave this gate "
                    "with neither a green half nor a red half for the golden, which is the vacuity #688 was filed "
                    "about.\n"
                    "  Two ways out, both real:\n"
                    "    1. fix the sandbox — the reason above names what could not be done; or\n"
                    "    2. move oot.o2r and mm.o2r out of ${WORK_DIR} and re-run, which needs no sandbox at all:\n"
                    "         ${WORK_DIR}/oot.o2r\n"
                    "         ${WORK_DIR}/mm.o2r\n"
                    "  Why the sandbox exists: the goldens pin the world generated with the PORT archives only, "
                    "because that is the world hosted CI can reproduce (a runner never has ROM-derived archives), and "
                    "a ROM-staged run checks that world AND the ROM-mounted one, requiring them to be equal (#702). "
                    "See docs/determinism-goldens.md.")
            endif()
        endif()
    endif()
endif()

# ----------------------------------------------------------------------------
# Generate. One process per environment, each with its own output path.
# ----------------------------------------------------------------------------
# A stale artifact from a previous invocation must never be mistaken for this
# run's output, so "the run succeeded but wrote nothing" stays an unambiguous
# error rather than a silent pass against week-old bytes.
#
# WORKING_DIRECTORY is explicit even in the plain case, where it is the value this
# row inherited from ctest anyway: the sandbox above is only archive-free because
# the run happens INSIDE it, so "where did this run happen" must not be an
# inherited accident.
function(_golden_generate run_dir run_exe run_env actual)
    file(REMOVE "${actual}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "${DIGEST_ENV}=${actual}"
                "${run_exe}" --test ${DISPATCH}
        WORKING_DIRECTORY "${run_dir}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "CheckGoldenDigest(${GOLDEN_NAME}): '--test ${DISPATCH}' exited ${_rc} — generation failed before any "
            "comparison could happen, so this row says NOTHING about whether the world moved.\n"
            "  ran: ${run_exe}\n  in:  ${run_dir} [${run_env}]\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
    if(NOT EXISTS "${actual}")
        message(FATAL_ERROR
            "CheckGoldenDigest(${GOLDEN_NAME}): '--test ${DISPATCH}' reported success but wrote no digest at "
            "${actual} (is ${DIGEST_ENV} the right variable for this dispatch?).\n"
            "  ran: ${run_exe}\n  in:  ${run_dir} [${run_env}]\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
endfunction()

# ----------------------------------------------------------------------------
# Normalise: CR-stripped, ordered list of lines. Blank lines and free text are
# NOT dropped here — they are refused by _digest_keys below, in both directions,
# because a digest writer that emits them is a bug to report rather than noise to
# absorb.
# ----------------------------------------------------------------------------
function(_digest_lines path out_var)
    file(READ "${path}" _raw)
    string(REPLACE "\r" "" _raw "${_raw}")
    string(REGEX REPLACE "\n+$" "" _raw "${_raw}")
    string(REPLACE "\n" ";" _lines "${_raw}")
    set(${out_var} "${_lines}" PARENT_SCOPE)
endfunction()

# Split `key=value` lines into a key list plus one <prefix><key> variable each,
# and FATAL_ERROR on any line that is not `key=value` — including an empty one,
# which is what an interior blank line becomes. Defined here, above BOTH callers,
# because the REGEN branch validates through it before it writes: a blank or
# free-text line pinned into a golden would make every later COMPARE of that
# golden die in this same function, on a file tests/golden/README.md forbids
# hand-editing.
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

set(_actual "${WORK_DIR}/golden-${GOLDEN_NAME}-actual.txt")
_golden_generate("${_run_dir}" "${_run_exe}" "${_run_env}" "${_actual}")
_digest_lines("${_actual}" _actual_lines)

# The digest as the CI LOG will carry it. Requirement, not decoration: the Linux
# leg's digest is only knowable from its log, and "are Windows and Linux the same
# world?" is a question about two logs. Printed before any comparison so it is
# present even when the row fails. The environment is part of the line because an
# archive-sensitive digest only means something together with the archive set that
# produced it.
string(REPLACE ";" "\n  " _pretty "${_actual_lines}")
message(STATUS "[golden-digest] ${GOLDEN_NAME} host=${CMAKE_HOST_SYSTEM_NAME} dispatch=${DISPATCH} "
               "env=${_run_env}\n  ${_pretty}")

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
    # VALIDATE before writing, through the same `key=value` rule the comparison
    # applies. _digest_lines normalises CR and trailing newlines and NOTHING else
    # — it does not drop an interior blank line or a free-text line, and a golden
    # carrying one would make every later COMPARE of it die in _digest_keys, red
    # forever, on a file nobody may hand-edit. So the malformed digest is refused
    # at re-pin time, where the writer that produced it is the thing to fix.
    _digest_keys("${_actual_lines}" _regen_keys "_regen_")
    if(NOT _regen_keys)
        message(FATAL_ERROR
            "CheckGoldenDigest(${GOLDEN_NAME}): '--test ${DISPATCH}' produced an EMPTY digest at ${_actual}; "
            "refusing to pin a golden with no fields — it would pass against anything.")
    endif()
    # Line endings are NOT guaranteed here — CMake's file(WRITE) still lands CRLF
    # on Windows (measured) — and deliberately do not have to be: the comparison
    # above strips CR before diffing, and .gitattributes' `* text=auto eol=lf`
    # normalises the committed bytes, so a golden re-pinned on Windows and one
    # re-pinned on Linux produce the same blob and the same reviewable diff.
    string(REPLACE ";" "\n" _normalised "${_actual_lines}")
    file(WRITE "${_golden}" "${_normalised}\n")
    list(LENGTH _regen_keys _regen_field_count)
    message(STATUS
        "CheckGoldenDigest(${GOLDEN_NAME}): RE-PINNED ${_golden_kind} golden ${_golden} "
        "(${_regen_field_count} field(s), generated in ${_run_dir} [${_run_env}]).\n"
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
# THE SECOND ENVIRONMENT (#702): one golden covers both. Until #702 the settings
# string Playthrough_Init hashes depended on which archives were mounted, so the
# ROM-mounted world was a DIFFERENT world from the pinned one and a ROM-staged run
# could only check the archive-free half, in the sandbox above. With the option
# groups built the same way in both environments, the ROM-mounted world IS the
# pinned world, and a ROM-staged run now generates it too — in the build
# directory, with oot.o2r/mm.o2r mounted (the archive set a player has; it is
# still the headless dispatch, with the row's RSBS_DISABLE_OTR_INIT=1 and the
# harness's empty exclude/trick sets, not the file-select Generate path, so it is
# NOT the whole of a player's bring-up) — and compares it against the SAME golden. So in the operator's tree every golden row
# checks both worlds against one file, and additionally against each other, which
# is the cross-environment lock on `settingsHash` (and every other field) that
# #702 asks for: the same binary, run archive-free and ROM-mounted, must write the
# same digest.
#
# Hosted CI has only the archive-free environment, so there this second run does
# not happen and the row says so in its STATUS line; the lock's ROM half is
# enforced wherever ROM archives are staged (the operator's local merge gate).
# ----------------------------------------------------------------------------
set(_rom_run OFF)
if(_run_env STREQUAL "archive-free-sandbox")
    set(_rom_run ON)
    set(_rom_actual "${WORK_DIR}/golden-${GOLDEN_NAME}-actual-rom-mounted.txt")
    _golden_generate("${WORK_DIR}" "${REDSHIP_EXE}" "rom-mounted-work-dir" "${_rom_actual}")
    _digest_lines("${_rom_actual}" _rom_lines)
    string(REPLACE ";" "\n  " _rom_pretty "${_rom_lines}")
    message(STATUS "[golden-digest] ${GOLDEN_NAME} host=${CMAKE_HOST_SYSTEM_NAME} dispatch=${DISPATCH} "
                   "env=rom-mounted-work-dir\n  ${_rom_pretty}")
endif()

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

# Diff `actual_lines` (`actual_label`) against `ref_lines` (`ref_label`) and put a
# field-by-field report into `out_report`, empty when every field matches.
function(_digest_diff ref_lines ref_label actual_lines actual_label out_report)
    _digest_keys("${ref_lines}" _rkeys "_r_")
    _digest_keys("${actual_lines}" _akeys "_a_")
    set(_moved_outputs "")
    set(_moved_inputs "")
    set(_missing "")
    set(_added "")
    foreach(_key IN LISTS _rkeys)
        if(_key IN_LIST _akeys)
            if(NOT "${_a_${_key}}" STREQUAL "${_r_${_key}}")
                if(_key IN_LIST _input_fields)
                    list(APPEND _moved_inputs "    ${_key}: ${ref_label} '${_r_${_key}}' -> ${actual_label} '${_a_${_key}}'")
                else()
                    list(APPEND _moved_outputs "    ${_key}: ${ref_label} '${_r_${_key}}' -> ${actual_label} '${_a_${_key}}'")
                endif()
            endif()
        else()
            list(APPEND _missing "    ${_key} (was '${_r_${_key}}')")
        endif()
    endforeach()
    foreach(_key IN LISTS _akeys)
        if(NOT _key IN_LIST _rkeys)
            list(APPEND _added "    ${_key} = '${_a_${_key}}'")
        endif()
    endforeach()
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
        string(APPEND _report "\n  FIELDS ${ref_label} HAS AND ${actual_label} DID NOT EMIT:\n${_block}\n")
    endif()
    if(_added)
        string(REPLACE ";" "\n" _block "${_added}")
        string(APPEND _report "\n  FIELDS ${actual_label} EMITTED THAT ${ref_label} DOES NOT HAVE:\n${_block}\n")
    endif()
    set(${out_report} "${_report}" PARENT_SCOPE)
endfunction()

_digest_diff("${_golden_lines}" "golden" "${_actual_lines}" "this-build" _report)
set(_cross_report "")
set(_rom_report "")
if(_rom_run)
    _digest_diff("${_golden_lines}" "golden" "${_rom_lines}" "rom-mounted" _rom_report)
    _digest_diff("${_actual_lines}" "archive-free" "${_rom_lines}" "rom-mounted" _cross_report)
endif()

set(_failure "")
if(_report)
    string(APPEND _failure
        "\n  THE PINNED WORLD MOVED — this build does not reproduce the golden in ${_run_dir} [${_run_env}]:"
        "${_report}")
endif()
if(_cross_report)
    # Named separately from a moved golden because it is a different bug, and
    # reported even when the archive-free half matches. It has TWO candidate
    # causes and the message names both: the two runs differ in the archive set
    # AND in everything the sandbox deliberately leaves behind (mods/, assets/ —
    # see _archive_free_sandbox's header), so "#702 regressed" is only one reading.
    # What mods/ holds is listed so a reader can tell which applies without
    # opening the build directory.
    file(GLOB _wd_mods LIST_DIRECTORIES true RELATIVE "${WORK_DIR}" "${WORK_DIR}/mods/*")
    if(_wd_mods)
        string(REPLACE ";" ", " _wd_mods_list "${_wd_mods}")
        set(_mods_note "${WORK_DIR}/mods holds: ${_wd_mods_list}")
    else()
        set(_mods_note "${WORK_DIR}/mods is empty or absent, which points at the archive set")
    endif()
    string(APPEND _failure
        "\n  THE TWO ENVIRONMENTS GENERATED DIFFERENT WORLDS — the same binary wrote a different digest in the build "
        "directory (${WORK_DIR}: ROM archives ${_rom_list} mounted, plus its mods/ and assets/) than in the "
        "archive-free sandbox (${_run_dir}: port archives only). Either #702 regressed (the archive set changes what "
        "the settings string folds again), or build-directory content the sandbox does not carry moved the world "
        "(${_mods_note}):"
        "${_cross_report}")
elseif(_rom_report)
    string(APPEND _failure
        "\n  THE ROM-MOUNTED WORLD does not reproduce the golden in ${WORK_DIR} [rom-mounted-work-dir]:"
        "${_rom_report}")
endif()

if(_failure)
    set(_rom_line "")
    if(_rom_run)
        set(_rom_line "\n  rom-mounted actual: ${_rom_actual}")
    endif()
    message(FATAL_ERROR
        "CheckGoldenDigest(${GOLDEN_NAME}): this build does not reproduce the pinned world in every environment "
        "it ran in.\n"
        "  golden: ${_golden} [${_golden_kind}]\n"
        "  actual: ${_actual} [${_run_env}]"
        "${_rom_line}\n"
        "  host:   ${CMAKE_HOST_SYSTEM_NAME}"
        "${_failure}"
        "\n  IF THE MOVE IS INTENDED, re-pin DELIBERATELY — the golden's diff is the review artifact:\n"
        "      cmake --build <build-dir> --target regen-golden-digests\n"
        "  and commit the changed file(s) stating which fields moved and why (docs/determinism-goldens.md).\n"
        "  IF IT IS NOT INTENDED, this row is the bug report: a change in this PR moved the world.\n"
        "  Note: the self-diff rows (SeedDeterminism / MMPairedAttemptDeterminism) stay GREEN through a moved "
        "world by construction — they only compare two runs of THIS binary to each other. Their being green is "
        "not evidence against this failure.")
endif()

_digest_keys("${_golden_lines}" _gkeys "_g_")
list(LENGTH _gkeys _field_count)
if(_rom_run)
    message(STATUS
        "CheckGoldenDigest(${GOLDEN_NAME}): ${_field_count} field(s) match the ${_golden_kind} golden "
        "${_golden} on ${CMAKE_HOST_SYSTEM_NAME} in BOTH environments — archive-free (${_run_dir}) and ROM-mounted "
        "(${WORK_DIR}) — so the pinned world did not move and the archive set does not change it (#702).")
elseif(_rom_archives)
    # ARCHIVE_FREE_ONLY=OFF with ROM archives staged: the one run WAS ROM-mounted,
    # and no archive-free run happened to compare it with.
    message(STATUS
        "CheckGoldenDigest(${GOLDEN_NAME}): ${_field_count} field(s) match the ${_golden_kind} golden "
        "${_golden} on ${CMAKE_HOST_SYSTEM_NAME} (generated in ${_run_dir} [${_run_env}] with ${_rom_list} mounted) "
        "— the pinned world did not move. This row is ARCHIVE_FREE_ONLY=OFF, so it ran ROM-mounted only and nothing "
        "compared the two environments (#702).")
else()
    message(STATUS
        "CheckGoldenDigest(${GOLDEN_NAME}): ${_field_count} field(s) match the ${_golden_kind} golden "
        "${_golden} on ${CMAKE_HOST_SYSTEM_NAME} (generated in ${_run_dir} [${_run_env}]) — the pinned world did not "
        "move. No ROM-derived archive is staged here, so the ROM-mounted half of this row (#702) did not run.")
endif()
