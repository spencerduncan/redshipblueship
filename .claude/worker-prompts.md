# Web Worker Prompts for RedShip Issues

## Issue #157 — MM Audio System Init for Cross-Game Switch

You are working on the `redshipblueship` project — a unified single executable that runs both Ocarina of Time (OoT) and Majora's Mask (MM) together. The repo is at `/home/sd/claude/redshipblueship`. You are on branch `claude/mm-init-debug`.

**Problem:** When OoT triggers a cross-game switch to MM, MM crashes during audio init at `AudioSfx_Init → AudioThread_ScheduleProcessCmds → osSendMesg`. The crash is a SIGSEGV because `gAudioCtx.threadCmdProcQueueP` is NULL.

**Root cause:** `MM_AudioLoad_Init` is stubbed as a no-op in `src/common/mm_stubs.c` (line 77). The real implementation at `games/mm/src/audio/lib/load.c:1150` calls `AudioThread_InitMesgQueues()` at line 1189, which calls `AudioThread_InitMesgQueuesInternal()` in `games/mm/src/audio/lib/thread.c:261`, which sets `gAudioCtx.threadCmdProcQueueP = &gAudioCtx.threadCmdProcQueue` at line 267 and creates the message queue at line 271.

The call chain that crashes:
1. `MM_Game_Init` (in `games/mm/2s2h/GameExports_SingleExe.cpp:65`) calls `MM_AudioMgr_Init`
2. `MM_AudioMgr_Init` (in `games/mm/src/code/audio_thread_manager.c:110`) calls `MM_Audio_Init()` then `MM_Audio_InitSound()`
3. `MM_Audio_Init()` (in `games/mm/src/audio/code_8019AF00.c:6499`) calls `MM_AudioLoad_Init(NULL, 0)` — **STUBBED**
4. `MM_Audio_InitSound()` calls `AudioSfx_Init(10)` which calls `AudioThread_ScheduleProcessCmds()` which uses `gAudioCtx.threadCmdProcQueueP` — **NULL → CRASH**

**What to do:**

The real `MM_AudioLoad_Init` in `games/mm/src/audio/lib/load.c:1150` does extensive work: zeroes `gAudioCtx`, initializes message queues, allocates audio heap pools, loads audio tables from resource archives via `ResourceMgr_ListFiles("audio/sequences*", ...)`, etc.

The problem is that `MM_AudioLoad_Init` calls functions that depend on the resource manager being initialized with MM's archives (mm.o2r). In single-exe mode, OoT initialized Ship::Context with OoT archives (oot.o2r, soh.o2r). MM archives haven't been loaded yet.

**Your task:** Remove the `MM_AudioLoad_Init` stub from `src/common/mm_stubs.c` and make the real implementation at `games/mm/src/audio/lib/load.c:1150` work in single-exe mode. This requires:

1. **Remove the stub** for `MM_AudioLoad_Init` (and related stubs: `MM_AudioLoad_InitAsyncLoads`, `MM_AudioLoad_InitSampleDmaBuffers`, `MM_AudioLoad_InitScriptLoads`, `MM_AudioLoad_InitSlowLoads`) from `src/common/mm_stubs.c` lines 77-81.

2. **Ensure MM's audio tables are available**: The real `MM_AudioLoad_Init` at line 1256 calls `ResourceMgr_ListFiles("audio/sequences*", ...)` which requires MM's resource archives to be loaded. Before audio init, you need to ensure MM's archive (mm.o2r) is loaded into the existing Ship::Context's ArchiveManager:
   ```cpp
   auto archiveMgr = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
   archiveMgr->AddArchive("mm.o2r");
   ```
   Add this to `MM_Game_Init` in `games/mm/2s2h/GameExports_SingleExe.cpp` before the `MM_AudioMgr_Init` call. The `AddArchive` API is at `libultraship/include/ship/resource/archive/ArchiveManager.h:23`.

