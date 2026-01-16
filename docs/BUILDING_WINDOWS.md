# Building RedShipBlueShip on Windows

This guide walks through building the combo launcher that runs both OoT and MM.

## Prerequisites

### 1. Visual Studio 2022 (or Build Tools)

**Option A: Full IDE** - [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/) (free)

**Option B: Command-line only** - [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (scroll to "Tools for Visual Studio")

During installation, select these workloads:
- **Desktop development with C++**

And these individual components (in the "Individual components" tab):
- **C++ CMake tools for Windows**
- **Windows 10/11 SDK** (latest version)

Both options provide the MSVC compiler and required tools. Build Tools is smaller if you don't need the IDE.

### 2. Git

Download and install [Git for Windows](https://git-scm.com/download/win).

### 3. Python 3

Download and install [Python 3](https://www.python.org/downloads/) (3.10+ recommended).

**IMPORTANT:** Check "Add Python to PATH" during installation.

### 4. CMake (for command-line builds)

If using Build Tools instead of VS IDE, install [CMake](https://cmake.org/download/) (3.26+).

Check "Add CMake to PATH" during installation.

## Clone the Repository

```cmd
git clone https://github.com/spencerduncan/redshipblueship.git
cd redshipblueship
git submodule update --init --recursive
```

## Building

### Visual Studio IDE

1. Open Visual Studio 2022
2. Click "Open a local folder"
3. Select the `redshipblueship` folder
4. Open CMakeSettings.json and add `-DREDSHIP_BUILD_SHARED=ON` to CMake arguments
5. Wait for CMake to configure (watch the Output window)
6. Set startup item to `redship.exe`
7. Build > Build All (Ctrl+Shift+B)

### Command Line

Open **Developer Command Prompt for VS 2022**:

```cmd
cd path\to\redshipblueship

:: Clear VCPKG_ROOT - the Developer Prompt sets this to a non-git directory
set VCPKG_ROOT=

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DREDSHIP_BUILD_SHARED=ON
cmake --build build -j%NUMBER_OF_PROCESSORS%
```

Or with the Visual Studio generator (slower but doesn't require Ninja):

```cmd
set VCPKG_ROOT=
cmake -B build -G "Visual Studio 17 2022" -A x64 -DREDSHIP_BUILD_SHARED=ON
cmake --build build --config Release -j%NUMBER_OF_PROCESSORS%
```

The first build takes a while as vcpkg is cloned and dependencies are built.

## Asset Extraction

Place your legally obtained ROM files in `games/oot/` then:

```cmd
cmake --build build --target ExtractAssets
```

This creates the `.o2r` asset files needed to run.

## Running

```cmd
build\combo\Release\redship.exe --game oot
```

Options:
- `--game oot` - Start with Ocarina of Time
- `--game mm` - Start with Majora's Mask
- `--test-entrance` - Use Mido's House for cross-game testing (closer to spawn)

## Troubleshooting

### "vcpkg bootstrap failed"
- Make sure you have internet access
- Try running as Administrator
- Check that Visual Studio C++ tools are installed

### "Python not found"
- Reinstall Python with "Add to PATH" checked
- Or set `-DPython3_EXECUTABLE=C:\Python311\python.exe`

### "CMake version too old"
- Install CMake 3.26+ from cmake.org
- Or update Visual Studio

### Link errors about missing symbols
- Clean build: delete the `build` folder and reconfigure
- Make sure submodules are initialized: `git submodule update --init --recursive`

### Build is slow
- Use Ninja generator instead of Visual Studio generator
- Enable parallel builds: `-j%NUMBER_OF_PROCESSORS%`
