/**
 * Game Export Interface for MM (2Ship2Harkinian)
 *
 * This file implements the standard game interface that allows the combo
 * launcher to load and run MM as a shared library.
 */

#include "combo/GameExports.h"
#include "combo/ComboContextBridge.h"
#include "combo/CrossGameEntrance.h"
#include "combo/FrozenState.h"
#include "BenPort.h"
#include <libultraship/bridge/crashhandlerbridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/window/Window.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h>
#include <cstring>
#include <filesystem>

// Resource Types/Factories for MM
#include <ship/resource/type/Blob.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Matrix.h>
#include <fast/resource/type/Texture.h>
#include <fast/resource/type/Vertex.h>
#include "2s2h/resource/type/2shResourceType.h"
#include "2s2h/resource/type/Animation.h"
#include "2s2h/resource/type/Array.h"
#include "2s2h/resource/type/AudioSample.h"
#include "2s2h/resource/type/AudioSequence.h"
#include "2s2h/resource/type/AudioSoundFont.h"
#include "2s2h/resource/type/CollisionHeader.h"
#include "2s2h/resource/type/Cutscene.h"
#include "2s2h/resource/type/Path.h"
#include "2s2h/resource/type/PlayerAnimation.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/Skeleton.h"
#include "2s2h/resource/type/SkeletonLimb.h"
#include <ship/resource/factory/BlobFactory.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include "2s2h/resource/importer/AnimationFactory.h"
#include "2s2h/resource/importer/ArrayFactory.h"
#include "2s2h/resource/importer/AudioSampleFactory.h"
#include "2s2h/resource/importer/AudioSequenceFactory.h"
#include "2s2h/resource/importer/AudioSoundFontFactory.h"
#include "2s2h/resource/importer/CollisionHeaderFactory.h"
#include "2s2h/resource/importer/CutsceneFactory.h"
#include "2s2h/resource/importer/PathFactory.h"
#include "2s2h/resource/importer/PlayerAnimationFactory.h"
#include "2s2h/resource/importer/SceneFactory.h"
#include "2s2h/resource/importer/SkeletonFactory.h"
#include "2s2h/resource/importer/SkeletonLimbFactory.h"
#include "2s2h/resource/importer/TextMMFactory.h"
#include "2s2h/resource/importer/BackgroundFactory.h"
#include "2s2h/resource/importer/TextureAnimationFactory.h"
#include "2s2h/resource/importer/KeyFrameFactory.h"

// From main.c - need these includes for initialization
extern "C" {
#include "audiomgr.h"
#include "fault.h"
#include "idle.h"
#include "irqmgr.h"
#include "padmgr.h"
#include "scheduler.h"
#include "stack.h"
#include "system_heap.h"
#include "z64thread.h"
#include "global.h"
#include "z64save.h"
}

// External declarations from main.c
extern "C" {
    void InitOTR(void);
    void DeinitOTR(void);
    void MM_Heaps_Alloc(void);
    void MM_Heaps_Free(void);
    void CrashHandler_PrintExt(char* buffer, size_t* pos);
    void MM_Graph_ThreadEntry(void* arg);

    // Additional init functions from main.c
    void Nmi_Init(void);
    void MM_Fault_Init(void);
    void Check_RegionIsSupported(void);
    void Check_ExpansionPak(void);
    void Regs_Init(void);

    // Globals from main.c that need to be referenced
    extern s32 MM_gScreenWidth;
    extern s32 MM_gScreenHeight;
    extern uintptr_t MM_gSystemHeap;
    extern OSMesgQueue sSerialEventQueue;
    extern OSMesg sSerialMsgBuf[1];
    extern OSMesgQueue sIrqMgrMsgQueue;
    extern OSMesg sIrqMgrMsgBuf[60];
    extern SchedContext MM_gSchedContext;
    extern AudioMgr sAudioMgr;
    extern PadMgr MM_gPadMgr;
    extern IrqMgr MM_gIrqMgr;

    // Save context for state preservation
    extern SaveContext gSaveContext;
}