3. **Handle the audio heap**: `MM_AudioLoad_Init` uses `MM_gAudioHeap` (a global audio heap pointer). Make sure MM's heaps are allocated before audio init — this is already done (`MM_Heaps_Alloc()` is called at line 74).

4. **Handle dependencies**: `MM_AudioLoad_Init` calls `MM_AudioHeap_AllocZeroed`, `MM_AudioHeap_ResetStep`, `AudioHeap_InitMainPool`, `MM_osCartRomInit` — these need real implementations. Check `src/common/mm_stubs.c` for any that are stubbed and remove stubs as needed. Also check `games/mm/src/audio/lib/heap.c` and `games/mm/src/audio/lib/load.c` for the real implementations.

5. **Test**: After removing stubs and loading archives, rebuild with:
   ```bash
   cd /home/sd/claude/redshipblueship/build-cmake
   cmake .. && cmake --build . --target redship -j$(nproc)
   ```
   Then run: `echo "1" | ./redship` (select OoT, load save, walk into Mido's House to trigger cross-game switch to MM).

**Build system:** This is a CMake project. The single-exe target is `redship`. Build flag `RSBS_SINGLE_EXECUTABLE` is defined. MM symbols are prefixed with `MM_` by a namespace tool (e.g., `osCreateMesgQueue` → `MM_osCreateMesgQueue`). These aliases are in `src/common/mm_stubs.c` which redirect to the shared libultraship implementations.

**Key files:**
- `src/common/mm_stubs.c` — stubs to remove
- `games/mm/src/audio/lib/load.c:1150` — real `MM_AudioLoad_Init`
- `games/mm/src/audio/lib/thread.c:261` — `AudioThread_InitMesgQueuesInternal` (sets queue pointers)
- `games/mm/src/audio/code_8019AF00.c:6499` — `MM_Audio_Init` call chain
- `games/mm/src/code/audio_thread_manager.c:110` — `MM_AudioMgr_Init`
- `games/mm/2s2h/GameExports_SingleExe.cpp` — `MM_Game_Init` where archive loading should go
- `libultraship/include/ship/resource/archive/ArchiveManager.h` — `AddArchive` API

Push your changes when done. Do NOT create a PR.

---

## Issue #158 — MM Graphics/Window Bridge for Single-Exe

You are working on the `redshipblueship` project — a unified single executable running OoT + MM. Repo at `/home/sd/claude/redshipblueship`, branch `claude/mm-init-debug`.

**Problem:** When OoT switches to MM via a cross-game entrance, MM needs to share OoT's existing SDL window and OpenGL context. OoT's `InitOTR()` creates Ship::Context which owns the Window (SDL2 + OpenGL). MM must NOT create a second window.

**Current state:** There's already infrastructure for shared graphics in gfx_sdl2. The crash log shows:
```
[GFX_SDL2] Checking for shared graphics from combo library...
[GFX_SDL2] Weak symbol is NULL - combo not linked
[GFX_SDL2] SharedGraphics result: has=false, windowID=0, ctx=0x0
[GFX_SDL2] Created new window, storing for sharing: ID=1
[GFX_SDL2] Combo_SetSharedGraphics weak symbol is NULL
```

This shows OoT creates a window (ID=1) and tries to store it for sharing via `Combo_SetSharedGraphics` but the weak symbol is NULL.

**Your task:** Make MM's graphics subsystem reuse OoT's existing SDL window and OpenGL context instead of creating new ones.

1. **Investigate the existing sharing mechanism** in `libultraship/src/ship/port/sdl2/gfx_sdl2.cpp` — search for `SharedGraphics`, `Combo_SetSharedGraphics`, `Combo_GetSharedGraphics`. Understand the weak symbol pattern.

2. **Implement the combo bridge functions**: `Combo_SetSharedGraphics` and `Combo_GetSharedGraphics` need to be implemented (not weak/NULL). These should store/retrieve the SDL window ID and GL context pointer. Implement them in `src/common/` somewhere appropriate (e.g., a new `graphics_bridge.c` or in an existing common file).

3. **Ensure MM's gfx init path checks for shared graphics**: When MM calls into gfx_sdl2 init, it should find the shared graphics from OoT and skip creating a new window.

4. **Update CMake** if new files are created — add to `CMake/SingleExecutable.cmake` in the `REDSHIP_COMMON_SOURCES` list.

**Key files to investigate:**
- `libultraship/src/ship/port/sdl2/gfx_sdl2.cpp` — shared graphics check
- `src/common/mm_stubs.c` — may have relevant stubs
- `games/mm/2s2h/BenPort.cpp` — MM's `InitOTR()` which creates Ship::Context
- `games/mm/2s2h/GameExports_SingleExe.cpp` — MM's init for single-exe mode (skips InitOTR)

**Build:** `cd /home/sd/claude/redshipblueship/build-cmake && cmake .. && cmake --build . --target redship -j$(nproc)`

Push your changes when done. Do NOT create a PR.

---

## Issue #159 — MM BenPort/Resource Manager Init for Single-Exe

You are working on `redshipblueship` — unified OoT+MM single executable. Repo at `/home/sd/claude/redshipblueship`, branch `claude/mm-init-debug`.

**Problem:** In standalone MM (2Ship2Harkinian), `InitOTR()` in `games/mm/2s2h/BenPort.cpp:728` creates Ship::Context, registers resource factories for MM-specific types (Animation, CollisionHeader, AudioSample, AudioSequence, AudioSoundFont, etc.), and sets up the GUI. In single-exe mode, `MM_Game_Init` in `GameExports_SingleExe.cpp` skips `InitOTR()` entirely because OoT already created Ship::Context.

But MM needs its resource factories registered so that MM-specific resources (stored in mm.o2r) can be deserialized. Without this, any MM code that loads resources via `ResourceMgr_LoadResource*` will fail for MM-specific types.

**Your task:** Create a lightweight `MM_InitResourceFactories()` function that registers MM's resource factories into the existing Ship::Context without creating a new context or window.

1. **Study MM's InitOTR()** in `games/mm/2s2h/BenPort.cpp:728`. Identify which resource factory registrations are needed. Look for `ResourceManager::RegisterResourceFactory` calls. You need to replicate those registrations.

2. **Study the resource types** in `games/mm/2s2h/resource/type/` — these are the MM-specific types that need factories. Also look at `games/mm/2s2h/resource/type/2shResourceType.h` for the type enum.

3. **Create `MM_InitResourceFactories()`** — either in `GameExports_SingleExe.cpp` or a new helper file. This function should:
   - Get the existing `Ship::Context::GetInstance()->GetResourceManager()`
   - Register all MM-specific resource factories
   - NOT create a new context, window, or audio player

4. **Call it from `MM_Game_Init`** in `GameExports_SingleExe.cpp` before audio init (since audio init needs to load audio resources).

5. **Also load MM's archive** (mm.o2r) into the ArchiveManager if not already done:
   ```cpp
   auto archiveMgr = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
   archiveMgr->AddArchive("mm.o2r");
   ```
   Note: This may overlap with Issue #157 (audio init). If #157 already adds archive loading, just ensure factories are registered before resources are loaded.

**Key files:**
- `games/mm/2s2h/BenPort.cpp:728` — MM's full `InitOTR()` (reference for what to extract)
- `games/mm/2s2h/resource/type/` — MM resource types and factories
- `games/mm/2s2h/GameExports_SingleExe.cpp` — where to call the new function
- `libultraship/include/ship/resource/ResourceManager.h` — `RegisterResourceFactory` API

**Build:** `cd /home/sd/claude/redshipblueship/build-cmake && cmake .. && cmake --build . --target redship -j$(nproc)`

Push your changes when done. Do NOT create a PR.

---

## Issue #160 — OoT Cleanup on Suspend (Audio, Threads)

You are working on `redshipblueship` — unified OoT+MM single executable. Repo at `/home/sd/claude/redshipblueship`, branch `claude/mm-init-debug`.

**Problem:** When OoT suspends (game switch to MM), `OoT_Game_Suspend()` in `games/oot/soh/GameExports_SingleExe.cpp:82` is currently a no-op. It preserves Ship::Context (correct), but it doesn't stop OoT's audio playback. This means:
- OoT audio may continue playing over MM
- OoT's audio thread may conflict with MM's audio thread
- OoT timers and background threads may keep running

**Your task:** Implement proper cleanup in `OoT_Game_Suspend()` to pause OoT subsystems without destroying them.

1. **Stop OoT audio**: Find OoT's audio shutdown/pause mechanism. Look in:
   - `games/oot/soh/OTRGlobals.cpp` — search for `DeinitOTR` to see what it cleans up
   - `games/oot/src/code/audio/` — OoT audio system
   - `libultraship/` — Ship::Context audio player API

   The goal is to pause/mute audio, not destroy the audio system. Something like:
   ```cpp
   auto audio = Ship::Context::GetInstance()->GetAudio();
   if (audio) audio->Stop(); // or Pause()
   ```

2. **Stop OoT scheduler/threads**: OoT may have background threads for scheduler, audio, etc. Check:
   - `games/oot/src/code/graph.c` — game loop / scheduler
   - The OoT `Main()` function — when it returns, the game loop stops but threads may still be running

3. **Implement matching `OoT_Game_Resume()`**: When switching back to OoT, resume audio playback and any paused subsystems.

4. **Do the same for MM**: `MM_Game_Suspend()` in `games/mm/2s2h/GameExports_SingleExe.cpp:136` is also a no-op. Implement MM audio pause there too.

**Key files:**
- `games/oot/soh/GameExports_SingleExe.cpp:82` — `OoT_Game_Suspend` (modify this)
- `games/oot/soh/OTRGlobals.cpp` — search for `DeinitOTR` for cleanup patterns
- `games/mm/2s2h/GameExports_SingleExe.cpp:136` — `MM_Game_Suspend` (modify this)
- `libultraship/include/ship/audio/AudioPlayer.h` — audio API

**Build:** `cd /home/sd/claude/redshipblueship/build-cmake && cmake .. && cmake --build . --target redship -j$(nproc)`

Push your changes when done. Do NOT create a PR.

---

## Issue #154 — Resource Archive Hot-Swapping

You are working on `redshipblueship` — unified OoT+MM single executable. Repo at `/home/sd/claude/redshipblueship`, branch `claude/mm-init-debug`.

**Problem:** OoT initializes Ship::Context with OoT's archives (oot.o2r, soh.o2r). When switching to MM, MM's archive (mm.o2r) needs to be loaded. When switching back to OoT, MM's archive should ideally be unloaded (or at minimum, OoT's resources shouldn't collide with MM's).

