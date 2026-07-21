#!/usr/bin/env bash
# Strong-DATA-symbol collision gate between the OTRExporter_OoT and
# OTRExporter_MM archives (issue #411, permanent follow-up to the Fault A fix
# in PR #413).
#
# #413 namespaced the 11 external-linkage globals of
# OTRExporter/OTRExporter/Main.cpp + VersionInfo.cpp per game variant.
# Before that fix, both TUs compiled identically into OTRExporter_OoT and
# OTRExporter_MM, and both archives are deliberately linked into redship for
# in-app ROM extraction (CMake/SingleExecutable.cmake:141-153). Windows
# /FORCE:MULTIPLE (root CMakeLists.txt) keeps ONE storage slot per external
# symbol defined by both archives but runs BOTH variants' dynamic
# initializers and atexit destructors against it — every such global was
# constructed twice at startup and destroyed twice at exit, the second
# destruction walking freed heap (0xC0000374 on every normal exit,
# reproducible with `redship --version`; see #396).
#
# Unlike the soh_*/2ship_* gate (check-symbol-collisions.sh), which tolerates
# ~20 identical strong FUNCTION symbols against a committed baseline because
# both exporter variants compile from IDENTICAL sources, this gate carries no
# baseline at all: it filters to strong DATA symbols only (mutable objects
# with dynamic init / atexit dtors — the actual Fault A hazard), and after
# #413 that intersection is empty. The ~20 duplicated function symbols are
# out of scope BY DESIGN (see #411's "Note on the duplicated function
# pairs") — they fold harmlessly under /FORCE:MULTIPLE because redship never
# executes them: in-app extraction spawns the standalone ZAPD_OoT/ZAPD_MM
# subprocesses, each linking exactly one variant (issue #325), so the
# correct body is always present in the process that runs it. Namespacing
# them (especially ImportExporters, ZAPD's plugin ABI entry point) would
# break the plugin contract for no safety benefit. Do NOT extend this script
# to cover them.
#
# Data type letters under `nm --defined-only --extern-only`: B/b D/d G/g S/s
# R/r (BSS / data / small-data / small-bss / rodata); with --extern-only they
# arrive uppercase. Weak (W/V/w/v), GNU-unique (u), and text (T/t) symbols
# are excluded, along with per-TU `_GLOBAL__sub_[ID]_` init aggregators (both
# variants name these after identical upstream filenames, so they always
# "collide" and are never the hazard this gate looks for).
#
# nm-based, Linux-only (mirrors check-symbol-collisions.sh and
# check-registrar-elision.sh; the Windows-side equivalent audit uses dumpbin
# over the same archive split).
#
# Usage:
#   check-exporter-symbol-collisions.sh [build-dir]
#       Production gate (wired into build-linux in generate-builds.yml).
#       Compares the two real archives built from build-dir (default
#       build-cmake):
#         <build-dir>/OTRExporter/OTRExporter/libOTRExporter_OoT.a
#         <build-dir>/OTRExporter/OTRExporter/libOTRExporter_MM.a
#       Exit 0: empty intersection. Exit 1: new collision found — fix it by
#       namespacing the MM side per game variant (see
#       OTRExporter/OTRExporter/ExporterVariant.h), never by adding a
#       baseline. Exit 2: collection error (missing archive / nm failure /
#       vacuous symbol set) — refuses to pass vacuously.
#
#   check-exporter-symbol-collisions.sh --check-archives <archive-a> <archive-b>
#       Runs the identical gate logic against two explicit archive paths.
#       Used internally by --self-test, and by the one-off, submodule-free
#       FAIL-path demonstration described in #411's validation notes (see
#       --emit-collision-pair below) — never touches the OTRExporter
#       submodule, so it needs no throwaway pointer bump.
#
#   check-exporter-symbol-collisions.sh --self-test
#       Synthesizes tiny archive pairs with gcc/ar (no OTRExporter submodule
#       involved: entirely synthetic, self-contained TUs) and asserts the
#       detection logic still (a) flags a shared strong DATA global, (b)
#       passes a pair with none, and (c) refuses to pass vacuously when one
#       side defines zero strong DATA symbols. Exit 0: all three hold — the
#       expected result on every run, safe to wire permanently into CI as a
#       "guard the guard" regression check (the gate's own logic silently
#       breaking would otherwise look identical to "nothing to report").
#       Exit 1: the detection logic itself is broken.
#
#   check-exporter-symbol-collisions.sh --emit-collision-pair <dir>
#       Writes a synthetic archive pair that shares one strong DATA global
#       into <dir> (libSelftestCollisionA.a / libSelftestCollisionB.a) and
#       exits 0. Deliberately NOT wired into the permanent workflow: it
#       exists so a temporary, revertable CI commit can pipe its output
#       through --check-archives and capture a real, honest red run proving
#       the FAIL path — without a throwaway OTRExporter submodule pointer
#       bump.

