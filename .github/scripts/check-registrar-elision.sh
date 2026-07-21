#!/usr/bin/env bash
# Post-link guard for #341: translation units whose only effect is a static
# initializer (RegisterShipInitFunc and friends) are silently dropped by plain
# archive linking unless something references a symbol they export. This
# script compares each archive's _GLOBAL__sub_I_* registrar symbols against
# the linked redship binary and fails when a WHOLE_ARCHIVE-protected archive
# lost members anyway (a regression in the link setup).
#
# Symbol ground truth per the PR #340 investigation: nm, not strings — GCC
# inlines short std::string constructions, so strings-based checks give false
# negatives.
#
# MEASUREMENT ARTIFACT FIXED HERE (docs/unified-surface-findings.md §4):
# `_GLOBAL__sub_I_<file>` carries no directory/archive component — it is
# keyed purely on source basename. These symbols have LOCAL (file-static)
# linkage, so when two different TUs across DIFFERENT archives share a
# basename (e.g. games/oot/soh/Enhancements/AudioCollection.cpp and
# games/mm/2s2h/Enhancements/AudioCollection.cpp — 11 such pairs exist
# between soh_enh and 2ship_enh, plus Miscellaneous.cpp between soh_rando
# and 2ship_rando), the linker keeps BOTH as separate, non-colliding symbol
# table entries when both are linked, but a plain "does this name exist
# anywhere in the binary" check cannot tell that apart from "only one side's
# copy survived". Since soh_enh/soh_rando are WHOLE_ARCHIVE'd (always fully
# linked), their copy is always present — so the old set-membership check
# always reported the MM twin as "present" too, whether or not MM's own TU
# actually made it into the link. This inflated 2ship_enh's report-only
# "present" count (26) with up to 11 basenames that were never really MM's.
#
# The fix: track symbol OCCURRENCE COUNTS in the binary, not just presence,
# and attribute them to archives in the fixed order they are checked below
# (required archives first). A colliding name is only credited to a
# later-checked archive once the binary contains MORE occurrences of it than
# the earlier-checked archives already claimed — i.e. a report-only archive's
# copy of a colliding basename is "present" only when the binary shows a
# genuine second occurrence, not merely the required twin's one. This keeps
# the required-archive check exact (any real drop in a required archive's own
# registrar count still shows up as a shortfall) while making the
# report-only numbers for 2ship_enh/2ship_rando_ui trustworthy.
#
# One residual edge case, noted rather than solved: if two archives that are
# BOTH required happen to share a basename (currently just Miscellaneous.cpp,
# soh_rando/2ship_rando) and exactly one physical copy is ever dropped, the
# fixed-order attribution may credit the wrong one of the two — but the
# overall gate still fails either way, since the shortfall has to land on
# one of them. It cannot silently vanish the way the old bug did.
#
# Usage: check-registrar-elision.sh [build-dir]   (default: build-cmake)
#
#        check-registrar-elision.sh --self-test
#            Synthesizes tiny archive/object files with gcc/ar/ld -r (no real
#            build involved — entirely synthetic, self-contained inputs) and
#            asserts the counting-based attribution above still (a) passes a
#            plain no-collision present case, (b) FAILS when a required
#            archive's own registrar member is dropped from the link, (c)
#            reports a basename-colliding report-only archive PRESENT when
#            its own member is genuinely also linked, and (d) reports it
#            MISSING when only the required archive's same-named member
#            survived — the exact measurement artifact this fix closes.
#            Mirrors the check-exporter-symbol-collisions.sh --self-test
#            added in #430 for the same "guard the guard" reason. Exit 0:
#            all four scenarios behaved as expected. Exit 1: the detection
#            logic itself is broken. Exit 2: no usable toolchain found.

set -uo pipefail

REGISTRAR_REGEX='^_GLOBAL__sub_I_'

# registrar_symbols <path>
# Raw (non-deduplicated) list of _GLOBAL__sub_I_* symbol names defined in
# <path> — an archive or a linked binary/object. One line per matching
# symbol; callers that care about occurrence counts must not dedupe this.
registrar_symbols() {
    nm "$1" 2>/dev/null | awk '{print $NF}' | grep "$REGISTRAR_REGEX" || true
}

