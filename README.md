# RedShipBlueShip

Ocarina of Time and Majora's Mask in one executable, with cross-game randomization.

RedShipBlueShip links [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) and
[2Ship2Harkinian](https://github.com/HarbourMasters/2ship2harkinian) into a single `redship`
binary. A randomized file spans both games: items from either game can be placed in either
world, and you move between them in-game through the Happy Mask Shop and the Clock Tower.
The shuffle design follows [OoTMM](https://ootmm.com).

The project is in alpha. Pre-release builds for Windows, Linux and macOS are on the
[releases page](https://github.com/spencerduncan/redshipblueship/releases). Read
[docs/known-issues.md](docs/known-issues.md) before playing, and back up your saves.

## Running a release

1. Download the package for your platform and unpack it.
2. Place your Ocarina of Time and Majora's Mask ROMs next to `redship`. The first launch
   finds them, or asks you to browse for them, and generates `oot.o2r` and `mm.o2r`.
   Supported ROM hashes are listed in [docs/supportedHashes.json](docs/supportedHashes.json).
3. Launch `redship`. It boots into Ocarina of Time; `--game mm` boots into Majora's Mask.

## Building from source

Requires CMake 3.26+, a C++20 compiler, Python 3, and at least 8 GB of RAM. Platform
details are in [docs/BUILDING.md](docs/BUILDING.md) and
[docs/BUILDING_WINDOWS.md](docs/BUILDING_WINDOWS.md).

```bash
git clone --recursive https://github.com/spencerduncan/redshipblueship.git
cd redshipblueship
cmake -B build -S .
cmake --build build --parallel
```

A source build has no packaged `assets/` folder, so it cannot generate the archives on first
launch. Generate them from the build tree instead, then copy `oot.o2r` and `mm.o2r` next
to `redship`:

```bash
# ROM at OTRExporter/oot.z64
cmake --build build --target ExtractAssets
# ROM at OTRExporter/mm.z64
cmake --build build --target ExtractMMAssets
```

See [docs/mm-archive-setup.md](docs/mm-archive-setup.md) for alternatives.

Tests run with `ctest --test-dir build`.

## Default controls

| N64      | A | B | Z | Start | Stick | C buttons  | D-pad |
| -------- | - | - | - | ----- | ----- | ---------- | ----- |
| Keyboard | X | C | Z | Space | WASD  | Arrow keys | TFGH  |

Esc opens the menu, F11 toggles fullscreen, Ctrl+R resets.

## Layout

```
games/oot/      Ocarina of Time (Ship of Harkinian)
games/mm/       Majora's Mask (2Ship2Harkinian)
src/common/     Shared logic: game lifecycle, cross-game state, entrances, combo randomizer
rsbs/           Entry point
libultraship/   N64 compatibility layer (submodule)
ZAPDTR/         Asset extraction (submodule)
OTRExporter/    Archive generation (submodule)
docs/           Build guides, modding, versioning, design records, known issues
```

## Documentation

- [Known issues](docs/known-issues.md)
- [Modding](docs/MODDING.md) and [custom music](docs/CUSTOM_MUSIC.md)
- [Versioning](docs/VERSIONING.md)
- [Architecture decision records](docs/adr)
- [Credits](docs/CREDITS.md)

## License

RedShipBlueShip's own code is MIT licensed; [LICENSE](LICENSE) states which paths that covers.
The game ports, submodules, build dependencies and fonts keep their own licenses, inventoried
in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Read it before redistributing a build.

No Nintendo assets are included. Everything is extracted from ROMs you supply.

Built on [libultraship](https://github.com/Kenix3/libultraship) and the
[zeldaret](https://github.com/zeldaret) decompilations.