Currently there's no archive hot-swap mechanism. OoT's archives are loaded at startup and never change.

**Your task:** Implement per-game archive loading/unloading during game switches.

1. **Study the ArchiveManager API** in `libultraship/include/ship/resource/archive/ArchiveManager.h`:
   - `AddArchive(const std::string& archivePath)` — adds an archive
   - Check if there's a `RemoveArchive` or `UnloadArchive` method. If not, you may need to add one.

2. **Study the ResourceManager** in `libultraship/include/ship/resource/ResourceManager.h`:
   - Resources are cached by hash. When switching games, the cache may contain stale resources from the previous game.
   - Check if there's a cache clear/invalidate method.

3. **Implement archive management in game lifecycle**:
   - In `MM_Game_Init` (or a new helper): Load mm.o2r via `AddArchive`
   - In `MM_Game_Suspend`: Optionally unload mm.o2r
   - In `OoT_Game_Resume`: Ensure oot.o2r/soh.o2r are still loaded
   - Consider: Do resources from both games conflict? They use different resource paths (MM uses `audio/sequences*`, OoT uses similar but different paths). If paths don't collide, maybe both can stay loaded.

4. **Handle resource cache**: If there's a ResourceManager cache, ensure switching games doesn't serve stale resources. You may need to flush the cache on game switch.