set -uo pipefail

SELFTEST_DUP_SYMBOL="otrexporter_selftest_shared_global"

# Resolve a C compiler for the synthetic self-test / emit paths only. The
# production gate below never compiles anything and does not need this.
CC_BIN="$(command -v "${CC:-}" 2>/dev/null || command -v cc 2>/dev/null || command -v gcc 2>/dev/null || command -v gcc-11 2>/dev/null || true)"

# collect_data_symbols <out-file> <archive>
# Strong, external, DATA-typed defined symbol names in <archive>, sorted and
# de-duplicated into <out-file>. Missing archives collect as empty (the
# caller enforces the non-empty guard so the error message can name both
# archives together).
collect_data_symbols() {
    local out="$1" lib="$2"
    : > "$out.raw"
    if [ -f "$lib" ]; then
        nm --defined-only --extern-only "$lib" 2>/dev/null |
            awk 'NF == 3 && $2 ~ /^[BDGSR]$/ { print $3 }' |
            grep -v '^_GLOBAL__sub_[ID]_' >> "$out.raw" || true
    fi
    sort -u "$out.raw" > "$out"
}

demangle() {
    if command -v c++filt > /dev/null 2>&1; then
        c++filt
    else
        cat
    fi
}

# check_pair <label> <archive-a> <archive-b>
# Reports and returns 0 (clean), 1 (collision found), or 2 (collection
# error) — never calls exit, so callers (production mode vs. self-test
# scenarios) decide what the result means.
check_pair() {
    local label="$1" lib_a="$2" lib_b="$3"
    local tmp; tmp="$(mktemp -d)"

    local lib
    for lib in "$lib_a" "$lib_b"; do
        if [ ! -f "$lib" ]; then
            echo "error [$label]: archive not found: $lib — build layout changed? Fix the path here rather than losing the guard." >&2
            rm -rf "$tmp"
            return 2
        fi
    done

    collect_data_symbols "$tmp/a" "$lib_a"
    collect_data_symbols "$tmp/b" "$lib_b"

    # Each real archive defines a good number of strong DATA globals; an
    # empty collection from EITHER side means nm/binutils broke or the
    # archive format changed, and proceeding would risk passing vacuously on
    # an empty intersection that means nothing (per #411's calibration note).
    if [ ! -s "$tmp/a" ]; then
        echo "error [$label]: collected zero strong DATA symbols from $lib_a — nm/binutils failure or archive format change? Refusing to pass vacuously; fix the collection here rather than losing the guard." >&2
        rm -rf "$tmp"
        return 2
    fi
    if [ ! -s "$tmp/b" ]; then
        echo "error [$label]: collected zero strong DATA symbols from $lib_b — nm/binutils failure or archive format change? Refusing to pass vacuously; fix the collection here rather than losing the guard." >&2
        rm -rf "$tmp"
        return 2
    fi

    comm -12 "$tmp/a" "$tmp/b" > "$tmp/intersection"

    local rc
    if [ -s "$tmp/intersection" ]; then
        echo "FAIL [$label]: strong DATA symbol(s) defined by BOTH archives:" >&2
        demangle < "$tmp/intersection" | sed 's/^/  /' >&2
        echo "First-wins /FORCE:MULTIPLE resolution keeps one storage slot but runs BOTH" >&2
        echo "variants' dynamic initializers and atexit destructors against it — the" >&2
        echo "Fault A double-construction/double-destruction class (#396, fixed in #413" >&2
        echo "for the original 11). Namespace the MM side per game variant (see" >&2
        echo "OTRExporter/OTRExporter/ExporterVariant.h) instead of suppressing this check." >&2
        rc=1
    else
        echo "OK [$label]: strong DATA symbol intersection is empty ($(wc -l < "$tmp/a") OoT-side / $(wc -l < "$tmp/b") MM-side strong DATA symbol(s) collected, none shared)"
        rc=0
    fi

    rm -rf "$tmp"
    return "$rc"
}

# build_selftest_archive <out-archive> <unique-symbol> <include-dup:0|1>
# Compiles a one-line-per-symbol TU and archives it. Entirely synthetic and
# self-contained — no OTRExporter submodule content involved.
build_selftest_archive() {
    local out_archive="$1" unique_symbol="$2" include_dup="$3"
    local tmp; tmp="$(mktemp -d)"
    {
        if [ "$include_dup" -eq 1 ]; then
            echo "int ${SELFTEST_DUP_SYMBOL} = 1;"
        fi
        echo "int ${unique_symbol} = 2;"
    } > "$tmp/src.c"
    "$CC_BIN" -c -o "$tmp/src.o" "$tmp/src.c"
    rm -f "$out_archive"
    ar rcs "$out_archive" "$tmp/src.o"
    rm -rf "$tmp"
}

