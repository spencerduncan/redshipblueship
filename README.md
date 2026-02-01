# RedShipBlueShip

A libultraship-based combo randomizer for Ocarina of Time and Majora's Mask.

**Red** = OoT cartridge color | **Blue** = MM cartridge color

## Overview

RedShipBlueShip combines [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) (OoT port) and [2Ship2Harkinian](https://github.com/HarbourMasters/2ship2harkinian) (MM port) into a single unified executable with cross-game randomization inspired by [OoTMM](https://ootmm.com).

## Features (Planned)

### MVP (In Development)
- [ ] Single executable running either OoT or MM
- [ ] Game switching (Happy Mask Shop ↔ Clock Tower)
- [ ] Basic cross-game item shuffle

### Future
- [ ] Full OoTMM feature parity (entrance rando, souls, hints, etc.)
- [ ] Archipelago multiworld support
- [ ] Extended platform support (Switch, Wii U)

## Building

### Requirements
- CMake 3.16+
- C++20 compiler
- Original OoT and MM ROMs for asset extraction

### Build Steps

```bash
# Clone with submodules
git clone --recursive https://github.com/sd/redshipblueship.git
cd redshipblueship

# Generate build files
cmake -B build -S .

# Build
cmake --build build --parallel

# Extract assets (requires ROMs in roms/ directory)
# TBD - asset extraction pipeline
```

## Project Structure

```
redshipblueship/
├── games/
│   ├── oot/          # Ocarina of Time (from Ship of Harkinian)
│   └── mm/           # Majora's Mask (from 2Ship2Harkinian)
├── combo/            # Cross-game systems
│   ├── GameManager/  # Game switching logic
│   ├── SharedState/  # Cross-game save state
│   ├── Randomizer/   # Combo randomizer
│   └── UI/           # Unified menu
├── libultraship/     # Shared engine (submodule)
├── ZAPDTR/           # Asset extraction
└── OTRExporter/      # OTR generation
```

## Documentation

- [Building](docs/BUILDING.md) - Build instructions
- [Error Handling Policy](docs/error-handling.md) - Error handling patterns and guidelines
- [Modding](docs/MODDING.md) - Modding guide
- [Custom Music](docs/CUSTOM_MUSIC.md) - Custom music support
- [Credits](docs/CREDITS.md) - Full credits

## Quick Start (Once Built)

1. Place `oot.z64` and `mm.z64` ROMs in the game directory
2. Run asset extraction to generate `oot.otr` and `mm.otr`
3. Launch `redshipblueship`
4. Select game or load existing combo save

## Default Controls

| N64 | A | B | Z | Start | Analog stick | C buttons | D-Pad |
| - | - | - | - | - | - | - | - |
| Keyboard | X | C | Z | Space | WASD | Arrow keys | TFGH |

### Shortcuts
| Keys | Action |
| - | - |
| ESC | Toggle menu |
| F11 | Fullscreen |
| Ctrl+R | Reset |

## Credits

- [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) - OoT PC port
- [2Ship2Harkinian](https://github.com/HarbourMasters/2ship2harkinian) - MM PC port
- [OoTMM](https://github.com/OoTMM/OoTMM) - Combo randomizer inspiration & logic reference
- [libultraship](https://github.com/kenix3/libultraship) - N64 compatibility layer
- [zeldaret](https://github.com/zeldaret) - OoT and MM decompilation projects

## License

This project is built upon the work of HarbourMasters and follows their licensing.

<a href="https://github.com/Kenix3/libultraship/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>
