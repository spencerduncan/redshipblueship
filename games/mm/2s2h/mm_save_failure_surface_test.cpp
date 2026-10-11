/**
 * @file mm_save_failure_surface_test.cpp
 * ROM-free, display-free lock for #564 V18: an MM save that cannot commit is
 * shown to the player. CTest label "redship", row MMSaveFailureSurface in
 * CMake/SingleExecutable.cmake, dispatch "mm-save-failure-surface" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. Every durable MM save route (owl statue, pause save, Song of
 * Time, game over, the two special saves, the autosave) marshals through
 * func_8014546C / func_80145698, whose tail is the one commit funnel,
 * Sram_ComboCommitUnifiedSave (games/mm/src/code/z_sram_NES.c, #530). The funnel
 * discarded MM_Combo_CaptureSaveToUnifiedSlot's result, and the capture reported
 * a skipped or failed commit on stderr only. The route then ran its own success
 * ceremony for a commit that never reached the file. One-game semantics: a
 * capture that cannot commit is a save FAILURE, and the player must be told.
 *
 * WHAT THIS LOCKS, driving the real marshallers and one whole route:
 *
 *   1. Each of the capture's three non-commit outcomes queues one toast on the
 *      shared overlay, "Save failed:" and the cause's words
 *      (src/common/pairing_refusal_toast.h, RsbsSaveFailure): no slot active
 *      (driven through the whole Sram_SaveSpecialEnterClockTown route), a slot
 *      the #533/#568 latch keeps closed (refused, and never opened), and a
 *      writable slot whose file write fails. RED before the fix: no toast at
 *      all, only the stderr line.
 *   2. Once per cause, never per save: the same cause again (the next autosave,
 *      the next new day) queues nothing.
 *   3. Any change of the unified save's slot state starts a new episode, so
 *      the next failure is shown again: a commit that lands from MM's capture
 *      or from OoT's save worker, an erase and create of the same slot, the
 *      same slot refused again. Another active slot is another file and is
 *      told too. (A save that commits nothing moves no slot state, so the
 *      record holds across repeats.)
 *   4. The toast leaves the Combo > Save Files page's refusal words alone: a
 *      refused slot keeps the reason it was refused for.
 *   5. A commit that lands queues nothing, and a refused one wrote nothing.
 *   6. The words: every cause has some, capitalized without a trailing period
 *      (the overlay's copy rule; PairingRefusalToastFit draws their width).
 */

#include "global.h"

#include "save.h"
#include "context.h"
#include "game.h"
#include "notification_bridge.h"
#include "pairing_refusal_toast.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

extern "C" SaveContext gSaveContext;

namespace {

#define SFS_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            fflush(stdout);                           \
            return 1;                                 \
        }                                             \
    } while (0)

// Backing store for the marshallers' saveBuf (static: 128 KiB).
u8 sSaveBuf[SAVE_BUFFER_SIZE];

// gSaveContext in the shape a cross-game MM session runs in: the 0xFF "no real
// flash slot" sentinel, flash reported available.
void ArmCrossGameSaveContext() {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
}

std::string Expected(int cause) {
    return std::string(RSBS_SAVE_FAILURE_TOAST_PREFIX) + " " + Combo_SaveFailureWords(cause);
}

int ToastCount() {
    return OoT_Notification_EmittedCountForTest();
}

std::string LastToast() {
    char text[256];
    const int count = ToastCount();
    if (count <= 0 || !OoT_Notification_EmittedAtForTest(count - 1, text, sizeof(text))) {
        return std::string();
    }
    return text;
}

// One marshal through func_80145698, the shape every special save and the
// game-over save reach; returns how many toasts it queued and the last one.
int MarshalAndCount(SramContext* sramCtx, std::string* last) {
    const int before = ToastCount();
    ArmCrossGameSaveContext();
    func_80145698(sramCtx);
    *last = LastToast();
    return ToastCount() - before;
}

} // namespace

