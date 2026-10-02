/**
 * @file oot_file_select_refusal_test.cpp
 * ROM-free, display-free lock for #836 PR 1: a refused paired file is never
 * opened. CTest label "redship", row OoTFileSelectRefusal in
 * CMake/SingleExecutable.cmake, dispatch "oot-file-select-refusal" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. OoT's file select opened every occupied file. The .redsave
 * was read only inside the open (the OnLoadFile seam, at the tail of LoadFile),
 * after Context_InvalidateSessionOnSlotLoad had dropped the pairing identity,
 * and the seam discarded the load's verdict. So a randomizer file whose
 * cross-game record was missing, damaged or another build's played its OoT
 * half unpaired; a missing record also ARMED the slot, so the first base save
 * wrote a .redsave from the dropped session and the file was unpaired for good.
 *
 * THE FIX UNDER TEST. Two hooks the SoH SaveManager registers (no edit to the
 * vendored file select):
 *   - the GATE (OnFileChooseMain): A or START on an occupied file in the main
 *     menu runs the probe (RsbsSave_ProbeSlotForOpen); a refusal clears the
 *     press, posts one toast, and the player stays on the file list;
 *   - the BACKSTOP (OnLoadGame): a load the OnLoadFile seam refused returns the
 *     player to the file select instead of entering Play, once.
 *
 * THE LEGS, through the hooks a fresh SoH SaveManager registers (isolated by
 * hook id, the oot-exit-harvest-gate pattern):
 *   1. a randomizer file with NO .redsave: A is consumed, one toast whose words
 *      are "Cross-game record is missing", the Save Files record says the same;
 *      nothing is written (no slot file, no .tmp, no .bak), gComboCtx is
 *      byte-identical and the active slot is unchanged;
 *   2. a randomizer file whose .redsave is garbage: START is consumed, one
 *      toast, the file is byte-identical and still at the slot path, no .bak or
 *      .tmp appeared, gComboCtx and the active slot are unchanged;
 *   3. control: a VANILLA file with no .redsave keeps its A press, no toast;
 *   4. the backstop: OnLoadFile for the garbage file refuses, then OnLoadGame
 *      sets the next state to FileChoose_Init, running false, gameMode
 *      GAMEMODE_FILE_SELECT, posts one toast; a second OnLoadGame (no refused
 *      load) leaves the Play transition alone.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "SaveManager.h"
#include "Enhancements/game-interactor/GameInteractor.h"

#include "context.h"
#include "notification_bridge.h"
#include "save.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "src/overlays/gamestates/ovl_file_choose/file_choose.h"
extern SaveContext gSaveContext;
extern GameState* OoT_gGameState;
void OoT_Play_Init(GameState* thisx);
void FileChoose_Init(GameState* thisx);
}

namespace {

#define FSR_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

template <typename Hook> std::vector<HOOK_ID> HookIds() {
    std::vector<HOOK_ID> ids;
    for (auto& kv : GameInteractor::RegisteredGameHooks<Hook>::functions) {
        ids.push_back(kv.first);
    }
    return ids;
}

template <typename Hook> std::vector<HOOK_ID> NewIds(const std::vector<HOOK_ID>& before) {
    std::vector<HOOK_ID> out;
    for (HOOK_ID id : HookIds<Hook>()) {
        if (std::find(before.begin(), before.end(), id) == before.end()) {
            out.push_back(id);
        }
    }
    return out;
}

std::vector<uint8_t> FileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/** Every file in @p dir other than the slot files themselves (a .tmp or a
 *  .bak is something the probe wrote or renamed). */
int ExtraFiles(const std::filesystem::path& dir) {
    int n = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.size() < 8 || name.compare(name.size() - 8, 8, ".redsave") != 0) {
            n++;
        }
    }
    return n;
}

std::string LastToast() {
    const int n = OoT_Notification_EmittedCountForTest();
    char line[256] = { 0 };
    if (n > 0) {
        OoT_Notification_EmittedAtForTest(n - 1, line, sizeof(line));
    }
    return line;
}

} // namespace

