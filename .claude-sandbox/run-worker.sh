#!/usr/bin/env bash
# Dispatch one or more autonomous Claude Code workers for GitHub issues.
#
# Usage:
#   ./run-worker.sh 201                    # Work on issue #201
#   ./run-worker.sh --parallel 201 170 168 # Multiple workers in parallel
#   ./run-worker.sh --list                 # List worker-ready open issues
#
# Each worker:
#   1. Fetches issue details from GitHub
#   2. Creates a branch claude/fix-issue-<num>
#   3. Implements the fix inside a Docker container
#   4. Builds and runs tests
#   5. Pushes the branch

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DRY_RUN=0

list_issues() {
    echo "Open issues:"
    echo ""
    gh issue list -R spencerduncan/redshipblueship --state open --limit 30 \
        --json number,title,labels \
        --jq '.[] | "  #\(.number)\t\(.title)\t[\(.labels | map(.name) | join(", "))]"'
}

run_worker() {
    local issue_num="$1"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[dry-run] Would dispatch worker for issue #${issue_num}"
        echo "[dry-run]   Command: $SCRIPT_DIR/run-sandboxed-claude.sh --worker $issue_num"
        return 0
    fi
    echo "=== Dispatching worker for issue #${issue_num} ==="
    "$SCRIPT_DIR/run-sandboxed-claude.sh" --worker "$issue_num"
    echo "=== Worker for issue #${issue_num} complete ==="
}

# Parse leading flags
while [[ "${1:-}" == --dry-run ]]; do
    DRY_RUN=1
    shift
done

case "${1:-}" in
    --list)
        list_issues
        ;;
    --parallel)
        shift
        if [[ $# -eq 0 ]]; then
            echo "Usage: $0 [--dry-run] --parallel <issue1> <issue2> ..."
            exit 1
        fi
        pids=()
        issues=("$@")
        for issue in "$@"; do
            run_worker "$issue" &
            pids+=($!)
            echo "  Started worker PID ${pids[-1]} for issue #${issue}"
        done
        echo ""
        echo "Waiting for ${#pids[@]} workers..."
        failed=0
        for i in "${!pids[@]}"; do
            pid="${pids[$i]}"
            issue_num="${issues[$i]}"
            if wait "$pid"; then
                echo "  Worker for issue #${issue_num} (PID $pid): SUCCESS"
            else
                status=$?
                echo "  Worker for issue #${issue_num} (PID $pid): FAILED (exit $status)"
                failed=$((failed + 1))
            fi
        done
        echo ""
        echo "Done. $((${#pids[@]} - failed))/${#pids[@]} workers succeeded."
        exit $failed
        ;;
    "")
        echo "Usage: $0 [--dry-run] <issue-number> | --parallel <issues...> | --list"
        exit 1
        ;;
    *)
        run_worker "$1"
        ;;
esac
