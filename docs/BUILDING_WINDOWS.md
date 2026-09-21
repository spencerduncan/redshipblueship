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

:: Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release 
:: Extract assets (both use their respective ZAPD variants automatically)
cmake --build build --target ExtractAssets      :: OoT (uses ZAPD)
cmake --build build --target ExtractMMAssets    :: MM (uses ZAPD_MM)

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

## sccache and several checkouts on one machine (#676)

If you build with `-DCMAKE_C_COMPILER_LAUNCHER=sccache` (and the C++ one) under
the Ninja generator, and you have more than one checkout or git worktree of this
repo on the machine sharing one sccache, read this.

**The hazard.** Ninja learns each object's header dependencies by parsing
`cl.exe`'s `/showIncludes` lines off stdout (`deps = msvc` in `build.ninja`).
`cl` prints every header the way the include search spelled it, and CMake's
Ninja generator passes absolute `-I` flags for every directory outside the build
tree, so those lines are absolute paths rooted in the checkout that ran the
compile. sccache stores the compiler's stdout beside the object and replays it
verbatim on a cache hit, and it deliberately keeps `-I` flags out of the cache
key (the preprocessed output it hashes is made with `/EP`, which carries no
paths). So two checkouts sharing one sccache hit each other's objects, and the
second one records the first one's header paths. Editing a header in the second
checkout then changes a file ninja is not watching, and the object is never
recompiled. You see either `LNK2019` on a symbol no source at HEAD mentions, or
nothing at all — a stale object that links and passes tests.

**What the build does about it.** `CMake/SccacheWorktreeDeps.cmake` detects
exactly that combination (MSVC + Ninja + an sccache launcher) and repoints the
compiler launcher at a small generated shim, `build/rsbs-sccache-tree.cmd`, that
sets sccache's `SCCACHE_C_CUSTOM_CACHE_BUSTER` to the source tree's path. An
object is then only ever served back to the checkout whose headers its
dependency records name. Configure prints:

```
-- sccache: partitioning the C/C++ cache key by source tree (#676): C:\path\to\your\checkout
```

Caching is not disabled. Every rebuild, branch switch and object-directory wipe
inside a checkout still hits the cache, and if you build one checkout — which is
every CI runner and every ordinary clone — the buster value is constant and your
hit rate is unchanged. What you give up is reuse *between* checkouts on the same
machine, which is the reuse that was producing wrong answers.

**Turning it off.** `-DRSBS_SCCACHE_TREE_PARTITION=OFF` restores cross-checkout
reuse. If you do that and you have more than one checkout, wipe the object
directories after every header change and after every merge, or expect stale
objects:

```cmd
:: from the build dir's parent
for /d %D in (build\games\mm\CMakeFiles\*.dir build\games\oot\CMakeFiles\*.dir) do rmdir /s /q "%D"
```

**Things that look like they would fix it and do not** (all measured on
sccache 0.17.0): `SCCACHE_DIRECT=false` — the replay still happens;
`SCCACHE_BASEDIR=<common ancestor>` — it rewrites preprocessor output, not the
stored `/showIncludes` stdout; a private `SCCACHE_DIR` per checkout — correct,
but it also splits the disk budget and the eviction pool. Relative `-I` flags
*would* fix it and keep cross-checkout reuse, but CMake's Ninja generator has no
supported way to emit them.

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
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release cmake --build build --target ZAPD
cmake --build build --target ExtractAssets
```

### Asset extraction picks wrong ROM

If OoT extraction runs MM assets (or vice versa), make sure your ROMs are named exactly `oot.z64` and `mm.z64`.