// Game state for pause/resume
static bool sGamePaused = false;
static int sArgc = 0;
static char** sArgv = nullptr;

/**
 * Register MM-specific resource factories into the existing Ship::Context.
 * Used in single-exe mode where OoT already created the context.
 * This registers all MM resource types so mm.o2r resources can be deserialized.
 */
static void MM_InitResourceFactories() {
    auto loader = Ship::Context::GetInstance()->GetResourceManager()->GetResourceLoader();

    // Shared resource types (Texture, Vertex, DisplayList, Matrix, Blob)
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLVertexV0>(), RESOURCE_FORMAT_XML, "Vertex",
                                    static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLDisplayListV0>(), RESOURCE_FORMAT_XML,
                                    "DisplayList", static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Ship::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Ship::ResourceType::Blob), 0);

    // MM-specific resource types
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryArrayV0>(), RESOURCE_FORMAT_BINARY,
                                    "Array", static_cast<uint32_t>(SOH::ResourceType::SOH_Array), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAnimationV0>(), RESOURCE_FORMAT_BINARY,
                                    "Animation", static_cast<uint32_t>(SOH::ResourceType::SOH_Animation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPlayerAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "PlayerAnimation",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Room", static_cast<uint32_t>(SOH::ResourceType::SOH_Room), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCollisionHeaderV0>(),
                                    RESOURCE_FORMAT_BINARY, "CollisionHeader",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
                                    RESOURCE_FORMAT_BINARY, "SkeletonLimb",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPathMMV0>(), RESOURCE_FORMAT_BINARY,
                                    "Path", static_cast<uint32_t>(SOH::ResourceType::SOH_Path), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCutsceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Cutscene", static_cast<uint32_t>(SOH::ResourceType::SOH_Cutscene), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextMMV0>(), RESOURCE_FORMAT_BINARY,
                                    "TextMM", static_cast<uint32_t>(SOH::ResourceType::TSH_TextMM), 0);

    // Audio resources
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSampleV2>(), RESOURCE_FORMAT_BINARY,
                                    "AudioSample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSampleV0>(), RESOURCE_FORMAT_XML,
                                    "Sample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSoundFontV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSoundFont",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSoundFontV0>(), RESOURCE_FORMAT_XML,
                                    "SoundFont", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSequenceV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSequence",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSequenceV0>(), RESOURCE_FORMAT_XML,
                                    "Sequence", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 0);

    // Background, TextureAnimation, KeyFrame
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryBackgroundV0>(), RESOURCE_FORMAT_BINARY,
                                    "Background", static_cast<uint32_t>(SOH::ResourceType::SOH_Background), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextureAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "TextureAnimation",
                                    static_cast<uint32_t>(SOH::ResourceType::TSH_TexAnim), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryKeyFrameAnim>(), RESOURCE_FORMAT_BINARY,
                                    "KeyFrameAnim", static_cast<uint32_t>(SOH::ResourceType::TSH_CKeyFrameAnim), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryKeyFrameSkel>(), RESOURCE_FORMAT_BINARY,
                                    "KeyFrameSkel", static_cast<uint32_t>(SOH::ResourceType::TSH_CKeyFrameSkel), 0);

    fprintf(stderr, "[MM] Resource factories registered into existing context\n");
    fflush(stderr);
}

