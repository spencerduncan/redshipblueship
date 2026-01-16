# Building RedShipBlueShip on Windows

This guide assumes you have nothing installed and walks through the full setup.

## Prerequisites

### 1. Visual Studio 2022

Download and install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/) (free).

During installation, select these workloads:
- **Desktop development with C++**

And these individual components (in the "Individual components" tab):
- **C++ CMake tools for Windows**
- **Windows 10/11 SDK** (latest version)

### 2. Git

Download and install [Git for Windows](https://git-scm.com/download/win).

Use default options. This gives you Git Bash which is useful.

### 3. Python 3

Download and install [Python 3](https://www.python.org/downloads/) (3.10+ recommended).

**IMPORTANT:** Check "Add Python to PATH" during installation.

### 4. CMake (if not using VS CMake)

Visual Studio includes CMake, but if you want command-line builds:

Download and install [CMake](https://cmake.org/download/) (3.26+).

Check "Add CMake to PATH" during installation.

## Clone the Repository

Open Git Bash or Command Prompt:

```bash
git clone https://github.com/spencerduncan/redshipblueship.git
cd redshipblueship
git submodule update --init --recursive
```

## Building

### Option A: Visual Studio (Recommended for Windows devs)

1. Open Visual Studio 2022
2. Click "Open a local folder"
3. Select the `redshipblueship` folder
4. Wait for CMake to configure (watch the Output window)
5. Select build target from the dropdown (Debug/Release)
6. Build > Build All (or Ctrl+Shift+B)

The first build will take a while as vcpkg downloads and builds dependencies.

### Option B: Command Line

Open **Developer Command Prompt for VS 2022** (search in Start menu):

```cmd
cd path\to\redshipblueship

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release -j%NUMBER_OF_PROCESSORS%
```

Or with Ninja (faster incremental builds):

```cmd
# Configure with Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j%NUMBER_OF_PROCESSORS%
```

## Build Targets

- `soh` - Ocarina of Time (Ship of Harkinian)
- `2s2h` - Majora's Mask (2Ship2Harkinian)
- `redship` - Combo launcher (requires `-DREDSHIP_BUILD_SHARED=ON`)

### Building the Combo Launcher

The combo launcher requires building games as shared libraries:

```cmd
cmake -B build-shared -G "Visual Studio 17 2022" -A x64 -DREDSHIP_BUILD_SHARED=ON
cmake --build build-shared --config Release
```

## Asset Extraction

You need ROM files to generate game assets. Place your legally obtained ROMs in `games/oot/` then:

```cmd
cmake --build build --target ExtractAssets
```

This creates the `.o2r` asset files needed to run the games.

## Running

After building and extracting assets:

```cmd
# Run OoT
build\games\oot\Release\soh.exe

# Run MM
build\games\mm\Release\2s2h.exe

# Run combo launcher (if built with REDSHIP_BUILD_SHARED)
build-shared\combo\Release\redship.exe --game oot
```

## Troubleshooting

### "vcpkg bootstrap failed"
- Make sure you have internet access
- Try running as Administrator
- Check that Visual Studio C++ tools are installed

### "Python not found"
- Reinstall Python with "Add to PATH" checked
- Or set `Python3_EXECUTABLE` in CMake: `-DPython3_EXECUTABLE=C:\Python311\python.exe`

### "CMake version too old"
- Install CMake 3.26+ from cmake.org
- Or update Visual Studio

### Link errors about missing symbols
- Clean build: delete the `build` folder and reconfigure
- Make sure all submodules are initialized: `git submodule update --init --recursive`

### Build is incredibly slow
- Use Ninja generator instead of Visual Studio
- Enable parallel builds: `-j%NUMBER_OF_PROCESSORS%`
- Consider using `sccache` for caching

## Development Tips

### Using VS Code instead of Visual Studio

1. Install [VS Code](https://code.visualstudio.com/)
2. Install extensions: C/C++, CMake Tools
3. Open the folder in VS Code
4. Select kit: "Visual Studio Community 2022 Release - amd64"
5. Configure and build from the CMake panel

### Faster iteration

For faster builds during development:
- Build only the target you're working on: `cmake --build build --target soh`
- Use Debug config for faster compile, Release for testing performance
- Use Ninja generator
