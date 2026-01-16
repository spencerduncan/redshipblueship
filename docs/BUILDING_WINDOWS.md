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

Copy your ROM files to the `OTRExporter/` directory:
```
redshipblueship/
└── OTRExporter/
    ├── oot.z64    (or other OoT ROM name)
    └── mm.z64     (or other MM ROM name)
```

## Build

Open **x64 Native Tools Command Prompt for VS 2022** (not the regular Developer Prompt):

```cmd
cd path\to\redshipblueship

:: Clear VCPKG_ROOT (the prompt sets this to a broken path)
set VCPKG_ROOT=

:: Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DREDSHIP_BUILD_SHARED=ON

:: Extract assets (generates headers + .o2r files)
cmake --build build --target ExtractAssets      :: OoT assets
cmake --build build --target ExtractMMAssets    :: MM assets

:: Build everything
cmake --build build -j%NUMBER_OF_PROCESSORS%
```

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