GAME_EXPORT int Game_Init(int argc, char** argv) {
    fprintf(stderr, "[MM INIT DEBUG] Game_Init called, argc=%d\n", argc);
    fflush(stderr);

    // Store args for potential restart
    sArgc = argc;
    sArgv = argv;

    // In single-exe mode, OoT already created the Ship::Context. We only need
    // to register MM's resource factories and load mm.o2r so MM resources can
    // be deserialized. If no context exists yet (standalone MM), do full init.
    if (Ship::Context::GetInstance() != nullptr) {
        fprintf(stderr, "[MM INIT DEBUG] Context already exists (single-exe mode), registering MM resource factories\n");
        fflush(stderr);

        // Register MM resource factories into the existing context
        MM_InitResourceFactories();

        // Load MM archive into the existing ArchiveManager
        std::string mmArchivePath = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", "2ship2harkinian");
        if (!mmArchivePath.empty() && std::filesystem::exists(mmArchivePath)) {
            auto archiveMgr = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
            archiveMgr->AddArchive(mmArchivePath);
            fprintf(stderr, "[MM INIT DEBUG] mm.o2r archive added from: %s\n", mmArchivePath.c_str());
        } else {
            fprintf(stderr, "[MM INIT DEBUG] WARNING: mm.o2r not found!\n");
        }
        fflush(stderr);
    } else {
        // Standalone MM mode - do full initialization
        fprintf(stderr, "[MM INIT DEBUG] About to call InitOTR()...\n");
        fflush(stderr);
        InitOTR();
        fprintf(stderr, "[MM INIT DEBUG] InitOTR() complete\n");
        fflush(stderr);
    }

    fprintf(stderr, "[MM INIT DEBUG] Registering crash handler...\n");
    fflush(stderr);
    CrashHandlerRegisterCallback(CrashHandler_PrintExt);

    fprintf(stderr, "[MM INIT DEBUG] Allocating heaps...\n");
    fflush(stderr);
    MM_Heaps_Alloc();
    fprintf(stderr, "[MM INIT DEBUG] Heaps allocated\n");
    fflush(stderr);

    // Additional initialization from main.c that happens before MM_Graph_ThreadEntry
    MM_gScreenWidth = SCREEN_WIDTH;
    MM_gScreenHeight = SCREEN_HEIGHT;

    fprintf(stderr, "[MM INIT DEBUG] Calling Nmi_Init()...\n");
    fflush(stderr);
    Nmi_Init();

    fprintf(stderr, "[MM INIT DEBUG] Calling MM_Fault_Init()...\n");
    fflush(stderr);
    MM_Fault_Init();

    fprintf(stderr, "[MM INIT DEBUG] Calling Check_RegionIsSupported()...\n");
    fflush(stderr);
    Check_RegionIsSupported();

    fprintf(stderr, "[MM INIT DEBUG] Calling Check_ExpansionPak()...\n");
    fflush(stderr);
    Check_ExpansionPak();

    fprintf(stderr, "[MM INIT DEBUG] Calling MM_SystemHeap_Init()...\n");
    fflush(stderr);
    MM_SystemHeap_Init((void*)MM_gSystemHeap, SYSTEM_HEAP_SIZE);

    fprintf(stderr, "[MM INIT DEBUG] Calling Regs_Init()...\n");
    fflush(stderr);
    Regs_Init();

    // Set up message queues
    fprintf(stderr, "[MM INIT DEBUG] Setting up message queues...\n");
    fflush(stderr);
    osCreateMesgQueue(&sSerialEventQueue, sSerialMsgBuf, ARRAY_COUNT(sSerialMsgBuf));
    osSetEventMesg(OS_EVENT_SI, &sSerialEventQueue, OS_MESG_PTR(NULL));
    osCreateMesgQueue(&sIrqMgrMsgQueue, sIrqMgrMsgBuf, ARRAY_COUNT(sIrqMgrMsgBuf));

    // Initialize PadMgr and AudioMgr - these are required for MM_Graph_ThreadEntry
    // Note: Stack addresses are handled differently in shared library context
    fprintf(stderr, "[MM INIT DEBUG] Calling MM_PadMgr_Init()...\n");
    fflush(stderr);
    MM_PadMgr_Init(&sSerialEventQueue, &MM_gIrqMgr, Z_THREAD_ID_PADMGR, Z_PRIORITY_PADMGR, NULL);

    fprintf(stderr, "[MM INIT DEBUG] Calling MM_AudioMgr_Init()...\n");
    fflush(stderr);
    MM_AudioMgr_Init(&sAudioMgr, NULL, Z_PRIORITY_AUDIOMGR, Z_THREAD_ID_AUDIOMGR, &MM_gSchedContext, &MM_gIrqMgr);

    fprintf(stderr, "[MM INIT DEBUG] Game_Init complete, returning 0\n");
    fflush(stderr);
    return 0;
}

