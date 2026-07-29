#!/usr/bin/env bash
# Cross-game symbol-collision tripwire.
#
# Both games link into one redship binary with first-wins archive resolution
# (and /FORCE:MULTIPLE on Windows), so any symbol DEFINED by both a soh_* and
# a 2ship_* archive silently executes one game's implementation against the
# other game's data. That failure class has now recurred four times: the #367
# GameInteractor VB-enum aliasing, the cosmetic-gfx stubs, the
# FrameInterpolation_* family (MM 3D rendered garbage), and the
# AudioCollection/AudioEditor family. This script turns the next recurrence
# into a build-time failure instead of a runtime haunting.
#
# Mechanism: intersect the strong (non-weak) defined global symbols of the
# soh_* archives with those of the 2ship_* archives and compare against the
# committed baseline (.github/symbol-collision-baseline.txt) of tolerated
# collisions. New intersecting symbols fail the build — fix them with the
# MM_ prefix / namespace S2H conventions (see games/mm/include/
# mm_audio_prefix.h for a worked example) rather than extending the baseline.
#
# Weak (W/V) and GNU-unique (u) symbols are out of scope: templates and
# inline functions from shared headers legitimately COMDAT-fold, and the
# dangerous divergent instantiations are keyed by strong-named types this
# script does catch.
#
# nm-based, Linux-only (mirrors check-registrar-elision.sh; the Windows-side
# equivalent audit uses dumpbin over the same lib split).
#
# Usage: check-symbol-collisions.sh [build-dir] [--baseline <path>]
#                                   [--write-baseline]
#   --baseline <path>: use <path> instead of .github/symbol-collision-
#                     baseline.txt. Used by --self-test to run against a
#                     throwaway baseline, and by CI to regenerate one into an
#                     artifact directory without dirtying the checkout.
#   --write-baseline: regenerate the baseline from the current build instead
#                     of checking. For deliberate refreshes only.
#
#        check-symbol-collisions.sh --self-test
#            Synthesizes a build-dir-shaped tree of tiny archives with gcc/ar
#            (no real soh/2ship content) and drives THIS script over it as a
#            child process, so every scenario exercises the real glob, the
#            real nm collection and the real baseline comparison rather than
#            a parallel reimplementation. Asserts that the gate still (a)
#            passes archives sharing only a weak global and a
#            _GLOBAL__sub_I_ name — the two things the filters exist to drop,
#            so those filters stay falsifiable, (b) FAILS on a shared strong
#            global absent from the baseline, (c) passes that same collision
#            once it is baselined, (d) regenerates a baseline containing
#            exactly the intersection under --write-baseline and accepts it
#            on the next run, and (e) refuses to pass vacuously when no
#            archives match. Exit 0: all five hold. Exit 1: the detection
#            logic itself is broken. Exit 2: no usable toolchain found.
#            Mirrors the --self-test in check-registrar-elision.sh and
#            check-exporter-symbol-collisions.sh for the same "guard the
#            guard" reason — scenario (d) matters most, because the CI
#            baseline-regeneration path below has no other cover.
#
# REGENERATING THE BASELINE — one command, from any machine:
#
#   On a Linux box with a build:
#     bash .github/scripts/check-symbol-collisions.sh build-cmake --write-baseline
#
#   Without one (the #387 path — local iteration happens on Windows, where
#   nm-over-ELF-archives does not exist). Every .github/workflows/link-check.yml
#   run uploads a regenerated baseline as the `symbol-collision-baseline`
#   artifact, pass or fail, so there is no separate "regenerate" dispatch to
#   remember. Take the newest run of that workflow and:
#     gh run download <run-id> -R spencerduncan/redshipblueship \
#         -n symbol-collision-baseline -D .github
#   which lands the file directly at .github/symbol-collision-baseline.txt,
#   ready to commit. The companion `symbol-census` artifact from the same run
#   carries the demangled listing and a diff against the committed baseline —
#   read that BEFORE committing. Every entry is a latent cross-game aliasing
#   bug or deliberate glue; the baseline is a burn-down list, not a
#   suppression file.

set -uo pipefail

