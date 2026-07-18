/**
 * ROM-free locks for MM's cross-game resume contracts (CTest label "redship",
 * rows mm-resume-arena / mm-startup-restore in src/common/test_runner.cpp).
 *
 * Background (2026-07-18, int-gameplay-roundtrip soak on the operator's
 * workstation): every MM re-entry cold-starts the gamestate chain — the
 * switch path retires the live Play gamestate WITHOUT running the frame
 * loop's destroy/free epilogue (see MM_Graph_ResetRunFrameContext,
 * games/mm/src/code/graph.c), and Play's GameState_Realloc(&state, 0) had
 * taken the entire largest free block of the system arena. Two contracts
 * keep the cold boot correct, and each gets a lock here:
 *
 * 1. mm-resume-arena: MM_ResumeColdBootPrep (GameExports_SingleExe.cpp,
 *    called from MM_Game_Resume) must make an exhausted MM system arena
 *    allocatable again. Without it, the second entry's first gamestate
 *    malloc returned NULL and graph.c memset(NULL, ...) died as an opaque
 *    WRITE AV near 0 (observed: "WRITE at 0x20" — NULL plus the vectorized
 *    memset's first-store offset).
 *
 * 2. mm-startup-restore: MM_Play_ConsumeStartupEntrance (z_play.c) must
 *    re-apply the frozen MM save at startup-entrance consumption. The boot
 *    chain wipes gSaveContext AFTER MM_Game_Resume's restore
 *    (Setup_InitImpl -> MM_SaveContext_Init memset; TitleSetup rewrites it
 *    as a new file), so Play_Init-time restore is what carries MM
 *    continuity across cycle-2+ round trips.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdio>
#include <cstring>

extern "C" {
// GameExports_SingleExe.cpp — the resume-path cold-boot re-arm under test.
void MM_ResumeColdBootPrep(void);
// games/mm/src/code/z_play.c — the startup-entrance consumption under test.
void MM_Play_ConsumeStartupEntrance(void);
// games/mm/src/buffers/heaps.c — backing storage for the system arena.
void MM_Heaps_Alloc(void);
// games/mm/src/boot/O2/system_malloc.c
void* MM_SystemArena_Malloc(size_t size);
// src/common/context.cpp + entrance.cpp — C surface, same externs z_play uses.
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance, const void* saveContext, size_t size);
void Combo_ClearFrozenState(const char* gameId);
void Combo_SetStartupEntrance(uint16_t entrance);
bool Combo_HasStartupEntrance(void);
void Combo_ClearStartupEntrance(void);
}

extern "C" u8* MM_gSystemHeap;

namespace {

#define RESUME_ASSERT(cond, msg)                                       \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                  \
        }                                                              \
    } while (0)

} // namespace

/**
 * mm-resume-arena: after exhausting the system arena (the state a retired MM
 * session leaves behind), MM_ResumeColdBootPrep must restore allocatability
 * for both allocations the cold boot chain needs — a gamestate instance and
 * the 0x100000 per-gamestate game arena.
 */
extern "C" int MM_ResumeArena_RunHeadless(void) {
    printf("[TEST] mm-resume-arena: resume re-arms an exhausted MM system arena\n");

    if (MM_gSystemHeap == NULL) {
        MM_Heaps_Alloc();
    }
    RESUME_ASSERT(MM_gSystemHeap != NULL, "MM_Heaps_Alloc did not provide a system heap");

    // Initial arm (what MM_Game_Init does via MM_SystemHeap_Init + Regs_Init).
    MM_ResumeColdBootPrep();
    RESUME_ASSERT(MM_SystemArena_Malloc(0x1C0) != NULL, "fresh arena refused a small alloc");

    // Exhaust the arena the way a leaked session does: swallow every free
    // block. Geometric size descent keeps both the block count and the
    // free-list walks small.
    int blocks = 0;
    for (size_t size = 0x1000000; size >= 16; size /= 2) {
        while (MM_SystemArena_Malloc(size) != NULL) {
            blocks++;
            RESUME_ASSERT(blocks < 4096, "exhaustion loop runaway — arena math broken");
        }
    }
    RESUME_ASSERT(MM_SystemArena_Malloc(0x1C0) == NULL, "arena not actually exhausted");
    printf("[TEST] mm-resume-arena: arena exhausted after %d blocks; re-arming\n", blocks);

    // The contract under test: a resume must hand the cold boot chain a
    // usable arena again.
    MM_ResumeColdBootPrep();

    void* gameStateAlloc = MM_SystemArena_Malloc(0x1C0);
    RESUME_ASSERT(gameStateAlloc != NULL, "re-armed arena refused a gamestate-sized alloc");
    void* gameArenaAlloc = MM_SystemArena_Malloc(0x100000);
    RESUME_ASSERT(gameArenaAlloc != NULL, "re-armed arena refused the 0x100000 game-arena alloc");

    // Leave a clean arena (with gRegEditor re-created) for later tests.
    MM_ResumeColdBootPrep();

    printf("[TEST] PASS: mm-resume-arena — exhausted arena allocatable again after resume prep\n");
    return 0;
}

