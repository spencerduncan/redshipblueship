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
check_archive "$BUILD_DIR/games/oot/libsoh_enh.a"    required
# 2ship_rando is WHOLE_ARCHIVE-wrapped since Lane C0 (#392): its registrar
# TUs (the Logic/Regions graph) must all survive the link. The measured
# dependency gap that used to block this (PR #422's diagnostic link: the
# CustomMessage/CustomItem/ShipUtils families, UpdateGameTime, and four
# BenGui symbols) was closed by compiling the first four in and splitting the
# BenGui-dependent UI TUs (Rando/Menu.cpp, Rando/CheckTracker/*) into
# 2ship_rando_ui, which stays deliberately elided with the rest of MM's menu
# surface — report-only below keeps that exposure visible.
check_archive "$BUILD_DIR/games/mm/lib2ship_rando.a" required
check_archive "$BUILD_DIR/games/mm/lib2ship_enh.a"      report-only
check_archive "$BUILD_DIR/games/mm/lib2ship_rando_ui.a" report-only

# Lane C0 reachability probes (#392): beyond registrar symbols, assert MM's
# randomizer generation surface is actually IN the binary — the historical
# failure was 2ship_rando compiling green while the linker discarded every
# object. nm is the primary ground truth (per the header note, strings can
# false-negative on folded std::string constructions); the spoiler tag is a
# plain string literal, checked with strings as the belt-and-braces probe the
# Lane C0 brief calls for.
if ! nm --demangle "$BIN" 2>/dev/null | grep -q "Rando::Spoiler::GenerateFromSaveContext"; then
    echo "FAIL: Rando::Spoiler::GenerateFromSaveContext missing from redship — 2ship_rando was elided (#392)" >&2
    overall=1
fi
if ! strings "$BIN" 2>/dev/null | grep -q "2S2H_RANDO_SPOILER"; then
    echo "FAIL: 2S2H_RANDO_SPOILER tag missing from redship — MM spoiler writer not linked (#392)" >&2
    overall=1
fi

if [ "$overall" -ne 0 ]; then
    echo "FAIL: registrar symbols were elided from a WHOLE_ARCHIVE-protected archive (#341)" >&2
    exit 1
fi
echo "OK: no registrar elision in protected archives; 2ship_rando generation surface present"