BUILD_DIR="build-cmake"
WRITE_BASELINE=0
SELF_TEST=0
BASELINE="$(dirname "$0")/../symbol-collision-baseline.txt"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --write-baseline) WRITE_BASELINE=1 ;;
        --self-test) SELF_TEST=1 ;;
        --baseline)
            if [ "$#" -lt 2 ]; then
                echo "usage: $0 [build-dir] [--baseline <path>] [--write-baseline]" >&2
                exit 2
            fi
            BASELINE="$2"
            shift
            ;;
        --*)
            echo "error: unknown option $1" >&2
            echo "usage: $0 [build-dir] [--baseline <path>] [--write-baseline] | $0 --self-test" >&2
            exit 2
            ;;
        *) BUILD_DIR="$1" ;;
    esac
    shift
done

# ---------------------------------------------------------------------------
# --self-test machinery: entirely synthetic, no soh/2ship content. Defined
# before the production body so the dispatch below can short-circuit it.
# ---------------------------------------------------------------------------

SELFTEST_SHARED_SYMBOL="rsbs_selftest_shared_strong"

CC_BIN="$(command -v "${CC:-}" 2>/dev/null || command -v cc 2>/dev/null || command -v gcc 2>/dev/null || command -v gcc-11 2>/dev/null || true)"

# selftest_build_archive <out-archive> <unique-symbol> <include-shared:0|1>
# One TU carrying: the shared strong global (optional), a per-side unique
# strong global, a WEAK global, and a global whose assembler name is a
# _GLOBAL__sub_I_ static-init aggregator. The last two carry the SAME name on
# both sides deliberately — they are exactly what collect_defined's type and
# name filters exist to drop, so the clean scenario passing with them present
# is what makes those filters falsifiable rather than assumed.
selftest_build_archive() {
    local out_archive="$1" unique_symbol="$2" include_shared="$3"
    local tmp
    tmp="$(mktemp -d)"
    {
        if [ "$include_shared" -eq 1 ]; then
            echo "int ${SELFTEST_SHARED_SYMBOL} = 1;"
        fi
        echo "int ${unique_symbol} = 2;"
        echo "int rsbs_selftest_weak __attribute__((weak)) = 3;"
        echo "int rsbs_selftest_ctor_alias __asm__(\"_GLOBAL__sub_I_rsbs_selftest\") = 4;"
    } > "$tmp/src.c"
    # Named failure rather than a missing archive: without this, a toolchain
    # problem reaches the scenarios below as "gate returned 2 where 0 was
    # expected" and reads as a broken gate instead of a broken self-test.
    if ! "$CC_BIN" -c -o "$tmp/src.o" "$tmp/src.c" 2> "$tmp/cc.err"; then
        echo "error: self-test TU failed to compile with $CC_BIN:" >&2
        sed 's/^/  /' < "$tmp/cc.err" >&2
        sed 's/^/  | /' < "$tmp/src.c" >&2
        rm -rf "$tmp"
        return 2
    fi
    mkdir -p "$(dirname "$out_archive")"
    rm -f "$out_archive"
    if ! ar rcs "$out_archive" "$tmp/src.o"; then
        echo "error: self-test failed to archive $out_archive" >&2
        rm -rf "$tmp"
        return 2
    fi
    rm -rf "$tmp"
}

# selftest_make_build_dir <dir> <include-shared:0|1>
# A tree shaped like the real build layout, so the child invocation below
# resolves it through the same games/oot/libsoh_*.a and games/mm/lib2ship_*.a
# globs the production path uses.
selftest_make_build_dir() {
    local dir="$1" include_shared="$2"
    selftest_build_archive "$dir/games/oot/libsoh_selftest.a" rsbs_selftest_oot_only "$include_shared" || return 2
    selftest_build_archive "$dir/games/mm/lib2ship_selftest.a" rsbs_selftest_mm_only "$include_shared" || return 2
}

# selftest_expect <label> <expected-rc> <log-file> <actual-rc>
# Returns 0 when the scenario behaved as expected, 1 otherwise (printing the
# child's own output, which is the only place the reason lives).
selftest_expect() {
    local label="$1" want="$2" log="$3" got="$4"
    if [ "$got" -eq "$want" ]; then
        echo "self-test: $label behaved as expected (rc=$got)"
        return 0
    fi
    echo "self-test FAIL: $label returned rc=$got, expected $want. Child output:" >&2
    sed 's/^/  /' < "$log" >&2
    return 1
}