extern "C" int OoT_FileSelectRefusal_RunHeadless(void) {
    printf("[TEST] oot-file-select-refusal: a file the open path would refuse is not opened at the file select, and a "
           "load refused after the gate returns to the file select (#836)\n");
    int failures = 0;

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_oot_file_select_refusal_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager& rsbsSave = rsbs::SaveManager::Instance();
    rsbsSave.SetSaveDirectory(dir.string());
    RsbsSave_ResetSlotSessionState();
    Context_InitFrozenStates();
    ComboContext_Init();

    const GameId gameBefore = Context_GetCurrentGame();
    GameState* gameStateBefore = OoT_gGameState;
    auto saveBefore = std::make_unique<SaveContext>();
    memcpy(saveBefore.get(), &gSaveContext, sizeof(SaveContext));
    Context_SetCurrentGame(GAME_OOT);
    OoT_Notification_RecordForTest(1);

    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const auto chooseBefore = HookIds<GameInteractor::OnFileChooseMain>();
    const auto loadFileBefore = HookIds<GameInteractor::OnLoadFile>();
    const auto loadGameBefore = HookIds<GameInteractor::OnLoadGame>();
    SaveManager* fresh = new SaveManager(); // kept alive, like every other *::Instance singleton
    const auto chooseHooks = NewIds<GameInteractor::OnFileChooseMain>(chooseBefore);
    const auto loadFileHooks = NewIds<GameInteractor::OnLoadFile>(loadFileBefore);
    const auto loadGameHooks = NewIds<GameInteractor::OnLoadGame>(loadGameBefore);
    printf("[TEST] a fresh SoH SaveManager registered %zu OnFileChooseMain, %zu OnLoadFile and %zu OnLoadGame hooks\n",
           chooseHooks.size(), loadFileHooks.size(), loadGameHooks.size());

    auto dispatchChoose = [&](FileChooseContext* fc) {
        for (HOOK_ID id : chooseHooks) {
            GameInteractor::RegisteredGameHooks<GameInteractor::OnFileChooseMain>::functions[id](fc);
        }
    };

    // A heap file select in the main menu, cursor on @p slot, @p button pressed.
    std::unique_ptr<uint8_t[]> fcBytes(new uint8_t[sizeof(FileChooseContext)]());
    FileChooseContext* fc = reinterpret_cast<FileChooseContext*>(fcBytes.get());
    auto press = [&](int slot, u16 button) {
        memset(fcBytes.get(), 0, sizeof(FileChooseContext));
        fc->menuMode = FS_MENU_MODE_CONFIG;
        fc->configMode = CM_MAIN_MENU;
        fc->buttonIndex = (s16)slot;
        fc->state.input[0].press.button = button;
        fc->state.input[0].cur.button = button;
    };

    const int kAbsent = 0;  // a randomizer file with no .redsave
    const int kGarbage = 1; // a randomizer file whose .redsave is not one
    const int kVanilla = 2; // a vanilla file with no .redsave
    for (int i = 0; i < SaveManager::MaxFiles; i++) {
        fresh->fileMetaInfo[i].valid = true;
        fresh->fileMetaInfo[i].randoSave = i != kVanilla;
    }
    const std::string garbagePath = rsbsSave.SlotPath(kGarbage);
    {
        std::ofstream out(garbagePath, std::ios::binary);
        const char junk[] = "this is not a .redsave; the gate must leave it exactly where it is";
        out.write(junk, sizeof(junk));
    }
    const std::vector<uint8_t> garbageBytes = FileBytes(garbagePath);
    RsbsSave_SetActiveSlot(kVanilla);    // a slot from an earlier session; the gate must not move it
    gComboCtx.sharedRandoSeed = 0x0836u; // a recognisable resident record
    ComboContext comboBefore;
    memcpy(&comboBefore, &gComboCtx, sizeof(ComboContext));

    // ---- 1. a randomizer file with no .redsave ------------------------------
    {
        const int toastsBefore = OoT_Notification_EmittedCountForTest();
        press(kAbsent, BTN_A);
        dispatchChoose(fc);
        const u16 left = fc->state.input[0].press.button;
        const int toasts = OoT_Notification_EmittedCountForTest() - toastsBefore;
        const std::string toast = LastToast();
        printf("[TEST] leg 1 OBSERVED: press left %04X, %d toast(s) \"%s\", slot state %d reason %d words \"%s\", "
               "slot file %s, extra files %d, active slot %d\n",
               left, toasts, toast.c_str(), RsbsSave_GetSlotState(kAbsent), RsbsSave_GetSlotRefuseReason(kAbsent),
               RsbsSave_SlotRefusalWords(kAbsent),
               std::filesystem::exists(rsbsSave.SlotPath(kAbsent)) ? "present" : "absent", ExtraFiles(dir),
               RsbsSave_GetActiveSlot());
        FSR_ASSERT((left & (BTN_A | BTN_START)) == 0,
                   "leg 1: the A press on a randomizer file with no cross-game record was not consumed: the file opens "
                   "and plays unpaired");
        FSR_ASSERT(toasts == 1 && toast.find("Cross-game record is missing") != std::string::npos,
                   "leg 1: %d toast(s), last \"%s\" (want one naming the missing record)", toasts, toast.c_str());
        FSR_ASSERT(std::string(RsbsSave_SlotRefusalWords(kAbsent)) == "Cross-game record is missing" &&
                       RsbsSave_GetSlotRefuseReason(kAbsent) == (int)RSBS_REFUSE_MISSING &&
                       RsbsSave_IsSlotWritable(kAbsent) == 0,
                   "leg 1: the session's refusal record does not name the missing record, or the slot is writable");
        FSR_ASSERT(!std::filesystem::exists(rsbsSave.SlotPath(kAbsent)) && ExtraFiles(dir) == 0,
                   "leg 1: the gate wrote a file");
        FSR_ASSERT(memcmp(&comboBefore, &gComboCtx, sizeof(ComboContext)) == 0, "leg 1: the gate changed gComboCtx");
        FSR_ASSERT(RsbsSave_GetActiveSlot() == kVanilla, "leg 1: the gate moved the active slot");
    }

    // ---- 2. a randomizer file whose .redsave is garbage ---------------------
    {
        const int toastsBefore = OoT_Notification_EmittedCountForTest();
        press(kGarbage, BTN_START);
        dispatchChoose(fc);
        const u16 left = fc->state.input[0].press.button;
        const int toasts = OoT_Notification_EmittedCountForTest() - toastsBefore;
        const std::string toast = LastToast();
        printf("[TEST] leg 2 OBSERVED: press left %04X, %d toast(s) \"%s\", reason %d, file bytes %s, extra files %d\n",
               left, toasts, toast.c_str(), RsbsSave_GetSlotRefuseReason(kGarbage),
               FileBytes(garbagePath) == garbageBytes ? "unchanged" : "CHANGED", ExtraFiles(dir));
        FSR_ASSERT((left & (BTN_A | BTN_START)) == 0, "leg 2: the START press on a garbage record was not consumed");
        FSR_ASSERT(toasts == 1 && toast.rfind("Not paired:", 0) == 0 &&
                       toast.find(RsbsSave_SlotRefusalWords(kGarbage)) != std::string::npos,
                   "leg 2: %d toast(s), last \"%s\" (want one with the page's words \"%s\")", toasts, toast.c_str(),
                   RsbsSave_SlotRefusalWords(kGarbage));
        FSR_ASSERT(FileBytes(garbagePath) == garbageBytes && ExtraFiles(dir) == 0,
                   "leg 2: the refused record was changed, renamed, or a .tmp/.bak appeared");
        FSR_ASSERT(memcmp(&comboBefore, &gComboCtx, sizeof(ComboContext)) == 0, "leg 2: the gate changed gComboCtx");
        FSR_ASSERT(RsbsSave_GetActiveSlot() == kVanilla, "leg 2: the gate moved the active slot");
    }

    // ---- 3. control: a vanilla file with no .redsave opens ------------------
    {
        const int toastsBefore = OoT_Notification_EmittedCountForTest();
        press(kVanilla, BTN_A);
        dispatchChoose(fc);
        const u16 left = fc->state.input[0].press.button;
        const int toasts = OoT_Notification_EmittedCountForTest() - toastsBefore;
        printf("[TEST] leg 3 OBSERVED: vanilla file, no record: press left %04X, %d toast(s)\n", left, toasts);
        FSR_ASSERT((left & BTN_A) != 0 && toasts == 0, "leg 3: a vanilla file with no cross-game record was refused");
    }

    // ---- 4. the backstop ----------------------------------------------------
    {
        static uint8_t fakeState[sizeof(GameState)];
        memset(fakeState, 0, sizeof(fakeState));
        GameState* state = reinterpret_cast<GameState*>(fakeState);
        // What FileChoose_LoadGame leaves before OnLoadGame: the Play transition.
        SET_NEXT_GAMESTATE(state, OoT_Play_Init, PlayState);
        state->running = 1;
        OoT_gGameState = state;
        gSaveContext.fileNum = kGarbage;
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.ship.quest.id = QUEST_RANDOMIZER;

        const int toastsBefore = OoT_Notification_EmittedCountForTest();
        for (HOOK_ID id : loadFileHooks) {
            GameInteractor::RegisteredGameHooks<GameInteractor::OnLoadFile>::functions[id](kGarbage);
        }
        for (HOOK_ID id : loadGameHooks) {
            GameInteractor::RegisteredGameHooks<GameInteractor::OnLoadGame>::functions[id](kGarbage);
        }
        const bool toFileSelect = state->init == FileChoose_Init;
        const int toasts = OoT_Notification_EmittedCountForTest() - toastsBefore;
        printf("[TEST] leg 4 OBSERVED: after a refused load, next state %s, running %d, gameMode %d, %d toast(s) "
               "\"%s\"\n",
               toFileSelect ? "FileChoose_Init" : (state->init == OoT_Play_Init ? "OoT_Play_Init" : "other"),
               (int)state->running, (int)gSaveContext.gameMode, toasts, LastToast().c_str());
        FSR_ASSERT(toFileSelect && state->running == 0 && gSaveContext.gameMode == GAMEMODE_FILE_SELECT,
                   "leg 4: a load the seam refused still enters Play");
        FSR_ASSERT(toasts == 1, "leg 4: the backstop posted %d toast(s), want one", toasts);

        // Once: the next OnLoadGame with no refused load leaves Play alone.
        SET_NEXT_GAMESTATE(state, OoT_Play_Init, PlayState);
        state->running = 1;
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        for (HOOK_ID id : loadGameHooks) {
            GameInteractor::RegisteredGameHooks<GameInteractor::OnLoadGame>::functions[id](kGarbage);
        }
        printf("[TEST] leg 4 OBSERVED: a second OnLoadGame leaves next state %s\n",
               state->init == OoT_Play_Init ? "OoT_Play_Init" : "CHANGED");
        FSR_ASSERT(state->init == OoT_Play_Init && state->running == 1,
                   "leg 4: the backstop fired again on a load nothing refused");
    }

    // Put back everything this row touched.
    OoT_gGameState = gameStateBefore;
    memcpy(&gSaveContext, saveBefore.get(), sizeof(SaveContext));
    Context_SetCurrentGame(gameBefore);
    OoT_Notification_RecordForTest(0);
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    if (failures != 0) {
        printf("[TEST] oot-file-select-refusal: %d failure(s)\n", failures);
        return 1;
    }
    printf("[TEST] PASS: oot-file-select-refusal - a refused file stays on the file list, and a refused load returns "
           "to it\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