**Key files:**
- `libultraship/include/ship/resource/archive/ArchiveManager.h` — archive management API
- `libultraship/src/ship/resource/archive/ArchiveManager.cpp` — implementation (check for remove/unload)
- `libultraship/include/ship/resource/ResourceManager.h` — resource caching
- `games/oot/soh/GameExports_SingleExe.cpp` — OoT lifecycle hooks
- `games/mm/2s2h/GameExports_SingleExe.cpp` — MM lifecycle hooks

**Build:** `cd /home/sd/claude/redshipblueship/build-cmake && cmake .. && cmake --build . --target redship -j$(nproc)`

Push your changes when done. Do NOT create a PR.

---

## Issue #155 — GameInteractor Singleton Collision

You are working on `redshipblueship` — unified OoT+MM single executable. Repo at `/home/sd/claude/redshipblueship`, branch `claude/mm-init-debug`.

**Problem:** Both OoT and MM have their own `GameInteractor` class with a static `Instance` pointer. In the single-exe build, both sets of GameInteractor code are linked. When MM initializes, its `GameInteractor::Instance` may collide with OoT's, causing hooks from one game to fire in the other, or crashes from accessing the wrong game's state.

**Current state:** In single-exe mode, MM's GameInteractor functions are currently **stubbed** in `src/common/mm_stubs.c` (lines 108-141). This means MM's game loop calls like `GameInteractor_ExecuteOnGameStateUpdate` are no-ops. This works for now but means MM enhancements won't function.

