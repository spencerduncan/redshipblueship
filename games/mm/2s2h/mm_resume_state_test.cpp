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
 *
 * mm-startup-restore's poison byte for "was the frozen save restored"
 * used to sit at the very last byte of gSaveContext, which is the last byte
 * of shipSaveContext — a trailing member z64save.h documents as "values
 * added by 2S2H that aren't persisted to the save file". That member is
 * legitimately live-seeded on every restore: RegisterSavingEnhancements'
 * OnSaveLoad hook stamps shipSaveContext.lastTimeLog, the same way production
 * always has (z_sram_NES.c zeroes it in lockstep on every new-file/continue
 * path, and MM_Play_ConsumeStartupEntrance dispatches OnSaveLoad after every
 * restore, the #439 fix). #617 found the poison byte's placement wrong, not
 * the seeder — it moved to the last byte of `Save save` (still inside the
 * persisted, byte-exact-restorable region, but ahead of both the seeder and
 * every field this function's own arrival-spawn logic explicitly resets) —
 * plus an explicit freshness assertion on lastTimeLog. See the assertion site
 * below, same shape as the pre-existing sSoundMode carve-out a few lines down
 * from it.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdio>
#include <cstring>

#include <ship/Context.h>

#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
#include "context.h"        // gComboCtx, snapshotted around the real suspend
#include "game_lifecycle.h" // GameOps

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
// games/mm/2s2h/BenPort.cpp — the wall clock RegisterSavingEnhancements'
// OnSaveLoad hook stamps into shipSaveContext.lastTimeLog (#617). Used here to
// check the restored value is a fresh timestamp, not to seed it ourselves.
uint64_t GetUnixTimestamp(void);
// ovl_En_Test4/z_en_test4.c — the clock actor's file-static latch (#666).
s32 MM_EnTest4_IsLoadedLatchForTest(void);
void MM_EnTest4_SetLoadedLatchForTest(s32 isLoaded);
// The overlays whose Destroy was the only thing restoring a static (#666): seed
// (dirty != 0) or clear their statics, and read whether they are stale.
void MM_EnGrasshopper_SetDestroyStaticsForTest(s32 dirty);
s32 MM_EnGrasshopper_DestroyStaticsDirtyForTest(void);
void MM_EnHoll_SetDestroyStaticsForTest(s32 dirty);
s32 MM_EnHoll_DestroyStaticsDirtyForTest(void);
void MM_EnTanron5_SetDestroyStaticsForTest(s32 dirty);
s32 MM_EnTanron5_DestroyStaticsDirtyForTest(void);
void MM_EnViewer_SetDestroyStaticsForTest(s32 dirty);
s32 MM_EnViewer_DestroyStaticsDirtyForTest(void);
void MM_EnMushi2_SetDestroyStaticsForTest(s32 dirty);
s32 MM_EnMushi2_DestroyStaticsDirtyForTest(void);
void MM_EnInvadepoh_SetDestroyStaticsForTest(s32 dirty);
s32 MM_EnInvadepoh_DestroyStaticsDirtyForTest(void);
// GameExports_SingleExe.cpp: MM's registered lifecycle. The row drives its
// suspend, the call GameRunner makes on every departure from MM.
GameOps* MM_GetGameOps(void);
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
 * cutscene/game-mode state reset, (c) clear the startup slot, (d) on a first
 * entry (no frozen state) re-author the new-file clock over the title-demo
 * bootstrap the boot chain hands it, while leaving the rest of that save alone
 * (#639), and (e) re-apply the sound mode DERIVED from the restored save's
 * options.audioSetting, which lives outside gSaveContext and so cannot ride
 * the restore memcpy (#483, audit #482 row M5).
 */
extern "C" int MM_StartupRestore_RunHeadless(void) {
    printf("[TEST] mm-startup-restore: startup consumption restores the frozen save post-wipe\n");

    // The seeder under test (#617): MM_Play_ConsumeStartupEntrance ends in
    // GameInteractor_ExecuteOnSaveLoad, and in every real MM boot
    // RegisterSavingEnhancements' OnSaveLoad hook is already registered by
    // then (MM_Rando_Init always runs before Play). Bring that one registrar
    // up directly rather than the full MM_Rando_Init: MM_Rando_Init also arms
    // Rando::Init()'s OnSaveLoad handler, whose IS_RANDO-gated legs are
    // unrelated to this contract and untested against this function's
    // pattern-filled SaveContext. RegisterSavingEnhancements' OnSaveLoad hooks
    // unregister-before-register (COND_HOOK), so calling it again is harmless
    // if MMRegistrarCoverage already ran in this process.
    auto ctx = Ship::Context::GetInstance();
    RESUME_ASSERT(ctx != nullptr && ctx->GetConsoleVariables() != nullptr,
                  "Ship::Context/ConsoleVariables missing — run the shared bring-up first");
    RegisterSavingEnhancements();

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
    // Poison byte: the last byte of `Save save` (z64save.h), landing inside
    // shipSaveInfo -- 2S2H-added but PERSISTED (unlike shipSaveContext) and
    // untouched by both MM_Play_ConsumeStartupEntrance's explicit arrival-spawn
    // resets (all scattered through SaveContext's OTHER top-level members --
    // seqId, ambienceId, magicState, forcedSeqId, nextCutsceneIndex,
    // nextDayTime, timerStates[], powderKegTimer -- none of which live inside
    // `save`) and RegisterSavingEnhancements' live OnSaveLoad hook (which only
    // writes shipSaveContext and, since fileCreatedAt is non-zero here, does
    // not touch shipSaveInfo either). #617: the OLD poison offset,
    // sizeof(gSaveContext) - 1, is the last byte of shipSaveContext instead --
    // seeded, not restored verbatim, once that hook is live.
    ((uint8_t*)&gSaveContext)[sizeof(Save) - 1] = 0x77;
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
    const uint64_t timestampBeforeArrival = GetUnixTimestamp();
    MM_Play_ConsumeStartupEntrance();

    // Assert: frozen save is back...
    RESUME_ASSERT(gSaveContext.save.day == 3, "frozen save.day not restored after wipe");
    RESUME_ASSERT(gSaveContext.save.time == 0x4321, "frozen save.time not restored after wipe");
    RESUME_ASSERT(((uint8_t*)&gSaveContext)[sizeof(Save) - 1] == 0x77,
                  "frozen save tail byte (last byte of `save`, ahead of the arrival-spawn resets and the "
                  "2S2H-added, non-persisted shipSaveContext tail) not restored after wipe (#617)");
    // ...and shipSaveContext.lastTimeLog -- deliberately NOT covered by the
    // poison byte above, since RegisterSavingEnhancements' OnSaveLoad hook
    // genuinely re-seeds it on every restore, the same way sSoundMode below is
    // genuinely re-derived rather than restored verbatim -- is a FRESH stamp,
    // not the frozen/poisoned value riding the restore memcpy untouched. The
    // seeder must actually have run, not merely have been tolerated.
    // GetUnixTimestamp (BenPort.cpp) returns milliseconds since the Unix
    // epoch; 5000ms is generous slack for a synchronous in-process call.
    RESUME_ASSERT(gSaveContext.shipSaveContext.lastTimeLog != 0,
                  "shipSaveContext.lastTimeLog not seeded by the OnSaveLoad hook after restore (#617)");
    RESUME_ASSERT(gSaveContext.shipSaveContext.lastTimeLog >= timestampBeforeArrival &&
                      gSaveContext.shipSaveContext.lastTimeLog - timestampBeforeArrival < 5000,
                  "shipSaveContext.lastTimeLog not a fresh plausible timestamp after restore (#617)");
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

    // First-entry behavior: no frozen state -> the arrival spawns on the boot
    // chain's bootstrap save. Arrange THAT save exactly, not a zeroed one
    // (#639): a first MM entry reaches MM_Play_Init through
    // TitleSetup_SetupTitleScreen (ovl_opening/z_opening.c), which authors a
    // new file with MM_Sram_InitNewSave() and then lays the title-screen
    // attract demo's 08:00 / day 1 over its clock. This leg used to memset the
    // save to 0 before the act, so its "day == 0 afterwards" could never see
    // the demo clock riding through the consume -- which is exactly what
    // shipped (#636: South Clock Town at 08:00 on Day 1, no dawn telop).
    // TitleSetup_SetupTitleScreen itself is not callable here (it hops
    // gamestates and needs a gfx context); its two-line override is pinned
    // to its source instead.
    Combo_ClearFrozenState("mm");
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.save.time = CLOCK_TIME(8, 0); // z_opening.c TitleSetup_SetupTitleScreen:
    gSaveContext.save.day = 1;                 //   "save.time = CLOCK_TIME(8, 0); save.day = 1;"
    // Non-vacuity guard: the staged bootstrap must be the demo clock, and the
    // demo clock must differ from the new-file clock the assertions below
    // expect, or a consume that never touches the clock passes by coincidence.
    RESUME_ASSERT(gSaveContext.save.time == CLOCK_TIME(8, 0) && gSaveContext.save.day == 1,
                  "first-entry arrange did not stage the title-demo bootstrap clock");
    RESUME_ASSERT(gSaveContext.save.time != CLOCK_TIME(6, 0) - 1 && gSaveContext.save.day != 0 &&
                      gSaveContext.save.eventDayCount == 0,
                  "first-entry arrange cannot distinguish the demo clock from a new file's");
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();
    RESUME_ASSERT(gSaveContext.save.entrance == kArrival, "first-entry startup entrance not applied");
    // The clock is re-authored exactly as MM_Sram_InitNewSave authors it
    // (z_sram_NES.c: CLOCK_TIME(6, 0) - 1, day 0, eventDayCount 0), so
    // En_Test4 runs vanilla's "Dawn of the First Day" ceremony on the arrival
    // load (EnTest4_Init's day-0 branch -> DayTelop) instead of the player
    // landing at 08:00 on Day 1. skyboxTime is paired with save.time the way
    // MM_Play_Init's nextDayTime rewrite pairs them.
    RESUME_ASSERT(gSaveContext.save.time == CLOCK_TIME(6, 0) - 1,
                  "first-entry arrival kept the title-demo 08:00 instead of the new-file clock (#639)");
    RESUME_ASSERT(gSaveContext.save.day == 0,
                  "first-entry arrival kept the title-demo day 1 instead of the new-file day 0 (#639)");
    RESUME_ASSERT(gSaveContext.save.eventDayCount == 0,
                  "first-entry arrival eventDayCount not re-authored to 0 (#639)");
    RESUME_ASSERT(gSaveContext.skyboxTime == CLOCK_TIME(6, 0) - 1,
                  "first-entry arrival skyboxTime not paired with the re-authored save.time (#639)");
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

namespace {

int sProbeResetCalls = 0;
int sIdleProbeResetCalls = 0;
// Whether MM_gPlayState was already NULL when the probe's reset ran: the
// ordering half of the wiring lock (the retire must run after the graph, and
// with it the Play gamestate, is retired).
bool sProbeSawRetiredGraph = false;

void ProbeReset(void) {
    sProbeResetCalls++;
    sProbeSawRetiredGraph = (MM_gPlayState == NULL);
}

void IdleProbeReset(void) {
    sIdleProbeResetCalls++;
}

// The overlays whose Destroy (directly, or through a per-type Destroy helper)
// was the only thing restoring a file-scope static, each with the seed/read
// accessors its TU exports for this row.
struct DestroyMaintainedOverlay {
    s16 actorId;
    const char* name;
    void (*setDirty)(s32 dirty);
    s32 (*isDirty)(void);
};

const DestroyMaintainedOverlay kDestroyMaintained[] = {
    { ACTOR_EN_GRASSHOPPER, "En_Grasshopper sOccupiedIndices", MM_EnGrasshopper_SetDestroyStaticsForTest,
      MM_EnGrasshopper_DestroyStaticsDirtyForTest },
    { ACTOR_EN_HOLL, "En_Holl sInstancePlayingSound", MM_EnHoll_SetDestroyStaticsForTest,
      MM_EnHoll_DestroyStaticsDirtyForTest },
    { ACTOR_EN_TANRON5, "En_Tanron5 sFragmentAndItemDropCount", MM_EnTanron5_SetDestroyStaticsForTest,
      MM_EnTanron5_DestroyStaticsDirtyForTest },
    { ACTOR_EN_VIEWER, "En_Viewer D_8089F3E0", MM_EnViewer_SetDestroyStaticsForTest,
      MM_EnViewer_DestroyStaticsDirtyForTest },
    { ACTOR_EN_MUSHI2, "En_Mushi2 D_80A6B994", MM_EnMushi2_SetDestroyStaticsForTest,
      MM_EnMushi2_DestroyStaticsDirtyForTest },
    { ACTOR_EN_INVADEPOH, "En_Invadepoh sUfo/sNight3Cremia/sNight3Romani", MM_EnInvadepoh_SetDestroyStaticsForTest,
      MM_EnInvadepoh_DestroyStaticsDirtyForTest },
};
constexpr size_t kDestroyMaintainedCount = ARRAY_COUNT(kDestroyMaintained);

// Everything the row seeds, put back on EVERY exit path (AllTests runs every
// row in one process), including a failed assertion's early return.
struct AbandonedSessionRowGuard {
    s32 probeSlot = -1;
    s32 idleSlot = -1;
    bool seeded = false;
    Actor* fakeActor = nullptr;
    PlayState* playState = nullptr;
    s32 gameMode = 0;
    u32 resetTimer = 0;
    s32 audioInitialized = 0;
    const ComboContext* comboSnapshot = nullptr;

    ~AbandonedSessionRowGuard() {
        if (!seeded) {
            return;
        }
        gActorOverlayTable[probeSlot].profile = NULL;
        gActorOverlayTable[probeSlot].numLoaded = 0;
        gActorOverlayTable[idleSlot].profile = NULL;
        gActorOverlayTable[idleSlot].numLoaded = 0;
        gActorOverlayTable[ACTOR_EN_TEST4].numLoaded = 0;
        for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
            gActorOverlayTable[kDestroyMaintained[k].actorId].numLoaded = 0;
            kDestroyMaintained[k].setDirty(false);
        }
        MM_EnTest4_SetLoadedLatchForTest(false);
        ObjectExtension_Free(fakeActor);
        MM_gPlayState = playState;
        gSaveContext.gameMode = gameMode;
        gAudioCtx.resetTimer = resetTimer;
        gAudioCtxInitalized = audioInitialized;
        memcpy(&gComboCtx, comboSnapshot, sizeof(ComboContext));
    }
};

} // namespace

/**
 * mm-abandoned-session-statics (#666): a cross-game departure retires MM's
 * Play gamestate without MM_Play_Destroy, so the actors live at that instant
 * are never deleted. The row drives the REAL departure -- MM's registered
 * GameOps suspend, the call GameRunner makes on both switch paths -- and
 * checks it leaves the next session what a normal teardown would have:
 *
 *   (a) En_Test4's sIsLoaded latch is back to false. Left latched, the next
 *       session's clock actor kills itself in EnTest4_Init;
 *   (b) every overlay that had clients has none, and its profile's reset ran
 *       exactly once (the path MM_Actor_Delete takes for the last client), and
 *       only after the graph -- and with it MM_gPlayState -- was retired;
 *   (c) an overlay with NO clients is not reset again: it was reset when its
 *       last client went, and some resets free memory;
 *   (d) each overlay whose Destroy was the only thing restoring a static has
 *       that static back at its initial value (seeded to what a live client
 *       leaves, then read back through the TU's accessors);
 *   (e) no per-actor ObjectExtension entry survives: the next session's arena
 *       hands the same addresses to different actors.
 *
 * Headless: the suspend's shared-resource harvest is gated off by a
 * non-gameplay gameMode, the audio calls only latch resetTimer while MM audio
 * is uninitialized, and gComboCtx is snapshotted around the staged-item commit.
 */
extern "C" int MM_AbandonedSessionStatics_RunHeadless(void) {
    printf("[TEST] mm-abandoned-session-statics: an abandoned MM session's overlay statics are reset (#666)\n");

    // Two unused overlay-table slots carry probe profiles, so (b) and (c) are
    // observed on a reset this row owns rather than inferred.
    s32 probeSlot = -1;
    s32 idleSlot = -1;
    for (s32 i = 0; i < ACTOR_ID_MAX; i++) {
        if (gActorOverlayTable[i].profile == NULL && gActorOverlayTable[i].numLoaded == 0) {
            if (probeSlot < 0) {
                probeSlot = i;
            } else {
                idleSlot = i;
                break;
            }
        }
    }
    // Preconditions: nothing is seeded yet, so these may return directly.
    RESUME_ASSERT(probeSlot >= 0 && idleSlot >= 0, "no two unused overlay-table slots for the probes");
    RESUME_ASSERT(!gAudioCtxInitalized, "MM audio is initialized: the real suspend would reset a live audio heap");
    RESUME_ASSERT(MM_GetGameOps() != NULL && MM_GetGameOps()->suspend != NULL, "MM registers no suspend");
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        const ActorOverlay* entry = &gActorOverlayTable[kDestroyMaintained[k].actorId];
        RESUME_ASSERT(entry->profile != NULL, "Destroy-maintained overlay has no profile");
        RESUME_ASSERT(!kDestroyMaintained[k].isDirty(), "a Destroy-maintained static is not at its initial value");
    }

    static ActorProfile probeProfile;
    static ActorProfile idleProfile;
    static Actor fakeActor;
    static u8 fakePlayState[sizeof(PlayState)];
    static ComboContext comboSnapshot;
    probeProfile = {};
    probeProfile.id = (s16)probeSlot;
    probeProfile.reset = ProbeReset;
    idleProfile = {};
    idleProfile.id = (s16)idleSlot;
    idleProfile.reset = IdleProbeReset;
    fakeActor = {};
    sProbeResetCalls = 0;
    sIdleProbeResetCalls = 0;
    sProbeSawRetiredGraph = false;

    AbandonedSessionRowGuard guard;
    guard.probeSlot = probeSlot;
    guard.idleSlot = idleSlot;
    guard.fakeActor = &fakeActor;
    guard.playState = MM_gPlayState;
    guard.gameMode = gSaveContext.gameMode;
    guard.resetTimer = gAudioCtx.resetTimer;
    guard.audioInitialized = gAudioCtxInitalized;
    memcpy(&comboSnapshot, &gComboCtx, sizeof(ComboContext));
    guard.comboSnapshot = &comboSnapshot;
    guard.seeded = true;

    // The abandoned session, as MM_Game_Suspend finds it: a live Play
    // gamestate; the clock actor ran EnTest4_Init (latch set) and is still a
    // live client; each Destroy-maintained overlay has a live client and its
    // static where that client leaves it; the probe overlay has two clients;
    // the idle probe had its last client deleted normally; an actor carries an
    // ObjectExtension entry. gameMode is the title screen so the suspend's
    // harvest stays gated off (it must not fold this row's save into the pool).
    MM_gPlayState = (PlayState*)fakePlayState;
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    MM_EnTest4_SetLoadedLatchForTest(true);
    gActorOverlayTable[ACTOR_EN_TEST4].numLoaded = 1;
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        gActorOverlayTable[kDestroyMaintained[k].actorId].numLoaded = 1;
        kDestroyMaintained[k].setDirty(true);
    }
    gActorOverlayTable[probeSlot].profile = &probeProfile;
    gActorOverlayTable[probeSlot].numLoaded = 2;
    gActorOverlayTable[idleSlot].profile = &idleProfile;
    gActorOverlayTable[idleSlot].numLoaded = 0;
    SetActorListIndex(&fakeActor, 7);

    const s32 latchBefore = MM_EnTest4_IsLoadedLatchForTest();
    s32 dirtyBefore = 0;
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        dirtyBefore += kDestroyMaintained[k].isDirty() ? 1 : 0;
    }
    const s16 listIndexBefore = GetActorListIndex(&fakeActor);
    const size_t extensionsBefore = ObjectExtension::GetInstance().Count();

    // The departure itself.
    MM_GetGameOps()->suspend();

    const s32 latchAfter = MM_EnTest4_IsLoadedLatchForTest();
    const s8 test4ClientsAfter = gActorOverlayTable[ACTOR_EN_TEST4].numLoaded;
    const s8 probeClientsAfter = gActorOverlayTable[probeSlot].numLoaded;
    const int probeCalls = sProbeResetCalls;
    const int idleCalls = sIdleProbeResetCalls;
    const bool probeSawRetiredGraph = sProbeSawRetiredGraph;
    s32 destroyMaintainedClientsAfter = 0;
    const char* staleStatic = NULL;
    s32 staleStatics = 0;
    for (size_t k = 0; k < kDestroyMaintainedCount; k++) {
        destroyMaintainedClientsAfter += gActorOverlayTable[kDestroyMaintained[k].actorId].numLoaded;
        if (kDestroyMaintained[k].isDirty()) {
            staleStatics++;
            if (staleStatic == NULL) {
                staleStatic = kDestroyMaintained[k].name;
            }
        }
    }
    const s16 listIndexAfter = GetActorListIndex(&fakeActor);
    const size_t extensionsAfter = ObjectExtension::GetInstance().Count();

    printf("[TEST] mm-abandoned-session-statics: before suspend: latch %d, stale Destroy-maintained statics %d/%zu, "
           "list index %d, extension entries %zu\n",
           (int)latchBefore, (int)dirtyBefore, kDestroyMaintainedCount, (int)listIndexBefore, extensionsBefore);
    printf("[TEST] mm-abandoned-session-statics: after suspend: latch %d, En_Test4 clients %d, probe clients %d, "
           "probe resets %d (graph retired first: %d), idle resets %d, Destroy-maintained clients %d, stale "
           "Destroy-maintained statics %d/%zu%s%s, list index %d, extension entries %zu\n",
           (int)latchAfter, (int)test4ClientsAfter, (int)probeClientsAfter, probeCalls, (int)probeSawRetiredGraph,
           idleCalls, (int)destroyMaintainedClientsAfter, (int)staleStatics, kDestroyMaintainedCount,
           staleStatic != NULL ? ", first " : "", staleStatic != NULL ? staleStatic : "", (int)listIndexAfter,
           extensionsAfter);

    // The seeding took (the values were read before the suspend; asserting them
    // here, with the guard armed, keeps every exit path restoring the state).
    RESUME_ASSERT(latchBefore != 0, "the En_Test4 latch accessor did not take the seeded value");
    RESUME_ASSERT(dirtyBefore == (s32)kDestroyMaintainedCount, "a Destroy-maintained static did not take its seed");
    RESUME_ASSERT(listIndexBefore == 7, "ObjectExtension did not take the seeded entry");
    RESUME_ASSERT(extensionsBefore > 0, "ObjectExtension count does not see the seeded entry");

    RESUME_ASSERT(latchAfter == false,
                  "En_Test4's sIsLoaded survived the abandoned session: the next session's clock actor kills itself");
    RESUME_ASSERT(test4ClientsAfter == 0, "En_Test4 still counts a client from the abandoned session");
    RESUME_ASSERT(probeClientsAfter == 0, "the probe overlay still counts clients from the abandoned session");
    RESUME_ASSERT(probeCalls == 1, "an overlay with abandoned clients was not reset exactly once");
    RESUME_ASSERT(probeSawRetiredGraph, "the abandoned session was retired before the graph (MM_gPlayState still set)");
    RESUME_ASSERT(idleCalls == 0, "an overlay with no clients was reset again (resets are not all idempotent)");
    RESUME_ASSERT(destroyMaintainedClientsAfter == 0, "a Destroy-maintained overlay still counts abandoned clients");
    RESUME_ASSERT(staleStatics == 0, "a static only Destroy restored survived the abandoned session");
    RESUME_ASSERT(listIndexAfter == -1, "a per-actor ObjectExtension entry survived the abandoned session");
    RESUME_ASSERT(extensionsAfter == 0, "ObjectExtension entries survived the abandoned session");

    printf("[TEST] PASS: mm-abandoned-session-statics — the abandoned session's overlay and actor state is retired\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
