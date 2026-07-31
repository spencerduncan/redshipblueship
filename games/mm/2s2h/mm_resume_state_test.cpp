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
// games/mm/src/audio/code_8019AF00.c — the derived sound-mode global the
// arrival must re-apply from the restored save (#483). Not declared in any
// header; Audio_SetFileSelectSettings (declared in sequence.h, reached through
// global.h) is its only writer.
extern s8 sSoundMode;
// games/mm/src/audio/sequence.c — write cursor into MM_sAudioSeqCmds (declared
// in sequence.h). Nothing drains the queue in a headless run, so the delta
// across a call is exactly what that call handed the audio thread. Needed
// because sSoundMode alone cannot see the second half of the #483 write:
// Audio_SetFileSelectSettings' default branch leaves sSoundMode untouched but
// still queues SEQCMD_SET_SOUND_MODE with an unassigned soundMode.
extern u8 sSeqCmdWritePos;
}

extern "C" u8* MM_gSystemHeap;

namespace {

#define RESUME_ASSERT(cond, msg)                                          \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

/**
 * How many seq commands were queued since write cursor `from`.
 *
 * This is the half of the sound-mode write sSoundMode cannot report.
 * Audio_SetFileSelectSettings has no `default` case that assigns soundMode, so
 * an out-of-range audioSetting leaves sSoundMode standing — indistinguishable
 * from not calling it at all — while still queueing a command whose payload the
 * audio thread uses to index sSoundModeList[5]. A DEPTH check is what catches
 * that, and it is deliberately payload-blind: the queued word is
 * `... | (soundMode)` on an `s8`, so an unassigned NEGATIVE soundMode
 * sign-extends over the opcode nibble and an opcode-matched probe would miss
 * the very command it exists to catch.
 */
u8 SeqCmdsQueuedSince(u8 from) {
    return (u8)(sSeqCmdWritePos - from);
}

/**
 * The SoundMode payload of the last SEQCMD_SET_SOUND_MODE queued since write
 * cursor `from`, or -1 if none was queued. Sound only for in-range values (see
 * SeqCmdsQueuedSince) — used to check WHICH mode a good call asked for.
 */
int LastQueuedSoundModeSince(u8 from) {
    // Same op/sub-op mask-and-value shape AudioSeq_IsSeqCmdNotQueued uses:
    // op in bits 31..28, sub-op in bits 11..8, payload in bits 7..0.
    const u32 kMask = 0xF0000F00;
    const u32 kSetSoundMode = ((u32)SEQCMD_OP_GLOBAL_CMD << 28) | ((u32)SEQCMD_SUB_OP_GLOBAL_SET_SOUND_MODE << 8);
    int found = -1;

    for (u8 i = from; i != sSeqCmdWritePos; i++) {
        u32 cmd = MM_sAudioSeqCmds[i];

        if ((cmd & kMask) == kSetSoundMode) {
            found = (int)(cmd & 0xFF);
        }
    }
    return found;
}

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
 * cutscene/game-mode state reset, (c) clear the startup slot, (d) leave
 * a wiped save alone when there is no frozen state (first entry), and (e)
 * re-apply the sound mode DERIVED from the restored save's
 * options.audioSetting, which lives outside gSaveContext and so cannot ride
 * the restore memcpy (#483, audit #482 row M5).
 */
extern "C" int MM_StartupRestore_RunHeadless(void) {
    printf("[TEST] mm-startup-restore: startup consumption restores the frozen save post-wipe\n");

    const uint16_t kArrival = 0xD800; // ENTRANCE(SOUTH_CLOCK_TOWN, 0), the OoT->MM arrival
    const uint8_t kPattern = 0x5A;

    // Arrange: a distinctive live save, frozen as the switch path would.
    memset(&gSaveContext, kPattern, sizeof(gSaveContext));
    gSaveContext.save.day = 3;
    gSaveContext.save.time = 0x4321;
    // A NON-DEFAULT sound preference in the frozen save. The 0x5A fill would
    // otherwise leave audioSetting out of range, which is a separate case
    // (locked at the end of this function).
    gSaveContext.options.audioSetting = SAVE_AUDIO_HEADSET;
    ((uint8_t*)&gSaveContext)[sizeof(gSaveContext) - 1] = 0x77;
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));

    // The boot chain's wipe (Setup_InitImpl -> MM_SaveContext_Init).
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    // ...and the boot chain's derived sound mode, taken from the VANILLA
    // BOOTSTRAP options the wipe just left behind (ConsoleLogo_Destroy ->
    // MM_Sram_InitSram -> Audio_SetFileSelectSettings, z_sram_NES.c). This
    // runs BEFORE the restore below — which is the whole bug.
    Audio_SetFileSelectSettings(gSaveContext.options.audioSetting);
    RESUME_ASSERT(sSoundMode == SOUNDMODE_STEREO, "bootstrap-derived sound mode not the STEREO default");

    // The switch path's pending startup entrance (wildcard tag — visible to
    // MM, same visibility rule the game-scoped setter grants).
    Combo_SetStartupEntrance(kArrival);

