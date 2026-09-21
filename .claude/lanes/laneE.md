# Lane E — #676: shared-sccache /showIncludes hazard

**Branch:** `claude/676-sccache-showincludes-deps`

**Task:** sccache caches MSVC's `/showIncludes` stdout with the object and
replays it on a cross-worktree hit, so ninja's dependency records can name a
different worktree's header paths — a header edit then never triggers a
rebuild (phantom LNK2019, or worse, silently stale objects). Reproduce
deterministically in a script; evaluate candidate fixes (sccache
direct-mode/config options, worktree-specific cache keys,
`CMAKE_DEPENDS_USE_COMPILER=OFF` / `/sourceDependencies`, per-worktree
`SCCACHE_DIR`) with measured hit-rate cost for each; land the one that
defeats the repro without silently disabling caching for everyone. Never
touch the global sccache config, clear the shared cache, or touch other
worktrees.

**Files owned:** `CMakeLists.txt` / `CMake/DefaultCXX.cmake` (sccache
conditional block only), `docs/BUILDING_WINDOWS.md`,
`.claude/worker-prompts.md` (one bullet under Standing conventions), optional
`scripts/` tool.

**Keywords:** `Fixes #676` if the repro is defeated in-tree; otherwise
`Refs #676` with the measured options table.