# run_elision_gate <bin-path> <archive-path:mode> [<archive-path:mode> ...]
#
# Core matching engine, shared by production mode and --self-test. Builds an
# occurrence-count map of every registrar symbol in <bin-path>, then walks
# the given archives IN THE ORDER PASSED, greedily attributing one binary
# occurrence of a symbol name to each archive that defines it. Because
# required archives are always listed before report-only ones by the caller,
# a required archive's claim on a colliding name is always resolved first —
# a report-only archive's same-named copy is only counted present once the
# binary has a genuine occurrence left over after that.
#
# Prints the same "archive: N registrar symbol(s), M missing [mode]" report
# line per archive as before (interface preserved). Returns 0 if every
# required archive has zero missing, 1 otherwise.
run_elision_gate() {
    local bin="$1"
    shift

    local -A bin_counts=()
    local sym
    while IFS= read -r sym; do
        [ -z "$sym" ] && continue
        bin_counts["$sym"]=$(( ${bin_counts["$sym"]:-0} + 1 ))
    done < <(registrar_symbols "$bin")

    local -A claimed=()
    local overall=0
    local spec archive mode missing total avail

    for spec in "$@"; do
        archive="${spec%%:*}"
        mode="${spec#*:}"

        if [ ! -f "$archive" ]; then
            if [ "$mode" = "required" ]; then
                echo "error: required archive $archive not found — build layout changed? Fix the path here rather than losing the guard." >&2
                overall=1
            else
                echo "note: $archive not found, skipping"
            fi
            continue
        fi

        missing=0
        total=0
        while IFS= read -r sym; do
            [ -z "$sym" ] && continue
            total=$((total + 1))
            avail=$(( ${bin_counts["$sym"]:-0} - ${claimed["$sym"]:-0} ))
            if [ "$avail" -gt 0 ]; then
                claimed["$sym"]=$(( ${claimed["$sym"]:-0} + 1 ))
            else
                echo "  MISSING from redship: $sym"
                missing=$((missing + 1))
            fi
        done < <(registrar_symbols "$archive" | sort -u)

        echo "$archive: $total registrar symbol(s), $missing missing [$mode]"
        if [ "$missing" -gt 0 ] && [ "$mode" = "required" ]; then
            overall=1
        fi
    done

    return "$overall"
}

# ---------------------------------------------------------------------------
# --self-test machinery: entirely synthetic, no OTRExporter/soh/2ship content.
# ---------------------------------------------------------------------------

CC_BIN="$(command -v "${CC:-}" 2>/dev/null || command -v cc 2>/dev/null || command -v gcc 2>/dev/null || command -v gcc-11 2>/dev/null || true)"
LD_BIN="$(command -v "${LD:-}" 2>/dev/null || command -v ld 2>/dev/null || true)"

# write_registrar_object <out.o> <symbol-name>
# Compiles a tiny object file defining exactly one LOCAL (file-static)
# symbol named <symbol-name> — dots and all, since real _GLOBAL__sub_I_
# names always carry a source extension (".cpp") and GAS accepts '.' in
# label names. Explicitly local, mirroring how GCC actually emits these:
# that is exactly why two same-named copies from different archives can
# coexist in one linked binary without a "multiple definition" error, the
# behavior this self-test needs to reproduce faithfully.
write_registrar_object() {
    local out="$1" symname="$2"
    local tmp
    tmp="$(mktemp -d)"
    {
        echo "    .text"
        echo "    .local ${symname}"
        echo "    .type ${symname}, @function"
        echo "${symname}:"
        echo "    ret"
    } > "$tmp/sym.s"
    "$CC_BIN" -c -o "$out" "$tmp/sym.s"
    rm -rf "$tmp"
}

# build_archive <out.a> <symbol-name...>
# One member object per symbol name, archived together.
build_archive() {
    local out_archive="$1"
    shift
    local tmp
    tmp="$(mktemp -d)"
    local objs=() symname i=0
    for symname in "$@"; do
        i=$((i + 1))
        write_registrar_object "$tmp/obj_$i.o" "$symname"
        objs+=("$tmp/obj_$i.o")
    done
    rm -f "$out_archive"
    ar rcs "$out_archive" "${objs[@]}"
    rm -rf "$tmp"
}