# build_selftest_funconly_archive <out-archive>
# A TU with a function but zero strong DATA symbols, for the vacuous-
# collection-guard scenario.
build_selftest_funconly_archive() {
    local out_archive="$1"
    local tmp; tmp="$(mktemp -d)"
    echo "void otrexporter_selftest_func(void) {}" > "$tmp/src.c"
    "$CC_BIN" -c -o "$tmp/src.o" "$tmp/src.c"
    rm -f "$out_archive"
    ar rcs "$out_archive" "$tmp/src.o"
    rm -rf "$tmp"
}

emit_collision_pair() {
    local dir="$1"
    mkdir -p "$dir"
    build_selftest_archive "$dir/libSelftestCollisionA.a" otrexporter_selftest_a_only 1
    build_selftest_archive "$dir/libSelftestCollisionB.a" otrexporter_selftest_b_only 1
    echo "wrote synthetic colliding archive pair to $dir (shared strong DATA symbol: ${SELFTEST_DUP_SYMBOL})"
    echo "check with: $0 --check-archives $dir/libSelftestCollisionA.a $dir/libSelftestCollisionB.a"
}

run_self_test() {
    if [ -z "$CC_BIN" ]; then
        echo "error: no C compiler found (checked \$CC, cc, gcc, gcc-11) — cannot self-test" >&2
        return 2
    fi

    local dir; dir="$(mktemp -d)"
    local overall=0
    local rc

    # Scenario 1: both sides define the shared dup global -> gate must FAIL.
    build_selftest_archive "$dir/collide_a.a" otrexporter_selftest_a_only 1
    build_selftest_archive "$dir/collide_b.a" otrexporter_selftest_b_only 1
    check_pair "self-test:collision" "$dir/collide_a.a" "$dir/collide_b.a"
    rc=$?
    if [ "$rc" -eq 1 ]; then
        echo "self-test: collision scenario correctly detected (rc=1)"
    elif [ "$rc" -eq 0 ]; then
        echo "self-test FAIL: gate passed on synthetic archives deliberately sharing '${SELFTEST_DUP_SYMBOL}' — detection logic is broken." >&2
        overall=1
    else
        echo "self-test FAIL: collision scenario returned rc=$rc (collection error), expected 1 (collision)." >&2
        overall=1
    fi

    # Scenario 2: unique-only symbols on both sides -> gate must PASS.
    build_selftest_archive "$dir/clean_a.a" otrexporter_selftest_clean_a 0
    build_selftest_archive "$dir/clean_b.a" otrexporter_selftest_clean_b 0
    check_pair "self-test:clean" "$dir/clean_a.a" "$dir/clean_b.a"
    rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "self-test: clean scenario (no shared globals) correctly passed (rc=0)"
    else
        echo "self-test FAIL: clean scenario returned rc=$rc, expected 0 (pass)." >&2
        overall=1
    fi

    # Scenario 3: one side defines zero strong DATA symbols -> vacuous-
    # collection guard must trip rather than pass vacuously.
    build_selftest_funconly_archive "$dir/funconly.a"
    build_selftest_archive "$dir/normal.a" otrexporter_selftest_normal_only 0
    check_pair "self-test:vacuous-guard" "$dir/funconly.a" "$dir/normal.a"
    rc=$?
    if [ "$rc" -eq 2 ]; then
        echo "self-test: vacuous-collection guard correctly tripped (rc=2) on a DATA-symbol-free archive"
    else
        echo "self-test FAIL: vacuous-collection guard did not trip (rc=$rc, expected 2) when one archive defines zero strong DATA symbols." >&2
        overall=1
    fi

    rm -rf "$dir"

    if [ "$overall" -eq 0 ]; then
        echo "self-test: OK (all 3 scenarios behaved as expected)"
    fi
    return "$overall"
}

main() {
    if [ "$#" -ge 1 ]; then
        case "$1" in
            --self-test)
                run_self_test
                exit $?
                ;;
            --emit-collision-pair)
                if [ "$#" -lt 2 ]; then
                    echo "usage: $0 --emit-collision-pair <dir>" >&2
                    exit 2
                fi
                if [ -z "$CC_BIN" ]; then
                    echo "error: no C compiler found (checked \$CC, cc, gcc, gcc-11)" >&2
                    exit 2
                fi
                emit_collision_pair "$2"
                exit 0
                ;;
            --check-archives)
                if [ "$#" -lt 3 ]; then
                    echo "usage: $0 --check-archives <archive-a> <archive-b>" >&2
                    exit 2
                fi
                check_pair "explicit" "$2" "$3"
                exit $?
                ;;
        esac
    fi

    local build_dir="${1:-build-cmake}"
    local oot_lib="$build_dir/OTRExporter/OTRExporter/libOTRExporter_OoT.a"
    local mm_lib="$build_dir/OTRExporter/OTRExporter/libOTRExporter_MM.a"
    check_pair "OTRExporter_OoT/OTRExporter_MM" "$oot_lib" "$mm_lib"
    exit $?
}

main "$@"