    // Act: the consumption point in MM_Play_Init.
    const u8 seqCmdPosBeforeArrival = sSeqCmdWritePos;
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
    // ...live gameplay state carried by the frozen blob is neutralized (#373).
    // The 0x5A fill above put every timerStates[] byte at a non-OFF value (the
    // regression: a minigame timer frozen mid-count resumes in the arrival
    // scene where its actor does not exist). The OoT consumption twin cleared
    // these; MM's had drifted and did not.
    for (int i = 0; i < TIMER_ID_MAX; i++) {
        RESUME_ASSERT(gSaveContext.timerStates[i] == TIMER_STATE_OFF,
                      "frozen live timer not neutralized on MM arrival (#373)");
    }
    RESUME_ASSERT(gSaveContext.magicState == MAGIC_STATE_IDLE, "frozen magicState not reset on MM arrival (#373)");
    RESUME_ASSERT(gSaveContext.forcedSeqId == NA_BGM_GENERAL_SFX, "frozen forcedSeqId not reset on MM arrival (#373)");
    RESUME_ASSERT(gSaveContext.powderKegTimer == 0, "frozen powder-keg timer not cleared on MM arrival (#373)");
    // ...the sound mode DERIVED from the restored options is live (#483). This
    // is the assertion the fix exists for: options.audioSetting rides the
    // restore memcpy, but sSoundMode and the audio thread's copy do not, and
    // nothing downstream of an arrival re-derives them —
    // Audio_SetFileSelectSettings is sSoundMode's only writer and its other
    // call sites are file-select-only. Without the re-apply in
    // MM_Play_ConsumeStartupEntrance this still reads SOUNDMODE_STEREO, the
    // bootstrap's value, for the rest of the MM session.
    RESUME_ASSERT(gSaveContext.options.audioSetting == SAVE_AUDIO_HEADSET,
                  "frozen options.audioSetting not restored after wipe");
    RESUME_ASSERT(sSoundMode == SOUNDMODE_HEADSET, "restored options.audioSetting not re-applied to sSoundMode (#483)");
    // ...and the audio thread was told, not just the CPU-side global: sSoundMode
    // gates SFX panning/distance on the game thread, the queued command is what
    // reaches AUDIOCMD_GLOBAL_SET_SOUND_MODE. Both halves or the mode is only
    // half live.
    //
    // Exactly one command, not "at least one": that is what licenses the
    // out-of-range leg below to assert a queue depth of ZERO. If the consume
    // ever legitimately queues other audio work, this assertion fails loudly
    // here instead of quietly making that leg vacuous.
    RESUME_ASSERT(SeqCmdsQueuedSince(seqCmdPosBeforeArrival) == 1,
                  "arrival queued something other than exactly one seq command (#483)");
    RESUME_ASSERT(LastQueuedSoundModeSince(seqCmdPosBeforeArrival) == SOUNDMODE_HEADSET,
                  "arrival queued no SET_SOUND_MODE for the restored audioSetting (#483)");
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
    // Cross-game arrivals are plain spawns: the Clock Town first-visit intro
    // layer must be pre-suppressed on every consumed startup entrance. The
    // SCT intro is ACTOR-triggered (ObjTokeiTobira on WEEKEVENTREG_59_04,
    // Elf_Msg6 Tatl interrupt on WEEKEVENTREG_31_04) — cutsceneIndex=0 alone
    // does not stop it.
    RESUME_ASSERT(CHECK_WEEKEVENTREG(WEEKEVENTREG_59_04),
                  "SCT tower-exit intro flag (WEEKEVENTREG_59_04) not pre-set on arrival");
    RESUME_ASSERT(CHECK_WEEKEVENTREG(WEEKEVENTREG_31_04),
                  "Tatl interrupt flag (WEEKEVENTREG_31_04) not pre-set on arrival");
    // First entry re-derives from the bootstrap options, i.e. the same value
    // the boot chain already applied — the re-apply is a no-op, never an
    // invention. (Non-vacuous: the previous leg left sSoundMode at
    // SOUNDMODE_HEADSET.)
    RESUME_ASSERT(sSoundMode == SOUNDMODE_STEREO, "first-entry consumption did not track the bootstrap sound mode");

    // Out-of-range guard: a restored blob — unlike the freshly-initialised
    // options block MM_Sram_InitSram sees — can carry any byte, and
    // Audio_SetFileSelectSettings has no case for one: its default branch
    // leaves sSoundMode alone but still runs SEQCMD_SET_SOUND_MODE(soundMode)
    // with soundMode never assigned, and the audio thread indexes
    // sSoundModeList[5] (sequence.c) with that payload. So the arrival must
    // leave the boot chain's mode standing and queue NOTHING.
    //
    // The queue-DEPTH observation is what makes this leg falsifiable: sSoundMode
    // alone cannot fail it, because the default branch does not write
    // sSoundMode either way. Depth rather than opcode-match — an unassigned
    // negative soundMode sign-extends over the opcode nibble (SeqCmdsQueuedSince).
    memset(&gSaveContext, kPattern, sizeof(gSaveContext)); // audioSetting = 0x5A, no valid case
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.options.audioSetting = SAVE_AUDIO_MONO;
    Audio_SetFileSelectSettings(gSaveContext.options.audioSetting);
    RESUME_ASSERT(sSoundMode == SOUNDMODE_MONO, "bootstrap sound mode not armed for the out-of-range leg");
    Combo_SetStartupEntrance(kArrival);
    const u8 seqCmdPosBeforeCorrupt = sSeqCmdWritePos;
    MM_Play_ConsumeStartupEntrance();
    RESUME_ASSERT(gSaveContext.options.audioSetting == kPattern,
                  "out-of-range leg did not actually restore an out-of-range audioSetting");
    RESUME_ASSERT(sSoundMode == SOUNDMODE_MONO, "out-of-range restored audioSetting was applied anyway (#483)");
    RESUME_ASSERT(SeqCmdsQueuedSince(seqCmdPosBeforeCorrupt) == 0,
                  "out-of-range restored audioSetting still queued a seq command (#483)");

    // Leave clean global state for later tests.
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    Audio_SetFileSelectSettings(SAVE_AUDIO_STEREO);

    printf("[TEST] PASS: mm-startup-restore — restore-then-spawn contract holds\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
