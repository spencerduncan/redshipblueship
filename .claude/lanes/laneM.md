# Lane M — #670: MM never mounts its `mods/` folder in single-exe

**Branch:** `claude/670-mm-mods-folder-mount`

**Task:** Read #670 in full, PR #671 (`Combo_ExtensionCache_ScanGame`,
`MM_ExtensionRescan_AfterArchiveMount`), how OoT mounts `mods/` in
`games/oot/soh/OTRGlobals.cpp`, how MM would in `games/mm/2s2h/BenPort.cpp`
standalone, and `ArchiveManager::AddArchive` ordering in libultraship
(read-only; do not edit the submodule). Verify the premise in code first.
Deliver: in single-exe, MM's mod archives and loose `mods/` content mount
when MM's archives mount, in an order that lets a mod override
`mm.o2r`/`2ship.o2r` but never lets an MM mod shadow an OoT path or vice
versa (the two games share one resource manager: read
`docs/resource-namespace-audit.md` and `docs/asset-collision-analysis.md`,
and say how a colliding path is resolved). Decide and state where MM mods
live on disk (a shared `mods/` with per-game subfolders, or `mods/mm/`),
keep OoT's existing behaviour unchanged, and document it in
`docs/MODDING.md`. Locks: a redship-tier row with a tiny synthetic mod
archive proving the MM-side override applies, an OoT path is NOT shadowed by
an MM mod, and unmount/remount across a game switch keeps the mod applied.

**Files owned:** MM's archive-mount path in `games/mm/2s2h/` (function
granularity in `GameExports_SingleExe.cpp` / `BenPort.cpp`), `src/common`
mount helpers if one exists, `docs/MODDING.md`, a new test, shared
append-only files.

**Keywords:** `Fixes #670`.