run_self_test() {
    if [ -z "$CC_BIN" ]; then
        echo "error: no C compiler found (checked \$CC, cc, gcc, gcc-11) — cannot self-test" >&2
        return 2
    fi
    if ! command -v ar > /dev/null 2>&1; then
        echo "error: ar not found — cannot self-test" >&2
        return 2
    fi

    local dir
    dir="$(mktemp -d)"
    local overall=0 rc written

    : > "$dir/empty-baseline.txt"

    # (a) Only a weak global and a _GLOBAL__sub_I_ name are shared -> PASS.
    selftest_make_build_dir "$dir/clean" 0 || { rm -rf "$dir"; return 2; }
    bash "$0" "$dir/clean" --baseline "$dir/empty-baseline.txt" > "$dir/log.clean" 2>&1
    rc=$?
    selftest_expect "clean scenario (weak + _GLOBAL__sub_I_ shared, filtered)" 0 "$dir/log.clean" "$rc" || overall=1

    # (b) A shared STRONG global with an empty baseline -> FAIL.
    selftest_make_build_dir "$dir/collide" 1 || { rm -rf "$dir"; return 2; }
    bash "$0" "$dir/collide" --baseline "$dir/empty-baseline.txt" > "$dir/log.collide" 2>&1
    rc=$?
    selftest_expect "collision scenario (shared strong global, unbaselined)" 1 "$dir/log.collide" "$rc" || overall=1

    # (c) The same collision, baselined -> PASS. Without this the gate could
    # be "detects everything, tolerates nothing", which is a different tool.
    printf '# self-test baseline\n%s\n' "$SELFTEST_SHARED_SYMBOL" > "$dir/tolerating-baseline.txt"
    bash "$0" "$dir/collide" --baseline "$dir/tolerating-baseline.txt" > "$dir/log.tolerated" 2>&1
    rc=$?
    selftest_expect "baselined collision scenario" 0 "$dir/log.tolerated" "$rc" || overall=1

    # (d) --write-baseline regenerates EXACTLY the intersection, and the
    # result is accepted on the next run. This is the CI baseline-generation
    # path (link-check.yml uploads what this writes); nothing else covers it.
    bash "$0" "$dir/collide" --baseline "$dir/generated.txt" --write-baseline > "$dir/log.write" 2>&1
    rc=$?
    if selftest_expect "--write-baseline regeneration" 0 "$dir/log.write" "$rc"; then
        written="$(grep -v '^[[:space:]]*#' "$dir/generated.txt" | grep -v '^[[:space:]]*$' || true)"
        if [ "$written" = "$SELFTEST_SHARED_SYMBOL" ]; then
            echo "self-test: regenerated baseline contains exactly the intersection"
        else
            echo "self-test FAIL: regenerated baseline should contain exactly '$SELFTEST_SHARED_SYMBOL', got:" >&2
            printf '%s\n' "$written" | sed 's/^/  /' >&2
            overall=1
        fi
        bash "$0" "$dir/collide" --baseline "$dir/generated.txt" > "$dir/log.regen" 2>&1
        rc=$?
        selftest_expect "check against the regenerated baseline" 0 "$dir/log.regen" "$rc" || overall=1
    else
        overall=1
    fi

    # (e) No archives at all -> collection error, never a vacuous pass. An
    # empty intersection is indistinguishable from a broken collection unless
    # this refuses.
    mkdir -p "$dir/empty/games/oot" "$dir/empty/games/mm"
    bash "$0" "$dir/empty" --baseline "$dir/empty-baseline.txt" > "$dir/log.empty" 2>&1
    rc=$?
    selftest_expect "vacuous-collection guard (no archives present)" 2 "$dir/log.empty" "$rc" || overall=1

    rm -rf "$dir"

    if [ "$overall" -eq 0 ]; then
        echo "self-test: OK (all 5 scenarios behaved as expected)"
    fi
    return "$overall"
}

if [ "$SELF_TEST" -eq 1 ]; then
    run_self_test
    exit $?
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