OoT's GameInteractor is at: `games/oot/soh/Enhancements/game-interactor/GameInteractor.h`
MM's GameInteractor is at: `games/mm/2s2h/GameInteractor/GameInteractor.h`

Both use `GameInteractor::Instance` as a static singleton.

**Your task:** Design and implement a solution so both GameInteractors can coexist. Options:

**Option A (Recommended — Namespace separation):** MM's symbols are already prefixed with `MM_` by the namespace tool, but the C++ class `GameInteractor` itself may not be namespaced. Check if the namespace tool handles C++ classes. If not, you may need to manually namespace MM's GameInteractor (e.g., `namespace MM { class GameInteractor { ... }; }`).

**Option B (Active game gating):** Keep a single GameInteractor but gate hooks by active game. Add a `GameId activeGame` field and skip hook execution when the wrong game is active.

**Option C (Keep stubs for now):** If the collision is benign with current stubs, document why and mark as deferred. The stubs in mm_stubs.c effectively disable MM's GameInteractor.

**Investigation steps:**
1. Check how the namespace tool handles C++ classes: `grep -r "class GameInteractor" games/mm/` — is the class itself renamed?
2. Check for symbol collisions: are there duplicate `GameInteractor` symbols in the linked binary? Use `nm` on object files.
3. If OoT's GameInteractor and MM's are already separate symbols (due to namespace tool), the stubs may be sufficient.

**Key files:**
- `games/oot/soh/Enhancements/game-interactor/GameInteractor.h` — OoT version
- `games/mm/2s2h/GameInteractor/GameInteractor.h` — MM version
- `games/mm/2s2h/GameInteractor/GameInteractor.cpp` — MM implementation (uses `GameInteractor::Instance` extensively)
- `src/common/mm_stubs.c:108-141` — current stubs
- `games/mm/2s2h/BenPort.cpp` — MM's InitOTR creates GameInteractor::Instance

**Build:** `cd /home/sd/claude/redshipblueship/build-cmake && cmake .. && cmake --build . --target redship -j$(nproc)`

Push your changes when done. Do NOT create a PR.