# build_fake_binary <out> <member.o...>
# Simulates "what the linker actually pulled into redship": a relocatable
# link (ld -r) of exactly the given member objects. Local symbols with
# identical names across inputs merge without conflict, exactly like the
# real link — an object left OUT of this call is the self-test's way of
# simulating that TU being elided.
build_fake_binary() {
    local out="$1"
    shift
    "$LD_BIN" -r -o "$out" "$@"
}

run_self_test() {
    if [ -z "$CC_BIN" ]; then
        echo "error: no C compiler found (checked \$CC, cc, gcc, gcc-11) — cannot self-test" >&2
        return 2
    fi
    if [ -z "$LD_BIN" ]; then
        echo "error: no linker (checked \$LD, ld) found — cannot self-test" >&2
        return 2
    fi

    local dir
    dir="$(mktemp -d)"
    local overall=0
    local out rc

    # --- Scenario 1: no collision, everything linked -> PASS, 0 missing.
    build_archive "$dir/s1_req.a" "_GLOBAL__sub_I_ReqOnly.cpp"
    write_registrar_object "$dir/s1_member.o" "_GLOBAL__sub_I_ReqOnly.cpp"
    build_fake_binary "$dir/s1_bin" "$dir/s1_member.o"
    out="$(run_elision_gate "$dir/s1_bin" "$dir/s1_req.a:required")"
    rc=$?
    if [ "$rc" -eq 0 ] && ! grep -q "MISSING" <<< "$out"; then
        echo "self-test: baseline no-collision scenario correctly passed (rc=0, no MISSING)"
    else
        echo "self-test FAIL: baseline no-collision scenario expected rc=0/no MISSING, got rc=$rc:" >&2
        echo "$out" >&2
        overall=1
    fi

    # --- Scenario 2: required archive's own member is dropped from the link
    #     (no collision involved) -> gate must FAIL. This is the "still bites"
    #     proof: the fix must not soften real required-archive elision.
    build_archive "$dir/s2_req.a" "_GLOBAL__sub_I_ReqLost.cpp"
    write_registrar_object "$dir/s2_unrelated.o" "_GLOBAL__sub_I_SomethingElsePresent.cpp"
    build_fake_binary "$dir/s2_bin" "$dir/s2_unrelated.o"
    out="$(run_elision_gate "$dir/s2_bin" "$dir/s2_req.a:required")"
    rc=$?
    if [ "$rc" -eq 1 ] && grep -q "MISSING from redship: _GLOBAL__sub_I_ReqLost.cpp" <<< "$out"; then
        echo "self-test: required-archive member loss correctly FAILS the gate (rc=1)"
    else
        echo "self-test FAIL: required-archive member loss did not fail as expected (rc=$rc):" >&2
        echo "$out" >&2
        overall=1
    fi

    # --- Scenario 3: basename collision between a required and a report-only
    #     archive, BOTH copies genuinely linked -> report-only must show
    #     PRESENT (0 missing), proving the fix credits a real second copy.
    build_archive "$dir/s3_req.a" "_GLOBAL__sub_I_Collide.cpp"
    build_archive "$dir/s3_rpt.a" "_GLOBAL__sub_I_Collide.cpp"
    write_registrar_object "$dir/s3_req_member.o" "_GLOBAL__sub_I_Collide.cpp"
    write_registrar_object "$dir/s3_rpt_member.o" "_GLOBAL__sub_I_Collide.cpp"
    build_fake_binary "$dir/s3_bin" "$dir/s3_req_member.o" "$dir/s3_rpt_member.o"
    out="$(run_elision_gate "$dir/s3_bin" "$dir/s3_req.a:required" "$dir/s3_rpt.a:report-only")"
    rc=$?
    if [ "$rc" -eq 0 ] && grep -q "^$dir/s3_req.a: 1 registrar symbol(s), 0 missing \[required\]$" <<< "$out" \
        && grep -q "^$dir/s3_rpt.a: 1 registrar symbol(s), 0 missing \[report-only\]$" <<< "$out"; then
        echo "self-test: colliding basename with BOTH copies linked correctly reports both PRESENT"
    else
        echo "self-test FAIL: colliding basename with both copies linked did not report both present:" >&2
        echo "$out" >&2
        overall=1
    fi

    # --- Scenario 4: THE FIX. Same colliding basename, but only the required
    #     archive's copy is actually linked (report-only's twin was elided).
    #     The old set-membership check would have reported the report-only
    #     archive's copy as present too, purely because the name exists
    #     somewhere in the binary. The fix must report it MISSING.
    build_fake_binary "$dir/s4_bin" "$dir/s3_req_member.o"
    out="$(run_elision_gate "$dir/s4_bin" "$dir/s3_req.a:required" "$dir/s3_rpt.a:report-only")"
    rc=$?
    if [ "$rc" -eq 0 ] && grep -q "^$dir/s3_req.a: 1 registrar symbol(s), 0 missing \[required\]$" <<< "$out" \
        && grep -q "^$dir/s3_rpt.a: 1 registrar symbol(s), 1 missing \[report-only\]$" <<< "$out" \
        && grep -q "MISSING from redship: _GLOBAL__sub_I_Collide.cpp" <<< "$out"; then
        echo "self-test: colliding basename with only the required copy linked correctly reports the report-only copy MISSING (rc=0, since report-only never fails the gate)"
    else
        echo "self-test FAIL: colliding basename with only the required copy linked did not correctly attribute the missing report-only copy (rc=$rc):" >&2
        echo "$out" >&2
        overall=1
    fi

    rm -rf "$dir"

    if [ "$overall" -eq 0 ]; then
        echo "self-test: OK (all 4 scenarios behaved as expected)"
    fi
    return "$overall"
}