# collect_defined <out-file> <lib...>
# Union of defined, external, non-weak symbol names across the given archives.
collect_defined() {
    local out="$1"
    shift
    : > "$out.raw"
    local lib found=0
    for lib in "$@"; do
        [ -f "$lib" ] || continue
        found=$((found + 1))
        # nm archive output: "addr TYPE name" per symbol, member-header and
        # blank lines otherwise. Keep strong definitions only (drop weak
        # W/V/w/v and GNU-unique u), and drop per-TU static-init symbols —
        # both games name them after identical upstream filenames.
        nm --defined-only --extern-only "$lib" 2>/dev/null |
            awk 'NF == 3 && $2 !~ /^[WVwvu]$/ { print $3 }' |
            grep -v '^_GLOBAL__sub_[ID]_' >> "$out.raw" || true
    done
    if [ "$found" -eq 0 ]; then
        echo "error: none of the archives exist: $* — build layout changed? Fix the paths here rather than losing the guard." >&2
        exit 2
    fi
    sort -u "$out.raw" > "$out"
    # Each game's archives define thousands of strong external symbols; an
    # empty collection means nm (or the awk filter) broke, and proceeding
    # would pass vacuously with an empty intersection — a silent disarm.
    if [ ! -s "$out" ]; then
        echo "error: collected zero defined symbols from: $* — nm/binutils failure or archive format change? Refusing to pass vacuously; fix the collection here rather than losing the guard." >&2
        exit 2
    fi
}

collect_defined "$TMP_DIR/soh"   "$BUILD_DIR"/games/oot/libsoh_*.a
collect_defined "$TMP_DIR/2ship" "$BUILD_DIR"/games/mm/lib2ship_*.a

comm -12 "$TMP_DIR/soh" "$TMP_DIR/2ship" > "$TMP_DIR/intersection"
COUNT="$(wc -l < "$TMP_DIR/intersection")"

demangle() {
    if command -v c++filt > /dev/null 2>&1; then
        c++filt
    else
        cat
    fi
}

if [ "$WRITE_BASELINE" -eq 1 ]; then
    mkdir -p "$(dirname "$BASELINE")"
    {
        echo "# Tolerated soh_*/2ship_* defined-symbol collisions (see"
        echo "# .github/scripts/check-symbol-collisions.sh). Every entry here is a"
        echo "# latent cross-game aliasing bug or deliberate glue — shrink this"
        echo "# list, never grow it without a comment in the PR explaining why the"
        echo "# collision is safe."
        cat "$TMP_DIR/intersection"
    } > "$BASELINE"
    echo "wrote $COUNT symbol(s) to $BASELINE"
    exit 0
fi

if [ ! -f "$BASELINE" ]; then
    echo "WARNING: baseline $BASELINE missing — running in bootstrap mode."
    echo "Current soh_*/2ship_* strong-symbol intersection ($COUNT symbol(s)),"
    echo "raw mangled names between the markers (copy into the baseline file"
    echo "or run this script with --write-baseline on a Linux build):"
    echo "-----BEGIN SYMBOL INTERSECTION-----"
    cat "$TMP_DIR/intersection"
    echo "-----END SYMBOL INTERSECTION-----"
    echo "Demangled, for review:"
    demangle < "$TMP_DIR/intersection" | sed 's/^/  /'
    echo "Until the baseline is committed this check cannot fail."
    exit 0
fi

grep -v '^[[:space:]]*#' "$BASELINE" | grep -v '^[[:space:]]*$' | sort -u > "$TMP_DIR/baseline"

comm -23 "$TMP_DIR/intersection" "$TMP_DIR/baseline" > "$TMP_DIR/new"
comm -13 "$TMP_DIR/intersection" "$TMP_DIR/baseline" > "$TMP_DIR/stale"

if [ -s "$TMP_DIR/stale" ]; then
    echo "note: $(wc -l < "$TMP_DIR/stale") baseline entr(y/ies) no longer collide — prune them from $BASELINE:"
    demangle < "$TMP_DIR/stale" | sed 's/^/  /'
fi

if [ -s "$TMP_DIR/new" ]; then
    echo "FAIL: new symbol(s) defined by BOTH soh_* and 2ship_* archives:" >&2
    demangle < "$TMP_DIR/new" | sed 's/^/  /' >&2
    echo "First-wins link resolution will silently run one game's code against" >&2
    echo "the other's data. Rename the MM side (MM_ prefix for functions," >&2
    echo "namespace S2H for classes — see games/mm/include/mm_audio_prefix.h)" >&2
    echo "instead of adding to the baseline." >&2
    exit 1
fi

echo "OK: soh_*/2ship_* symbol intersection matches baseline ($COUNT symbol(s), all tolerated)"
