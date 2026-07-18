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
# Usage: check-symbol-collisions.sh [build-dir] [--write-baseline]
#   --write-baseline: regenerate the baseline from the current build instead
#                     of checking. For deliberate refreshes only.

set -uo pipefail

BUILD_DIR="build-cmake"
WRITE_BASELINE=0
for arg in "$@"; do
    case "$arg" in
        --write-baseline) WRITE_BASELINE=1 ;;
        *) BUILD_DIR="$arg" ;;
    esac
done

BASELINE="$(dirname "$0")/../symbol-collision-baseline.txt"

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
