# Claude Code worker sandbox — extends the CI base image
# with Node.js + Claude Code for autonomous agent work.
#
# Build:  docker build -t rsbs-claude-sandbox .claude-sandbox/
# Usage:  .claude-sandbox/run-sandboxed-claude.sh --shell
#
FROM ghcr.io/spencerduncan/redshipblueship-build:latest

# ============================================================
# Claude Code + Node.js
# ============================================================
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && npm install -g @anthropic-ai/claude-code \
    && rm -rf /var/lib/apt/lists/*

# Python for tooling/tests
RUN apt-get update && apt-get install -y python3 python3-pip \
    && pip3 install pytest \
    && rm -rf /var/lib/apt/lists/*

# Git (needed for worker commits inside container)
RUN apt-get update && apt-get install -y gh \
    && rm -rf /var/lib/apt/lists/* \
    || true  # gh may not be in default repos

# ============================================================
# Non-root user for Claude Code
# ============================================================
RUN useradd -m -s /bin/bash claude \
    && chown -R claude:claude /home/claude

# Allow git operations on the mounted workspace (--system writes to
# /etc/gitconfig so the entry survives the USER switch below)
RUN git config --system --add safe.directory /workspace

USER claude

# Create the .claude dir so credentials can be mounted in
RUN mkdir -p /home/claude/.claude

WORKDIR /workspace
