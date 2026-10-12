#!/usr/bin/env bash
#
# Formats the RSBS-enforced paths with clang-format 14: the RSBS-owned trees
# (src/common/, rsbs/) whole, plus a subset of the OoT and MM game trees.
#
# The enforced paths are listed in .github/clang-format-paths.txt. Inside the
# game trees it is an incremental allowlist, not a directory sweep. See the
# header of that file for why: turning the gate on for all of games/oot +
# games/mm at once would reformat ~1050 vendored decomp files and create
# permanent conflict surface against the upstream SoH / 2Ship trees.
#
# .clang-format configs live at games/oot/.clang-format,
# games/mm/.clang-format, src/common/.clang-format and rsbs/.clang-format (all
# the same style); clang-format discovers them per-file by walking up from
# each file, so every enforced file picks up its tree's config.
#
# CI runs this and then `git diff --exit-code`, so any file listed in the
# allowlist must be format-clean at HEAD.

set -euo pipefail

PATHS_FILE=".github/clang-format-paths.txt"

if command -v clang-format-14 > /dev/null 2>&1; then
    CLANG_FORMAT=clang-format-14
elif [ -x ./clang-format.exe ]; then
    CLANG_FORMAT=./clang-format.exe
elif command -v clang-format > /dev/null 2>&1 && clang-format --version | grep -q " 14\."; then
    CLANG_FORMAT=clang-format
else
    echo "error: clang-format 14 not found (install clang-format-14, or run run-clang-format.ps1 on Windows to download it)" >&2
    exit 1
fi

if [ ! -f "$PATHS_FILE" ]; then
    echo "error: $PATHS_FILE is missing — the format gate has no path list and would be a no-op" >&2
    exit 1
fi

# Strip comments and blank lines.
mapfile -t ENTRIES < <(grep -v '^[[:space:]]*#' "$PATHS_FILE" | grep -v '^[[:space:]]*$')

# Guard against this gate silently becoming a no-op again (it targeted a
# nonexistent soh/ directory for the whole life of the repo): an empty list
# is an error, and so is any entry that no longer exists on disk. A renamed
# or deleted file must fail loudly rather than silently shrink coverage.
if [ "${#ENTRIES[@]}" -eq 0 ]; then
    echo "error: $PATHS_FILE lists no paths — the format gate would be a no-op" >&2
    exit 1
fi

# An entry ending in '/' enrols a whole RSBS-owned tree: every tracked
# .c/.cc/.cpp/.h/.hpp file under it, so a new file there is enforced without
# anyone remembering to list it. Any other entry is a single file.
FILES=()
missing=0
for f in "${ENTRIES[@]}"; do
    if [[ "$f" == */ ]]; then
        if [ ! -d "$f" ]; then
            echo "error: $PATHS_FILE lists '$f', which does not exist (renamed or deleted?)" >&2
            missing=1
            continue
        fi
        mapfile -t TREE < <(git ls-files -- "$f" | grep -E '\.(c|cc|cpp|h|hpp)$')
        if [ "${#TREE[@]}" -eq 0 ]; then
            echo "error: $PATHS_FILE lists '$f', which holds no tracked C/C++ files" >&2
            missing=1
            continue
        fi
        FILES+=("${TREE[@]}")
    elif [ ! -f "$f" ]; then
        echo "error: $PATHS_FILE lists '$f', which does not exist (renamed or deleted?)" >&2
        missing=1
    else
        FILES+=("$f")
    fi
done
if [ "$missing" -ne 0 ]; then
    echo "error: fix the stale entries above, or the format gate silently loses coverage" >&2
    exit 1
fi

printf '%s\0' "${FILES[@]}" | xargs -0 "$CLANG_FORMAT" -i --verbose
