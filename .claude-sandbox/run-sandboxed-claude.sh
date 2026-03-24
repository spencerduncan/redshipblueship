#!/usr/bin/env bash
# Run a headless Claude Code instance in a sandboxed Docker container.
#
# Uses your Max/Pro plan OAuth credentials from ~/.claude/.credentials.json
# — no API key needed.
#
# Usage:
#   ./run-sandboxed-claude.sh "implement feature X in combo/"
#   ./run-sandboxed-claude.sh --interactive    # drop into interactive Claude
#   ./run-sandboxed-claude.sh --shell          # drop into bash for debugging
#   ./run-sandboxed-claude.sh --worker 201     # work on issue #201
#
# Environment:
#   RSBS_IMAGE  - override image name (default: rsbs-claude-sandbox)
#   RSBS_GPU    - set to 1 for GPU passthrough (for running the game)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE_NAME="${RSBS_IMAGE:-rsbs-claude-sandbox}"
CLAUDE_HOME="${HOME}/.claude"

# Verify credentials exist
if [[ ! -f "$CLAUDE_HOME/.credentials.json" ]]; then
    echo "ERROR: No credentials found at $CLAUDE_HOME/.credentials.json"
    echo "Run 'claude' on the host and log in first."
    exit 1
fi

# Build image if it doesn't exist
if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "Building sandbox image..."
    docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"
fi

# Base docker args
DOCKER_ARGS=(
    --rm
    -v "$PROJECT_DIR:/workspace"
    -w /workspace
    # Persistent ccache volume
    -v redship-ccache:/root/.ccache
    # Mount credentials + onboarding state read-only
    -v "$CLAUDE_HOME/.credentials.json:/home/claude/.claude/.credentials.json:ro"
    -v "$CLAUDE_HOME/.claude.json:/home/claude/.claude/.claude.json:ro"
    # Git config for worker commits
    -e GIT_AUTHOR_NAME="Claude Worker"
    -e GIT_AUTHOR_EMAIL="noreply@anthropic.com"
    -e GIT_COMMITTER_NAME="Claude Worker"
    -e GIT_COMMITTER_EMAIL="noreply@anthropic.com"
)

# GPU passthrough if requested
if [[ "${RSBS_GPU:-0}" == "1" ]]; then
    DOCKER_ARGS+=(--gpus all -e DISPLAY="$DISPLAY" -v /tmp/.X11-unix:/tmp/.X11-unix)
fi

case "${1:-}" in
    --shell)
        echo "Dropping into container shell..."
        docker run -it --stop-timeout 3600 "${DOCKER_ARGS[@]}" "$IMAGE_NAME" bash
        ;;
    --interactive)
        echo "Starting interactive Claude Code..."
        docker run -it --stop-timeout 3600 "${DOCKER_ARGS[@]}" "$IMAGE_NAME" \
            claude --dangerously-skip-permissions
        ;;
    --worker)
        if [[ -z "${2:-}" ]]; then
            echo "Usage: $0 --worker <issue-number>"
            exit 1
        fi
        ISSUE_NUM="$2"
        BRANCH_NAME="claude/fix-issue-${ISSUE_NUM}"
        echo "Starting worker for issue #${ISSUE_NUM}..."

        # Check if the target branch already exists remotely
        if git ls-remote --exit-code --heads origin "$BRANCH_NAME" &>/dev/null; then
            echo "WARNING: Branch '$BRANCH_NAME' already exists on remote."
            echo "The worker may encounter conflicts or overwrite existing work."
        fi

        # Set up log capture
        LOG_DIR="$SCRIPT_DIR/logs"
        mkdir -p "$LOG_DIR"
        LOG_FILE="$LOG_DIR/worker-$(date +%Y%m%d-%H%M%S).log"
        echo "Logging worker output to: $LOG_FILE"

        # Get issue details from GitHub
        ISSUE_TITLE=$(gh issue view "$ISSUE_NUM" --json title -q .title 2>/dev/null || echo "Unknown")
        ISSUE_BODY=$(gh issue view "$ISSUE_NUM" --json body -q .body 2>/dev/null || echo "No description")

        PROMPT="You are an autonomous worker fixing issue #${ISSUE_NUM}: ${ISSUE_TITLE}

Issue description:
${ISSUE_BODY}

Instructions:
1. Create a branch: ${BRANCH_NAME}
2. Read the relevant code and understand the problem
3. Implement the fix
4. Build and test:
   cmake -B build -S . -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure --label-exclude '^integration$'
5. Commit your changes with a descriptive message
6. Push the branch (do NOT create a PR)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"

        docker run --stop-timeout 3600 "${DOCKER_ARGS[@]}" "$IMAGE_NAME" \
            claude -p "$PROMPT" \
            --dangerously-skip-permissions \
            --output-format stream-json \
            2>&1 | tee "$LOG_FILE"
        ;;
    "")
        echo "Usage: $0 <prompt> | --interactive | --shell | --worker <issue>"
        echo ""
        echo "Options:"
        echo "  <prompt>            Run headless Claude with the given task"
        echo "  --interactive       Start interactive Claude session in container"
        echo "  --shell             Drop into container bash for debugging"
        echo "  --worker <issue>    Work on a GitHub issue autonomously"
        echo ""
        echo "Environment:"
        echo "  RSBS_GPU=1          Enable GPU passthrough"
        exit 1
        ;;
    *)
        # Headless mode: pass prompt, get results
        echo "Running sandboxed Claude with prompt: $1"
        docker run --stop-timeout 3600 "${DOCKER_ARGS[@]}" "$IMAGE_NAME" \
            claude -p "$1" \
            --dangerously-skip-permissions \
            --output-format stream-json
        ;;
esac