/**
 * mm-startup-restore: MM_Play_ConsumeStartupEntrance must (a) restore the
 * frozen save over a boot-chain wipe, (b) spawn at the startup entrance with
 * cutscene/game-mode state reset, (c) clear the startup slot, and (d) leave
 * a wiped save alone when there is no frozen state (first entry).
 */
extern "C" int MM_StartupRestore_RunHeadless(void) {
    printf("[TEST] mm-startup-restore: startup consumption restores the frozen save post-wipe\n");

    const uint16_t kArrival = 0xD800; // ENTRANCE(SOUTH_CLOCK_TOWN, 0), the OoT->MM arrival
    const uint8_t kPattern = 0x5A;

    // Arrange: a distinctive live save, frozen as the switch path would.
    memset(&gSaveContext, kPattern, sizeof(gSaveContext));
    gSaveContext.save.day = 3;
    gSaveContext.save.time = 0x4321;
    ((uint8_t*)&gSaveContext)[sizeof(gSaveContext) - 1] = 0x77;
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));

    // The boot chain's wipe (Setup_InitImpl -> MM_SaveContext_Init).
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    // The switch path's pending startup entrance (wildcard tag — visible to
    // MM, same visibility rule the game-scoped setter grants).
    Combo_SetStartupEntrance(kArrival);

    // Act: the consumption point in MM_Play_Init.
    MM_Play_ConsumeStartupEntrance();

    // Assert: frozen save is back...
    RESUME_ASSERT(gSaveContext.save.day == 3, "frozen save.day not restored after wipe");
    RESUME_ASSERT(gSaveContext.save.time == 0x4321, "frozen save.time not restored after wipe");
    RESUME_ASSERT(((uint8_t*)&gSaveContext)[sizeof(gSaveContext) - 1] == 0x77,
                  "frozen save tail byte not restored after wipe");
    // ...the arrival spawn is armed with plain-gameplay cutscene state...
    RESUME_ASSERT(gSaveContext.save.entrance == kArrival, "startup entrance not applied to save.entrance");
    RESUME_ASSERT(gSaveContext.save.cutsceneIndex == 0, "cutsceneIndex not reset for arrival spawn");
    RESUME_ASSERT(gSaveContext.gameMode == GAMEMODE_NORMAL, "gameMode not reset for arrival spawn");
    // ...and the slot is consumed.
    RESUME_ASSERT(!Combo_HasStartupEntrance(), "startup entrance not cleared after consumption");

    // First-entry behavior: no frozen state -> the wiped save must stay
    // wiped (bootstrap fills it elsewhere), only the entrance is applied.
    Combo_ClearFrozenState("mm");
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();
    RESUME_ASSERT(gSaveContext.save.entrance == kArrival, "first-entry startup entrance not applied");
    RESUME_ASSERT(gSaveContext.save.day == 0, "first-entry consumption must not invent save state");
    RESUME_ASSERT(!Combo_HasStartupEntrance(), "first-entry startup entrance not cleared");

    // Leave clean global state for later tests.
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    printf("[TEST] PASS: mm-startup-restore — restore-then-spawn contract holds\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