GAME_EXPORT void Game_Run(void) {
    // Run the main game loop
    MM_Graph_ThreadEntry(nullptr);
}

GAME_EXPORT void Game_Shutdown(void) {
    DeinitOTR();
    MM_Heaps_Free();
}

GAME_EXPORT void Game_Pause(void) {
    // TODO: Implement pause logic
    // For now, this is a placeholder. Full implementation requires
    // modifying the game loop to check sGamePaused flag.
    sGamePaused = true;
}

GAME_EXPORT void Game_Resume(void) {
    sGamePaused = false;
}

GAME_EXPORT void* Game_SaveState(size_t* outSize) {
    // Freeze state to the combo layer's FrozenStateManager
    // The returnEntrance will be set separately by the entrance hook
    Combo_FreezeState("mm", 0, &gSaveContext, sizeof(SaveContext));

    // The FrozenStateManager owns the buffer, so we don't return one
    // Callers should use Combo_GetMMSaveContext() for read access
    *outSize = sizeof(SaveContext);
    return nullptr;
}

GAME_EXPORT int Game_LoadState(void* data, size_t size) {
    // If data is provided, use it directly (external state)
    if (data != nullptr && size == sizeof(SaveContext)) {
        memcpy(&gSaveContext, data, sizeof(SaveContext));
        return 0;
    }

    // Otherwise, try to restore from FrozenStateManager
    if (Combo_RestoreState("mm", &gSaveContext, sizeof(SaveContext))) {
        return 0;
    }

    return -1;  // No state to restore
}

GAME_EXPORT const char* Game_GetName(void) {
    return "Majora's Mask";
}

GAME_EXPORT const char* Game_GetId(void) {
    return "mm";
}

// ============================================================================
// Hot-swap support - F10 detection
// ============================================================================

// Track last F10 state to detect edge (press, not hold)
static bool sLastF10State = false;

/**
 * Check if F10 was pressed and request game switch if so.
 * Also checks for pending cross-game entrance switches.
 * Called from the game loop (MM_Graph_ThreadEntry) each frame.
 * Returns true if a switch was requested (game should exit its loop).
 */
extern "C" bool Combo_CheckHotSwap(void) {
    // Check for pending cross-game entrance switch first
    if (Combo_IsCrossGameSwitch()) {
        return true;
    }

    auto context = Ship::Context::GetInstance();
    if (!context) {
        return Combo_IsGameSwitchRequested();
    }

    auto window = context->GetWindow();
    if (!window) {
        return Combo_IsGameSwitchRequested();
    }

    int32_t scancode = window->GetLastScancode();
    bool f10Pressed = (scancode == Ship::LUS_KB_F10);

    // Detect rising edge (just pressed, not held)
    if (f10Pressed && !sLastF10State) {
        Combo_RequestGameSwitch();
    }
    sLastF10State = f10Pressed;

    return Combo_IsGameSwitchRequested();
}

// ============================================================================
// Cross-game entrance hook - called from MM's entrance handling
// ============================================================================

/**
 * Check if an entrance is a cross-game entrance and handle the switch.
 * Called from the entrance system when transitioning.
 *
 * @param entranceIndex The entrance being taken
 * @return The entrance to use (original if not cross-game)
 */
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex) {
    // Check if this entrance triggers a cross-game switch
    uint16_t result = Combo_CheckCrossGameEntrance("mm", entranceIndex);

    // If a cross-game switch was triggered, freeze our state
    if (Combo_IsCrossGameSwitch()) {
        // Get the return entrance from the pending switch
        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();

        // Freeze MM state with the return entrance
        Combo_FreezeState("mm", returnEntrance, &gSaveContext, sizeof(SaveContext));

        // Signal that we're ready to switch
        Combo_SignalReadyToSwitch();
    }

    return result;
}
