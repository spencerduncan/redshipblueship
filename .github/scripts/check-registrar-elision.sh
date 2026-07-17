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
# Usage: check-registrar-elision.sh [build-dir]   (default: build-cmake)

set -uo pipefail

BUILD_DIR="${1:-build-cmake}"
BIN="$BUILD_DIR/redship"

if [ ! -f "$BIN" ]; then
    echo "error: $BIN not found (run after the redship link)" >&2
    exit 2
fi

BIN_SYMS="$(mktemp)"
trap 'rm -f "$BIN_SYMS"' EXIT
nm "$BIN" 2>/dev/null | awk '{print $NF}' | grep '^_GLOBAL__sub_I_' | sort -u > "$BIN_SYMS" || true

overall=0

# check_archive <path> <required|report-only>
#   required:    any missing registrar fails the build (WHOLE_ARCHIVE-wrapped
#                archives must be fully present by construction).
#   report-only: print what is missing without failing — visibility for
#                archives not yet wrapped (soh_enh, see #341 remaining scope).
check_archive() {
    local archive="$1" mode="$2"
    if [ ! -f "$archive" ]; then
        if [ "$mode" = "required" ]; then
            echo "error: required archive $archive not found — build layout changed? Fix the path here rather than losing the guard." >&2
            overall=1
        else
            echo "note: $archive not found, skipping"
        fi
        return
    fi
    local missing=0 total=0 sym
    while IFS= read -r sym; do
        [ -z "$sym" ] && continue
        total=$((total + 1))
        if ! grep -qxF "$sym" "$BIN_SYMS"; then
            echo "  MISSING from redship: $sym"
            missing=$((missing + 1))
        fi
    done < <(nm "$archive" 2>/dev/null | awk '{print $NF}' | grep '^_GLOBAL__sub_I_' | sort -u || true)
    echo "$archive: $total registrar symbol(s), $missing missing [$mode]"
    if [ "$missing" -gt 0 ] && [ "$mode" = "required" ]; then
        overall=1
    fi
}

check_archive "$BUILD_DIR/games/oot/libsoh_rando.a"  required
check_archive "$BUILD_DIR/games/mm/lib2ship_enh.a"   required
check_archive "$BUILD_DIR/games/mm/lib2ship_rando.a" required
# Not yet WHOLE_ARCHIVE-wrapped — report drops without failing so the
# remaining #341 exposure stays visible in every CI run.
check_archive "$BUILD_DIR/games/oot/libsoh_enh.a"    report-only

if [ "$overall" -ne 0 ]; then
    echo "FAIL: registrar symbols were elided from a WHOLE_ARCHIVE-protected archive (#341)" >&2
    exit 1
fi
echo "OK: no registrar elision in protected archives"
