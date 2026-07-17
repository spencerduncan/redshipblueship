# CLAUDE.md

RedShipBlueShip is a libultraship-based combo that combines Ocarina of Time (Ship of Harkinian) and Majora's Mask (2Ship2Harkinian) into a single executable with cross-game randomization inspired by OoTMM.

## Build

```bash
git submodule update --init    # libultraship, ZAPDTR, OTRExporter
cmake -B build -S .
cmake --build build --parallel
```

- CMake 3.26+, C++20, C23
- Single-exe target: `redship` (`SINGLE_EXECUTABLE_BUILD=ON` by default)
- Requires original OoT and MM ROMs for asset extraction
- 8GB+ RAM recommended (4GB causes compiler failures)
- CMake options: `SUPPRESS_WARNINGS` (ON by default), `USE_LLD_LINKER` (OFF by default)

## Testing

```bash
ctest --test-dir build                # run all tests
./build/redship --test <name>         # run specific test
```

Named CTest targets (60s timeout): BootOoT, BootMM, SwitchOoTMM, SwitchMMOoT, Roundtrip, Context, AllTests

Test source: `src/common/tests/`

CI: GitHub Actions — builds, clang-format, static analysis, multi-distro testing

## Code Style

clang-format 14 (configs at `games/oot/.clang-format`, `games/mm/.clang-format`):
- 4-space indent, no tabs, 120-col limit
- Left-aligned pointers, attached braces
- Run: `./run-clang-format.sh` (Linux/macOS) or `./run-clang-format.ps1` (Windows)

clang-tidy: `readability-braces-around-statements`, header filter `(src|include)/.*\.h$`

Naming:
- Functions: C-style underscores — `Game_Init`, `Context_FreezeState`
- Types: CamelCase — `GameId`, `SaveContext`
- Macros: UPPER_SNAKE_CASE
- MM symbols prefixed `MM_` in single-exe builds to avoid linker collisions

## Architecture

**Single executable**: both OoT and MM are compiled as object libraries and linked into one `redship` binary.

**Shared Ship::Context**: OoT creates the Ship::Context (SDL window, GL context, resource manager). MM reuses it — no second window.

**Resource archives**: `.o2r` format — `oot.o2r`, `mm.o2r`, `soh.o2r`. Archives are loaded/swapped via `ArchiveManager::AddArchive`.

**Game lifecycle** (`src/common/game_lifecycle.h`):
`Game_Init` → `Game_Run` → `Game_Suspend`/`Game_Resume` → `Game_Shutdown`

**Cross-game state**: `Context_FreezeState` / `Context_RestoreState` (`src/common/context.h`) preserve SaveContexts across game switches. Blob capacities come from `src/common/game.h` (`OOT_SAVE_CONTEXT_SIZE` 0x22000, `MM_SAVE_CONTEXT_SIZE` 0x10000 — the ports' runtime structs are far larger than the N64 sizes); each game's `GameExports_SingleExe.cpp` static-asserts `sizeof(SaveContext)` fits. Both shadows are kept in memory.

**MM stubs**: `src/common/mm_stubs.c` has stubs for MM functions not yet ported to single-exe mode. When working on MM integration, check this file for functions that may need real implementations.

**Game switching**: entrance-based — Happy Mask Shop ↔ Clock Tower. See `src/common/entrance.h`.

## Key Directories

```
games/oot/          # OoT source (Ship of Harkinian)
games/mm/           # MM source (2Ship2Harkinian)
src/common/         # Unified logic (lifecycle, context, entrance, menu, stubs, shared graphics)
rsbs/               # RedShipBlueShip entry point (main.cpp)
libultraship/       # N64 compat layer (submodule)
CMake/              # Build config (SingleExecutable.cmake, etc.)
docs/               # Build guides, modding docs, versioning
```

## Docker / Sandboxed Builds

**CI base image** (pre-compiled deps, suitable for network-isolated runs):
```bash
# Image: ghcr.io/spencerduncan/redshipblueship-build:latest
# Dockerfile: .github/docker/build-base.Dockerfile
# All external downloads baked in at image build time

# Network-isolated build agent:
docker run --network=none \
  -v /path/to/repo:/workspace \
  -v ccache-vol:/root/.ccache \
  ghcr.io/spencerduncan/redshipblueship-build:latest \
  bash -c "cd /workspace && cmake -B build -S . && cmake --build build --parallel"
```

Prerequisites for isolated runs:
- Submodules must be initialized before mounting (`git submodule update --init`)
- ccache volume recommended for build caching across runs
- APT deps list: `.github/workflows/apt-deps.txt`

**Dev container**: `.devcontainer/` — VS Code Remote, same dep versions as CI

## Conventions for Claude Agents

- Work branches: `claude/<description>` prefix
- Worker task descriptions: `.claude/worker-prompts.md`
- Pushes, PRs, and merges are automated: push your branch as you work, open a PR when the change is ready for CI, and squash-merge once CI is fully green — no need to ask
- Never merge red or partial CI; if blocked or uncertain, push the branch and report instead
- When modifying MM code in single-exe mode, check `src/common/mm_stubs.c` for related stubs
- Upstream repos: [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright), [2Ship2Harkinian](https://github.com/HarbourMasters/2ship2harkinian)
- Issue tracking: `gh issue list -R spencerduncan/redshipblueship`
