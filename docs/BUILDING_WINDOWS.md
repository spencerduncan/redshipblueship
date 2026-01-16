# Building RedShipBlueShip on Windows

## Prerequisites

### Visual Studio 2022 (or Build Tools)

**Option A: Full IDE** - [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/) (free)

**Option B: Command-line only** - [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (scroll to "Tools for Visual Studio")

During installation, select:
- **Desktop development with C++**
- **C++ CMake tools for Windows** (Individual Components tab)
- **Windows 10/11 SDK** (Individual Components tab)

### Other Requirements

- [Git for Windows](https://git-scm.com/download/win)
- [Python 3.10+](https://www.python.org/downloads/) - **Check "Add Python to PATH"**
- Legally obtained OoT and MM ROMs

## Clone

```cmd
git clone https://github.com/spencerduncan/redshipblueship.git
cd redshipblueship
git submodule update --init --recursive
```

## Place ROMs

Copy your ROM files to the `OTRExporter/` directory with these exact names:
```
redshipblueship/
└── OTRExporter/
    ├── oot.z64    (OoT ROM - must be named exactly oot.z64)
    └── mm.z64     (MM ROM - must be named exactly mm.z64)
```

## Build

Open **x64 Native Tools Command Prompt for VS 2022** (not the regular Developer Prompt):

```cmd
cd path\to\redshipblueship

:: Clear VCPKG_ROOT (the prompt sets this to a broken path)
set VCPKG_ROOT=

:: Configure and build for OoT (extracts OoT assets)
cmake -B build-oot -G Ninja -DCMAKE_BUILD_TYPE=Release -DREDSHIP_BUILD_SHARED=ON -DGAME_STR=OoT
cmake --build build-oot --target ExtractAssets

:: Configure and build for MM (extracts MM assets)
cmake -B build-mm -G Ninja -DCMAKE_BUILD_TYPE=Release -DREDSHIP_BUILD_SHARED=ON -DGAME_STR=MM
cmake --build build-mm --target ExtractMMAssets

:: Build everything (use either build dir)
cmake --build build-oot -j%NUMBER_OF_PROCESSORS%
```

**Note:** ZAPD/OTRExporter must be compiled separately for OoT vs MM due to game-specific resource types. That's why we use two build directories for asset extraction.

The first build takes a while as vcpkg downloads dependencies.

## Run

```cmd
build\combo\redship.exe --game oot
```

Options:
- `--game oot` - Start with Ocarina of Time
- `--game mm` - Start with Majora's Mask
- `--test-entrance` - Use Mido's House for cross-game testing

## Troubleshooting

### "add_subdirectory given source ... which is not an existing directory"

A submodule didn't initialize. Fix:
```cmd
git submodule deinit -f <path>
git submodule update --init --recursive <path>
```

### "Python not found"

Reinstall Python with "Add to PATH" checked, or:
```cmd
cmake -B build ... -DPython3_EXECUTABLE=C:\Python311\python.exe
```

### Build fails with architecture mismatch

Make sure you're using **x64 Native Tools Command Prompt**, not the regular Developer Prompt (which defaults to x86).

### Link errors about missing symbols

```cmd
rmdir /s /q build
git submodule update --init --recursive
cmake -B build ...
```

### ZAPD "Unsupported argument" errors

If you see errors like `Unsupported argument: --customAssetsPath`, you have an old ZAPD build that doesn't have OTRExporter linked. Fix:

```cmd
:: Delete old Visual Studio build artifacts
rmdir /s /q x64
rmdir /s /q build

:: Reconfigure and rebuild
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DREDSHIP_BUILD_SHARED=ON
cmake --build build --target ZAPD
cmake --build build --target ExtractAssets
```

### Asset extraction picks wrong ROM

If OoT extraction runs MM assets (or vice versa), make sure your ROMs are named exactly `oot.z64` and `mm.z64`.