extern "C" int MM_SaveFailureSurface_RunHeadless(void) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "rsbs_mm_save_failure_surface_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    // A directory under a regular FILE: every write into it fails to open.
    const fs::path blocker = dir / "blocker";
    {
        std::ofstream make(blocker.string(), std::ios::binary | std::ios::trunc);
        make << "not a directory";
    }
    const fs::path unwritable = blocker / "saves";

    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    RsbsSave_ResetSlotSessionState();
    Context_InitFrozenStates();
    ComboContext_Init();
    OoT_Notification_ClearForTest();
    OoT_Notification_RecordForTest(1);

    SFS_ASSERT(MM_gPlayState == NULL, "the funnel must be driven with no play state (#516)");

    SramContext sramCtx;
    memset(&sramCtx, 0, sizeof(sramCtx));
    sramCtx.saveBuf = sSaveBuf;
    std::string last;

    // ---- 6. The words -------------------------------------------------------
    for (int cause = 0; cause < RSBS_SAVE_FAILURE_COUNT; cause++) {
        const std::string words = Combo_SaveFailureWords(cause);
        SFS_ASSERT(!words.empty() && std::isupper(static_cast<unsigned char>(words.front())) && words.back() != '.',
                   "cause %d's words '%s' are not a capitalized message without a trailing period", cause,
                   words.c_str());
    }

    // ---- 5. A commit that lands queues nothing ------------------------------
    // Also the clean start: a landed commit clears whatever an earlier row in
    // the same process left recorded.
    const int kGood = 0;
    RsbsSave_SetActiveSlot(kGood);
    SFS_ASSERT(RsbsSave_LoadSlot(kGood) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    {
        const uint32_t gen = gComboCtx.commitGeneration;
        const int queued = MarshalAndCount(&sramCtx, &last);
        SFS_ASSERT(gComboCtx.commitGeneration == gen + 1 && RsbsSave_HasSave(kGood) == 1,
                   "test setup: a marshal into an armed slot must commit");
        SFS_ASSERT(queued == 0, "a save that landed queued %d toast(s), last '%s'", queued, last.c_str());
    }

    // ---- 1/2. No slot: the whole first-entry-to-Clock-Town route ------------
    RsbsSave_SetActiveSlot(-1);
    {
        std::vector<uint8_t> playMem(sizeof(PlayState), uint8_t(0));
        PlayState* play = reinterpret_cast<PlayState*>(playMem.data());
        play->sramCtx.saveBuf = sSaveBuf;
        for (int attempt = 1; attempt <= 2; attempt++) {
            const int before = ToastCount();
            ArmCrossGameSaveContext();
            Sram_SaveSpecialEnterClockTown(play);
            const int queued = ToastCount() - before;
            last = LastToast();
            printf("[TEST] no slot, save %d: %d toast(s) queued, last '%s'\n", attempt, queued, last.c_str());
            if (attempt == 1) {
                SFS_ASSERT(queued == 1 && last == Expected(RSBS_SAVE_FAILURE_NO_FILE),
                           "a save route with no active slot committed nothing and the player was told nothing: "
                           "%d toast(s), last '%s' (want one '%s')",
                           queued, last.c_str(), Expected(RSBS_SAVE_FAILURE_NO_FILE).c_str());
            } else {
                SFS_ASSERT(queued == 0, "the same cause toasted again on the next save: '%s' (once per cause)",
                           last.c_str());
            }
        }
    }

    // ---- 1/2/4. A refused slot ----------------------------------------------
    const int kRefused = 1;
    const std::string refusedWords = Combo_RefusalWords(RSBS_REFUSAL_WORDS_MM_OPTIONS_DIFFER);
    RsbsSave_SetActiveSlot(kRefused);
    SFS_ASSERT(RsbsSave_LoadSlot(kRefused) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    RsbsSave_RefuseSlotIdentity(kRefused);
    RsbsSave_NoteSlotRefusalWords(kRefused, refusedWords.c_str());
    for (int attempt = 1; attempt <= 3; attempt++) {
        const uint32_t gen = gComboCtx.commitGeneration;
        const int queued = MarshalAndCount(&sramCtx, &last);
        printf("[TEST] refused slot, save %d: %d toast(s) queued, last '%s'\n", attempt, queued, last.c_str());
        SFS_ASSERT(gComboCtx.commitGeneration == gen && RsbsSave_HasSave(kRefused) == 0,
                   "a refused slot was written (the #533 latch must hold)");
        if (attempt == 1) {
            SFS_ASSERT(queued == 1 && last == Expected(RSBS_SAVE_FAILURE_LOCKED),
                       "a save into a refused slot committed nothing and the player was told nothing: %d toast(s), "
                       "last '%s' (want one '%s')",
                       queued, last.c_str(), Expected(RSBS_SAVE_FAILURE_LOCKED).c_str());
        } else {
            SFS_ASSERT(queued == 0, "the same cause toasted again on save %d: '%s' (once per cause)", attempt,
                       last.c_str());
        }
        SFS_ASSERT(refusedWords == RsbsSave_SlotRefusalWords(kRefused),
                   "the save-failure toast replaced the refused slot's words on the Save Files page: '%s' (want '%s')",
                   RsbsSave_SlotRefusalWords(kRefused), refusedWords.c_str());
    }

    // ---- 3. Another slot is another file: a never-opened slot ---------------
    // (Slot 2, before the next leg opens it: there are RSBS_SAVE_MAX_SLOTS.)
    const int kNeverOpened = 2;
    RsbsSave_SetActiveSlot(kNeverOpened);
    {
        const int queued = MarshalAndCount(&sramCtx, &last);
        printf("[TEST] never-opened slot: %d toast(s) queued, last '%s'\n", queued, last.c_str());
        SFS_ASSERT(queued == 1 && last == Expected(RSBS_SAVE_FAILURE_LOCKED),
                   "a save into another, never-opened slot was not told: %d toast(s), last '%s' (want one '%s')",
                   queued, last.c_str(), Expected(RSBS_SAVE_FAILURE_LOCKED).c_str());
        SFS_ASSERT(RsbsSave_HasSave(kNeverOpened) == 0, "a never-opened slot was written (the #533 latch)");
    }

    // ---- 1/2/3. A writable slot whose write fails ---------------------------
    // The same slot, now opened: a different cause on the same file is told.
    const int kDisk = kNeverOpened;
    RsbsSave_SetActiveSlot(kDisk);
    SFS_ASSERT(RsbsSave_LoadSlot(kDisk) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    SFS_ASSERT(RsbsSave_IsSlotWritable(kDisk) == 1, "test setup: the slot must be writable");
    rsbs::SaveManager::Instance().SetSaveDirectory(unwritable.string());
    for (int attempt = 1; attempt <= 2; attempt++) {
        const int queued = MarshalAndCount(&sramCtx, &last);
        printf("[TEST] failed write, save %d: %d toast(s) queued, last '%s'\n", attempt, queued, last.c_str());
        if (attempt == 1) {
            SFS_ASSERT(queued == 1 && last == Expected(RSBS_SAVE_FAILURE_NOT_WRITTEN),
                       "a save whose file write failed was not told: %d toast(s), last '%s' (want one '%s')", queued,
                       last.c_str(), Expected(RSBS_SAVE_FAILURE_NOT_WRITTEN).c_str());
        } else {
            SFS_ASSERT(queued == 0, "the same cause toasted again: '%s' (once per cause)", last.c_str());
        }
    }
    // The write lands again: no toast, and the record is cleared...
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    {
        const uint32_t gen = gComboCtx.commitGeneration;
        const int queued = MarshalAndCount(&sramCtx, &last);
        SFS_ASSERT(RsbsSave_HasSave(kDisk) == 1 && gComboCtx.commitGeneration > gen,
                   "test setup: the slot must commit once its directory is writable again");
        SFS_ASSERT(queued == 0, "a save that landed queued a toast: '%s'", last.c_str());
    }
    // ...so the next failure is told again, through the other marshaller (the
    // owl statue's and the pause save's).
    rsbs::SaveManager::Instance().SetSaveDirectory(unwritable.string());
    {
        const int before = ToastCount();
        ArmCrossGameSaveContext();
        gSaveContext.save.isOwlSave = true;
        func_8014546C(&sramCtx);
        const int queued = ToastCount() - before;
        last = LastToast();
        printf("[TEST] failed write after a landed save: %d toast(s) queued, last '%s'\n", queued, last.c_str());
        SFS_ASSERT(queued == 1 && last == Expected(RSBS_SAVE_FAILURE_NOT_WRITTEN),
                   "a failure after a landed save was not told again: %d toast(s), last '%s'", queued, last.c_str());
    }

    // ---- 3. Any slot-state change starts a new episode, from either game ----
    // These legs report every miss before failing, so one run shows each.
    int episodeMisses = 0;
    const auto expectToldAgain = [&](const char* what, int wantQueued, int cause) {
        const int queued = MarshalAndCount(&sramCtx, &last);
        printf("[TEST] %s: %d toast(s) queued, last '%s'\n", what, queued, last.c_str());
        if (queued != wantQueued || (wantQueued > 0 && last != Expected(cause))) {
            printf("[TEST] FAIL: %s: %d toast(s), last '%s' (want %d '%s') (%s:%d)\n", what, queued, last.c_str(),
                   wantQueued, Expected(cause).c_str(), __FILE__, __LINE__);
            fflush(stdout);
            episodeMisses++;
        }
    };
    // OoT's side commits the same file (its save worker's two-phase path):
    // under one-game semantics that is a landed commit of the same game.
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    SFS_ASSERT(RsbsSave_StageCommit() != 0 && RsbsSave_WriteStagedCommit(kDisk) == 1,
               "test setup: OoT's two-phase commit into the slot must land");
    rsbs::SaveManager::Instance().SetSaveDirectory(unwritable.string());
    expectToldAgain("failed write after OoT's commit landed", 1, RSBS_SAVE_FAILURE_NOT_WRITTEN);
    expectToldAgain("failed write after OoT's commit landed, save 2", 0, RSBS_SAVE_FAILURE_NOT_WRITTEN);
    // The player erases the file and creates a new one in the same slot.
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    RsbsSave_DeleteSave(kDisk);
    RsbsSave_ArmSlotOnCreate(kDisk);
    rsbs::SaveManager::Instance().SetSaveDirectory(unwritable.string());
    expectToldAgain("failed write after erase and create", 1, RSBS_SAVE_FAILURE_NOT_WRITTEN);
    // The same slot refused, then refused again (the next arrival's check).
    RsbsSave_RefuseSlotIdentity(kDisk);
    expectToldAgain("refused again, save 1", 1, RSBS_SAVE_FAILURE_LOCKED);
    expectToldAgain("refused again, save 2", 0, RSBS_SAVE_FAILURE_LOCKED);
    RsbsSave_RefuseSlotIdentity(kDisk);
    expectToldAgain("refused once more", 1, RSBS_SAVE_FAILURE_LOCKED);
    SFS_ASSERT(episodeMisses == 0, "%d save failure(s) after a slot-state change were not told again", episodeMisses);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());

    OoT_Notification_RecordForTest(0);
    OoT_Notification_ClearForTest();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    fs::remove_all(dir, ec);

    printf("[TEST] PASS: every MM save that did not reach the file was told once per cause, as '%s ...'\n",
           RSBS_SAVE_FAILURE_TOAST_PREFIX);
    return 0;
}