# ---------------------------------------------------------------------------
# Production entry point
# ---------------------------------------------------------------------------

main() {
    local build_dir="${1:-build-cmake}"
    local bin="$build_dir/redship"

    if [ ! -f "$bin" ]; then
        echo "error: $bin not found (run after the redship link)" >&2
        exit 2
    fi

    local overall=0
    run_elision_gate "$bin" \
        "$build_dir/games/oot/libsoh_rando.a:required" \
        "$build_dir/games/oot/libsoh_enh.a:required" \
        "$build_dir/games/mm/lib2ship_rando.a:required" \
        "$build_dir/games/mm/lib2ship_enh.a:report-only" \
        "$build_dir/games/mm/lib2ship_rando_ui.a:report-only"
    overall=$?

    # Lane C0 reachability probes (#392): beyond registrar symbols, assert MM's
    # randomizer generation surface is actually IN the binary — the historical
    # failure was 2ship_rando compiling green while the linker discarded every
    # object. nm is the primary ground truth (per the header note, strings can
    # false-negative on folded std::string constructions); the spoiler tag is a
    # plain string literal, checked with strings as the belt-and-braces probe the
    # Lane C0 brief calls for.
    #
    # NOT `grep -q`: this script runs under pipefail, and -q exits at the first
    # match while nm/strings are still writing — the producer dies with SIGPIPE
    # (141), pipefail reports the pipeline failed, and a FOUND symbol reads as
    # missing. Plain grep with discarded stdout consumes the stream to EOF.
    if ! nm --demangle "$bin" 2>/dev/null | grep "Rando::Spoiler::GenerateFromSaveContext" > /dev/null; then
        echo "FAIL: Rando::Spoiler::GenerateFromSaveContext missing from redship — 2ship_rando was elided (#392)" >&2
        overall=1
    fi
    if ! strings "$bin" 2>/dev/null | grep "2S2H_RANDO_SPOILER" > /dev/null; then
        echo "FAIL: 2S2H_RANDO_SPOILER tag missing from redship — MM spoiler writer not linked (#392)" >&2
        overall=1
    fi

    if [ "$overall" -ne 0 ]; then
        echo "FAIL: registrar symbols were elided from a WHOLE_ARCHIVE-protected archive (#341)" >&2
        exit 1
    fi
    echo "OK: no registrar elision in protected archives; 2ship_rando generation surface present"
}

if [ "${1:-}" = "--self-test" ]; then
    run_self_test
    exit $?
fi

main "$@"
