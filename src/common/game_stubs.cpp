/**
 * @file game_stubs.cpp
 * @brief Game entry point implementations for single-executable mode
 *
 * These provide the OoT_* and MM_* namespaced functions that the
 * single executable main.cpp expects. OoT stubs remain as placeholders
 * until full symbol namespacing is integrated. MM_Game_Init is a real
 * implementation that loads MM's resource archive into the existing
 * Ship::Context and initializes MM's audio subsystem.
 */

#include <cstdio>
#include <cstring>
#include <ship/Context.h>
#include <filesystem>

// ============================================================================
// OoT Stub Implementations
// ============================================================================

extern "C" {

int OoT_Game_Init(int argc, char** argv) {
    printf("[OoT STUB] Game_Init called (argc=%d)\n", argc);
    printf("[OoT STUB] This is a stub implementation for testing.\n");
    printf("[OoT STUB] Replace with actual OoT code when symbol namespacing is complete.\n");
    return 0;
}

void OoT_Game_Run(void) {
    printf("[OoT STUB] Game_Run called\n");
    printf("[OoT STUB] Game would run here. Press Ctrl+C to exit.\n");
}

void OoT_Game_Shutdown(void) {
    printf("[OoT STUB] Game_Shutdown called\n");
}

const char* OoT_Game_GetName(void) {
    return "Ocarina of Time (Stub)";
}

const char* OoT_Game_GetId(void) {
    return "oot";
}

// ============================================================================
// MM Implementations for single-executable cross-game switching
// ============================================================================

// MM subsystem functions (from MM game library)
void MM_Heaps_Alloc(void);
void MM_Heaps_Free(void);

// MM audio initialization chain
struct AudioMgr;
struct SchedContext;
struct IrqMgr;
typedef int OSPri;
typedef int OSId;
void MM_AudioMgr_Init(AudioMgr* audioMgr, void* stack, OSPri pri, OSId id,
                       SchedContext* sched, IrqMgr* irqMgr);

// MM globals needed for audio init
extern AudioMgr sAudioMgr;
extern SchedContext MM_gSchedContext;
extern IrqMgr MM_gIrqMgr;

// Thread priority/ID constants matching MM's z64thread.h
#define MM_Z_PRIORITY_AUDIOMGR 11
#define MM_Z_THREAD_ID_AUDIOMGR 10

// Track whether MM has been initialized
static bool sMmInitialized = false;

int MM_Game_Init(int argc, char** argv) {
    fprintf(stderr, "[MM] MM_Game_Init called (single-exe mode, argc=%d)\n", argc);
    fflush(stderr);

    // Load MM's resource archive into the existing Ship::Context.
    // OoT already created the context with oot.o2r/soh.o2r; we add mm.o2r
    // so that ResourceMgr_ListFiles("audio/sequences*", ...) finds MM resources.
    auto context = Ship::Context::GetInstance();
    if (context) {
        auto archiveMgr = context->GetResourceManager()->GetArchiveManager();

        // Locate mm.o2r relative to the executable
        std::string mmArchivePath = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", "2ship2harkinian");
        if (std::filesystem::exists(mmArchivePath)) {
            fprintf(stderr, "[MM] Loading MM archive: %s\n", mmArchivePath.c_str());
            fflush(stderr);
            archiveMgr->AddArchive(mmArchivePath);
            fprintf(stderr, "[MM] MM archive loaded successfully\n");
            fflush(stderr);
        } else {
            fprintf(stderr, "[MM] WARNING: mm.o2r not found at: %s\n", mmArchivePath.c_str());
            fflush(stderr);
        }
    } else {
        fprintf(stderr, "[MM] ERROR: No Ship::Context available\n");
        fflush(stderr);
        return -1;
    }

    // Allocate MM heaps (includes gAudioHeap)
    fprintf(stderr, "[MM] Allocating MM heaps...\n");
    fflush(stderr);
    MM_Heaps_Alloc();
    fprintf(stderr, "[MM] MM heaps allocated\n");
    fflush(stderr);

    // Initialize MM audio subsystem.
    // MM_AudioMgr_Init calls MM_Audio_Init() -> MM_AudioLoad_Init() which:
    //   - Zeroes gAudioCtx
    //   - Calls AudioThread_InitMesgQueues() (sets threadCmdProcQueueP)
    //   - Initializes audio heap pools
    //   - Loads audio tables from mm.o2r via ResourceMgr_ListFiles
    // Then calls MM_Audio_InitSound() -> AudioSfx_Init() which uses the queues.
    fprintf(stderr, "[MM] Initializing MM audio...\n");
    fflush(stderr);
    MM_AudioMgr_Init(&sAudioMgr, NULL, MM_Z_PRIORITY_AUDIOMGR, MM_Z_THREAD_ID_AUDIOMGR,
                      &MM_gSchedContext, &MM_gIrqMgr);
    fprintf(stderr, "[MM] MM audio initialized\n");
    fflush(stderr);

    sMmInitialized = true;
    fprintf(stderr, "[MM] MM_Game_Init complete\n");
    fflush(stderr);
    return 0;
}

void MM_Game_Run(void) {
    printf("[MM STUB] Game_Run called\n");
    printf("[MM STUB] Game would run here. Press Ctrl+C to exit.\n");
}

void MM_Game_Shutdown(void) {
    if (sMmInitialized) {
        fprintf(stderr, "[MM] Shutting down MM...\n");
        MM_Heaps_Free();
        sMmInitialized = false;
    }
}

const char* MM_Game_GetName(void) {
    return "Majora's Mask";
}

const char* MM_Game_GetId(void) {
    return "mm";
}

} // extern "C"
